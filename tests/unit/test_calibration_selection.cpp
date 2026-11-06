// U7 di M3.a (G-CAL parte 2, G-OFFSET con P-OFFSET): predicati, insieme ammissibile, default D-16, selezione.
// ADR-16.b §b.1 punti 1-2, §b.4, §b.5, §b.6, §b.9; C-01.b §1; D-16. Test: T-M3A-CAL-01, 02, 06; T-M3A-C01-02.
// Oracle: le regole di §b.5 e §b.6 ricodificate a mano qui sotto (`oracle`), controllate dalla tabella scritta a mano
// (`hand_rows`, almeno una configurazione per riga di §b.6 e per catena di §b.5). Nessun valore atteso viene dal prodotto.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/calibration_analysis.hpp"
#include "astro/error.hpp"
#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "exact_rational.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;
using exact::rat;

constexpr std::uint32_t frame_w = 4;
constexpr std::uint32_t frame_h = 2;
constexpr std::size_t frame_n = std::size_t{frame_w} * frame_h;
constexpr std::uint64_t decode_budget = std::uint64_t{1} << 26;

// ---------------------------------------------------------------- nomi delle catene (§b.5), scritti a mano

const char* name_of(light_chain c) {
  switch (c) {
    case light_chain::none: return "light.none";
    case light_chain::bias: return "light.bias";
    case light_chain::dark: return "light.dark";
    case light_chain::bias_dark_sub: return "light.bias_dark_sub";
    case light_chain::dark_sub_partial: return "light.dark_sub_partial";
  }
  return "?";
}

const char* name_of(flat_chain c) {
  switch (c) {
    case flat_chain::none: return "flat.none";
    case flat_chain::dark_flat: return "flat.dark_flat";
    case flat_chain::bias_dark_flat_sub: return "flat.bias_dark_flat_sub";
    case flat_chain::bias: return "flat.bias";
    case flat_chain::normalize: return "flat.normalize";
    case flat_chain::given_q: return "flat.given_q";
  }
  return "?";
}

const light_chain all_light[] = {light_chain::none, light_chain::bias, light_chain::dark, light_chain::bias_dark_sub,
                                 light_chain::dark_sub_partial};
const flat_chain all_flat[] = {flat_chain::none,      flat_chain::dark_flat, flat_chain::bias_dark_flat_sub,
                               flat_chain::bias,      flat_chain::normalize, flat_chain::given_q};

// ---------------------------------------------------------------- configurazioni di T-M3A-CAL-01

enum class dark_kind { absent, d, d_prime, scaled, unknown, incompatible };
enum class bias_kind { absent, valid, neither, only_light, only_flat };
enum class flat_kind { absent, raw, corrected_unnormalized, calibrated, calibrated_no_normalization, unknown, incompatible };
enum class dark_flat_kind { absent, inc, sub, incompatible, unknown, scaled };
enum class thermal_kind { absent, in_budget, out_of_budget };

struct config {
  bool raw_light = false;
  dark_kind dark = dark_kind::absent;
  bias_kind bias = bias_kind::absent;
  flat_kind flat = flat_kind::absent;
  dark_flat_kind dark_flat = dark_flat_kind::absent;
  thermal_kind thermal = thermal_kind::absent;
};

std::string describe(const config& c) {
  static const char* dk[] = {"absent", "D", "D'", "scaled", "unknown", "incompatible"};
  static const char* bk[] = {"absent", "valid", "neither", "only_L", "only_F"};
  static const char* fk[] = {"absent", "raw", "corrected_unnormalized", "calibrated", "calibrated_no_norm", "unknown",
                             "incompatible"};
  static const char* dfk[] = {"absent", "INC", "SUB", "incompatible", "unknown", "scaled"};
  static const char* tk[] = {"absent", "in_budget", "out_of_budget"};
  std::ostringstream s;
  s << "light=" << (c.raw_light ? "RAW" : "FITS") << " dark=" << dk[static_cast<int>(c.dark)]
    << " bias=" << bk[static_cast<int>(c.bias)] << " flat=" << fk[static_cast<int>(c.flat)]
    << " dark_flat=" << dfk[static_cast<int>(c.dark_flat)] << " thermal=" << tk[static_cast<int>(c.thermal)];
  return s.str();
}

// Valori dei frame e delle dichiarazioni. Compatibilità per coppia provocata solo con:
//  - esposizione (L–D, F–DF; `exposure_rel_tol` = 0): light e dark 60 s, flat e dark-flat 5 s; dark incompatibile 30 s,
//    dark-flat incompatibile 7 s;
//  - temperatura (L–B, F–B; tolleranza 3 °C): light e dark −10, flat e dark-flat −6; bias valido −8, solo L −11,
//    solo F −5, né L né F 0 (L–F non confronta la temperatura, ADR-16.a §2);
//  - filtro (L–F): flat incompatibile con FILTER = R invece di L.
// Controllo termico di `flat.bias`: F = 1124, B = 100, quindi Fc = 1024 = N; q_min = 0,5, q_min · N = 512;
// budget 0,25 (soglia 128): nel budget bound 64 (β = 1/8), fuori budget bound 256.
struct tuning {
  std::vector<double> flat_raw = std::vector<double>(frame_n, 1124.0);
  std::vector<double> bias = std::vector<double>(frame_n, 100.0);
  double q_min = 0.5;
  normalization_mode normalization = normalization_mode::global_median;
  double bound_in = 64.0;
  double bound_out = 256.0;
  double budget = 0.25;
  bool mono = false;  // light, bias e flat `raw` senza BAYERPAT (layout mono)
};

constexpr double light_value = 500.0;
constexpr double tol_temperature = 3.0;

bool bias_light_ok(bias_kind b) { return b == bias_kind::valid || b == bias_kind::only_light; }
bool bias_flat_ok(bias_kind b) { return b == bias_kind::valid || b == bias_kind::only_flat; }

// ---------------------------------------------------------------- costruzione degli ingressi

struct built_frame {
  image img;
  content_digest digest;
};

built_frame frame(const char* imagetyp, double exposure, double temperature, const char* filter,
                  const std::vector<double>& values, bool mono = false) {
  fx::calib_header h;
  h.imagetyp = imagetyp;
  h.exposure = exposure;
  h.temperature = temperature;
  h.filter = filter;
  h.bayer = mono ? "" : "RGGB";
  const fx::bytes bytes = fx::float_frame(frame_w, frame_h, -64, values, fx::calib_cards(h));
  return {decode_fits(bytes, decode_budget), digest_of(bytes)};
}

std::vector<double> constant(double v) { return std::vector<double>(frame_n, v); }

decoder_state raw_state() {
  decoder_state s;
  s.raw_width = frame_w;
  s.raw_height = frame_h;
  s.window = decoder_window{0, 0, frame_w, frame_h};
  s.black = 64;
  s.cfa_color_index = {0, 1, 3, 2};  // RGGB all'origine: R, G, G2, B
  s.white = 4095;
  s.source.origin = metadata_origin::decoder;
  return s;
}

decoder_identity raw_identity() {
  decoder_identity id;
  id.libraw_version = "0.22.3";
  id.libraw_version_number = 5635;
  id.libraw_capabilities = 0;
  id.libraw_tarball_sha256 = std::string(64, '0');
  return id;
}

master_descriptor descriptor_of(master_role role) {
  master_descriptor d;
  d.role = role;
  d.combine_count = 4;
  d.combine = combine_method::mean;
  return d;
}

master_descriptor additive_descriptor(master_role role, additive_state a, scaling_state s) {
  master_descriptor d = descriptor_of(role);
  d.additive = a;
  d.scaling = s;
  return d;
}

master_descriptor flat_descriptor(flat_state s, std::optional<normalization_mode> declared = std::nullopt) {
  master_descriptor d = descriptor_of(master_role::flat);
  d.flat = s;
  d.flat_normalization_declared = declared;
  return d;
}

analysis_candidate candidate(built_frame f, master_descriptor d, bool raw) {
  if (raw) {
    d.frame_decoder_states = std::vector<frame_decoder_state>(*d.combine_count, frame_decoder_state{raw_state(), raw_identity()});
  }
  analysis_candidate c;
  c.img = std::move(f.img);
  c.descriptor = std::move(d);
  c.digest = std::move(f.digest);
  return c;
}

std::optional<analysis_candidate> dark_of(dark_kind k, bool raw) {
  using as = additive_state;
  using ss = scaling_state;
  switch (k) {
    case dark_kind::absent: return std::nullopt;
    case dark_kind::d:
      return candidate(frame("Dark Frame", 60, -10, "L", constant(200)), additive_descriptor(master_role::dark, as::bias_included, ss::unscaled), raw);
    case dark_kind::d_prime:
      return candidate(frame("Dark Frame", 60, -10, "L", constant(90)), additive_descriptor(master_role::dark, as::bias_subtracted, ss::unscaled), raw);
    case dark_kind::scaled:
      return candidate(frame("Dark Frame", 60, -10, "L", constant(201)), additive_descriptor(master_role::dark, as::bias_included, ss::scaled), raw);
    case dark_kind::unknown:
      return candidate(frame("Dark Frame", 60, -10, "L", constant(202)), additive_descriptor(master_role::dark, as::unknown, ss::unscaled), raw);
    case dark_kind::incompatible:
      return candidate(frame("Dark Frame", 30, -10, "L", constant(203)), additive_descriptor(master_role::dark, as::bias_included, ss::unscaled), raw);
  }
  return std::nullopt;
}

std::optional<analysis_candidate> bias_of(bias_kind k, bool raw, const tuning& t) {
  double temperature = 0;
  switch (k) {
    case bias_kind::absent: return std::nullopt;
    case bias_kind::valid: temperature = -8; break;
    case bias_kind::neither: temperature = 0; break;
    case bias_kind::only_light: temperature = -11; break;
    case bias_kind::only_flat: temperature = -5; break;
  }
  return candidate(frame("Bias Frame", 0.001, temperature, "L", t.bias, t.mono), descriptor_of(master_role::bias), raw);
}

std::optional<analysis_candidate> flat_of(flat_kind k, bool raw, const tuning& t) {
  switch (k) {
    case flat_kind::absent: return std::nullopt;
    case flat_kind::raw: return candidate(frame("Flat Field", 5, -6, "L", t.flat_raw, t.mono), flat_descriptor(flat_state::raw), raw);
    case flat_kind::corrected_unnormalized:
      return candidate(frame("Flat Field", 5, -6, "L", constant(1024)), flat_descriptor(flat_state::corrected_unnormalized), raw);
    case flat_kind::calibrated: {
      analysis_candidate c = candidate(frame("Flat Field", 5, -6, "L", constant(1.0)),
                                       flat_descriptor(flat_state::calibrated, normalization_mode::global_median), raw);
      c.img.unit = pixel_unit::dimensionless;
      return c;
    }
    case flat_kind::calibrated_no_normalization: {
      analysis_candidate c = candidate(frame("Flat Field", 5, -6, "L", constant(1.0)), flat_descriptor(flat_state::calibrated), raw);
      c.img.unit = pixel_unit::dimensionless;
      return c;
    }
    case flat_kind::unknown: return candidate(frame("Flat Field", 5, -6, "L", constant(1125)), flat_descriptor(flat_state::unknown), raw);
    case flat_kind::incompatible: return candidate(frame("Flat Field", 5, -6, "R", t.flat_raw), flat_descriptor(flat_state::raw), raw);
  }
  return std::nullopt;
}

std::optional<analysis_candidate> dark_flat_of(dark_flat_kind k, bool raw) {
  using as = additive_state;
  using ss = scaling_state;
  switch (k) {
    case dark_flat_kind::absent: return std::nullopt;
    case dark_flat_kind::inc:
      return candidate(frame("Dark Flat", 5, -6, "L", constant(150)), additive_descriptor(master_role::dark_flat, as::bias_included, ss::unscaled), raw);
    case dark_flat_kind::sub:
      return candidate(frame("Dark Flat", 5, -6, "L", constant(50)), additive_descriptor(master_role::dark_flat, as::bias_subtracted, ss::unscaled), raw);
    case dark_flat_kind::incompatible:
      return candidate(frame("Dark Flat", 7, -6, "L", constant(151)), additive_descriptor(master_role::dark_flat, as::bias_included, ss::unscaled), raw);
    case dark_flat_kind::unknown:
      return candidate(frame("Dark Flat", 5, -6, "L", constant(152)), additive_descriptor(master_role::dark_flat, as::unknown, ss::unscaled), raw);
    case dark_flat_kind::scaled:
      return candidate(frame("Dark Flat", 5, -6, "L", constant(153)), additive_descriptor(master_role::dark_flat, as::bias_included, ss::scaled), raw);
  }
  return std::nullopt;
}

calibration_analysis_input input_of(const config& c, const tuning& t = {}) {
  calibration_analysis_input in;
  built_frame light = frame("Light Frame", 60, -10, "L", constant(light_value), t.mono);
  in.light = std::move(light.img);
  in.light_digest = std::move(light.digest);
  if (c.raw_light) in.light.decoder_state = raw_state();
  in.dark = dark_of(c.dark, c.raw_light);
  in.bias = bias_of(c.bias, c.raw_light, t);
  in.flat = flat_of(c.flat, c.raw_light, t);
  in.dark_flat = dark_flat_of(c.dark_flat, c.raw_light);
  calibration_declarations& d = in.declarations;
  d.exposure_rel_tol = 0.0;
  d.temperature_abs_tol_c = tol_temperature;
  d.temp_policy = temperature_policy::compare;
  d.normalization = t.normalization;
  d.q_min = t.q_min;
  d.q_rel_sigma_max = 10.0;
  if (c.raw_light) d.decoder_black_abs_tol_adu = 0u;
  if (c.thermal != thermal_kind::absent) {
    d.flat_thermal_bound_adu = c.thermal == thermal_kind::in_budget ? t.bound_in : t.bound_out;
    d.flat_thermal_rel_budget = t.budget;
  }
  return in;
}

// ---------------------------------------------------------------- oracle ricodificato da §b.3–§b.6

struct expected_verdict {
  std::string chain;
  bool usable = true;
  std::string reason;  // vuoto se usable
};

struct expected_candidate {
  master_role role = master_role::bias;
  bool usable = false;
  // all: elenco esatto; light_only: solo le catene del light; any_with_reason: almeno una catena, tutte excluded con
  // `state_reason` (master escluso per stato: quali catene non è fissato); none: non si confronta.
  enum class compare { all, light_only, any_with_reason, none } chains_compared = compare::all;
  std::vector<expected_verdict> chains;
  std::string state_reason;
  // Catene escluse dal confronto (esito non fissato dal contratto, vedi `oracle`).
  std::vector<std::string> skipped;
};

using pair_key = std::tuple<light_chain, flat_chain, bool>;

struct expected_analysis {
  std::vector<expected_candidate> candidates;
  std::optional<std::string> dfb;  // nullopt: non determinato
  dark_flat_predicate df = dark_flat_predicate::absent;
  std::optional<flat_state> flat_available;
  std::vector<pair_key> admissible;  // ordinato
  light_chain default_light = light_chain::none;
  flat_chain default_flat = flat_chain::none;
};

void state_excluded(expected_candidate& c, const char* reason) {
  c.chains_compared = expected_candidate::compare::any_with_reason;
  c.state_reason = reason;
}

std::vector<expected_verdict> uniform(std::initializer_list<const char*> chains, const std::string& reason) {
  std::vector<expected_verdict> v;
  for (const char* c : chains) v.push_back({c, reason.empty(), reason});
  return v;
}

offset_state offset_after(light_chain lc, bool raw_light) {
  if (lc == light_chain::bias || lc == light_chain::dark || lc == light_chain::bias_dark_sub) return offset_state::removed;
  return raw_light ? offset_state::known_not_removed : offset_state::unknown;
}

int light_priority(light_chain c) {  // §b.6: dark > bias_dark_sub > dark_sub_partial > bias > none
  switch (c) {
    case light_chain::dark: return 5;
    case light_chain::bias_dark_sub: return 4;
    case light_chain::dark_sub_partial: return 3;
    case light_chain::bias: return 2;
    case light_chain::none: return 1;
  }
  return 0;
}

int flat_priority(flat_chain c) {  // §b.6: given_q o normalize > dark_flat > bias_dark_flat_sub > bias > none
  switch (c) {
    case flat_chain::given_q:
    case flat_chain::normalize: return 5;
    case flat_chain::dark_flat: return 4;
    case flat_chain::bias_dark_flat_sub: return 3;
    case flat_chain::bias: return 2;
    case flat_chain::none: return 1;
  }
  return 0;
}

// Bit z (§b.4, §b.6): una catena conta solo se tutti i suoi membri sono ammessi, e la riga DFB si calcola senza i
// master esclusi; senza flat candidato o con il flat escluso, z dipende dalle sole catene del light. Lo stato del bias
// resta quello dei verdetti di coppia (§b.1), anche quando z = 0.
// Interpretazioni del test dove il contratto tace, dichiarate nel record dei fatti:
//  - le catene di un candidato sono quelle di §b.5 che accettano il suo stato, in ordine di tabella; con uno stato
//    escluso (unknown, scaled, incompleto) almeno una catena, tutte excluded con il motivo di stato (quali non è fissato);
//  - una coppia si valuta solo se l'altro candidato è presente (anche escluso, come per il dark-flat in §b.3);
//    l'esclusione di coppia va al master (L–X al master X; F–DF al dark-flat; F–B al bias);
//  - `flat_thermal_budget` esclude la catena `flat.bias` del flat (§b.3: «esclude la sola catena flat.bias, non il flat»);
//  - senza flat candidato le catene del flat del bias e del dark-flat non si confrontano;
//  - con un bias presente ma escluso dalla coppia F–B e dichiarazioni termiche presenti, il verdetto `flat.bias` del flat
//    non si confronta (non è fissato se il controllo termico si calcoli con quel bias).
expected_analysis oracle(const config& c) {
  expected_analysis e;
  const bool flat_present = c.flat != flat_kind::absent;

  // bias
  bool bias_usable_light = false;
  bool bias_usable_flat = false;
  if (c.bias != bias_kind::absent) {
    expected_candidate b;
    b.role = master_role::bias;
    const std::string lr = bias_light_ok(c.bias) ? "" : "incompatible.temperature_c";
    bias_usable_light = lr.empty();
    b.chains = uniform({"light.bias", "light.bias_dark_sub"}, lr);
    if (flat_present) {
      const std::string fr = bias_flat_ok(c.bias) ? "" : "incompatible.temperature_c";
      bias_usable_flat = fr.empty();
      for (const auto& v : uniform({"flat.bias_dark_flat_sub", "flat.bias"}, fr)) b.chains.push_back(v);
      b.usable = bias_usable_light || bias_usable_flat;
    } else {
      b.chains_compared = expected_candidate::compare::light_only;
      b.usable = bias_usable_light;
    }
    e.candidates.push_back(b);
  }

  // dark
  bool d_usable = false;
  bool dprime_usable = false;
  if (c.dark != dark_kind::absent) {
    expected_candidate d;
    d.role = master_role::dark;
    switch (c.dark) {
      case dark_kind::d: d.chains = uniform({"light.dark"}, ""); d_usable = true; break;
      case dark_kind::d_prime: d.chains = uniform({"light.bias_dark_sub", "light.dark_sub_partial"}, ""); dprime_usable = true; break;
      case dark_kind::scaled: state_excluded(d, "state_scaled"); break;
      case dark_kind::unknown: state_excluded(d, "state_unknown"); break;
      case dark_kind::incompatible: d.chains = uniform({"light.dark"}, "incompatible.exposure_s"); break;
      case dark_kind::absent: break;
    }
    d.usable = d_usable || dprime_usable;
    e.candidates.push_back(d);
  }

  // dark-flat: si confronta col flat candidato anche se escluso; senza flat, P-DF dal solo stato (§b.3)
  bool df_inc_usable = false;
  bool df_sub_usable = false;
  if (c.dark_flat != dark_flat_kind::absent) {
    expected_candidate f;
    f.role = master_role::dark_flat;
    switch (c.dark_flat) {
      case dark_flat_kind::inc: f.chains = uniform({"flat.dark_flat"}, ""); df_inc_usable = true; break;
      case dark_flat_kind::sub: f.chains = uniform({"flat.bias_dark_flat_sub"}, ""); df_sub_usable = true; break;
      case dark_flat_kind::incompatible:
        f.chains = uniform({"flat.dark_flat"}, flat_present ? "incompatible.exposure_s" : "");
        df_inc_usable = !flat_present;
        break;
      case dark_flat_kind::unknown: state_excluded(f, "state_unknown"); break;
      case dark_flat_kind::scaled: state_excluded(f, "state_scaled"); break;
      case dark_flat_kind::absent: break;
    }
    if (!flat_present && f.chains_compared == expected_candidate::compare::all) f.chains_compared = expected_candidate::compare::none;
    f.usable = df_inc_usable || df_sub_usable;
    e.candidates.push_back(f);
    e.df = df_inc_usable ? dark_flat_predicate::inc : df_sub_usable ? dark_flat_predicate::sub : dark_flat_predicate::bad;
  }

  // flat
  bool flat_raw_usable = false;
  bool flat_bias_thermal_ok = false;
  if (flat_present) {
    expected_candidate f;
    f.role = master_role::flat;
    switch (c.flat) {
      case flat_kind::raw: {
        flat_raw_usable = true;
        const bool checked = bias_usable_flat && c.thermal != thermal_kind::absent;
        const bool out = checked && c.thermal == thermal_kind::out_of_budget;
        flat_bias_thermal_ok = checked && !out;
        f.chains = uniform({"flat.dark_flat", "flat.bias_dark_flat_sub"}, "");
        // Bias presente ma escluso dalla coppia F–B, dichiarazioni presenti: se il controllo termico si calcoli con quel
        // bias non è fissato, quindi il verdetto `flat.bias` del flat non si confronta.
        if (c.bias != bias_kind::absent && !bias_usable_flat && c.thermal != thermal_kind::absent) f.skipped.push_back("flat.bias");
        else f.chains.push_back({"flat.bias", !out, out ? "flat_thermal_budget" : ""});
        e.flat_available = flat_state::raw;
        break;
      }
      case flat_kind::corrected_unnormalized:
        f.chains = uniform({"flat.normalize"}, "");
        e.flat_available = flat_state::corrected_unnormalized;
        break;
      case flat_kind::calibrated:
        f.chains = uniform({"flat.given_q"}, "");
        e.flat_available = flat_state::calibrated;
        break;
      case flat_kind::calibrated_no_normalization: state_excluded(f, "state_incomplete"); break;
      case flat_kind::unknown: state_excluded(f, "state_unknown"); break;
      case flat_kind::incompatible:
        f.chains = uniform({"flat.dark_flat", "flat.bias_dark_flat_sub", "flat.bias"}, "incompatible.filter");
        break;
      case flat_kind::absent: break;
    }
    f.usable = e.flat_available.has_value();
    e.candidates.push_back(f);
  }

  // P-DFBxyz (§b.4)
  const bool x = d_usable || dprime_usable;
  const bool y = e.flat_available.has_value();
  const bool z = bias_usable_light || (bias_usable_flat && y);  // senza flat candidato bias_usable_flat è falso
  e.dfb = std::string("P-DFB") + (x ? '1' : '0') + (y ? '1' : '0') + (z ? '1' : '0');

  // insieme ammissibile (§b.5)
  auto light_ok = [&](light_chain lc) {
    switch (lc) {
      case light_chain::none: return true;
      case light_chain::bias: return bias_usable_light;
      case light_chain::dark: return d_usable;
      case light_chain::bias_dark_sub: return dprime_usable && bias_usable_light;
      case light_chain::dark_sub_partial: return dprime_usable;
    }
    return false;
  };
  auto flat_ok = [&](flat_chain fc) {
    switch (fc) {
      case flat_chain::none: return true;
      case flat_chain::dark_flat: return flat_raw_usable && df_inc_usable;
      case flat_chain::bias_dark_flat_sub: return flat_raw_usable && df_sub_usable && bias_usable_flat;
      case flat_chain::bias: return flat_raw_usable && bias_usable_flat && flat_bias_thermal_ok;
      case flat_chain::normalize: return e.flat_available == flat_state::corrected_unnormalized;
      case flat_chain::given_q: return e.flat_available == flat_state::calibrated;
    }
    return false;
  };
  for (light_chain lc : all_light) {
    if (!light_ok(lc)) continue;
    const offset_state off = offset_after(lc, c.raw_light);
    for (flat_chain fc : all_flat) {
      if (!flat_ok(fc)) continue;
      if (fc == flat_chain::none) e.admissible.emplace_back(lc, fc, false);
      else if (off == offset_state::removed) e.admissible.emplace_back(lc, fc, false);
      else if (off == offset_state::known_not_removed) e.admissible.emplace_back(lc, fc, true);
    }
  }
  std::sort(e.admissible.begin(), e.admissible.end());

  // default D-16 (§b.6): priorità più alta fra le ammissibili, mai il percorso parziale
  for (light_chain lc : all_light) {
    if (light_ok(lc) && light_priority(lc) > light_priority(e.default_light)) e.default_light = lc;
  }
  if (offset_after(e.default_light, c.raw_light) == offset_state::removed) {
    for (flat_chain fc : all_flat) {
      if (flat_ok(fc) && flat_priority(fc) > flat_priority(e.default_flat)) e.default_flat = fc;
    }
  }
  return e;
}

// ---------------------------------------------------------------- confronto prodotto / oracle

std::string pair_text(const pair_key& p) {
  std::ostringstream s;
  s << "(" << name_of(std::get<0>(p)) << ", " << name_of(std::get<1>(p)) << (std::get<2>(p) ? ", parziale" : "") << ")";
  return s.str();
}

std::string pairs_text(const std::vector<pair_key>& v) {
  std::string s;
  for (const auto& p : v) s += pair_text(p) + " ";
  return s;
}

std::vector<pair_key> admissible_of(const calibration_analysis& a) {
  std::vector<pair_key> v;
  for (const admissible_pair& p : a.admissible) v.emplace_back(p.light, p.flat, p.partial);
  std::sort(v.begin(), v.end());
  return v;
}

const char* role_text(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  return "?";
}

const candidate_analysis* find_candidate(const calibration_analysis& a, master_role role) {
  for (const candidate_analysis& c : a.candidates) {
    if (c.role == role) return &c;
  }
  return nullptr;
}

std::string verdicts_text(const std::vector<expected_verdict>& v) {
  std::string s;
  for (const auto& x : v) s += x.chain + (x.usable ? "=usable " : "=excluded(" + x.reason + ") ");
  return s;
}

std::vector<expected_verdict> verdicts_of(const candidate_analysis& c, bool light_only) {
  std::vector<expected_verdict> v;
  for (const chain_verdict& x : c.chains) {
    if (light_only && x.chain.rfind("light.", 0) != 0) continue;
    v.push_back({x.chain, x.status == candidate_status::usable, x.reason.value_or("")});
  }
  return v;
}

bool same(const std::vector<expected_verdict>& a, const std::vector<expected_verdict>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].chain != b[i].chain || a[i].usable != b[i].usable || a[i].reason != b[i].reason) return false;
  }
  return true;
}

// Differenze fra l'analisi del prodotto e l'oracle; vuota se coincidono.
std::string diff(const calibration_analysis& got, const expected_analysis& e) {
  std::ostringstream s;
  if (got.candidates.size() != e.candidates.size()) {
    s << "candidati: " << got.candidates.size() << " invece di " << e.candidates.size() << "; ";
  }
  for (const expected_candidate& ec : e.candidates) {
    const candidate_analysis* gc = find_candidate(got, ec.role);
    if (gc == nullptr) {
      s << role_text(ec.role) << " assente; ";
      continue;
    }
    for (const chain_verdict& v : gc->chains) {
      if ((v.status == candidate_status::usable) == v.reason.has_value()) {
        s << role_text(ec.role) << "." << v.chain << ": motivo presente se e solo se excluded violato; ";
      }
    }
    if ((gc->status == candidate_status::usable) != ec.usable) {
      s << role_text(ec.role) << ": stato " << (gc->status == candidate_status::usable ? "usable" : "excluded")
        << " invece di " << (ec.usable ? "usable" : "excluded") << "; ";
    }
    if (ec.chains_compared == expected_candidate::compare::any_with_reason) {
      bool ok = !gc->chains.empty();
      for (const chain_verdict& v : gc->chains) {
        ok = ok && v.status == candidate_status::excluded && v.reason.value_or("") == ec.state_reason;
      }
      if (!ok) {
        s << role_text(ec.role) << ": catene {" << verdicts_text(verdicts_of(*gc, false)) << "} non tutte excluded con "
          << ec.state_reason << "; ";
      }
    } else if (ec.chains_compared != expected_candidate::compare::none) {
      auto gv = verdicts_of(*gc, ec.chains_compared == expected_candidate::compare::light_only);
      std::erase_if(gv, [&](const expected_verdict& v) {
        return std::find(ec.skipped.begin(), ec.skipped.end(), v.chain) != ec.skipped.end();
      });
      if (!same(gv, ec.chains)) {
        s << role_text(ec.role) << ": catene {" << verdicts_text(gv) << "} invece di {" << verdicts_text(ec.chains) << "}; ";
      }
    }
  }
  if (e.dfb && got.predicates.dfb != *e.dfb) s << "P-DFB " << got.predicates.dfb << " invece di " << *e.dfb << "; ";
  if (got.predicates.df != e.df) s << "P-DF " << static_cast<int>(got.predicates.df) << " invece di " << static_cast<int>(e.df) << "; ";
  if (got.predicates.flat_available != e.flat_available) s << "stato del flat disponibile diverso; ";
  const auto ga = admissible_of(got);
  if (ga != e.admissible) s << "ammissibili {" << pairs_text(ga) << "} invece di {" << pairs_text(e.admissible) << "}; ";
  if (got.default_recipe.light != e.default_light || got.default_recipe.flat != e.default_flat) {
    s << "default " << pair_text({got.default_recipe.light, got.default_recipe.flat, false}) << " invece di "
      << pair_text({e.default_light, e.default_flat, false}) << "; ";
  }
  return s.str();
}

std::vector<config> all_configs() {
  std::vector<config> v;
  for (bool raw : {false, true}) {
    for (dark_kind d : {dark_kind::absent, dark_kind::d, dark_kind::d_prime, dark_kind::scaled, dark_kind::unknown,
                        dark_kind::incompatible}) {
      for (bias_kind b : {bias_kind::absent, bias_kind::valid, bias_kind::neither, bias_kind::only_light, bias_kind::only_flat}) {
        for (flat_kind f : {flat_kind::absent, flat_kind::raw, flat_kind::corrected_unnormalized, flat_kind::calibrated,
                            flat_kind::calibrated_no_normalization, flat_kind::unknown, flat_kind::incompatible}) {
          for (dark_flat_kind df : {dark_flat_kind::absent, dark_flat_kind::inc, dark_flat_kind::sub,
                                    dark_flat_kind::incompatible, dark_flat_kind::unknown}) {
            for (thermal_kind t : {thermal_kind::absent, thermal_kind::in_budget, thermal_kind::out_of_budget}) {
              v.push_back({raw, d, b, f, df, t});
            }
          }
        }
      }
    }
  }
  return v;
}

// ---------------------------------------------------------------- ricette per la selezione (§b.5, §b.9)

bool divides(flat_chain fc) { return fc != flat_chain::none; }

// Parametri applicabili alla coppia di catene, tutti e soli (§b.5: un parametro di una catena non selezionata è vietato).
// Ricodifica del test: esposizione con dark o dark-flat; temperatura con dark, bias o dark-flat; normalizzazione con
// le catene che calcolano N; q_min e q_rel_sigma_max con ogni divisione; dichiarazioni termiche con `flat.bias`;
// tolleranza del nero con un light RAW (§b.7).
chain_recipe recipe_for(light_chain lc, flat_chain fc, bool raw_light, const tuning& t = {}, double bound = 64.0) {
  chain_recipe r;
  r.light = lc;
  r.flat = fc;
  const bool uses_dark = lc == light_chain::dark || lc == light_chain::bias_dark_sub || lc == light_chain::dark_sub_partial;
  const bool uses_bias = lc == light_chain::bias || lc == light_chain::bias_dark_sub || fc == flat_chain::bias_dark_flat_sub ||
                         fc == flat_chain::bias;
  const bool uses_dark_flat = fc == flat_chain::dark_flat || fc == flat_chain::bias_dark_flat_sub;
  if (uses_dark || uses_dark_flat) r.exposure_rel_tol = 0.0;
  if (uses_dark || uses_bias || uses_dark_flat) {
    r.temperature_abs_tol_c = tol_temperature;
    r.temp_policy = temperature_policy::compare;
  }
  if (fc == flat_chain::dark_flat || fc == flat_chain::bias_dark_flat_sub || fc == flat_chain::bias || fc == flat_chain::normalize) {
    r.normalization = t.normalization;
  }
  if (divides(fc)) {
    r.q_min = t.q_min;
    r.q_rel_sigma_max = 10.0;
  }
  if (fc == flat_chain::bias) {
    r.flat_thermal_bound_adu = bound;
    r.flat_thermal_rel_budget = t.budget;
  }
  if (raw_light) r.decoder_black_abs_tol_adu = 0u;
  return r;
}

// Motivo presente nel contesto, di valore non fissato.
const std::string any_reason = "*";

// Stop atteso con categoria, codice e (se dato) il motivo nel contesto alla chiave `reason`.
::testing::AssertionResult stops(const calibration_analysis& a, const chain_recipe& r, error_category cat,
                                 const std::string& code, const std::string& reason = "") {
  try {
    require_admissible(a, r);
  } catch (const stop_error& e) {
    if (e.category() != cat || e.code() != code) {
      return ::testing::AssertionFailure() << "stop diverso: " << e.what() << " invece di " << code;
    }
    if (reason.empty()) return ::testing::AssertionSuccess();
    for (const auto& [k, v] : e.context()) {
      if (k == "reason" && (v == reason || (reason == any_reason && !v.empty()))) return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "motivo " << reason << " assente dal contesto: " << e.what();
  }
  return ::testing::AssertionFailure() << "nessuno stop, atteso " << code;
}

// Stop atteso con la sola categoria (codice non fissato dal contratto).
::testing::AssertionResult stops_with_category(const calibration_analysis& a, const chain_recipe& r, error_category cat) {
  try {
    require_admissible(a, r);
  } catch (const stop_error& e) {
    if (e.category() == cat) return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure() << "stop di categoria diversa: " << e.what();
  }
  return ::testing::AssertionFailure() << "nessuno stop";
}

::testing::AssertionResult passes(const calibration_analysis& a, const chain_recipe& r) {
  try {
    require_admissible(a, r);
  } catch (const stop_error& e) {
    return ::testing::AssertionFailure() << "stop inatteso: " << e.what();
  }
  return ::testing::AssertionSuccess();
}

// ---------------------------------------------------------------- T-M3A-CAL-01

struct hand_row {
  config c;
  const char* dfb;
  dark_flat_predicate df;
  std::optional<flat_state> flat_available;
  light_chain default_light;
  flat_chain default_flat;
  std::vector<pair_key> admissible;  // vuoto: non controllato nella riga
};

// Tabella scritta a mano dalle tabelle di §b.5 e §b.6. Righe §b.6: 111 (1-7), 110 (8-9b), 101 (10-11), 100 (12-12b),
// 011 (13-16), 010 (17-18), 001 (19), 000 (20); candidati esclusi (21-25). Catene di §b.5: light.none (20),
// light.bias (13), light.dark (1), light.bias_dark_sub (2), light.dark_sub_partial (9); flat.none (4), flat.dark_flat (1),
// flat.bias_dark_flat_sub (2), flat.bias (3), flat.normalize (6), flat.given_q (5).
std::vector<hand_row> hand_rows() {
  using L = light_chain;
  using F = flat_chain;
  using P = dark_flat_predicate;
  const auto fits = [](dark_kind d, bias_kind b, flat_kind f, dark_flat_kind df, thermal_kind t = thermal_kind::absent) {
    return config{false, d, b, f, df, t};
  };
  const auto raw = [](dark_kind d, bias_kind b, flat_kind f, dark_flat_kind df) { return config{true, d, b, f, df, thermal_kind::absent}; };
  using dk = dark_kind;
  using bk = bias_kind;
  using fk = flat_kind;
  using dfk = dark_flat_kind;
  return {
      // 1. 111, D, flat raw con INC: light.dark + flat.dark_flat (bias not_applicable nel matched dark)
      {fits(dk::d, bk::valid, fk::raw, dfk::inc), "P-DFB111", P::inc, flat_state::raw, L::dark, F::dark_flat,
       {{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias, F::dark_flat, false}, {L::dark, F::none, false},
        {L::dark, F::dark_flat, false}}},
      // 2. 111, D', flat raw con SUB
      {fits(dk::d_prime, bk::valid, fk::raw, dfk::sub), "P-DFB111", P::sub, flat_state::raw, L::bias_dark_sub, F::bias_dark_flat_sub,
       {{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias, F::bias_dark_flat_sub, false},
        {L::bias_dark_sub, F::none, false}, {L::bias_dark_sub, F::bias_dark_flat_sub, false}, {L::dark_sub_partial, F::none, false}}},
      // 3. 111, flat raw con ABS, dichiarazioni nel budget: flat.bias
      {fits(dk::d, bk::valid, fk::raw, dfk::absent, thermal_kind::in_budget), "P-DFB111", P::absent, flat_state::raw, L::dark, F::bias,
       {{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias, F::bias, false}, {L::dark, F::none, false},
        {L::dark, F::bias, false}}},
      // 4. 111, flat raw con ABS senza dichiarazioni: flat.none (flat_thermal_not_declared)
      {fits(dk::d, bk::valid, fk::raw, dfk::absent), "P-DFB111", P::absent, flat_state::raw, L::dark, F::none,
       {{L::none, F::none, false}, {L::bias, F::none, false}, {L::dark, F::none, false}}},
      // 4b. 111, flat raw con ABS fuori budget: flat.none (flat_thermal_budget)
      {fits(dk::d, bk::valid, fk::raw, dfk::absent, thermal_kind::out_of_budget), "P-DFB111", P::absent, flat_state::raw, L::dark,
       F::none, {{L::none, F::none, false}, {L::bias, F::none, false}, {L::dark, F::none, false}}},
      // 5. 111, flat calibrated: given_q
      {fits(dk::d, bk::valid, fk::calibrated, dfk::absent), "P-DFB111", P::absent, flat_state::calibrated, L::dark, F::given_q, {}},
      // 6. 111, D', flat corrected_unnormalized: normalize (il dark-flat INC non è applicabile)
      {fits(dk::d_prime, bk::valid, fk::corrected_unnormalized, dfk::inc), "P-DFB111", P::inc, flat_state::corrected_unnormalized,
       L::bias_dark_sub, F::normalize, {}},
      // 7. 111 con D' e bias usable solo col flat (esempio di §b.6): light.dark_sub_partial e flat.none
      {fits(dk::d_prime, bk::only_flat, fk::raw, dfk::inc), "P-DFB111", P::inc, flat_state::raw, L::dark_sub_partial, F::none,
       {{L::none, F::none, false}, {L::dark_sub_partial, F::none, false}}},
      // 8. 110, D, raw con SUB: flat.none (dark_flat_sub_requires_bias)
      {fits(dk::d, bk::absent, fk::raw, dfk::sub), "P-DFB110", P::sub, flat_state::raw, L::dark, F::none, {}},
      // 9. 110, D' su FITS: light.dark_sub_partial, flat.none (light_offset_not_removed)
      {fits(dk::d_prime, bk::absent, fk::raw, dfk::inc), "P-DFB110", P::inc, flat_state::raw, L::dark_sub_partial, F::none,
       {{L::none, F::none, false}, {L::dark_sub_partial, F::none, false}}},
      // 9b. 110, D' su RAW: divisione solo come alternativa esperta con flat_division_offset
      {raw(dk::d_prime, bk::absent, fk::raw, dfk::inc), "P-DFB110", P::inc, flat_state::raw, L::dark_sub_partial, F::none,
       {{L::none, F::none, false}, {L::none, F::dark_flat, true}, {L::dark_sub_partial, F::none, false},
        {L::dark_sub_partial, F::dark_flat, true}}},
      // 10. 101, D: come 111, flat.none (master_absent)
      {fits(dk::d, bk::valid, fk::absent, dfk::absent), "P-DFB101", P::absent, std::nullopt, L::dark, F::none,
       {{L::none, F::none, false}, {L::bias, F::none, false}, {L::dark, F::none, false}}},
      // 11. 101, D'
      {fits(dk::d_prime, bk::valid, fk::absent, dfk::absent), "P-DFB101", P::absent, std::nullopt, L::bias_dark_sub, F::none, {}},
      // 12. 100, D'
      {fits(dk::d_prime, bk::absent, fk::absent, dfk::absent), "P-DFB100", P::absent, std::nullopt, L::dark_sub_partial, F::none,
       {{L::none, F::none, false}, {L::dark_sub_partial, F::none, false}}},
      // 12b. 100, D
      {fits(dk::d, bk::absent, fk::absent, dfk::absent), "P-DFB100", P::absent, std::nullopt, L::dark, F::none, {}},
      // 13. 011, flat raw con INC
      {fits(dk::absent, bk::valid, fk::raw, dfk::inc), "P-DFB011", P::inc, flat_state::raw, L::bias, F::dark_flat,
       {{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias, F::dark_flat, false}}},
      // 14. 011 con bias usable solo col light, raw con SUB: flat.none
      {fits(dk::absent, bk::only_light, fk::raw, dfk::sub), "P-DFB011", P::sub, flat_state::raw, L::bias, F::none,
       {{L::none, F::none, false}, {L::bias, F::none, false}}},
      // 15. 011 con bias usable solo col light, flat calibrated: given_q
      {fits(dk::absent, bk::only_light, fk::calibrated, dfk::absent), "P-DFB011", P::absent, flat_state::calibrated, L::bias,
       F::given_q, {}},
      // 16. 011 con bias usable solo col flat (esempio di §b.6): light.none e flat.none
      {fits(dk::absent, bk::only_flat, fk::raw, dfk::inc), "P-DFB011", P::inc, flat_state::raw, L::none, F::none,
       {{L::none, F::none, false}}},
      // 17. 010 su FITS: nessuna divisione ammissibile (offset unknown)
      {fits(dk::absent, bk::absent, fk::raw, dfk::inc), "P-DFB010", P::inc, flat_state::raw, L::none, F::none,
       {{L::none, F::none, false}}},
      // 18. 010 su RAW: divisione ammissibile solo con known_not_removed (raw con INC)
      {raw(dk::absent, bk::absent, fk::raw, dfk::inc), "P-DFB010", P::inc, flat_state::raw, L::none, F::none,
       {{L::none, F::none, false}, {L::none, F::dark_flat, true}}},
      // 19. 001
      {fits(dk::absent, bk::valid, fk::absent, dfk::absent), "P-DFB001", P::absent, std::nullopt, L::bias, F::none,
       {{L::none, F::none, false}, {L::bias, F::none, false}}},
      // 20. 000: passaggio
      {fits(dk::absent, bk::absent, fk::absent, dfk::absent), "P-DFB000", P::absent, std::nullopt, L::none, F::none,
       {{L::none, F::none, false}}},
      // 21. dark scaled escluso: la riga è calcolata senza di lui (011)
      {fits(dk::scaled, bk::valid, fk::raw, dfk::inc), "P-DFB011", P::inc, flat_state::raw, L::bias, F::dark_flat, {}},
      // 22. flat unknown escluso (100)
      {fits(dk::d, bk::absent, fk::unknown, dfk::absent), "P-DFB100", P::absent, std::nullopt, L::dark, F::none, {{L::none, F::none, false}, {L::dark, F::none, false}}},
      // 23. dark-flat incompatibile col flat: P-DF-BAD
      {fits(dk::d, bk::valid, fk::raw, dfk::incompatible), "P-DFB111", P::bad, flat_state::raw, L::dark, F::none, {}},
      // 24. dark-flat incompatibile ma nessun flat candidato: P-DF dal solo stato (INC)
      {fits(dk::d, bk::absent, fk::absent, dfk::incompatible), "P-DFB100", P::inc, std::nullopt, L::dark, F::none, {}},
      // 25. bias escluso da entrambe le coppie (110), flat calibrated senza normalizzazione escluso
      {fits(dk::d_prime, bk::neither, fk::raw, dfk::sub), "P-DFB110", P::sub, flat_state::raw, L::dark_sub_partial, F::none, {}},
      {fits(dk::d, bk::absent, fk::calibrated_no_normalization, dfk::absent), "P-DFB100", P::absent, std::nullopt, L::dark, F::none, {}},
  };
}

TEST(CalibSelection, T_M3A_CAL_01_chain_names_are_those_of_the_contract) {
  for (light_chain c : all_light) EXPECT_STREQ(to_string(c), name_of(c));
  for (flat_chain c : all_flat) EXPECT_STREQ(to_string(c), name_of(c));
}

TEST(CalibSelection, T_M3A_CAL_01_hand_table_controls_the_recoded_oracle) {
  for (const hand_row& r : hand_rows()) {
    SCOPED_TRACE(describe(r.c));
    const expected_analysis e = oracle(r.c);
    ASSERT_TRUE(e.dfb.has_value());
    EXPECT_EQ(*e.dfb, r.dfb);
    EXPECT_EQ(e.df, r.df);
    EXPECT_EQ(e.flat_available, r.flat_available);
    EXPECT_EQ(e.default_light, r.default_light) << name_of(e.default_light);
    EXPECT_EQ(e.default_flat, r.default_flat) << name_of(e.default_flat);
    if (!r.admissible.empty()) {
      std::vector<pair_key> want = r.admissible;
      std::sort(want.begin(), want.end());
      EXPECT_EQ(e.admissible, want) << pairs_text(e.admissible) << " invece di " << pairs_text(want);
    }
  }
}

// Verdetti per catena scritti a mano per candidati esclusi per coppia o dal controllo termico (righe 4b, 7, 23, 25).
TEST(CalibSelection, T_M3A_CAL_01_hand_verdicts_control_the_recoded_oracle) {
  using dk = dark_kind;
  using bk = bias_kind;
  using fk = flat_kind;
  using dfk = dark_flat_kind;
  const std::string temp = "incompatible.temperature_c";
  struct row {
    config c;
    master_role role;
    std::vector<expected_verdict> chains;
  };
  const std::vector<row> rows = {
      {{false, dk::d, bk::valid, fk::raw, dfk::absent, thermal_kind::out_of_budget}, master_role::flat,
       {{"flat.dark_flat", true, ""}, {"flat.bias_dark_flat_sub", true, ""}, {"flat.bias", false, "flat_thermal_budget"}}},
      {{false, dk::d_prime, bk::only_flat, fk::raw, dfk::inc, thermal_kind::absent}, master_role::bias,
       {{"light.bias", false, temp}, {"light.bias_dark_sub", false, temp}, {"flat.bias_dark_flat_sub", true, ""}, {"flat.bias", true, ""}}},
      {{false, dk::d, bk::valid, fk::raw, dfk::incompatible, thermal_kind::absent}, master_role::dark_flat,
       {{"flat.dark_flat", false, "incompatible.exposure_s"}}},
      {{false, dk::d_prime, bk::neither, fk::raw, dfk::sub, thermal_kind::absent}, master_role::bias,
       {{"light.bias", false, temp}, {"light.bias_dark_sub", false, temp}, {"flat.bias_dark_flat_sub", false, temp},
        {"flat.bias", false, temp}}},
  };
  for (const row& r : rows) {
    SCOPED_TRACE(describe(r.c));
    const expected_analysis e = oracle(r.c);
    const auto it = std::find_if(e.candidates.begin(), e.candidates.end(), [&](const expected_candidate& x) { return x.role == r.role; });
    ASSERT_NE(it, e.candidates.end());
    EXPECT_EQ(it->chains_compared, expected_candidate::compare::all);
    EXPECT_TRUE(same(it->chains, r.chains)) << verdicts_text(it->chains) << " invece di " << verdicts_text(r.chains);
  }
}

TEST(CalibSelection, T_M3A_CAL_01_hand_table_rows_on_the_product) {
  for (const hand_row& r : hand_rows()) {
    SCOPED_TRACE(describe(r.c));
    const calibration_analysis a = analyze_calibration(input_of(r.c));
    EXPECT_EQ(a.predicates.dfb, r.dfb);
    EXPECT_EQ(a.predicates.df, r.df);
    EXPECT_EQ(a.predicates.flat_available, r.flat_available);
    EXPECT_EQ(a.default_recipe.light, r.default_light) << name_of(a.default_recipe.light);
    EXPECT_EQ(a.default_recipe.flat, r.default_flat) << name_of(a.default_recipe.flat);
    if (!r.admissible.empty()) {
      std::vector<pair_key> want = r.admissible;
      std::sort(want.begin(), want.end());
      EXPECT_EQ(admissible_of(a), want) << pairs_text(admissible_of(a)) << " invece di " << pairs_text(want);
    }
    EXPECT_EQ(diff(a, oracle(r.c)), "");
  }
}

// Bit z di P-DFB e presenza del flat candidato (§b.4): z = 1 se il bias è usable in almeno una catena i cui membri sono
// tutti ammessi; senza flat candidato le catene del flat non esistono, quindi un bias escluso dalle catene del light ha
// z = 0 ed è excluded. Un flat escluso per qualunque motivo di §b.2 o §b.3 (unknown, filtro incompatibile,
// normalizzazione non dichiarata) non forma catene e la riga DFB si calcola senza di lui (§b.4, §b.6): z = 0 anche qui,
// mentre il bias resta usable per il verdetto di coppia F–B (§b.1). Valori scritti a mano da §b.4, §b.5 e §b.6, insieme ammissibile completo per ogni riga. Già coperti in `hand_rows`: bias solo F con flat raw INC, con e
// senza D' (righe 7, 16); bias né L né F con D' e flat raw SUB (riga 25).
struct z_row {
  config c;
  const char* dfb;
  bool bias_usable;
  dark_flat_predicate df;
  std::optional<flat_state> flat_available;
  light_chain default_light;
  flat_chain default_flat;
  std::vector<pair_key> admissible;
};

std::vector<z_row> z_rows() {
  using L = light_chain;
  using F = flat_chain;
  using P = dark_flat_predicate;
  using dk = dark_kind;
  using bk = bias_kind;
  using fk = flat_kind;
  using dfk = dark_flat_kind;
  using fs = flat_state;
  const auto fits = [](dk d, bk b, fk f, dfk df, thermal_kind t = thermal_kind::absent) { return config{false, d, b, f, df, t}; };
  const auto raw = [](dk d, bk b, fk f, dfk df) { return config{true, d, b, f, df, thermal_kind::absent}; };
  const std::nullopt_t no_flat = std::nullopt;
  return {
      // bias usable solo col flat, nessun flat candidato: z = 0
      {fits(dk::absent, bk::only_flat, fk::absent, dfk::absent), "P-DFB000", false, P::absent, no_flat, L::none, F::none,
       {{L::none, F::none, false}}},
      {fits(dk::d, bk::only_flat, fk::absent, dfk::absent), "P-DFB100", false, P::absent, no_flat, L::dark, F::none,
       {{L::none, F::none, false}, {L::dark, F::none, false}}},
      {fits(dk::d_prime, bk::only_flat, fk::absent, dfk::absent), "P-DFB100", false, P::absent, no_flat, L::dark_sub_partial,
       F::none, {{L::none, F::none, false}, {L::dark_sub_partial, F::none, false}}},
      // bias usable solo col flat, flat raw usable: z = 1, catene del flat con bias
      {fits(dk::d, bk::only_flat, fk::raw, dfk::sub), "P-DFB111", true, P::sub, fs::raw, L::dark, F::bias_dark_flat_sub,
       {{L::none, F::none, false}, {L::dark, F::none, false}, {L::dark, F::bias_dark_flat_sub, false}}},
      {fits(dk::d, bk::only_flat, fk::raw, dfk::absent, thermal_kind::in_budget), "P-DFB111", true, P::absent, fs::raw, L::dark,
       F::bias, {{L::none, F::none, false}, {L::dark, F::none, false}, {L::dark, F::bias, false}}},
      // bias usable solo col flat, flat calibrated usable: z = 1
      {fits(dk::d, bk::only_flat, fk::calibrated, dfk::absent), "P-DFB111", true, P::absent, fs::calibrated, L::dark, F::given_q,
       {{L::none, F::none, false}, {L::dark, F::none, false}, {L::dark, F::given_q, false}}},
      {fits(dk::absent, bk::only_flat, fk::calibrated, dfk::absent), "P-DFB011", true, P::absent, fs::calibrated, L::none, F::none,
       {{L::none, F::none, false}}},
      // bias usable solo col flat, flat candidato ma escluso (unknown, filtro incompatibile, normalizzazione non
      // dichiarata): riga senza il flat, z = 0, bias usable per la coppia F–B
      {fits(dk::d, bk::only_flat, fk::unknown, dfk::absent), "P-DFB100", true, P::absent, no_flat, L::dark, F::none,
       {{L::none, F::none, false}, {L::dark, F::none, false}}},
      {fits(dk::absent, bk::only_flat, fk::unknown, dfk::absent), "P-DFB000", true, P::absent, no_flat, L::none, F::none,
       {{L::none, F::none, false}}},
      {fits(dk::d_prime, bk::only_flat, fk::incompatible, dfk::absent), "P-DFB100", true, P::absent, no_flat, L::dark_sub_partial,
       F::none, {{L::none, F::none, false}, {L::dark_sub_partial, F::none, false}}},
      {fits(dk::absent, bk::only_flat, fk::incompatible, dfk::absent), "P-DFB000", true, P::absent, no_flat, L::none, F::none,
       {{L::none, F::none, false}}},
      {fits(dk::d, bk::only_flat, fk::calibrated_no_normalization, dfk::absent), "P-DFB100", true, P::absent, no_flat, L::dark,
       F::none, {{L::none, F::none, false}, {L::dark, F::none, false}}},
      // bias escluso da L e da F: z = 0 con e senza flat candidato
      {fits(dk::absent, bk::neither, fk::absent, dfk::absent), "P-DFB000", false, P::absent, no_flat, L::none, F::none,
       {{L::none, F::none, false}}},
      {fits(dk::d, bk::neither, fk::absent, dfk::absent), "P-DFB100", false, P::absent, no_flat, L::dark, F::none,
       {{L::none, F::none, false}, {L::dark, F::none, false}}},
      {fits(dk::d_prime, bk::neither, fk::absent, dfk::absent), "P-DFB100", false, P::absent, no_flat, L::dark_sub_partial, F::none,
       {{L::none, F::none, false}, {L::dark_sub_partial, F::none, false}}},
      {fits(dk::absent, bk::neither, fk::raw, dfk::sub), "P-DFB010", false, P::sub, fs::raw, L::none, F::none,
       {{L::none, F::none, false}}},
      {fits(dk::d, bk::neither, fk::raw, dfk::sub), "P-DFB110", false, P::sub, fs::raw, L::dark, F::none,
       {{L::none, F::none, false}, {L::dark, F::none, false}}},
      {fits(dk::d, bk::neither, fk::calibrated, dfk::absent), "P-DFB110", false, P::absent, fs::calibrated, L::dark, F::given_q,
       {{L::none, F::none, false}, {L::dark, F::none, false}, {L::dark, F::given_q, false}}},
      {fits(dk::absent, bk::neither, fk::calibrated, dfk::absent), "P-DFB010", false, P::absent, fs::calibrated, L::none, F::none,
       {{L::none, F::none, false}}},
      {fits(dk::d, bk::neither, fk::unknown, dfk::absent), "P-DFB100", false, P::absent, no_flat, L::dark, F::none,
       {{L::none, F::none, false}, {L::dark, F::none, false}}},
      // controllo: bias usable solo col light, nessun flat candidato: z = 1
      {fits(dk::absent, bk::only_light, fk::absent, dfk::absent), "P-DFB001", true, P::absent, no_flat, L::bias, F::none,
       {{L::none, F::none, false}, {L::bias, F::none, false}}},
      {fits(dk::d_prime, bk::only_light, fk::absent, dfk::absent), "P-DFB101", true, P::absent, no_flat, L::bias_dark_sub, F::none,
       {{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias_dark_sub, F::none, false},
        {L::dark_sub_partial, F::none, false}}},
      // light RAW: z apre o chiude la divisione parziale con known_not_removed
      {raw(dk::absent, bk::only_flat, fk::raw, dfk::sub), "P-DFB011", true, P::sub, fs::raw, L::none, F::none,
       {{L::none, F::none, false}, {L::none, F::bias_dark_flat_sub, true}}},
      {raw(dk::absent, bk::neither, fk::raw, dfk::sub), "P-DFB010", false, P::sub, fs::raw, L::none, F::none,
       {{L::none, F::none, false}}},
      {raw(dk::d_prime, bk::only_flat, fk::absent, dfk::absent), "P-DFB100", false, P::absent, no_flat, L::dark_sub_partial,
       F::none, {{L::none, F::none, false}, {L::dark_sub_partial, F::none, false}}},
      {raw(dk::d_prime, bk::only_flat, fk::raw, dfk::sub), "P-DFB111", true, P::sub, fs::raw, L::dark_sub_partial, F::none,
       {{L::none, F::none, false}, {L::none, F::bias_dark_flat_sub, true}, {L::dark_sub_partial, F::none, false},
        {L::dark_sub_partial, F::bias_dark_flat_sub, true}}},
  };
}

TEST(CalibSelection, T_M3A_CAL_01_z_bit_table_controls_the_recoded_oracle) {
  const std::vector<z_row> rows = z_rows();
  ASSERT_EQ(rows.size(), 26u);
  for (const z_row& r : rows) {
    SCOPED_TRACE(describe(r.c));
    const expected_analysis e = oracle(r.c);
    ASSERT_TRUE(e.dfb.has_value());
    EXPECT_EQ(*e.dfb, r.dfb);
    const auto it = std::find_if(e.candidates.begin(), e.candidates.end(),
                                 [](const expected_candidate& x) { return x.role == master_role::bias; });
    ASSERT_NE(it, e.candidates.end());
    EXPECT_EQ(it->usable, r.bias_usable);
    EXPECT_EQ(e.df, r.df);
    EXPECT_EQ(e.flat_available, r.flat_available);
    std::vector<pair_key> want = r.admissible;
    std::sort(want.begin(), want.end());
    EXPECT_EQ(e.admissible, want) << pairs_text(e.admissible) << " invece di " << pairs_text(want);
    EXPECT_EQ(e.default_light, r.default_light) << name_of(e.default_light);
    EXPECT_EQ(e.default_flat, r.default_flat) << name_of(e.default_flat);
  }
}

// Sul prodotto: predicato, stato del bias, insieme ammissibile e default della tabella; con il bias escluso dalle
// catene del light, `light.bias` si ferma alla selezione con il motivo di coppia (§b.9).
TEST(CalibSelection, T_M3A_CAL_01_z_bit_table_on_the_product) {
  for (const z_row& r : z_rows()) {
    SCOPED_TRACE(describe(r.c));
    const calibration_analysis a = analyze_calibration(input_of(r.c));
    EXPECT_EQ(a.predicates.dfb, r.dfb);
    const candidate_analysis* b = find_candidate(a, master_role::bias);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->status, r.bias_usable ? candidate_status::usable : candidate_status::excluded);
    EXPECT_EQ(a.predicates.df, r.df);
    EXPECT_EQ(a.predicates.flat_available, r.flat_available);
    std::vector<pair_key> want = r.admissible;
    std::sort(want.begin(), want.end());
    EXPECT_EQ(admissible_of(a), want) << pairs_text(admissible_of(a)) << " invece di " << pairs_text(want);
    EXPECT_EQ(a.default_recipe.light, r.default_light) << name_of(a.default_recipe.light);
    EXPECT_EQ(a.default_recipe.flat, r.default_flat) << name_of(a.default_recipe.flat);
    EXPECT_TRUE(passes(a, a.default_recipe));
    if (r.c.bias == bias_kind::only_flat || r.c.bias == bias_kind::neither) {
      EXPECT_TRUE(stops(a, recipe_for(light_chain::bias, flat_chain::none, r.c.raw_light), error_category::invalid_input,
                        "calib.recipe_not_admissible", "incompatible.temperature_c"));
    }
    EXPECT_EQ(diff(a, oracle(r.c)), "");
  }
}

TEST(CalibSelection, T_M3A_CAL_01_all_configurations_match_the_oracle) {
  const std::vector<config> configs = all_configs();
  ASSERT_EQ(configs.size(), 2u * 6 * 5 * 7 * 5 * 3);
  std::size_t mismatches = 0;
  for (const config& c : configs) {
    const std::string d = diff(analyze_calibration(input_of(c)), oracle(c));
    if (d.empty()) continue;
    if (++mismatches <= 20) ADD_FAILURE() << describe(c) << ": " << d;
  }
  EXPECT_EQ(mismatches, 0u) << "configurazioni diverse dall'oracle su " << configs.size();
}

// Ogni ruolo in ogni fase (§b.2: `raw` per bias, D, DF e flat raw; `raw` o `calibrated` per D', DF' e flat preparati;
// `prestacked` sempre escluso) e un master con `pedestal_adu` ≠ 0 (§b.3: `pedestal`). Un candidato escluso conta come
// quello escluso per stato nella riga DFB, nell'insieme ammissibile e nel default.
struct domain_case {
  const char* what;
  config base;
  master_role role;
  std::optional<process_phase> phase;  // nullopt: piedistallo
  const char* reason;                  // vuoto: usable
  config as_excluded;                  // configurazione equivalente per l'oracle, se escluso
};

TEST(CalibSelection, T_M3A_CAL_01_phase_and_pedestal_per_role) {
  using dk = dark_kind;
  using bk = bias_kind;
  using fk = flat_kind;
  using dfk = dark_flat_kind;
  const auto fits = [](dk d, bk b, fk f, dfk df) { return config{false, d, b, f, df, thermal_kind::absent}; };
  const config d_inc = fits(dk::d, bk::valid, fk::raw, dfk::inc);
  const config dp_sub = fits(dk::d_prime, bk::valid, fk::raw, dfk::sub);
  const config dp_corr = fits(dk::d_prime, bk::valid, fk::corrected_unnormalized, dfk::sub);
  const config dp_cal = fits(dk::d_prime, bk::valid, fk::calibrated, dfk::sub);
  const auto with_dark = [](config c, dk d) { c.dark = d; return c; };
  const auto with_bias = [](config c, bk b) { c.bias = b; return c; };
  const auto with_flat = [](config c, fk f) { c.flat = f; return c; };
  const auto with_df = [](config c, dfk f) { c.dark_flat = f; return c; };
  using ph = process_phase;
  const std::vector<domain_case> cases = {
      {"bias raw", d_inc, master_role::bias, ph::raw, "", d_inc},
      {"bias calibrated", d_inc, master_role::bias, ph::calibrated, "phase", with_bias(d_inc, bk::neither)},
      {"bias prestacked", d_inc, master_role::bias, ph::prestacked, "phase", with_bias(d_inc, bk::neither)},
      {"D raw", d_inc, master_role::dark, ph::raw, "", d_inc},
      {"D calibrated", d_inc, master_role::dark, ph::calibrated, "phase", with_dark(d_inc, dk::unknown)},
      {"D prestacked", d_inc, master_role::dark, ph::prestacked, "phase", with_dark(d_inc, dk::unknown)},
      {"D' raw", dp_sub, master_role::dark, ph::raw, "", dp_sub},
      {"D' calibrated", dp_sub, master_role::dark, ph::calibrated, "", dp_sub},
      {"D' prestacked", dp_sub, master_role::dark, ph::prestacked, "phase", with_dark(dp_sub, dk::unknown)},
      {"DF raw", d_inc, master_role::dark_flat, ph::raw, "", d_inc},
      {"DF calibrated", d_inc, master_role::dark_flat, ph::calibrated, "phase", with_df(d_inc, dfk::unknown)},
      {"DF prestacked", d_inc, master_role::dark_flat, ph::prestacked, "phase", with_df(d_inc, dfk::unknown)},
      {"DF' raw", dp_sub, master_role::dark_flat, ph::raw, "", dp_sub},
      {"DF' calibrated", dp_sub, master_role::dark_flat, ph::calibrated, "", dp_sub},
      {"DF' prestacked", dp_sub, master_role::dark_flat, ph::prestacked, "phase", with_df(dp_sub, dfk::unknown)},
      {"flat raw raw", d_inc, master_role::flat, ph::raw, "", d_inc},
      {"flat raw calibrated", d_inc, master_role::flat, ph::calibrated, "phase", with_flat(d_inc, fk::unknown)},
      {"flat raw prestacked", d_inc, master_role::flat, ph::prestacked, "phase", with_flat(d_inc, fk::unknown)},
      {"flat corrected raw", dp_corr, master_role::flat, ph::raw, "", dp_corr},
      {"flat corrected calibrated", dp_corr, master_role::flat, ph::calibrated, "", dp_corr},
      {"flat corrected prestacked", dp_corr, master_role::flat, ph::prestacked, "phase", with_flat(dp_corr, fk::unknown)},
      {"flat calibrated raw", dp_cal, master_role::flat, ph::raw, "", dp_cal},
      {"flat calibrated calibrated", dp_cal, master_role::flat, ph::calibrated, "", dp_cal},
      {"flat calibrated prestacked", dp_cal, master_role::flat, ph::prestacked, "phase", with_flat(dp_cal, fk::unknown)},
      {"bias pedestal", d_inc, master_role::bias, std::nullopt, "pedestal", with_bias(d_inc, bk::neither)},
      {"D pedestal", d_inc, master_role::dark, std::nullopt, "pedestal", with_dark(d_inc, dk::unknown)},
      {"DF pedestal", d_inc, master_role::dark_flat, std::nullopt, "pedestal", with_df(d_inc, dfk::unknown)},
      {"flat pedestal", d_inc, master_role::flat, std::nullopt, "pedestal", with_flat(d_inc, fk::unknown)},
  };
  for (const domain_case& k : cases) {
    SCOPED_TRACE(k.what);
    calibration_analysis_input in = input_of(k.base);
    std::optional<analysis_candidate>* slot = k.role == master_role::bias   ? &in.bias
                                              : k.role == master_role::dark ? &in.dark
                                              : k.role == master_role::flat ? &in.flat
                                                                            : &in.dark_flat;
    ASSERT_TRUE(slot->has_value());
    image& img = (*slot)->img;
    if (k.phase) img.phase = metadata_value<process_phase>{*k.phase, {metadata_origin::declared, {}}};
    else img.metadata.pedestal_adu = metadata_value<double>{5.0, {metadata_origin::declared, {}}};
    const calibration_analysis a = analyze_calibration(in);
    const candidate_analysis* c = find_candidate(a, k.role);
    ASSERT_NE(c, nullptr);
    const std::string reason = k.reason;
    if (reason.empty()) {
      EXPECT_EQ(c->status, candidate_status::usable);
      EXPECT_EQ(diff(a, oracle(k.base)), "");
      continue;
    }
    EXPECT_EQ(c->status, candidate_status::excluded);
    ASSERT_FALSE(c->chains.empty());
    for (const chain_verdict& v : c->chains) {
      EXPECT_EQ(v.status, candidate_status::excluded) << v.chain;
      EXPECT_EQ(v.reason.value_or(""), reason) << v.chain;
    }
    const expected_analysis e = oracle(k.as_excluded);
    EXPECT_EQ(a.predicates.dfb, *e.dfb);
    EXPECT_EQ(a.predicates.df, e.df);
    EXPECT_EQ(a.predicates.flat_available, e.flat_available);
    EXPECT_EQ(admissible_of(a), e.admissible) << pairs_text(admissible_of(a)) << " invece di " << pairs_text(e.admissible);
    EXPECT_EQ(a.default_recipe.light, e.default_light);
    EXPECT_EQ(a.default_recipe.flat, e.default_flat);
  }
}

// P-FLAT-RAW e P-FLAT-Q sono predicati della ricetta (§b.4): veri per le catene su flat raw o per flat.given_q, falsi con
// flat.none anche col flat usable. P-OFFSET: catena del flat diversa da flat.none.
TEST(CalibSelection, T_M3A_CAL_01_p_flat_predicates_on_default_and_on_flat_none) {
  for (const hand_row& r : hand_rows()) {
    SCOPED_TRACE(describe(r.c));
    const calibration_analysis a = analyze_calibration(input_of(r.c));
    const flat_chain fc = a.default_recipe.flat;
    const bool raw_chain = fc == flat_chain::dark_flat || fc == flat_chain::bias_dark_flat_sub || fc == flat_chain::bias;
    EXPECT_EQ(p_flat_raw(a.default_recipe), raw_chain);
    EXPECT_EQ(p_flat_q(a.default_recipe), fc == flat_chain::given_q);
    EXPECT_EQ(p_offset(a.default_recipe), fc != flat_chain::none);
    if (r.flat_available) {
      const chain_recipe none = recipe_for(a.default_recipe.light, flat_chain::none, r.c.raw_light);
      EXPECT_FALSE(p_flat_raw(none));
      EXPECT_FALSE(p_flat_q(none));
      EXPECT_FALSE(p_offset(none));
    }
  }
}

// ---------------------------------------------------------------- T-M3A-CAL-02

TEST(CalibSelection, T_M3A_CAL_02_default_properties_on_every_configuration) {
  std::size_t failures = 0;
  for (const config& c : all_configs()) {
    const calibration_analysis a = analyze_calibration(input_of(c));
    const chain_recipe& d = a.default_recipe;
    std::ostringstream s;
    const auto adm = admissible_of(a);
    if (!std::binary_search(adm.begin(), adm.end(), pair_key{d.light, d.flat, false})) s << "default fuori dall'insieme; ";
    if (d.flat_division_offset.has_value()) s << "default con flat_division_offset; ";
    if (d.flat != flat_chain::none && offset_after(d.light, c.raw_light) != offset_state::removed) s << "divisione su offset non removed; ";
    if (d.flat == flat_chain::bias && c.thermal != thermal_kind::in_budget) s << "flat.bias senza dichiarazioni nel budget; ";
    try {
      require_admissible(a, d);
    } catch (const stop_error& e) {
      s << "il default non supera la selezione: " << e.what() << "; ";
    }
    if (s.str().empty()) continue;
    if (++failures <= 20) ADD_FAILURE() << describe(c) << ": " << s.str();
  }
  EXPECT_EQ(failures, 0u);
}

// Configurazione del controllo termico: light FITS, bias valido, flat raw, nessun dark né dark-flat.
struct thermal_case {
  const char* what;
  double flat;
  double bias;
  double q_min;
  double bound;
  double budget;
  bool admitted;
  std::optional<std::vector<double>> flat_pixels = std::nullopt;  // al posto del flat costante
  normalization_mode normalization = normalization_mode::global_median;
  std::vector<std::size_t> masked_flat = {};  // pixel del flat con il bit `source_flagged`
  double flat_saturation = 0;                 // 0: saturazione del flat non dichiarata
};

void check_thermal_case(const thermal_case& k) {
  SCOPED_TRACE(k.what);
  tuning t;
  t.flat_raw = k.flat_pixels ? *k.flat_pixels : constant(k.flat);
  t.normalization = k.normalization;
  t.bias = constant(k.bias);
  t.q_min = k.q_min;
  t.bound_in = k.bound;
  t.budget = k.budget;
  const config c{false, dark_kind::absent, bias_kind::valid, flat_kind::raw, dark_flat_kind::absent, thermal_kind::in_budget};
  calibration_analysis_input in = input_of(c, t);
  for (std::size_t i : k.masked_flat) in.flat->img.invalid.bits.at(i) = invalid_reason::source_flagged;
  if (k.flat_saturation != 0) {
    in.flat->img.metadata.saturation_adu = metadata_value<double>{k.flat_saturation, {metadata_origin::header, "SATURATE"}};
  }
  const calibration_analysis a = analyze_calibration(in);
  const auto adm = admissible_of(a);
  EXPECT_EQ(std::binary_search(adm.begin(), adm.end(), pair_key{light_chain::bias, flat_chain::bias, false}), k.admitted)
      << pairs_text(adm);
  const candidate_analysis* f = find_candidate(a, master_role::flat);
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->status, candidate_status::usable);
  bool found = false;
  for (const chain_verdict& v : f->chains) {
    if (v.chain != "flat.bias") continue;
    found = true;
    EXPECT_EQ(v.status == candidate_status::usable, k.admitted);
    if (!k.admitted) {
      EXPECT_EQ(v.reason.value_or(""), "flat_thermal_budget");
    }
  }
  EXPECT_TRUE(found);
  EXPECT_EQ(a.default_recipe.light, light_chain::bias);
  EXPECT_EQ(a.default_recipe.flat, k.admitted ? flat_chain::bias : flat_chain::none);
  const chain_recipe r = recipe_for(light_chain::bias, flat_chain::bias, false, t, k.bound);
  if (k.admitted) {
    EXPECT_TRUE(passes(a, r));
  } else {
    EXPECT_TRUE(stops(a, r, error_category::invalid_input, "calib.recipe_not_admissible", "flat_thermal_budget"));
  }
}

rat q(double x) { return exact::from_double(x); }
rat unit_roundoff() { return rat{exact::from_u64(1), exact::shl(exact::from_u64(1), 53)}; }

// β_c rappresentabile: Fc = 1024 = N, q_min = 0,5, bound 64, β = 1/8. Budget = β ammesso (uguaglianza, non «>»);
// budget = predecessore f64 di β escluso.
TEST(CalibSelection, T_M3A_CAL_02_thermal_boundary_with_representable_beta) {
  ASSERT_EQ(exact::cmp(q(64) / (q(0.5) * q(1024)), q(0.125)), 0);
  check_thermal_case({"budget = beta", 1124, 100, 0.5, 64, 0.125, true});
  check_thermal_case({"budget = pred(beta)", 1124, 100, 0.5, 64, std::nextafter(0.125, 0.0), false});
}

// β_c = 1/3 non rappresentabile: Fc = 3, q_min = 1, bound 1. fl(1/3) è sotto 1/3, quindi budget · q_min · N < bound in
// aritmetica esatta (escluso), mentre il prodotto f64 arrotondato vale 1 (un confronto in f64 ammetterebbe). Controllo:
// il successore f64 di fl(1/3) è sopra 1/3 (ammesso).
TEST(CalibSelection, T_M3A_CAL_02_thermal_boundary_with_unrepresentable_beta) {
  const double third = 1.0 / 3.0;
  const rat exact_third{exact::from_u64(1), exact::from_u64(3)};
  ASSERT_TRUE(q(third) < exact_third);
  ASSERT_EQ(third * 1.0 * 3.0, 1.0);
  const double above = std::nextafter(third, 1.0);
  ASSERT_TRUE(exact_third < q(above));
  check_thermal_case({"budget = fl(1/3) per difetto", 103, 100, 1.0, 1.0, third, false});
  check_thermal_case({"budget = succ(fl(1/3))", 103, 100, 1.0, 1.0, above, true});
}

// β_c ≤ budget ma β⁺_c = β_c (1 + u)² ≥ 1: Fc = 1, q_min = 1, bound = budget = 1 − u. Escluso. Controllo con β = 1/2.
TEST(CalibSelection, T_M3A_CAL_02_thermal_beta_plus_at_least_one_is_excluded) {
  const double one_minus_u = std::nextafter(1.0, 0.0);
  const rat u = unit_roundoff();
  ASSERT_EQ(exact::cmp(q(one_minus_u), exact::from_u(1) - u), 0);
  const rat one_plus_u = exact::from_u(1) + u;
  ASSERT_FALSE(q(one_minus_u) * one_plus_u * one_plus_u < exact::from_u(1));
  check_thermal_case({"beta+ >= 1", 101, 100, 1.0, one_minus_u, one_minus_u, false});
  check_thermal_case({"beta = 1/2", 101, 100, 1.0, 0.5, 0.5, true});
}

// Controllo per canale di normalizzazione (§b.5 «per ogni canale c»): RGGB 4×2, Fc = 64 sui siti R e 1024 sui siti G e B
// (i due G uguali: l'esito non dipende dal raggruppare G1 e G2), q_min = 0,5, budget 0,25. Soglia budget · q_min · N_c:
// 8 su R, 128 su G e B; con `global_median` N = 1024 (mediana degli 8 pixel) e soglia 128. Bound 16: escluso per canale
// (solo R supera), ammesso con la mediana globale; bound 8: ammesso per canale (uguaglianza su R).
TEST(CalibSelection, T_M3A_CAL_02_thermal_check_per_normalization_channel) {
  // Riga 0: R G R G; riga 1: G B G B (origine RGGB, offset 0).
  const std::vector<double> fc = {64, 1024, 64, 1024, 1024, 1024, 1024, 1024};
  std::vector<double> flat;
  for (double v : fc) flat.push_back(v + 100);
  const auto per_channel = normalization_mode::cfa_channel_median;
  check_thermal_case({"cfa_channel_median, bound 16: R fuori budget", 0, 100, 0.5, 16, 0.25, false, flat, per_channel});
  check_thermal_case({"cfa_channel_median, bound 8: tutti i canali nel budget", 0, 100, 0.5, 8, 0.25, true, flat, per_channel});
  check_thermal_case({"global_median, bound 16: nel budget", 0, 100, 0.5, 16, 0.25, true, flat, normalization_mode::global_median});
}

// F − B ≤ 0 con le dichiarazioni termiche presenti (§b.5, §b.1 punto 1). `bound > budget · q_min · N_c` è vera con
// N_c < 0 (secondo membro negativo, bound ≥ 0) e con N_c = 0 (bound > 0 o bound = 0): `flat.bias` `excluded`
// (`flat_thermal_budget`), il flat resta `usable`, l'analisi non si ferma e il default è `light.bias` + `flat.none`.
// Controllo: Fc = 1024, soglia 0,25 · 0,5 · 1024 = 128 ≥ 64, ammesso.
TEST(CalibSelection, T_M3A_CAL_02_thermal_nonpositive_flat_minus_bias_excludes_flat_bias) {
  check_thermal_case({"Fc = -10 (flat 90, bias 100)", 90, 100, 0.5, 64, 0.25, false});
  check_thermal_case({"Fc = 0 (flat 100, bias 100), bound 64", 100, 100, 0.5, 64, 0.25, false});
  check_thermal_case({"Fc = 0 (flat 100, bias 100), bound 0", 100, 100, 0.5, 0, 0.25, false});
  check_thermal_case({"controllo: Fc = 1024", 1124, 100, 0.5, 64, 0.25, true});
}

// Lo stesso flat (Fc = −10) con un dark `D`: le ricette senza `flat.bias` non cambiano. §b.6 riga 111 con `D` e flat
// `raw` senza dark-flat: `light.dark`, e `flat.bias` solo se nel budget, altrimenti `flat.none` (`flat_thermal_budget`).
TEST(CalibSelection, T_M3A_CAL_02_thermal_nonpositive_leaves_other_recipes_admissible) {
  tuning t;
  t.flat_raw = constant(90);
  const config c{false, dark_kind::d, bias_kind::valid, flat_kind::raw, dark_flat_kind::absent, thermal_kind::in_budget};
  const calibration_analysis a = analyze_calibration(input_of(c, t));
  EXPECT_EQ(a.predicates.dfb, "P-DFB111");
  EXPECT_EQ(a.default_recipe.light, light_chain::dark);
  EXPECT_EQ(a.default_recipe.flat, flat_chain::none);
  EXPECT_TRUE(passes(a, recipe_for(light_chain::dark, flat_chain::none, false, t)));
  EXPECT_TRUE(passes(a, recipe_for(light_chain::bias, flat_chain::none, false, t)));
  EXPECT_TRUE(stops(a, recipe_for(light_chain::dark, flat_chain::bias, false, t), error_category::invalid_input,
                    "calib.recipe_not_admissible", "flat_thermal_budget"));
}

// Canale CFA senza pixel validi per N (§b.5 «per ogni canale di normalizzazione c»; pixel validi per N di ADR-16.a §6:
// senza bit di input, `saturated` compreso). Con `cfa_channel_median` N_R non esiste e β_R non è definito: §b.5 non nomina
// il caso, §b.1 punto 1 vieta lo stop nell'analisi; atteso `flat.bias` `excluded` (`flat_thermal_budget`). RGGB 4×2
// (origine 0): siti R agli indici 0 e 2, mascherati (`source_flagged`) o saturi (F = 1124 ≥ 1000). G, G2, B: Fc = 500,
// soglia 0,25 · 0,5 · 500 = 62,5 ≥ bound 8. Controlli: `global_median` sugli stessi pixel (N = 500) e nessun pixel
// escluso con `cfa_channel_median` (N_R = 1024, soglia 128).
TEST(CalibSelection, T_M3A_CAL_02_thermal_cfa_channel_without_valid_pixels_excludes_flat_bias) {
  const std::vector<double> flat = {1124, 600, 1124, 600, 600, 600, 600, 600};
  const auto per_channel = normalization_mode::cfa_channel_median;
  const auto global = normalization_mode::global_median;
  check_thermal_case({"R mascherato, cfa_channel_median", 0, 100, 0.5, 8, 0.25, false, flat, per_channel, {0, 2}});
  check_thermal_case({"R saturo, cfa_channel_median", 0, 100, 0.5, 8, 0.25, false, flat, per_channel, {}, 1000});
  check_thermal_case({"controllo: R mascherato, global_median", 0, 100, 0.5, 8, 0.25, true, flat, global, {0, 2}});
  check_thermal_case({"controllo: R saturo, global_median", 0, 100, 0.5, 8, 0.25, true, flat, global, {}, 1000});
  check_thermal_case({"controllo: nessun pixel escluso, cfa_channel_median", 0, 100, 0.5, 8, 0.25, true, flat, per_channel});
}

// D-16 e §b.6: il default non inventa una dichiarazione. Light FITS, bias valido, nessun dark. Flat
// `corrected_unnormalized` (`usable`): `flat.normalize` usa `normalization`, `q_min` e `q_rel_sigma_max` (§b.5, ADR-16.a
// §2); senza una delle tre il default è `light.bias` + `flat.none`, mentre l'insieme ammissibile (§b.5: ingressi `usable`
// e regole delle catene, non le dichiarazioni) contiene ancora `light.bias` + `flat.normalize`. Flat `calibrated`:
// `flat.given_q` non calcola N, quindi senza `normalization` il default resta `flat.given_q`; senza `q_min` è `flat.none`.
TEST(CalibSelection, T_M3A_CAL_02_default_does_not_invent_a_declaration) {
  enum class drop { none, q_min, q_rel_sigma_max, normalization };
  struct row {
    const char* what;
    flat_kind flat;
    drop dropped;
    flat_chain chain;     // catena del flat che resta nell'insieme ammissibile
    flat_chain expected;  // flat del default
  };
  const row rows[] = {
      {"corrected_unnormalized, tutte le dichiarazioni", flat_kind::corrected_unnormalized, drop::none, flat_chain::normalize, flat_chain::normalize},
      {"corrected_unnormalized senza q_min", flat_kind::corrected_unnormalized, drop::q_min, flat_chain::normalize, flat_chain::none},
      {"corrected_unnormalized senza q_rel_sigma_max", flat_kind::corrected_unnormalized, drop::q_rel_sigma_max, flat_chain::normalize, flat_chain::none},
      {"corrected_unnormalized senza normalization", flat_kind::corrected_unnormalized, drop::normalization, flat_chain::normalize, flat_chain::none},
      {"calibrated senza normalization", flat_kind::calibrated, drop::normalization, flat_chain::given_q, flat_chain::given_q},
      {"calibrated senza q_min", flat_kind::calibrated, drop::q_min, flat_chain::given_q, flat_chain::none},
  };
  for (const row& r : rows) {
    SCOPED_TRACE(r.what);
    calibration_analysis_input in =
        input_of({false, dark_kind::absent, bias_kind::valid, r.flat, dark_flat_kind::absent, thermal_kind::absent});
    calibration_declarations& d = in.declarations;
    switch (r.dropped) {
      case drop::none: break;
      case drop::q_min: d.q_min.reset(); break;
      case drop::q_rel_sigma_max: d.q_rel_sigma_max.reset(); break;
      case drop::normalization: d.normalization.reset(); break;
    }
    const calibration_analysis a = analyze_calibration(in);
    const candidate_analysis* f = find_candidate(a, master_role::flat);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->status, candidate_status::usable);
    EXPECT_EQ(a.predicates.dfb, "P-DFB011");
    const auto adm = admissible_of(a);
    EXPECT_TRUE(std::binary_search(adm.begin(), adm.end(), pair_key{light_chain::bias, r.chain, false})) << pairs_text(adm);
    EXPECT_EQ(a.default_recipe.light, light_chain::bias);
    EXPECT_EQ(a.default_recipe.flat, r.expected) << name_of(a.default_recipe.flat);
    EXPECT_TRUE(passes(a, a.default_recipe));
  }
}

// Perturbazione relativa di Q con termico iniettato t ∈ [0, bound], contro l'oracle razionale:
// ≤ max(q_min · β⁺, β⁺ / (1 − β⁺)) su ogni pixel valido (§b.5), con β⁺ vicino a 1 da sotto.
rat median_of(const std::vector<rat>& v) { return exact::median(v); }

rat max_relative_perturbation(const std::vector<rat>& fc, const std::vector<rat>& thermal) {
  std::vector<rat> k(fc.size());
  for (std::size_t i = 0; i < fc.size(); ++i) k[i] = fc[i] - thermal[i];
  const rat n_obs = median_of(fc);
  const rat n_true = median_of(k);
  rat worst = exact::from_u(0);
  for (std::size_t i = 0; i < fc.size(); ++i) {
    const rat q_obs = fc[i] / n_obs;
    const rat q_true = k[i] / n_true;
    const rat rel = exact::abs(q_obs - q_true) / q_true;
    if (worst < rel) worst = rel;
  }
  return worst;
}

rat limit_of(const rat& q_min, const rat& beta_plus) {
  const rat a = q_min * beta_plus;
  const rat b = beta_plus / (exact::from_u(1) - beta_plus);
  return a < b ? b : a;
}

TEST(CalibSelection, T_M3A_CAL_02_contract_example_reaches_the_bound) {
  // Fc = [3, 3, 3], q_min = 1, bound 1, termico [1, 0, 0]: perturbazione 1/2 = β/(1 − β) con β = 1/3 (§b.5).
  const std::vector<rat> fc = {exact::from_u(3), exact::from_u(3), exact::from_u(3)};
  const std::vector<rat> t = {exact::from_u(1), exact::from_u(0), exact::from_u(0)};
  const rat half{exact::from_u64(1), exact::from_u64(2)};
  EXPECT_EQ(exact::cmp(max_relative_perturbation(fc, t), half), 0);
  const rat beta{exact::from_u64(1), exact::from_u64(3)};
  const rat one_plus_u = exact::from_u(1) + unit_roundoff();
  EXPECT_TRUE(half <= limit_of(exact::from_u(1), beta * one_plus_u * one_plus_u));
}

TEST(CalibSelection, T_M3A_CAL_02_injected_thermal_perturbation_within_bound) {
  const std::vector<double> fc_values = {8, 8, 8, 8, 9, 9, 10, 12};  // N = mediana = 8,5
  const double bias = 100, q_min = 0.5, bound = 4.0, budget = 0.96875;
  std::vector<rat> fc;
  for (double v : fc_values) fc.push_back(q(v));
  const rat n = median_of(fc);
  ASSERT_EQ(exact::cmp(n, rat{exact::from_u64(17), exact::from_u64(2)}), 0);
  for (const rat& v : fc) ASSERT_TRUE(q(q_min) <= v / n);  // pixel validi: Q ≥ q_min
  const rat beta = q(bound) / (q(q_min) * n);
  const rat one_plus_u = exact::from_u(1) + unit_roundoff();
  const rat beta_plus = beta * one_plus_u * one_plus_u;
  ASSERT_TRUE(beta_plus < exact::from_u(1));
  const rat nine_tenths{exact::from_u64(9), exact::from_u64(10)};
  ASSERT_TRUE(nine_tenths < beta_plus);  // vicino a 1 da sotto (16/17)
  ASSERT_FALSE(q(budget) * q(q_min) * n < q(bound));                       // nel budget

  // Il prodotto ammette flat.bias con queste dichiarazioni.
  tuning t;
  t.flat_raw.clear();
  for (double v : fc_values) t.flat_raw.push_back(v + bias);
  t.bias = constant(bias);
  t.q_min = q_min;
  t.bound_in = bound;
  t.budget = budget;
  const config c{false, dark_kind::absent, bias_kind::valid, flat_kind::raw, dark_flat_kind::absent, thermal_kind::in_budget};
  const calibration_analysis a = analyze_calibration(input_of(c, t));
  EXPECT_EQ(a.default_recipe.flat, flat_chain::bias);

  const rat limit = limit_of(q(q_min), beta_plus);
  std::vector<std::vector<rat>> patterns;
  patterns.push_back(std::vector<rat>(fc.size(), exact::from_u(0)));
  patterns.push_back(std::vector<rat>(fc.size(), q(bound)));
  patterns.push_back(std::vector<rat>(fc.size(), q(bound / 2)));
  for (std::size_t i = 0; i < fc.size(); ++i) {
    std::vector<rat> one(fc.size(), exact::from_u(0));
    one[i] = q(bound);
    patterns.push_back(one);
    std::vector<rat> all_but_one(fc.size(), q(bound));
    all_but_one[i] = exact::from_u(0);
    patterns.push_back(all_but_one);
  }
  std::vector<rat> alternating(fc.size()), ramp(fc.size());
  for (std::size_t i = 0; i < fc.size(); ++i) {
    alternating[i] = i % 2 == 0 ? q(bound) : exact::from_u(0);
    ramp[i] = q(bound) * rat{exact::from_u64(i), exact::from_u64(fc.size() - 1)};
  }
  patterns.push_back(alternating);
  patterns.push_back(ramp);
  for (std::size_t p = 0; p < patterns.size(); ++p) {
    const rat got = max_relative_perturbation(fc, patterns[p]);
    EXPECT_TRUE(got <= limit) << "schema " << p << ": " << exact::to_ld(got) << " oltre " << exact::to_ld(limit);
  }
}

// ---------------------------------------------------------------- T-M3A-CAL-06

TEST(CalibSelection, T_M3A_CAL_06_unknown_or_scaled_master_selected) {
  using dk = dark_kind;
  using bk = bias_kind;
  using fk = flat_kind;
  using dfk = dark_flat_kind;
  {
    SCOPED_TRACE("flat unknown in flat.dark_flat (P-FLAT-UNKNOWN)");
    const calibration_analysis a = analyze_calibration(input_of({false, dk::d, bk::valid, fk::unknown, dfk::inc, thermal_kind::absent}));
    EXPECT_TRUE(stops(a, recipe_for(light_chain::dark, flat_chain::dark_flat, false), error_category::metadata_missing,
                      "calib.master_state_unknown"));
    EXPECT_TRUE(stops(a, recipe_for(light_chain::dark, flat_chain::given_q, false), error_category::metadata_missing,
                      "calib.master_state_unknown"));
  }
  {
    SCOPED_TRACE("dark unknown in light.dark");
    const calibration_analysis a = analyze_calibration(input_of({false, dk::unknown, bk::valid, fk::absent, dfk::absent, thermal_kind::absent}));
    EXPECT_TRUE(stops(a, recipe_for(light_chain::dark, flat_chain::none, false), error_category::metadata_missing,
                      "calib.master_state_unknown"));
  }
  {
    SCOPED_TRACE("dark-flat unknown in flat.dark_flat");
    const calibration_analysis a = analyze_calibration(input_of({false, dk::d, bk::valid, fk::raw, dfk::unknown, thermal_kind::absent}));
    EXPECT_TRUE(stops(a, recipe_for(light_chain::dark, flat_chain::dark_flat, false), error_category::metadata_missing,
                      "calib.master_state_unknown"));
  }
  {
    SCOPED_TRACE("dark scaled in light.dark");
    const calibration_analysis a = analyze_calibration(input_of({false, dk::scaled, bk::valid, fk::absent, dfk::absent, thermal_kind::absent}));
    EXPECT_TRUE(stops(a, recipe_for(light_chain::dark, flat_chain::none, false), error_category::unsupported,
                      "calib.master_state_scaled"));
  }
  {
    SCOPED_TRACE("dark-flat scaled in flat.dark_flat");
    const calibration_analysis a = analyze_calibration(input_of({false, dk::d, bk::valid, fk::raw, dfk::scaled, thermal_kind::absent}));
    EXPECT_TRUE(stops(a, recipe_for(light_chain::dark, flat_chain::dark_flat, false), error_category::unsupported,
                      "calib.master_state_scaled"));
  }
}

TEST(CalibSelection, T_M3A_CAL_06_recipe_not_admissible) {
  using dk = dark_kind;
  using bk = bias_kind;
  using fk = flat_kind;
  using dfk = dark_flat_kind;
  const auto analysis = [](config c) { return analyze_calibration(input_of(c)); };
  const auto not_admissible = [](const calibration_analysis& a, const chain_recipe& r, const std::string& reason) {
    return stops(a, r, error_category::invalid_input, "calib.recipe_not_admissible", reason);
  };
  // L − B − D: light.bias_dark_sub con un dark bias_included (doppia sottrazione dell'offset).
  EXPECT_TRUE(not_admissible(analysis({false, dk::d, bk::valid, fk::absent, dfk::absent, thermal_kind::absent}),
                             recipe_for(light_chain::bias_dark_sub, flat_chain::none, false), "double_offset"));
  // F − B − DF: flat.bias_dark_flat_sub con un dark-flat bias_included.
  EXPECT_TRUE(not_admissible(analysis({false, dk::absent, bk::valid, fk::raw, dfk::inc, thermal_kind::absent}),
                             recipe_for(light_chain::bias, flat_chain::bias_dark_flat_sub, false), "double_offset"));
  // Divisione con offset unknown: light FITS, light.none + flat.dark_flat.
  EXPECT_TRUE(not_admissible(analysis({false, dk::absent, bk::absent, fk::raw, dfk::inc, thermal_kind::absent}),
                             recipe_for(light_chain::none, flat_chain::dark_flat, false), "light_offset_not_removed"));
  // flat.bias senza dichiarazioni termiche (né nell'analisi né nella ricetta).
  {
    chain_recipe r = recipe_for(light_chain::bias, flat_chain::bias, false);
    r.flat_thermal_bound_adu.reset();
    r.flat_thermal_rel_budget.reset();
    EXPECT_TRUE(not_admissible(analysis({false, dk::absent, bk::valid, fk::raw, dfk::absent, thermal_kind::absent}), r,
                               "flat_thermal_not_declared"));
  }
  // flat.bias fuori budget.
  EXPECT_TRUE(not_admissible(analysis({false, dk::absent, bk::valid, fk::raw, dfk::absent, thermal_kind::out_of_budget}),
                             recipe_for(light_chain::bias, flat_chain::bias, false, {}, 256.0), "flat_thermal_budget"));
  // Master escluso per coppia: dark incompatibile in light.dark; bias usable solo col flat in light.bias.
  EXPECT_TRUE(not_admissible(analysis({false, dk::incompatible, bk::absent, fk::absent, dfk::absent, thermal_kind::absent}),
                             recipe_for(light_chain::dark, flat_chain::none, false), "incompatible.exposure_s"));
  EXPECT_TRUE(not_admissible(analysis({false, dk::absent, bk::only_flat, fk::raw, dfk::absent, thermal_kind::absent}),
                             recipe_for(light_chain::bias, flat_chain::none, false), "incompatible.temperature_c"));
  // Flat escluso per coppia (filtro) in flat.dark_flat.
  EXPECT_TRUE(not_admissible(analysis({false, dk::d, bk::absent, fk::incompatible, dfk::inc, thermal_kind::absent}),
                             recipe_for(light_chain::dark, flat_chain::dark_flat, false), "incompatible.filter"));
  // Master assente.
  EXPECT_TRUE(not_admissible(analysis({false, dk::absent, bk::absent, fk::absent, dfk::absent, thermal_kind::absent}),
                             recipe_for(light_chain::dark, flat_chain::none, false), "master_absent"));
  EXPECT_TRUE(not_admissible(analysis({false, dk::d, bk::absent, fk::absent, dfk::absent, thermal_kind::absent}),
                             recipe_for(light_chain::dark, flat_chain::given_q, false), "master_absent"));
  // Catena incoerente con lo stato del master: light.dark con D', flat.given_q con flat corrected_unnormalized,
  // flat.dark_flat con flat calibrated. Motivo nel contesto (§b.9); il terzo è `flat_already_corrected` di §b.10.
  EXPECT_TRUE(not_admissible(analysis({false, dk::d_prime, bk::absent, fk::absent, dfk::absent, thermal_kind::absent}),
                             recipe_for(light_chain::dark, flat_chain::none, false), any_reason));
  EXPECT_TRUE(not_admissible(analysis({false, dk::d, bk::absent, fk::corrected_unnormalized, dfk::absent, thermal_kind::absent}),
                             recipe_for(light_chain::dark, flat_chain::given_q, false), any_reason));
  EXPECT_TRUE(not_admissible(analysis({false, dk::d, bk::absent, fk::calibrated, dfk::inc, thermal_kind::absent}),
                             recipe_for(light_chain::dark, flat_chain::dark_flat, false), "flat_already_corrected"));
}

// Parametri di una catena non selezionata presenti: invalid_input, calib.param_not_applicable.<nome> (§b.5).
TEST(CalibSelection, T_M3A_CAL_06_parameters_not_applicable) {
  const calibration_analysis a = analyze_calibration(
      input_of({false, dark_kind::d, bias_kind::valid, flat_kind::raw, dark_flat_kind::inc, thermal_kind::in_budget}));
  ASSERT_TRUE(passes(a, recipe_for(light_chain::dark, flat_chain::none, false)));
  ASSERT_TRUE(passes(a, recipe_for(light_chain::dark, flat_chain::dark_flat, false)));
  ASSERT_TRUE(passes(a, recipe_for(light_chain::bias, flat_chain::none, false)));
  const auto not_applicable = [&](const chain_recipe& r, const char* name) {
    return stops(a, r, error_category::invalid_input, std::string("calib.param_not_applicable.") + name);
  };
  chain_recipe r = recipe_for(light_chain::dark, flat_chain::none, false);
  r.normalization = normalization_mode::global_median;
  EXPECT_TRUE(not_applicable(r, "normalization"));
  r = recipe_for(light_chain::dark, flat_chain::none, false);
  r.q_min = 0.5;
  EXPECT_TRUE(not_applicable(r, "q_min"));
  r = recipe_for(light_chain::dark, flat_chain::none, false);
  r.q_rel_sigma_max = 10.0;
  EXPECT_TRUE(not_applicable(r, "q_rel_sigma_max"));
  r = recipe_for(light_chain::bias, flat_chain::none, false);
  r.exposure_rel_tol = 0.0;
  EXPECT_TRUE(not_applicable(r, "exposure_rel_tol"));
  r = recipe_for(light_chain::dark, flat_chain::dark_flat, false);
  r.flat_thermal_bound_adu = 64.0;
  EXPECT_TRUE(not_applicable(r, "flat_thermal_bound_adu"));
  r = recipe_for(light_chain::dark, flat_chain::dark_flat, false);
  r.flat_thermal_rel_budget = 0.25;
  EXPECT_TRUE(not_applicable(r, "flat_thermal_rel_budget"));
  r = recipe_for(light_chain::dark, flat_chain::none, false);
  r.flat_division_offset = offset_state::known_not_removed;
  EXPECT_TRUE(not_applicable(r, "flat_division_offset"));
  // Temperatura senza master confrontati per temperatura (light.none + flat.none).
  r = recipe_for(light_chain::none, flat_chain::none, false);
  ASSERT_TRUE(passes(a, r));
  r.temperature_abs_tol_c = tol_temperature;
  EXPECT_TRUE(not_applicable(r, "temperature_abs_tol_c"));
  // Tolleranza del nero del decoder con soli ingressi FITS (§b.7).
  r = recipe_for(light_chain::dark, flat_chain::none, false);
  r.decoder_black_abs_tol_adu = 0u;
  EXPECT_TRUE(not_applicable(r, "decoder_black_abs_tol_adu"));
  // `normalization` con flat.given_q (Q = F: nessuna N calcolata dalla ricetta, scelta 7 del test).
  const calibration_analysis g = analyze_calibration(
      input_of({false, dark_kind::d, bias_kind::absent, flat_kind::calibrated, dark_flat_kind::absent, thermal_kind::absent}));
  r = recipe_for(light_chain::dark, flat_chain::given_q, false);
  ASSERT_TRUE(passes(g, r));
  r.normalization = normalization_mode::global_median;
  EXPECT_TRUE(stops(g, r, error_category::invalid_input, "calib.param_not_applicable.normalization"));
}

// Stop atteso `invalid_input`, `calib.param.<nome>`, con il nome alla chiave `parametro` del contesto.
::testing::AssertionResult stops_on_param(const calibration_analysis& a, const chain_recipe& r, const std::string& name) {
  try {
    require_admissible(a, r);
  } catch (const stop_error& e) {
    if (e.category() != error_category::invalid_input || e.code() != "calib.param." + name) {
      return ::testing::AssertionFailure() << "stop diverso: " << e.what() << " invece di calib.param." << name;
    }
    for (const auto& [k, v] : e.context()) {
      if (k == "parametro" && v == name) return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "parametro " << name << " assente dal contesto: " << e.what();
  }
  return ::testing::AssertionFailure() << "nessuno stop, atteso calib.param." << name;
}

// Selezione (§b.1 punti 1-2): i verdetti dell'analisi valgono per le dichiarazioni con cui sono stati calcolati, e la
// ricetta le porta nel nodo. Un parametro applicabile diverso dalla dichiarazione dell'analisi ferma prima del calcolo:
// `invalid_input`, `calib.param.<nome>` (codice dei parametri di ADR-16.a §2; il caso «diverso» non ha un codice proprio
// nel contratto). Ogni ricetta passa con i valori dichiarati.
TEST(CalibSelection, T_M3A_CAL_06_parameter_differs_from_the_analysis_declaration) {
  const calibration_analysis a = analyze_calibration(
      input_of({false, dark_kind::d, bias_kind::valid, flat_kind::raw, dark_flat_kind::inc, thermal_kind::in_budget}));
  chain_recipe r = recipe_for(light_chain::dark, flat_chain::none, false);
  ASSERT_TRUE(passes(a, r));
  r.exposure_rel_tol = 0.5;
  EXPECT_TRUE(stops_on_param(a, r, "exposure_rel_tol"));
  r = recipe_for(light_chain::bias, flat_chain::none, false);
  ASSERT_TRUE(passes(a, r));
  r.temperature_abs_tol_c = 2.0;
  EXPECT_TRUE(stops_on_param(a, r, "temperature_abs_tol_c"));
  r = recipe_for(light_chain::bias, flat_chain::none, false);
  r.temp_policy = temperature_policy::not_applicable_declared;
  EXPECT_TRUE(stops_on_param(a, r, "temperature_policy"));
  r = recipe_for(light_chain::dark, flat_chain::dark_flat, false);
  ASSERT_TRUE(passes(a, r));
  r.normalization = normalization_mode::cfa_channel_median;
  EXPECT_TRUE(stops_on_param(a, r, "normalization"));
  r = recipe_for(light_chain::dark, flat_chain::dark_flat, false);
  r.q_min = 0.25;
  EXPECT_TRUE(stops_on_param(a, r, "q_min"));
  r = recipe_for(light_chain::dark, flat_chain::dark_flat, false);
  r.q_rel_sigma_max = 5.0;
  EXPECT_TRUE(stops_on_param(a, r, "q_rel_sigma_max"));
  r = recipe_for(light_chain::bias, flat_chain::bias, false);
  ASSERT_TRUE(passes(a, r));
  r.flat_thermal_bound_adu = 32.0;
  EXPECT_TRUE(stops_on_param(a, r, "flat_thermal_bound_adu"));
  r = recipe_for(light_chain::bias, flat_chain::bias, false);
  r.flat_thermal_rel_budget = 0.5;
  EXPECT_TRUE(stops_on_param(a, r, "flat_thermal_rel_budget"));
}

// Valori fuori dominio nella ricetta (ADR-16.a §2: `q_min` > 0, `q_rel_sigma_max` > 0, finiti; §b.5:
// `flat_thermal_bound_adu` ≥ 0, `flat_thermal_rel_budget` in (0, 1)): `invalid_input`, `calib.param.<nome>` (T-M2E-27).
// Con le dichiarazioni dell'analisi presenti lo stop vale anche come «diverso»; con `q_min` e `q_rel_sigma_max` assenti
// dall'analisi (insieme ammissibile senza dichiarazioni, §b.5) ferma solo il dominio: controllo, la stessa ricetta con
// valori nel dominio passa (scelta esperta, §b.1 punto 2).
TEST(CalibSelection, T_M3A_CAL_06_parameter_out_of_domain) {
  const calibration_analysis a = analyze_calibration(
      input_of({false, dark_kind::d, bias_kind::valid, flat_kind::raw, dark_flat_kind::inc, thermal_kind::in_budget}));
  chain_recipe r = recipe_for(light_chain::dark, flat_chain::dark_flat, false);
  r.q_min = 0.0;
  EXPECT_TRUE(stops_on_param(a, r, "q_min"));
  r = recipe_for(light_chain::bias, flat_chain::bias, false);
  r.flat_thermal_rel_budget = 1.0;
  EXPECT_TRUE(stops_on_param(a, r, "flat_thermal_rel_budget"));
  r = recipe_for(light_chain::bias, flat_chain::bias, false);
  r.flat_thermal_bound_adu = -1.0;
  EXPECT_TRUE(stops_on_param(a, r, "flat_thermal_bound_adu"));

  calibration_analysis_input in = input_of(
      {false, dark_kind::absent, bias_kind::valid, flat_kind::corrected_unnormalized, dark_flat_kind::absent, thermal_kind::absent});
  in.declarations.q_min.reset();
  in.declarations.q_rel_sigma_max.reset();
  const calibration_analysis u = analyze_calibration(in);
  r = recipe_for(light_chain::bias, flat_chain::normalize, false);
  ASSERT_TRUE(passes(u, r));
  for (double bad : {0.0, -0.5, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
    SCOPED_TRACE(bad);
    r = recipe_for(light_chain::bias, flat_chain::normalize, false);
    r.q_min = bad;
    EXPECT_TRUE(stops_on_param(u, r, "q_min"));
    r = recipe_for(light_chain::bias, flat_chain::normalize, false);
    r.q_rel_sigma_max = bad;
    EXPECT_TRUE(stops_on_param(u, r, "q_rel_sigma_max"));
  }
}

// ---------------------------------------------------------------- T-M3A-C01-02

// Ogni catena del flat × stato dell'offset del light × `flat_division_offset` presente o assente (C-01.b §9, ADR-16.b §b.5).
struct flat_setup {
  flat_chain chain;
  flat_kind flat;
  dark_flat_kind dark_flat;
  thermal_kind thermal;
};

struct offset_setup {
  light_chain light;
  bool raw_light;
  offset_state state;
};

TEST(CalibSelection, T_M3A_C01_02_p_offset_admissibility_and_default) {
  const flat_setup flats[] = {
      {flat_chain::none, flat_kind::raw, dark_flat_kind::inc, thermal_kind::absent},
      {flat_chain::dark_flat, flat_kind::raw, dark_flat_kind::inc, thermal_kind::absent},
      {flat_chain::bias_dark_flat_sub, flat_kind::raw, dark_flat_kind::sub, thermal_kind::absent},
      {flat_chain::bias, flat_kind::raw, dark_flat_kind::absent, thermal_kind::in_budget},
      {flat_chain::normalize, flat_kind::corrected_unnormalized, dark_flat_kind::absent, thermal_kind::absent},
      {flat_chain::given_q, flat_kind::calibrated, dark_flat_kind::absent, thermal_kind::absent},
  };
  const offset_setup offsets[] = {
      {light_chain::bias, false, offset_state::removed},
      {light_chain::bias_dark_sub, false, offset_state::removed},
      {light_chain::bias, true, offset_state::removed},
      {light_chain::none, true, offset_state::known_not_removed},
      {light_chain::dark_sub_partial, true, offset_state::known_not_removed},
      {light_chain::none, false, offset_state::unknown},
      {light_chain::dark_sub_partial, false, offset_state::unknown},
  };
  for (const flat_setup& f : flats) {
    for (const offset_setup& o : offsets) {
      const config c{o.raw_light, dark_kind::d_prime, bias_kind::valid, f.flat, f.dark_flat, f.thermal};
      const calibration_analysis a = analyze_calibration(input_of(c));
      ASSERT_EQ(offset_after(o.light, o.raw_light), o.state);
      const auto adm = admissible_of(a);
      for (bool declared : {false, true}) {
        SCOPED_TRACE(std::string(name_of(o.light)) + " + " + name_of(f.chain) + (o.raw_light ? " RAW" : " FITS") +
                     (declared ? " con flat_division_offset" : " senza flat_division_offset"));
        chain_recipe r = recipe_for(o.light, f.chain, o.raw_light);
        if (declared) r.flat_division_offset = offset_state::known_not_removed;
        const std::string not_applicable = "calib.param_not_applicable.flat_division_offset";
        const bool plain = f.chain == flat_chain::none || o.state == offset_state::removed;
        if (plain) {
          if (declared) {
            EXPECT_TRUE(stops(a, r, error_category::invalid_input, not_applicable));
          } else {
            EXPECT_TRUE(passes(a, r));
          }
          continue;
        }
        switch (o.state) {
          case offset_state::removed: break;
          case offset_state::known_not_removed:
            if (declared) {
              EXPECT_TRUE(passes(a, r));
              // Un valore diverso da known_not_removed non è la dichiarazione del percorso parziale: stop.
              for (offset_state wrong : {offset_state::removed, offset_state::unknown}) {
                chain_recipe w = r;
                w.flat_division_offset = wrong;
                EXPECT_TRUE(stops_with_category(a, w, error_category::invalid_input))
                    << "flat_division_offset = " << static_cast<int>(wrong);
              }
            } else {
              EXPECT_TRUE(stops(a, r, error_category::invalid_input, "calib.recipe_not_admissible"));
            }
            break;
          case offset_state::unknown:
            EXPECT_TRUE(stops(a, r, error_category::invalid_input, "calib.recipe_not_admissible"));
            break;
        }
      }
      SCOPED_TRACE(std::string(name_of(o.light)) + " + " + name_of(f.chain) + (o.raw_light ? " RAW" : " FITS"));
      // Insieme ammissibile: la coppia c'è (parziale solo con known_not_removed) se e solo se la regola la ammette.
      const bool has_plain = std::binary_search(adm.begin(), adm.end(), pair_key{o.light, f.chain, false});
      const bool has_partial = std::binary_search(adm.begin(), adm.end(), pair_key{o.light, f.chain, true});
      const bool want_plain = f.chain == flat_chain::none || o.state == offset_state::removed;
      const bool want_partial = f.chain != flat_chain::none && o.state == offset_state::known_not_removed;
      EXPECT_EQ(has_plain, want_plain);
      EXPECT_EQ(has_partial, want_partial);
      // Default: nessuna divisione su offset non removed, mai flat_division_offset.
      const chain_recipe& d = a.default_recipe;
      EXPECT_FALSE(d.flat_division_offset.has_value());
      if (d.flat != flat_chain::none) {
        EXPECT_EQ(offset_after(d.light, o.raw_light), offset_state::removed) << name_of(d.light);
      }
      EXPECT_EQ(diff(a, oracle(c)), "");
    }
  }
}

// ---------------------------------------------------------------- dichiarazioni lette dall'analisi

// Stop dell'analisi atteso con categoria e codice; con `param` non vuoto, il nome alla chiave `parametro` del contesto.
::testing::AssertionResult analysis_stops(const calibration_analysis_input& in, error_category cat, const std::string& code,
                                          const std::string& param = "") {
  try {
    (void)analyze_calibration(in);
  } catch (const stop_error& e) {
    if (e.category() != cat || e.code() != code) {
      return ::testing::AssertionFailure() << "stop diverso: " << e.what() << " invece di " << code;
    }
    if (param.empty()) return ::testing::AssertionSuccess();
    for (const auto& [k, v] : e.context()) {
      if (k == "parametro" && v == param) return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << "parametro " << param << " assente dal contesto: " << e.what();
  }
  return ::testing::AssertionFailure() << "nessuno stop, atteso " << code;
}

::testing::AssertionResult analysis_completes(const calibration_analysis_input& in) {
  try {
    (void)analyze_calibration(in);
  } catch (const stop_error& e) {
    return ::testing::AssertionFailure() << "stop inatteso: " << e.what();
  }
  return ::testing::AssertionSuccess();
}

// Light FITS, bias valido, flat `raw`, nessun dark né dark-flat, dichiarazioni termiche presenti: l'analisi esegue il
// controllo termico di `flat.bias` (§b.5), che calcola `N_c` secondo `normalization` e confronta con `q_min`.
const config thermal_config{false, dark_kind::absent, bias_kind::valid, flat_kind::raw, dark_flat_kind::absent,
                            thermal_kind::in_budget};

// Controllo termico con le dichiarazioni termiche presenti (§b.5; ADR-16.a §2: `normalization` e `q_min` obbligatori, senza
// default; assenti: `invalid_input`, `calib.param.<nome>`). `cfa_channel_median` su un flat mono: `invalid_input`,
// `calib.normalization_requires_cfa` (ADR-16.a §6, T-M2E-07). Light, bias e flat mono (L–F e F–B confrontano il CFA per
// uguaglianza esatta, quindi tutti e tre senza BAYERPAT). Controlli: tutte le dichiarazioni presenti; flat mono con
// `global_median` (N = 1024, soglia 0,25 · 0,5 · 1024 = 128 ≥ bound 64: `flat.bias` ammesso).
TEST(CalibSelection, T_M3A_CAL_02_thermal_check_requires_its_declarations) {
  {
    SCOPED_TRACE("normalization assente");
    calibration_analysis_input in = input_of(thermal_config);
    in.declarations.normalization.reset();
    EXPECT_TRUE(analysis_stops(in, error_category::invalid_input, "calib.param.normalization", "normalization"));
  }
  {
    SCOPED_TRACE("q_min assente");
    calibration_analysis_input in = input_of(thermal_config);
    in.declarations.q_min.reset();
    EXPECT_TRUE(analysis_stops(in, error_category::invalid_input, "calib.param.q_min", "q_min"));
  }
  {
    SCOPED_TRACE("controllo: tutte le dichiarazioni presenti");
    EXPECT_TRUE(analysis_completes(input_of(thermal_config)));
  }
  tuning t;
  t.mono = true;
  t.normalization = normalization_mode::cfa_channel_median;
  {
    SCOPED_TRACE("cfa_channel_median su flat mono");
    const calibration_analysis_input in = input_of(thermal_config, t);
    ASSERT_EQ(in.flat->img.layout, plane_layout::mono);
    ASSERT_FALSE(in.flat->img.cfa.has_value());
    EXPECT_TRUE(analysis_stops(in, error_category::invalid_input, "calib.normalization_requires_cfa"));
  }
  t.normalization = normalization_mode::global_median;
  {
    SCOPED_TRACE("controllo: flat mono con global_median");
    const calibration_analysis_input in = input_of(thermal_config, t);
    ASSERT_TRUE(analysis_completes(in));
    const calibration_analysis a = analyze_calibration(in);
    const auto adm = admissible_of(a);
    EXPECT_TRUE(std::binary_search(adm.begin(), adm.end(), pair_key{light_chain::bias, flat_chain::bias, false}))
        << pairs_text(adm);
    EXPECT_EQ(a.default_recipe.flat, flat_chain::bias);
  }
}

// Dominio delle dichiarazioni lette dall'analisi (ADR-16.a §2: `q_min` > 0, `q_rel_sigma_max` > 0, valore non finito
// escluso; §b.5: `flat_thermal_bound_adu` ≥ 0, `flat_thermal_rel_budget` in (0, 1), e la ricetta le dichiara entrambe):
// fuori dominio `invalid_input`, `calib.param.<nome>`. `exposure_rel_tol`, `temperature_policy` e `temperature_abs_tol_c`
// sono coperti da T-M3A-C01-06 (test di U6). Controlli sui bordi del dominio: l'analisi si completa.
TEST(CalibSelection, T_M3A_CAL_06_analysis_declarations_out_of_domain) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const double tiny = std::numeric_limits<double>::denorm_min();
  enum class field { q_min, q_rel_sigma_max, bound, budget };
  const auto set = [](calibration_analysis_input& in, field f, std::optional<double> v) {
    calibration_declarations& d = in.declarations;
    switch (f) {
      case field::q_min: d.q_min = v; break;
      case field::q_rel_sigma_max: d.q_rel_sigma_max = v; break;
      case field::bound: d.flat_thermal_bound_adu = v; break;
      case field::budget: d.flat_thermal_rel_budget = v; break;
    }
  };
  const struct {
    field f;
    const char* name;
    std::vector<double> bad;
    std::vector<double> good;
  } rows[] = {
      {field::q_min, "q_min", {0.0, -0.0, -0.5, nan, inf, -inf}, {tiny, 0.5}},
      {field::q_rel_sigma_max, "q_rel_sigma_max", {0.0, -0.0, -10.0, nan, inf, -inf}, {tiny, 10.0}},
      {field::bound, "flat_thermal_bound_adu", {-1.0, -tiny, nan, inf, -inf}, {0.0, 64.0}},
      {field::budget, "flat_thermal_rel_budget", {0.0, 1.0, -0.25, 1.5, nan, inf, -inf}, {tiny, std::nextafter(1.0, 0.0)}},
  };
  for (const auto& row : rows) {
    for (double v : row.bad) {
      SCOPED_TRACE(std::string(row.name) + " = " + std::to_string(v));
      calibration_analysis_input in = input_of(thermal_config);
      set(in, row.f, v);
      EXPECT_TRUE(analysis_stops(in, error_category::invalid_input, std::string("calib.param.") + row.name, row.name));
    }
    for (double v : row.good) {
      SCOPED_TRACE(std::string("controllo: ") + row.name + " = " + std::to_string(v));
      calibration_analysis_input in = input_of(thermal_config);
      set(in, row.f, v);
      EXPECT_TRUE(analysis_completes(in));
    }
  }
  // Una sola delle due dichiarazioni termiche: l'altra è assente (`calib.param.<nome mancante>`).
  {
    SCOPED_TRACE("solo flat_thermal_bound_adu");
    calibration_analysis_input in = input_of(thermal_config);
    in.declarations.flat_thermal_rel_budget.reset();
    EXPECT_TRUE(analysis_stops(in, error_category::invalid_input, "calib.param.flat_thermal_rel_budget",
                               "flat_thermal_rel_budget"));
  }
  {
    SCOPED_TRACE("solo flat_thermal_rel_budget");
    calibration_analysis_input in = input_of(thermal_config);
    in.declarations.flat_thermal_bound_adu.reset();
    EXPECT_TRUE(analysis_stops(in, error_category::invalid_input, "calib.param.flat_thermal_bound_adu",
                               "flat_thermal_bound_adu"));
  }
}

// `temperature_policy = not_applicable_declared` con `temperature_abs_tol_c` presente (ADR-16.a §2: il secondo valore
// della politica salta solo il confronto di temperatura; T-M2E-09). Bias valido a −8 °C e light a −10 °C: con `compare` e
// tolleranza 0,5 °C le coppie L–B e F–B (flat −6 °C) sono `incompatible.temperature_c`; con `not_applicable_declared` e la
// stessa tolleranza il confronto non avviene, il bias è `usable` e il report ha l'avviso di ogni coppia additiva.
TEST(CalibSelection, T_M3A_CAL_06_temperature_tolerance_with_not_applicable_declared_skips_the_comparison) {
  const config c{false, dark_kind::absent, bias_kind::valid, flat_kind::raw, dark_flat_kind::absent, thermal_kind::absent};
  calibration_analysis_input in = input_of(c);
  in.declarations.temperature_abs_tol_c = 0.5;
  {
    SCOPED_TRACE("controllo: compare, 2 °C oltre 0,5 °C");
    const calibration_analysis a = analyze_calibration(in);
    const candidate_analysis* b = find_candidate(a, master_role::bias);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->status, candidate_status::excluded);
    for (const chain_verdict& v : b->chains) EXPECT_EQ(v.reason.value_or(""), "incompatible.temperature_c") << v.chain;
  }
  in.declarations.temp_policy = temperature_policy::not_applicable_declared;
  const calibration_analysis a = analyze_calibration(in);
  const candidate_analysis* b = find_candidate(a, master_role::bias);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->status, candidate_status::usable);
  ASSERT_FALSE(b->chains.empty());
  for (const chain_verdict& v : b->chains) {
    EXPECT_EQ(v.status, candidate_status::usable) << v.chain;
    EXPECT_FALSE(v.reason.has_value()) << v.chain;
  }
  for (const char* pair : {"temperature_comparison_skipped:L-B", "temperature_comparison_skipped:F-B"}) {
    EXPECT_NE(std::find(a.warnings.begin(), a.warnings.end(), pair), a.warnings.end()) << pair;
  }
  EXPECT_EQ(a.predicates.dfb, "P-DFB011");
  EXPECT_EQ(a.default_recipe.light, light_chain::bias);
}

// `temperature_abs_tol_c` obbligatorio e nel dominio (≥ 0, finito) con entrambe le `temperature_policy` (ADR-16 §b.9,
// ADR-16.a «Parametri obbligatori»): con `not_applicable_declared` non entra nel confronto ma non è un parametro non
// applicabile. Analisi: assente, NaN, ±∞ o negativo → `invalid_input`, `calib.param.temperature_abs_tol_c`; 0 e 0,5
// completano. Ricetta: la tolleranza dichiarata passa, una diversa o assente → `calib.param.temperature_abs_tol_c`, mai
// `calib.param_not_applicable.temperature_abs_tol_c`; la ricetta di default porta la dichiarazione.
TEST(CalibSelection, T_M3A_CAL_06_temperature_tolerance_is_required_with_not_applicable_declared) {
  const config c{false, dark_kind::absent, bias_kind::valid, flat_kind::raw, dark_flat_kind::absent, thermal_kind::absent};
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  const auto with = [&](std::optional<double> tol) {
    calibration_analysis_input in = input_of(c);
    in.declarations.temp_policy = temperature_policy::not_applicable_declared;
    in.declarations.temperature_abs_tol_c = tol;
    return in;
  };
  const std::optional<double> bad[] = {std::nullopt, nan, inf, -inf, -0.5, -std::numeric_limits<double>::denorm_min()};
  for (const std::optional<double>& v : bad) {
    SCOPED_TRACE("analisi: temperature_abs_tol_c = " + (v ? std::to_string(*v) : std::string("assente")));
    EXPECT_TRUE(analysis_stops(with(v), error_category::invalid_input, "calib.param.temperature_abs_tol_c",
                               "temperature_abs_tol_c"));
  }
  for (double v : {0.0, 0.5}) {
    SCOPED_TRACE("controllo: analisi con temperature_abs_tol_c = " + std::to_string(v));
    EXPECT_TRUE(analysis_completes(with(v)));
  }

  const calibration_analysis a = analyze_calibration(with(0.5));
  ASSERT_EQ(a.default_recipe.light, light_chain::bias);
  EXPECT_EQ(a.default_recipe.temp_policy, temperature_policy::not_applicable_declared);
  EXPECT_EQ(a.default_recipe.temperature_abs_tol_c, std::optional<double>(0.5));
  EXPECT_TRUE(passes(a, a.default_recipe));
  chain_recipe r = recipe_for(light_chain::bias, flat_chain::none, false);
  r.temp_policy = temperature_policy::not_applicable_declared;
  r.temperature_abs_tol_c = 0.5;
  EXPECT_TRUE(passes(a, r));
  for (const std::optional<double>& v : {std::optional<double>(2.0), std::optional<double>(0.0), std::optional<double>()}) {
    SCOPED_TRACE("ricetta: temperature_abs_tol_c = " + (v ? std::to_string(*v) : std::string("assente")));
    r.temperature_abs_tol_c = v;
    EXPECT_TRUE(stops_on_param(a, r, "temperature_abs_tol_c"));
  }
}

}  // namespace

