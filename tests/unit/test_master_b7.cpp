// M3.b, unità B7: livelli e statistiche del flat per canale (C-02.b §b.2 «Canali CFA», §b.6 «Flat», §b.12 riga
// T-M3B-C02-14, §b.13 punto 4; C-02.a §1, §2, §6.2, §7 e «Aggiunte di R2» per il flat mono).
// Test: T-M3B-C02-14 (livello fuori tolleranza in un solo canale con livello globale entro tolleranza;
// `level_bias_estimate` per canale; flat mono identico a `.a`), versione dell'algoritmo del nodo, riproducibilità del job;
// T-M3B-MEM-01 (minimo di memoria del job con Cr = 2 per un master CFA, ADR-06 «Estensione M3.b del piano del master»).
// Fuori da questo file: la parte di T-M3B-C02-14 sullo stato `flat_level` nell'analisi resta a B13.
//
// Oracle scritti qui, nessun valore atteso dal prodotto:
// - livelli: verità di FIX-SYN-CAL-08 (regione di livello: m_(f,c) = c_(f,c), tests/fixtures/syn_cal.hpp) oppure
//   mediana calcolata qui sui campioni del canale; m̄_c media dei livelli; scarto m_(f,c)/m̄_c − 1 in f64 da operandi
//   esatti, come lo specifica C-02.b §b.6;
// - M, V e level_bias_estimate in aritmetica razionale su casi in cui ogni operazione è esatta (tolleranza 0);
//   rapporto col piano e mediane pari con le operazioni f64 specificate da operandi esatti: quoziente correttamente
//   arrotondato; (z_k + z_(k+1)) / 2, somma poi divisione (C-02.a §1).
// Tolleranza: 0 ovunque (classe B per gli stop, E per i valori). Confine della tolleranza: la regola è `>` (C-02.a §2),
// quindi lo scarto uguale è ammesso; predecessore f64 della tolleranza escluso, successore ammesso.
//
// Forma del report per canale: C-02.b non la fissa. Proposta di questa unità, letta solo dagli helper `*_of` sotto:
// - frame di un flat CFA: `flat_levels`, un elemento per canale nell'ordine di `channels`, {label, level, rel_dev};
//   nessun `flat_level` né `flat_level_rel_dev` (il livello globale del frame non esiste più per un flat CFA);
// - `channels[c].flat_level_mean` = m̄_c; nessun `flat_level_mean` in cima al report di un flat CFA;
// - flat mono: i campi di `.a` (`flat_level`, `flat_level_rel_dev`, `flat_level_mean` in cima), nessun campo nuovo.
// Stop per canale: il contesto di `.a` più `canale` con l'etichetta del canale (R, G1, G2, B), come
// `master.flat_norm_budget` (C-02.b-flat §f.2) e `calib.normalization_invalid`.
// Canali: indice 2·((y + YBAYROFF) mod 2) + ((x + XBAYROFF) mod 2) del motivo; etichetta = lettera del pattern, i
// verdi G1 (primo in ordine di riga del motivo) e G2 (C-02.b §b.2, ADR-16.a §6).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "master_support.hpp"
#include "store_test_support.hpp"
#include "syn_cal.hpp"

namespace {

using namespace mst;
namespace sc = astro_fixture::syn_cal;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kTol = 0.0625;  // 1/16, esatto in f64
const std::array<const char*, 4> kPatterns = {"RGGB", "BGGR", "GRBG", "GBRG"};

// ---------------------------------------------------------------- canali

struct cfa_layout {
  std::string pattern;
  int ox = 0, oy = 0;
  std::string name() const { return pattern + "@" + std::to_string(ox) + "," + std::to_string(oy); }
};

std::array<std::string, 4> motif_labels(const std::string& pattern) {
  std::array<std::string, 4> out;
  int greens = 0;
  for (std::size_t k = 0; k < 4; ++k) {
    out[k] = pattern[k] == 'G' ? "G" + std::to_string(++greens) : std::string(1, pattern[k]);
  }
  return out;
}

std::size_t motif_index(std::uint32_t x, std::uint32_t y, int ox, int oy) {
  return 2 * ((y + static_cast<std::uint32_t>(oy)) & 1u) + ((x + static_cast<std::uint32_t>(ox)) & 1u);
}

// levels[f][k]: livello (x − offset) del frame f nella posizione k del motivo; ogni pixel della posizione vale
// offset + livello, quindi m_(f,c) = levels[f][c] per costruzione.
group cfa_flat(const cfa_layout& l, std::uint32_t w, std::uint32_t h, const std::vector<std::array<double, 4>>& levels) {
  group g = make_group(master_role::flat, w, h, levels.size());
  for (std::size_t f = 0; f < levels.size(); ++f) {
    mframe& fr = g.frames[f];
    fr.hdr.bayer = l.pattern;
    fr.hdr.bayer_x = l.ox;
    fr.hdr.bayer_y = l.oy;
    for (std::uint32_t y = 0; y < h; ++y)
      for (std::uint32_t x = 0; x < w; ++x) fr.v[std::size_t{y} * w + x] = fr.hdr.offset + levels[f][motif_index(x, y, l.ox, l.oy)];
  }
  return g;
}

// ---------------------------------------------------------------- oracle

// Mediana con la regola di C-02.a §1: elemento centrale, oppure (z_k + z_(k+1)) / 2 in f64.
double median_rule(std::vector<double> z) {
  std::sort(z.begin(), z.end());
  const std::size_t n = z.size();
  return n % 2 ? z[n / 2] : (z[n / 2 - 1] + z[n / 2]) / 2;
}

// Mediana di (x − offset) sui pixel non NaN del frame, sull'insieme di pixel scelto da `keep`.
template <class Keep>
std::optional<double> level_where(const group& g, std::size_t f, Keep keep) {
  std::vector<double> z;
  for (std::uint32_t y = 0; y < g.h; ++y)
    for (std::uint32_t x = 0; x < g.w; ++x) {
      const double v = g.frames[f].v[std::size_t{y} * g.w + x];
      if (!std::isnan(v) && keep(x, y)) z.push_back(v - g.frames[f].hdr.offset);
    }
  if (z.empty()) return std::nullopt;
  return median_rule(z);
}

// Massimo di |m_f / m̄ − 1| per il livello scelto da `keep` (precondizioni dei casi: livello globale, verdi uniti).
template <class Keep>
double max_rel_dev(const group& g, Keep keep) {
  std::vector<double> lv;
  for (std::size_t f = 0; f < g.frames.size(); ++f) lv.push_back(*level_where(g, f, keep));
  double sum = 0;
  for (double v : lv) sum += v;
  const double mean = sum / static_cast<double>(lv.size());
  double worst = 0;
  for (double v : lv) worst = std::max(worst, std::fabs(v / mean - 1));
  return worst;
}

// Posizione canonica → indice del frame nella ricetta (C-02.a §8: digest SHA-256 crescente).
std::vector<std::size_t> canonical(const master_recipe& r) {
  std::vector<std::size_t> order(r.frames.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return r.frames[a].digest.sha256 < r.frames[b].digest.sha256; });
  return order;
}

std::size_t canonical_position(const master_recipe& r, std::size_t generator_index) {
  const std::vector<std::size_t> order = canonical(r);
  return static_cast<std::size_t>(std::find(order.begin(), order.end(), generator_index) - order.begin());
}

// Nome del frame nel contesto degli stop: posizione canonica a tre cifre.
std::string frame_label(std::size_t canonical_pos) {
  char b[32];
  std::snprintf(b, sizeof b, "frame_%03zu", canonical_pos);
  return b;
}

std::string num(double v) {
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", v);
  return b;
}

// ---------------------------------------------------------------- stop e report

std::optional<std::string> ctx(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

std::set<std::string> ctx_keys(const stop_error& e) {
  std::set<std::string> out;
  for (const auto& kv : e.context()) out.insert(kv.first);
  return out;
}

std::optional<stop_error> stop_of(const master_recipe& r, const group& g) {
  try {
    (void)combine_master(r, images_of(g));
  } catch (const stop_error& e) {
    return e;
  }
  return std::nullopt;
}

std::optional<master_result> combine_or_fail(const master_recipe& r, const group& g, const std::string& what) {
  try {
    return combine_master(r, images_of(g));
  } catch (const stop_error& e) {
    ADD_FAILURE() << what << ": stop " << e.code() << " — " << e.what();
    return std::nullopt;
  }
}

const json* labelled(const json* list, const std::string& label) {
  if (list == nullptr || list->type() != json::kind::array) return nullptr;
  for (const json& e : list->items()) {
    const json* l = e.find("label");
    if (l != nullptr && l->type() == json::kind::string && l->as_string() == label) return &e;
  }
  return nullptr;
}

std::optional<double> number_of(const json* obj, const char* key) {
  if (obj == nullptr) return std::nullopt;
  const json* v = obj->find(key);
  if (v == nullptr) return std::nullopt;
  if (v->type() == json::kind::number) return v->as_number();
  if (v->type() == json::kind::integer) return static_cast<double>(v->as_integer());
  return std::nullopt;
}

const json* frame_of(const json& report, std::size_t k) {
  const json* frames = report.find("frames");
  if (frames == nullptr || frames->type() != json::kind::array || k >= frames->items().size()) return nullptr;
  return &frames->items()[k];
}

// m_(f,c) del frame canonico k nel canale `label`.
std::optional<double> level_of(const json& report, std::size_t k, const std::string& label) {
  const json* f = frame_of(report, k);
  return number_of(labelled(f ? f->find("flat_levels") : nullptr, label), "level");
}
std::optional<double> rel_dev_of(const json& report, std::size_t k, const std::string& label) {
  const json* f = frame_of(report, k);
  return number_of(labelled(f ? f->find("flat_levels") : nullptr, label), "rel_dev");
}
// m̄_c del canale `label`.
std::optional<double> level_mean_of(const json& report, const std::string& label) {
  return number_of(labelled(report.find("channels"), label), "flat_level_mean");
}

// ---------------------------------------------------------------- fixture: flat CFA a livelli per canale

// FIX-SYN-CAL-08 senza rumore: c_(f,c) = s_c · (960, 1024, 1024, 1088), s = (1, 5/4, 3/2, 2); m̄_c = 1024 s_c e
// scarti (−1/16, 0, 0, +1/16) in ogni canale: con tolleranza 1/16 ogni canale sta sul confine ammesso.
std::vector<std::vector<sc::rat>> b7_levels() {
  const std::int64_t base[4] = {960, 1024, 1024, 1088};
  std::vector<std::vector<sc::rat>> out;
  for (std::size_t c = 0; c < 4; ++c) {
    std::vector<sc::rat> lc;
    for (std::int64_t b : base) lc.push_back(sc::q(b * sc::channel_scale_quarters[c], 4));
    out.push_back(lc);
  }
  return out;
}

struct fixture_flat {
  sc::flat_set set;
  group g;
};

fixture_flat fixture_cfa_flat(std::uint32_t w, std::uint32_t h, const std::string& pattern) {
  fixture_flat out{sc::make_flat(w, h, true, sc::q(512), b7_levels(), sc::majority_level_pixels(w, h, true), std::nullopt,
                                 sc::pattern_measure()),
                   {}};
  sc::gauss_stream gs(0);  // varianza nulla ovunque: nessuna estrazione
  const sc::frames x = sc::realize(out.set.set, gs);
  EXPECT_EQ(gs.count(), 0u);
  out.g = make_group(master_role::flat, w, h, x.size());
  for (std::size_t f = 0; f < x.size(); ++f) {
    out.g.frames[f].hdr.bayer = pattern;
    out.g.frames[f].hdr.offset = 512;
    out.g.frames[f].v = x[f];
  }
  return out;
}

// ================================================================ T-M3B-C02-14: tolleranza per canale

// Tre posizioni del motivo a 1024 in ogni frame; la posizione `bad` a 960, 960, 1152 (m̄ = 1024, scarti −1/16, −1/16,
// +1/8). Tolleranza 1/16: oltre solo il terzo frame, solo in quel canale. Livello globale: 12 pixel su 16 a 1024 in
// ogni frame, scarto 0. Per una posizione verde, anche il livello dei due verdi uniti resta entro la tolleranza:
// lo stop dimostra G1 e G2 separati.
TEST(MasterB7, T_M3B_C02_14_one_cfa_channel_beyond_tolerance_stops_although_the_global_level_is_within) {
  std::vector<cfa_layout> layouts;
  for (const char* p : kPatterns) layouts.push_back({p, 0, 0});
  for (const auto& [ox, oy] : {std::pair{1, 0}, std::pair{0, 1}, std::pair{1, 1}}) layouts.push_back({"RGGB", ox, oy});
  for (const cfa_layout& l : layouts) {
    const std::array<std::string, 4> labels = motif_labels(l.pattern);
    for (std::size_t bad = 0; bad < 4; ++bad) {
      SCOPED_TRACE(l.name() + ", canale " + labels[bad]);
      std::vector<std::array<double, 4>> levels(3, {1024, 1024, 1024, 1024});
      levels[0][bad] = 960;
      levels[1][bad] = 960;
      levels[2][bad] = 1152;
      const group g = cfa_flat(l, 4, 4, levels);
      const master_recipe r = recipe_of(g, kTol);
      ASSERT_LE(max_rel_dev(g, [](std::uint32_t, std::uint32_t) { return true; }), kTol) << "livello globale fuori";
      if (labels[bad][0] == 'G') {
        const auto green = [&](std::uint32_t x, std::uint32_t y) { return labels[motif_index(x, y, l.ox, l.oy)][0] == 'G'; };
        ASSERT_LE(max_rel_dev(g, green), kTol) << "verdi uniti fuori";
      }
      const std::optional<stop_error> e = stop_of(r, g);
      ASSERT_TRUE(e.has_value()) << "nessuno stop";
      EXPECT_EQ(e->category(), error_category::unsupported) << e->what();
      EXPECT_EQ(e->code(), "master.variance_domain") << e->what();
      EXPECT_EQ(ctx_keys(*e), (std::set<std::string>{"canale", "frame", "m_f", "m_media", "tolleranza"}));
      EXPECT_EQ(ctx(*e, "canale"), std::optional<std::string>(labels[bad]));
      EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(canonical_position(r, 2))));
      EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>("1152"));
      EXPECT_EQ(ctx(*e, "m_media"), std::optional<std::string>("1024"));
      EXPECT_EQ(ctx(*e, "tolleranza"), std::optional<std::string>("0.0625"));
    }
  }
}

// Confine esatto in f64, canale B di RGGB a 960 e 1088 (scarti ∓1/16 esatti), altri canali a 1024: tolleranza 1/16
// ammessa (la regola è `>`), predecessore f64 escluso, successore ammesso.
TEST(MasterB7, T_M3B_C02_14_cfa_channel_tolerance_boundary_is_exact_in_f64) {
  const group g = cfa_flat({"RGGB", 0, 0}, 4, 4, {{1024, 1024, 1024, 960}, {1024, 1024, 1024, 1088}});
  const master_recipe base = recipe_of(g, kTol);
  const std::size_t first = canonical(base)[0];
  const double first_level = first == 0 ? 960 : 1088;
  {
    SCOPED_TRACE("tolleranza uguale allo scarto: ammesso");
    const std::optional<master_result> m = combine_or_fail(base, g, "uguale");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(rel_dev_of(m->report, 0, "B"), std::optional<double>(first_level / 1024 - 1));
    EXPECT_EQ(rel_dev_of(m->report, 1, "B"), std::optional<double>((first == 0 ? 1088.0 : 960.0) / 1024 - 1));
  }
  {
    SCOPED_TRACE("predecessore f64 della tolleranza: stop");
    master_recipe r = base;
    r.flat_level_rel_tol = std::nextafter(kTol, 0.0);
    const std::optional<stop_error> e = stop_of(r, g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    EXPECT_EQ(e->category(), error_category::unsupported) << e->what();
    EXPECT_EQ(e->code(), "master.variance_domain") << e->what();
    EXPECT_EQ(ctx(*e, "canale"), std::optional<std::string>("B"));
    EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(0)));
    EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>(num(first_level)));
    EXPECT_EQ(ctx(*e, "m_media"), std::optional<std::string>("1024"));
    EXPECT_EQ(ctx(*e, "tolleranza"), std::optional<std::string>(num(std::nextafter(kTol, 0.0))));
  }
  {
    SCOPED_TRACE("successore f64 della tolleranza: ammesso");
    master_recipe r = base;
    r.flat_level_rel_tol = std::nextafter(kTol, 1.0);
    EXPECT_TRUE(combine_or_fail(r, g, "successore").has_value());
  }
}

// Tutti i canali entro la tolleranza (sul confine, scarti ±1/16): ammesso; nel report m_(f,c), lo scarto e m̄_c per
// canale uguali alla verità della fixture, per ogni motivo Bayer; nessun livello globale del frame.
TEST(MasterB7, T_M3B_C02_14_cfa_channels_within_tolerance_are_admitted_and_reported_per_channel) {
  for (const char* pattern : kPatterns) {
    SCOPED_TRACE(pattern);
    const fixture_flat fx = fixture_cfa_flat(8, 8, pattern);
    const master_recipe r = recipe_of(fx.g, kTol);
    const std::optional<master_result> m = combine_or_fail(r, fx.g, pattern);
    if (!m) continue;
    const std::array<std::string, 4> labels = motif_labels(pattern);
    const std::vector<std::size_t> order = canonical(r);
    for (std::size_t c = 0; c < 4; ++c) {
      SCOPED_TRACE(labels[c]);
      sc::rat sum = sc::q(0);
      for (const sc::rat& v : fx.set.levels[c]) sum = sum + v;
      const double mean = sc::exact_double(sum / sc::q(4), "m̄_c");
      EXPECT_EQ(level_mean_of(m->report, labels[c]), std::optional<double>(mean));
      for (std::size_t k = 0; k < order.size(); ++k) {
        const double lv = sc::exact_double(fx.set.levels[c][order[k]], "m_(f,c)");
        EXPECT_EQ(level_of(m->report, k, labels[c]), std::optional<double>(lv)) << "frame canonico " << k;
        EXPECT_EQ(rel_dev_of(m->report, k, labels[c]), std::optional<double>(lv / mean - 1)) << "frame canonico " << k;
      }
    }
    for (std::size_t k = 0; k < order.size(); ++k) {
      const json* f = frame_of(m->report, k);
      ASSERT_NE(f, nullptr);
      EXPECT_EQ(f->find("flat_level"), nullptr) << "livello globale in un flat CFA, frame " << k;
      EXPECT_EQ(f->find("flat_level_rel_dev"), nullptr) << "scarto globale in un flat CFA, frame " << k;
    }
    EXPECT_EQ(m->report.find("flat_level_mean"), nullptr) << "media globale dei livelli in un flat CFA";
  }
}

// `flat_min_signal_adu` vale per ogni m_(f,c) (C-02.b-flat §f.2: «garantisce m_(f,c) > 0»): R a 0.5 ADU in ogni frame,
// altri canali a 1024, livello globale 1024.
TEST(MasterB7, T_M3B_C02_14_cfa_channel_level_below_min_signal_stops_with_flat_low_signal) {
  const group g = cfa_flat({"RGGB", 0, 0}, 4, 4, std::vector<std::array<double, 4>>(3, {0.5, 1024, 1024, 1024}));
  const master_recipe r = recipe_of(g, kTol);
  ASSERT_EQ(*r.flat_min_signal_adu, 1.0);
  ASSERT_EQ(level_where(g, 0, [](std::uint32_t, std::uint32_t) { return true; }), std::optional<double>(1024));
  const std::optional<stop_error> e = stop_of(r, g);
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  EXPECT_EQ(e->category(), error_category::invalid_input) << e->what();
  EXPECT_EQ(e->code(), "master.flat_low_signal") << e->what();
  EXPECT_EQ(ctx(*e, "canale"), std::optional<std::string>("R"));
  EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>("0.5"));
  EXPECT_EQ(ctx(*e, "minimo"), std::optional<std::string>("1"));
  EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(0)));  // R = 0.5 in ogni frame: primo canonico
}

// Un canale senza pixel validi in un frame non ha m_(f,c): stop `master.flat_low_signal` (C-02.a §2, «nessun pixel
// valido», per canale), anche se il frame ha pixel validi negli altri canali.
TEST(MasterB7, T_M3B_C02_14_cfa_channel_without_valid_pixels_in_a_frame_stops_with_flat_low_signal) {
  group g = cfa_flat({"RGGB", 0, 0}, 4, 4, std::vector<std::array<double, 4>>(3, {1024, 1024, 1024, 1024}));
  for (const std::size_t i : {0, 2, 8, 10}) g.frames[1].v[i] = kNaN;  // tutti i pixel R del frame 1
  const master_recipe r = recipe_of(g, kTol);
  const std::optional<stop_error> e = stop_of(r, g);
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  EXPECT_EQ(e->category(), error_category::invalid_input) << e->what();
  EXPECT_EQ(e->code(), "master.flat_low_signal") << e->what();
  EXPECT_EQ(ctx(*e, "canale"), std::optional<std::string>("R"));
  EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(canonical_position(r, 1))));
}

// Frame `dev` con motivo `bayer` ("" = mono) e la posizione 0 del motivo RGGB a 0.5 ADU (sotto flat_min_signal_adu se
// letto col motivo del riferimento; da mono il suo livello è 1024), gli altri frame RGGB a 1024 ovunque. `dev` è il
// primo frame che non è il riferimento canonico.
struct deviant_case {
  group g;
  master_recipe r;
  std::size_t dev = 0;
};

std::optional<deviant_case> deviant_cfa_case(const std::string& bayer) {
  for (std::size_t dev = 0; dev < 3; ++dev) {
    std::vector<std::array<double, 4>> levels(3, {1024, 1024, 1024, 1024});
    levels[dev][0] = 0.5;
    group g = cfa_flat({"RGGB", 0, 0}, 4, 4, levels);
    g.frames[dev].hdr.bayer = bayer;
    master_recipe r = recipe_of(g, kTol);
    if (canonical(r)[0] != dev) return deviant_case{std::move(g), std::move(r), dev};
  }
  return std::nullopt;
}

void expect_cfa_mismatch(const stop_error& e, const deviant_case& c) {
  EXPECT_EQ(e.category(), error_category::incompatible) << e.code() << " — " << e.what();
  EXPECT_EQ(e.code(), "master.mismatch.cfa") << e.what();
  EXPECT_EQ(ctx(e, "campo"), std::optional<std::string>("cfa"));
  EXPECT_EQ(ctx(e, "coppia"), std::optional<std::string>(frame_label(0) + "-" + frame_label(canonical_position(c.r, c.dev))));
}

// Il motivo di un frame k > 0 si confronta col riferimento prima dei suoi livelli m_(f,c) (C-02.a §2, C-02.b §b.2): un
// frame con altro motivo o mono si ferma con `master.mismatch.cfa`, non con lo stop del segnale minimo per canale.
TEST(MasterB7, T_M3B_C02_14_frame_with_another_cfa_motif_stops_on_cfa_mismatch_before_its_channel_levels) {
  for (const std::string bayer : {"BGGR", ""}) {
    SCOPED_TRACE(bayer.empty() ? "mono" : bayer);
    const std::optional<deviant_case> c = deviant_cfa_case(bayer);
    ASSERT_TRUE(c.has_value()) << "il frame diverso è sempre il riferimento canonico";
    const std::optional<stop_error> e = stop_of(c->r, c->g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_cfa_mismatch(*e, *c);
  }
}

TEST(MasterB7, T_M3B_C02_14_job_frame_with_another_cfa_motif_stops_on_cfa_mismatch_before_its_channel_levels) {
  const fs::path dir = fresh_dir("b7-job-cfa-mismatch");
  for (const std::string bayer : {"BGGR", ""}) {
    SCOPED_TRACE(bayer.empty() ? "mono" : bayer);
    std::optional<deviant_case> c = deviant_cfa_case(bayer);
    ASSERT_TRUE(c.has_value()) << "il frame diverso è sempre il riferimento canonico";
    const std::string tag = bayer.empty() ? "mono" : bayer;
    write_frames(c->g, c->r, dir, tag);
    auto store = sqlite_dag_store::create(dir / ("store-" + tag));
    const node_spec node = make_master_node(c->r);
    store->put_node(node);
    store->create_slot("master", node.key());
    memory_pool pool(256 * (std::uint64_t{1} << 20));
    std::optional<stop_error> e;
    try {
      const job_outcome out = run_master_quiet(*store, "master", c->r, pool);
      EXPECT_NE(out.exit_code, 0);
      if (out.stop) e = *out.stop;
    } catch (const stop_error& thrown) {
      e = thrown;
    }
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_cfa_mismatch(*e, *c);
    EXPECT_EQ(by_role(*store, node.key()).count("primary"), 0u);
  }
}

// Nel job lo stop per canale arriva nel preflight: nessuna lettura per blocchi, nessun artefatto pubblicato.
TEST(MasterB7, T_M3B_C02_14_job_stops_on_one_channel_before_any_block_and_publishes_nothing) {
  const fs::path dir = fresh_dir("b7-job-stop");
  std::vector<std::array<double, 4>> levels(3, {1024, 1024, 1024, 1024});
  levels[0][1] = 960;
  levels[1][1] = 960;
  levels[2][1] = 1152;
  const group g = cfa_flat({"RGGB", 0, 0}, 4, 4, levels);
  master_recipe r = recipe_of(g, kTol);
  write_frames(g, r, dir, "f");
  auto store = sqlite_dag_store::create(dir / "store");
  const node_spec node = make_master_node(r);
  store->put_node(node);
  store->create_slot("master", node.key());
  bool frames_read = false;
  master_job_options o;
  o.hook = [&frames_read](std::string_view point) {
    if (point == "frames.read") frames_read = true;
  };
  memory_pool pool(256 * (std::uint64_t{1} << 20));
  std::optional<stop_error> e;
  try {
    const job_outcome out = run_master_quiet(*store, "master", r, pool, o);
    EXPECT_NE(out.exit_code, 0);
    if (out.stop) e = *out.stop;
  } catch (const stop_error& thrown) {
    e = thrown;
  }
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  EXPECT_EQ(e->category(), error_category::unsupported) << e->what();
  EXPECT_EQ(e->code(), "master.variance_domain") << e->what();
  EXPECT_EQ(ctx(*e, "canale"), std::optional<std::string>("G1"));
  EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(canonical_position(r, 2))));
  EXPECT_FALSE(frames_read);
  const auto roles = by_role(*store, node.key());
  EXPECT_EQ(roles.count("primary"), 0u);
  EXPECT_EQ(roles.count("report"), 0u);
}

// ================================================================ T-M3B-C02-14: level_bias_estimate per canale

// RGGB 8 × 4, n = 2, offset 10, ε = 1/8. Per pixel x_0 = 10 + b_c + v − d, x_1 = 10 + b_c + v + d: M = 10 + b_c + v,
// V = d², level_bias_estimate = ε² max(M − 10, 0)² (n − 1 = 1), tutti esatti. Casi: un pixel G1 con d = 0 (piano
// nullo, rapporto null); un pixel B con M − 10 = −2 (pixels_nonpositive_signal, stima 0); un NaN in G2 su un pixel con
// l'altro campione grande (invalido, escluso); i pixel R invalidi metà nel frame 0 e metà nel frame 1, così ogni
// frame ha un livello R ma il master non ha pixel R validi (statistiche null).
TEST(MasterB7, T_M3B_C02_14_level_bias_estimate_per_channel_matches_the_oracle_on_valid_pixels) {
  constexpr std::uint32_t w = 8, h = 4;
  const double base[4] = {1024, 1280, 1536, 2048};
  group g = make_group(master_role::flat, w, h, 2);
  for (mframe& f : g.frames) f.hdr.bayer = "RGGB";
  std::size_t r_seen = 0;
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = std::size_t{y} * w + x, c = motif_index(x, y, 0, 0);
      const double v = static_cast<double>((i * 5) % 7);
      const double d = static_cast<double>(1u << (i % 3));
      g.frames[0].v[i] = 10 + base[c] + v - d;
      g.frames[1].v[i] = 10 + base[c] + v + d;
      if (c == 0) g.frames[(r_seen++) % 2].v[i] = kNaN;
    }
  }
  const std::size_t zero_plane = 1;  // (1, 0): G1
  g.frames[0].v[zero_plane] = g.frames[1].v[zero_plane] = 10 + 1300;
  const std::size_t nonpositive = 1 * w + 3;  // (3, 1): B
  g.frames[0].v[nonpositive] = 10 - 8;
  g.frames[1].v[nonpositive] = 10 + 4;
  const std::size_t invalid = 1 * w + 4;  // (4, 1): G2
  g.frames[0].v[invalid] = kNaN;
  g.frames[1].v[invalid] = 50000;
  ASSERT_EQ(motif_index(1, 0, 0, 0), 1u);
  ASSERT_EQ(motif_index(3, 1, 0, 0), 3u);
  ASSERT_EQ(motif_index(4, 1, 0, 0), 2u);

  const double eps = 0.125;
  const master_recipe r = recipe_of(g, eps);
  for (std::size_t c = 0; c < 4; ++c) {
    ASSERT_LE(max_rel_dev(g, [c](std::uint32_t x, std::uint32_t y) { return motif_index(x, y, 0, 0) == c; }), eps)
        << "livelli del canale " << c << " fuori dalla tolleranza: il caso non arriva alle statistiche";
  }
  const std::optional<master_result> m = combine_or_fail(r, g, "ammesso");
  ASSERT_TRUE(m.has_value());

  const std::array<std::string, 4> labels = motif_labels("RGGB");
  const sc::rat e2 = sc::q(1, 64);
  for (std::size_t c = 0; c < 4; ++c) {
    SCOPED_TRACE(labels[c]);
    std::vector<double> lbe, ratio;
    std::uint64_t valid = 0, ratio_null = 0, nonpos = 0;
    for (std::uint32_t y = 0; y < h; ++y) {
      for (std::uint32_t x = 0; x < w; ++x) {
        const std::size_t i = std::size_t{y} * w + x;
        if (motif_index(x, y, 0, 0) != c || std::isnan(g.frames[0].v[i]) || std::isnan(g.frames[1].v[i])) continue;
        ++valid;
        const sc::rat x0 = exact::from_double(g.frames[0].v[i]), x1 = exact::from_double(g.frames[1].v[i]);
        const sc::rat M = (x0 + x1) / sc::q(2);
        const sc::rat d = (x1 - x0) / sc::q(2);
        const sc::rat V = d * d * sc::q(2) / sc::q(2);  // s² = 2 d² / (n − 1), V = s² / n
        const sc::rat a = M - sc::q(10);
        const sc::rat est = sc::q(0) < a ? e2 * a * a : sc::q(0);
        if (!(sc::q(0) < a)) ++nonpos;
        lbe.push_back(sc::exact_double(est, "level_bias_estimate"));
        EXPECT_EQ(m->level_bias_estimate[i], lbe.back()) << "pixel " << i;
        const double vd = sc::exact_double(V, "piano");
        if (vd == 0) ++ratio_null;
        else ratio.push_back(lbe.back() / vd);
      }
    }
    const json* ch = labelled(m->report.find("channels"), labels[c]);
    ASSERT_NE(ch, nullptr) << "canale assente dal report";
    const json* lb = ch->find("level_bias_estimate");
    ASSERT_NE(lb, nullptr);
    EXPECT_EQ(number_of(ch, "valid_pixels"), std::optional<double>(static_cast<double>(valid)));
    EXPECT_EQ(number_of(ch, "pixels_nonpositive_signal"), std::optional<double>(static_cast<double>(nonpos)));
    if (valid == 0) {
      for (const char* k : {"median", "max", "ratio_median", "ratio_max"}) {
        ASSERT_NE(lb->find(k), nullptr) << k;
        EXPECT_EQ(lb->find(k)->type(), json::kind::null) << k;
      }
      continue;
    }
    EXPECT_EQ(number_of(lb, "median"), std::optional<double>(median_rule(lbe)));
    EXPECT_EQ(number_of(lb, "max"), std::optional<double>(*std::max_element(lbe.begin(), lbe.end())));
    EXPECT_EQ(number_of(lb, "ratio_median"), std::optional<double>(median_rule(ratio)));
    EXPECT_EQ(number_of(lb, "ratio_max"), std::optional<double>(*std::max_element(ratio.begin(), ratio.end())));
    EXPECT_EQ(number_of(lb, "level_bias_ratio_null"), std::optional<double>(static_cast<double>(ratio_null)));
  }
  // Precondizioni dei casi: R senza pixel validi nel master, un piano nullo in G1, un segnale non positivo in B.
  const json* channels = m->report.find("channels");
  const json* g1 = labelled(channels, "G1");
  EXPECT_EQ(number_of(labelled(channels, "R"), "valid_pixels"), std::optional<double>(0));
  EXPECT_EQ(number_of(g1 ? g1->find("level_bias_estimate") : nullptr, "level_bias_ratio_null"), std::optional<double>(1));
  EXPECT_EQ(number_of(labelled(channels, "B"), "pixels_nonpositive_signal"), std::optional<double>(1));
}

// ================================================================ T-M3B-C02-14: flat mono invariato

// Mono 8 × 4, n = 2, offset 10, ε = 1/8, campioni diadici: master e piano esatti come l'oracle razionale di `.a`;
// report con i campi di `.a` (C-02.a §7, «Aggiunte di R2») uguali all'oracle e nessun campo per canale.
TEST(MasterB7, T_M3B_C02_14_mono_flat_master_and_report_are_those_of_a) {
  constexpr std::uint32_t w = 8, h = 4;
  group g = make_group(master_role::flat, w, h, 2);
  for (std::size_t i = 0; i < std::size_t{w} * h; ++i) {
    const double v = static_cast<double>((i * 3) % 11), d = static_cast<double>(1u << (i % 4));
    g.frames[0].v[i] = 10 + 1024 + v - d;
    g.frames[1].v[i] = 10 + 1024 + v + 2 * d;
  }
  g.frames[0].v[5] = g.frames[1].v[5] = 10 + 1030;  // piano nullo
  const double eps = 0.125;
  const master_recipe r = recipe_of(g, eps);
  const std::optional<master_result> m = combine_or_fail(r, g, "mono");
  ASSERT_TRUE(m.has_value());

  const std::vector<oracle_pixel> o = oracle(g);
  std::vector<double> planes, lbe, ratio;
  std::uint64_t ratio_null = 0;
  for (std::size_t i = 0; i < o.size(); ++i) {
    ASSERT_EQ(m->output.invalid.bits[i], 0) << i;
    const double M = sc::exact_double(o[i].mean, "M"), V = sc::exact_double(o[i].plane, "V");
    EXPECT_EQ(sample(m->output, i), M) << i;
    EXPECT_EQ(plane(m->output, i), V) << i;
    planes.push_back(V);
    const sc::rat a = o[i].mean - sc::q(10);
    lbe.push_back(sc::exact_double(sc::q(0) < a ? sc::q(1, 64) * a * a : sc::q(0), "level_bias_estimate"));
    if (V == 0) ++ratio_null;
    else ratio.push_back(lbe.back() / V);
  }
  ASSERT_EQ(ratio_null, 1u);

  const json& rep = m->report;
  const std::vector<std::size_t> order = canonical(r);
  std::vector<double> level(2);
  for (std::size_t k = 0; k < 2; ++k) level[k] = *level_where(g, order[k], [](std::uint32_t, std::uint32_t) { return true; });
  const double mean = (level[0] + level[1]) / 2;
  EXPECT_EQ(number_of(&rep, "flat_level_mean"), std::optional<double>(mean));
  for (std::size_t k = 0; k < 2; ++k) {
    const json* f = frame_of(rep, k);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(number_of(f, "flat_level"), std::optional<double>(level[k])) << k;
    EXPECT_EQ(number_of(f, "flat_level_rel_dev"), std::optional<double>(level[k] / mean - 1)) << k;
    EXPECT_EQ(f->find("flat_levels"), nullptr) << "campo per canale in un flat mono, frame " << k;
  }
  const json* channels = rep.find("channels");
  ASSERT_NE(channels, nullptr);
  ASSERT_EQ(channels->items().size(), 1u);
  const json* ch = labelled(channels, "all");
  ASSERT_NE(ch, nullptr) << "canale mono non etichettato `all`";
  EXPECT_EQ(ch->find("flat_level_mean"), nullptr) << "media per canale in un flat mono";
  EXPECT_EQ(number_of(ch, "valid_pixels"), std::optional<double>(32));
  EXPECT_EQ(number_of(ch, "variance_plane_median"), std::optional<double>(median_rule(planes)));
  const json* lb = ch->find("level_bias_estimate");
  ASSERT_NE(lb, nullptr);
  EXPECT_EQ(number_of(lb, "median"), std::optional<double>(median_rule(lbe)));
  EXPECT_EQ(number_of(lb, "max"), std::optional<double>(*std::max_element(lbe.begin(), lbe.end())));
  EXPECT_EQ(number_of(lb, "ratio_median"), std::optional<double>(median_rule(ratio)));
  EXPECT_EQ(number_of(lb, "ratio_max"), std::optional<double>(*std::max_element(ratio.begin(), ratio.end())));
  EXPECT_EQ(number_of(lb, "level_bias_ratio_null"), std::optional<double>(1));
  EXPECT_EQ(number_of(ch, "pixels_nonpositive_signal"), std::optional<double>(0));
}

// Mono, livelli 960 e 1088 (scarti ∓1/16 esatti): confine come in `.a` e contesto dello stop con le sole chiavi di
// `.a` (nessun canale).
TEST(MasterB7, T_M3B_C02_14_mono_level_tolerance_boundary_and_stop_context_are_those_of_a) {
  group g = make_group(master_role::flat, 4, 4, 2);
  for (double& v : g.frames[0].v) v = 10 + 960;
  for (double& v : g.frames[1].v) v = 10 + 1088;
  const master_recipe base = recipe_of(g, kTol);
  const double first_level = canonical(base)[0] == 0 ? 960 : 1088;
  EXPECT_TRUE(combine_or_fail(base, g, "uguale").has_value());
  master_recipe up = base;
  up.flat_level_rel_tol = std::nextafter(kTol, 1.0);
  EXPECT_TRUE(combine_or_fail(up, g, "successore").has_value());
  master_recipe down = base;
  down.flat_level_rel_tol = std::nextafter(kTol, 0.0);
  const std::optional<stop_error> e = stop_of(down, g);
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  EXPECT_EQ(e->category(), error_category::unsupported) << e->what();
  EXPECT_EQ(e->code(), "master.variance_domain") << e->what();
  EXPECT_EQ(ctx_keys(*e), (std::set<std::string>{"frame", "m_f", "m_media", "tolleranza"}));
  EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(0)));
  EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>(num(first_level)));
  EXPECT_EQ(ctx(*e, "m_media"), std::optional<std::string>("1024"));
  EXPECT_EQ(ctx(*e, "tolleranza"), std::optional<std::string>(num(std::nextafter(kTol, 0.0))));
}

// ================================================================ versione dell'algoritmo del nodo

// C-02.b §b.13 punto 4: il passaggio ai livelli per canale entra nella versione dell'algoritmo del nodo; B3 l'ha
// portata a 2, quindi B7 la porta almeno a 3 e i master con la versione precedente diventano `stale`.
TEST(MasterB7, T_M3B_C02_14_node_algorithm_version_includes_per_channel_flat_levels) {
  EXPECT_GE(master_algorithm_version, 3u);
  const group g = cfa_flat({"RGGB", 0, 0}, 4, 4, std::vector<std::array<double, 4>>(2, {1024, 1280, 1536, 2048}));
  const node_spec node = make_master_node(recipe_of(g, kTol));
  EXPECT_EQ(node.algorithm_version(), master_algorithm_version);
}

// ================================================================ riproducibilità

// Job a blocchi con worker e tile_rows diversi (37 × 71: tasselli parziali con 7 e 32 righe): master, piano, maschera
// e report (livelli per canale compresi) bit-identici fra le varianti e uguali al calcolo in memoria (C-02.b §b.8).
TEST(MasterB7, T_M3B_C02_14_cfa_flat_job_is_bit_identical_across_workers_and_tile_rows) {
  const fs::path dir = fresh_dir("b7-job-tiles");
  const fixture_flat fx = fixture_cfa_flat(37, 71, "GBRG");
  master_recipe r = recipe_of(fx.g, kTol);
  write_frames(fx.g, r, dir, "f");
  const std::optional<master_result> memory = combine_or_fail(r, fx.g, "memoria");
  ASSERT_TRUE(memory.has_value());
  ASSERT_TRUE(level_of(memory->report, 0, "G1").has_value()) << "livelli per canale assenti dal report";
  const std::string frames_text = canonical_json(cal::at(memory->report, {"frames"}));
  const std::string channels_text = canonical_json(cal::at(memory->report, {"channels"}));
  std::set<std::string> primary, report;
  struct variant {
    unsigned workers;
    std::uint32_t rows;
  };
  for (const variant v : {variant{1, 32}, variant{4, 1}, variant{4, 7}, variant{2, 5}}) {
    SCOPED_TRACE(std::to_string(v.workers) + " worker, " + std::to_string(v.rows) + " righe");
    const std::string tag = std::to_string(v.workers) + "-" + std::to_string(v.rows);
    auto store = sqlite_dag_store::create(dir / ("store-" + tag));
    const node_spec node = make_master_node(r);
    store->put_node(node);
    store->create_slot("master", node.key());
    master_job_options o;
    o.resources.worker_count = v.workers;
    o.resources.tile_rows = v.rows;
    memory_pool pool(256 * (std::uint64_t{1} << 20));
    const job_outcome out = run_master_quiet(*store, "master", r, pool, o);
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    const auto roles = by_role(*store, node.key());
    ASSERT_TRUE(roles.count("primary") && roles.count("report"));
    const image decoded = decode_fits(read_artifact(*store, roles.at("primary")), cal::big_budget);
    EXPECT_TRUE(same_bits(decoded, memory->output));
    const std::vector<std::uint8_t> bytes = read_artifact(*store, roles.at("report"));
    const json rep = parse_json(std::string(bytes.begin(), bytes.end()));
    EXPECT_EQ(canonical_json(cal::at(rep, {"frames"})), frames_text);
    EXPECT_EQ(canonical_json(cal::at(rep, {"channels"})), channels_text);
    primary.insert(roles.at("primary").sha256);
    report.insert(roles.at("report").sha256);
  }
  EXPECT_EQ(primary.size(), 1u);
  EXPECT_EQ(report.size(), 1u);
}

// ================================================================ T-M3B-MEM-01: righe del report per canale nel job

// minimum_feasible_bytes del job del master (ADR-06, estensione R2.r ed «Estensione M3.b del piano del master»), tile
// automatico (t_min = 1). Ingressi del modello presi dal test: P = picco di lettura del frame più grande
// (`plan_fits`), HB = tre HDU di header, M = margine del report della configurazione di default; K = 32, D = 2^20.
std::uint64_t expected_minimum(const group& g, const master_recipe& r, std::uint64_t cr) {
  const std::uint64_t n = g.frames.size(), W = g.w, H = g.h, S = W * H, E = sample_size(r.output_type);
  std::uint64_t P = 0;
  for (const mframe& f : g.frames) P = std::max(P, plan_fits(file_of(f, g.w, g.h)).peak_bytes);
  const std::uint64_t HB = 3 * (std::uint64_t{fits_limits::max_header_cards} * 80 + 2 * 2880);
  const std::uint64_t M = resource_config{}.report_margin_bytes.value, D = std::uint64_t{1} << 20, K = 32;
  const std::uint64_t out = S * (2 * E + 2);
  const std::uint64_t shared = P + 8 * S + out + K * S + (out + HB) + D + M;
  const std::uint64_t R = 16 * cr * H * n, CS = 120 * n + 168, Cn = 24 * n + 544;
  const std::uint64_t F = 56 * n + 8 * ((n + 7) / 8) + 544;
  return shared + R + CS + Cn + F + W * (10 * n + 8);
}

struct budget_run {
  std::optional<stop_error> stop;
  bool frames_read = false;
  bool primary = false;
  std::uint64_t pool_peak = 0;
};

budget_run run_with_budget(const fs::path& root, const master_recipe& r, std::uint64_t budget) {
  budget_run out;
  auto store = sqlite_dag_store::create(root);
  const node_spec node = make_master_node(r);
  store->put_node(node);
  store->create_slot("master", node.key());
  master_job_options o;
  o.hook = [&out](std::string_view point) {
    if (point == "frames.read") out.frames_read = true;
  };
  memory_pool pool(budget);
  try {
    const job_outcome res = run_master_quiet(*store, "master", r, pool, o);
    if (res.stop) out.stop = *res.stop;
  } catch (const stop_error& e) {
    out.stop = e;
  }
  out.pool_peak = pool.peak_reserved_bytes();
  out.primary = by_role(*store, node.key()).count("primary") != 0;
  return out;
}

// Un master CFA ha due canali per riga (Cr = 2, R = 16·Cr·H·n), il mono uno: con budget uguale al minimo del modello
// il job completa; con il minimo − 1 si ferma con `sched.job_exceeds_budget` e quel minimo nel contesto, prima di
// ogni lettura e prenotazione. Flat 16 × 12, n = 3: 16·H·n = 576 byte separano i due minimi.
TEST(MasterB7, T_M3B_MEM_01_job_minimum_counts_two_row_channels_for_cfa_and_one_for_mono) {
  const fs::path dir = fresh_dir("b7-mem01");
  for (const bool cfa : {true, false}) {
    SCOPED_TRACE(cfa ? "CFA, Cr = 2" : "mono, Cr = 1");
    group g = cfa_flat({"RGGB", 0, 0}, 16, 12, {{1024, 1280, 1536, 2048}, {1032, 1288, 1544, 2056}, {1016, 1272, 1528, 2040}});
    if (!cfa)
      for (mframe& f : g.frames) f.hdr.bayer.clear();
    master_recipe r = recipe_of(g, kTol);
    const std::string tag = cfa ? "cfa" : "mono";
    write_frames(g, r, dir, tag);
    const std::uint64_t minimum = expected_minimum(g, r, cfa ? 2 : 1);
    ASSERT_EQ(expected_minimum(g, r, 2) - expected_minimum(g, r, 1), 16u * 12 * 3);

    const budget_run below = run_with_budget(dir / (tag + "-below"), r, minimum - 1);
    if (!below.stop) {
      // Diagnosi: il minimo che il job dichiara con un budget di un byte.
      const budget_run probe = run_with_budget(dir / (tag + "-probe"), r, 1);
      ADD_FAILURE() << "minimo − 1 ammesso; atteso " << minimum << ", il job dichiara "
                    << (probe.stop ? ctx(*probe.stop, "minimum_feasible_bytes").value_or("?") : "nessuno stop");
    } else {
      EXPECT_EQ(below.stop->category(), error_category::budget) << below.stop->what();
      EXPECT_EQ(below.stop->code(), "sched.job_exceeds_budget") << below.stop->what();
      EXPECT_EQ(ctx(*below.stop, "minimum_feasible_bytes"), std::optional<std::string>(std::to_string(minimum)));
    }
    EXPECT_FALSE(below.frames_read);
    EXPECT_EQ(below.pool_peak, 0u);
    EXPECT_FALSE(below.primary);

    const budget_run at = run_with_budget(dir / (tag + "-at"), r, minimum);
    EXPECT_FALSE(at.stop.has_value()) << (at.stop ? at.stop->code() + " — " + at.stop->what() : "");
    EXPECT_TRUE(at.frames_read);
    EXPECT_TRUE(at.primary);
  }
}

}  // namespace
