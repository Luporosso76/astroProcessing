// T-M2E-21 e T-M2E-26: Monte Carlo con seed fisso. Lungo: label `slow`.
//
// Soglie fissate prima dell'esecuzione. Quantile chi-quadro con nu gradi di liberta' dall'approssimazione di
// Wilson-Hilferty, chi2(nu, z) = nu (1 - 2/(9 nu) + z sqrt(2/(9 nu)))^3, errore relativo trascurabile per nu ~ 2000.
//  - T-M2E-21: alpha complessivo 1e-3 con Bonferroni su P = 8 pixel, due code: coda per pixel 1e-3/(2*8) = 6.25e-5,
//    z = 3.83. nu = K - 1 con K = 2000. Statistica (K-1) s^2 / Var_propagata, con Var_propagata calcolata dalla
//    formula del primo ordine sui valori veri (Var(Q)/Q^2 <= 1e-4).
//  - T-M2E-26: alpha = 1e-3 su due code, coda 5e-4, z = 3.2905, nu = K - 1 con K = 3000; intervallo del rapporto
//    fra varianza empirica relativa di N e stima, allargato di +-2 % per l'approssimazione asintotica.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

#include "calib_support.hpp"

namespace {

using namespace cal;

double chi2_quantile(double nu, double z) {
  const double c = 2.0 / (9.0 * nu);
  return nu * std::pow(1.0 - c + z * std::sqrt(c), 3);
}

calibration_recipe plane_recipe(const set_def& s) {
  calibration_recipe r = recipe_of(s);
  for (calibration_input_spec* in : {&r.light, &r.dark, &r.flat, &r.dark_flat}) in->variance = variance_spec{variance_source::plane, std::nullopt};
  r.q_min = 0.01;
  r.q_rel_sigma_max = 1.0;
  return r;
}

void add_noise(image& im, std::mt19937_64& rng, double sigma) {
  std::normal_distribution<double> g(0.0, sigma);
  double* s = im.data<double>();
  const std::size_t n = im.samples.size() / sizeof(double);
  for (std::size_t i = 0; i < n; ++i) s[i] += g(rng);
}

TEST(CalibStats, T_M2E_21_monte_carlo_variance_of_c_matches_first_order_propagation) {
  constexpr std::uint32_t w = 64, h = 64;
  constexpr std::size_t n_px = w * h;
  constexpr int K = 2000;
  constexpr double sigma = 40.0;
  set_def s = empty_set(w, h, -64);
  std::vector<double> fc0(n_px), lc0(n_px);
  for (std::size_t i = 0; i < n_px; ++i) {
    fc0[i] = 9000.0 + 100.0 * static_cast<double>(i % 16) + 7.0 * static_cast<double>(i % 5);
    lc0[i] = 1000.0 + 5.0 * static_cast<double>(i % 97);
    s.dark_flat.v[i] = 200;
    s.flat.v[i] = 200 + fc0[i];
    s.dark.v[i] = 500;
    s.light.v[i] = 500 + lc0[i];
  }
  for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->variance = std::vector<double>(n_px, sigma * sigma);
  const calibration_images base = images_of(s);
  const calibration_recipe recipe = plane_recipe(s);

  std::vector<std::size_t> probe;
  for (std::size_t k = 0; k < 8; ++k) probe.push_back(131 + k * 509);
  std::vector<std::vector<double>> c(probe.size());
  std::vector<double> mean_reported(probe.size(), 0.0);
  std::mt19937_64 rng(20261029);
  for (int k = 0; k < K; ++k) {
    calibration_images im = base;
    for (image* x : {&im.light, &im.dark, &im.flat, &im.dark_flat}) add_noise(*x, rng, sigma);
    const calibration_result r = calibrate(recipe, std::move(im));
    for (std::size_t j = 0; j < probe.size(); ++j) {
      ASSERT_EQ(r.output.invalid.bits[probe[j]], 0);
      c[j].push_back(sample_at(r.output, probe[j]));
      mean_reported[j] += variance_at(r.output, probe[j]) / K;
    }
  }
  std::vector<double> fc_sorted = fc0;
  std::sort(fc_sorted.begin(), fc_sorted.end());
  const double n0 = (fc_sorted[n_px / 2 - 1] + fc_sorted[n_px / 2]) / 2;
  const double lo = chi2_quantile(K - 1, -3.83), hi = chi2_quantile(K - 1, 3.83);
  for (std::size_t j = 0; j < probe.size(); ++j) {
    const std::size_t i = probe[j];
    const double q0 = fc0[i] / n0;
    const double var_q = 2 * sigma * sigma / (n0 * n0);
    EXPECT_LE(var_q / (q0 * q0), 1e-4);
    const double predicted = 2 * sigma * sigma / (q0 * q0) + lc0[i] * lc0[i] * var_q / std::pow(q0, 4);
    double mean = 0;
    for (double v : c[j]) mean += v / K;
    double m2 = 0;
    for (double v : c[j]) m2 += (v - mean) * (v - mean);
    const double stat = m2 / predicted;  // (K-1) s^2 / Var
    EXPECT_GT(stat, lo) << "pixel " << i;
    EXPECT_LT(stat, hi) << "pixel " << i;
    EXPECT_NEAR(mean_reported[j] / predicted, 1.0, 0.02) << "pixel " << i;
  }
}

struct n_sample {
  double mean = 0, rel_var = 0, estimate = 0;
};

n_sample sample_normalization(const set_def& s, const calibration_images& base, const calibration_recipe& recipe, int K,
                              double sigma, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::vector<double> ns;
  double estimate = 0;
  for (int k = 0; k < K; ++k) {
    calibration_images im = base;
    add_noise(im.flat, rng, sigma);
    add_noise(im.dark_flat, rng, sigma);
    prepared_calibration p = prepare_calibration(recipe, std::move(im));
    ns.push_back(p.normalization()[0].n);
    estimate = p.normalization()[0].rel_var_estimate;
  }
  (void)s;
  n_sample out;
  for (double v : ns) out.mean += v / K;
  double m2 = 0;
  for (double v : ns) m2 += (v - out.mean) * (v - out.mean);
  out.rel_var = m2 / (K - 1) / (out.mean * out.mean);
  out.estimate = estimate;
  return out;
}

TEST(CalibStats, T_M2E_26_normalization_variance_estimate_uniform_and_vignetted_flat) {
  constexpr std::uint32_t w = 100, h = 100;
  constexpr int K = 3000;
  constexpr double sigma = 30.0;
  const auto make = [&](double gradient) {
    set_def s = empty_set(w, h, -64);
    for (std::uint32_t y = 0; y < h; ++y) {
      for (std::uint32_t x = 0; x < w; ++x) {
        const std::size_t i = static_cast<std::size_t>(y) * w + x;
        s.dark_flat.v[i] = 200;
        s.flat.v[i] = 200 + 10000.0 * (1.0 + gradient * (static_cast<double>(x) / (w - 1) - 0.5) * 2);
        s.dark.v[i] = 500;
        s.light.v[i] = 1500;
      }
    }
    for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->variance = std::vector<double>(static_cast<std::size_t>(w) * h, sigma * sigma);
    return s;
  };

  const set_def uniform = make(0.0);
  const calibration_images base = images_of(uniform);
  const calibration_recipe recipe = plane_recipe(uniform);
  const n_sample u = sample_normalization(uniform, base, recipe, K, sigma, 20261030);
  const double ratio = u.rel_var / u.estimate;
  const double lo = chi2_quantile(K - 1, -3.2905) / (K - 1) * 0.98;
  const double hi = chi2_quantile(K - 1, 3.2905) / (K - 1) * 1.02;
  std::cout << "T-M2E-26 flat uniforme: n_valid=10000 K=" << K << " rapporto=" << ratio << " intervallo=[" << lo << ", " << hi << "]\n";
  EXPECT_GT(ratio, lo);
  EXPECT_LT(ratio, hi);
  {
    prepared_calibration p = prepare_calibration(recipe, base);
    const channel_normalization& ch = p.normalization()[0];
    EXPECT_EQ(ch.n_valid, 10000u);
    EXPECT_FALSE(ch.iid_not_verified);
    ASSERT_TRUE(ch.fc_cv.has_value());
    EXPECT_LT(*ch.fc_cv, 0.01);
  }

  // Flat vignettato: il rapporto si misura e si registra come limite, senza gate.
  const set_def vignetted = make(0.05);
  const calibration_images vbase = images_of(vignetted);
  const n_sample v = sample_normalization(vignetted, vbase, recipe, 1000, sigma, 20261031);
  std::cout << "T-M2E-26 flat vignettato (+-5 % lineare): K=1000 rapporto=" << v.rel_var / v.estimate << " (limite misurato, nessun gate)\n";
  prepared_calibration p = prepare_calibration(recipe, vbase);
  EXPECT_TRUE(p.normalization()[0].iid_not_verified);
  EXPECT_GT(*p.normalization()[0].fc_cv, 0.01);
}

}  // namespace
