// M3.b, unità B5: `single`, `weighted_mean`, `median`, `winsorized` (C-02.b §b.1, §b.6, §b.7, §b.8, §b.10, §b.12;
// protocollo preregistrato docs/evidence/M3b-protocollo.md, versione 3, SHA-256
// e21760c7f68590d71bccee78088f1577ea0a1bd7e337bc758c3a4aa7f3e1ffc7). Test: T-M3B-C02-01, 03, 04, 07, 13; T-M3B-C02-15
// (solo `result_nonfinite` e `master.param.weights`); capability e maschere di §b.7.
//
// Oracle in tests/unit/b5_oracle.hpp; κ di riferimento da tests/unit/kappa_oracle.hpp; verità dei momenti da
// FIX-SYN-CAL-08 (tests/fixtures/syn_cal.hpp). Nessun valore atteso viene dal prodotto, salvo κ̂ = robust_kappa(…),
// ingresso puntuale dei piani `gaussian_reference` nella regola I (C-02.b §b.6).
//
// Tolleranze, fissate prima della misura:
// - T-M3B-C02-01, 13 (classe E): 0 dove l'oracle verifica esatta ogni operazione della sequenza che porta al valore
//   (M e V separati); altrimenti la regola I (ampiezza dell'intervallo, ADR-17.b §b.10), più mezzo ulp f32 con uscita
//   f32. Pareggi: bit dell'elemento dell'ordine totale (valore, indice canonico) o dell'aritmetica IEEE esatta che lo
//   combina. Uscita f32: bit uguali a float(uscita f64), un solo arrotondamento.
// - T-M3B-C02-03, 04 (classe S): criteri e margini del protocollo §3.1, §3.2, con `τ_k` = ampiezza dell'intervallo
//   della regola I (indipendente dal prodotto) e il criterio binario di inclusione: ogni valore del prodotto confrontato
//   sta nel suo intervallo. Punti derivati prima della misura (protocollo §5): ρ_c,
//   κ e μ_4 dell'oracle e la verifica h < δ/2, dichiarati sotto e ricalcolati dai test che li usano.
// - T-M3B-C02-07 (classe C): nessuna soglia; solo la condizione d'errore dell'oracle ≤ 10⁻¹² (protocollo §3.7). La
//   parte binaria sul report passa a B11.
//
// Valori derivati, dichiarati prima della misura (ricalcolati e confrontati dai test MasterB5.*_derived_*):
// - ρ_c (C02-03): regola I in razionali esatti, massimo su tutti gli ordini canonici dei pesi (dipende solo dai
//   pesi): W1 9.39061934e-15, W2 1.526875334e-14 (kRhoC1, kRhoC2).
// - κ e μ_4 dell'oracle (σ = 1; stima d'errore ≤ 1.2e-13 in ogni cella) e h/(δ/2) con K, P del protocollo, celle di
//   C02-04(b): mediana n = 3 e winsorized (3, 1) 0.97467; n = 4 (mediana e g = 1) 0.5427; n = 9 mediana e g = 4
//   0.97296, g = 1 0.09934, g = 2 0.12817; n = 10 mediana e g = 4 0.9727, g = 1 0.26004, g = 2 0.31991; n = 101
//   g = 1 0.84313, g = 2 0.84926. κ e μ_4 coincidono con la tabella del protocollo §3.2 alle cifre riportate.
//
// Contenitore delle realizzazioni (C02-03, 04): R realizzazioni consecutive del flusso stanno una sotto l'altra in
// un'immagine 64 × 64R; il pixel p della realizzazione k è il pixel p dell'immagine 64 × 64 del protocollo, con la sua
// media μ_p. Il calcolo del master è per pixel (C-02.b §b.8): K, P, seed, ordine del flusso e statistiche sono quelli
// del protocollo.
#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "b5_oracle.hpp"
#include "chi2_exact.hpp"
#include "kappa_oracle.hpp"
#include "master_support.hpp"
#include "store_test_support.hpp"
#include "syn_cal.hpp"

namespace {

using namespace mst;
namespace sc = astro_fixture::syn_cal;
using b5o::ld;

constexpr double kAlpha = 1e-3;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------- metodi e ricette

enum class kind { single, weighted, median, winsorized };

struct method {
  kind k = kind::median;
  std::size_t g = 0;               // winsorized
  std::vector<double> w;           // weighted_mean: peso del frame, per indice di generazione
  double offset = 10, gain = 1.5, read_noise = 2;  // single

  std::string name() const {
    switch (k) {
      case kind::single: return "single";
      case kind::weighted: return "weighted_mean";
      case kind::median: return "median";
      case kind::winsorized: return "winsorized g=" + std::to_string(g);
    }
    return "";
  }
};

method median_m() {
  method m;
  m.k = kind::median;
  return m;
}
method winsor_m(std::size_t g) {
  method m;
  m.k = kind::winsorized;
  m.g = g;
  return m;
}
method weighted_m(std::vector<double> w) { return {kind::weighted, 0, std::move(w)}; }
method single_m(double offset, double gain, double read_noise) { return {kind::single, 0, {}, offset, gain, read_noise}; }

void apply(const method& m, master_recipe& r) {
  r.rejection = master_rejection::none;
  switch (m.k) {
    case kind::single:
      r.combine = combine_method::single;
      r.offset_adu = m.offset;
      r.conversion_gain_e_per_adu = m.gain;
      r.read_noise_adu = m.read_noise;
      break;
    case kind::weighted:
      r.combine = combine_method::weighted_mean;
      for (std::size_t f = 0; f < r.frames.size(); ++f) r.frames[f].weight = m.w[f];
      break;
    case kind::median: r.combine = combine_method::median; break;
    case kind::winsorized:
      r.combine = combine_method::winsorized;
      r.winsor_g = static_cast<double>(m.g);
      break;
  }
}

// Posizione canonica → indice del frame nella ricetta (C-02.a §8: digest SHA-256 crescente).
std::vector<std::size_t> canonical(const master_recipe& r) {
  std::vector<std::size_t> order(r.frames.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return r.frames[a].digest.sha256 < r.frames[b].digest.sha256; });
  return order;
}

// κ̂ del prodotto (ingresso puntuale della regola I per `gaussian_reference`).
double kappa_hat(const method& m, std::size_t n) {
  if (m.k == kind::median) return robust_kappa(kappa_estimator::median, static_cast<std::uint32_t>(n), 0).value;
  if (m.k == kind::winsorized) {
    return robust_kappa(kappa_estimator::winsorized, static_cast<std::uint32_t>(n), static_cast<std::uint32_t>(m.g)).value;
  }
  return 0;
}

std::vector<double> canonical_weights(const method& m, const std::vector<std::size_t>& order) {
  std::vector<double> w;
  for (std::size_t c : order) w.push_back(m.w[c]);
  return w;
}

// Combinazione in memoria; uno stop diventa un fallimento del test con codice e contesto.
std::optional<master_result> combine_or_fail(const master_recipe& r, std::vector<image> frames, const std::string& what) {
  try {
    return combine_master(r, std::move(frames));
  } catch (const stop_error& e) {
    ADD_FAILURE() << what << ": stop " << e.code() << " — " << e.what();
    return std::nullopt;
  }
}

// ---------------------------------------------------------------- attese per pixel (C02-01, 13, 15)

struct expectation {
  b5o::exact_pixel ex;
  ld width_m = 0, width_v = 0;  // regola I
  bool excluded = false;
};

expectation expect_of(const method& m, const std::vector<double>& x, const std::vector<double>& w_canon, double kappa) {
  expectation e;
  b5o::rule_i r;
  switch (m.k) {
    case kind::single: {
      e.ex = b5o::exact_single(x[0], m.offset, m.gain, m.read_noise);
      const b5o::ivl v = b5o::single_interval(r, x[0], m.offset, m.gain, m.read_noise);
      e.width_v = v.hi - v.lo;
      break;
    }
    case kind::weighted: {
      e.ex = b5o::exact_weighted(x, w_canon);
      const b5o::weight_intervals k = b5o::weight_constant_intervals(w_canon);
      const b5o::pixel_intervals p = b5o::weighted_intervals(r, x.data(), x.size(), w_canon, k);
      e.width_m = p.m.hi - p.m.lo;
      e.width_v = p.v.hi - p.v.lo;
      break;
    }
    case kind::median:
    case kind::winsorized: {
      e.ex = m.k == kind::median ? b5o::exact_median(x, kappa) : b5o::exact_winsorized(x, m.g, kappa);
      const std::vector<double> z = b5o::sorted_total(x);
      const b5o::ivl mi = m.k == kind::median ? b5o::median_interval(r, z.data(), z.size())
                                              : b5o::winsorized_interval(r, z.data(), z.size(), m.g);
      const b5o::ivl vi = b5o::kappa_plane(r, kappa, b5o::s2_interval(r, x));
      e.width_m = mi.hi - mi.lo;
      e.width_v = vi.hi - vi.lo;
      break;
    }
  }
  e.excluded = r.excluded;
  return e;
}

// |got − esatto| ≤ tolleranza: 0 se la sequenza è esatta, altrimenti l'ampiezza della regola I (+ mezzo ulp f32).
::testing::AssertionResult close_to(double got, const exact::rat& value, bool exact_seq, ld width, bool f32) {
  if (!std::isfinite(got)) return ::testing::AssertionFailure() << "valore non finito " << got;
  exact::rat tol = exact::from_u(0);
  if (!exact_seq) tol = b5o::from_ld(width);
  if (f32) tol = tol + half_ulp_out(value, got, true);
  const exact::rat diff = exact::abs(exact::from_double(got) - value);
  if (diff <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "scarto " << b5o::approx(diff) << " oltre " << b5o::approx(tol) << " (esatto "
                                       << std::setprecision(17) << b5o::approx(value) << ", ottenuto " << got
                                       << (exact_seq ? ", sequenza esatta" : ", regola I") << ")";
}

// Confronto di un pixel del master con l'attesa.
void expect_pixel(const master_result& res, std::size_t i, const expectation& e, const std::string& where) {
  const bool f32 = res.output.type == sample_type::f32;
  const double m = sample(res.output, i), v = plane(res.output, i);
  const std::uint16_t bits = res.output.invalid.bits[i];
  ASSERT_FALSE(e.excluded) << where << ": la regola I non si applica (caso da togliere dalla fixture)";
  if (e.ex.nonfinite) {
    EXPECT_EQ(bits, invalid_reason::result_nonfinite) << where;
    EXPECT_TRUE(std::isnan(m) && std::isnan(v)) << where << ": campione " << m << ", piano " << v;
    return;
  }
  EXPECT_EQ(bits, 0) << where;
  EXPECT_TRUE(close_to(m, e.ex.m, e.ex.exact_m, e.width_m, f32)) << where << ": M";
  EXPECT_TRUE(close_to(v, e.ex.v, e.ex.exact_v, e.width_v, f32)) << where << ": V";
}

// ---------------------------------------------------------------- gruppi di frame dai valori per pixel

// px[i][f]: campione del frame f (indice di generazione) al pixel i.
group group_of(const std::vector<std::vector<double>>& px, std::uint32_t w, std::uint32_t h) {
  const std::size_t n = px.at(0).size();
  group g = make_group(master_role::dark, w, h, n);
  for (std::size_t i = 0; i < px.size(); ++i) {
    if (px[i].size() != n) ADD_FAILURE() << "pixel " << i << " con un numero di frame diverso";
    for (std::size_t f = 0; f < n; ++f) g.frames[f].v[i] = px[i][f];
  }
  return g;
}

// Campioni del pixel i nell'ordine canonico.
std::vector<double> canonical_samples(const group& g, const std::vector<std::size_t>& order, std::size_t i) {
  std::vector<double> x;
  for (std::size_t c : order) x.push_back(stored(g.frames[c], i));
  return x;
}

struct case_result {
  std::optional<master_result> res;
  std::vector<expectation> exp;
};

case_result run_case(const group& g, const method& m, sample_type out = sample_type::f64) {
  case_result cr;
  master_recipe r = recipe_of(g);
  r.output_type = out;
  apply(m, r);
  const std::vector<std::size_t> order = canonical(r);
  const std::size_t n = g.frames.size();
  const std::vector<double> wc = m.k == kind::weighted ? canonical_weights(m, order) : std::vector<double>{};
  const double kap = kappa_hat(m, n);
  for (std::size_t i = 0; i < static_cast<std::size_t>(g.w) * g.h; ++i) cr.exp.push_back(expect_of(m, canonical_samples(g, order, i), wc, kap));
  cr.res = combine_or_fail(r, images_of(g), m.name());
  return cr;
}

void check_case(const case_result& cr, const std::string& what) {
  if (!cr.res) return;
  for (std::size_t i = 0; i < cr.exp.size(); ++i) expect_pixel(*cr.res, i, cr.exp[i], what + " pixel " + std::to_string(i));
}

// ---------------------------------------------------------------- T-M3B-C02-01: casi dyadic

struct dyadic_case {
  std::string name;
  method m;
  std::vector<std::vector<double>> px;  // per pixel, valori per frame
  bool v_exact_required;                // dichiarato: V esatto su ogni pixel (altrimenti come risulta dall'oracle)
};

// Permutazioni cicliche dei valori sui frame: la selezione non dipende dalla posizione del frame.
std::vector<std::vector<double>> rotations(const std::vector<std::vector<double>>& base) {
  std::vector<std::vector<double>> out;
  for (const auto& v : base) {
    for (std::size_t s = 0; s < v.size(); ++s) {
      std::vector<double> r(v.size());
      for (std::size_t f = 0; f < v.size(); ++f) r[f] = v[(f + s) % v.size()];
      out.push_back(r);
    }
  }
  return out;
}

std::vector<dyadic_case> dyadic_cases() {
  std::vector<dyadic_case> c;
  // Mediana a conteggio dispari: s² potenza di due o nulla, quindi κ̂ · s² esatto.
  c.push_back({"median n=3", median_m(),
               rotations({{1, 2, 3}, {0, 0, 0}, {4, 6, 8}, {-6, -4, -2}, {1000, 1002, 1004}, {0.5, 1, 1.5}}), true});
  c.push_back({"median n=5", median_m(), rotations({{0, 2, 2, 2, 4}, {-1, 0, 0, 0, 1}, {1000, 1002, 1002, 1002, 1004}}), true});
  c.push_back({"median n=9", median_m(), rotations({{-2, 0, 0, 0, 0, 0, 0, 0, 2}}), true});
  // Mediana a conteggio pari: (z_k + z_(k+1)) / 2 esatto; s² = ss/3 potenza di due.
  c.push_back({"median n=4", median_m(),
               rotations({{0, 0, 0, 2}, {0, 2, 2, 2}, {0, 2, 3, 3}, {-1, 1, 2, 2}, {1, 1, 1, 3}, {1000, 1002, 1003, 1003}}),
               true});
  c.push_back({"median n=8", median_m(), rotations({{0, 0, 0, 0, 0, 0, 0, 8}, {1, 1, 1, 1, 1, 1, 1, 9}}), true});
  // winsorized con n potenza di due (Σ v_i / n esatto), g = 1 e g = ⌊(n − 1)/2⌋.
  c.push_back({"winsorized n=4 g=1", winsor_m(1), rotations({{0, 2, 3, 3}, {0, 0, 0, 2}, {-1, 1, 2, 2}, {1000, 1002, 1003, 1003}}),
               true});
  c.push_back({"winsorized n=8 g=1", winsor_m(1), rotations({{0, 1, 2, 3, 4, 5, 6, 7}, {0, 0, 0, 0, 0, 0, 0, 8}}), false});
  c.push_back({"winsorized n=8 g=3", winsor_m(3), rotations({{0, 1, 2, 3, 4, 5, 6, 7}, {1, 1, 1, 1, 1, 1, 1, 9}}), false});
  // weighted_mean con W potenza di due: M esatto; i c_f non sono mai rappresentabili (ricerca esaustiva n = 3…6,
  // W ≤ 64: nessun peso dà T e c_f esatti), quindi V usa la regola I.
  c.push_back({"weighted_mean n=4 w=(3,1,2,2)", weighted_m({3, 1, 2, 2}), rotations({{0, 8, 16, 24}, {1000, 1001, 1002, 1003}, {-4, 4, -4, 4}}),
               false});
  c.push_back({"weighted_mean n=3 w=(3,3,2)", weighted_m({3, 3, 2}), rotations({{0, 8, 16}, {1000, 1004, 1008}, {5, 5, 5}}), false});
  return c;
}

TEST(MasterB5, T_M3B_C02_01_dyadic_cases_give_the_exact_master_and_plane) {
  for (const dyadic_case& dc : dyadic_cases()) {
    SCOPED_TRACE(dc.name);
    const std::uint32_t w = static_cast<std::uint32_t>(dc.px.size());
    const group g = group_of(dc.px, w, 1);
    const case_result cr = run_case(g, dc.m);
    // Precondizione: l'oracle verifica esatta la sequenza di M su ogni pixel (e di V dove dichiarato).
    bool all_m = true, all_v = true, any_v = false;
    for (const expectation& e : cr.exp) {
      all_m = all_m && e.ex.exact_m && !e.ex.nonfinite;
      all_v = all_v && e.ex.exact_v;
      any_v = any_v || e.ex.exact_v;
    }
    ASSERT_TRUE(all_m) << "caso non dyadic per M";
    if (dc.v_exact_required) {
      EXPECT_TRUE(all_v) << "V dichiarato esatto non lo è";
    }
    if (dc.m.k != kind::weighted) {
      EXPECT_TRUE(any_v) << "nessun pixel con V esatto";
    }
    check_case(cr, dc.name);
  }
}

// Pareggi (C-02.b §b.8): ordine totale su (valore, indice canonico); il segno dello zero è quello dell'elemento
// selezionato, o dell'aritmetica IEEE esatta su elementi scelti dall'ordine totale.
std::uint64_t bits_of(double v) { return std::bit_cast<std::uint64_t>(v); }

double ieee_median(const std::vector<double>& z) {
  const std::size_t n = z.size();
  if (n % 2 == 1) return z[n / 2];
  const double s = z[n / 2 - 1] + z[n / 2];
  return s / 2.0;
}
double ieee_winsorized(const std::vector<double>& z, std::size_t g) {
  const std::vector<double> v = b5o::winsor_values(z, g);
  double s = v[0];
  for (std::size_t i = 1; i < v.size(); ++i) s += v[i];
  return s / static_cast<double>(v.size());
}

// Tutte le assegnazioni dei segni di `zeros` zeri e di un valore non nullo ±x alle posizioni dei frame.
std::vector<std::vector<double>> signed_zero_pixels(std::size_t n, double x) {
  std::vector<std::vector<double>> out;
  for (std::size_t pos = 0; pos <= n; ++pos) {  // pos == n: nessun valore non nullo, solo zeri
    const std::size_t zeros = pos == n ? n : n - 1;
    for (std::uint32_t mask = 0; mask < (1u << zeros); ++mask) {
      for (double sx : pos == n ? std::vector<double>{x} : std::vector<double>{x, -x}) {
        std::vector<double> v(n);
        std::size_t zi = 0;
        for (std::size_t f = 0; f < n; ++f) {
          if (f == pos) v[f] = sx;
          else v[f] = ((mask >> zi++) & 1u) ? -0.0 : 0.0;
        }
        out.push_back(v);
        if (pos == n) break;
      }
    }
  }
  return out;
}

TEST(MasterB5, T_M3B_C02_01_ties_of_minus_and_plus_zero_follow_the_total_order) {
  struct tie_case {
    method m;
    std::size_t n;
  };
  for (const tie_case& tc : {tie_case{median_m(), 3}, tie_case{winsor_m(1), 3}, tie_case{median_m(), 4}, tie_case{winsor_m(1), 4}}) {
    SCOPED_TRACE(tc.m.name() + " n=" + std::to_string(tc.n));
    const std::vector<std::vector<double>> px = signed_zero_pixels(tc.n, tc.n == 3 ? 5.0 : 7.0);
    const group g = group_of(px, static_cast<std::uint32_t>(px.size()), 1);
    master_recipe r = recipe_of(g);
    apply(tc.m, r);
    const std::vector<std::size_t> order = canonical(r);
    const std::vector<image> imgs = images_of(g);
    for (std::size_t i = 0; i < px.size(); ++i) {
      for (std::size_t f = 0; f < tc.n; ++f) {
        ASSERT_EQ(std::signbit(sample(imgs[f], i)), std::signbit(px[i][f])) << "segno dello zero perso nella fixture";
      }
    }
    const std::optional<master_result> res = combine_or_fail(r, imgs, tc.m.name());
    if (!res) continue;
    std::size_t minus = 0, plus = 0;
    for (std::size_t i = 0; i < px.size(); ++i) {
      const std::vector<double> z = b5o::sorted_total(canonical_samples(g, order, i));
      const double want = tc.m.k == kind::median ? ieee_median(z) : ieee_winsorized(z, tc.m.g);
      EXPECT_EQ(res->output.invalid.bits[i], 0) << i;
      EXPECT_EQ(bits_of(sample(res->output, i)), bits_of(want)) << "pixel " << i << ": atteso " << want;
      if (want == 0) (std::signbit(want) ? minus : plus)++;
    }
    // La fixture produce entrambi i segni attesi, con qualunque ordine canonico dei digest.
    EXPECT_GT(minus, 0u);
    EXPECT_GT(plus, 0u);
  }
}

// ---------------------------------------------------------------- T-M3B-C02-01: casi non dyadic e n = 101

// Insiemi di FIX-SYN-CAL-08 per C02-01 (seed 2026110961), realizzati una volta ciascuno nell'ordine dichiarato.
struct realized_set {
  std::size_t n;
  group g;
};

std::vector<realized_set> c02_01_realized() {
  const sc::gaussian_cases cases = sc::c02_01_cases();
  sc::gauss_stream gs(cases.seed);
  std::vector<realized_set> out;
  for (std::size_t c = 0; c < cases.sets.size(); ++c) {
    const sc::frame_set& s = cases.sets[c];
    const sc::frames x = sc::realize(s, gs);
    group g = make_group(master_role::dark, s.width, s.height, s.frames());
    for (std::size_t f = 0; f < s.frames(); ++f) g.frames[f].v = x[f];
    out.push_back({cases.n[c], std::move(g)});
  }
  return out;
}

std::vector<method> methods_for(std::size_t n) {
  std::vector<double> w(n);
  for (std::size_t f = 0; f < n; ++f) w[f] = 2.0 + static_cast<double>(f % 3);  // 2 max w < W per ogni n ≥ 3
  std::vector<method> out = {weighted_m(w), median_m()};
  for (std::size_t g : {std::size_t{1}, std::size_t{2}, (n - 1) / 2}) {
    if (n < 2 * g + 1) continue;
    bool seen = false;
    for (const method& m : out) seen = seen || (m.k == kind::winsorized && m.g == g);
    if (!seen) out.push_back(winsor_m(g));
  }
  return out;
}

TEST(MasterB5, T_M3B_C02_01_non_dyadic_cases_and_n101_within_rule_i) {
  for (const realized_set& rs : c02_01_realized()) {
    for (const method& m : methods_for(rs.n)) {
      SCOPED_TRACE(m.name() + " n=" + std::to_string(rs.n));
      const case_result cr = run_case(rs.g, m);
      check_case(cr, m.name());
    }
  }
}

TEST(MasterB5, T_M3B_C02_01_n101_has_median_and_winsorized_g_1_2_50) {
  bool found = false;
  for (const realized_set& rs : c02_01_realized()) {
    if (rs.n != 101) continue;
    found = true;
    std::vector<std::size_t> gs;
    for (const method& m : methods_for(rs.n))
      if (m.k == kind::winsorized) gs.push_back(m.g);
    EXPECT_EQ(gs, (std::vector<std::size_t>{1, 2, 50}));
  }
  EXPECT_TRUE(found) << "la fixture di C02-01 non ha il caso n = 101";
}

// Un solo arrotondamento al sample_type: l'uscita f32 è float() dell'uscita f64, bit per bit.
TEST(MasterB5, T_M3B_C02_01_f32_output_is_the_single_rounding_of_the_f64_result) {
  for (const realized_set& rs : c02_01_realized()) {
    for (const method& m : methods_for(rs.n)) {
      SCOPED_TRACE(m.name() + " n=" + std::to_string(rs.n));
      master_recipe r = recipe_of(rs.g);
      apply(m, r);
      r.output_type = sample_type::f64;
      const std::optional<master_result> r64 = combine_or_fail(r, images_of(rs.g), "f64");
      r.output_type = sample_type::f32;
      const std::optional<master_result> r32 = combine_or_fail(r, images_of(rs.g), "f32");
      if (!r64 || !r32) continue;
      ASSERT_EQ(r32->output.type, sample_type::f32);
      for (std::size_t i = 0; i < r64->output.invalid.bits.size(); ++i) {
        ASSERT_EQ(r64->output.invalid.bits[i], 0) << i;
        EXPECT_EQ(std::bit_cast<std::uint32_t>(r32->output.data<float>()[i]),
                  std::bit_cast<std::uint32_t>(static_cast<float>(r64->output.data<double>()[i])))
            << "M al pixel " << i;
        EXPECT_EQ(std::bit_cast<std::uint32_t>(static_cast<float>(plane(r32->output, i))),
                  std::bit_cast<std::uint32_t>(static_cast<float>(plane(r64->output, i))))
            << "V al pixel " << i;
      }
    }
  }
}

// ---------------------------------------------------------------- §b.7: maschere e capability

TEST(MasterB5, T_M3B_C02_01_masks_invalid_and_saturated_samples_invalidate_the_pixel_for_every_method) {
  constexpr std::uint16_t nf = invalid_reason::source_nonfinite, fl = invalid_reason::source_flagged,
                          sat = invalid_reason::saturated;
  for (const method& m : {weighted_m({1, 2, 2}), median_m(), winsor_m(1), single_m(10, 1.5, 2)}) {
    SCOPED_TRACE(m.name());
    const std::size_t n = m.k == kind::single ? 1 : 3;
    // Frame 0 float (NaN), frame 1 intero con INVALID, frame 2 intero (saturazione); per `single` il solo frame 0.
    group g = make_group(master_role::dark, 4, 2, n, 16);
    g.frames[0].bitpix = -32;
    for (std::size_t f = 0; f < n; ++f)
      for (std::size_t i = 0; i < 8; ++i) g.frames[f].v[i] = 100 + static_cast<double>(4 * f + i % 4);
    std::map<std::size_t, std::uint16_t> want;
    g.frames[0].v[2] = kNaN;
    want[2] = nf;
    g.frames[0].v[5] = 900;  // saturazione nel frame 0
    want[5] = sat;
    if (n == 3) {
      g.frames[1].invalid = std::vector<std::uint16_t>(8, 0);
      g.frames[0].v[3] = kNaN;  // tre motivi da tre frame
      (*g.frames[1].invalid)[3] = fl;
      g.frames[2].v[3] = 950;
      want[3] = nf | fl | sat;
      (*g.frames[1].invalid)[6] = fl;  // flag e saturazione nello stesso frame
      g.frames[1].v[6] = 900;
      want[6] = fl | sat;
    }
    master_recipe r = recipe_of(g);
    r.saturation_adu = 900;
    r.saturated_fraction_max = 0.5;
    apply(m, r);
    const std::vector<std::size_t> order = canonical(r);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), m.name());
    if (!res) continue;
    const std::vector<double> wc = m.k == kind::weighted ? canonical_weights(m, order) : std::vector<double>{};
    for (std::size_t i = 0; i < 8; ++i) {
      if (want.count(i)) {
        EXPECT_EQ(res->output.invalid.bits[i], want[i]) << i;
        EXPECT_TRUE(std::isnan(sample(res->output, i)) && std::isnan(plane(res->output, i))) << i;
        continue;
      }
      // Pixel valido: tutti gli n frame entrano nel calcolo.
      expect_pixel(*res, i, expect_of(m, canonical_samples(g, order, i), wc, kappa_hat(m, n)), "pixel " + std::to_string(i));
    }
    EXPECT_TRUE(operator_output_invariants_hold(res->output));
  }
}

TEST(MasterB5, T_M3B_C02_01_capability_measure_is_the_merge_of_the_frames_without_own_codes) {
  using ml = measure_limit;
  for (const method& m : {weighted_m({1, 2, 2, 2}), median_m(), winsor_m(1), single_m(10, 1.5, 2)}) {
    SCOPED_TRACE(m.name());
    const std::size_t n = m.k == kind::single ? 1 : 4;
    group g = make_group(master_role::dark, 4, 2, n);
    for (std::size_t f = 0; f < n; ++f)
      for (std::size_t i = 0; i < 8; ++i) g.frames[f].v[i] = 200 + static_cast<double>(f + i);
    std::vector<image> frames = images_of(g);
    const std::vector<std::vector<ml>> codes = {{ml::calib_offset_unknown, ml::samples_interpolated},
                                                {ml::calib_omitted_bias_subtraction, ml::samples_interpolated},
                                                {ml::calib_offset_unknown, ml::calib_assumption_decoder_black},
                                                {}};
    std::vector<const image*> ptrs;
    for (std::size_t f = 0; f < n; ++f) {
      frames[f].measure = measure_capability{codes[f]};
      ptrs.push_back(&frames[f]);
    }
    const measure_capability expected = merge_measure(ptrs, {});
    master_recipe r = recipe_of(g);
    apply(m, r);
    const std::optional<master_result> res = combine_or_fail(r, frames, m.name());
    if (!res) continue;
    ASSERT_TRUE(res->output.measure.has_value());
    EXPECT_EQ(*res->output.measure, expected);
    const auto& lim = res->output.measure->limits;
    EXPECT_EQ(std::count(lim.begin(), lim.end(), ml::variance_fixed_weight_approximation), 0);
  }
}

// ---------------------------------------------------------------- T-M3B-C02-13: `single`

TEST(MasterB5, T_M3B_C02_13_single_plane_against_the_noise_model_below_at_and_above_the_offset) {
  struct single_case {
    std::string name;
    method m;
    std::vector<double> x;
    bool exact;
    std::optional<double> header_offset, header_egain;  // assenti: header uguale alla ricetta
  };
  const std::vector<single_case> cases = {
      // dyadic: x − offset intero, G = 2, R = 3: ogni operazione esatta.
      {"dyadic", single_m(10, 2, 3), {4, 10, 13, 16, 10.5, 1010, 9.75, 11}, true, std::nullopt, std::nullopt},
      // non dyadic: G = 1.3, R = 2.7.
      {"non dyadic", single_m(10, 1.3, 2.7), {4.1, 10, 13.3, 1017.77, 9.999, 10.0000001, 512.25, 3}, false, std::nullopt, std::nullopt},
      // offset negativo, R = 0.
      {"offset negativo, R = 0", single_m(-5, 1.5, 0), {-6, -5, -4.5, 0, 7, 100, -5.25, 1}, false, std::nullopt, std::nullopt},
      // Header diverso dalla ricetta (OFFSET 20, EGAIN 4): il modello di rumore usa i parametri dichiarati nella
      // ricetta (offset 10, G 2, R 3). I pixel fra 10 e 20 distinguono i due offset.
      {"header diverso dalla ricetta", single_m(10, 2, 3), {4, 10, 13, 16, 19, 20, 24, 1010}, true, 20.0, 4.0},
  };
  for (const single_case& sc_ : cases) {
    SCOPED_TRACE(sc_.name);
    std::vector<std::vector<double>> px;
    for (double v : sc_.x) px.push_back({v});
    group g = group_of(px, 8, 1);
    // OFFSET ed EGAIN dell'header: uguali alla ricetta salvo i casi che li dichiarano diversi; l'attesa segue sempre
    // la ricetta.
    g.frames[0].hdr.offset = sc_.header_offset.value_or(sc_.m.offset);
    g.frames[0].hdr.egain = sc_.header_egain.value_or(sc_.m.gain);
    const case_result cr = run_case(g, sc_.m);
    bool all_exact = true;
    for (const expectation& e : cr.exp) all_exact = all_exact && e.ex.exact_v;
    if (sc_.exact) {
      ASSERT_TRUE(all_exact) << "caso dichiarato dyadic non esatto";
    }
    check_case(cr, sc_.name);
    if (!cr.res) continue;
    const json& d = cal::at(cr.res->report, {"descriptor"});
    EXPECT_EQ(cal::at(d, {"combine_method"}).as_string(), "single");
    EXPECT_EQ(cal::at(d, {"variance_estimator"}).as_string(), "noise_model");
    EXPECT_EQ(cal::at(d, {"combine_count"}).as_integer(), 1);
    EXPECT_EQ(cal::at(d, {"rejection"}).as_string(), "none");
  }
}

TEST(MasterB5, T_M3B_C02_13_single_parameters_absent_one_at_a_time_stop_combine_with_var_source) {
  for (const char* field : {"offset_adu", "conversion_gain_e_per_adu", "read_noise_adu"}) {
    SCOPED_TRACE(field);
    group g = group_of({{20}, {21}}, 2, 1);
    master_recipe r = recipe_of(g);
    apply(single_m(10, 1.5, 2), r);
    if (std::string(field) == "offset_adu") r.offset_adu.reset();
    if (std::string(field) == "conversion_gain_e_per_adu") r.conversion_gain_e_per_adu.reset();
    if (std::string(field) == "read_noise_adu") r.read_noise_adu.reset();
    EXPECT_STOP(combine_master(r, images_of(g)), error_category::metadata_missing, (std::string("var.source.") + field).c_str());
  }
}

// ---------------------------------------------------------------- flat `single` e campi del report di B5

// Flat con `single` (C-02.b §b.2): M e V del modello di rumore; level_bias_estimate non definito (n = 1), null con
// motivo `single_frame` nel report e NaN per pixel.
TEST(MasterB5, T_M3B_C02_13_flat_single_plane_and_level_bias_estimate_null_single_frame) {
  const method m = single_m(10, 2, 3);
  group g = make_group(master_role::flat, 8, 1, 1);
  const std::vector<double> x = {2010, 2012, 2014, 2016, 2018, 2020, 2022, 2024};
  g.frames[0].v = x;
  g.frames[0].hdr.offset = m.offset;
  g.frames[0].hdr.egain = m.gain;
  master_recipe r = recipe_of(g, 0.25);
  apply(m, r);
  const std::optional<master_result> res = combine_or_fail(r, images_of(g), "flat single");
  if (!res) return;
  ASSERT_EQ(res->level_bias_estimate.size(), x.size());
  ASSERT_EQ(cal::at(res->report, {"channels"}).items().size(), 1u) << "flat mono: un solo canale";
  for (std::size_t i = 0; i < x.size(); ++i) {
    expect_pixel(*res, i, expect_of(m, {x[i]}, {}, 0), "pixel " + std::to_string(i));
  }
  for (double v : res->level_bias_estimate) EXPECT_TRUE(std::isnan(v));
  for (const json& ch : cal::at(res->report, {"channels"}).items()) {
    const json& lb = cal::at(ch, {"level_bias_estimate"});
    EXPECT_EQ(cal::at(lb, {"median"}).type(), json::kind::null);
    EXPECT_EQ(cal::at(lb, {"max"}).type(), json::kind::null);
    EXPECT_EQ(cal::at(lb, {"null_reason"}).as_string(), "single_frame");
  }
  EXPECT_EQ(cal::at(res->report, {"variance", "variance_estimator"}).as_string(), "noise_model");
}

// Sezione `variance` del report (C-02.b §b.6, §b.7): `variance_estimator` per metodo; CV gaussiano omoschedastico
// sqrt(2/(n − 1)) per `median` e `winsorized`, null con motivo per `single` (`single_frame`) e `weighted_mean`
// (`not_derived`). Di `plane` si controlla solo la presenza: i valori si fissano in B11.
TEST(MasterB5, T_M3B_C02_13_report_variance_section_per_method) {
  struct report_case {
    method m;
    std::size_t n;
    const char* estimator;
    const char* cv_null;  // nullptr: CV numerico
  };
  const std::vector<report_case> cases = {
      {single_m(10, 1.5, 2), 1, "noise_model", "single_frame"},
      {weighted_m({1, 2, 2, 2}), 4, "empirical_weighted", "not_derived"},
      {median_m(), 4, "gaussian_reference", nullptr},
      {winsor_m(1), 4, "gaussian_reference", nullptr},
  };
  for (const report_case& c : cases) {
    SCOPED_TRACE(c.m.name());
    std::vector<std::vector<double>> px(8, std::vector<double>(c.n));
    for (std::size_t i = 0; i < 8; ++i)
      for (std::size_t f = 0; f < c.n; ++f) px[i][f] = 300 + static_cast<double>((3 * i + 5 * f) % 11);
    const group g = group_of(px, 8, 1);
    master_recipe r = recipe_of(g);
    apply(c.m, r);
    const std::optional<master_result> res = combine_or_fail(r, images_of(g), c.m.name());
    if (!res) continue;
    const json& var = cal::at(res->report, {"variance"});
    EXPECT_EQ(cal::at(var, {"variance_estimator"}).as_string(), c.estimator);
    EXPECT_EQ(cal::at(res->report, {"descriptor", "variance_estimator"}).as_string(), c.estimator);
    EXPECT_NE(var.find("plane"), nullptr) << "campo plane assente";
    const json& cv = cal::at(var, {"variance_cv_gaussian_homoscedastic"});
    if (c.cv_null) {
      EXPECT_EQ(cv.type(), json::kind::null);
      EXPECT_EQ(cal::at(var, {"variance_cv_null_reason"}).as_string(), c.cv_null);
    } else {
      EXPECT_EQ(cv.as_number(), std::sqrt(2.0 / static_cast<double>(c.n - 1)));
    }
  }
}

// ---------------------------------------------------------------- T-M3B-C02-15: result_nonfinite

// Pixel il cui prodotto o quoziente con operandi non nulli dà zero o un subnormale: `result_nonfinite`, campione e
// piano NaN; gli altri pixel restano quelli del metodo scelto (nessun cambio di metodo).
TEST(MasterB5, T_M3B_C02_15_result_nonfinite_on_zero_or_subnormal_products_and_quotients) {
  const double t1022 = 0x1p-1022, t540 = 0x1p-540, t511 = 0x1p-511, t600 = 0x1p-600;
  struct nf_case {
    std::string name;
    method m;
    std::vector<std::vector<double>> px;
    std::vector<bool> nonfinite;  // atteso per pixel
    sample_type out = sample_type::f64;
  };
  const double w10 = 0x1p10;
  const std::vector<nf_case> cases = {
      {"weighted: / W subnormale, (x − M)² subnormale, w · x subnormale, pixel normale",
       weighted_m({w10, w10, w10, w10}),
       {{t1022, 0, 0, 0}, {t540, 0, 0, 0}, {0x1p-1065, 1, 2, 3}, {1, 2, 3, 4}},
       {true, true, true, false}},
      {"median n=4: (z_k + z_(k+1)) / 2 subnormale",
       median_m(),
       {{0, 0, t1022, 5}, {1, 2, 3, 3}},
       {true, false}},
      {"median n=4: z_k + z_(k+1) = ±2⁻¹⁰⁷⁴, metà arrotondata a zero",
       median_m(),
       {{-1, 0, 0x1p-1074, 7}, {-7, -0x1p-1074, 0, 1}, {1, 2, 3, 3}},
       {true, true, false}},
      {"median n=3: (x − x̄)² nullo, κ · s² subnormale",
       median_m(),
       {{t600, 0, 0}, {-t511, 0, t511}, {1, 2, 3}},
       {true, true, false}},
      {"winsorized n=4 g=1: Σ v / n subnormale",
       winsor_m(1),
       {{0, 0, t1022, 9}, {0, 2, 3, 3}},
       {true, false}},
      {"single: / G subnormale sopra l'offset, valido sotto",
       single_m(10, 0x1.8p1023, 2),
       {{11}, {9}, {10}, {12}},
       {true, false, false, true}},
      {"single: R² nullo", single_m(10, 1.5, t600), {{11}, {9}}, {true, true}},
      // Campione subnormale d'ingresso scelto senza operazione: valido con uscita f64 (decisione del coordinatore in B5),
      // result_nonfinite con uscita f32 (conversione al sample_type di un valore non nullo, C-02.a §6.3).
      {"single uscita f64: campione subnormale scelto, valido", single_m(10, 1.5, 2), {{0x1p-1070}, {20}}, {false, false}},
      {"single uscita f32: campione subnormale scelto, result_nonfinite",
       single_m(10, 1.5, 2),
       {{0x1p-1070}, {20}},
       {true, false},
       sample_type::f32},
      {"median n=3 uscita f64: z_2 subnormale scelto, valido", median_m(), {{0x1p-1070, 0x1p-1070, 5}, {1, 2, 3}}, {false, false}},
      {"median n=3 uscita f32: z_2 subnormale scelto, result_nonfinite",
       median_m(),
       {{0x1p-1070, 0x1p-1070, 5}, {1, 2, 3}},
       {true, false},
       sample_type::f32},
      {"median n=3 uscita f32: M normale in f64 e subnormale in f32",
       median_m(),
       {{0x1p-130, 0x1p-130, 0x1p-130}, {1, 2, 3}},
       {true, false},
       sample_type::f32},
  };
  for (const nf_case& c : cases) {
    SCOPED_TRACE(c.name);
    const group g = group_of(c.px, static_cast<std::uint32_t>(c.px.size()), 1);
    const case_result cr = run_case(g, c.m, c.out);
    for (std::size_t i = 0; i < c.px.size(); ++i) {
      // L'oracle esatto concorda con la fixture (il caso f32 riguarda la conversione: l'oracle f64 non lo vede).
      if (c.out == sample_type::f64) {
        ASSERT_EQ(cr.exp[i].ex.nonfinite, c.nonfinite[i]) << "fixture, pixel " << i;
      }
    }
    if (!cr.res) continue;
    for (std::size_t i = 0; i < c.px.size(); ++i) {
      if (c.nonfinite[i]) {
        EXPECT_EQ(cr.res->output.invalid.bits[i], invalid_reason::result_nonfinite) << i;
        EXPECT_TRUE(std::isnan(sample(cr.res->output, i)) && std::isnan(plane(cr.res->output, i))) << i;
      } else {
        expect_pixel(*cr.res, i, cr.exp[i], "pixel " + std::to_string(i));
      }
    }
    EXPECT_EQ(cal::at(cr.res->report, {"descriptor", "combine_method"}).as_string(),
              c.m.k == kind::weighted ? "weighted_mean" : c.m.k == kind::single ? "single" : c.m.k == kind::median ? "median" : "winsorized");
  }
}

// ---------------------------------------------------------------- T-M3B-C02-15: master.param.weights

struct weight_case {
  std::string name;
  std::vector<double> w;
};

// Pesi finiti, > 0 e con 2 max w < W in aritmetica esatta, ma con costanti del nodo non normali, non finite o ≤ 0,
// compreso 1 − 2 · a_f ≤ 0 (C-02.b §b.6, «Rappresentabilità»).
std::vector<weight_case> unrepresentable_weight_cases() {
  return {
      {"a_f subnormale", {0x1p-60, 0x1p1000, 0x1p1000, 0x1p1000}},
      {"c_f nullo (a_f · a_f in underflow)", {0x1p-700, 1, 1, 1}},
      {"c_f subnormale", {0x1.8p-519, 1, 1, 1}},
      // W = 2⁵⁴ in f64 (2⁵⁴ + 2 arrotondato), a = 1/2 esatto per il peso 2⁵³: 1 − 2a = 0, T non finito.
      {"T non finito (1 − 2 a_f = 0 in f64)", {0x1p53, 0x1p52, 0x1p52 + 2}},
      // Dominio esatto valido (2⁵³ < 2⁵³ + 1/2), ma W arrotondato in f64 dà a_f ≥ 1/2 per il peso 2⁵³: lo stop atteso
      // è su 1 − 2 · a_f ≤ 0 (senza, T e c_f sarebbero non positivi o non finiti secondo l'ordine canonico).
      {"a_f ≥ 1/2 in f64 con dominio esatto valido", {0x1p53, 0x1p52 + 1, 0x1p51 + 1, 0x1p51 - 1.5}},
  };
}

bool numeric_context(const stop_error& e) {
  for (const auto& [k, v] : e.context()) {
    char* end = nullptr;
    (void)std::strtod(v.c_str(), &end);
    if (!v.empty() && end != nullptr && *end == '\0') return true;
  }
  return false;
}

TEST(MasterB5, T_M3B_C02_15_weight_constants_not_normal_not_finite_or_not_positive_stop_with_param_weights) {
  for (const weight_case& wc : unrepresentable_weight_cases()) {
    SCOPED_TRACE(wc.name);
    // Precondizione: il dominio esatto 2 max w < W vale; la costante è non normale per costruzione.
    exact::rat W = exact::from_u(0), mx = exact::from_u(0);
    for (double x : wc.w) {
      W = W + exact::from_double(x);
      if (mx < exact::from_double(x)) mx = exact::from_double(x);
    }
    ASSERT_TRUE(exact::from_u(2) * mx < W);
    const std::size_t n = wc.w.size();
    std::vector<std::vector<double>> px(4, std::vector<double>(n));
    for (std::size_t i = 0; i < 4; ++i)
      for (std::size_t f = 0; f < n; ++f) px[i][f] = 100 + static_cast<double>(i + f);
    const group g = group_of(px, 4, 1);
    // Ogni assegnazione dei pesi ai frame, quindi ogni ordine canonico delle somme di W e T.
    std::vector<std::size_t> perm(n);
    std::iota(perm.begin(), perm.end(), std::size_t{0});
    do {
      std::vector<double> w(n);
      std::string tag;
      for (std::size_t f = 0; f < n; ++f) {
        w[f] = wc.w[perm[f]];
        tag += std::to_string(perm[f]);
      }
      SCOPED_TRACE("assegnazione " + tag);
      master_recipe r = recipe_of(g);
      apply(weighted_m(w), r);
      try {
        (void)combine_master(r, images_of(g));
        ADD_FAILURE() << "nessuno stop";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
        EXPECT_EQ(e.code(), "master.param.weights") << e.what();
        EXPECT_TRUE(numeric_context(e)) << "il contesto non porta il valore: " << e.what();
      }
    } while (std::next_permutation(perm.begin(), perm.end()));
  }
}

// ---------------------------------------------------------------- T-M3B-C02-15: percorso del job

struct job_probe {
  std::optional<std::string> setup_error;
  std::optional<std::string> code;
  std::optional<error_category> category;
  int faults = 0;
  bool frames_read = false;
  bool primary = false;
  std::optional<image> master;  // master pubblicato, riletto dall'artefatto
};

// Progetto col nodo della ricetta; il job gira con la stessa ricetta (o con `job_recipe`). Il contatore del
// fault_injector dello store conta ogni punto di protocollo dopo l'allestimento.
job_probe probe_job(const fs::path& root, const master_recipe& node_recipe, const master_recipe& job_recipe) {
  job_probe out;
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
  o.hook = [&out](std::string_view point) {
    if (point == "frames.read") out.frames_read = true;
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
  const auto roles = by_role(*store, node->key());
  out.primary = roles.count("primary") != 0;
  if (out.primary) out.master = decode_fits(read_artifact(*store, roles.at("primary")), cal::big_budget);
  return out;
}

master_recipe on_disk(const group& g, const method& m, const fs::path& dir, const std::string& prefix) {
  master_recipe r = recipe_of(g);
  write_frames(g, r, dir, prefix);
  apply(m, r);
  return r;
}

// Costanti dei pesi non rappresentabili: stop prima di ogni lettura, nessun artefatto, nessun lavoro sullo store
// oltre lo stop di parametro di riferimento. Vincolo assunto dal piano (B5, «costanti dei pesi nel preflight»): il nodo
// si costruisce, le costanti si calcolano nel preflight del job.
TEST(MasterB5, T_M3B_C02_15_weight_constant_stop_in_the_job_reads_nothing_and_does_no_extra_store_work) {
  const fs::path dir = fresh_dir("b5-t15-weights");
  std::vector<std::vector<double>> px(32, std::vector<double>(4));
  for (std::size_t i = 0; i < 32; ++i)
    for (std::size_t f = 0; f < 4; ++f) px[i][f] = 500 + static_cast<double>((i + f) % 7);
  const group g = group_of(px, 8, 4);
  const master_recipe good = on_disk(g, weighted_m({1, 2, 2, 2}), dir, "w");

  master_recipe base_stop = good;
  base_stop.combine.reset();
  const job_probe baseline = probe_job(dir / "store-baseline", good, base_stop);
  ASSERT_FALSE(baseline.setup_error.has_value()) << *baseline.setup_error;
  ASSERT_EQ(baseline.code, std::optional<std::string>("master.param.combine"));
  EXPECT_FALSE(baseline.frames_read);

  // Controllo positivo: la stessa ricetta con pesi rappresentabili legge, pubblica e lavora sullo store.
  const job_probe control = probe_job(dir / "store-control", good, good);
  ASSERT_FALSE(control.setup_error.has_value()) << *control.setup_error;
  EXPECT_FALSE(control.code.has_value()) << control.code.value_or("");
  EXPECT_TRUE(control.frames_read);
  EXPECT_TRUE(control.primary);
  EXPECT_GT(control.faults, baseline.faults) << "contatore del fault_injector non collegato";

  std::size_t k = 0;
  for (const weight_case& wc : unrepresentable_weight_cases()) {
    if (wc.w.size() != 4) continue;
    SCOPED_TRACE(wc.name);
    master_recipe bad = good;
    for (std::size_t f = 0; f < 4; ++f) bad.frames[f].weight = wc.w[f];
    const job_probe p = probe_job(dir / ("store-" + std::to_string(k++)), bad, bad);
    ASSERT_FALSE(p.setup_error.has_value()) << "nodo non creabile: " << *p.setup_error;
    EXPECT_EQ(p.code, std::optional<std::string>("master.param.weights"));
    EXPECT_EQ(p.category, std::optional<error_category>(error_category::invalid_input));
    EXPECT_FALSE(p.frames_read);
    EXPECT_FALSE(p.primary);
    EXPECT_EQ(p.faults, baseline.faults);
  }
}

// Un pixel `result_nonfinite` non cambia percorso: il job finisce e pubblica con lo stesso lavoro sullo store di un
// job con valori ordinari.
TEST(MasterB5, T_M3B_C02_15_result_nonfinite_pixel_in_the_job_takes_no_alternative_path) {
  const fs::path dir = fresh_dir("b5-t15-nonfinite");
  std::vector<std::vector<double>> ordinary(32, std::vector<double>(3)), tiny = ordinary;
  for (std::size_t i = 0; i < 32; ++i)
    for (std::size_t f = 0; f < 3; ++f) ordinary[i][f] = 500 + static_cast<double>((i + f) % 7);
  tiny = ordinary;
  tiny[5] = {-0x1p-511, 0, 0x1p-511};  // κ · s² subnormale
  for (const method& m : {median_m(), winsor_m(1)}) {
    SCOPED_TRACE(m.name());
    const std::string tag = m.k == kind::median ? "med" : "win";
    const master_recipe a = on_disk(group_of(ordinary, 8, 4), m, dir, tag + "-ord");
    const master_recipe b = on_disk(group_of(tiny, 8, 4), m, dir, tag + "-tiny");
    const job_probe pa = probe_job(dir / ("store-" + tag + "-ord"), a, a);
    const job_probe pb = probe_job(dir / ("store-" + tag + "-tiny"), b, b);
    ASSERT_FALSE(pa.setup_error.has_value()) << *pa.setup_error;
    ASSERT_FALSE(pb.setup_error.has_value()) << *pb.setup_error;
    EXPECT_FALSE(pa.code.has_value()) << pa.code.value_or("");
    EXPECT_FALSE(pb.code.has_value()) << pb.code.value_or("");
    EXPECT_TRUE(pa.primary);
    EXPECT_TRUE(pb.primary);
    EXPECT_EQ(pb.faults, pa.faults);
    // Nell'artefatto pubblicato il pixel 5 è result_nonfinite, gli altri sono validi.
    ASSERT_TRUE(pb.master.has_value());
    for (std::size_t i = 0; i < pb.master->invalid.bits.size(); ++i) {
      EXPECT_EQ(pb.master->invalid.bits[i], i == 5 ? invalid_reason::result_nonfinite : 0) << "pixel " << i;
    }
    EXPECT_TRUE(std::isnan(sample(*pb.master, 5)));
  }
}

// ---------------------------------------------------------------- T-M3B-C02-07: caratterizzazione

TEST(MasterB5, T_M3B_C02_07_quantized_median_variance_ratio_characterization) {
  const std::vector<double> sigmas = {0.5, 1, 1.5, 2, 3, 5};
  const std::vector<std::uint32_t> ns = {3, 9, 101};
  const std::vector<double> fracs = {0, 0.25, 0.5};
  ld worst = 0;
  std::ostringstream table;
  table << "T-M3B-C02-07 sigma n phi Var(M)/E[V] err_rel\n";
  for (std::uint32_t n : ns) {
    const kappa_oracle::result ko = kappa_oracle::median(n);
    ASSERT_LE(ko.est_rel, 1e-11L) << "oracle di κ, n = " << n;
    for (double s : sigmas) {
      for (double f : fracs) {
        const auto L40 = static_cast<std::int64_t>(std::ceil(40 * s)) + 1;
        const auto L60 = static_cast<std::int64_t>(std::ceil(60 * s)) + 1;
        const auto a = b5o::discrete_median<ld>(s, n, f, L40);
        const auto b = b5o::discrete_median<ld>(s, n, f, L60);
        const auto d = b5o::discrete_median<double>(s, n, f, L40);
        const ld ratio = a.var_median / (ko.kappa * a.var_sample);
        const ld ratio60 = b.var_median / (ko.kappa * b.var_sample);
        const ld ratio_d = static_cast<ld>(d.var_median / (static_cast<double>(ko.kappa) * d.var_sample));
        const ld err = std::max(std::fabs(ratio - ratio60), std::fabs(ratio - ratio_d)) / std::fabs(ratio);
        worst = std::max(worst, err);
        EXPECT_LE(err, 1e-12L) << "errore della distribuzione discreta, sigma " << s << " n " << n << " phi " << f;
        table << "T-M3B-C02-07 " << s << " " << n << " " << f << " " << std::setprecision(15) << ratio << " "
              << std::setprecision(3) << err << "\n";
      }
    }
  }
  std::cout << table.str() << "T-M3B-C02-07 errore massimo " << worst << "\n";
  ::testing::Test::RecordProperty("C02_07_worst_error", std::to_string(static_cast<double>(worst)));
}

// Parte binaria di C02-07 (`continuous_samples` fra le ipotesi non verificate del report): passa a B11 (report).

// ---------------------------------------------------------------- statistiche: punti derivati (protocollo §5)


// κ e μ_4 dell'oracle (σ = 1) per le celle di T-M3B-C02-04 e verifica h < δ/2 sulle celle di (b) con K e P del
// protocollo §3.2. Valori dichiarati nell'intestazione del file.
struct c0204_cell {
  kind k;
  std::uint32_t n, g;
  bool in_b;  // la cella entra in (b)
};

std::vector<c0204_cell> c0204_cells(std::uint32_t n) {
  std::vector<c0204_cell> out = {{kind::median, n, 0, n != 101}};
  for (std::uint32_t g : {1u, 2u, (n - 1) / 2}) {
    if (n < 2 * g + 1) continue;
    bool seen = false;
    for (const c0204_cell& c : out) seen = seen || (c.k == kind::winsorized && c.g == g);
    if (!seen) out.push_back({kind::winsorized, n, g, !(n == 101 && g == 50)});
  }
  return out;
}

constexpr std::uint32_t kC0204N[] = {3, 4, 9, 10, 101};
constexpr std::size_t kC0204M = 30;

b5o::moment_result oracle_moments(const c0204_cell& c) {
  return c.k == kind::median ? b5o::moments_median(c.n) : b5o::moments_winsorized(c.n, c.g);
}

// h = sqrt(m (μ_4/κ² − 1)/(α K P)), δ = |κ − π/(2n)|/κ.
struct h_delta {
  ld h, delta;
};
h_delta h_delta_of(const b5o::moment_result& mo, std::uint32_t n, std::size_t KP) {
  const ld pi = 3.141592653589793238462643383279502884L;
  const ld h = std::sqrt(static_cast<ld>(kC0204M) * (mo.mu4 / (mo.kappa * mo.kappa) - 1) / (kAlpha * static_cast<ld>(KP)));
  const ld delta = std::fabs(mo.kappa - pi / (2 * static_cast<ld>(n))) / mo.kappa;
  return {h, delta};
}

TEST(MasterB5, T_M3B_C02_04_derived_kappa_mu4_and_h_below_half_delta_before_measurement) {
  // Controlli propri dell'oracle di μ_4: media (g = 0) con κ = 1/n e μ_4 = 3/n²; κ uguale a kappa_oracle.
  for (std::uint32_t n : {2u, 5u}) {
    const b5o::moment_result mean = b5o::moments_dirichlet(n, 0);
    EXPECT_NEAR(static_cast<double>(mean.kappa * n), 1.0, 1e-12) << n;
    EXPECT_NEAR(static_cast<double>(mean.mu4 * n * n / 3), 1.0, 1e-12) << n;
  }
  for (std::uint32_t n : kC0204N) {
    const auto [seed, K] = sc::c02_04_seed_K(n);
    (void)seed;
    const std::size_t KP = K * 4096;
    for (const c0204_cell& c : c0204_cells(n)) {
      const b5o::moment_result mo = oracle_moments(c);
      const kappa_oracle::result ko = c.k == kind::median ? kappa_oracle::median(n) : kappa_oracle::winsorized_or_mean(n, c.g);
      EXPECT_NEAR(static_cast<double>(mo.kappa / ko.kappa), 1.0, 1e-12) << "n " << n << " g " << c.g;
      EXPECT_LE(mo.est_kappa, 1e-11L) << "n " << n << " g " << c.g;
      EXPECT_LE(mo.est_mu4, 1e-11L) << "n " << n << " g " << c.g;
      const h_delta hd = h_delta_of(mo, n, KP);
      std::cout << "T-M3B-C02-04 " << (c.k == kind::median ? "median" : "winsorized") << " n " << n << " g " << c.g
                << std::setprecision(13) << " kappa " << static_cast<double>(mo.kappa) << " mu4 "
                << static_cast<double>(mo.mu4) << std::setprecision(5) << " est " << static_cast<double>(mo.est_kappa)
                << " " << static_cast<double>(mo.est_mu4) << " h " << static_cast<double>(hd.h) << " delta/2 "
                << static_cast<double>(hd.delta / 2) << " h/(delta/2) " << static_cast<double>(hd.h / (hd.delta / 2))
                << (c.in_b ? "" : " (fuori da (b))") << "\n";
      if (c.in_b) {
        EXPECT_LT(hd.h, hd.delta / 2) << "regola su K·P violata: n " << n << " g " << c.g;
      }
    }
  }
}

// ---------------------------------------------------------------- statistiche: contenitore delle realizzazioni

// Il più grande divisore di K con n · R ≤ 1000 (memoria per chiamata ≤ circa 32 MB di campioni).
std::size_t batch_of(std::size_t K, std::size_t n) {
  for (std::size_t r = std::min<std::size_t>(K, 1000 / n); r >= 1; --r)
    if (K % r == 0) return r;
  return 1;
}

struct batch_harness {
  std::size_t n = 0, R = 0, P = 4096;
  group tpl_group;
  std::vector<image> tpl;
  master_recipe recipe;
  std::vector<std::size_t> order;
  std::vector<std::vector<double>> samples;  // [frame][r · P + p], indice di generazione

  batch_harness(std::size_t n_, std::size_t R_) : n(n_), R(R_) {
    tpl_group = make_group(master_role::dark, 64, static_cast<std::uint32_t>(64 * R), n);
    tpl = images_of(tpl_group);
    recipe = recipe_of(tpl_group);
    recipe.output_type = sample_type::f64;
    order = canonical(recipe);
    samples.assign(n, std::vector<double>(R * P));
  }

  // R realizzazioni consecutive del flusso: realizzazione, frame (indice di generazione), pixel in ordine di riga.
  void realize(const sc::frame_set& set, sc::gauss_stream& gs, sc::frames& buf) {
    for (std::size_t r = 0; r < R; ++r) {
      sc::realize_into(set, gs, buf);
      for (std::size_t f = 0; f < n; ++f) std::copy(buf[f].begin(), buf[f].end(), samples[f].begin() + static_cast<std::ptrdiff_t>(r * P));
    }
  }

  std::optional<master_result> run(const method& m) {
    std::vector<image> imgs = tpl;
    for (std::size_t f = 0; f < n; ++f) std::memcpy(imgs[f].data<double>(), samples[f].data(), samples[f].size() * sizeof(double));
    master_recipe r = recipe;
    apply(m, r);
    return combine_or_fail(r, std::move(imgs), m.name());
  }

  // Campioni del pixel i della pila nell'ordine canonico.
  void canonical_at(std::size_t i, double* x) const {
    for (std::size_t c = 0; c < n; ++c) x[c] = samples[order[c]][i];
  }
};

// ρ_c dichiarati prima della misura: regola I letterale in razionali esatti sui c_f, massimo su tutti gli ordini
// canonici possibili dei pesi, quindi funzione dei soli pesi (W1: 120 ordini, minimo 7.330382714e-15; W2: 4 ordini,
// minimo 1.344223087e-14).
constexpr double kRhoC1 = 9.39061934e-15;   // W1
constexpr double kRhoC2 = 1.526875334e-14;  // W2

ld rho_c_all_orders(std::vector<double> w) {
  std::sort(w.begin(), w.end());
  ld rho = 0;
  do {
    rho = std::max(rho, b5o::rho_c_of(w).rho);
  } while (std::next_permutation(w.begin(), w.end()));
  return rho;
}

TEST(MasterB5, T_M3B_C02_03_derived_rho_c_before_measurement) {
  for (int which : {1, 2}) {
    const sc::weighted_config cfg = sc::c02_03_config(which);
    const ld rho = rho_c_all_orders(std::vector<double>(cfg.w.begin(), cfg.w.end()));
    std::cout << "T-M3B-C02-03 W" << which << " rho_c " << std::setprecision(10) << static_cast<double>(rho) << "\n";
    const double declared = which == 1 ? kRhoC1 : kRhoC2;
    EXPECT_NEAR(static_cast<double>(rho), declared, 1e-6 * declared) << "W" << which;
  }
}

// τ_k = ampiezza dell'intervallo della regola I, che non dipende dal valore del prodotto; `outside` conta i valori del
// prodotto fuori dal proprio intervallo (criterio binario: deve essere 0, protocollo §1).
struct tau_max {
  ld v = 0;
  std::size_t outside = 0;
  void add(const b5o::ivl& i, double got) {
    const ld x = got;
    v = std::max(v, i.hi - i.lo);
    if (!(x >= i.lo && x <= i.hi)) ++outside;
  }
};

double seconds_since(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// ---------------------------------------------------------------- T-M3B-C02-03 (protocollo §3.1)

void run_c02_03(int which) {
  const auto t0 = std::chrono::steady_clock::now();
  const sc::weighted_config cfg = sc::c02_03_config(which);
  const sc::frame_set set = sc::c02_03_set(which);
  const std::size_t n = set.frames(), K = 2000, P = set.pixels(), m = 4;
  ASSERT_EQ(P, 4096u);
  std::vector<exact::rat> wr, var;
  std::vector<double> w;
  for (std::size_t f = 0; f < n; ++f) {
    w.push_back(static_cast<double>(cfg.w[f]));
    wr.push_back(sc::q(cfg.w[f]));
    var.push_back(sc::q(cfg.sigma[f] * cfg.sigma[f]));
  }
  const sc::weighted_truth truth = sc::weighted_truth_of(wr, var);
  const ld E = sc::approx(truth.var_m), varX = sc::approx(truth.var_v);
  const method meth = weighted_m(w);
  const std::size_t R = batch_of(K, n);
  batch_harness bh(n, R);
  const std::vector<double> wc = canonical_weights(meth, bh.order);
  // ρ_c dichiarato prima della misura, ricalcolato.
  const ld rho = rho_c_all_orders(w);
  ASSERT_NEAR(static_cast<double>(rho), which == 1 ? kRhoC1 : kRhoC2, 1e-6 * (which == 1 ? kRhoC1 : kRhoC2));

  const b5o::weight_intervals kint = b5o::weight_constant_intervals(wc);
  ASSERT_FALSE(kint.excluded);
  sc::gauss_stream gs(cfg.seed);
  sc::frames buf;
  ld sumV = 0;
  std::size_t NV = 0, excluded = 0;
  tau_max tV, tM;
  std::vector<ld> sy(P, 0), syy(P, 0);
  std::vector<std::size_t> kp(P, 0);
  std::vector<double> x(n);
  for (std::size_t b = 0; b < K / R; ++b) {
    bh.realize(set, gs, buf);
    const std::optional<master_result> res = bh.run(meth);
    if (!res) return;
    for (std::size_t i = 0; i < R * P; ++i) {
      const std::size_t p = i % P;
      bh.canonical_at(i, x.data());
      b5o::rule_i ri;
      const b5o::pixel_intervals pi = b5o::weighted_intervals(ri, x.data(), n, wc, kint);
      if (ri.excluded) {
        ++excluded;
        continue;
      }
      ASSERT_EQ(res->output.invalid.bits[i], 0) << "pixel invalido " << i;
      const double M = sample(res->output, i), V = plane(res->output, i);
      tV.add(pi.v, V);
      tM.add(pi.m, M);
      sumV += V;
      ++NV;
      const ld y = static_cast<ld>(M) - static_cast<ld>(set.mean[0][p]);
      sy[p] += y;
      syy[p] += y * y;
      ++kp[p];
    }
  }
  // (a) |media(V̂) − E| ≤ sqrt(m Var(X)/(α N)) + ρ_c E + max τ.
  const ld meanV = sumV / static_cast<ld>(NV);
  const ld cheb = std::sqrt(static_cast<ld>(m) * varX / (kAlpha * static_cast<ld>(NV)));
  const ld half_a = cheb + rho * E + tV.v;
  // (b) sd_pool(M̂) ∈ [sqrt(V q_lo/ν) − Δ, sqrt(V q_hi/ν) + Δ], Δ = sqrt(N/ν) max τ.
  ld ss = 0;
  std::size_t nu = 0, N = 0;
  for (std::size_t p = 0; p < P; ++p) {
    if (kp[p] < 2) continue;
    ss += syy[p] - sy[p] * sy[p] / static_cast<ld>(kp[p]);
    nu += kp[p] - 1;
    N += kp[p];
  }
  const double tail = kAlpha / (2.0 * static_cast<double>(m));
  const ld q_lo = chi2x::lower_quantile(static_cast<double>(nu), tail);
  const ld q_hi = chi2x::upper_quantile(static_cast<double>(nu), tail);
  const ld sd = std::sqrt(ss / static_cast<ld>(nu));
  const ld delta = std::sqrt(static_cast<ld>(N) / static_cast<ld>(nu)) * tM.v;
  const ld lo_b = std::sqrt(E * q_lo / static_cast<ld>(nu)) - delta, hi_b = std::sqrt(E * q_hi / static_cast<ld>(nu)) + delta;
  std::cout << std::setprecision(10) << "T-M3B-C02-03 W" << which << " R " << R << " esclusi " << excluded << " V fuori "
            << tV.outside << "/" << NV << ", M fuori " << tM.outside << "/" << NV << "\n  (a) media V " << static_cast<double>(meanV) << " E "
            << static_cast<double>(E) << " semiampiezza " << static_cast<double>(half_a) << " (Chebyshev "
            << static_cast<double>(cheb) << ", rho_c E " << static_cast<double>(rho * E) << ", max tau "
            << static_cast<double>(tV.v) << ")\n  (b) sd " << static_cast<double>(sd) << " in [" << static_cast<double>(lo_b)
            << ", " << static_cast<double>(hi_b) << "] nu " << nu << " Delta " << static_cast<double>(delta) << "\n  durata "
            << seconds_since(t0) << " s\n";
  EXPECT_EQ(tV.outside, 0u) << "(a) valori di V fuori dall'intervallo: " << tV.outside << " su " << NV;
  EXPECT_EQ(tM.outside, 0u) << "(b) valori di M fuori dall'intervallo: " << tM.outside << " su " << NV;
  EXPECT_LE(std::fabs(meanV - E), half_a) << "(a)";
  EXPECT_GE(sd, lo_b) << "(b)";
  EXPECT_LE(sd, hi_b) << "(b)";
}

TEST(MasterB5Slow, T_M3B_C02_03_W1_weighted_mean_plane_and_values) { run_c02_03(1); }
TEST(MasterB5Slow, T_M3B_C02_03_W2_weighted_mean_plane_and_values) { run_c02_03(2); }

// ---------------------------------------------------------------- T-M3B-C02-04 (protocollo §3.2)

// `only`: indice della sola cella da misurare (n = 9, una cella per test ctest: la durata in dev con ASan resta sotto
// i 600 s), oppure tutte con −1. Il flusso e le realizzazioni sono gli stessi in ogni caso.
void run_c02_04(std::uint32_t n, int only = -1) {
  const auto t0 = std::chrono::steady_clock::now();
  const auto [seed, K] = sc::c02_04_seed_K(n);
  const std::size_t Ka = sc::c02_04_K_a, P = 4096;
  const ld sigma2 = 16;
  const sc::frame_set set = sc::c02_04_set(n);
  std::vector<c0204_cell> cells = c0204_cells(n);
  if (only >= 0) {
    const c0204_cell chosen = cells.at(static_cast<std::size_t>(only));
    cells.assign(1, chosen);
  }
  // Prima della misura: κ, μ_4 dell'oracle e h < δ/2 (protocollo §3.2).
  std::vector<b5o::moment_result> mo;
  std::vector<method> meths;
  std::vector<double> khat;
  for (const c0204_cell& c : cells) {
    mo.push_back(oracle_moments(c));
    ASSERT_LE(mo.back().est_kappa, 1e-11L);
    ASSERT_LE(mo.back().est_mu4, 1e-11L);
    if (c.in_b) {
      const h_delta hd = h_delta_of(mo.back(), n, K * P);
      ASSERT_LT(hd.h, hd.delta / 2) << "g " << c.g;
    }
    meths.push_back(c.k == kind::median ? median_m() : winsor_m(c.g));
    khat.push_back(kappa_hat(meths.back(), n));
  }
  const std::size_t C = cells.size();
  const std::size_t R = batch_of(K, n);
  batch_harness bh(n, R);
  sc::gauss_stream gs(seed);
  sc::frames buf;
  std::vector<ld> sumV(C, 0), sumX(C, 0);
  std::vector<std::size_t> NA(C, 0), NB(C, 0), excluded(C, 0);
  std::vector<tau_max> tV(C);
  std::vector<ld> marginB(C, 0);
  std::vector<std::size_t> outsideM(C, 0);
  std::vector<double> x(n), z(n);
  std::vector<std::pair<double, std::size_t>> sbuf;
  std::vector<std::optional<master_result>> res(C);
  for (std::size_t b = 0; b < K / R; ++b) {
    bh.realize(set, gs, buf);
    for (std::size_t j = 0; j < C; ++j) {
      res[j] = bh.run(meths[j]);
      if (!res[j]) return;
    }
    for (std::size_t i = 0; i < R * P; ++i) {
      const std::size_t k = b * R + i / P, p = i % P;
      const ld mu = set.mean[0][p];
      bh.canonical_at(i, x.data());
      b5o::sort_total(x.data(), n, sbuf, z.data());
      b5o::rule_i rs;
      const b5o::ivl s2 = b5o::s2_interval(rs, x.data(), n);
      for (std::size_t j = 0; j < C; ++j) {
        b5o::rule_i ri = rs;
        const b5o::ivl mi = cells[j].k == kind::median ? b5o::median_interval(ri, z.data(), n)
                                                       : b5o::winsorized_interval(ri, z.data(), n, cells[j].g);
        const b5o::ivl vi = b5o::kappa_plane(ri, khat[j], s2);
        if (ri.excluded) {
          ++excluded[j];
          continue;
        }
        ASSERT_EQ(res[j]->output.invalid.bits[i], 0) << meths[j].name() << " pixel invalido " << i;
        const double M = sample(res[j]->output, i), V = plane(res[j]->output, i);
        if (k < Ka) {
          tV[j].add(vi, V);
          sumV[j] += V;
          ++NA[j];
        }
        if (cells[j].in_b) {
          const ld xm = M;
          const ld tau = mi.hi - mi.lo;
          if (!(xm >= mi.lo && xm <= mi.hi)) ++outsideM[j];
          const ld d = xm - mu;
          marginB[j] = std::max(marginB[j], tau * (2 * std::fabs(d) + tau));
          sumX[j] += d * d;
          ++NB[j];
        }
      }
    }
  }
  const double tail = kAlpha / (2.0 * static_cast<double>(kC0204M));
  std::cout << "T-M3B-C02-04 n " << n << " K " << K << " R " << R << "\n";
  for (std::size_t j = 0; j < C; ++j) {
    SCOPED_TRACE(meths[j].name());
    const ld kap = mo[j].kappa, E = kap * sigma2;
    // (a) media(V̂) ∈ [κσ² q_lo/ν, κσ² q_hi/ν] ± max τ, ν = N_a (n − 1).
    const double nu = static_cast<double>(NA[j]) * static_cast<double>(n - 1);
    const ld q_lo = chi2x::lower_quantile(nu, tail), q_hi = chi2x::upper_quantile(nu, tail);
    const ld meanV = sumV[j] / static_cast<ld>(NA[j]);
    const ld lo_a = E * q_lo / nu - tV[j].v, hi_a = E * q_hi / nu + tV[j].v;
    std::cout << std::setprecision(10) << "  " << meths[j].name() << " esclusi " << excluded[j] << " (a) media V "
              << static_cast<double>(meanV) << " in [" << static_cast<double>(lo_a) << ", " << static_cast<double>(hi_a)
              << "] N_a " << NA[j] << " max tau " << static_cast<double>(tV[j].v) << " fuori " << tV[j].outside;
    EXPECT_EQ(tV[j].outside, 0u) << "(a) valori di V fuori dall'intervallo: " << tV[j].outside << " su " << NA[j];
    EXPECT_GE(meanV, lo_a) << "(a)";
    EXPECT_LE(meanV, hi_a) << "(a)";
    if (cells[j].in_b) {
      // (b) |media(X̂) − κσ²| ≤ sqrt(m Var(X)/(α N)) + max τ (2|M̂ − μ| + τ), Var(X) = (μ_4 − κ²) σ⁴.
      const ld varX = (mo[j].mu4 - kap * kap) * sigma2 * sigma2;
      const ld meanX = sumX[j] / static_cast<ld>(NB[j]);
      const ld half = std::sqrt(static_cast<ld>(kC0204M) * varX / (kAlpha * static_cast<ld>(NB[j]))) + marginB[j];
      std::cout << " (b) media X " << static_cast<double>(meanX) << " E " << static_cast<double>(E) << " semiampiezza "
                << static_cast<double>(half) << " margine " << static_cast<double>(marginB[j]) << " fuori " << outsideM[j];
      EXPECT_EQ(outsideM[j], 0u) << "(b) valori di M fuori dall'intervallo: " << outsideM[j] << " su " << NB[j];
      EXPECT_LE(std::fabs(meanX - E), half) << "(b)";
    }
    std::cout << "\n";
  }
  std::cout << "  durata " << seconds_since(t0) << " s\n";
}

TEST(MasterB5Slow, T_M3B_C02_04_n3_median_and_winsorized) { run_c02_04(3); }
TEST(MasterB5Slow, T_M3B_C02_04_n4_median_and_winsorized) { run_c02_04(4); }
TEST(MasterB5Slow, T_M3B_C02_04_n9_median) { run_c02_04(9, 0); }
TEST(MasterB5Slow, T_M3B_C02_04_n9_winsorized_g1) { run_c02_04(9, 1); }
TEST(MasterB5Slow, T_M3B_C02_04_n9_winsorized_g2) { run_c02_04(9, 2); }
TEST(MasterB5Slow, T_M3B_C02_04_n9_winsorized_g4) { run_c02_04(9, 3); }
TEST(MasterB5Slow, T_M3B_C02_04_n10_median_and_winsorized) { run_c02_04(10); }
TEST(MasterB5Slow, T_M3B_C02_04_n101_median_and_winsorized) { run_c02_04(101); }

}  // namespace
