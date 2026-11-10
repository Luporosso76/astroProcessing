// M3.b, unità B8: `median_ratio`, parte 1 — parametri, costanti delle scale, preflight (C-02.b-flat §f.1 regola, §f.2
// tabella dei metodi, parametri e controlli 1-4, §f.10, §f.13 punti 1-2, 4-5, 8; decisione L11 in
// docs/decisions/M3b-lacune.md).
// Test: T-M3B-FLAT-06 parte stop (caselle della tabella di §f.2 con `median_ratio`; parametri assenti, vietati, fuori
// dominio; `sigma_clip` con soglie diverse; `max m` al successore f64 di `4 min m`; budget al predecessore f64 e
// confronto esatto; `flat_min_signal_adu` e livello subnormali; nessun calcolo e nessun lavoro sullo store negli stop
// del job), T-M3B-FLAT-01 parte costanti (m_H e ρ dyadic).
// Fuori da questo file: i casi ammessi al confine (rapporto uguale a 4, A · ρ = budget · m_H con la combinazione
// eseguita) e i valori della combinazione sono di B9.
//
// Oracle scritti qui, nessun valore atteso dal prodotto. Ogni frame ha un solo livello per canale (tutti i pixel del
// canale a offset + livello), quindi m_(f,c) è il livello scritto; costanti in f64 secondo §f.1:
//   m_H,c = n / Σ_f (1 / m_(f,c)) con la somma nell'ordine canonico, r_(f,c) = m_H,c / m_(f,c), ρ_c = max_f |r_(f,c) − 1|.
// I casi numerici sono scelti in modo che ogni ordine canonico dia lo stesso valore f64 (derivazione nel commento del
// test). Tolleranza 0 ovunque (classe B per gli stop).
//
// Contesti degli stop che il contratto non nomina, decisi dal coordinatore (docs/decisions/M3b-lacune.md, B8):
// - `master.variance_domain` di §f.2 (tabella e controllo 3): il contesto esistente del codice (combine, rejection, n,
//   role, motivo) più `canale` per un flat CFA, come lo stop della tolleranza di B7;
// - `master.flat_norm_budget`: `A`, `rho`, `m_H`, `budget` (i quattro valori che §f.2 elenca, in `%.17g`) più `canale`
//   per un flat CFA; nel mono nessun `canale`, come gli altri stop per canale di B7;
// - `master.flat_scale_nonfinite`: `costante` ∈ {1/m_f, r_f, somma, m_H}; per 1/m_f e r_f anche `frame`, `m_f`; per
//   somma e m_H anche `valore`; `canale` per un flat CFA;
// - `master.flat_low_signal`: il contesto di `.a` (frame, m_f, minimo; solo frame se nessun pixel è valido) più `canale` per un flat CFA;
// - `master.param.<nome>`: il contesto esistente (parametro, motivo);
// - `master.method_not_computable` (temporaneo, B9 lo toglie): {flat_normalization: median_ratio}.
#include <gtest/gtest.h>

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

namespace {

using namespace mst;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kMinSub = 0x1p-1074;  // il più piccolo subnormale positivo

// ---------------------------------------------------------------- metodi e ricetta

enum class kind { mean, weighted, median, winsorized, sigma_clip };
constexpr std::array<kind, 5> kAllMethods = {kind::mean, kind::weighted, kind::median, kind::winsorized, kind::sigma_clip};
constexpr std::array<kind, 4> kBudgetMethods = {kind::weighted, kind::median, kind::winsorized, kind::sigma_clip};
constexpr std::array<kind, 3> kRatioMethods = {kind::median, kind::winsorized, kind::sigma_clip};

const char* name_of(kind k) {
  switch (k) {
    case kind::mean: return "mean";
    case kind::weighted: return "weighted_mean";
    case kind::median: return "median";
    case kind::winsorized: return "winsorized";
    case kind::sigma_clip: return "mean + sigma_clip";
  }
  return "";
}

// Nome del metodo e della rejection nel contesto di `master.variance_domain`.
const char* combine_text(kind k) {
  switch (k) {
    case kind::mean:
    case kind::sigma_clip: return "mean";
    case kind::weighted: return "weighted_mean";
    case kind::median: return "median";
    case kind::winsorized: return "winsorized";
  }
  return "";
}

bool needs_budget(kind k) { return k != kind::mean; }

// Parametri del metodo: pesi uguali (2 max w < W per n ≥ 3), g = 1 (n ≥ 3), `sigma_clip` con κ = 1 su entrambi i lati,
// non inerte per n ≥ 3 ((n − 1)² > κ² n), una iterazione, almeno 2 tenuti.
void apply_method(master_recipe& r, kind k, double kappa_low = 1, double kappa_high = 1) {
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  switch (k) {
    case kind::mean: break;
    case kind::weighted:
      r.combine = combine_method::weighted_mean;
      for (master_frame_spec& f : r.frames) f.weight = 1;
      break;
    case kind::median: r.combine = combine_method::median; break;
    case kind::winsorized:
      r.combine = combine_method::winsorized;
      r.winsor_g = 1;
      break;
    case kind::sigma_clip:
      r.rejection = master_rejection::sigma_clip;
      r.clip_kappa_low = kappa_low;
      r.clip_kappa_high = kappa_high;
      r.clip_max_iter = 1;
      r.clip_min_kept = 2;
      break;
  }
}

// Ricetta valida con `median_ratio` (§f.2): A dichiarato, budget solo per i metodi diversi da `mean`, nessuna
// tolleranza dei livelli, segnale minimo 1 ADU (recipe_of).
master_recipe ratio_recipe(const group& g, kind k, double additive_bound = 0, double budget = 0.5) {
  master_recipe r = recipe_of(g);
  r.flat_norm = flat_normalization::median_ratio;
  r.flat_level_rel_tol.reset();
  apply_method(r, k);
  set_flat_additive_bound(r, additive_bound);
  if (needs_budget(k)) set_flat_norm_budget(r, budget);
  return r;
}

// Flat mono: ogni pixel del frame f vale offset + levels[f].
group level_flat(const std::vector<double>& levels, double offset = 10, std::uint32_t w = 4, std::uint32_t h = 4) {
  group g = make_group(master_role::flat, w, h, levels.size());
  for (std::size_t f = 0; f < levels.size(); ++f) {
    g.frames[f].hdr.offset = offset;
    for (double& v : g.frames[f].v) v = offset + levels[f];
  }
  return g;
}

// Flat CFA RGGB senza offset di motivo: posizione k = 2 (y mod 2) + (x mod 2) → R, G1, G2, B; levels[f][k].
group rggb_flat(const std::vector<std::array<double, 4>>& levels, double offset = 10) {
  group g = make_group(master_role::flat, 4, 4, levels.size());
  for (std::size_t f = 0; f < levels.size(); ++f) {
    mframe& fr = g.frames[f];
    fr.hdr.bayer = "RGGB";
    fr.hdr.offset = offset;
    for (std::uint32_t y = 0; y < g.h; ++y)
      for (std::uint32_t x = 0; x < g.w; ++x) fr.v[std::size_t{y} * g.w + x] = offset + levels[f][2 * (y & 1u) + (x & 1u)];
  }
  return g;
}

// ---------------------------------------------------------------- stop

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

std::string num(double v) {
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", v);
  return b;
}

std::optional<stop_error> stop_of(const master_recipe& r, const group& g) {
  try {
    (void)combine_master(r, images_of(g));
  } catch (const stop_error& e) {
    return e;
  }
  return std::nullopt;
}

std::optional<stop_error> validation_stop(const master_recipe& r) {
  try {
    validate_master_recipe(r);
  } catch (const stop_error& e) {
    return e;
  }
  return std::nullopt;
}

// Posizione canonica del frame `generator_index` (C-02.a §8: digest SHA-256 crescente) e nome nel contesto.
std::size_t canonical_position(const master_recipe& r, std::size_t generator_index) {
  std::vector<std::size_t> order(r.frames.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return r.frames[a].digest.sha256 < r.frames[b].digest.sha256; });
  return static_cast<std::size_t>(std::find(order.begin(), order.end(), generator_index) - order.begin());
}

std::string frame_label(std::size_t canonical_pos) {
  char b[32];
  std::snprintf(b, sizeof b, "frame_%03zu", canonical_pos);
  return b;
}

// Stop di parametro: validazione e nodo (nessun nodo costruito) con lo stesso codice; contesto {parametro, motivo}.
void expect_param_stop(const master_recipe& r, const std::string& name) {
  const std::string code = "master.param." + name;
  const std::optional<stop_error> e = validation_stop(r);
  ASSERT_TRUE(e.has_value()) << "nessuno stop, atteso " << code;
  EXPECT_EQ(e->category(), error_category::invalid_input) << e->what();
  EXPECT_EQ(e->code(), code) << e->what();
  EXPECT_EQ(ctx_keys(*e), (std::set<std::string>{"motivo", "parametro"})) << e->what();
  EXPECT_EQ(ctx(*e, "parametro"), std::optional<std::string>(name));
  EXPECT_STOP(make_master_node(r), error_category::invalid_input, code);
}

// Stop `master.variance_domain` col contesto esistente (combine, rejection, n, role, motivo), più `canale` se dato.
void expect_variance_domain(const stop_error& e, const std::string& combine, const std::string& rejection, std::size_t n,
                            const std::optional<std::string>& canale = std::nullopt) {
  EXPECT_EQ(e.category(), error_category::unsupported) << e.what();
  EXPECT_EQ(e.code(), "master.variance_domain") << e.what();
  std::set<std::string> keys = {"combine", "motivo", "n", "rejection", "role"};
  if (canale) keys.insert("canale");
  EXPECT_EQ(ctx_keys(e), keys) << e.what();
  EXPECT_EQ(ctx(e, "combine"), std::optional<std::string>(combine));
  EXPECT_EQ(ctx(e, "rejection"), std::optional<std::string>(rejection));
  EXPECT_EQ(ctx(e, "n"), std::optional<std::string>(std::to_string(n)));
  EXPECT_EQ(ctx(e, "role"), std::optional<std::string>("flat"));
  if (canale) {
    EXPECT_EQ(ctx(e, "canale"), canale);
  }
}

void expect_budget_stop(const stop_error& e, double a, double rho, double m_h, double budget,
                        const std::optional<std::string>& canale = std::nullopt) {
  EXPECT_EQ(e.category(), error_category::unsupported) << e.what();
  EXPECT_EQ(e.code(), "master.flat_norm_budget") << e.what();
  std::set<std::string> keys = {"A", "budget", "m_H", "rho"};
  if (canale) keys.insert("canale");
  EXPECT_EQ(ctx_keys(e), keys) << e.what();
  EXPECT_EQ(ctx(e, "A"), std::optional<std::string>(num(a)));
  EXPECT_EQ(ctx(e, "rho"), std::optional<std::string>(num(rho)));
  EXPECT_EQ(ctx(e, "m_H"), std::optional<std::string>(num(m_h)));
  EXPECT_EQ(ctx(e, "budget"), std::optional<std::string>(num(budget)));
  if (canale) {
    EXPECT_EQ(ctx(e, "canale"), canale);
  }
}

// ================================================================ T-M3B-FLAT-06: tabella di §f.2, colonna median_ratio

// §f.2, §f.13 punti 1 e 5: con `median_ratio` sono ammessi `mean`, `weighted_mean`, `median`, `winsorized` e
// `mean` + `sigma_clip` con soglie uguali; la validazione e il nodo non si fermano (i valori sono di B9).
TEST(MasterB8, T_M3B_FLAT_06_median_ratio_admits_every_method_of_the_f2_table_in_validation_and_node) {
  const group g = level_flat({1024, 1024, 1024, 1024});
  for (const kind k : kAllMethods) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k);
    const std::optional<stop_error> e = validation_stop(r);
    EXPECT_FALSE(e.has_value()) << (e ? e->code() + " — " + e->what() : "");
    EXPECT_NO_THROW((void)make_master_node(r));
  }
}

// §f.2: `single` con `median_ratio` → `unsupported`, `master.variance_domain`. Ricetta altrimenti valida (A dichiarato,
// nessuna tolleranza, modello di rumore di `single`): l'unica violazione è la casella della tabella.
TEST(MasterB8, T_M3B_FLAT_06_single_with_median_ratio_stops_with_variance_domain) {
  const group g = level_flat({1024});
  master_recipe r = recipe_of(g);
  r.flat_norm = flat_normalization::median_ratio;
  r.flat_level_rel_tol.reset();
  r.combine = combine_method::single;
  r.offset_adu = 10;
  r.conversion_gain_e_per_adu = 1.5;
  r.read_noise_adu = 2;
  set_flat_additive_bound(r, 0.0);
  const std::optional<stop_error> e = validation_stop(r);
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  expect_variance_domain(*e, "single", "none", 1);
  EXPECT_STOP(make_master_node(r), error_category::unsupported, "master.variance_domain");
}

// §f.2: `sigma_clip` sul flat con `median_ratio` solo con `clip_kappa_low = clip_kappa_high`. κ = (1, 5/4) con n = 4:
// entrambe le soglie non inerti ((n − 1)² = 9 > κ² n = 4 e 6.25), quindi l'unica violazione è l'asimmetria.
TEST(MasterB8, T_M3B_FLAT_06_sigma_clip_with_unequal_thresholds_on_the_flat_stops_with_variance_domain) {
  const group g = level_flat({1024, 1024, 1024, 1024});
  for (const auto& [lo, hi] : {std::pair{1.0, 1.25}, std::pair{1.25, 1.0}}) {
    SCOPED_TRACE("kappa " + num(lo) + ", " + num(hi));
    master_recipe r = ratio_recipe(g, kind::sigma_clip);
    r.clip_kappa_low = lo;
    r.clip_kappa_high = hi;
    const std::optional<stop_error> e = validation_stop(r);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_variance_domain(*e, "mean", "sigma_clip", 4);
    EXPECT_STOP(make_master_node(r), error_category::unsupported, "master.variance_domain");
  }
}

// ================================================================ T-M3B-FLAT-06: parametri di §f.2

// `flat_additive_bound_adu` ≥ 0 obbligatorio per ogni metodo: assente, negativo (anche il subnormale negativo), NaN,
// ±inf → `invalid_input`, `master.param.flat_additive_bound_adu`. Confine: 0 ammesso (casi del test di ammissione) e
// anche −0.0 (−0.0 ≥ 0 in IEEE 754).
TEST(MasterB8, T_M3B_FLAT_06_additive_bound_is_required_finite_and_nonnegative_for_every_method) {
  const group g = level_flat({1024, 1024, 1024, 1024});
  for (const kind k : kAllMethods) {
    for (const std::optional<double> a : {std::optional<double>{}, std::optional<double>{-kMinSub},
                                          std::optional<double>{-1.0}, std::optional<double>{kNaN},
                                          std::optional<double>{kInf}, std::optional<double>{-kInf}}) {
      SCOPED_TRACE(std::string(name_of(k)) + ", A = " + (a ? num(*a) : "assente"));
      master_recipe r = ratio_recipe(g, k);
      set_flat_additive_bound(r, a);
      expect_param_stop(r, "flat_additive_bound_adu");
    }
    SCOPED_TRACE(std::string(name_of(k)) + ", A = -0 ammesso");
    master_recipe r = ratio_recipe(g, k);
    set_flat_additive_bound(r, -0.0);
    const std::optional<stop_error> e = validation_stop(r);
    EXPECT_FALSE(e.has_value()) << (e ? e->code() + " — " + e->what() : "");
  }
}

// `flat_norm_rel_budget` in (0, 1): obbligatorio per `weighted_mean`, `median`, `winsorized`, `sigma_clip`; assente,
// 0, 1, negativo, NaN, inf → `master.param.flat_norm_rel_budget`. Confini interni ammessi: il più piccolo subnormale
// positivo e il predecessore f64 di 1.
TEST(MasterB8, T_M3B_FLAT_06_norm_budget_is_required_in_the_open_unit_interval_for_every_method_but_mean) {
  const group g = level_flat({1024, 1024, 1024, 1024});
  for (const kind k : kBudgetMethods) {
    for (const std::optional<double> b :
         {std::optional<double>{}, std::optional<double>{0.0}, std::optional<double>{-0.0}, std::optional<double>{1.0},
          std::optional<double>{-0.5}, std::optional<double>{kNaN}, std::optional<double>{kInf}}) {
      SCOPED_TRACE(std::string(name_of(k)) + ", budget = " + (b ? num(*b) : "assente"));
      master_recipe r = ratio_recipe(g, k);
      set_flat_norm_budget(r, b);
      expect_param_stop(r, "flat_norm_rel_budget");
    }
    for (const double b : {kMinSub, std::nextafter(1.0, 0.0)}) {
      SCOPED_TRACE(std::string(name_of(k)) + ", budget = " + num(b) + " ammesso");
      master_recipe r = ratio_recipe(g, k);
      set_flat_norm_budget(r, b);
      const std::optional<stop_error> e = validation_stop(r);
      EXPECT_FALSE(e.has_value()) << (e ? e->code() + " — " + e->what() : "");
    }
  }
}

// §f.2: il budget è vietato con `mean` (fatto 1), anche con un valore nel dominio.
TEST(MasterB8, T_M3B_FLAT_06_norm_budget_is_forbidden_with_mean) {
  const group g = level_flat({1024, 1024});
  master_recipe r = ratio_recipe(g, kind::mean);
  set_flat_norm_budget(r, 0.5);
  expect_param_stop(r, "flat_norm_rel_budget");
}

// §f.2, §f.13 punto 2, L11: `flat_level_rel_tol` vietato con `median_ratio`, per ogni metodo.
TEST(MasterB8, T_M3B_FLAT_06_level_tolerance_is_forbidden_with_median_ratio) {
  const group g = level_flat({1024, 1024, 1024, 1024});
  for (const kind k : kAllMethods) {
    SCOPED_TRACE(name_of(k));
    master_recipe r = ratio_recipe(g, k);
    r.flat_level_rel_tol = 0.25;
    expect_param_stop(r, "flat_level_rel_tol");
  }
}

// §f.2: `flat_min_signal_adu` obbligatorio come in `.a` (> 0): assente o 0 → stop; il subnormale positivo è nel
// dominio e la validazione lo ammette (il suo effetto sulle costanti è nel test di `master.flat_scale_nonfinite`).
TEST(MasterB8, T_M3B_FLAT_06_min_signal_is_still_required_with_median_ratio) {
  const group g = level_flat({1024, 1024, 1024, 1024});
  for (const kind k : kAllMethods) {
    SCOPED_TRACE(name_of(k));
    for (const std::optional<double> s : {std::optional<double>{}, std::optional<double>{0.0}}) {
      SCOPED_TRACE(s ? "0" : "assente");
      master_recipe r = ratio_recipe(g, k);
      r.flat_min_signal_adu = s;
      expect_param_stop(r, "flat_min_signal_adu");
    }
    master_recipe r = ratio_recipe(g, k);
    r.flat_min_signal_adu = kMinSub;
    const std::optional<stop_error> e = validation_stop(r);
    EXPECT_FALSE(e.has_value()) << (e ? e->code() + " — " + e->what() : "");
  }
}

// §f.2: con `none` A e budget sono vietati (la tolleranza resta obbligatoria); su un ruolo diverso dal flat i
// parametri del flat sono vietati come gli altri.
TEST(MasterB8, T_M3B_FLAT_06_additive_bound_and_budget_are_forbidden_with_none_and_on_other_roles) {
  const group flat = level_flat({1024, 1024});
  {
    SCOPED_TRACE("none + A");
    master_recipe r = recipe_of(flat);
    set_flat_additive_bound(r, 0.0);
    expect_param_stop(r, "flat_additive_bound_adu");
  }
  {
    SCOPED_TRACE("none + budget");
    master_recipe r = recipe_of(flat);
    set_flat_norm_budget(r, 0.5);
    expect_param_stop(r, "flat_norm_rel_budget");
  }
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    group g = make_group(role, 4, 4, 2);
    for (mframe& f : g.frames)
      for (double& v : f.v) v = 500;
    {
      SCOPED_TRACE(std::string(imagetyp_of(role)) + " + A");
      master_recipe r = recipe_of(g);
      set_flat_additive_bound(r, 0.0);
      expect_param_stop(r, "flat_additive_bound_adu");
    }
    {
      SCOPED_TRACE(std::string(imagetyp_of(role)) + " + budget");
      master_recipe r = recipe_of(g);
      set_flat_norm_budget(r, 0.5);
      expect_param_stop(r, "flat_norm_rel_budget");
    }
  }
}

// ================================================================ T-M3B-FLAT-06: controlli del preflight (§f.2)

// Controllo 3: per mediana, winsorized e `sigma_clip`, `max m ≤ 4 min m` confrontati in modo esatto sui valori f64.
// Livelli (1024, 2048, succ(4096)), succ(4096) = 4096 + 2⁻⁴⁰ (ulp di [4096, 8192)); offset 10: 4106 + 2⁻⁴⁰ è
// rappresentabile e x − offset è esatto. A = 0: il controllo 4 passa (A · ρ = 0), l'unica violazione è il rapporto.
TEST(MasterB8, T_M3B_FLAT_06_level_ratio_at_the_f64_successor_of_four_stops_the_robust_methods) {
  const double above = std::nextafter(4096.0, 8192.0);
  ASSERT_EQ(above, 4096.0 + 0x1p-40);
  const group g = level_flat({1024, 2048, above});
  for (const kind k : kRatioMethods) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k, 0.0);
    const std::optional<stop_error> e = stop_of(r, g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_variance_domain(*e, combine_text(k), k == kind::sigma_clip ? "sigma_clip" : "none", 3);
  }
}

// Il controllo 3 è per canale: canale B di RGGB a (1024, 2048, succ(4096)), gli altri tre canali a 1024 in ogni frame.
// Il livello del frame su tutti i pixel resta entro il rapporto: lo stop dimostra il controllo per canale.
TEST(MasterB8, T_M3B_FLAT_06_level_ratio_beyond_four_in_one_cfa_channel_stops_with_that_channel) {
  const double above = std::nextafter(4096.0, 8192.0);
  const group g = rggb_flat({{1024, 1024, 1024, 1024}, {1024, 1024, 1024, 2048}, {1024, 1024, 1024, above}});
  for (const kind k : kRatioMethods) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k, 0.0);
    const std::optional<stop_error> e = stop_of(r, g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_variance_domain(*e, combine_text(k), k == kind::sigma_clip ? "sigma_clip" : "none", 3, "B");
  }
}

// Controllo 4: A · ρ ≤ budget · m_H confrontati in modo esatto. Livelli (1, 3, 3/2, 3/2), n = 4:
//   Σ 1/m (8/3 in aritmetica esatta) vale 0x1.5555555555555p+1 = 8/3 − 1/(3 · 2⁵¹) in ogni ordine delle dodici
//   permutazioni (somma f64 da sinistra), e m_H = fl(4 / Σ) = 3/2 esatto; r = (3/2, 1/2, 1, 1); ρ = 1/2.
// Con A = 3/2: A · ρ = 3/4 = budget · m_H per budget = 1/2 (ammesso, B9). Budget = pred(1/2) = 1/2 − 2⁻⁵⁴:
// budget · m_H = 3/4 − 3 · 2⁻⁵⁵ < 3/4 → stop, con A, ρ, m_H e budget nel contesto.
TEST(MasterB8, T_M3B_FLAT_06_budget_at_the_f64_predecessor_stops_every_method_but_mean) {
  const double budget = std::nextafter(0.5, 0.0);
  const group g = level_flat({1, 3, 1.5, 1.5}, 512);
  for (const kind k : kBudgetMethods) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k, 1.5, budget);
    const std::optional<stop_error> e = stop_of(r, g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_budget_stop(*e, 1.5, 0.5, 1.5, budget);
  }
}

// Il confronto del controllo 4 è esatto, non sul prodotto arrotondato. Stessi livelli (m_H = 3/2, ρ = 1/2);
// budget b = 0x1.ffffffffffffdp-2 (= 1/2 − 3 · 2⁻⁵⁴): b · 3/2 = 3/4 − 9 · 2⁻⁵⁵ esatto, che in f64 arrotonda per eccesso a
// 0x1.7fffffffffffep-1 = 3/4 − 2⁻⁵². A = 0x1.7fffffffffffep+0 dà A · ρ = 3/4 − 2⁻⁵² (esatto): uguale al prodotto
// arrotondato (un confronto f64 ammetterebbe), maggiore di quello esatto di 2⁻⁵⁵ → stop.
TEST(MasterB8, T_M3B_FLAT_06_budget_comparison_is_exact_not_on_the_rounded_product) {
  const double b = 0x1.ffffffffffffdp-2, a = 0x1.7fffffffffffep+0;
  ASSERT_EQ(b * 1.5, 0x1.7fffffffffffep-1) << "premessa: prodotto f64 arrotondato per eccesso";
  ASSERT_EQ(a * 0.5, b * 1.5) << "premessa: A · ρ uguale al prodotto arrotondato";
  const group g = level_flat({1, 3, 1.5, 1.5}, 512);
  for (const kind k : kBudgetMethods) {
    SCOPED_TRACE(name_of(k));
    const master_recipe r = ratio_recipe(g, k, a, b);
    const std::optional<stop_error> e = stop_of(r, g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_budget_stop(*e, a, 0.5, 1.5, b);
  }
}

// Controllo 2: con `flat_min_signal_adu` = 2⁻¹⁰⁷⁴ (subnormale, nel dominio > 0) un livello subnormale 2⁻¹⁰⁷⁰ supera il
// controllo 1 (2⁻¹⁰⁷⁰ ≥ 2⁻¹⁰⁷⁴) ma 1/m = 2¹⁰⁷⁰ non è finito → `numeric`, `master.flat_scale_nonfinite`. Offset 0, così
// x − offset = x è esatto. L'altro frame ha livello 1. Si usa `mean` (nessun controllo 3 o 4 dopo).
TEST(MasterB8, T_M3B_FLAT_06_subnormal_min_signal_and_subnormal_level_stop_with_scale_nonfinite) {
  const double tiny = 0x1p-1070;
  ASSERT_LT(tiny, std::numeric_limits<double>::min());
  ASSERT_TRUE(std::isinf(1.0 / tiny));
  const group g = level_flat({tiny, 1.0}, 0.0);
  master_recipe r = ratio_recipe(g, kind::mean);
  r.flat_min_signal_adu = kMinSub;
  const std::optional<stop_error> e = stop_of(r, g);
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  EXPECT_EQ(e->category(), error_category::numeric) << e->what();
  EXPECT_EQ(e->code(), "master.flat_scale_nonfinite") << e->what();
  EXPECT_EQ(ctx_keys(*e), (std::set<std::string>{"costante", "frame", "m_f"})) << e->what();
  EXPECT_EQ(ctx(*e, "costante"), std::optional<std::string>("1/m_f"));
  EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(canonical_position(r, 0))));
  EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>(num(tiny)));
}

// Controllo 2, costante non normale: livello 1.5 · 2¹⁰²², 1/m = 2⁻¹⁰²² / 1.5 subnormale; r_f = m_H / m_f è subnormale per
// lo stesso frame. Il livello resta finito anche con la mediana a conteggio pari dei 16 pixel ((z_k + z_(k+1)) / 2 =
// 1.5 · 2¹⁰²³ prima della divisione); l'altro frame ha livello 1 e la media dei livelli resta finita. Offset 0.
TEST(MasterB8, T_M3B_FLAT_06_level_with_a_subnormal_reciprocal_stops_with_scale_nonfinite) {
  const double big = 1.5 * 0x1p1022;
  ASSERT_TRUE(std::isfinite(big + big));
  ASSERT_LT(1.0 / big, std::numeric_limits<double>::min());
  ASSERT_GT(1.0 / big, 0.0);
  const group g = level_flat({1.0, big}, 0.0);
  const master_recipe r = ratio_recipe(g, kind::mean);
  const std::optional<stop_error> e = stop_of(r, g);
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  EXPECT_EQ(e->category(), error_category::numeric) << e->what();
  EXPECT_EQ(e->code(), "master.flat_scale_nonfinite") << e->what();
  EXPECT_EQ(ctx_keys(*e), (std::set<std::string>{"costante", "frame", "m_f"})) << e->what();
  EXPECT_EQ(ctx(*e, "costante"), std::optional<std::string>("1/m_f"));
  EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(canonical_position(r, 1))));
  EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>(num(big)));
}

// Controllo 1 invariato con `median_ratio`: un livello subnormale sotto un `flat_min_signal_adu` normale →
// `invalid_input`, `master.flat_low_signal` con il contesto di `.a` (frame, m_f, minimo).
TEST(MasterB8, T_M3B_FLAT_06_subnormal_level_below_a_normal_min_signal_stops_with_low_signal) {
  const double tiny = 0x1p-1070;
  const group g = level_flat({tiny, 1.0}, 0.0);
  const master_recipe r = ratio_recipe(g, kind::mean);
  const std::optional<stop_error> e = stop_of(r, g);
  ASSERT_TRUE(e.has_value()) << "nessuno stop";
  EXPECT_EQ(e->category(), error_category::invalid_input) << e->what();
  EXPECT_EQ(e->code(), "master.flat_low_signal") << e->what();
  EXPECT_EQ(ctx_keys(*e), (std::set<std::string>{"frame", "m_f", "minimo"})) << e->what();
  EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(canonical_position(r, 0))));
  EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>(num(tiny)));
  EXPECT_EQ(ctx(*e, "minimo"), std::optional<std::string>("1"));
}

// ================================================================ T-M3B-FLAT-01: costanti dyadic

// Livelli (1, 3) → m_H = 2 / (1 + fl(1/3)) = 3/2 in f64, r = (3/2, 1/2), ρ = 1/2. Le costanti si leggono dal contesto
// dello stop del budget, che serve un metodo con n ≥ 3: si aggiungono due frame a 3/2, che lasciano m_H = 3/2 in ogni
// ordine canonico (Σ 1/m = 0x1.5555555555555p+1 e m_H = fl(4 / Σ) = 3/2 esatto, vedi il test del predecessore) e
// hanno r = 1. Mono e un canale di un flat CFA (G2,
// gli altri canali a livello 8 in ogni frame: ρ = 0, controllo 4 soddisfatto con qualunque A).
TEST(MasterB8, T_M3B_FLAT_01_levels_one_and_three_give_dyadic_harmonic_mean_and_rho) {
  ASSERT_EQ(2.0 / (1.0 + 1.0 / 3.0), 1.5) << "premessa: m_H dei livelli (1, 3)";
  const double budget = std::nextafter(0.5, 0.0);
  {
    SCOPED_TRACE("mono");
    const group g = level_flat({1, 3, 1.5, 1.5}, 512);
    const std::optional<stop_error> e = stop_of(ratio_recipe(g, kind::median, 1.5, budget), g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_budget_stop(*e, 1.5, 0.5, 1.5, budget);
  }
  {
    SCOPED_TRACE("CFA, canale G2");
    const group g = rggb_flat({{8, 8, 1, 8}, {8, 8, 3, 8}, {8, 8, 1.5, 8}, {8, 8, 1.5, 8}}, 512);
    const std::optional<stop_error> e = stop_of(ratio_recipe(g, kind::median, 1.5, budget), g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_budget_stop(*e, 1.5, 0.5, 1.5, budget, "G2");
  }
}

// ================================================================ T-M3B-FLAT-06: nessun calcolo negli stop del job

struct job_probe {
  std::optional<std::string> setup_error;
  std::optional<std::string> code;
  std::optional<error_category> category;
  int faults = 0;
  bool frames_read = false;
  bool primary = false;
  std::optional<stop_error> stop;
};

// Progetto col nodo della ricetta e job con la stessa ricetta. Il contatore del fault_injector conta ogni punto di
// protocollo dello store dopo l'allestimento; "frames.read" scatta dopo il preflight, prima di ogni task.
job_probe probe_job(const fs::path& root, const master_recipe& r) {
  job_probe out;
  auto store = sqlite_dag_store::create(root);
  std::optional<node_spec> node;
  try {
    node = make_master_node(r);
  } catch (const stop_error& e) {
    out.setup_error = e.code() + " — " + e.what();
    return out;
  }
  store->put_node(*node);
  store->create_slot("master", node->key());
  int faults = 0;
  store->set_fault_injector([&faults](std::string_view) {
    ++faults;
    return 0;
  });
  master_job_options o;
  o.hook = [&out](std::string_view point) {
    if (point == "frames.read") out.frames_read = true;
  };
  memory_pool pool(256 * (std::uint64_t{1} << 20));
  try {
    const job_outcome res = run_master_quiet(*store, "master", r, pool, o);
    if (res.stop) {
      out.stop = *res.stop;
      out.code = res.stop->code();
      out.category = res.stop->category();
    }
  } catch (const stop_error& e) {
    out.stop = e;
    out.code = e.code();
    out.category = e.category();
  }
  out.faults = faults;
  store->set_fault_injector({});
  out.primary = by_role(*store, node->key()).count("primary") != 0;
  return out;
}

// Ogni stop del preflight di `median_ratio` nel job: nessun task (frames.read non raggiunto), nessun artefatto, nessun
// lavoro sullo store oltre quello di uno stop del preflight di riferimento sugli stessi frame (`none` con segnale
// minimo oltre il livello: `master.flat_low_signal`). Il controllo (`none` valido) prova che il contatore è collegato.
TEST(MasterB8, T_M3B_FLAT_06_preflight_stops_in_the_job_run_no_task_and_do_no_extra_store_work) {
  const fs::path dir = fresh_dir("b8-job-stops");
  struct job_case {
    std::string name;
    group g;
    kind k;
    double a, budget;
    std::optional<double> min_signal;
    error_category category;
    std::string code;
  };
  const std::vector<job_case> cases = {
      {"rapporto", level_flat({1024, 2048, std::nextafter(4096.0, 8192.0)}), kind::median, 0.0, 0.5, std::nullopt,
       error_category::unsupported, "master.variance_domain"},
      {"budget", level_flat({1, 3, 1.5, 1.5}, 512), kind::median, 1.5, std::nextafter(0.5, 0.0), std::nullopt,
       error_category::unsupported, "master.flat_norm_budget"},
      {"scale", level_flat({0x1p-1070, 1.0}, 0.0), kind::mean, 0.0, 0.5, kMinSub, error_category::numeric,
       "master.flat_scale_nonfinite"},
  };
  {
    SCOPED_TRACE("controllo: none valido");
    const group g = level_flat({1024, 1024});
    master_recipe ok = recipe_of(g);
    write_frames(g, ok, dir, "control");
    master_recipe low = ok;
    low.flat_min_signal_adu = 1e9;
    const job_probe base = probe_job(dir / "store-control-base", low);
    const job_probe control = probe_job(dir / "store-control", ok);
    ASSERT_FALSE(base.setup_error.has_value()) << *base.setup_error;
    ASSERT_FALSE(control.setup_error.has_value()) << *control.setup_error;
    ASSERT_EQ(base.code, std::optional<std::string>("master.flat_low_signal"));
    EXPECT_FALSE(base.frames_read);
    EXPECT_FALSE(control.code.has_value()) << control.code.value_or("");
    EXPECT_TRUE(control.frames_read);
    EXPECT_TRUE(control.primary);
    EXPECT_GT(control.faults, base.faults) << "contatore del fault_injector non collegato";
  }
  for (const job_case& c : cases) {
    SCOPED_TRACE(c.name);
    master_recipe none = recipe_of(c.g, 0.25);
    write_frames(c.g, none, dir, c.name);
    master_recipe low = none;
    low.flat_min_signal_adu = 1e9;
    const job_probe base = probe_job(dir / ("store-" + c.name + "-base"), low);
    ASSERT_FALSE(base.setup_error.has_value()) << *base.setup_error;
    ASSERT_EQ(base.code, std::optional<std::string>("master.flat_low_signal"));

    master_recipe r = ratio_recipe(c.g, c.k, c.a, c.budget);
    for (std::size_t f = 0; f < r.frames.size(); ++f) r.frames[f].path = none.frames[f].path;
    if (c.min_signal) r.flat_min_signal_adu = *c.min_signal;
    const job_probe p = probe_job(dir / ("store-" + c.name), r);
    ASSERT_FALSE(p.setup_error.has_value()) << "nodo non creabile: " << *p.setup_error;
    EXPECT_EQ(p.code, std::optional<std::string>(c.code));
    EXPECT_EQ(p.category, std::optional<error_category>(c.category));
    EXPECT_FALSE(p.frames_read);
    EXPECT_FALSE(p.primary);
    EXPECT_EQ(p.faults, base.faults);
  }
}

// ================================================================ B8: nessun master con `median_ratio` prima di B9

// Decisione del coordinatore (B8, punto 6): una ricetta `median_ratio` che supera validazione e preflight non produce un
// master in B8, perché combinare senza normalizzare darebbe un risultato sbagliato. Dopo il preflight, prima dei blocchi
// e di ogni artefatto: `unsupported`, `master.method_not_computable`, contesto esatto {flat_normalization: median_ratio}.
// B9 sostituisce questo stop col calcolo degli y_f (C-02.b-flat §f.1) e toglie il test.
void expect_not_computable(const stop_error& e) {
  EXPECT_EQ(e.category(), error_category::unsupported) << e.what();
  EXPECT_EQ(e.code(), "master.method_not_computable") << e.what();
  EXPECT_EQ(ctx_keys(e), (std::set<std::string>{"flat_normalization"})) << e.what();
  EXPECT_EQ(ctx(e, "flat_normalization"), std::optional<std::string>("median_ratio"));
}

// Livelli uguali (1024 in ogni frame): ogni controllo di §f.2 passa per ogni metodo ammesso; in memoria nessun risultato,
// nel job nessun artefatto e nessun lavoro sullo store oltre uno stop del preflight di riferimento sugli stessi frame.
TEST(MasterB8, T_M3B_FLAT_06_admitted_median_ratio_stops_with_method_not_computable_until_b9) {
  const fs::path dir = fresh_dir("b8-not-computable");
  const group g = level_flat({1024, 1024, 1024, 1024});
  master_recipe none = recipe_of(g, 0.25);
  write_frames(g, none, dir, "f");
  master_recipe low = none;
  low.flat_min_signal_adu = 1e9;
  const job_probe base = probe_job(dir / "store-base", low);
  ASSERT_FALSE(base.setup_error.has_value()) << *base.setup_error;
  ASSERT_EQ(base.code, std::optional<std::string>("master.flat_low_signal"));
  for (const kind k : kAllMethods) {
    SCOPED_TRACE(name_of(k));
    master_recipe r = ratio_recipe(g, k);
    for (std::size_t f = 0; f < r.frames.size(); ++f) r.frames[f].path = none.frames[f].path;
    {
      SCOPED_TRACE("combine_master");
      const std::optional<stop_error> e = stop_of(r, g);
      ASSERT_TRUE(e.has_value()) << "master prodotto senza normalizzazione";
      expect_not_computable(*e);
    }
    {
      SCOPED_TRACE("job");
      const job_probe p = probe_job(dir / (std::string("store-") + std::to_string(static_cast<int>(k))), r);
      ASSERT_FALSE(p.setup_error.has_value()) << "nodo non creabile: " << *p.setup_error;
      ASSERT_TRUE(p.stop.has_value()) << "job completato senza normalizzazione";
      expect_not_computable(*p.stop);
      EXPECT_FALSE(p.primary);
      EXPECT_EQ(p.faults, base.faults);
    }
  }
}

// Confini ammessi di §f.2 visti in B8 attraverso lo stop `master.method_not_computable` (un caso respinto darebbe il
// codice del controllo). B9 sostituisce questo stop col calcolo e porta qui i valori della combinazione.
// - Controllo 4 con uguaglianza esatta: livelli (1, 3, 3/2, 3/2) → m_H = 3/2, ρ = 1/2 (test del predecessore); A = 3/2,
//   budget = 1/2: A · ρ = 3/4 = budget · m_H in aritmetica esatta, ammesso (la regola è `≤`).
// - Controllo 3 con uguaglianza esatta: livelli (1024, 2048, 4096), max m = 4 min m, ammesso; A = 0.
TEST(MasterB8, T_M3B_FLAT_06_exact_budget_and_ratio_boundaries_are_admitted_and_reach_method_not_computable) {
  {
    const group g = level_flat({1, 3, 1.5, 1.5}, 512);
    for (const kind k : kBudgetMethods) {
      SCOPED_TRACE(std::string("budget uguale, ") + name_of(k));
      const std::optional<stop_error> e = stop_of(ratio_recipe(g, k, 1.5, 0.5), g);
      ASSERT_TRUE(e.has_value()) << "master prodotto senza normalizzazione";
      expect_not_computable(*e);
    }
  }
  {
    const group g = level_flat({1024, 2048, 4096});
    for (const kind k : kRatioMethods) {
      SCOPED_TRACE(std::string("rapporto 4, ") + name_of(k));
      const std::optional<stop_error> e = stop_of(ratio_recipe(g, k, 0.0), g);
      ASSERT_TRUE(e.has_value()) << "master prodotto senza normalizzazione";
      expect_not_computable(*e);
    }
  }
}

// §f.1 e plan B8: m_H = n / Σ_f (1/m_f) con la somma f64 nell'ordine canonico (digest crescente). Livelli scelti in modo
// che la somma da sinistra nell'ordine canonico e nell'ordine inverso diano m_H f64 diversi (premessa verificata qui);
// l'oracle riesegue la somma f64 nell'ordine canonico, r_f = m_H / m_f e ρ = max |r_f − 1| come §f.1 e §f.2.
// A = 1024, budget = 1/2: A · ρ > budget · m_H, stop del budget con m_H e ρ dell'ordine canonico nel contesto.
// Con i digest di questa fixture il primo candidato utile è (1, 3, 3, 1), ordine canonico (3, 3, 1, 1):
//   canonico fl(fl(fl(1/3) + fl(1/3)) + 1) + 1 = 0x1.5555555555555p+1 → m_H = 4 / Σ = 3/2, r = (1/2, 1/2, 3/2, 3/2), ρ = 1/2;
//   inverso 1 + 1 + fl(1/3) + fl(1/3) = 0x1.5555555555556p+1 → m_H = 1.4999999999999998.
TEST(MasterB8, T_M3B_FLAT_01_harmonic_mean_sums_the_reciprocals_in_canonical_order) {
  const std::vector<std::vector<double>> candidates = {
      {1, 1, 3, 3}, {1, 3, 3, 1}, {3, 1, 1, 3}, {1, 3, 1, 3}, {3, 3, 1, 1}, {3, 1, 3, 1}};
  bool found = false;
  for (const std::vector<double>& levels : candidates) {
    const group g = level_flat(levels, 512);
    const master_recipe r = ratio_recipe(g, kind::weighted, 1024, 0.5);
    std::vector<double> canon(levels.size());
    for (std::size_t f = 0; f < levels.size(); ++f) canon[canonical_position(r, f)] = levels[f];
    double forward = 0, backward = 0;
    for (std::size_t k = 0; k < canon.size(); ++k) {
      forward += 1 / canon[k];
      backward += 1 / canon[canon.size() - 1 - k];
    }
    const double n = static_cast<double>(canon.size());
    const double m_h = n / forward;
    if (m_h == n / backward) continue;
    found = true;
    double rho = 0;
    for (double m : canon) rho = std::max(rho, std::fabs(m_h / m - 1));
    SCOPED_TRACE("m_H canonico " + num(m_h) + ", inverso " + num(n / backward));
    const std::optional<stop_error> e = stop_of(r, g);
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_budget_stop(*e, 1024, rho, m_h, 0.5);
    break;
  }
  EXPECT_TRUE(found) << "nessun candidato con m_H diverso fra ordine canonico e inverso";
}

// ================================================================ correzioni della review di B8 (giro 1)

// Controllo 3 solo per mediana, winsorized e `sigma_clip`: con `mean` e `weighted_mean` il rapporto succ(4) è ammesso
// (A = 0, controllo 4 soddisfatto) e il caso arriva allo stop temporaneo `master.method_not_computable` (B9: calcolo).
TEST(MasterB8, T_M3B_FLAT_06_ratio_above_four_is_not_checked_for_mean_and_weighted_mean) {
  const group g = level_flat({1024, 2048, std::nextafter(4096.0, 8192.0)});
  for (const kind k : {kind::mean, kind::weighted}) {
    SCOPED_TRACE(name_of(k));
    const std::optional<stop_error> e = stop_of(ratio_recipe(g, k, 0.0), g);
    ASSERT_TRUE(e.has_value()) << "master prodotto senza normalizzazione";
    expect_not_computable(*e);
  }
}

// Controllo 2, una costante per caso, `mean`, offset 0, segnale minimo 2⁻¹⁰⁷⁴ (ogni livello supera il controllo 1):
// - somma: quattro frame a 2⁻¹⁰²², 1/m = 2¹⁰²² normale; Σ = 2¹⁰²³, 1.5 · 2¹⁰²³, poi 2¹⁰²⁴ = inf in qualunque ordine;
// - m_H: livelli (2⁻¹⁰²³, 1.5 · 2⁻¹⁰²³), 1/m = 2¹⁰²³ e 2¹⁰²³ / 1.5 normali, Σ = (5/3) · 2¹⁰²³ finita,
//   m_H = 2 / Σ = 1.2 · 2⁻¹⁰²³ subnormale (valore f64 ricalcolato qui: due addendi, la somma è commutativa);
// - r_f: livelli (2⁻¹⁰⁰⁰, 2³⁰), Σ = fl(2¹⁰⁰⁰ + 2⁻³⁰) = 2¹⁰⁰⁰, m_H = 2⁻⁹⁹⁹, r = (2, 2⁻¹⁰²⁹): r del frame a 2³⁰ subnormale;
// - CFA: canale G1 di RGGB come il caso della somma, gli altri canali a 8 (costanti normali) → `canale` G1.
// 1/m_f è nei due test precedenti.
TEST(MasterB8, T_M3B_FLAT_06_each_scale_constant_of_control_2_stops_with_its_own_context) {
  const auto run = [](const group& g) {
    master_recipe r = ratio_recipe(g, kind::mean);
    r.flat_min_signal_adu = kMinSub;
    return std::pair{r, stop_of(r, g)};
  };
  const auto expect_code = [](const stop_error& e, std::set<std::string> keys, const std::string& constant) {
    EXPECT_EQ(e.category(), error_category::numeric) << e.what();
    EXPECT_EQ(e.code(), "master.flat_scale_nonfinite") << e.what();
    EXPECT_EQ(ctx_keys(e), keys) << e.what();
    EXPECT_EQ(ctx(e, "costante"), std::optional<std::string>(constant));
  };
  {
    SCOPED_TRACE("somma");
    const double m = 0x1p-1022;
    ASSERT_TRUE(std::isinf(4 / m));
    const auto [r, e] = run(level_flat({m, m, m, m}, 0.0));
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_code(*e, {"costante", "valore"}, "somma");
    EXPECT_EQ(ctx(*e, "valore"), std::optional<std::string>(num(kInf)));
  }
  {
    SCOPED_TRACE("m_H");
    const double a = 0x1p-1023, b = 1.5 * 0x1p-1023;
    const double m_h = 2 / (1 / a + 1 / b);
    ASSERT_GT(m_h, 0.0);
    ASSERT_LT(m_h, std::numeric_limits<double>::min());
    const auto [r, e] = run(level_flat({a, b}, 0.0));
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_code(*e, {"costante", "valore"}, "m_H");
    EXPECT_EQ(ctx(*e, "valore"), std::optional<std::string>(num(m_h)));
  }
  {
    SCOPED_TRACE("r_f");
    const double lo = 0x1p-1000, hi = 0x1p30;
    ASSERT_EQ(1 / lo + 1 / hi, 0x1p1000);
    ASSERT_LT(0x1p-999 / hi, std::numeric_limits<double>::min());
    const auto [r, e] = run(level_flat({lo, hi}, 0.0));
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_code(*e, {"costante", "frame", "m_f"}, "r_f");
    EXPECT_EQ(ctx(*e, "frame"), std::optional<std::string>(frame_label(canonical_position(r, 1))));
    EXPECT_EQ(ctx(*e, "m_f"), std::optional<std::string>(num(hi)));
  }
  {
    SCOPED_TRACE("somma, CFA canale G1");
    const double m = 0x1p-1022;
    const auto [r, e] = run(rggb_flat({{8, m, 8, 8}, {8, m, 8, 8}, {8, m, 8, 8}, {8, m, 8, 8}}, 0.0));
    ASSERT_TRUE(e.has_value()) << "nessuno stop";
    expect_code(*e, {"canale", "costante", "valore"}, "somma");
    EXPECT_EQ(ctx(*e, "valore"), std::optional<std::string>(num(kInf)));
    EXPECT_EQ(ctx(*e, "canale"), std::optional<std::string>("G1"));
  }
}

// m̄_c serve solo alla tolleranza di `none`: con `median_ratio` non si calcola. Cinque frame a 4e307: la somma dei livelli
// (2e308) non è finita, ma 1/m = 2.5e-308 è normale, Σ, m_H ≈ 4e307 e r ≈ 1 sono finiti e normali → il caso è ammesso
// e arriva allo stop temporaneo `master.method_not_computable` (B9: calcolo), non a `master.report_nonfinite`.
TEST(MasterB8, T_M3B_FLAT_06_huge_admitted_levels_do_not_stop_on_the_level_mean_with_median_ratio) {
  const double m = 4e307;
  ASSERT_TRUE(std::isinf(m + m + m + m + m));
  ASSERT_GE(1 / m, std::numeric_limits<double>::min());
  const group g = level_flat({m, m, m, m, m}, 0.0);
  const std::optional<stop_error> e = stop_of(ratio_recipe(g, kind::mean, 0.0), g);
  ASSERT_TRUE(e.has_value()) << "master prodotto senza normalizzazione";
  expect_not_computable(*e);
}

}  // namespace
