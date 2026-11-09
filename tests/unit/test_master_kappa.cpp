// M3.b, unità B4: κ di mediana e winsorized (C-02.b §b.6 «Metodi robusti», §b.2, §b.12; ADR-09.b classe P).
// Test: T-M3B-C02-05 (κ del prodotto contro l'oracle e le ancore esatte, dominio), T-M3B-C02-15 (solo
// `master.kappa_accuracy`).
//
// Nessun valore atteso viene dal prodotto. L'oracle è tests/unit/kappa_oracle.hpp (long double, Kronrod a 21 punti
// nelle coordinate logit della fattorizzazione di Dirichlet); la sua stima d'errore (dimezzamento del passo e confronto
// K21/G20) deve essere ≤ 1e-11 in ogni cella prima del confronto (docs/evidence/M3b-protocollo.md, T-M3B-C02-05).
// Le soglie sono quelle di C-02 §b.12, scritte qui come letterali, non lette dalle costanti del prodotto.
//
// Celle (C-02 §b.12): mediana per ogni n in 2…64; winsorized per ogni g con n ≤ 32; n ∈ {101, 255, 1024} con
// g ∈ {1, ⌊n/10⌋, ⌊(n − 1)/2⌋}; mediana con n = 101 e winsorized (101, 2). Le celle complete sono nei test `slow`
// (circa 10 s di sola quadratura in dev); i test `fast` coprono un sottoinsieme fisso con ogni forma di cella.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "astro/master_kappa.hpp"
#include "kappa_oracle.hpp"
#include "master_support.hpp"

namespace {

using namespace mst;
namespace ko = kappa_oracle;
using ld = ko::ld;

constexpr double kProductRelTol = 1e-9;      // C-02 §b.12: scarto relativo contro oracle e ancore
constexpr ld kOracleEstimateMax = 1e-11L;    // C-02 §b.12: stima dell'errore dell'oracle, prima della misura
constexpr ld kOracleMassMax = 1e-12L;        // massa della misura del prodotto tensoriale
constexpr ld kOracleCrossMax = 1e-11L;       // controllo indipendente E[(nT)(n Z̄)] = n
constexpr ld kPi = 3.141592653589793238462643383279502884L;

struct cell {
  kappa_estimator t;
  std::uint32_t n, g;
  bool operator<(const cell& o) const { return std::tie(t, n, g) < std::tie(o.t, o.n, o.g); }
};

std::string name_of(const cell& c) {
  return std::string(c.t == kappa_estimator::median ? "median" : "winsorized") + " n=" + std::to_string(c.n) +
         " g=" + std::to_string(c.g);
}

std::vector<cell> contract_cells() {
  std::set<cell> s;
  for (std::uint32_t n = 2; n <= 64; ++n) s.insert({kappa_estimator::median, n, 0});
  for (std::uint32_t n = 3; n <= 32; ++n)
    for (std::uint32_t g = 1; 2 * g + 1 <= n; ++g) s.insert({kappa_estimator::winsorized, n, g});
  for (std::uint32_t n : {101u, 255u, 1024u})
    for (std::uint32_t g : {1u, n / 10, (n - 1) / 2}) s.insert({kappa_estimator::winsorized, n, g});
  s.insert({kappa_estimator::median, 101, 0});
  s.insert({kappa_estimator::winsorized, 101, 2});
  return {s.begin(), s.end()};
}

// Sottoinsieme fisso: mediane dispari (integrale semplice) e pari, winsorized con m = 0, m piccolo e m grande, code
// lunghe (g = 1 con n = 1024) e gli estremi del dominio.
std::vector<cell> fast_cells() {
  std::set<cell> s;
  for (std::uint32_t n = 3; n <= 63; n += 2) s.insert({kappa_estimator::median, n, 0});
  for (std::uint32_t n : {2u, 4u, 10u, 64u, 1024u}) s.insert({kappa_estimator::median, n, 0});
  for (std::uint32_t n : {101u}) s.insert({kappa_estimator::median, n, 0});
  for (auto [n, g] : std::vector<std::pair<std::uint32_t, std::uint32_t>>{
           {3, 1}, {4, 1}, {5, 1}, {10, 2}, {31, 15}, {32, 15}, {32, 1}, {101, 2}, {101, 10}, {101, 50},
           {255, 25}, {1024, 1}, {1024, 102}, {1024, 511}})
    s.insert({kappa_estimator::winsorized, n, g});
  return {s.begin(), s.end()};
}

ko::result oracle_of(const cell& c) {
  return c.t == kappa_estimator::median ? ko::median(c.n) : ko::winsorized_or_mean(c.n, c.g);
}

// Controlli dell'oracle prima della misura: stima ≤ 1e-11, massa, covarianza con la media, κ ≥ 1/n (la media è lo
// stimatore equivariante di varianza minima sotto la gaussiana).
::testing::AssertionResult oracle_ok(const ko::result& r, std::uint32_t n) {
  if (!(r.est_rel <= kOracleEstimateMax))
    return ::testing::AssertionFailure() << "stima d'errore dell'oracle " << static_cast<double>(r.est_rel)
                                         << " (dimezzamento " << static_cast<double>(r.est_halving) << ", regole "
                                         << static_cast<double>(r.est_rules) << ")";
  if (!(r.mass_dev <= kOracleMassMax))
    return ::testing::AssertionFailure() << "massa dell'oracle: scarto " << static_cast<double>(r.mass_dev);
  if (!(r.cross_dev <= kOracleCrossMax))
    return ::testing::AssertionFailure() << "E[(nT)(n Z̄)] / n − 1 = " << static_cast<double>(r.cross_dev);
  if (!(r.kappa * n >= 1.0L - 1e-12L))
    return ::testing::AssertionFailure() << "n κ = " << static_cast<double>(r.kappa * n) << " < 1";
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult rel_close(double got, ld expected, double tol) {
  if (!std::isfinite(got)) return ::testing::AssertionFailure() << "valore non finito " << got;
  const ld rel = std::fabs(static_cast<ld>(got) - expected) / std::fabs(expected);
  if (rel <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "atteso " << static_cast<double>(expected) << ", ottenuto " << got
                                       << ", scarto relativo " << static_cast<double>(rel) << " > " << tol;
}

std::optional<kappa_result> product_kappa(const cell& c) {
  try {
    return robust_kappa(c.t, c.n, c.g);
  } catch (const stop_error& e) {
    ADD_FAILURE() << name_of(c) << ": stop inatteso " << e.what();
    return std::nullopt;
  }
}

// La stima del prodotto è finita, non negativa ed entro la soglia: altrimenti il prodotto si fermerebbe con
// master.kappa_accuracy su una cella del dominio.
::testing::AssertionResult estimate_ok(const kappa_result& k) {
  if (std::isfinite(k.rel_error_estimate) && k.rel_error_estimate >= 0 && k.rel_error_estimate <= kProductRelTol)
    return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "stima d'errore del prodotto " << k.rel_error_estimate;
}

void compare_cells(const std::vector<cell>& cells) {
  // Prima della misura: l'oracle deve rispettare la propria stima in ogni cella.
  std::vector<ld> expected;
  for (const cell& c : cells) {
    const ko::result r = oracle_of(c);
    ASSERT_TRUE(oracle_ok(r, c.n)) << name_of(c);
    expected.push_back(r.kappa);
  }
  for (std::size_t i = 0; i < cells.size(); ++i) {
    SCOPED_TRACE(name_of(cells[i]));
    const std::optional<kappa_result> k = product_kappa(cells[i]);
    if (!k) continue;
    EXPECT_TRUE(estimate_ok(*k));
    EXPECT_TRUE(rel_close(k->value, expected[i], kProductRelTol));
  }
}

void oracle_cells(const std::vector<cell>& cells) {
  for (const cell& c : cells) EXPECT_TRUE(oracle_ok(oracle_of(c), c.n)) << name_of(c);
}

// ---------------------------------------------------------------- oracle (devono passare senza il prodotto)

TEST(MasterKappaOracle, T_M3B_C02_05_oracle_rules_are_exact_on_monomials) {
  // K21 è esatta fino al grado 31, G20 fino al 39: ∫_{−1}^{1} t^k dt = 2/(k + 1) per k pari, 0 per k dispari.
  auto err = [](const ko::quad_rule& r, int k) {
    ld s = 0;
    for (std::size_t j = 0; j < r.t.size(); ++j) s += r.w[j] * std::pow(r.t[j], static_cast<ld>(k));
    return std::fabs(s - (k % 2 == 0 ? 2.0L / (k + 1) : 0.0L));
  };
  for (int k = 0; k <= 31; ++k) EXPECT_LE(err(ko::k21(), k), 2e-18L) << "K21 grado " << k;
  for (int k = 0; k <= 39; ++k) EXPECT_LE(err(ko::g20(), k), 2e-18L) << "G20 grado " << k;
  // Il controllo distingue: oltre il grado di esattezza l'errore è visibile.
  EXPECT_GT(err(ko::k21(), 32), 1e-14L);
  EXPECT_GT(err(ko::g20(), 40), 1e-14L);
}

TEST(MasterKappaOracle, T_M3B_C02_05_oracle_inverse_normal) {
  EXPECT_EQ(ko::inv_Phi_lower(0.5L).x, 0.0L);
  // Quantile noto: Φ⁻¹(0.975) = 1.959963984540054…
  EXPECT_NEAR(static_cast<double>(ko::inv_Phi(0.975L, 0.025L).x), 1.959963984540054, 1e-15);
  for (ld e = -60; e <= 0; e += 0.25L) {
    const ld P = 0.5L * std::pow(10.0L, e);
    const ko::normal_point r = ko::inv_Phi_lower(P);
    // Residui relativi entro il condizionamento in coda (x φ(x)/Φ(x) ≈ x², e x²/2 nell'esponente di φ): poche unità
    // di arrotondamento per (1 + x²).
    const ld tol = 8 * std::numeric_limits<ld>::epsilon() * (1 + r.x * r.x);
    EXPECT_LE(std::fabs(ko::Phi_lower(r.x) - P) / P, tol) << static_cast<double>(P);
    EXPECT_LE(std::fabs(r.phi - ko::phi(r.x)) / ko::phi(r.x), tol) << static_cast<double>(P);  // exp(−x²/2): stesso ordine
  }
}

TEST(MasterKappaOracle, T_M3B_C02_05_oracle_reproduces_exact_values) {
  // Ancore di C-02.b §b.6 e la media (g = 0, κ = 1/n), che percorre la parte a due coordinate con m = n − 2 interni.
  const ko::result m2 = ko::median(2), m3 = ko::median(3);
  EXPECT_TRUE(oracle_ok(m2, 2));
  EXPECT_TRUE(oracle_ok(m3, 3));
  EXPECT_LE(std::fabs(m2.kappa - 0.5L) / 0.5L, 1e-13L);
  const ld k3 = 1.0L - std::sqrt(3.0L) / kPi;
  EXPECT_LE(std::fabs(m3.kappa - k3) / k3, 1e-13L);
  for (std::uint32_t n : {2u, 3u, 5u, 32u, 101u, 1024u}) {
    const ko::result r = ko::winsorized_or_mean(n, 0);
    EXPECT_TRUE(oracle_ok(r, n)) << n;
    EXPECT_LE(std::fabs(r.kappa * n - 1.0L), 1e-13L) << n;
  }
}

TEST(MasterKappaOracle, T_M3B_C02_05_oracle_estimate_within_bound_fast_cells) { oracle_cells(fast_cells()); }

TEST(MasterKappaOracleSlow, T_M3B_C02_05_oracle_estimate_within_bound_all_cells) {
  const std::vector<cell> cells = contract_cells();
  // Mediane 2…64 (63); winsorized con n ≤ 32 (240); n ∈ {101, 255, 1024} con tre g (9); mediana 101; winsorized (101, 2).
  EXPECT_EQ(cells.size(), 63u + 240u + 9u + 1u + 1u);
  oracle_cells(cells);
}

// ---------------------------------------------------------------- T-M3B-C02-05: prodotto

TEST(MasterKappa, T_M3B_C02_05_exact_anchors) {
  const cell m2{kappa_estimator::median, 2, 0}, m3{kappa_estimator::median, 3, 0};
  if (const auto k = product_kappa(m2)) {
    EXPECT_TRUE(rel_close(k->value, 0.5L, kProductRelTol));
  }
  if (const auto k = product_kappa(m3)) {
    EXPECT_TRUE(rel_close(k->value, 1.0L - std::sqrt(3.0L) / kPi, kProductRelTol));
  }
  // Winsorized con n ∈ {2g + 1, 2g + 2} coincide con la mediana (C-02.b §b.1): contro la mediana del prodotto e
  // contro l'oracle della mediana (che per n = 2g + 1 usa l'integrale semplice, non la parte a due coordinate).
  std::vector<std::uint32_t> gs;
  for (std::uint32_t g = 1; g <= 15; ++g) gs.push_back(g);
  for (std::uint32_t g : {50u, 127u, 511u}) gs.push_back(g);
  for (std::uint32_t g : gs) {
    for (std::uint32_t n : {2 * g + 1, 2 * g + 2}) {
      if (n > 1024) continue;
      SCOPED_TRACE("n=" + std::to_string(n) + " g=" + std::to_string(g));
      const ko::result om = ko::median(n);
      ASSERT_TRUE(oracle_ok(om, n));
      const auto w = product_kappa({kappa_estimator::winsorized, n, g});
      const auto m = product_kappa({kappa_estimator::median, n, 0});
      if (!w || !m) continue;
      EXPECT_TRUE(rel_close(w->value, static_cast<ld>(m->value), kProductRelTol));
      EXPECT_TRUE(rel_close(w->value, om.kappa, kProductRelTol));
    }
  }
}

TEST(MasterKappa, T_M3B_C02_05_product_matches_oracle_fast_cells) { compare_cells(fast_cells()); }

TEST(MasterKappaSlow, T_M3B_C02_05_product_matches_oracle_all_cells) { compare_cells(contract_cells()); }

TEST(MasterKappa, T_M3B_C02_05_same_bits_on_repeated_calls) {
  for (const cell& c : fast_cells()) {
    const auto a = product_kappa(c), b = product_kappa(c);
    if (!a || !b) continue;
    EXPECT_EQ(std::memcmp(&a->value, &b->value, sizeof(double)), 0) << name_of(c);
    EXPECT_EQ(std::memcmp(&a->rel_error_estimate, &b->rel_error_estimate, sizeof(double)), 0) << name_of(c);
  }
}

// Dominio (C-02.b §b.1, §b.2): oltre n = 1024 i metodi robusti sono fuori dominio; la mediana di κ vale da n = 2
// (ancora), winsorized da g = 1 e n = 2g + 1.
TEST(MasterKappa, T_M3B_C02_05_outside_the_domain_is_variance_domain) {
  const std::vector<cell> outside = {
      {kappa_estimator::median, 0, 0},     {kappa_estimator::median, 1, 0},      {kappa_estimator::median, 1025, 0},
      {kappa_estimator::median, 4096, 0},  {kappa_estimator::median, 5, 1},      {kappa_estimator::winsorized, 5, 0},
      {kappa_estimator::winsorized, 4, 2}, {kappa_estimator::winsorized, 2, 1},  {kappa_estimator::winsorized, 1025, 1},
      {kappa_estimator::winsorized, 1024, 512}, {kappa_estimator::winsorized, 2048, 1023},
  };
  for (const cell& c : outside) {
    SCOPED_TRACE(name_of(c));
    EXPECT_STOP(robust_kappa(c.t, c.n, c.g), error_category::unsupported, "master.variance_domain");
  }
}

// ---------------------------------------------------------------- T-M3B-C02-15: master.kappa_accuracy

TEST(MasterKappa, T_M3B_C02_15_kappa_accuracy_threshold) {
  const kappa_estimator t = kappa_estimator::median;
  for (double bad : {2e-9, std::nextafter(1e-9, 1.0), 1e-3, std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN()}) {
    SCOPED_TRACE(bad);
    EXPECT_STOP(check_kappa_accuracy(kappa_result{0.4486711045782080, bad}, t, 3, 0), error_category::numeric,
                "master.kappa_accuracy");
  }
  for (double good : {0.0, 1e-12, 1e-9}) {
    SCOPED_TRACE(good);
    EXPECT_NO_THROW(check_kappa_accuracy(kappa_result{0.4486711045782080, good}, t, 3, 0));
  }
}

struct kappa_probe {
  std::optional<std::string> setup_error;  // nodo non creabile dalla ricetta del nodo
  std::optional<std::string> code;
  std::optional<error_category> category;
  int faults = 0;
  bool primary = false;
  std::vector<std::string> events;  // "kappa" dal gancio on_kappa, i punti del gancio `hook` ("frames.read")
  std::vector<double> kappas;
};

// Progetto col nodo della ricetta `node_recipe`; il job gira con `job_recipe`. Il contatore del fault_injector conta
// ogni punto di protocollo dello store dopo l'allestimento. `alter` riceve il κ prima del controllo d'accuratezza.
kappa_probe probe(const fs::path& root, const master_recipe& node_recipe, const master_recipe& job_recipe,
                  std::function<void(kappa_result&)> alter) {
  kappa_probe out;
  auto store = sqlite_dag_store::create(root);
  std::optional<node_spec> node;
  try {
    node = make_master_node(node_recipe);
  } catch (const stop_error& e) {
    out.setup_error = e.what();
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
  o.hook = [&out](std::string_view point) { out.events.emplace_back(point); };
  o.on_kappa = [&out, alter](kappa_result& k) {
    out.events.emplace_back("kappa");
    out.kappas.push_back(k.value);
    if (alter) alter(k);
  };
  memory_pool pool(256 * (std::uint64_t{1} << 20));
  try {
    const job_outcome res = run_master_quiet(*store, "master", job_recipe, pool, o);
    if (res.stop) {
      out.code = res.stop->code();
      out.category = res.stop->category();
    }
  } catch (const stop_error& e) {
    out.code = e.code();
    out.category = e.category();
  }
  out.faults = faults;
  store->set_fault_injector({});
  out.primary = by_role(*store, node->key()).count("primary") != 0;
  return out;
}

bool has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

// Il controllo positivo del contatore usa `mean`, che non ha κ: non dipende dall'esecuzione completa della mediana
// (B5). Per la mediana si verificano solo il calcolo di κ nel preflight e lo stop d'accuratezza.
TEST(MasterKappa, T_M3B_C02_15_kappa_accuracy_stops_before_any_sample_read_and_store_work) {
  const fs::path dir = fresh_dir("b4-t15");
  group dark = make_group(master_role::dark, 8, 4, 3);
  for (std::size_t k = 0; k < dark.frames.size(); ++k)
    for (std::size_t i = 0; i < dark.frames[k].v.size(); ++i) dark.frames[k].v[i] = 500 + static_cast<double>((i + k) % 7);
  master_recipe mean_r = recipe_of(dark);
  write_frames(dark, mean_r, dir, "dark");
  master_recipe med_r = mean_r;
  med_r.combine = combine_method::median;
  med_r.rejection = master_rejection::none;

  // Controllo positivo del contatore e del gancio dei punti: `mean` legge i frame, pubblica, lavora sullo store e non
  // calcola κ.
  const kappa_probe control = probe(dir / "store-control", mean_r, mean_r, {});
  ASSERT_FALSE(control.setup_error.has_value()) << *control.setup_error;
  EXPECT_FALSE(control.code.has_value()) << control.code.value_or("");
  EXPECT_TRUE(control.primary);
  EXPECT_TRUE(has(control.events, "frames.read"));
  EXPECT_TRUE(control.kappas.empty());

  // Riferimento: stop di parametro (combine assente), prima di ogni lettura e di ogni lavoro sullo store del job.
  master_recipe base_stop = med_r;
  base_stop.combine.reset();
  const kappa_probe baseline = probe(dir / "store-baseline", med_r, base_stop, {});
  ASSERT_FALSE(baseline.setup_error.has_value()) << *baseline.setup_error;
  ASSERT_EQ(baseline.code, std::optional<std::string>("master.param.combine"));
  EXPECT_FALSE(has(baseline.events, "frames.read"));
  EXPECT_TRUE(baseline.kappas.empty());
  EXPECT_GT(control.faults, baseline.faults) << "contatore del fault_injector non collegato";

  // Mediana senza alterazioni: κ calcolato una volta, prima di ogni altro punto, uguale all'ancora; nessuno stop
  // d'accuratezza.
  const kappa_probe honest = probe(dir / "store-honest", med_r, med_r, {});
  ASSERT_FALSE(honest.setup_error.has_value()) << *honest.setup_error;
  ASSERT_EQ(honest.kappas.size(), 1u);
  EXPECT_TRUE(rel_close(honest.kappas[0], 1.0L - std::sqrt(3.0L) / kPi, kProductRelTol));
  EXPECT_EQ(honest.events.front(), "kappa");
  EXPECT_NE(honest.code, std::optional<std::string>("master.kappa_accuracy"));

  // Stima d'errore oltre la soglia (anche non finita): numeric, master.kappa_accuracy, nessuna lettura, nessun nodo,
  // nessun lavoro sullo store oltre il riferimento.
  std::size_t k = 0;
  for (double bad : {2e-9, 1e-6, std::numeric_limits<double>::quiet_NaN()}) {
    SCOPED_TRACE(bad);
    const kappa_probe p = probe(dir / ("store-" + std::to_string(k++)), med_r, med_r, [bad](kappa_result& kr) {
      kr.rel_error_estimate = bad;
    });
    ASSERT_FALSE(p.setup_error.has_value()) << *p.setup_error;
    EXPECT_EQ(p.code, std::optional<std::string>("master.kappa_accuracy"));
    EXPECT_EQ(p.category, std::optional<error_category>(error_category::numeric));
    EXPECT_EQ(p.kappas.size(), 1u);
    EXPECT_FALSE(has(p.events, "frames.read"));
    EXPECT_FALSE(p.primary);
    EXPECT_EQ(p.faults, baseline.faults);
  }
}

}  // namespace
