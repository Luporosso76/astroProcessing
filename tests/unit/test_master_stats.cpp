// T-M2R2-04 e T-M2E-21 sui master del prodotto: Monte Carlo con seed fisso. Lungo: label `slow`.
//
// Parametri, seed e soglie fissati prima dell'esecuzione (C-02.a §9; ADR-17, aggiunte di R2). Riferimento: la
// varianza vera della simulazione. Campioni gaussiani continui, frame e master f64.
//
// T-M2R2-04: alpha = 1e-3 complessivo, Bonferroni su m = 68 statistiche, n = 8 frame, K = 2000 realizzazioni.
//  A (dark, stessa media e stessa sigma = 4): per 16 pixel sonda, K (n-1) media_K(piano)/(sigma^2/n) in
//    chi2_(K(n-1)) e (K-1) var_K(M)/V in chi2_(K-1), intervalli bilaterali al livello alpha/m (32 statistiche).
//  B (dark, sigma_f = 1..8): piano mediato su K x 4096 valori contro E = sum sigma_f^2/n^2 con il limite di
//    Chebyshev e Var = 2 tr((A S)^2)/((n-1)^2 n^2); (K-1) var_K(M)/V in chi2_(K-1) su 16 sonde (17 statistiche).
//  C (flat, livelli c_f = c (1 +- eps), eps = 1/16, c = 1024, regione g = 0.75, sigma = 8): piano mediato su
//    K x 2048 valori contro E = sum sigma^2/n^2 + g^2 Var_f(c)/n, Var dalla forma quadratica non centrale;
//    level_bias_estimate mediata contro E[eps^2 max(Y,0)^2/(n-1)] con Y ~ N(a, V) (momenti troncati esatti);
//    chi2_(K-1) su 16 sonde per i valori del master (18 statistiche).
//  D (stessa fixture, regione fuori da nonnegative_signal: b - offset = 2, c g = -2, sigma = 2): media della stima
//    contro il valore atteso esatto con la troncatura, Chebyshev con secondo e quarto momento di max(Y, 0) (1).
//  La regione di fondo del flat (meta' dei pixel piu' uno, senza rumore, b = offset, g = 1) fissa m_f = c_f esatto.
// T-M2E-21 sui master del prodotto: alpha = 1e-3, Bonferroni su m = 16, K = 2000.
//  (a) 8 sonde: (K-1) var_K(C)/Var_vera(C) in chi2_(K-1), Var_vera dal primo ordine con le varianze vere dei master
//      (sigma^2/n), regime Var(Q)/Q^2 <= 1e-4. (b) 8 sonde, flat e dark-flat senza rumore, Fc costante (Q = 1),
//      piano del light nullo: K (n-1) media_K(Var(C))/(sigma_D^2/n) in chi2_(K(n-1)).
#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <random>

#include "chi2_exact.hpp"
#include "master_support.hpp"

namespace {

using namespace mst;

constexpr double alpha = 1e-3;
constexpr int K = 2000;
constexpr std::size_t n = 8;

struct interval {
  double lo, hi;
};

interval chi2_interval(double dof, double tail) {
  return {chi2x::lower_quantile(dof, tail), chi2x::upper_quantile(dof, tail)};
}

TEST(MasterStats, chi2_quantiles_match_closed_forms) {
  // k = 2: Q(x) = exp(-x/2), quindi x = -2 ln p; k = 4: Q(x) = exp(-x/2) (1 + x/2).
  for (const double p : {1e-3, 1e-5, 6.25e-7}) {
    EXPECT_NEAR(chi2x::upper_quantile(2, p), -2 * std::log(p), 1e-10 * -2 * std::log(p));
    EXPECT_NEAR(chi2x::lower_quantile(2, p), -2 * std::log1p(-p), 1e-10 * -2 * std::log1p(-p));
    const double x4 = chi2x::upper_quantile(4, p);
    EXPECT_NEAR(std::exp(-x4 / 2) * (1 + x4 / 2), p, 1e-10 * p);
  }
}

// Frame f64 con campioni `mu` e una card propria (digest distinti anche a parita' di valori).
mframe base_frame(master_role role, std::uint32_t w, std::uint32_t h, std::size_t k) {
  mframe f = frame_for(role, w, h, -64);
  f.hdr.extra = {fx::card_int("FRAMENO", static_cast<long long>(k))};
  return f;
}

void add_noise(image& im, std::mt19937_64& rng, const std::vector<double>& sigma) {
  double* s = im.data<double>();
  for (std::size_t i = 0; i < sigma.size(); ++i) {
    if (sigma[i] == 0) continue;
    s[i] += std::normal_distribution<double>(0.0, sigma[i])(rng);
  }
}

struct mc_group {
  group g;
  master_recipe recipe;
  std::vector<image> base;
  std::vector<std::vector<double>> sigma;  // per frame e pixel
};

mc_group make_mc(master_role role, std::uint32_t w, std::uint32_t h, const std::vector<std::vector<double>>& mu,
                 const std::vector<std::vector<double>>& sigma, double flat_tol) {
  mc_group m;
  m.g.w = w;
  m.g.h = h;
  m.g.role = role;
  for (std::size_t k = 0; k < mu.size(); ++k) {
    mframe f = base_frame(role, w, h, k);
    f.v = mu[k];
    m.g.frames.push_back(f);
  }
  m.recipe = recipe_of(m.g, flat_tol);
  m.base = images_of(m.g);
  m.sigma = sigma;
  return m;
}

master_result realize(const mc_group& m, std::mt19937_64& rng) {
  std::vector<image> frames = m.base;
  for (std::size_t k = 0; k < frames.size(); ++k) add_noise(frames[k], rng, m.sigma[k]);
  return combine_master(m.recipe, std::move(frames));
}

struct stats_book {
  std::size_t count = 0;
  std::size_t m;
  explicit stats_book(std::size_t m_) : m(m_) {}
  double tail() const { return alpha / static_cast<double>(m) / 2; }
  void chi2(double stat, double dof, const std::string& what) {
    ++count;
    const interval iv = chi2_interval(dof, tail());
    EXPECT_GT(stat, iv.lo) << what << " (dof " << dof << ")";
    EXPECT_LT(stat, iv.hi) << what << " (dof " << dof << ")";
  }
  void chebyshev(long double mean, long double expected, long double var, double samples, const std::string& what) {
    ++count;
    const long double half = std::sqrt(static_cast<long double>(m) * var / (alpha * samples));
    EXPECT_LE(std::fabs(mean - expected), half) << what << ": media " << static_cast<double>(mean) << ", attesa "
                                                << static_cast<double>(expected) << ", semiampiezza " << static_cast<double>(half);
  }
};

// Varianza esatta di s^2/n per x ~ N(mu, diag(sigma^2)): [2 tr((A S)^2) + 4 mu^T A S A mu] / ((n-1)^2 n^2).
long double plane_variance(const std::vector<double>& mu, const std::vector<double>& s2) {
  const std::size_t nn = mu.size();
  long double tr = 0;
  for (std::size_t i = 0; i < nn; ++i) {
    for (std::size_t j = 0; j < nn; ++j) {
      const long double aij = (i == j ? 1.0L : 0.0L) - 1.0L / nn, aji = aij;
      tr += aij * s2[j] * aji * s2[i];
    }
  }
  long double mbar = 0;
  for (double v : mu) mbar += v / static_cast<long double>(nn);
  long double q = 0;
  for (std::size_t i = 0; i < nn; ++i) q += (mu[i] - mbar) * (mu[i] - mbar) * s2[i];
  const long double d = static_cast<long double>(nn - 1) * nn;
  return (2 * tr + 4 * q) / (d * d);
}

long double plane_expectation(const std::vector<double>& mu, const std::vector<double>& s2) {
  const std::size_t nn = mu.size();
  long double sum = 0, mbar = 0, dev = 0;
  for (double v : s2) sum += v;
  for (double v : mu) mbar += v / static_cast<long double>(nn);
  for (double v : mu) dev += (v - mbar) * (v - mbar);
  return sum / (static_cast<long double>(nn) * nn) + dev / (static_cast<long double>(nn - 1) * nn);
}

// Momenti di max(Y, 0) con Y ~ N(a, V): E[max^2] e E[max^4] dai momenti della normale troncata.
struct trunc_moments {
  long double m2, m4;
};
trunc_moments truncated(long double a, long double v) {
  const long double s = std::sqrt(v), t = a / s;
  const long double phi = std::exp(-t * t / 2) / std::sqrt(2 * 3.14159265358979323846264338327950288L);
  const long double Phi = 0.5L * std::erfc(-t / std::sqrt(2.0L));
  // M_k = E[Z^k 1{Z > -t}] = (-t)^(k-1) phi(t) + (k-1) M_(k-2)
  const long double M0 = Phi, M1 = phi, M2 = -t * phi + M0, M3 = t * t * phi + 2 * M1, M4 = -t * t * t * phi + 3 * M2;
  const long double m2 = a * a * M0 + 2 * a * s * M1 + s * s * M2;
  const long double m4 = a * a * a * a * M0 + 4 * a * a * a * s * M1 + 6 * a * a * s * s * M2 + 4 * a * s * s * s * M3 +
                         s * s * s * s * M4;
  return {m2, m4};
}

TEST(MasterStats, T_M2R2_04_monte_carlo_variance_plane_master_and_level_bias) {
  stats_book book(68);
  std::vector<std::size_t> probes;
  for (std::size_t j = 0; j < 16; ++j) probes.push_back(37 + 251 * j);

  // ---------------------------------------------------------------- A e B (dark 64x64)
  constexpr std::uint32_t w = 64, h = 64;
  constexpr std::size_t px = w * h;
  std::vector<std::vector<double>> mu_dark(n, std::vector<double>(px));
  for (std::size_t k = 0; k < n; ++k) {
    for (std::size_t i = 0; i < px; ++i) mu_dark[k][i] = 1000.0 + static_cast<double>(i % 37);
  }
  {
    const mc_group a = make_mc(master_role::dark, w, h, mu_dark, std::vector<std::vector<double>>(n, std::vector<double>(px, 4.0)), 0);
    std::mt19937_64 rng(20261101);
    std::vector<long double> plane_sum(probes.size(), 0), m_sum(probes.size(), 0), m_sq(probes.size(), 0);
    std::vector<std::vector<double>> mvals(probes.size());
    for (int r = 0; r < K; ++r) {
      const master_result res = realize(a, rng);
      for (std::size_t j = 0; j < probes.size(); ++j) {
        ASSERT_EQ(res.output.invalid.bits[probes[j]], 0);
        plane_sum[j] += plane(res.output, probes[j]);
        mvals[j].push_back(sample(res.output, probes[j]));
      }
    }
    const double s2n = 16.0 / n, V = 16.0 / n;
    for (std::size_t j = 0; j < probes.size(); ++j) {
      book.chi2(static_cast<double>(K * (n - 1) * (plane_sum[j] / K) / s2n), K * (n - 1.0), "A piano " + std::to_string(probes[j]));
      long double mean = 0, m2 = 0;
      for (double v : mvals[j]) mean += v / K;
      for (double v : mvals[j]) m2 += (v - mean) * (v - mean);
      book.chi2(static_cast<double>(m2 / V), K - 1.0, "A master " + std::to_string(probes[j]));
    }
  }
  {
    std::vector<double> sig(n), s2(n);
    for (std::size_t k = 0; k < n; ++k) {
      sig[k] = static_cast<double>(k + 1);
      s2[k] = sig[k] * sig[k];
    }
    std::vector<std::vector<double>> sigma(n);
    for (std::size_t k = 0; k < n; ++k) sigma[k].assign(px, sig[k]);
    const mc_group b = make_mc(master_role::dark, w, h, mu_dark, sigma, 0);
    std::mt19937_64 rng(20261102);
    long double pooled = 0;
    std::vector<std::vector<double>> mvals(probes.size());
    for (int r = 0; r < K; ++r) {
      const master_result res = realize(b, rng);
      for (std::size_t i = 0; i < px; ++i) {
        ASSERT_EQ(res.output.invalid.bits[i], 0);
        pooled += plane(res.output, i);
      }
      for (std::size_t j = 0; j < probes.size(); ++j) mvals[j].push_back(sample(res.output, probes[j]));
    }
    const std::vector<double> mu_const(n, 1000.0);
    const double samples = static_cast<double>(K) * px;
    book.chebyshev(pooled / samples, plane_expectation(mu_const, s2), plane_variance(mu_const, s2), samples, "B piano");
    long double V = 0;
    for (double v : s2) V += v / (static_cast<long double>(n) * n);
    for (std::size_t j = 0; j < probes.size(); ++j) {
      long double mean = 0, m2 = 0;
      for (double v : mvals[j]) mean += v / K;
      for (double v : mvals[j]) m2 += (v - mean) * (v - mean);
      book.chi2(static_cast<double>(m2 / V), K - 1.0, "B master " + std::to_string(probes[j]));
    }
  }

  // ---------------------------------------------------------------- C e D (flat 128x64)
  constexpr std::uint32_t fw = 128, fh = 64;
  constexpr std::size_t fpx = fw * fh;
  constexpr double off = 10, cbar = 1024, eps = 1.0 / 16, gC = 0.75, gD = -2.0 / 1024, sC = 8, sD = 2;
  std::vector<double> c(n);
  for (std::size_t k = 0; k < n; ++k) c[k] = cbar * (k % 2 == 0 ? 1 + eps : 1 - eps);
  // Righe 0-32 fondo (4224 pixel > meta' + 1), 33-48 regione C (2048), 49-63 regione D (1920).
  const auto region = [](std::size_t i) { const std::size_t y = i / fw; return y <= 32 ? 0 : y <= 48 ? 1 : 2; };
  std::vector<std::vector<double>> mu(n, std::vector<double>(fpx)), sigma(n, std::vector<double>(fpx));
  for (std::size_t k = 0; k < n; ++k) {
    for (std::size_t i = 0; i < fpx; ++i) {
      const int rg = region(i);
      const double b = rg == 2 ? off + 2 : off, g = rg == 0 ? 1.0 : rg == 1 ? gC : gD;
      mu[k][i] = b + c[k] * g;
      sigma[k][i] = rg == 0 ? 0 : rg == 1 ? sC : sD;
    }
  }
  const mc_group fl = make_mc(master_role::flat, fw, fh, mu, sigma, eps);
  std::vector<std::size_t> fprobes;
  for (std::size_t j = 0; j < 16; ++j) fprobes.push_back(33 * fw + 5 + 127 * j);
  for (std::size_t p : fprobes) ASSERT_EQ(region(p), 1);
  std::mt19937_64 rng(20261103);
  long double planeC = 0, lbeC = 0, lbeD = 0;
  std::size_t countC = 0, countD = 0;
  std::vector<std::vector<double>> mvals(fprobes.size());
  bool report_checked = false;
  for (int r = 0; r < K; ++r) {
    const master_result res = realize(fl, rng);
    ASSERT_EQ(res.level_bias_estimate.size(), fpx);
    for (std::size_t i = 0; i < fpx; ++i) {
      ASSERT_EQ(res.output.invalid.bits[i], 0);
      const int rg = region(i);
      if (rg == 1) {
        planeC += plane(res.output, i);
        lbeC += res.level_bias_estimate[i];
        ++countC;
      } else if (rg == 2) {
        lbeD += res.level_bias_estimate[i];
        ++countD;
      }
    }
    for (std::size_t j = 0; j < fprobes.size(); ++j) mvals[j].push_back(sample(res.output, fprobes[j]));
    if (!report_checked) {
      report_checked = true;
      EXPECT_EQ(cal::as_double(cal::at(res.report, {"flat_level_mean"})), cbar);
      bool listed = false;
      for (const json& hyp : cal::at(res.report, {"assumptions_not_verified"}).items()) listed = listed || hyp.as_string() == "nonnegative_signal";
      EXPECT_TRUE(listed);
      EXPECT_GT(cal::at(res.report, {"channels"}).items()[0].find("pixels_nonpositive_signal")->as_integer(), 0);
    }
  }
  long double cvar = 0;
  for (double v : c) cvar += (v - cbar) * (v - cbar) / (n - 1);
  {
    std::vector<double> muC(n), s2C(n, sC * sC);
    for (std::size_t k = 0; k < n; ++k) muC[k] = off + c[k] * gC;
    const long double E = plane_expectation(muC, s2C);
    EXPECT_NEAR(static_cast<double>(E), static_cast<double>(sC * sC / n + gC * gC * cvar / n), 1e-9);
    book.chebyshev(planeC / countC, E, plane_variance(muC, s2C), static_cast<double>(countC), "C piano");
    const long double V = sC * sC / n, a = cbar * gC;
    EXPECT_GE(a, 8 * std::sqrt(V));
    const trunc_moments t = truncated(a, V);
    const long double e2 = eps * eps / (n - 1);
    const long double El = e2 * t.m2, Vl = e2 * e2 * (t.m4 - t.m2 * t.m2);
    EXPECT_GE(El, gC * gC * cvar / n);  // la stima maggiora B in valore atteso sotto le ipotesi
    book.chebyshev(lbeC / countC, El, Vl, static_cast<double>(countC), "C level_bias_estimate");
    for (std::size_t j = 0; j < fprobes.size(); ++j) {
      long double mean = 0, m2 = 0;
      for (double v : mvals[j]) mean += v / K;
      for (double v : mvals[j]) m2 += (v - mean) * (v - mean);
      book.chi2(static_cast<double>(m2 / V), K - 1.0, "C master " + std::to_string(fprobes[j]));
    }
  }
  {
    const long double V = sD * sD / n, a = 2 + cbar * gD;
    EXPECT_EQ(a, 0.0L);
    EXPECT_LE(V, 1.0L);
    const trunc_moments t = truncated(a, V);
    const long double e2 = eps * eps / (n - 1);
    const long double El = e2 * t.m2, Vl = e2 * e2 * (t.m4 - t.m2 * t.m2);
    const long double B = gD * gD * cvar / n;
    EXPECT_GT(B, El) << "fuori da nonnegative_signal la stima non maggiora B";
    book.chebyshev(lbeD / countD, El, Vl, static_cast<double>(countD), "D level_bias_estimate");
  }
  EXPECT_EQ(book.count, book.m);
}

TEST(MasterStats, T_M2E_21_product_masters_variance_of_c) {
  stats_book book(16);
  constexpr std::uint32_t w = 32, h = 32;
  constexpr std::size_t px = w * h;
  std::vector<std::size_t> probes;
  for (std::size_t j = 0; j < 8; ++j) probes.push_back(19 + 123 * j);
  const auto frames_of = [&](master_role role, const std::vector<double>& mu, double sig) {
    return make_mc(role, w, h, std::vector<std::vector<double>>(n, mu),
                   std::vector<std::vector<double>>(n, std::vector<double>(px, sig)), 0.01);
  };
  const auto plane_recipe = [](calibration_recipe r) {
    for (calibration_input_spec* in : {&r.light, &r.dark, &r.flat, &r.dark_flat}) in->variance = variance_spec{variance_source::plane, std::nullopt};
    r.q_min = 0.01;
    r.q_rel_sigma_max = 1.0;
    return r;
  };
  const auto descriptors = [](calibration_recipe& r, const mc_group& d, const mc_group& f, const mc_group& df) {
    r.dark.descriptor = master_descriptor_from_node(make_master_node(d.recipe));
    r.flat.descriptor = master_descriptor_from_node(make_master_node(f.recipe));
    r.dark_flat.descriptor = master_descriptor_from_node(make_master_node(df.recipe));
  };

  // (a) valori di C, tutti i frame rumorosi.
  {
    constexpr double sL = 30, sD = 40, sF = 40, sDF = 40;
    std::vector<double> fc0(px), lc0(px), muD(px, 500), muDF(px, 200), muF(px), light(px);
    for (std::size_t i = 0; i < px; ++i) {
      fc0[i] = 9000.0 + 100.0 * static_cast<double>(i % 16) + 7.0 * static_cast<double>(i % 5);
      lc0[i] = 1000.0 + 5.0 * static_cast<double>(i % 97);
      muF[i] = 200 + fc0[i];
      light[i] = 500 + lc0[i];
    }
    const mc_group d = frames_of(master_role::dark, muD, sD), f = frames_of(master_role::flat, muF, sF),
                   df = frames_of(master_role::dark_flat, muDF, sDF);
    cal::set_def s = cal::empty_set(w, h, -64);
    s.light.v = light;
    s.light.variance = std::vector<double>(px, sL * sL);
    s.dark.v = muD;
    s.flat.v = muF;
    s.dark_flat.v = muDF;
    calibration_recipe r = plane_recipe(cal::recipe_of(s));
    descriptors(r, d, f, df);
    const image light_base = cal::decode(s.light, w, h);
    std::mt19937_64 rng(20261104);
    std::vector<std::vector<double>> cvals(probes.size());
    for (int k = 0; k < K; ++k) {
      image l = light_base;
      add_noise(l, rng, std::vector<double>(px, sL));
      calibration_images im{std::move(l), realize(d, rng).output, realize(f, rng).output, realize(df, rng).output};
      const calibration_result out = calibrate(r, std::move(im));
      for (std::size_t j = 0; j < probes.size(); ++j) {
        ASSERT_EQ(out.output.invalid.bits[probes[j]], 0);
        cvals[j].push_back(sample(out.output, probes[j]));
      }
    }
    std::vector<double> sorted = fc0;
    std::sort(sorted.begin(), sorted.end());
    const double n0 = (sorted[px / 2 - 1] + sorted[px / 2]) / 2;
    for (std::size_t j = 0; j < probes.size(); ++j) {
      const std::size_t i = probes[j];
      const double q0 = fc0[i] / n0;
      const double var_q = (sF * sF + sDF * sDF) / n / (n0 * n0);
      EXPECT_LE(var_q / (q0 * q0), 1e-4);
      const double var_c = (sL * sL + sD * sD / n) / (q0 * q0) + lc0[i] * lc0[i] * var_q / std::pow(q0, 4);
      long double mean = 0, m2 = 0;
      for (double v : cvals[j]) mean += v / K;
      for (double v : cvals[j]) m2 += (v - mean) * (v - mean);
      book.chi2(static_cast<double>(m2 / var_c), K - 1.0, "(a) C " + std::to_string(i));
    }
  }
  // (b) Var(C) riportata: Q = 1 esatto, Var(Q) = 0, piano del light nullo: Var(C) = piano del master dark.
  {
    constexpr double sD = 4;
    std::vector<double> muD(px, 500), muDF(px, 200), muF(px, 1200), light(px);
    for (std::size_t i = 0; i < px; ++i) light[i] = 600 + static_cast<double>(i % 7);
    const mc_group d = frames_of(master_role::dark, muD, sD), f = frames_of(master_role::flat, muF, 0),
                   df = frames_of(master_role::dark_flat, muDF, 0);
    cal::set_def s = cal::empty_set(w, h, -64);
    s.light.v = light;
    s.light.variance = std::vector<double>(px, 0.0);
    s.dark.v = muD;
    s.flat.v = muF;
    s.dark_flat.v = muDF;
    calibration_recipe r = plane_recipe(cal::recipe_of(s));
    descriptors(r, d, f, df);
    const image light_base = cal::decode(s.light, w, h);
    std::mt19937_64 noiseless(1);  // sigma = 0: nessuna estrazione
    const master_result mf = realize(f, noiseless), mdf = realize(df, noiseless);
    std::mt19937_64 rng(20261105);
    std::vector<long double> vsum(probes.size(), 0);
    for (int k = 0; k < K; ++k) {
      calibration_images im{light_base, realize(d, rng).output, mf.output, mdf.output};
      const calibration_result out = calibrate(r, std::move(im));
      for (std::size_t j = 0; j < probes.size(); ++j) {
        ASSERT_EQ(out.output.invalid.bits[probes[j]], 0);
        vsum[j] += plane(out.output, probes[j]);
      }
    }
    for (std::size_t j = 0; j < probes.size(); ++j) {
      book.chi2(static_cast<double>(K * (n - 1) * (vsum[j] / K) / (sD * sD / n)), K * (n - 1.0),
                "(b) Var(C) " + std::to_string(probes[j]));
    }
  }
  EXPECT_EQ(book.count, book.m);
}

}  // namespace
