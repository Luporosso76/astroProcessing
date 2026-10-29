// M2.e: calibrazione dark_matched_flat_dark_flat sulle immagini in memoria. Oracolo razionale esatto
// (tests/fixtures/exact_rational.hpp), indipendente dal prodotto; frame scritti dal writer FITS indipendente.
//
// Tolleranze derivate prima della misura (ADR-16 §6, ADR-17):
//  - casi dyadic: zero;
//  - C, uscita f64: gamma_6 |C|; uscita f32: mezzo ulp f32 + gamma_6 |C|.
//    Conteggio: Lc 1, Fc 1, N pari 2 (Fc centrale + somma; il dimezzamento e' esatto), Q = Fc/N: 1, C = Lc/Q: 1.
//  - Var(C): gamma_36 Var(C). Tutti i termini sono non negativi.
//    Var(X) = (rn2 + max(X-off,0)/g)/n: ramo Poisson sub, div, add, div = 4 operazioni, ramo lettura mul, add, div = 3;
//    Var(Lc), Var(Fc): +1 somma = 5. Q: Fc 1 + N 2 + div 1 = 4; q2 = Q*Q: 2*4+1 = 9; q4 = q2*q2: 9+9+1 = 19.
//    vlc/q2: 5+9+1 = 15. Lc*Lc: 2*1+1 = 3; vq = vfc/(N*N): N*N 2*2+1 = 5, 5+5+1 = 11;
//    (Lc*Lc)*vq: 3+11+1 = 15; /q4: 15+19+1 = 35. Somma dei due termini: 35+1 = 36.
#include <gtest/gtest.h>

#include <limits>

#include "calib_support.hpp"

namespace {

using namespace cal;

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double inf = std::numeric_limits<double>::infinity();

std::vector<int> cfa_channels(const set_def& s, int x_off, int y_off) {
  std::vector<int> ch(static_cast<std::size_t>(s.w) * s.h);
  for (std::uint32_t y = 0; y < s.h; ++y) {
    for (std::uint32_t x = 0; x < s.w; ++x) ch[static_cast<std::size_t>(y) * s.w + x] = ((y + y_off) % 2) * 2 + (x + x_off) % 2;
  }
  return ch;
}

set_def dyadic_set(int bitpix) {
  set_def s = empty_set(8, 4, bitpix, 2.0);
  for (std::size_t i = 0; i < 32; ++i) {
    const double fc = i < 8 ? 512 : i < 24 ? 1024 : 2048;
    const double df = 1000 + static_cast<double>(i % 16);
    const double d = 2000 + static_cast<double>(i);
    s.dark_flat.v[i] = df;
    s.flat.v[i] = df + fc;
    s.dark.v[i] = d;
    s.light.v[i] = d + static_cast<double>(static_cast<int>(i * 37 % 101) - 20);
  }
  return s;
}

// Valori non dyadic con forte cancellazione L ~ D: scarto di 1e-7 (f64) o 3e-5 (f32, sopra lo spacing) su campioni da ~400.
set_def nondyadic_set(std::uint32_t w, std::uint32_t h, int bitpix, double egain = 2.0) {
  set_def s = empty_set(w, h, bitpix, egain);
  for (std::size_t i = 0; i < s.light.v.size(); ++i) {
    const double idx = static_cast<double>(i);
    const double l = stored(1000.0 / 3 + idx * 0.7, bitpix);
    s.light.v[i] = l;
    const double delta = bitpix == -32 ? 3e-5 : 1e-7;
    s.dark.v[i] = stored(l - delta * (1 + static_cast<double>(i % 5)), bitpix);
    const double df = stored(500.0 / 3 + idx * 0.01, bitpix);
    s.dark_flat.v[i] = df;
    s.flat.v[i] = stored(df + 20000.0 / 7 + static_cast<double>(i % 9) * 13.3, bitpix);
  }
  return s;
}

void expect_all_close(const calibration_result& r, const std::vector<oracle_pixel>& expected, unsigned m_c,
                      unsigned m_var, bool f32) {
  ASSERT_EQ(expected.size(), static_cast<std::size_t>(r.output.geometry.width) * r.output.geometry.height);
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ASSERT_EQ(r.output.invalid.bits[i], 0) << "pixel " << i;
    EXPECT_TRUE(close_to(expected[i].c, sample_at(r.output, i), m_c, f32)) << "C pixel " << i;
    EXPECT_TRUE(close_to(expected[i].var, variance_at(r.output, i), m_var, f32)) << "Var pixel " << i;
  }
  EXPECT_TRUE(operator_output_invariants_hold(r.output));
}

// ---------------------------------------------------------------- T-M2E-01 / 02 / 03 / 20 / 24

TEST(CalibM2E, T_M2E_01_dyadic_exact_zero_tolerance) {
  for (const int bitpix : {-64, -32}) {
    const set_def s = dyadic_set(bitpix);
    const calibration_result r = calibrate(recipe_of(s), images_of(s));
    EXPECT_EQ(r.output.type, bitpix == -32 ? sample_type::f32 : sample_type::f64);
    const auto expected = oracle_run(s, oracle_params{}, all_zero(s), 1);
    for (std::size_t i = 0; i < 32; ++i) {
      ASSERT_EQ(r.output.invalid.bits[i], 0);
      EXPECT_TRUE(close_to(expected[i].c, sample_at(r.output, i), 0, false)) << "C " << i;
      // f64: varianza esatta; f32: un solo arrotondamento finale.
      EXPECT_TRUE(close_to(expected[i].var, variance_at(r.output, i), 0, bitpix == -32)) << "Var " << i;
    }
    EXPECT_EQ(r.report.find("normalization")->items()[0].find("N")->as_number(), 1024.0);
  }
}

TEST(CalibM2E, T_M2E_02_nondyadic_cancellation_even_and_odd_n) {
  for (const int bitpix : {-64, -32}) {
    for (const auto& dims : {std::pair<std::uint32_t, std::uint32_t>{6, 4}, {5, 5}, {3, 1}}) {
      const set_def s = nondyadic_set(dims.first, dims.second, bitpix, 1.5);
      const calibration_result r = calibrate(recipe_of(s, 2.3), images_of(s));
      const auto expected = oracle_run(s, oracle_params{1.5, 10.0, 2.3, 4}, all_zero(s), 1);
      expect_all_close(r, expected, 6, 36, bitpix == -32);
    }
  }
}

TEST(CalibM2E, T_M2E_03_bias_in_dark_no_double_subtraction) {
  const set_def s = synth_set(16, 12, -64);
  const calibration_result r = calibrate(recipe_of(s), images_of(s));
  std::vector<rat> q;
  for (std::size_t i = 0; i < s.flat.v.size(); ++i) q.push_back(exact::from_double(s.flat.v[i]) - exact::from_double(s.dark_flat.v[i]));
  const rat n = exact::median(q);
  for (std::uint32_t y = 0; y < 12; ++y) {
    for (std::uint32_t x = 0; x < 16; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * 16 + x;
      const rat truth = exact::from_double(fx::synth_sky(x, y, 0) / 8192.0) * n;  // S * N / K, K = 1
      ASSERT_TRUE(close_to(truth, sample_at(r.output, i), 6, false)) << i;
      const double with_extra_bias_subtraction = (s.light.v[i] - s.dark.v[i] - fx::synth_bias(x, y)) * exact::to_ld(n) /
                                                  (s.flat.v[i] - s.dark_flat.v[i]);
      EXPECT_GT(std::fabs(with_extra_bias_subtraction - sample_at(r.output, i)), 100.0);
    }
  }
}

TEST(CalibM2E, T_M2E_24_variance_invariants_valid_and_invalid_pixels) {
  set_def s = dyadic_set(-64);
  s.flat.v[3] = s.dark_flat.v[3];  // Fc = 0
  const calibration_result r = calibrate(recipe_of(s), images_of(s));
  ASSERT_EQ(r.output.invalid.bits[3], invalid_reason::q_nonpositive);
  EXPECT_TRUE(std::isnan(sample_at(r.output, 3)));
  EXPECT_TRUE(std::isnan(variance_at(r.output, 3)));
  for (std::size_t i = 0; i < 32; ++i) {
    if (i == 3) continue;
    EXPECT_TRUE(std::isfinite(variance_at(r.output, i)));
    EXPECT_GE(variance_at(r.output, i), 0.0);
  }
  EXPECT_TRUE(operator_output_invariants_hold(r.output));
}

TEST(CalibM2E, T_M2E_25_value_below_offset_keeps_read_noise_only) {
  EXPECT_EQ(exact::cmp(var_model(5.0, oracle_params{}, 4), exact::from_double(4.0) / exact::from_u(4)), 0);
  set_def s = dyadic_set(-64);
  for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->v[0] = 5.0;
  s.flat.v[0] = 5.0 + 1024;  // Fc = 1019 con DF sotto l'offset
  s.dark_flat.v[0] = 5.0;
  const calibration_result r = calibrate(recipe_of(s), images_of(s));
  const auto expected = oracle_run(s, oracle_params{}, all_zero(s), 1);
  ASSERT_EQ(r.output.invalid.bits[0], 0);
  EXPECT_TRUE(close_to(expected[0].var, variance_at(r.output, 0), 36, false));
}

// ---------------------------------------------------------------- T-M2E-04 / 05: maschere di Q
// Il Q non finito con input validi si ottiene solo per overflow (Fc = inf da F, DF finiti); un campione NaN
// nell'input e' un pixel di input invalido, mai q_nonfinite.

set_def mask_set() {
  set_def s = empty_set(8, 4, -64);
  const double fcs[8] = {0, -5, 30, 60, 90, 2000, 0, 1000};
  for (std::size_t i = 0; i < 32; ++i) {
    s.dark_flat.v[i] = 100;
    s.flat.v[i] = 100 + (i < 8 ? fcs[i] : 1000);
    s.dark.v[i] = 500;
    s.light.v[i] = 510;
  }
  s.flat.v[6] = 1.5e308;
  s.dark_flat.v[6] = -1.5e308;
  for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->variance = std::vector<double>(32, 1.0);
  return s;
}

calibration_recipe plane_recipe(const set_def& s, double q_min, double rel_max) {
  calibration_recipe r = recipe_of(s);
  for (calibration_input_spec* in : {&r.light, &r.dark, &r.flat, &r.dark_flat}) in->variance = variance_spec{variance_source::plane, std::nullopt};
  r.q_min = q_min;
  r.q_rel_sigma_max = rel_max;
  return r;
}

TEST(CalibM2E, T_M2E_04_05_q_bits_separate_and_together) {
  constexpr std::uint16_t below = invalid_reason::q_below_min, unrel = invalid_reason::q_unreliable;
  const set_def s = mask_set();
  // Soglia di affidabilita': sqrt(Var(Q))/Q > 0.02 con Var(Q) = 2/1000^2, cioe' Q < 0.0707.
  {
    const calibration_result r = calibrate(plane_recipe(s, 0.05, 0.02), images_of(s));
    const auto& m = r.output.invalid.bits;
    EXPECT_EQ(m[0], invalid_reason::q_nonpositive);
    EXPECT_EQ(m[1], invalid_reason::q_nonpositive);
    EXPECT_EQ(m[2], below | unrel);
    EXPECT_EQ(m[3], unrel);
    EXPECT_EQ(m[4], 0);
    EXPECT_EQ(m[5], 0);  // Q = 2 > 1 resta valido
    EXPECT_EQ(m[6], invalid_reason::q_nonfinite);
    EXPECT_EQ(m[7], 0);
    EXPECT_DOUBLE_EQ(sample_at(r.output, 5), 5.0);  // Lc = 10, Q = 2
    EXPECT_EQ(sample_at(r.output, 7), 10.0);
    for (const std::size_t i : {0, 1, 2, 3, 6}) {
      EXPECT_TRUE(std::isnan(sample_at(r.output, i)));
      EXPECT_TRUE(std::isnan(variance_at(r.output, i)));
    }
    EXPECT_TRUE(operator_output_invariants_hold(r.output));
  }
  {
    const calibration_result r = calibrate(plane_recipe(s, 0.1, 0.02), images_of(s));
    const auto& m = r.output.invalid.bits;
    EXPECT_EQ(m[2], below | unrel);
    EXPECT_EQ(m[3], below | unrel);
    EXPECT_EQ(m[4], below);
    EXPECT_EQ(m[5], 0);
  }
}

TEST(CalibM2E, T_M2E_04_nan_flat_sample_is_input_invalid_not_q_bit) {
  set_def s = mask_set();
  s.flat.v[9] = std::numeric_limits<double>::quiet_NaN();
  const calibration_result r = calibrate(plane_recipe(s, 0.05, 0.02), images_of(s));
  EXPECT_EQ(r.output.invalid.bits[9], invalid_reason::source_nonfinite | invalid_reason::master_invalid);
}

// ADR-16 §7 (aggiunte di R2): una Var(Q) non rappresentabile calcolata da input finiti e' result_nonfinite, mai
// q_unreliable. Fc = N = 1e-150 ovunque (Q = 1, N^2 = 1e-300 normale); la mediana di Var(Fc) resta normale.
TEST(CalibM2E, T_M2E_04_var_q_overflow_is_result_nonfinite_not_q_unreliable) {
  set_def s = empty_set(8, 4, -64);
  std::vector<double> vf(32, 1e-301), vdf(32, 1e-301);
  vf[0] = 5e299;  // Var(Fc) = 1e300 finita, Var(Fc)/N^2 = 1e600: overflow del quoziente
  vdf[0] = 5e299;
  vf[1] = 1e-310;  // Var(F) + Var(DF) sottonormale da termini non tutti nulli
  vdf[1] = 0;
  for (std::size_t i = 0; i < 32; ++i) {
    s.dark_flat.v[i] = 0;
    s.flat.v[i] = 1e-150;
    s.dark.v[i] = 500;
    s.light.v[i] = 510;
  }
  s.light.variance = std::vector<double>(32, 1.0);
  s.dark.variance = std::vector<double>(32, 1.0);
  s.flat.variance = vf;
  s.dark_flat.variance = vdf;
  const calibration_result r = calibrate(plane_recipe(s, 0.05, 1.0), images_of(s));
  EXPECT_EQ(r.output.invalid.bits[0], invalid_reason::result_nonfinite);
  EXPECT_EQ(r.output.invalid.bits[1], invalid_reason::result_nonfinite);
  for (std::size_t i = 2; i < 32; ++i) EXPECT_EQ(r.output.invalid.bits[i], 0) << i;
  EXPECT_TRUE(operator_output_invariants_hold(r.output));
}

TEST(CalibM2E, T_M2E_28_half_ulp_f32_at_subnormal_boundaries) {
  const auto pow2 = [](int k) {
    return k >= 0 ? rat{exact::shl(exact::from_u64(1), static_cast<unsigned>(k)), exact::from_u64(1)}
                   : rat{exact::from_u64(1), exact::shl(exact::from_u64(1), static_cast<unsigned>(-k))};
  };
  const auto same = [](const rat& a, const rat& b) { return exact::cmp(a, b) == 0; };
  EXPECT_TRUE(same(half_ulp_f32(pow2(0)), pow2(-24)));
  EXPECT_TRUE(same(half_ulp_f32(pow2(-125)), pow2(-149)));
  EXPECT_TRUE(same(half_ulp_f32(pow2(-126)), pow2(-150)));   // minimo normale: spacing 2^-149
  EXPECT_TRUE(same(half_ulp_f32(pow2(-127)), pow2(-150)));   // subnormale: spacing 2^-149
  EXPECT_TRUE(same(half_ulp_f32(pow2(-149)), pow2(-150)));   // minimo subnormale
  EXPECT_TRUE(same(half_ulp_f32(exact::from_u(0)), exact::from_u(0)));
}

// Intermedi di varianza: N^2 non finito ferma con errore dichiarato (Var(Q) non diventa zero in silenzio).
TEST(CalibM2E, T_M2E_29_normalization_square_overflow_stops) {
  set_def s = empty_set(8, 4, -64);
  for (std::size_t i = 0; i < 32; ++i) {
    s.dark_flat.v[i] = 0;
    s.flat.v[i] = 1e155;
    s.dark.v[i] = 500;
    s.light.v[i] = 510;
  }
  s.light.variance = std::vector<double>(32, 1.0);
  s.dark.variance = std::vector<double>(32, 1.0);
  s.flat.variance = std::vector<double>(32, 5e307);
  s.dark_flat.variance = std::vector<double>(32, 5e307);
  EXPECT_STOP(calibrate(plane_recipe(s, 0.05, 0.02), images_of(s)), error_category::numeric, "calib.variance_overflow");
}

TEST(CalibM2E, T_M2E_29_nonfinite_variance_term_stops_before_the_median) {
  // Gain minuscolo: a/gain overflowa solo sul pixel con Fc enorme, il modello restituisce NaN per quel termine.
  set_def s = empty_set(8, 4, -64, 1e-10);
  for (std::size_t i = 0; i < 32; ++i) {
    s.dark_flat.v[i] = 0;
    s.flat.v[i] = 1000;
    s.dark.v[i] = 500;
    s.light.v[i] = 510;
  }
  s.flat.v[9] = 1e300;
  EXPECT_STOP(calibrate(recipe_of(s), images_of(s)), error_category::numeric, "calib.variance_overflow");
}

TEST(CalibM2E, T_M2E_29_normalization_square_underflow_stops) {
  // N = 1e-160: N^2 = 1e-320 e' subnormale e perde precisione.
  set_def s = empty_set(8, 4, -64);
  for (std::size_t i = 0; i < 32; ++i) {
    s.dark_flat.v[i] = 0;
    s.flat.v[i] = 1e-160;
    s.dark.v[i] = 500;
    s.light.v[i] = 510;
  }
  for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->variance = std::vector<double>(32, 1.0);
  EXPECT_STOP(calibrate(plane_recipe(s, 0.05, 10.0), images_of(s)), error_category::numeric, "calib.variance_overflow");
}

TEST(CalibM2E, T_M2E_29_pixel_intermediate_overflow_is_result_nonfinite) {
  set_def s = mask_set();
  for (std::size_t i = 8; i < 32; ++i) s.flat.v[i] = 1100;
  s.flat.v[6] = 100 + 1000;
  s.dark_flat.v[6] = 100;
  s.flat.v[9] = 100 + 1e303;  // Q = 1e300: Q^2 e Q^4 non finiti
  const calibration_result r = calibrate(plane_recipe(s, 0.05, 0.02), images_of(s));
  EXPECT_EQ(r.output.invalid.bits[9], invalid_reason::result_nonfinite);
  EXPECT_TRUE(std::isnan(sample_at(r.output, 9)));
  EXPECT_TRUE(std::isnan(variance_at(r.output, 9)));
  // Il CV spaziale di Fc non e' finito: il report lo dichiara null e l'ipotesi iid non e' verificata.
  const json& ch = r.report.find("normalization")->items()[0];
  EXPECT_EQ(ch.find("fc_cv")->type(), json::kind::null);
  EXPECT_EQ(ch.find("iid_assumption")->as_string(), "not_verified");
  EXPECT_NE(r.report_text.find("fc_cv: null"), std::string::npos);
}

TEST(CalibM2E, T_M2E_30_report_records_image_fields_with_origin) {
  set_def s = synth_set(8, 4, -64);
  s.light.hdr.with_unit_and_linear = false;
  s.light.hdr.xbin = 1;
  calibration_recipe rec = recipe_of(s);
  rec.light.declared_fields.unit = pixel_unit::adu;
  rec.light.declared_fields.lin = linearity::linear;
  rec.light.declared_fields.binning_x = 1;  // uguale all'header: l'origine resta header
  const calibration_result r = calibrate(rec, images_of(s));
  const json& light = *r.report.find("inputs")->find("light")->find("image_fields");
  EXPECT_EQ(at(light, {"unit", "origin"}).as_string(), "declared");
  EXPECT_EQ(at(light, {"unit", "value"}).as_string(), "adu");
  EXPECT_EQ(at(light, {"linearity", "origin"}).as_string(), "declared");
  EXPECT_EQ(at(light, {"linearity", "value"}).as_string(), "linear");
  EXPECT_EQ(at(light, {"binning_x", "origin"}).as_string(), "header");
  EXPECT_EQ(at(light, {"binning_y", "value"}).as_integer(), 1);
  const json& dark = *r.report.find("inputs")->find("dark")->find("image_fields");
  EXPECT_EQ(at(dark, {"unit", "origin"}).as_string(), "header");
  EXPECT_NE(r.report_text.find("image_fields"), std::string::npos);
  EXPECT_NE(r.report_text.find("origin: \"declared\""), std::string::npos);
  // binning dichiarato quando l'header non lo porta
  calibration_images im = images_of(s);
  im.light.geometry.binning_x.reset();
  const calibration_result r2 = calibrate(rec, std::move(im));
  EXPECT_EQ(at(*r2.report.find("inputs")->find("light")->find("image_fields"), {"binning_x", "origin"}).as_string(), "declared");
}

// Underflow silenzioso: operandi non nulli con risultato zero o subnormale non sono pubblicati come validi.
set_def underflow_base(int bitpix) {
  set_def s = empty_set(8, 4, bitpix);
  for (std::size_t i = 0; i < 32; ++i) {
    s.dark_flat.v[i] = 100;
    s.flat.v[i] = 1100;
    s.dark.v[i] = 500;
    s.light.v[i] = 510;
  }
  for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) f->variance = std::vector<double>(32, 1.0);
  return s;
}

TEST(CalibM2E, T_M2E_32_variance_underflow_is_result_nonfinite) {
  // N = Fc = 1e150, Lc = 1e150, Var(Fc) = 1e-30 sul pixel 0: Var(Q) = 1e-330 non e' rappresentabile (prima: Var(C) = 0).
  set_def s = empty_set(8, 4, -64);
  for (std::size_t i = 0; i < 32; ++i) {
    s.dark_flat.v[i] = 100;
    s.flat.v[i] = 100 + 1e150;
    s.dark.v[i] = i == 0 ? 0 : 500;
    s.light.v[i] = i == 0 ? 1e150 : 510;
  }
  s.light.variance = std::vector<double>(32, 1.0);
  s.dark.variance = std::vector<double>(32, 1.0);
  s.flat.variance = std::vector<double>(32, 5e299);
  s.dark_flat.variance = std::vector<double>(32, 5e299);
  s.light.variance->at(0) = 0;
  s.dark.variance->at(0) = 0;
  s.flat.variance->at(0) = 5e-31;
  s.dark_flat.variance->at(0) = 5e-31;
  const calibration_result r = calibrate(plane_recipe(s, 0.05, 10.0), images_of(s));
  EXPECT_EQ(r.output.invalid.bits[0], invalid_reason::result_nonfinite);
  EXPECT_TRUE(std::isnan(sample_at(r.output, 0)));
  EXPECT_TRUE(std::isnan(variance_at(r.output, 0)));
  for (std::size_t i = 1; i < 32; ++i) EXPECT_EQ(r.output.invalid.bits[i], 0) << i;
  EXPECT_TRUE(operator_output_invariants_hold(r.output));
}

TEST(CalibM2E, T_M2E_32_quotient_underflow_is_result_nonfinite) {
  set_def s = underflow_base(-64);
  s.light.v[0] = 1e-300;
  s.dark.v[0] = 0;
  s.flat.v[0] = 100 + 1e13;  // Q = 1e10: C = 1e-310 subnormale
  const calibration_result r = calibrate(plane_recipe(s, 0.05, 10.0), images_of(s));
  EXPECT_EQ(r.output.invalid.bits[0], invalid_reason::result_nonfinite);
  for (std::size_t i = 1; i < 32; ++i) EXPECT_EQ(r.output.invalid.bits[i], 0) << i;
}

TEST(CalibM2E, T_M2E_32_f32_subnormal_result_from_normal_f64_is_result_nonfinite) {
  set_def s = underflow_base(-32);
  s.light.v[0] = 1e-40;  // C = 1e-40: normale in f64, subnormale in f32
  s.dark.v[0] = 0;
  const calibration_result r = calibrate(plane_recipe(s, 0.05, 10.0), images_of(s));
  ASSERT_EQ(r.output.type, sample_type::f32);
  EXPECT_EQ(r.output.invalid.bits[0], invalid_reason::result_nonfinite);
  for (std::size_t i = 1; i < 32; ++i) EXPECT_EQ(r.output.invalid.bits[i], 0) << i;
}

TEST(CalibM2E, T_M2E_32_model_step_underflow_is_result_nonfinite) {
  // Rumore di lettura 1e-170: rn^2 = 1e-340 non e' rappresentabile; il parametro e' respinto.
  set_def s = synth_set(8, 4, -64);
  EXPECT_STOP(calibrate(recipe_of(s, 1e-170), images_of(s)), error_category::invalid_input, "var.param.read_noise_adu");
}

// ---------------------------------------------------------------- T-M2E-06 / 07: normalizzazione

set_def cfa_set(int x_off, int y_off) {
  set_def s = empty_set(8, 8, -64);
  for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) {
    f->hdr.bayer = "RGGB";
    f->hdr.bayer_x = x_off;
    f->hdr.bayer_y = y_off;
  }
  const double level[4] = {1000, 1200, 1100, 900};
  const auto ch = cfa_channels(s, x_off, y_off);
  for (std::size_t i = 0; i < 64; ++i) {
    s.dark_flat.v[i] = 100 + static_cast<double>(i % 5);
    s.flat.v[i] = s.dark_flat.v[i] + level[ch[i]] + static_cast<double>(i % 3);
    s.dark.v[i] = 300;
    s.light.v[i] = 300 + level[ch[i]] * 0.5 + static_cast<double>(i % 7);
  }
  return s;
}

TEST(CalibM2E, T_M2E_06_global_and_per_channel_normalization_with_g1_neq_g2) {
  for (const auto& off : {std::pair<int, int>{0, 0}, {1, 0}, {0, 1}, {1, 1}}) {
    const set_def s = cfa_set(off.first, off.second);
    calibration_recipe rec = recipe_of(s);
    const calibration_result global = calibrate(rec, images_of(s));
    expect_all_close(global, oracle_run(s, oracle_params{}, all_zero(s), 1), 6, 36, false);
    EXPECT_EQ(global.report.find("normalization")->items().size(), 1u);

    rec.normalization = normalization_mode::cfa_channel_median;
    const calibration_result per = calibrate(rec, images_of(s));
    expect_all_close(per, oracle_run(s, oracle_params{}, cfa_channels(s, off.first, off.second), 4), 6, 36, false);
    const auto& ch = per.report.find("normalization")->items();
    ASSERT_EQ(ch.size(), 4u);
    EXPECT_EQ(ch[0].find("channel")->as_string(), "R");
    EXPECT_EQ(ch[1].find("channel")->as_string(), "G1");
    EXPECT_EQ(ch[2].find("channel")->as_string(), "G2");
    EXPECT_EQ(ch[3].find("channel")->as_string(), "B");
    EXPECT_NE(ch[1].find("N")->as_number(), ch[2].find("N")->as_number());
    std::uint64_t n_total = 0;
    for (const json& c : ch) n_total += static_cast<std::uint64_t>(c.find("n_valid")->as_integer());
    EXPECT_EQ(n_total, 64u);
  }
}

TEST(CalibM2E, T_M2E_07_normalization_errors_and_exclusions) {
  {
    set_def s = dyadic_set(-64);
    s.flat.v = s.dark_flat.v;  // Fc = 0 ovunque
    EXPECT_STOP(calibrate(recipe_of(s), images_of(s)), error_category::numeric, "calib.normalization_invalid");
  }
  {
    set_def s = dyadic_set(-64);
    calibration_recipe rec = recipe_of(s);
    rec.normalization = normalization_mode::cfa_channel_median;
    EXPECT_STOP(calibrate(rec, images_of(s)), error_category::invalid_input, "calib.normalization_requires_cfa");
  }
  {
    set_def s = dyadic_set(-64);
    calibration_recipe rec = recipe_of(s);
    rec.flat.declared.saturation_adu = metadata_value<double>{0.5, {metadata_origin::declared, {}}};
    EXPECT_STOP(calibrate(rec, images_of(s)), error_category::numeric, "calib.normalization_invalid");
  }
  {
    // Pixel con bit di input esclusi da N; Fc <= 0 inclusi e contati.
    set_def s = empty_set(6, 4, -64);
    const double fc[24] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
                           1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, -3,   0,    1000};
    for (std::size_t i = 0; i < 24; ++i) {
      s.dark_flat.v[i] = 50;
      s.flat.v[i] = 50 + fc[i];
      s.dark.v[i] = 200;
      s.light.v[i] = 300;
    }
    calibration_images im = images_of(s);
    im.flat.invalid.bits[0] = invalid_reason::source_flagged;
    im.flat.data<double>()[0] = 1e9;
    im.dark_flat.invalid.bits[1] = invalid_reason::source_flagged;
    im.dark_flat.data<double>()[1] = -1e9;
    const calibration_result r = calibrate(recipe_of(s), std::move(im));
    const json& ch = r.report.find("normalization")->items()[0];
    EXPECT_EQ(ch.find("n_valid")->as_integer(), 22);
    EXPECT_EQ(ch.find("fc_nonpositive_in_N")->as_integer(), 2);
    EXPECT_EQ(ch.find("N")->as_number(), 1000.0);
  }
}

// ---------------------------------------------------------------- T-M2E-08 / 09 / 19: compatibilita'

struct mismatch_case {
  const char* field;
  const char* code;
  std::function<void(set_def&)> edit;
};

TEST(CalibM2E, T_M2E_08_each_compatibility_field_out_of_rule_stops) {
  const std::vector<mismatch_case> cases = {
      {"light-dark camera", "calib.mismatch.camera", [](set_def& s) { s.dark.hdr.camera = "OTHER"; }},
      {"light-dark gain", "calib.mismatch.gain", [](set_def& s) { s.dark.hdr.gain = 101; }},
      {"light-dark offset", "calib.mismatch.offset_adu", [](set_def& s) { s.dark.hdr.offset = 11; }},
      {"light-dark exposure", "calib.mismatch.exposure_s", [](set_def& s) { s.dark.hdr.exposure = 61; }},
      {"light-dark temperature", "calib.mismatch.temperature_c", [](set_def& s) { s.dark.hdr.temperature = -20; }},
      {"light-dark binning_x", "calib.mismatch.binning_x", [](set_def& s) { s.dark.hdr.xbin = 2; }},
      {"light-dark binning_y", "calib.mismatch.binning_y", [](set_def& s) { s.dark.hdr.ybin = 2; }},
      {"light-dark cfa", "calib.mismatch.cfa", [](set_def& s) { s.dark.hdr.bayer = "RGGB"; }},
      {"flat-df camera", "calib.mismatch.camera", [](set_def& s) { s.dark_flat.hdr.camera = "OTHER"; }},
      {"flat-df gain", "calib.mismatch.gain", [](set_def& s) { s.dark_flat.hdr.gain = 101; }},
      {"flat-df offset", "calib.mismatch.offset_adu", [](set_def& s) { s.dark_flat.hdr.offset = 11; }},
      {"flat-df exposure", "calib.mismatch.exposure_s", [](set_def& s) { s.dark_flat.hdr.exposure = 6; }},
      {"flat-df temperature", "calib.mismatch.temperature_c", [](set_def& s) { s.dark_flat.hdr.temperature = -20; }},
      {"flat-df binning_x", "calib.mismatch.binning_x", [](set_def& s) { s.dark_flat.hdr.xbin = 2; }},
      {"light-flat filter", "calib.mismatch.filter", [](set_def& s) {
         s.flat.hdr.filter = "R";
         s.dark_flat.hdr.filter = "R";
       }},
      {"light-flat camera", "calib.mismatch.camera", [](set_def& s) {
         s.flat.hdr.camera = "OTHER";
         s.dark_flat.hdr.camera = "OTHER";
       }},
      {"light-flat binning_y", "calib.mismatch.binning_y", [](set_def& s) {
         s.flat.hdr.ybin = 2;
         s.dark_flat.hdr.ybin = 2;
       }},
      {"light-flat cfa", "calib.mismatch.cfa", [](set_def& s) {
         s.flat.hdr.bayer = "RGGB";
         s.dark_flat.hdr.bayer = "RGGB";
       }},
      {"light-dark roi_x", "calib.mismatch.roi_x", [](set_def& s) {
         s.light.hdr.extra = {fx::card_int("XORGSUBF", 0)};
         s.dark.hdr.extra = {fx::card_int("XORGSUBF", 5)};
       }},
      {"light-dark roi_y", "calib.mismatch.roi_y", [](set_def& s) {
         s.light.hdr.extra = {fx::card_int("YORGSUBF", 0)};
         s.dark.hdr.extra = {fx::card_int("YORGSUBF", 5)};
       }},
      {"flat-df readout_mode", "calib.mismatch.readout_mode", [](set_def& s) {
         s.flat.hdr.extra = {fx::card_str("READOUTM", "A")};
         s.dark_flat.hdr.extra = {fx::card_str("READOUTM", "B")};
       }},
      {"light-flat row_order", "calib.mismatch.row_order", [](set_def& s) {
         s.light.hdr.extra = {fx::card_str("ROWORDER", "TOP-DOWN")};
         s.flat.hdr.extra = {fx::card_str("ROWORDER", "BOTTOM-UP")};
         s.dark.hdr.extra = {fx::card_str("ROWORDER", "TOP-DOWN")};
         s.dark_flat.hdr.extra = {fx::card_str("ROWORDER", "BOTTOM-UP")};
       }},
  };
  for (const mismatch_case& c : cases) {
    set_def s = synth_set(8, 4, -64);
    c.edit(s);
    SCOPED_TRACE(c.field);
    EXPECT_STOP(calibrate(recipe_of(s), images_of(s)), error_category::incompatible, c.code);
  }
}

TEST(CalibM2E, T_M2E_08_geometry_mismatch_stops) {
  const set_def s = synth_set(8, 4, -64);
  {
    calibration_images im = images_of(s);
    frame_def wide = s.dark;
    wide.v.assign(36, 1.0);
    im.dark = decode(wide, 9, 4);
    EXPECT_STOP(calibrate(recipe_of(s), std::move(im)), error_category::incompatible, "calib.mismatch.width");
  }
  {
    calibration_images im = images_of(s);
    frame_def tall = s.flat;
    tall.v.assign(40, 1.0);
    im.flat = decode(tall, 8, 5);
    frame_def tall_df = s.dark_flat;
    tall_df.v.assign(40, 1.0);
    im.dark_flat = decode(tall_df, 8, 5);
    EXPECT_STOP(calibrate(recipe_of(s), std::move(im)), error_category::incompatible, "calib.mismatch.height");
  }
}

TEST(CalibM2E, T_M2E_09_missing_declared_and_conflicting_metadata) {
  const set_def s = synth_set(8, 4, -64);
  const auto declared = [](double v) { return metadata_value<double>{v, {metadata_origin::declared, {}}}; };
  struct field_case {
    const char* name;
    std::function<void(calibration_images&)> drop;
  };
  const std::vector<field_case> fields = {
      {"exposure_s", [](calibration_images& im) { im.dark.metadata.exposure_s.reset(); }},
      {"gain", [](calibration_images& im) { im.dark.metadata.gain.reset(); }},
      {"offset_adu", [](calibration_images& im) { im.dark.metadata.offset_adu.reset(); }},
      {"camera", [](calibration_images& im) { im.dark.metadata.camera.reset(); }},
      {"temperature_c", [](calibration_images& im) { im.dark.metadata.temperature_c.reset(); }},
      {"binning_x", [](calibration_images& im) { im.dark.geometry.binning_x.reset(); }},
      {"binning_y", [](calibration_images& im) { im.dark.geometry.binning_y.reset(); }},
      {"filter", [](calibration_images& im) { im.flat.metadata.filter.reset(); }},
  };
  for (const field_case& f : fields) {
    calibration_images im = images_of(s);
    f.drop(im);
    SCOPED_TRACE(f.name);
    EXPECT_STOP(calibrate(recipe_of(s), std::move(im)), error_category::metadata_missing,
                std::string("calib.metadata.") + f.name);
  }
  {
    calibration_images im = images_of(s);
    im.dark.metadata.exposure_s.reset();
    calibration_recipe rec = recipe_of(s);
    rec.dark.declared.exposure_s = declared(60);
    const calibration_result r = calibrate(rec, std::move(im));
    EXPECT_EQ(at(r.report, {"inputs", "dark", "metadata", "exposure_s", "origin"}).as_string(), "declared");
  }
  {
    calibration_recipe rec = recipe_of(s);
    rec.dark.declared.exposure_s = declared(61);
    EXPECT_STOP(calibrate(rec, images_of(s)), error_category::incompatible, "metadata.declared_conflict");
  }
  {
    calibration_images im = images_of(s);
    im.dark.metadata.temperature_c.reset();
    im.dark_flat.metadata.temperature_c.reset();
    calibration_recipe rec = recipe_of(s);
    rec.temp_policy = temperature_policy::not_applicable_declared;
    const calibration_result r = calibrate(rec, std::move(im));
    EXPECT_NE(std::find(r.warnings.begin(), r.warnings.end(), "temperature_comparison_skipped:light-dark"), r.warnings.end());
    bool skipped = false;
    for (const json& c : r.report.find("compatibility")->items()) {
      if (c.find("field")->as_string() == "temperature_c" && c.find("result")->as_string() == "skipped") skipped = true;
    }
    EXPECT_TRUE(skipped);
  }
}

TEST(CalibM2E, T_M2E_19_absent_optional_fields_are_listed_as_hypotheses) {
  const set_def s = synth_set(8, 4, -64);
  const calibration_result r = calibrate(recipe_of(s), images_of(s));
  const auto& hyp = r.report.find("assumptions")->items();
  const auto has = [&](const std::string& text) {
    return std::any_of(hyp.begin(), hyp.end(), [&](const json& h) { return h.as_string() == text; });
  };
  EXPECT_TRUE(has("not_verified:light-dark:roi_x:absent_in=light,dark"));
  EXPECT_TRUE(has("not_verified:flat-dark_flat:roi_y:absent_in=flat,dark_flat"));
  EXPECT_TRUE(has("not_verified:light-flat:readout_mode:absent_in=light,flat"));
  EXPECT_TRUE(has("not_verified:light-flat:row_order:absent_in=light,flat"));
  EXPECT_TRUE(has("amp_glow_not_modeled"));

  set_def partial = synth_set(8, 4, -64);
  partial.dark.hdr.extra = {fx::card_int("XORGSUBF", 0)};
  const calibration_result r2 = calibrate(recipe_of(partial), images_of(partial));
  bool one_side = false;
  for (const json& h : r2.report.find("assumptions")->items()) {
    if (h.as_string() == "not_verified:light-dark:roi_x:absent_in=light") one_side = true;
  }
  EXPECT_TRUE(one_side);
}

// ---------------------------------------------------------------- T-M2E-10 / 17 / 18: ruoli e stati

TEST(CalibM2E, T_M2E_10_master_states_roles_and_imagetyp) {
  const set_def s = synth_set(8, 4, -64);
  const auto run = [&](const std::function<void(calibration_recipe&)>& edit) {
    calibration_recipe rec = recipe_of(s);
    edit(rec);
    return calibrate(rec, images_of(s));
  };
  EXPECT_STOP(run([](calibration_recipe& r) { r.dark.descriptor->additive = additive_state::unknown; }),
              error_category::metadata_missing, "calib.master_state_unknown");
  EXPECT_STOP(run([](calibration_recipe& r) { r.dark.descriptor->scaling.reset(); }),
              error_category::metadata_missing, "calib.master_state_unknown");
  EXPECT_STOP(run([](calibration_recipe& r) { r.flat.descriptor->flat = flat_state::unknown; }),
              error_category::metadata_missing, "calib.master_state_unknown");
  EXPECT_STOP(run([](calibration_recipe& r) { r.dark_flat.descriptor.reset(); }),
              error_category::metadata_missing, "calib.master_state_unknown");
  EXPECT_STOP(run([](calibration_recipe& r) { r.flat.descriptor->flat = flat_state::calibrated; }),
              error_category::unsupported, "calib.master_state_m2");
  EXPECT_STOP(run([](calibration_recipe& r) { r.flat.descriptor->flat = flat_state::corrected_unnormalized; }),
              error_category::unsupported, "calib.master_state_m2");
  EXPECT_STOP(run([](calibration_recipe& r) { r.dark.descriptor->additive = additive_state::bias_subtracted; }),
              error_category::unsupported, "calib.master_state_m2");
  EXPECT_STOP(run([](calibration_recipe& r) { r.dark_flat.descriptor->scaling = scaling_state::scaled; }),
              error_category::unsupported, "calib.master_state_m2");
  EXPECT_STOP(run([](calibration_recipe& r) { r.dark.descriptor->role = master_role::flat; }),
              error_category::invalid_input, "calib.role_slot");
  EXPECT_STOP(run([](calibration_recipe& r) { r.dark.descriptor->role = master_role::bias; }),
              error_category::invalid_input, "calib.role_slot");
  EXPECT_STOP(run([](calibration_recipe& r) { r.flat.descriptor->role = master_role::dark_flat; }),
              error_category::invalid_input, "calib.role_slot");
  EXPECT_STOP(run([](calibration_recipe& r) { r.light.descriptor = descriptor(master_role::dark); }),
              error_category::invalid_input, "calib.role_slot");

  for (const auto& [slot, type] : {std::pair<const char*, const char*>{"dark", "Flat Field"},
                                   {"flat", "Bias Frame"},
                                   {"light", "Dark Frame"},
                                   {"dark_flat", "Light Frame"},
                                   {"dark", "dark flat"}}) {
    set_def c = synth_set(8, 4, -64);
    frame_def* f = std::string(slot) == "dark" ? &c.dark : std::string(slot) == "flat" ? &c.flat
                   : std::string(slot) == "light" ? &c.light : &c.dark_flat;
    f->hdr.imagetyp = type;
    SCOPED_TRACE(std::string(slot) + " " + type);
    EXPECT_STOP(calibrate(recipe_of(c), images_of(c)), error_category::incompatible, "calib.imagetyp_conflict");
  }
  {
    set_def c = synth_set(8, 4, -64);
    c.dark_flat.hdr.imagetyp = "dark frame";  // un IMAGETYP di dark e' compatibile col ruolo dark_flat dichiarato
    c.flat.hdr.imagetyp = "FLAT";
    c.light.hdr.imagetyp = "Mystery";
    const calibration_result r = calibrate(recipe_of(c), images_of(c));
    EXPECT_NE(std::find(r.warnings.begin(), r.warnings.end(), "imagetyp_unrecognized:light:Mystery"), r.warnings.end());
  }
}

TEST(CalibM2E, T_M2E_17_rgb_and_multi_plane_stop_before_compute) {
  set_def s = synth_set(8, 4, -64);
  {
    calibration_images im = images_of(s);
    frame_def three = s.light;
    three.v.assign(96, 1.0);
    three.planes = 3;
    im.light = decode(three, 8, 4);
    EXPECT_EQ(im.light.layout, plane_layout::multi_plane);
    EXPECT_STOP(calibrate(recipe_of(s), std::move(im)), error_category::unsupported, "calib.plane_layout");
  }
  {
    calibration_images im = images_of(s);
    frame_def three = s.flat;
    three.v.assign(96, 1.0);
    three.planes = 3;
    three.hdr.extra = {fx::card_str("CTYPE3", "RGB")};
    im.flat = decode(three, 8, 4);
    EXPECT_EQ(im.flat.layout, plane_layout::rgb);
    EXPECT_STOP(calibrate(recipe_of(s), std::move(im)), error_category::unsupported, "calib.plane_layout");
  }
}

TEST(CalibM2E, T_M2E_18_dark_at_flat_exposure_only_as_declared_dark_flat) {
  set_def s = synth_set(8, 4, -64);
  s.dark_flat.hdr.imagetyp = "Dark Frame";  // un dark ripreso all'esposizione del flat (5 s)
  calibration_recipe rec = recipe_of(s);
  EXPECT_NO_THROW(calibrate(rec, images_of(s)));
  rec.dark_flat.descriptor->role = master_role::dark;
  EXPECT_STOP(calibrate(rec, images_of(s)), error_category::invalid_input, "calib.role_slot");
  // Lo slot dark non e' riempito con un dark all'esposizione del flat: la compatibilita' L-D lo rifiuta.
  frame_def short_dark = s.dark_flat;
  short_dark.v[0] += 1;
  calibration_recipe swapped = recipe_of(s);
  swapped.dark.digest = digest_for(short_dark, s.w, s.h);
  calibration_images im = images_of(s);
  im.dark = decode(short_dark, s.w, s.h);
  EXPECT_STOP(calibrate(swapped, std::move(im)), error_category::incompatible, "calib.mismatch.exposure_s");
}

// ---------------------------------------------------------------- T-M2E-14 / 15: maschere

TEST(CalibM2E, T_M2E_14_input_masks_are_united) {
  const set_def s = synth_set(8, 4, -64);
  frame_def nan_light = s.light;
  nan_light.v[11] = std::numeric_limits<double>::quiet_NaN();
  calibration_images im = images_of(s);
  im.light = decode(nan_light, 8, 4);
  im.light.invalid.bits[3] = invalid_reason::source_flagged;
  im.dark.invalid.bits[5] = invalid_reason::source_flagged;
  im.flat.invalid.bits[7] = invalid_reason::source_flagged;
  im.dark_flat.invalid.bits[9] = invalid_reason::source_flagged;
  const calibration_result r = calibrate(recipe_of(s), std::move(im));
  const auto& m = r.output.invalid.bits;
  constexpr std::uint16_t master = invalid_reason::master_invalid | invalid_reason::source_flagged;
  EXPECT_EQ(m[3], invalid_reason::source_flagged);
  EXPECT_EQ(m[5], master);
  EXPECT_EQ(m[7], master);
  EXPECT_EQ(m[9], master);
  EXPECT_EQ(m[11], invalid_reason::source_nonfinite);
  for (std::size_t i = 0; i < 32; ++i) {
    if (i == 3 || i == 5 || i == 7 || i == 9 || i == 11) {
      EXPECT_TRUE(std::isnan(sample_at(r.output, i)));
      EXPECT_TRUE(std::isnan(variance_at(r.output, i)));
    } else {
      EXPECT_EQ(m[i], 0) << i;
    }
  }
  EXPECT_TRUE(operator_output_invariants_hold(r.output));
}

TEST(CalibM2E, T_M2E_15_declared_saturation_masked_undeclared_warned) {
  set_def s = synth_set(8, 4, -64);
  const calibration_result undeclared = calibrate(recipe_of(s), images_of(s));
  const auto has = [](const calibration_result& r, const std::string& w) {
    return std::find(r.warnings.begin(), r.warnings.end(), w) != r.warnings.end();
  };
  EXPECT_TRUE(has(undeclared, "saturation_not_evaluated:light"));
  EXPECT_TRUE(has(undeclared, "saturation_not_evaluated:flat"));
  bool in_ledger = false;
  for (const json& w : at(undeclared.report, {"correction_ledger", "warnings"}).items()) {
    if (w.as_string() == "saturation_not_evaluated:light") in_ledger = true;
  }
  EXPECT_TRUE(in_ledger);

  const double light_sat = s.light.v[10];
  const double flat_sat = s.flat.v[20];
  s.light.hdr.saturation = light_sat;
  s.flat.hdr.saturation = flat_sat;
  const calibration_result r = calibrate(recipe_of(s), images_of(s));
  EXPECT_FALSE(has(r, "saturation_not_evaluated:light"));
  EXPECT_FALSE(has(r, "saturation_not_evaluated:flat"));
  for (std::size_t i = 0; i < 32; ++i) {
    const bool sat = s.light.v[i] >= light_sat || s.flat.v[i] >= flat_sat;
    EXPECT_EQ((r.output.invalid.bits[i] & invalid_reason::saturated) != 0, sat) << i;
  }
}

// ---------------------------------------------------------------- T-M2E-22 / 23 / 27: sorgenti, parametri

TEST(CalibM2E, T_M2E_22_shared_input_digest_stops) {
  const set_def s = synth_set(8, 4, -64);
  calibration_recipe rec = recipe_of(s);
  rec.flat.digest = rec.light.digest;
  EXPECT_STOP(calibrate(rec, images_of(s)), error_category::unsupported, "calib.shared_input");
  rec = recipe_of(s);
  rec.dark_flat.digest = rec.dark.digest;
  EXPECT_STOP(calibrate(rec, images_of(s)), error_category::unsupported, "calib.shared_input");
}

TEST(CalibM2E, T_M2E_23_variance_sources_and_parameters) {
  const set_def s = synth_set(8, 4, -64);
  const auto stops = [&](const std::function<void(calibration_recipe&, calibration_images&)>& edit, error_category cat,
                         const char* code) {
    calibration_recipe rec = recipe_of(s);
    calibration_images im = images_of(s);
    edit(rec, im);
    EXPECT_STOP(calibrate(rec, std::move(im)), cat, code);
  };
  stops([](auto& r, auto&) { r.light.variance.source.reset(); }, error_category::metadata_missing, "var.source.variance_source");
  stops([](auto& r, auto&) { r.flat.variance.read_noise_adu.reset(); }, error_category::metadata_missing, "var.source.read_noise_adu");
  stops([](auto& r, auto&) { r.dark.descriptor->combine_count.reset(); }, error_category::metadata_missing, "var.source.combine_count");
  stops([](auto& r, auto&) { r.dark.descriptor->combine.reset(); }, error_category::metadata_missing, "var.source.combine_method");
  stops([](auto&, auto& im) { im.light.metadata.conversion_gain_e_per_adu.reset(); }, error_category::metadata_missing,
        "var.source.conversion_gain_e_per_adu");
  // offset_adu e' richiesto dalla compatibilita' di tutti gli input: l'assenza ferma prima, con calib.metadata.offset_adu.
  stops([](auto&, auto& im) { im.light.metadata.offset_adu.reset(); }, error_category::metadata_missing,
        "calib.metadata.offset_adu");
  stops([](auto& r, auto&) { r.light.variance.read_noise_adu = std::numeric_limits<double>::quiet_NaN(); },
        error_category::invalid_input, "var.param.read_noise_adu");
  stops([](auto& r, auto&) { r.light.variance.read_noise_adu = -1.0; }, error_category::invalid_input, "var.param.read_noise_adu");
  stops([](auto& r, auto&) { r.dark.descriptor->combine_count = 0; }, error_category::invalid_input, "var.param.combine_count");
  stops([](auto&, auto& im) { im.light.metadata.conversion_gain_e_per_adu->value = 0.0; }, error_category::invalid_input,
        "var.param.conversion_gain_e_per_adu");
  stops([](auto&, auto& im) { im.light.metadata.conversion_gain_e_per_adu->value = std::numeric_limits<double>::infinity(); },
        error_category::invalid_input, "var.param.conversion_gain_e_per_adu");
  stops([](auto& r, auto&) { r.flat.descriptor->combine = combine_method::median; }, error_category::unsupported,
        "var.combine_method");
  stops([](auto& r, auto&) { r.dark_flat.descriptor->combine = combine_method::other; }, error_category::unsupported,
        "var.combine_method");
  stops([](auto& r, auto&) { r.light.variance = variance_spec{variance_source::plane, std::nullopt}; },
        error_category::metadata_missing, "var.source.variance_plane");
  stops([](auto& r, auto& im) {
          r.light.variance = variance_spec{variance_source::plane, std::nullopt};
          im.light.variance = variance_plane{aligned_buffer(8)};
        },
        error_category::invalid_input, "var.plane_geometry");
  // La mediana e' ammessa con un piano dichiarato.
  {
    set_def p = mask_set();
    calibration_recipe rec = plane_recipe(p, 0.05, 10.0);
    rec.flat.descriptor->combine = combine_method::median;
    EXPECT_NO_THROW(calibrate(rec, images_of(p)));
  }
}

TEST(CalibM2E, T_M2E_27_parameters_and_pedestal) {
  const set_def s = synth_set(8, 4, -64);
  const auto stops = [&](const std::function<void(calibration_recipe&)>& edit, const char* code) {
    calibration_recipe rec = recipe_of(s);
    edit(rec);
    EXPECT_STOP(calibrate(rec, images_of(s)), error_category::invalid_input, code);
  };
  stops([](auto& r) { r.normalization.reset(); }, "calib.param.normalization");
  stops([](auto& r) { r.q_min.reset(); }, "calib.param.q_min");
  stops([](auto& r) { r.q_min = nan; }, "calib.param.q_min");
  stops([](auto& r) { r.q_min = 0.0; }, "calib.param.q_min");
  stops([](auto& r) { r.q_min = -1.0; }, "calib.param.q_min");
  stops([](auto& r) { r.q_rel_sigma_max.reset(); }, "calib.param.q_rel_sigma_max");
  stops([](auto& r) { r.q_rel_sigma_max = inf; }, "calib.param.q_rel_sigma_max");
  stops([](auto& r) { r.q_rel_sigma_max = 0.0; }, "calib.param.q_rel_sigma_max");
  stops([](auto& r) { r.exposure_rel_tol.reset(); }, "calib.param.exposure_rel_tol");
  stops([](auto& r) { r.exposure_rel_tol = -1e-3; }, "calib.param.exposure_rel_tol");
  stops([](auto& r) { r.exposure_rel_tol = nan; }, "calib.param.exposure_rel_tol");
  stops([](auto& r) { r.temperature_abs_tol_c.reset(); }, "calib.param.temperature_abs_tol_c");
  stops([](auto& r) { r.temperature_abs_tol_c = -0.1; }, "calib.param.temperature_abs_tol_c");
  stops([](auto& r) { r.temp_policy.reset(); }, "calib.param.temperature_policy");
  {
    calibration_recipe rec = recipe_of(s);
    rec.exposure_rel_tol = 0.0;
    rec.temperature_abs_tol_c = 0.0;
    EXPECT_NO_THROW(calibrate(rec, images_of(s)));
  }
  for (const double pedestal : {5.0, -1.0}) {
    set_def p = synth_set(8, 4, -64);
    p.dark.hdr.extra = {fx::card_real("PEDESTAL", pedestal)};
    EXPECT_STOP(calibrate(recipe_of(p), images_of(p)), error_category::unsupported, "calib.pedestal");
  }
  {
    set_def p = synth_set(8, 4, -64);
    p.light.hdr.extra = {fx::card_real("PEDESTAL", 0.0)};
    EXPECT_NO_THROW(calibrate(recipe_of(p), images_of(p)));
  }
}

TEST(CalibM2E, unit_and_linearity_must_be_declared) {
  set_def s = synth_set(8, 4, -64);
  s.light.hdr.with_unit_and_linear = false;
  EXPECT_STOP(calibrate(recipe_of(s), images_of(s)), error_category::metadata_missing, "calib.metadata.linearity");
  calibration_recipe rec = recipe_of(s);
  rec.light.declared_fields.lin = linearity::linear;
  EXPECT_STOP(calibrate(rec, images_of(s)), error_category::metadata_missing, "calib.metadata.unit");
  rec.light.declared_fields.unit = pixel_unit::adu;
  EXPECT_NO_THROW(calibrate(rec, images_of(s)));
  rec.light.declared_fields.unit = pixel_unit::electron;
  set_def with = synth_set(8, 4, -64);
  EXPECT_STOP(calibrate(rec, images_of(with)), error_category::incompatible, "metadata.declared_conflict");
}

TEST(CalibM2E, report_text_matches_report_content) {
  const set_def s = synth_set(8, 4, -64);
  const calibration_result r = calibrate(recipe_of(s), images_of(s));
  EXPECT_EQ(r.report_text, calibration_report_text(r.report));
  EXPECT_NE(r.report_text.find("correction_ledger"), std::string::npos);
  EXPECT_NE(r.report_text.find("bias_in_matched_dark"), std::string::npos);
  EXPECT_EQ(at(r.report, {"variance_model", "covariance_model"}).as_string(), "diagonal_independent");
  EXPECT_EQ(at(r.report, {"variance_model", "shot_estimator"}).as_string(), "observed_clamped");
  EXPECT_EQ(at(r.report, {"variance_model", "shared_master_digests"}).items().size(), 3u);
  const json& ch = r.report.find("normalization")->items()[0];
  EXPECT_EQ(ch.find("hypothesis")->as_string(), "iid_gaussian_fc");
  EXPECT_EQ(ch.find("iid_assumption")->as_string(), "not_verified");  // la risposta del flat sintetico varia piu' dell'1%
}

}  // namespace
