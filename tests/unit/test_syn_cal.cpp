// Test della fixture FIX-SYN-CAL-08 (tests/fixtures/syn_cal.hpp), unità B2 di plans/M3b.md. Nessun codice di prodotto.
// Criteri: determinismo bit a bit per seed; trasformazione gaussiana riscritta qui dal testo del protocollo
// (docs/evidence/M3b-protocollo.md §1); momenti empirici entro limiti derivati in testa al test; outlier nelle posizioni
// della regola, riscritta qui; regione di livello dei flat; verità esatta uguale al campione senza rumore; costanti del
// protocollo (§3) ricavate dalla verità razionale. Seed propri del test (fuori dal protocollo): 2026110998, 2026110999.

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "chi2_exact.hpp"
#include "syn_cal.hpp"

namespace {

namespace sc = astro_fixture::syn_cal;
using exact::rat;
using sc::eq;
using sc::q;

std::uint64_t bits(double x) { return std::bit_cast<std::uint64_t>(x); }

bool same_bits(const sc::frames& a, const sc::frames& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t f = 0; f < a.size(); ++f) {
    if (a[f].size() != b[f].size() || std::memcmp(a[f].data(), b[f].data(), a[f].size() * sizeof(double)) != 0) return false;
  }
  return true;
}

// z con P(Z > z) = p: bisezione su P(Z > z) = erfc(z/√2)/2.
double normal_upper(double p) {
  double lo = 0, hi = 40;
  for (int it = 0; it < 200; ++it) {
    const double mid = (lo + hi) / 2;
    (0.5 * std::erfc(mid / std::sqrt(2.0)) > p ? lo : hi) = mid;
  }
  return (lo + hi) / 2;
}

long double ld(const rat& r) { return sc::approx(r); }

TEST(SynCal, stream_bit_identical_per_seed) {
  sc::gauss_stream a(2026110901), b(2026110901), c(2026110902);
  std::size_t equal_other_seed = 0;
  for (int i = 0; i < 100000; ++i) {
    const double x = a.next(), y = b.next(), z = c.next();
    ASSERT_EQ(bits(x), bits(y)) << "estrazione " << i;
    equal_other_seed += bits(x) == bits(z);
  }
  EXPECT_EQ(equal_other_seed, 0u);
  EXPECT_EQ(a.count(), 100000u);

  const auto s = sc::flat_03_set();
  sc::gauss_stream g1(2026110998), g2(2026110998), g3(2026110999);
  const auto r1 = sc::realize(s.set, g1), r2 = sc::realize(s.set, g2), r3 = sc::realize(s.set, g3);
  EXPECT_TRUE(same_bits(r1, r2));
  std::size_t noisy_equal = 0, quiet_different = 0;
  for (std::size_t f = 0; f < s.frames(); ++f) {
    for (std::size_t p = 0; p < s.set.pixels(); ++p) {
      if (s.set.var[f][p] > 0) noisy_equal += bits(r1[f][p]) == bits(r3[f][p]);
      else quiet_different += bits(r1[f][p]) != bits(r3[f][p]);
    }
  }
  EXPECT_EQ(noisy_equal, 0u);
  EXPECT_EQ(quiet_different, 0u);
}

TEST(SynCal, polar_transform_as_written_in_protocol) {
  std::mt19937_64 w(2026110931);
  std::vector<double> ref;
  while (ref.size() < 20001) {
    const double u1 = std::ldexp(static_cast<double>(w() >> 11), -53);
    const double u2 = std::ldexp(static_cast<double>(w() >> 11), -53);
    const double v1 = 2 * u1 - 1, v2 = 2 * u2 - 1;
    const double s = v1 * v1 + v2 * v2;
    if (s >= 1 || s == 0) continue;
    const double k = std::sqrt(-2 * std::log(s) / s);
    ref.push_back(v1 * k);
    ref.push_back(v2 * k);
  }
  sc::gauss_stream g(2026110931);
  for (std::size_t i = 0; i < ref.size(); ++i) ASSERT_EQ(bits(g.next()), bits(ref[i])) << "estrazione " << i;
}

// Ordine del flusso: frame, poi pixel in ordine di riga; solo i campioni con varianza > 0 estraggono; la z₂ avanzata
// passa alla realizzazione successiva; un outlier sostituisce il campione ma consuma la sua estrazione.
TEST(SynCal, stream_order_spare_and_outliers) {
  auto s = sc::empty_set(3, 3, 3);
  for (std::size_t f = 0; f < 3; ++f) {
    for (std::size_t p = 0; p < 9; ++p) {
      const bool quiet = (f == 0 && p == 4) || (f == 2 && p == 0);
      sc::set_law(s, f, p, q(100 + static_cast<std::int64_t>(p)), quiet ? q(0) : q(9, 4));
    }
  }
  sc::add_outlier(s, 1, 5, q(-7));
  ASSERT_EQ(sc::draws_per_realization(s), 25u);
  sc::gauss_stream g(2026110998), z(2026110998);
  for (int k = 0; k < 2; ++k) {
    const auto x = sc::realize(s, g);
    for (std::size_t f = 0; f < 3; ++f) {
      for (std::size_t p = 0; p < 9; ++p) {
        if (s.var[f][p] == 0) {
          EXPECT_EQ(bits(x[f][p]), bits(s.mean[f][p]));
          continue;
        }
        const double t = 1.5 * z.next();
        const double expected = (f == 1 && p == 5) ? -7.0 : t + s.mean[f][p];
        EXPECT_EQ(bits(x[f][p]), bits(expected)) << "k " << k << " f " << f << " p " << p;
      }
    }
  }
  EXPECT_EQ(g.count(), 50u);
  EXPECT_EQ(g.next(), z.next());
}

// Momenti di z iid N(0, 1), N = 2²², m = 4 statistiche, α = 10⁻³, ciascuna al livello α/m:
// (1) √N z̄ ~ N(0, 1), esatta: |z̄| ≤ z_(1 − α/(2m)) / √N;
// (2) Σ z² ~ χ²_N, esatta (media nota): Σ z² ∈ [q(α/(2m)), q(1 − α/(2m))];
// (3) media di z³: E = 0, Var(z³) = E z⁶ = 15; Chebyshev |m₃| ≤ sqrt(m · 15/(α N));
// (4) media di z⁴: E = 3, Var(z⁴) = E z⁸ − 9 = 105 − 9 = 96; Chebyshev |m₄ − 3| ≤ sqrt(m · 96/(α N)).
// Accumulo in long double: errore relativo ≤ N · 2⁻⁶⁴ ≈ 2 · 10⁻¹³, trascurabile rispetto ai limiti (≥ 10⁻³).
TEST(SynCal, gaussian_moments_within_derived_bounds) {
  constexpr std::size_t N = std::size_t{1} << 22;
  constexpr double alpha = 1e-3, m = 4;
  sc::gauss_stream g(2026110999);
  long double s1 = 0, s2 = 0, s3 = 0, s4 = 0;
  for (std::size_t i = 0; i < N; ++i) {
    const long double z = g.next(), z2 = z * z;
    s1 += z;
    s2 += z2;
    s3 += z2 * z;
    s4 += z2 * z2;
  }
  const double tail = alpha / (2 * m);
  EXPECT_LE(std::fabs(static_cast<double>(s1 / N)), normal_upper(tail) / std::sqrt(static_cast<double>(N)));
  EXPECT_GE(static_cast<double>(s2), chi2x::lower_quantile(N, tail));
  EXPECT_LE(static_cast<double>(s2), chi2x::upper_quantile(N, tail));
  EXPECT_LE(std::fabs(static_cast<double>(s3 / N)), std::sqrt(m * 15 / (alpha * N)));
  EXPECT_LE(std::fabs(static_cast<double>(s4 / N) - 3), std::sqrt(m * 96 / (alpha * N)));
}

// Eteroschedastico (FIX-SYN-CAL-08 di T-M3B-FLAT-03, una realizzazione): per frame f e g ∈ {1/2, 3/2} i 3 · 1365
// pixel di misura hanno u = (x − μ)/σ iid N(0, 1). m = 16 statistiche (8 gruppi × 2), α = 10⁻³:
// Σ u ~ N(0, N_g), esatta: |Σ u| ≤ z_(1 − α/(2m)) √N_g; Σ u² ~ χ²_(N_g), esatta: dentro [q(α/(2m)), q(1 − α/(2m))].
// Limite: u differisce da z per l'arrotondamento f64 di σz + μ e di x − μ, ≤ 2⁻⁵² |x|/σ ≈ 10⁻¹³ per campione.
TEST(SynCal, heteroscedastic_variance_per_frame_and_class) {
  const auto s = sc::flat_03_set();
  sc::gauss_stream g(2026110998);
  const auto x = sc::realize(s.set, g);
  constexpr double alpha = 1e-3, m = 16;
  const double tail = alpha / (2 * m);
  for (std::size_t f = 0; f < 4; ++f) {
    for (int gi = 0; gi < 2; ++gi) {
      long double su = 0, su2 = 0;
      std::size_t count = 0;
      for (std::size_t p = 0; p < s.set.pixels(); ++p) {
        if (s.region[p] < 0 || s.region[p] / 3 != gi) continue;
        const long double u = (x[f][p] - s.set.mean[f][p]) / s.set.sigma[f][p];
        su += u;
        su2 += u * u;
        ++count;
      }
      ASSERT_EQ(count, 3u * 1365u);
      const double ng = static_cast<double>(count);
      EXPECT_LE(std::fabs(static_cast<double>(su)), normal_upper(tail) * std::sqrt(ng)) << "f " << f << " g " << gi;
      EXPECT_GE(static_cast<double>(su2), chi2x::lower_quantile(ng, tail)) << "f " << f << " g " << gi;
      EXPECT_LE(static_cast<double>(su2), chi2x::upper_quantile(ng, tail)) << "f " << f << " g " << gi;
    }
  }
}

// Regola degli outlier riscritta dal commento della fixture: classe k = p mod 8, j = p / 8, frame (j + i) mod n,
// valore μ_p + passo · 4.
TEST(SynCal, outliers_at_declared_positions) {
  const std::int64_t steps[8][3] = {{0}, {12}, {-12}, {12, -12}, {12, 12, 12}, {25, 6}, {4}, {-6, 6}};
  const std::size_t counts[8] = {0, 1, 1, 2, 3, 2, 1, 2};
  const auto cases = sc::c02_02_cases();
  ASSERT_EQ(cases.seed, 2026110962u);
  ASSERT_EQ(cases.n, (std::vector<std::size_t>{11, 15, 31}));
  const auto realized = sc::realize_cases(cases);
  sc::gauss_stream clean_stream(cases.seed);
  for (std::size_t c = 0; c < cases.sets.size(); ++c) {
    const auto& s = cases.sets[c];
    const std::size_t n = cases.n[c];
    ASSERT_EQ(s.frames(), n);
    ASSERT_EQ(s.pixels(), 32u * 32u);
    std::vector<sc::outlier> expected;
    for (std::size_t p = 0; p < s.pixels(); ++p) {
      for (std::size_t i = 0; i < counts[p % 8]; ++i) {
        expected.push_back({(p / 8 + i) % n, p, static_cast<double>(1000 + p % 37 + steps[p % 8][i] * 4)});
      }
    }
    std::sort(expected.begin(), expected.end(),
              [](const sc::outlier& a, const sc::outlier& b) { return std::pair{a.frame, a.pixel} < std::pair{b.frame, b.pixel}; });
    ASSERT_EQ(s.outliers.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(s.outliers[i].frame, expected[i].frame);
      EXPECT_EQ(s.outliers[i].pixel, expected[i].pixel);
      EXPECT_EQ(bits(s.outliers[i].value), bits(expected[i].value));
    }
    auto without = s;
    without.outliers.clear();
    const auto clean = sc::realize(without, clean_stream);
    std::size_t differing = 0, mismatched = 0;
    for (std::size_t f = 0; f < n; ++f) {
      for (std::size_t p = 0; p < s.pixels(); ++p) {
        const auto it = std::find_if(s.outliers.begin(), s.outliers.end(),
                                     [&](const sc::outlier& o) { return o.frame == f && o.pixel == p; });
        if (it != s.outliers.end()) {
          mismatched += bits(realized[c][f][p]) != bits(it->value);
          differing += bits(realized[c][f][p]) != bits(clean[f][p]);
        } else {
          mismatched += bits(realized[c][f][p]) != bits(clean[f][p]);
        }
      }
    }
    EXPECT_EQ(mismatched, 0u);
    EXPECT_EQ(differing, expected.size());
  }
}

// Ogni flat: per canale più della metà dei pixel nella regione di livello, con g = 1, β = 0 e nessun rumore; in una
// realizzazione quei campioni valgono offset + c_(f,c) esatto e le due statistiche d'ordine centrali del canale
// coincidono con esso (mediana di x − offset = c_(f,c)).
void check_level_region(const sc::flat_set& s, std::uint64_t seed, const std::string& name) {
  SCOPED_TRACE(name);
  const auto P = s.set.pixels();
  std::vector<std::size_t> total(s.channels(), 0), level(s.channels(), 0);
  for (std::size_t p = 0; p < P; ++p) {
    ++total[s.channel[p]];
    if (s.region[p] >= 0) continue;
    ++level[s.channel[p]];
    ASSERT_TRUE(eq(s.g[p], q(1)) && eq(s.beta[p], q(0)));
    for (std::size_t f = 0; f < s.frames(); ++f) ASSERT_EQ(s.set.var[f][p], 0.0);
  }
  for (std::size_t c = 0; c < s.channels(); ++c) EXPECT_GT(2 * level[c], total[c]) << "canale " << c;
  sc::gauss_stream g(seed);
  const auto x = sc::realize(s.set, g);
  for (std::size_t f = 0; f < s.frames(); ++f) {
    for (std::size_t c = 0; c < s.channels(); ++c) {
      const rat expected = s.offset + s.levels[c][f];
      std::vector<double> v;
      for (std::size_t p = 0; p < P; ++p) {
        if (s.channel[p] != c) continue;
        v.push_back(x[f][p]);
        if (s.region[p] < 0) {
          ASSERT_TRUE(eq(exact::from_double(x[f][p]), expected));
        }
      }
      const std::size_t k = (v.size() - 1) / 2, k2 = v.size() / 2;
      std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
      const double lo = v[k];
      std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k2), v.end());
      EXPECT_TRUE(eq(exact::from_double(lo), expected) && eq(exact::from_double(v[k2]), expected)) << "f " << f << " c " << c;
    }
  }
}

TEST(SynCal, flat_level_region_everywhere) {
  const auto f3 = sc::flat_03_set();
  check_level_region(f3, sc::flat_03_seed, "FLAT-03");
  for (std::size_t n : {3u, 9u, 11u, 31u}) check_level_region(sc::flat_04_set(n), sc::flat_04_seed(n), "FLAT-04");
  for (int regime : {0, 1}) {
    check_level_region(sc::flat_05_set(31, 40, regime), sc::flat_05_seed(31, 40, regime), "FLAT-05 31/4");
    check_level_region(sc::flat_05_set(3, 10, regime), sc::flat_05_seed(3, 10, regime), "FLAT-05 3/1");
  }
  for (const auto& cases : {sc::flat_01_cases(), sc::flat_02_cases(), sc::flat_07_cases(), sc::flat_08_cases()}) {
    for (std::size_t i = 0; i < cases.sets.size(); ++i) check_level_region(cases.sets[i], cases.seed, cases.names[i]);
  }
  const auto cal = sc::make_calibration_set(16, 16, true, 3, true);
  check_level_region(cal.flat, 2026110998, "calibrazione CFA");
}

// Classi di misura dei flat del protocollo: conteggi e parametri (§3.4, §3.5, §3.6).
TEST(SynCal, flat_measurement_classes) {
  struct expectation {
    sc::flat_set s;
    std::size_t level, per, classes;
  };
  const auto six = sc::six_classes(q(32));
  std::vector<expectation> all;
  all.push_back({sc::flat_03_set(), 8194, 1365, 6});
  all.push_back({sc::flat_04_set(9), 2050, 341, 6});
  all.push_back({sc::flat_05_set(9, 15, 1), 2050, 1023, 2});
  for (const auto& e : all) {
    std::vector<std::size_t> count(e.classes, 0);
    std::size_t level = 0;
    for (std::size_t p = 0; p < e.s.set.pixels(); ++p) {
      if (e.s.region[p] < 0) {
        ++level;
        continue;
      }
      const auto k = static_cast<std::size_t>(e.s.region[p]);
      ASSERT_LT(k, e.classes);
      ++count[k];
      if (e.classes == 6) {
        ASSERT_TRUE(eq(e.s.g[p], six[k].g) && eq(e.s.beta[p], six[k].beta));
      } else {
        ASSERT_TRUE(eq(e.s.g[p], k == 0 ? q(1, 2) : q(3, 2)) && eq(e.s.beta[p], q(0)));
      }
    }
    EXPECT_EQ(level, e.level);
    for (auto c : count) EXPECT_EQ(c, e.per);
  }
}

// Senza rumore ogni campione è la verità esatta, ricostruita qui dai parametri: flat `offset + β + c g`; normalizzati
// `offset + r_f (x − offset)` uguali a E[y_f | r]; Σ_f r_f = n, quindi media dei y_f = b + λ g (fatto 1); con β ≡ 0
// E[y_f | r] = offset + m_H g per ogni frame (fatto 2, λ = c_H = m_H).
TEST(SynCal, noise_free_truth_equals_sample) {
  const auto cases = sc::flat_02_cases();
  ASSERT_EQ(cases.seed, 2026110966u);
  ASSERT_EQ(cases.sets.size(), 12u);
  const auto realized = sc::realize_cases(cases);
  for (std::size_t i = 0; i < cases.sets.size(); ++i) {
    SCOPED_TRACE(cases.names[i]);
    const auto& s = cases.sets[i];
    const bool beta_zero = cases.names[i][0] == 'b';
    ASSERT_EQ(sc::draws_per_realization(s.set), 0u);
    const auto t = sc::truth_of(s);
    for (std::size_t c = 0; c < s.channels(); ++c) {
      rat sum = q(0);
      for (const auto& r : t.r[c]) sum = sum + r;
      ASSERT_TRUE(eq(sum, q(static_cast<std::int64_t>(s.frames()))));
      ASSERT_TRUE(eq(t.lambda[c], t.m_h[c]));
      rat lo = s.levels[c][0], hi = s.levels[c][0];
      for (const auto& l : s.levels[c]) {
        if (l < lo) lo = l;
        if (hi < l) hi = l;
      }
      EXPECT_TRUE(eq(hi, lo * q(beta_zero ? 4 : 16)));
    }
    for (std::size_t p = 0; p < s.set.pixels(); ++p) {
      const std::size_t c = s.channel[p];
      if (beta_zero) {
        ASSERT_TRUE(eq(s.beta[p], q(0)));
      }
      ASSERT_TRUE(exact::abs(s.beta[p]) <= q(32));
      rat mean_y = q(0);
      for (std::size_t f = 0; f < s.frames(); ++f) {
        const rat sample = exact::from_double(realized[i][f][p]);
        ASSERT_TRUE(eq(sample, s.offset + s.beta[p] + s.levels[c][f] * s.g[p]));
        const rat y = s.offset + t.r[c][f] * (sample - s.offset);
        const auto law = sc::normalized_law(s, t, f, p);
        ASSERT_TRUE(eq(y, law.mean));
        ASSERT_TRUE(exact::is_zero(law.var));
        if (beta_zero) {
          ASSERT_TRUE(eq(law.mean, s.offset + t.m_h[c] * s.g[p]));
        }
        mean_y = mean_y + y;
      }
      ASSERT_TRUE(eq(mean_y / q(static_cast<std::int64_t>(s.frames())), sc::global_center(s, t, p)));
    }
  }

  const auto cal = sc::make_calibration_set(16, 16, true, 3, false);
  sc::gauss_stream g(2026110998);
  const auto b = sc::realize(cal.bias, g), d = sc::realize(cal.dark, g), df = sc::realize(cal.dark_flat, g),
             fl = sc::realize(cal.flat.set, g);
  EXPECT_EQ(g.count(), 0u);
  for (std::size_t p = 0; p < 256; ++p) {
    const rat base = cal.offset + cal.structure[p];
    const rat bflat = base + cal.dark_rate[p] * cal.t_flat;
    const std::size_t c = cal.flat.channel[p];
    if (cal.flat.region[p] < 0) {
      ASSERT_TRUE(eq(cal.structure[p], q(0)) && eq(cal.dark_rate[p], q(0)));
    }
    for (std::size_t f = 0; f < 3; ++f) {
      ASSERT_TRUE(eq(exact::from_double(b[f][p]), base));
      ASSERT_TRUE(eq(exact::from_double(d[f][p]), base + cal.dark_rate[p] * cal.t_dark));
      ASSERT_TRUE(eq(exact::from_double(df[f][p]), bflat));
      ASSERT_TRUE(eq(exact::from_double(fl[f][p]), bflat + cal.flat.levels[c][f] * cal.flat.g[p]));
      ASSERT_EQ(fl[f][p], std::floor(fl[f][p]));
    }
  }
}

// Costanti del protocollo (§3.1, §3.4, §3.5, §3.6) e casi diadici di T-M3B-FLAT-01 dalla verità razionale. I valori
// decimali del protocollo sono arrotondati: tolleranza di mezza unità dell'ultima cifra stampata.
TEST(SynCal, protocol_constants_from_exact_truth) {
  {
    const auto t = sc::truth_of_levels({{q(1), q(3)}});
    EXPECT_TRUE(eq(t.m_h[0], q(3, 2)) && eq(t.r[0][0], q(3, 2)) && eq(t.r[0][1], q(1, 2)));
    EXPECT_TRUE(eq(sc::truth_of_levels({{q(1), q(1), q(3), q(3)}}).m_h[0], q(3, 2)));
  }
  {
    const auto s = sc::flat_03_set();
    const auto t = sc::truth_of(s);
    EXPECT_TRUE(eq(t.m_h[0], q(1600)) && eq(t.lambda[0], q(1600)) && eq(t.rho[0], q(3, 5)));
    const rat r[4] = {q(8, 5), q(16, 15), q(4, 5), q(8, 15)};
    for (std::size_t f = 0; f < 4; ++f) EXPECT_TRUE(eq(t.r[0][f], r[f]));
    const double E_tab[6] = {207.222, 260.319, 260.319, 607.222, 660.319, 660.319};
    const double ha_tab[6] = {4.94, 4.89, 4.89, 4.93, 4.94, 4.94};
    const double level_tab[6] = {0, 53.096, 53.096, 0, 53.096, 53.096};
    for (std::int32_t k = 0; k < 6; ++k) {
      std::size_t p = 0;
      while (s.region[p] != k) ++p;
      std::vector<rat> mu, var;
      rat var_m = q(0);
      for (std::size_t f = 0; f < 4; ++f) {
        const auto law = sc::normalized_law(s, t, f, p);
        mu.push_back(law.mean);
        var.push_back(law.var);
        var_m = var_m + law.var / q(16);
      }
      const auto mo = sc::quadratic_moments(sc::mean_plane_form(4), mu, var);
      if (k == 0) {
        EXPECT_TRUE(eq(mo.mean, q(1865, 9)));
      }
      EXPECT_NEAR(static_cast<double>(ld(mo.mean)), E_tab[k], 5e-4) << "classe " << k;
      EXPECT_NEAR(static_cast<double>(ld(mo.mean - var_m)), level_tab[k], 5e-4) << "classe " << k;
      const long double ha = std::sqrt(18.0L * ld(mo.var) / (1e-3L * 4000 * 1365));
      EXPECT_NEAR(static_cast<double>(100 * ha / ld(mo.mean)), ha_tab[k], 5e-3) << "classe " << k;
    }
  }
  {
    const rat A = q(32), budget = exact::from_double(0.02);
    const std::size_t ns[4] = {3, 9, 11, 31};
    const double mh_tab[4] = {1714.2857, 2068.6469, 2087.6021, 2138.5582};
    const double rho_tab[4] = {0.714286, 1.068647, 1.087602, 1.138558};
    const double arho_tab[4] = {22.857, 34.197, 34.803, 36.434};
    const double bm_tab[4] = {34.286, 41.373, 41.752, 42.771};
    for (int i = 0; i < 4; ++i) {
      const auto t = sc::truth_of_levels({sc::flat_04_levels(ns[i])});
      EXPECT_NEAR(static_cast<double>(ld(t.m_h[0])), mh_tab[i], 5e-5);
      EXPECT_NEAR(static_cast<double>(ld(t.rho[0])), rho_tab[i], 5e-7);
      EXPECT_NEAR(static_cast<double>(ld(A * t.rho[0])), arho_tab[i], 5e-4);
      EXPECT_NEAR(static_cast<double>(ld(budget * t.m_h[0])), bm_tab[i], 5e-4);
      EXPECT_TRUE(A * t.rho[0] <= budget * t.m_h[0]);
    }
  }
  {
    const rat A = q(32), budget = exact::from_double(0.02);
    std::vector<std::uint64_t> seeds;
    for (std::size_t n : {3u, 9u, 31u}) {
      for (std::int64_t r10 : {10, 15, 20, 30, 40}) {
        const auto t = sc::truth_of_levels({sc::flat_05_levels(n, r10)});
        EXPECT_TRUE(A * t.rho[0] <= budget * t.m_h[0]) << n << " " << r10;
        for (int regime : {0, 1}) seeds.push_back(sc::flat_05_seed(n, r10, regime));
      }
    }
    std::sort(seeds.begin(), seeds.end());
    EXPECT_EQ(std::adjacent_find(seeds.begin(), seeds.end()), seeds.end());
    EXPECT_EQ(sc::flat_05_seed(9, 15, 1), 2026110959151u);
    const auto t = sc::truth_of_levels({sc::flat_05_levels(31, 40)});
    EXPECT_NEAR(static_cast<double>(ld(A * t.rho[0])), 36.43, 5e-3);
    EXPECT_NEAR(static_cast<double>(ld(budget * t.m_h[0])), 42.77, 5e-3);
    const auto read = sc::flat_05_set(31, 40, 1);
    double max_photon = 0;
    for (std::size_t f = 0; f < read.frames(); ++f) {
      for (std::size_t p = 0; p < read.set.pixels(); ++p) {
        if (read.region[p] >= 0) max_photon = std::max(max_photon, read.set.var[f][p] - 2500);
      }
    }
    EXPECT_EQ(max_photon, 375.0);
  }
  {
    const std::vector<std::vector<rat>> c_tab = {
        {q(15, 5944), q(35, 2972), q(189, 5944), q(105, 1486), q(945, 2972)},
        {q(245, 432), q(1, 48), q(1, 48), q(1, 48)}};
    const rat E_tab[2] = {q(1401, 128), q(215, 128)};
    const double varx_tab[2] = {198.807, 3.840};
    const double h_tab[2] = {0.312, 0.0433};
    const double hrel_tab[2] = {2.85, 2.58};
    for (int which = 1; which <= 2; ++which) {
      const auto cfg = sc::c02_03_config(which);
      std::vector<rat> w, var;
      for (std::size_t f = 0; f < cfg.w.size(); ++f) {
        w.push_back(q(cfg.w[f]));
        var.push_back(q(cfg.sigma[f] * cfg.sigma[f]));
      }
      const auto t = sc::weighted_truth_of(w, var);
      ASSERT_EQ(t.c.size(), c_tab[which - 1].size());
      for (std::size_t f = 0; f < t.c.size(); ++f) EXPECT_TRUE(eq(t.c[f], c_tab[which - 1][f])) << which << " " << f;
      EXPECT_TRUE(eq(t.var_m, E_tab[which - 1]));
      EXPECT_TRUE(eq(sc::quadratic_moments(t.form, std::vector<rat>(w.size(), q(0)), var).mean, t.var_m));
      EXPECT_NEAR(static_cast<double>(ld(t.var_v)), varx_tab[which - 1], 5e-4);
      const long double h = std::sqrt(4.0L * ld(t.var_v) / (1e-3L * 2000 * 4096));
      EXPECT_NEAR(static_cast<double>(h), h_tab[which - 1], which == 1 ? 5e-4 : 5e-5);
      EXPECT_NEAR(static_cast<double>(100 * h / ld(t.var_m)), hrel_tab[which - 1], 5e-3);
      EXPECT_EQ(sc::c02_03_set(which).frames(), cfg.w.size());
    }
    EXPECT_EQ(sc::c02_03_config(1).seed, 2026110901u);
    EXPECT_EQ(sc::c02_03_config(2).seed, 2026110902u);
  }
  EXPECT_EQ(sc::c02_04_seed_K(9), (std::pair<std::uint64_t, std::size_t>{2026110913, 24300}));
  EXPECT_EQ(sc::c02_06_seed(51), 2026110923u);
  EXPECT_EQ(sc::flat_04_seed(31), 2026110944u);
  EXPECT_EQ(sc::c02_01_cases().seed, 2026110961u);
  EXPECT_EQ(sc::c02_10_cases().seed, 2026110963u);
  EXPECT_EQ(sc::c02_11_cases().seed, 2026110964u);
  EXPECT_EQ(sc::flat_01_cases().seed, 2026110965u);
  EXPECT_EQ(sc::flat_07_cases().seed, 2026110967u);
  EXPECT_EQ(sc::flat_08_cases().seed, 2026110968u);
}

// T-M3B-C02-11: i pixel con p mod 16 = 8 non estraggono e non hanno outlier (s = 0 nel master).
TEST(SynCal, report_cases_have_zero_variance_pixels) {
  const auto cases = sc::c02_11_cases();
  const auto x = sc::realize_cases(cases);
  for (std::size_t c = 0; c < cases.sets.size(); ++c) {
    const auto& s = cases.sets[c];
    for (std::size_t p = 8; p < s.pixels(); p += 16) {
      for (std::size_t f = 0; f < s.frames(); ++f) {
        ASSERT_EQ(s.var[f][p], 0.0);
        ASSERT_EQ(x[c][f][p], 1000.0 + static_cast<double>(p % 37));
      }
    }
  }
}

// Frame RAW (FIX-SYN-DNG-01): campioni u16 little-endian uguali al frame intero, motivo RGGB, nero e bianco
// dichiarati; G1 e G2 a livelli diversi; un campione non intero ferma la conversione.
TEST(SynCal, raw_frames_from_noise_free_set) {
  const auto cal = sc::make_calibration_set(16, 16, true, 3, false);
  sc::gauss_stream g(2026110998);
  const auto fl = sc::realize(cal.flat.set, g);
  const auto spec = sc::raw_frame(fl[1], 16, 16, 512, 65535, {1, 4}, 100);
  EXPECT_EQ(spec.cfa, (std::vector<std::uint8_t>{0, 1, 1, 2}));
  EXPECT_EQ(spec.black, (std::vector<std::uint16_t>{512}));
  EXPECT_EQ(spec.white, std::optional<std::uint32_t>{65535});
  ASSERT_EQ(spec.pixels.size(), 2u * 256u);
  for (std::size_t p = 0; p < 256; ++p) {
    const unsigned v = spec.pixels[2 * p] | (unsigned{spec.pixels[2 * p + 1]} << 8);
    ASSERT_EQ(static_cast<double>(v), fl[1][p]);
  }
  EXPECT_FALSE(eq(cal.flat.levels[1][0], cal.flat.levels[2][0]));
  EXPECT_GT(astro_fixture::make_dng(spec).size(), spec.pixels.size());

  const auto noisy = sc::make_calibration_set(16, 16, true, 3, true);
  sc::gauss_stream h(2026110998);
  const auto nf = sc::realize(noisy.flat.set, h);
  EXPECT_THROW(sc::raw_frame(nf[0], 16, 16, 512, 65535, {1, 4}, 100), std::invalid_argument);
}

// La fixture rifiuta parametri non rappresentabili esattamente e regioni di livello non maggioritarie.
TEST(SynCal, refuses_inexact_or_invalid_parameters) {
  auto s = sc::empty_set(2, 2, 2);
  EXPECT_THROW(sc::set_law(s, 0, 0, q(1, 3), q(1)), std::invalid_argument);
  EXPECT_THROW(sc::set_law(s, 0, 0, q(1), q(1, 10)), std::invalid_argument);
  EXPECT_THROW(sc::set_law(s, 0, 0, q(1), q(-1)), std::invalid_argument);
  const auto third = [](std::size_t, std::size_t, std::uint32_t, std::uint32_t) {
    return std::pair<std::int32_t, sc::flat_pixel>{0, {q(1, 3), q(0)}};
  };
  EXPECT_THROW(sc::make_flat(4, 4, false, q(512), {{q(1000), q(2000)}}, {9}, std::nullopt, third), std::invalid_argument);
  EXPECT_THROW(sc::make_flat(4, 4, false, q(512), {{q(1000), q(2000)}}, {8}, std::nullopt, sc::pattern_measure()),
               std::invalid_argument);
  EXPECT_THROW(sc::c02_04_set(5), std::invalid_argument);
  const std::vector<rat> v3(3, q(1));
  EXPECT_THROW(sc::weighted_truth_of({q(1), q(1)}, {q(1), q(1)}), std::invalid_argument);
  EXPECT_THROW(sc::weighted_truth_of({q(1), q(0), q(1)}, v3), std::invalid_argument);
  EXPECT_THROW(sc::weighted_truth_of({q(1), q(-1), q(3)}, v3), std::invalid_argument);
  EXPECT_THROW(sc::weighted_truth_of({q(1), rat{exact::from_u64(1), exact::from_u64(0)}, q(1)}, v3), std::invalid_argument);
  EXPECT_NO_THROW(sc::weighted_truth_of({q(1), q(1), q(1)}, v3));
  auto t = sc::gaussian_set(4, 4, 3, q(16));
  sc::add_outlier(t, 0, 1, q(5));
  EXPECT_THROW(sc::add_outlier(t, 0, 1, q(6)), std::invalid_argument);
}

}  // namespace
