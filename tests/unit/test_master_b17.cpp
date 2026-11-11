// M3.b, unità B17: dato reale PUB-LFC-01 (Palomar LFC, CC BY 4.0, docs/datasets/PUB-LFC-01.md). C-02.b §b.2 (analisi),
// §b.4 (proposta), §b.6 (sequenza di calcolo), §b.9 (classe R, oracle dagli header e dai campioni); C-02.b-flat §f.6,
// §f.9 (classe R).
// Test:
// - T-M3B-C02-16: analisi dei bias, dei dark, dei dark-flat e dei flat con i parametri sotto; gruppi, esclusioni e
//   proposta contro l'oracle scritto dagli header e dai campioni interi (T_M3B_C02_16_analysis_*); poi i master proposti,
//   eseguiti con `sample_type = f64`, entro la regola I contro l'oracle del test, come T-M2R2-09
//   (T_M3B_C02_16_masters_<ruolo>);
// - T-M3B-FLAT-09: i flat g e i (N = 3) con `mean` + `median_ratio`, `sample_type = f64`: livelli, costanti (m_H, r_f,
//   rho) e master/piano entro la regola I contro l'oracle del test (T_M3B_FLAT_09_band_<g|i>).
// Le riesecuzioni di B17 (T-M2R2-01…09, T-M3A-CAL-07, T-M2F-02) sono test esistenti e non stanno qui.
//
// Il dato non è versionato: se un file manca o ha un SHA-256 o una scala (BZERO, BSCALE) diversi da quelli dichiarati
// nel record del dato, il test FALLISCE con `PREREQUISITO MANCANTE: PUB-LFC-01 <percorso>` (nessuno skip; stessa regola di
// T-M2E-13 e T-M2R2-09). Lungo: label `slow`, esecuzione seriale.
//
// Parametri d'analisi, della ricetta e `A` per banda: solo nel record (docs/datasets/PUB-LFC-01.md, «Parametri d'uso in
// M3.b»); le costanti qui sotto li ripetono come ingressi del test e rimandano al record.
//
// Oracle (nessun valore atteso viene dal prodotto, salvo `κ̂`, che la regola I prende come ingresso puntuale dei piani
// `gaussian_reference`, C-02.b §b.6 «Errore d'arrotondamento»; per n = 3 si controlla anche contro 1 − √3/π):
// - header e campioni: letti da un parser FITS del test (fixture); saturati = campioni >= 65535; livelli del flat =
//   mediana di (x − offset) sui campioni non saturi (conteggio pari: (z_k + z_(k+1)) / 2);
// - gruppi: chiave (dimensioni, INSTRUME, GAIN, FILTER solo per il flat), poi tolleranza di esposizione col riferimento a
//   digest minimo, come C-02.b §b.2 punto 2; proposta: tabella di §b.4. Limiti: la chiave dell'oracle è un sottoinsieme
//   di quella di §b.2 (il dato non ha ROI, readout_mode, row_order né temperatura che la distinguano); nessun motivo
//   `absence` è esercitato dal dato (ogni gruppo ha n >= 3 e stato ok): il ramo che controlla `flat_level` non è eseguito;
// - esclusioni: il dato non ne produce (zero attese, asserite); la prova di ogni motivo di esclusione sta nei test di B12;
// - `node_key` della proposta contro `make_master_node(ricetta)`: coerenza interna del prodotto, non un oracle;
// - valori e piano: b5_oracle/b9_oracle (regola I di ADR-17.b §b.10) sugli x interi; un pixel con un campione saturo è
//   invalido (`saturated`, M e piano NaN), convenzione di T-M2R2-09.
//
// Tolleranze, derivate prima della misura:
// - statistiche per frame (saturi, pixel, livelli): uguaglianza esatta. Saturi e pixel sono interi; i livelli sono
//   mediane di numeri con al più un bit frazionario (x intero, offset mediana di interi) e una media di due: ogni
//   operazione è esatta in f64;
// - master e piano: appartenenza all'intervallo della regola I della sequenza di §b.6 (criterio binario, nessuna
//   tolleranza numerica scelta). Controllo di non vacuità, separato dal criterio: l'ampiezza dell'intervallo di M è al
//   più 40 ε · S, con ε = 2^-53 + 2^-60 e S = max(1, max_f (|offset| + |r_f (x_f − offset)|)) la somma dei moduli dei
//   termini (senza normalizzazione: max_f |x_f|). Derivazione: con n = 3 la costante r_f ha ampiezza relativa
//   <= 2 (n + 3) ε; y_f aggiunge 3 operazioni; la somma di n termini e la divisione ne aggiungono n + 1; in tutto
//   l'ampiezza di M è <= 2 ε S (2 (n + 3) + 4 + n + 1), cioè 2 · 20 ε S = 40 ε S per
//   n = 3 (il caso con `median_ratio`; gli altri hanno meno operazioni). Si normalizza per S e non per |M|: offset +
//   r (x − offset) cancella quando M è piccolo rispetto ai termini. L'ampiezza dell'intervallo del piano si riporta
//   come diagnostica, senza soglia (il piano cancella per costruzione quando i campioni sono quasi uguali);
// - costanti del flat: m_f uguali ai livelli dell'oracle (esatti); r_f e m_H dentro gli intervalli di `scale_intervals`;
//   rho dentro l'intervallo di max_f |r_f − 1| allargato di 2 · 2^-53 relativo (una sottrazione e un valore assoluto).
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/cancel.hpp"
#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/master.hpp"
#include "astro/master_analysis.hpp"
#include "astro/master_kappa.hpp"
#include "astro/memory_pool.hpp"
#include "b9_oracle.hpp"
#include "master_support.hpp"

namespace {

using namespace mst;
using b5o::ivl;
using b5o::ld;
using b9o::method_kind;
namespace fx = astro_fixture;

constexpr const char* kRoot = ASTRO_SOURCE_DIR "/data/public/zenodo-3254683/raw/example-cryo-LFC/";

constexpr double kSaturation = 65535;
constexpr double kSaturatedFractionMax = 0.05;
constexpr double kExposureRelTol = 1e-3;
constexpr double kTemperatureAbsTolC = 1.0;
constexpr double kFlatMinSignalAdu = 1000;
constexpr double kFlatLevelRelTol = 0.2;
// `flat_additive_bound_adu` di T-M3B-FLAT-09 per banda: docs/datasets/PUB-LFC-01.md, «Parametri d'uso in M3.b», tabella di
// `A` (g = 6956/3, i = 8390/3 ADU, massimo per pixel). Con `mean` non entra in nessun valore, né nel piano, né nelle
// costanti; entra nelle diagnostiche del report, che il test controlla.
constexpr double kAdditiveBoundG = 6956.0 / 3.0;
constexpr double kAdditiveBoundI = 8390.0 / 3.0;
constexpr double kWidthRelMax = 40 * (0x1p-53 + 0x1p-60);

// SHA-256 dichiarati in docs/datasets/PUB-LFC-01.md.
const std::map<std::string, std::string> kSha = {
    {"ccd.001.0.fits", "be73dedce9727aa708a5f9427328b6711d697e9d7935053bbd6a1bbd093bf8b9"},
    {"ccd.002.0.fits", "f00343b4bf78bbf84361720e0ffeeda7d936cc025408949e043dc601b60de0e1"},
    {"ccd.003.0.fits", "a315482f22198521752ac62f11c29ae4032c804d48f7872255201187e5f6c18b"},
    {"ccd.004.0.fits", "9c2bb15116593db1936ae6d33b6a7511ccbcadd1d0f89403b46ef961fe008d0e"},
    {"ccd.005.0.fits", "52a02cbca2fdd6cb8159f6525c4e453492fb41e9af30c5a845f3620efa9cea45"},
    {"ccd.006.0.fits", "1112e59f153ac5e69648983310e72ef9ca969e1aa36510e286ecdf69d3f70532"},
    {"ccd.014.0.fits", "b88697d8343b0910a23e108a8fcf7cbf63ad3872fcc0edee4fe12447ad4ddc07"},
    {"ccd.015.0.fits", "74b6b0cb4d39e7ec6722bb78baa988590eae5876adb711a10f6f44fe5b7d73c8"},
    {"ccd.016.0.fits", "67ed4f32553413c6835a07ee2bf40cd1ee6cf169285f0b4ac2373c41879ee20b"},
    {"ccd.017.0.fits", "8f730c5ac91708a9fd5ba23ea7e7fb55aef769d54057f884b8fe826cdc798aae"},
    {"ccd.018.0.fits", "c308de11075112f3fbc705a00758bff74ae2a6e055e07637219f35aa5e7eadc6"},
    {"ccd.019.0.fits", "db6662c74f97490dcbb552145f759372272497f76ea99e88f0860032484c17e8"},
    {"darks/ccd.002.0.fits", "745948e93f5f4ba37f01d7e0182a861bde0012b457c4312e71946718bd182216"},
    {"darks/ccd.013.0.fits", "f62a979edeab66206be26247951e6711f3e65fd73469a72a858025375255e198"},
    {"darks/ccd.014.0.fits", "0817828607ce8bd435ce3a1e66b4f69cb0394c3d3afb7a1bf341188631cb4782"},
    {"darks/ccd.015.0.fits", "e8cf7d754819d91ddb7f66856ad4faa09498d27d50ba1ceab0fc292e328a70d4"},
    {"darks/ccd.017.0.fits", "6a50cd57544b9fd2f5f3d5163724789af306874afe85a9e10edaf75bdebb7f76"},
    {"darks/ccd.018.0.fits", "fc9f1bf817a62d89446743880165340c8cd1624b83c5c7540bb40b5d2d56d364"},
    {"darks/ccd.019.0.fits", "8f25b570f4f59fdd48f5a3bdf57dd7904801093e5beb77c8f30924858facd42b"},
    {"darks/ccd.023.0.fits", "cc6dfea9a17641065910bf6b4288e2d31fc3dc36e57b71c37c65d659ade45bb2"},
    {"darks/ccd.024.0.fits", "7ce339d1aa9fce9409bb9fb2b2e8277946dcb7a4d72e7fdbc4ae93ff5e0def46"},
    {"darks/ccd.025.0.fits", "a253adca4a93ef09dd6b92a970f6881bca847f44b97ad21415cd1a39d64a84c7"},
};

const std::vector<std::string> kBias = {"ccd.001.0.fits", "ccd.002.0.fits", "ccd.003.0.fits", "ccd.004.0.fits",
                                        "ccd.005.0.fits", "ccd.006.0.fits", "darks/ccd.002.0.fits"};
const std::vector<std::string> kDark = {"darks/ccd.013.0.fits", "darks/ccd.014.0.fits", "darks/ccd.015.0.fits"};
const std::vector<std::string> kDarkFlat = {"darks/ccd.017.0.fits", "darks/ccd.018.0.fits", "darks/ccd.019.0.fits",
                                            "darks/ccd.023.0.fits", "darks/ccd.024.0.fits", "darks/ccd.025.0.fits"};
const std::vector<std::string> kFlatG = {"ccd.014.0.fits", "ccd.015.0.fits", "ccd.016.0.fits"};
const std::vector<std::string> kFlatI = {"ccd.017.0.fits", "ccd.018.0.fits", "ccd.019.0.fits"};

// ---------------------------------------------------------------- lettore del test

struct pub_frame {
  std::string rel;
  content_digest digest;
  std::uint32_t w = 0, h = 0;
  double exposure = 0, gain = 0;
  std::string filter, instrume;
  std::vector<std::uint16_t> v;  // campioni fisici (BZERO applicato), 0 … 65535
  fx::bytes file;                // byte verificati contro lo SHA-256 del record

  std::string path() const { return std::string(kRoot) + rel; }
};

std::string card_text(const std::vector<std::string>& cards, const char* key) {
  std::string pad = key;
  pad.resize(8, ' ');
  for (const std::string& card : cards) {
    if (card.compare(0, 8, pad) != 0 || card.size() < 10 || card[8] != '=') continue;
    std::string value = card.substr(10);
    const std::size_t q1 = value.find('\'');
    if (q1 != std::string::npos) {
      const std::size_t q2 = value.find('\'', q1 + 1);
      if (q2 == std::string::npos) throw std::runtime_error(std::string("apice di chiusura assente nella card ") + key);
      std::string s = value.substr(q1 + 1, q2 - q1 - 1);
      while (!s.empty() && s.back() == ' ') s.pop_back();
      return s;
    }
    const std::size_t slash = value.find('/');
    return value.substr(0, slash);
  }
  throw std::runtime_error(std::string("card assente: ") + key);
}

pub_frame load_pub(const std::string& rel) {
  pub_frame f;
  f.rel = rel;
  std::ifstream in(f.path(), std::ios::binary);
  if (!in) throw std::runtime_error("PREREQUISITO MANCANTE: PUB-LFC-01 " + f.path() + " (data/public non è versionato: vedi docs/datasets/PUB-LFC-01.md)");
  f.file.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  const fx::bytes& file = f.file;
  f.digest = digest_of(file);
  if (!kSha.count(rel)) throw std::runtime_error("SHA-256 non dichiarato per " + rel);
  if (f.digest.sha256 != kSha.at(rel)) throw std::runtime_error("PREREQUISITO MANCANTE: PUB-LFC-01 " + f.path() + " (SHA-256 diverso da quello dichiarato)");
  const fx::raw_primary_view hdu = fx::parse_hdu(file);
  if (hdu.bitpix != 16 || hdu.axes.size() != 2) throw std::runtime_error("formato inatteso: " + f.path());
  f.w = static_cast<std::uint32_t>(hdu.axes[0]);
  f.h = static_cast<std::uint32_t>(hdu.axes[1]);
  if (std::stod(card_text(hdu.cards, "BSCALE")) != 1.0 || std::stod(card_text(hdu.cards, "BZERO")) != 32768.0)
    throw std::runtime_error("PREREQUISITO MANCANTE: PUB-LFC-01 " + f.path() + " (BSCALE/BZERO diversi da 1 e 32768)");
  f.exposure = std::stod(card_text(hdu.cards, "EXPTIME"));
  f.gain = std::stod(card_text(hdu.cards, "GAIN"));
  f.filter = card_text(hdu.cards, "FILTER");
  f.instrume = card_text(hdu.cards, "INSTRUME");
  f.v.resize(static_cast<std::size_t>(f.w) * f.h);
  for (std::size_t i = 0; i < f.v.size(); ++i) {
    const auto raw = static_cast<std::int16_t>((hdu.data[2 * i] << 8) | hdu.data[2 * i + 1]);
    f.v[i] = static_cast<std::uint16_t>(static_cast<int>(raw) + 32768);
  }
  return f;
}

std::vector<pub_frame> load_all(const std::vector<std::string>& rels) {
  std::vector<pub_frame> out;
  for (const std::string& r : rels) out.push_back(load_pub(r));
  return out;
}

double median_of(std::vector<double> v) {
  const std::size_t mid = v.size() / 2;
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
  const double hi = v[mid];
  if (v.size() % 2 == 1) return hi;
  return (*std::max_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid)) + hi) / 2.0;
}

// Offset dichiarato: mediana del livello di ccd.001 nella regione dati (colonne 0 … 2047), come real_environment() di
// T-M2E-13.
double declared_offset() {
  static const double value = [] {
    const pub_frame b = load_pub("ccd.001.0.fits");
    std::vector<double> level;
    for (std::uint32_t y = 0; y < b.h; ++y)
      for (std::uint32_t x = 0; x < 2048; ++x) level.push_back(b.v[static_cast<std::size_t>(y) * b.w + x]);
    return median_of(std::move(level));
  }();
  return value;
}

std::uint64_t saturated_count(const pub_frame& f) {
  std::uint64_t n = 0;
  for (std::uint16_t x : f.v) n += x >= kSaturation ? 1 : 0;
  return n;
}

// m_f: mediana di (x − offset) sui campioni non saturi (C-02.b §b.6 «Flat»).
double level_of(const pub_frame& f, double offset) {
  std::vector<double> d;
  d.reserve(f.v.size());
  for (std::uint16_t x : f.v)
    if (x < kSaturation) d.push_back(static_cast<double>(x) - offset);
  return median_of(std::move(d));
}

// ---------------------------------------------------------------- oracle di analisi e proposta

std::vector<std::size_t> canonical_order(const std::vector<pub_frame>& frames, const std::vector<std::size_t>& idx) {
  std::vector<std::size_t> out = idx;
  std::sort(out.begin(), out.end(), [&](std::size_t a, std::size_t b) { return frames[a].digest.sha256 < frames[b].digest.sha256; });
  return out;
}

struct expected_group {
  std::vector<std::size_t> members;  // indici in `frames`, ordine canonico
  std::vector<double> levels;        // solo flat, nell'ordine dei membri
  bool flat_level = false;
  std::optional<method_kind> method;  // assente: nessuna proposta
};

struct expected_analysis {
  std::vector<std::size_t> excluded;
  std::vector<expected_group> groups;
};

bool same_exposure(double a, double b) { return std::fabs(a - b) <= kExposureRelTol * std::max(std::fabs(a), std::fabs(b)); }

expected_analysis expected_of(master_role role, const std::vector<pub_frame>& frames) {
  expected_analysis out;
  const double offset = declared_offset();
  std::vector<std::size_t> usable;
  std::map<std::size_t, double> level;
  for (std::size_t i = 0; i < frames.size(); ++i) {
    const pub_frame& f = frames[i];
    const double fraction = static_cast<double>(saturated_count(f)) / (static_cast<double>(f.w) * f.h);
    if (fraction > kSaturatedFractionMax) {
      out.excluded.push_back(i);
      continue;
    }
    if (role == master_role::flat) {
      level[i] = level_of(f, offset);
      if (level[i] < kFlatMinSignalAdu) {
        out.excluded.push_back(i);
        continue;
      }
    }
    usable.push_back(i);
  }
  out.excluded = canonical_order(frames, out.excluded);
  std::vector<std::size_t> rest = canonical_order(frames, usable);
  const auto same_key = [&](const pub_frame& a, const pub_frame& b) {
    return a.w == b.w && a.h == b.h && a.instrume == b.instrume && a.gain == b.gain &&
           (role != master_role::flat || a.filter == b.filter);
  };
  while (!rest.empty()) {
    const pub_frame& ref = frames[rest.front()];
    expected_group g;
    std::vector<std::size_t> left;
    for (std::size_t i : rest) {
      if (same_key(ref, frames[i]) && same_exposure(ref.exposure, frames[i].exposure)) g.members.push_back(i);
      else left.push_back(i);
    }
    rest = left;
    const std::size_t n = g.members.size();
    if (role == master_role::flat) {
      double sum = 0;
      for (std::size_t i : g.members) {
        g.levels.push_back(level[i]);
        sum += level[i];
      }
      const double mean = sum / static_cast<double>(n);
      for (double l : g.levels) g.flat_level = g.flat_level || std::fabs(l / mean - 1) > kFlatLevelRelTol;
    }
    if (n >= 2 && !g.flat_level) {
      g.method = (n >= 3 && n <= 1024 && role != master_role::flat) ? method_kind::median : method_kind::mean;
    }
    out.groups.push_back(std::move(g));
  }
  return out;
}

// ---------------------------------------------------------------- ingressi del prodotto

std::string declarations_text(double offset) {
  json fields = json::object();
  fields.set("binning_x", json::uinteger(1));
  fields.set("binning_y", json::uinteger(1));
  fields.set("unit", json::string("adu"));
  fields.set("linearity", json::string("linear"));
  json meta = json::object();
  meta.set("offset_adu", json::number(offset));
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  doc.set("declared_image_fields", std::move(fields));
  doc.set("frame_metadata", std::move(meta));
  return canonical_json(doc);
}

master_frame_spec spec_of(const pub_frame& f, double offset) {
  master_frame_spec s;
  s.path = f.path();
  s.digest = f.digest;
  s.declared.offset_adu = metadata_value<double>{offset, {metadata_origin::declared, {}}};
  s.declared_fields.binning_x = 1;
  s.declared_fields.binning_y = 1;
  s.declared_fields.unit = pixel_unit::adu;
  s.declared_fields.lin = linearity::linear;
  s.read_options.declared_phase = process_phase::raw;
  return s;
}

master_analysis_params params_of() {
  master_analysis_params p;
  p.exposure_rel_tol = kExposureRelTol;
  p.temperature_abs_tol_c = kTemperatureAbsTolC;
  p.temp_policy = temperature_policy::not_applicable_declared;
  p.saturation_adu = kSaturation;
  p.saturated_fraction_max = kSaturatedFractionMax;
  p.flat_min_signal_adu = kFlatMinSignalAdu;
  p.flat_level_rel_tol = kFlatLevelRelTol;
  p.offset_adu = declared_offset();
  return p;
}

master_proposal propose(master_role role, const std::vector<pub_frame>& frames, memory_pool& pool) {
  const double offset = declared_offset();
  master_proposal_request rq;
  rq.role = role;
  rq.params = params_of();
  rq.simd = simd_test::requested();
  std::uint64_t id = 1;
  for (const pub_frame& f : frames) {
    master_proposal_frame pf;
    master_stats_request sr;
    sr.saturation_adu = kSaturation;
    sr.flat_levels = role == master_role::flat;
    sr.offset_adu = offset;
    pf.analysis.input_id = id++;
    pf.analysis.digest = f.digest;
    pf.analysis.stats = read_master_frame_stats(f.path(), f.digest, sr, pool, never_cancelled());
    const image img = decode_fits(f.file, cal::big_budget, spec_of(f, offset).read_options);
    pf.analysis.observation = make_observation(img, declarations_text(offset));
    pf.spec = spec_of(f, offset);
    rq.frames.push_back(std::move(pf));
  }
  return propose_masters(rq);
}

// ---------------------------------------------------------------- confronto pixel per pixel

struct pixel_report {
  std::uint64_t pixels = 0, saturated = 0, excluded = 0, wrong_mask = 0, m_outside = 0, v_outside = 0;
  double worst_width_rel = 0, worst_v_width_rel = 0;
  std::uint64_t first_bad = std::numeric_limits<std::uint64_t>::max();
};

struct scales_in {
  const b9o::scales_ivl* ivl;
  double offset;
};

pixel_report compare_pixels(const image& m, const std::vector<const pub_frame*>& group, const b9o::method_spec& spec,
                            const scales_in* scales) {
  pixel_report rep;
  const std::size_t px = group[0]->v.size();
  const std::size_t n = group.size();
  rep.pixels = px;
  std::vector<ivl> y(n);
  for (std::size_t i = 0; i < px; ++i) {
    const double got_m = m.data<double>()[i];
    const double got_v = m.variance_data<double>()[i];
    bool sat = false;
    for (const pub_frame* f : group) sat = sat || f->v[i] >= kSaturation;
    if (sat) {
      ++rep.saturated;
      if (m.invalid.bits[i] != invalid_reason::saturated || !std::isnan(got_m) || !std::isnan(got_v)) ++rep.wrong_mask;
      continue;
    }
    if (m.invalid.bits[i] != 0) {
      ++rep.wrong_mask;
      continue;
    }
    b5o::rule_i ri;
    for (std::size_t j = 0; j < n; ++j) {
      const double x = group[j]->v[i];
      y[j] = scales ? b9o::y_interval(ri, x, scales->offset, scales->ivl->r[j]) : b5o::rule_i::pt(x);
    }
    const b9o::pix_ivl want = b9o::method_interval(spec, y);
    if (ri.excluded || want.excluded) {
      ++rep.excluded;
      continue;
    }
    const std::optional<ld> tau_m = b5o::tau_of(want.m, got_m);
    const std::optional<ld> tau_v = b5o::tau_of(want.v, got_v);
    ld magnitude = 1.0L, square_sum = 0;
    for (std::size_t j = 0; j < n; ++j) {
      const ld x = group[j]->v[i];
      const ld term = scales ? std::fabs(static_cast<ld>(scales->offset)) +
                                   std::fabs(scales->ivl->r[j].hi * (x - static_cast<ld>(scales->offset)))
                             : x;
      magnitude = std::max(magnitude, term);
      square_sum += term * term;
    }
    if (!tau_m) {
      ++rep.m_outside;
      rep.first_bad = std::min<std::uint64_t>(rep.first_bad, i);
    } else {
      rep.worst_width_rel = std::max(rep.worst_width_rel, static_cast<double>((want.m.hi - want.m.lo) / magnitude));
    }
    // Diagnostica del piano: ampiezza su una scala che non si annulla, la somma dei quadrati dei termini (max con 1).
    if (tau_v)
      rep.worst_v_width_rel = std::max(
          rep.worst_v_width_rel, static_cast<double>((want.v.hi - want.v.lo) / std::max<ld>(square_sum, 1.0L)));
    if (!tau_v) {
      ++rep.v_outside;
      rep.first_bad = std::min<std::uint64_t>(rep.first_bad, i);
    }
  }
  return rep;
}

void expect_clean(const pixel_report& r, const std::string& what) {
  std::cout << "  " << what << ": " << r.pixels << " pixel, " << r.saturated << " saturi, ampiezza di M / somma dei moduli, massimo "
            << r.worst_width_rel << ", ampiezza massima del piano / somma dei quadrati dei termini (diagnostica) " << r.worst_v_width_rel << "\n";
  EXPECT_EQ(r.wrong_mask, 0u) << what << ": maschera";
  EXPECT_EQ(r.excluded, 0u) << what << ": pixel fuori dalle condizioni della regola I (l'oracle non si applica)";
  EXPECT_EQ(r.m_outside, 0u) << what << ": M fuori dall'intervallo (primo pixel " << r.first_bad << ")";
  EXPECT_EQ(r.v_outside, 0u) << what << ": piano fuori dall'intervallo (primo pixel " << r.first_bad << ")";
  EXPECT_LE(r.worst_width_rel, kWidthRelMax) << what << ": oracle vacuo";
}

// ---------------------------------------------------------------- esecuzione del job

struct job_product {
  image master;
  json report;
};

job_product run_job(sqlite_dag_store& store, const std::string& slot, const master_recipe& r, memory_pool& pool) {
  const node_spec node = make_master_node(r);
  store.put_node(node);
  store.create_slot(slot, node.key());
  master_job_options o;
  o.resources.worker_count = 4;
  o.resources.tile_rows = 128;
  const job_outcome out = run_master_quiet(store, slot, r, pool, o);
  if (out.exit_code != 0) throw std::runtime_error("job " + slot + ": " + (out.stop ? out.stop->code() + " " + out.stop->what() : "uscita non nulla"));
  const auto roles = by_role(store, node.key());
  job_product p;
  const std::vector<std::uint8_t> img = read_artifact(store, roles.at("primary"));
  p.master = decode_fits(img, cal::big_budget);
  const std::vector<std::uint8_t> rep = read_artifact(store, roles.at("report"));
  p.report = parse_json(std::string(rep.begin(), rep.end()));
  if (p.master.type != sample_type::f64 || !p.master.variance) throw std::runtime_error("master " + slot + ": tipo o piano inatteso");
  return p;
}

std::vector<const pub_frame*> members_of(const std::vector<pub_frame>& frames, const std::vector<std::size_t>& idx) {
  std::vector<const pub_frame*> out;
  for (std::size_t i : idx) out.push_back(&frames[i]);
  return out;
}

std::vector<content_digest> digests_of(const std::vector<pub_frame>& frames, const std::vector<std::size_t>& idx) {
  std::vector<content_digest> out;
  for (std::size_t i : idx) out.push_back(frames[i].digest);
  return out;
}

b9o::method_spec method_spec_of(method_kind k, double kappa) {
  b9o::method_spec s;
  s.k = k;
  s.kappa = kappa;
  return s;
}

// ---------------------------------------------------------------- T-M3B-C02-16: analisi

void check_analysis(master_role role, const std::vector<std::string>& rels) {
  const std::vector<pub_frame> frames = load_all(rels);
  const expected_analysis want = expected_of(role, frames);
  memory_pool pool(4096 * (std::uint64_t{1} << 20));
  const master_proposal got = propose(role, frames, pool);

  EXPECT_TRUE(want.excluded.empty()) << "il dato non produce esclusioni: l'oracle ne ha trovate";
  EXPECT_TRUE(got.screening.missing_params.empty());
  ASSERT_EQ(got.screening.excluded.size(), want.excluded.size());
  for (std::size_t k = 0; k < want.excluded.size(); ++k)
    EXPECT_EQ(got.screening.excluded[k].digest.sha256, frames[want.excluded[k]].digest.sha256);
  ASSERT_EQ(got.groups.size(), want.groups.size());
  for (std::size_t g = 0; g < want.groups.size(); ++g) {
    SCOPED_TRACE("gruppo " + std::to_string(g));
    const expected_group& eg = want.groups[g];
    const proposed_master& pg = got.groups[g];
    const std::vector<content_digest> ed = digests_of(frames, eg.members);
    ASSERT_EQ(pg.frames.size(), ed.size());
    for (std::size_t k = 0; k < ed.size(); ++k) EXPECT_EQ(pg.frames[k].sha256, ed[k].sha256);
    EXPECT_EQ(pg.group.status, eg.flat_level ? master_group_status::flat_level : master_group_status::ok);
    if (role == master_role::flat) {
      ASSERT_EQ(pg.group.flat_levels.size(), eg.levels.size());
      for (std::size_t k = 0; k < eg.levels.size(); ++k) {
        ASSERT_EQ(pg.group.flat_levels[k].size(), 1u);
        EXPECT_EQ(pg.group.flat_levels[k][0], eg.levels[k]) << "livello del frame " << k;
      }
    }
    if (!eg.method) {
      EXPECT_FALSE(pg.method.has_value());
      EXPECT_FALSE(pg.recipe.has_value());
      if (eg.flat_level) {
        EXPECT_EQ(pg.absence, "flat_level");
      }
      continue;
    }
    ASSERT_TRUE(pg.method.has_value()) << "assenza: " << pg.absence;
    EXPECT_EQ(pg.method->combine, *eg.method == method_kind::median ? combine_method::median : combine_method::mean);
    EXPECT_EQ(pg.method->rejection, master_rejection::none);
    ASSERT_TRUE(pg.recipe.has_value());
    ASSERT_TRUE(pg.node.has_value());
    EXPECT_EQ(pg.node_key, make_master_node(*pg.recipe).key()) << "coerenza interna del prodotto";
  }
  // Statistiche per frame: ingresso dell'analisi, contro i campioni interi.
  for (std::size_t i = 0; i < frames.size(); ++i) {
    master_stats_request sr;
    sr.saturation_adu = kSaturation;
    sr.flat_levels = role == master_role::flat;
    sr.offset_adu = declared_offset();
    const master_frame_stats s = read_master_frame_stats(frames[i].path(), frames[i].digest, sr, pool, never_cancelled());
    EXPECT_FALSE(s.decode_failure.has_value()) << frames[i].rel;
    EXPECT_EQ(s.saturated_samples, saturated_count(frames[i])) << frames[i].rel;
    EXPECT_EQ(s.pixel_count, static_cast<std::uint64_t>(frames[i].w) * frames[i].h) << frames[i].rel;
    if (role == master_role::flat) {
      ASSERT_EQ(s.levels.size(), 1u);
      ASSERT_TRUE(s.levels[0].has_value());
      EXPECT_EQ(*s.levels[0], level_of(frames[i], declared_offset())) << frames[i].rel;
    }
  }
}

TEST(MasterB17Slow, T_M3B_C02_16_analysis_bias) { check_analysis(master_role::bias, kBias); }
TEST(MasterB17Slow, T_M3B_C02_16_analysis_dark) { check_analysis(master_role::dark, kDark); }
TEST(MasterB17Slow, T_M3B_C02_16_analysis_dark_flat) { check_analysis(master_role::dark_flat, kDarkFlat); }
TEST(MasterB17Slow, T_M3B_C02_16_analysis_flat) {
  std::vector<std::string> rels = kFlatG;
  rels.insert(rels.end(), kFlatI.begin(), kFlatI.end());
  check_analysis(master_role::flat, rels);
}

// ---------------------------------------------------------------- T-M3B-C02-16: master proposti

void check_proposed_masters(master_role role, const std::vector<std::string>& rels, const std::string& tag) {
  const std::vector<pub_frame> frames = load_all(rels);
  const expected_analysis want = expected_of(role, frames);
  memory_pool pool(4096 * (std::uint64_t{1} << 20));
  const master_proposal got = propose(role, frames, pool);
  ASSERT_EQ(got.groups.size(), want.groups.size());
  const fs::path dir = fresh_dir("b17_c02_16_" + tag);
  auto store = sqlite_dag_store::create(dir / "store");
  std::size_t executed = 0;
  for (std::size_t g = 0; g < want.groups.size(); ++g) {
    const expected_group& eg = want.groups[g];
    if (!eg.method) continue;
    ASSERT_TRUE(got.groups[g].recipe.has_value()) << "gruppo " << g << " senza proposta";
    master_recipe r = *got.groups[g].recipe;
    r.output_type = sample_type::f64;
    const std::string slot = tag + "_g" + std::to_string(g);
    const job_product p = run_job(*store, slot, r, pool);
    double kappa = 0;
    if (*eg.method == method_kind::median) {
      kappa = cal::as_double(cal::at(p.report, {"constants", "kappa"}));
      if (eg.members.size() == 3) {
        const double exact = 1.0 - std::sqrt(3.0) / std::acos(-1.0);
        EXPECT_LE(std::fabs(kappa - exact), 1e-9 * exact) << "κ(3) riportato";
      }
    } else {
      EXPECT_EQ(cal::at(p.report, {"constants", "kappa"}).type(), json::kind::null);
    }
    expect_clean(compare_pixels(p.master, members_of(frames, eg.members), method_spec_of(*eg.method, kappa), nullptr),
                 tag + " gruppo " + std::to_string(g) + " (" + std::to_string(eg.members.size()) + " frame)");
    ++executed;
  }
  EXPECT_GT(executed, 0u) << "nessun master proposto";
}

TEST(MasterB17Slow, T_M3B_C02_16_masters_bias) { check_proposed_masters(master_role::bias, kBias, "bias"); }
TEST(MasterB17Slow, T_M3B_C02_16_masters_dark) { check_proposed_masters(master_role::dark, kDark, "dark"); }
TEST(MasterB17Slow, T_M3B_C02_16_masters_dark_flat) { check_proposed_masters(master_role::dark_flat, kDarkFlat, "dark_flat"); }
TEST(MasterB17Slow, T_M3B_C02_16_masters_flat) {
  std::vector<std::string> rels = kFlatG;
  rels.insert(rels.end(), kFlatI.begin(), kFlatI.end());
  check_proposed_masters(master_role::flat, rels, "flat");
}

// ---------------------------------------------------------------- T-M3B-FLAT-09

master_recipe median_ratio_recipe(const std::vector<pub_frame>& frames, double additive_bound) {
  master_recipe r;
  r.role = master_role::flat;
  for (const pub_frame& f : frames) r.frames.push_back(spec_of(f, declared_offset()));
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  r.flat_norm = flat_normalization::median_ratio;
  r.flat_min_signal_adu = kFlatMinSignalAdu;
  r.flat_additive_bound_adu = additive_bound;
  r.saturation_adu = kSaturation;
  r.saturated_fraction_max = kSaturatedFractionMax;
  r.exposure_rel_tol = kExposureRelTol;
  r.temperature_abs_tol_c = kTemperatureAbsTolC;
  r.temp_policy = temperature_policy::not_applicable_declared;
  r.simd = simd_test::requested();
  r.output_type = sample_type::f64;
  return r;
}

void check_band(const std::vector<std::string>& rels, const std::string& tag, double additive_bound) {
  const std::vector<pub_frame> frames = load_all(rels);
  const double offset = declared_offset();
  std::vector<std::size_t> all(frames.size());
  std::iota(all.begin(), all.end(), std::size_t{0});
  const std::vector<std::size_t> order = canonical_order(frames, all);
  std::vector<double> levels;
  for (std::size_t i : order) levels.push_back(level_of(frames[i], offset));
  const b9o::scales_ivl scales = b9o::scale_intervals(levels);
  ASSERT_FALSE(scales.excluded);

  memory_pool pool(4096 * (std::uint64_t{1} << 20));
  const fs::path dir = fresh_dir("b17_flat_09_" + tag);
  auto store = sqlite_dag_store::create(dir / "store");
  const job_product p = run_job(*store, "flat_" + tag, median_ratio_recipe(frames, additive_bound), pool);

  // Livelli e costanti del report (nell'ordine canonico dei frame).
  const auto& list = cal::at(p.report, {"frames"}).items();
  ASSERT_EQ(list.size(), order.size());
  for (std::size_t j = 0; j < order.size(); ++j)
    EXPECT_EQ(cal::as_double(cal::at(list[j], {"flat_level"})), levels[j]) << "livello del frame " << j;
  const auto& sc = cal::at(p.report, {"constants", "flat_scales"}).items();
  ASSERT_EQ(sc.size(), 1u);
  const auto& m_rep = cal::at(sc[0], {"m"}).items();
  const auto& r_rep = cal::at(sc[0], {"r"}).items();
  ASSERT_EQ(m_rep.size(), levels.size());
  ASSERT_EQ(r_rep.size(), levels.size());
  ld rho_lo = 0, rho_hi = 0;
  for (std::size_t j = 0; j < levels.size(); ++j) {
    EXPECT_EQ(cal::as_double(m_rep[j]), levels[j]);
    EXPECT_TRUE(b5o::tau_of(scales.r[j], cal::as_double(r_rep[j])).has_value()) << "r_f del frame " << j << " fuori dall'intervallo";
    const ld lo = scales.r[j].lo - 1, hi = scales.r[j].hi - 1;
    rho_hi = std::max(rho_hi, std::max(std::fabs(lo), std::fabs(hi)));
    rho_lo = std::max(rho_lo, (lo <= 0 && hi >= 0) ? ld{0} : std::min(std::fabs(lo), std::fabs(hi)));
  }
  EXPECT_TRUE(b5o::tau_of(scales.m_h, cal::as_double(cal::at(sc[0], {"m_H"}))).has_value()) << "m_H fuori dall'intervallo";
  const ivl rho{rho_lo * (1 - 2 * 0x1p-53L), rho_hi * (1 + 2 * 0x1p-53L)};
  EXPECT_TRUE(b5o::tau_of(rho, cal::as_double(cal::at(sc[0], {"rho"}))).has_value()) << "rho fuori dall'intervallo";

  // Diagnostiche che dipendono da A (C-02.b-flat §f.7): con `mean` nessun budget, quindi `check_value` e `rel_budget`
  // null; `norm_level_bias_bound` = A² Σ (r_f − 1)² / (n − 1) / n (§f.6), `level_ratio` = max m / min m.
  const auto& chans = cal::at(p.report, {"channels"}).items();
  ASSERT_EQ(chans.size(), 1u);
  const json& fn = cal::at(chans[0], {"flat_normalization"});
  EXPECT_EQ(cal::as_double(cal::at(fn, {"additive_bound_adu"})), additive_bound);
  EXPECT_EQ(cal::at(fn, {"check_value"}).type(), json::kind::null);
  EXPECT_EQ(cal::at(fn, {"rel_budget"}).type(), json::kind::null);
  {
    b5o::rule_i ri;
    const double n = static_cast<double>(levels.size());
    ivl ss = ri.sqr(ri.sub(scales.r[0], b5o::rule_i::pt(1.0)));
    for (std::size_t j = 1; j < levels.size(); ++j) ss = ri.add(ss, ri.sqr(ri.sub(scales.r[j], b5o::rule_i::pt(1.0))));
    const ivl var_r = ri.div(ss, b5o::rule_i::pt(n - 1));
    const ivl bound = ri.div(ri.mul(ri.sqr(b5o::rule_i::pt(additive_bound)), var_r), b5o::rule_i::pt(n));
    ASSERT_FALSE(ri.excluded);
    EXPECT_TRUE(b5o::tau_of(bound, cal::as_double(cal::at(fn, {"norm_level_bias_bound"}))).has_value())
        << "norm_level_bias_bound fuori dall'intervallo";
    const ivl ratio = ri.div(b5o::rule_i::pt(*std::max_element(levels.begin(), levels.end())),
                             b5o::rule_i::pt(*std::min_element(levels.begin(), levels.end())));
    EXPECT_TRUE(b5o::tau_of(ratio, cal::as_double(cal::at(fn, {"level_ratio"}))).has_value()) << "level_ratio fuori dall'intervallo";
  }

  // Master e piano.
  const scales_in si{&scales, offset};
  expect_clean(compare_pixels(p.master, members_of(frames, order), method_spec_of(method_kind::mean, 0), &si),
               "flat " + tag + " mean + median_ratio");
}

TEST(MasterB17Slow, T_M3B_FLAT_09_band_g) { check_band(kFlatG, "g", kAdditiveBoundG); }
TEST(MasterB17Slow, T_M3B_FLAT_09_band_i) { check_band(kFlatI, "i", kAdditiveBoundI); }

}  // namespace
