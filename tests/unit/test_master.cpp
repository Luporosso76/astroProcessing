// R2: master del prodotto (C-02.a). T-M2R2-01, 02, 03, 05, 06, 07, 08; pipeline frame -> master -> light calibrato
// (T-M2E-11, 12, 16, 20 e T-M2C-11 sui master del prodotto, ADR-16 e ADR-17, aggiunte di R2).
//
// Tolleranze fissate prima della misura (C-02.a §6.3, §9):
//  - T-M2R2-01: valori dyadic scelti perche' media, s^2 e s^2/n siano rappresentabili: scarto 0.
//  - T-M2R2-02/03: media entro gamma_n sum|x|/n; piano entro gamma_(n+4) V + (1 + gamma_(n+4)) Delta^2/(n-1);
//    piu' mezzo ulp f32 con uscita f32 (master_support.hpp).
//  - T-M2R2-07: 0 ulp fra blocchi e memoria (stessa sequenza di operazioni IEEE per pixel).
#include <gtest/gtest.h>

#include <sys/stat.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "master_support.hpp"

namespace {

using namespace mst;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;

master_result combine(const group& g, const master_recipe& r) { return combine_master(r, images_of(g)); }

// ---------------------------------------------------------------- T-M2R2-01

void expect_exact(const master_result& m, const group& g) {
  const auto o = oracle(g);
  ASSERT_EQ(m.output.geometry.width, g.w);
  for (std::size_t i = 0; i < o.size(); ++i) {
    EXPECT_EQ(m.output.invalid.bits[i], 0) << i;
    EXPECT_EQ(exact::cmp(exact::from_double(sample(m.output, i)), o[i].mean), 0) << "media al pixel " << i;
    EXPECT_EQ(exact::cmp(exact::from_double(plane(m.output, i)), o[i].plane), 0) << "piano al pixel " << i;
  }
}

TEST(Master, T_M2R2_01_dyadic_values_give_exact_mean_and_plane) {
  // n = 4, uscita f32: per pixel i quattro campioni dei frame 0..3.
  const std::vector<std::array<double, 4>> px4 = {
      {0, 0, 0, 4},         {-3, 3, -3, 3},           {5, 5, 5, 5},      {1000, 1000, 1000, 1004},
      {-8, 0, 0, 0},        {2, 2, 2, -6},            {0.5, 0.5, 0.5, 2.5}, {100, 100, 100, 100}};
  group g4 = make_group(master_role::dark, 4, 2, 4);
  for (std::size_t i = 0; i < px4.size(); ++i) {
    for (std::size_t k = 0; k < 4; ++k) g4.frames[k].v[i] = px4[i][k];
  }
  master_recipe r4 = recipe_of(g4);
  r4.output_type = sample_type::f32;
  const master_result m4 = combine(g4, r4);
  EXPECT_EQ(m4.output.type, sample_type::f32);
  expect_exact(m4, g4);
  // Media nulla da campioni di segno opposto e campioni tutti uguali: validi.
  EXPECT_EQ(sample(m4.output, 1), 0.0);
  EXPECT_EQ(plane(m4.output, 2), 0.0);
  EXPECT_EQ(sample(m4.output, 5), 0.0);

  // n = 2, uscita f64: (-1, +1) da' media 0 e piano 1.
  const std::vector<std::array<double, 2>> px2 = {{-1, 1}, {3, 3}, {0.25, 0.75}, {-2, 6}};
  group g2 = make_group(master_role::bias, 2, 2, 2);
  for (std::size_t i = 0; i < px2.size(); ++i) {
    for (std::size_t k = 0; k < 2; ++k) g2.frames[k].v[i] = px2[i][k];
  }
  const master_result m2 = combine(g2, recipe_of(g2));
  expect_exact(m2, g2);
  EXPECT_EQ(sample(m2.output, 0), 0.0);
  EXPECT_EQ(plane(m2.output, 0), 1.0);
  EXPECT_EQ(plane(m2.output, 1), 0.0);
}

// ---------------------------------------------------------------- T-M2R2-02

group nondyadic(master_role role, std::size_t n, int bitpix) {
  group g = make_group(role, 6, 4, n, bitpix);
  for (std::size_t k = 0; k < n; ++k) {
    for (std::size_t i = 0; i < g.frames[k].v.size(); ++i) {
      g.frames[k].v[i] = 1000.1 + 0.37 * static_cast<double>(i) + 1.3 * static_cast<double>((k * 7 + i * 3) % 11) / 3.0;
    }
    // Forte cancellazione: valori vicini a 1e6 con scarti di 0.1.
    g.frames[k].v[3] = 1e6 + 0.1 * static_cast<double>(k + 1);
    // Segni opposti, media vicina a zero.
    g.frames[k].v[7] = (k % 2 == 0 ? 1.0 : -1.0) * (0.3 + 0.1 * static_cast<double>(k));
  }
  return g;
}

TEST(Master, T_M2R2_02_nondyadic_values_within_the_derived_rounding_bounds) {
  for (const std::size_t n : {std::size_t{5}, std::size_t{6}}) {
    for (const sample_type out : {sample_type::f32, sample_type::f64}) {
      for (const int bitpix : {-64, -32}) {
        const group g = nondyadic(master_role::dark, n, bitpix);
        master_recipe r = recipe_of(g);
        r.output_type = out;
        const master_result m = combine(g, r);
        const auto o = oracle(g);
        const bool f32 = out == sample_type::f32;
        for (std::size_t i = 0; i < o.size(); ++i) {
          ASSERT_EQ(m.output.invalid.bits[i], 0);
          EXPECT_TRUE(mean_close(o[i], sample(m.output, i), n, f32)) << "n=" << n << " pixel " << i;
          EXPECT_TRUE(plane_close(o[i], plane(m.output, i), n, f32)) << "n=" << n << " pixel " << i;
        }
      }
    }
  }
}

// ---------------------------------------------------------------- T-M2R2-03

TEST(Master, T_M2R2_03_outliers_are_not_rejected_and_match_the_oracle) {
  group g = nondyadic(master_role::dark_flat, 7, -64);
  g.frames[3].v[9] = 1e5;    // outlier positivo
  g.frames[5].v[14] = -5e4;  // outlier negativo
  const master_result m = combine(g, recipe_of(g));
  const auto o = oracle(g);
  for (std::size_t i = 0; i < o.size(); ++i) {
    ASSERT_EQ(m.output.invalid.bits[i], 0) << i;
    EXPECT_TRUE(mean_close(o[i], sample(m.output, i), 7, false)) << i;
    EXPECT_TRUE(plane_close(o[i], plane(m.output, i), 7, false)) << i;
  }
  // L'outlier sposta media e piano su quel pixel: il piano e' molto piu' grande di quello dei vicini.
  EXPECT_GT(plane(m.output, 9), 1e6 * plane(m.output, 10));
  EXPECT_GT(plane(m.output, 14), 1e6 * plane(m.output, 15));
  EXPECT_EQ(cal::at(m.report, {"rejection"}).as_string(), "none");
  EXPECT_EQ(cal::at(m.report, {"frames_used"}).as_integer(), 7);
  EXPECT_EQ(cal::at(m.report, {"frames_discarded"}).as_integer(), 0);
  for (const json& f : cal::at(m.report, {"frames"}).items()) EXPECT_EQ(f.find("rejected_samples")->as_integer(), 0);
}

// ---------------------------------------------------------------- maschere e rappresentabilita' (C-02.a §6.3, §7)

TEST(Master, T_M2R2_01_masks_propagate_by_union_and_saturation_marks_the_pixel) {
  // Frame 0 float (NaN), frame 1 intero con INVALID (source_flagged), frame 2 intero (saturazione).
  group g = make_group(master_role::dark, 4, 2, 3, 16);
  g.frames[0].bitpix = -32;
  for (mframe& f : g.frames) std::fill(f.v.begin(), f.v.end(), 100.0);
  g.frames[1].invalid = std::vector<std::uint16_t>(8, 0);
  g.frames[0].v[2] = std::numeric_limits<double>::quiet_NaN();  // solo NaN
  g.frames[2].v[5] = 900;                                       // solo saturazione (>= saturation_adu)
  g.frames[0].v[3] = std::numeric_limits<double>::quiet_NaN();  // pixel 3: tre motivi da tre frame
  (*g.frames[1].invalid)[3] = invalid_reason::source_flagged;
  g.frames[2].v[3] = 950;
  (*g.frames[1].invalid)[6] = invalid_reason::source_flagged;  // pixel 6: flag e saturazione nello stesso frame
  g.frames[1].v[6] = 900;
  master_recipe r = recipe_of(g);
  r.saturation_adu = 900;
  r.saturated_fraction_max = 0.5;
  const master_result m = combine(g, r);
  constexpr std::uint16_t nf = invalid_reason::source_nonfinite, fl = invalid_reason::source_flagged,
                          sat = invalid_reason::saturated;
  EXPECT_EQ(m.output.invalid.bits[2], nf);
  EXPECT_EQ(m.output.invalid.bits[5], sat);
  EXPECT_EQ(m.output.invalid.bits[3], nf | fl | sat);
  EXPECT_EQ(m.output.invalid.bits[6], fl | sat);
  for (const std::size_t i : {2, 3, 5, 6}) EXPECT_TRUE(std::isnan(sample(m.output, i)) && std::isnan(plane(m.output, i))) << i;
  for (const std::size_t i : {0, 1, 4, 7}) EXPECT_EQ(m.output.invalid.bits[i], 0) << i;
  EXPECT_TRUE(operator_output_invariants_hold(m.output));
  EXPECT_EQ(cal::at(m.report, {"invalid_counts", "source_nonfinite"}).as_integer(), 2);
  EXPECT_EQ(cal::at(m.report, {"invalid_counts", "source_flagged"}).as_integer(), 2);
  EXPECT_EQ(cal::at(m.report, {"invalid_counts", "saturated"}).as_integer(), 3);
}

TEST(Master, T_M2R2_01_underflow_and_overflow_give_result_nonfinite) {
  group g = make_group(master_role::dark, 4, 1, 2);
  // Uscita f32. pixel 0: (x - media)^2 sottonormale da scarti non nulli; pixel 1: media f64 finita oltre FLT_MAX;
  // pixel 2: media f64 normale che in f32 diventa sottonormale; pixel 3: valido.
  g.frames[0].v = {1e-170, 3.4e38, 1e-39, 10};
  g.frames[1].v = {-1e-170, 3.5e38, 1e-39, 12};
  master_recipe r = recipe_of(g);
  r.output_type = sample_type::f32;
  const master_result m = combine(g, r);
  EXPECT_EQ(m.output.invalid.bits[0], invalid_reason::result_nonfinite);
  EXPECT_EQ(m.output.invalid.bits[1], invalid_reason::result_nonfinite);
  EXPECT_EQ(m.output.invalid.bits[2], invalid_reason::result_nonfinite);
  EXPECT_EQ(m.output.invalid.bits[3], 0);
  EXPECT_TRUE(operator_output_invariants_hold(m.output));

  group h = make_group(master_role::dark, 2, 1, 2);
  h.frames[0].v = {1.7e308, 5};
  h.frames[1].v = {1.7e308, 5};
  const master_result m64 = combine(h, recipe_of(h));
  EXPECT_EQ(m64.output.invalid.bits[0], invalid_reason::result_nonfinite);  // somma in overflow
  EXPECT_EQ(m64.output.invalid.bits[1], 0);  // s^2 = 0 valido
  EXPECT_EQ(plane(m64.output, 1), 0.0);
}

// ---------------------------------------------------------------- T-M2R2-05

struct stop_case {
  const char* name;
  error_category category;
  const char* code;
  std::function<void(group&, master_recipe&)> mutate;  // dopo recipe_of; ricalcola i digest se cambia i frame
};

void refresh_digests(const group& g, master_recipe& r) {
  for (std::size_t k = 0; k < g.frames.size(); ++k) r.frames[k].digest = digest_of(file_of(g.frames[k], g.w, g.h));
}

group flat_group() {
  group g = make_group(master_role::flat, 8, 4, 4);
  for (std::size_t k = 0; k < 4; ++k) {
    for (std::size_t i = 0; i < g.frames[k].v.size(); ++i) g.frames[k].v[i] = 10 + 2000 + static_cast<double>(i % 5) + k;
  }
  return g;
}

group dark_group() {
  group g = make_group(master_role::dark, 8, 4, 3);
  for (std::size_t k = 0; k < 3; ++k) {
    for (std::size_t i = 0; i < g.frames[k].v.size(); ++i) g.frames[k].v[i] = 500 + static_cast<double>((i + k) % 7);
  }
  return g;
}

TEST(Master, T_M2R2_05_every_group_rule_violation_stops_before_combining) {
  const auto hdr = [](std::size_t k, auto f) {
    return [k, f](group& g, master_recipe& r) {
      f(g.frames[k].hdr);
      refresh_digests(g, r);
    };
  };
  std::vector<stop_case> cases = {
      {"camera", error_category::incompatible, "master.mismatch.camera", hdr(1, [](fx::calib_header& h) { h.camera = "X"; })},
      {"gain", error_category::incompatible, "master.mismatch.gain", hdr(2, [](fx::calib_header& h) { h.gain = 101; })},
      {"offset", error_category::incompatible, "master.mismatch.offset_adu", hdr(1, [](fx::calib_header& h) { h.offset = 11; })},
      {"binning", error_category::incompatible, "master.mismatch.binning_x", hdr(1, [](fx::calib_header& h) { h.xbin = 2; })},
      {"binning_y", error_category::incompatible, "master.mismatch.binning_y", hdr(2, [](fx::calib_header& h) { h.ybin = 2; })},
      {"cfa", error_category::incompatible, "master.mismatch.cfa", hdr(1, [](fx::calib_header& h) { h.bayer = "RGGB"; })},
      {"cfa_pattern", error_category::incompatible, "master.mismatch.cfa", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.hdr.bayer = "RGGB";
         g.frames[1].hdr.bayer = "GRBG";
         refresh_digests(g, r);
       }},
      {"cfa_offset", error_category::incompatible, "master.mismatch.cfa", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.hdr.bayer = "RGGB";
         g.frames[2].hdr.bayer_x = 1;
         refresh_digests(g, r);
       }},
      {"cfa_offset_y", error_category::incompatible, "master.mismatch.cfa", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.hdr.bayer = "RGGB";
         g.frames[1].hdr.bayer_y = 1;
         refresh_digests(g, r);
       }},
      {"roi_y", error_category::incompatible, "master.mismatch.roi_y", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.hdr.extra.push_back(fx::card_int("YORGSUBF", 0));
         g.frames[1].hdr.extra.back() = fx::card_int("YORGSUBF", 2);
         refresh_digests(g, r);
       }},
      {"row_order", error_category::incompatible, "master.mismatch.row_order", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.hdr.extra.push_back(fx::card_str("ROWORDER", "TOP-DOWN"));
         g.frames[2].hdr.extra.back() = fx::card_str("ROWORDER", "BOTTOM-UP");
         refresh_digests(g, r);
       }},
      {"exposure", error_category::incompatible, "master.mismatch.exposure_s", hdr(2, [](fx::calib_header& h) { h.exposure = 61; })},
      {"temperature", error_category::incompatible, "master.mismatch.temperature_c",
       hdr(1, [](fx::calib_header& h) { h.temperature = -9; })},
      {"roi_x", error_category::incompatible, "master.mismatch.roi_x", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.hdr.extra.push_back(fx::card_int("XORGSUBF", 0));
         g.frames[2].hdr.extra.back() = fx::card_int("XORGSUBF", 4);
         refresh_digests(g, r);
       }},
      {"readout_mode", error_category::incompatible, "master.mismatch.readout_mode", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.hdr.extra.push_back(fx::card_str("READOUTM", "fast"));
         g.frames[1].hdr.extra.back() = fx::card_str("READOUTM", "slow");
         refresh_digests(g, r);
       }},
      {"width", error_category::incompatible, "master.mismatch.width", nullptr},
      {"height", error_category::incompatible, "master.mismatch.height", nullptr},
      {"camera_missing", error_category::metadata_missing, "master.metadata.camera", [](group& g, master_recipe& r) {
         g.frames[1].omit_keys = {"INSTRUME"};
         refresh_digests(g, r);
       }},
      {"offset_missing", error_category::metadata_missing, "master.metadata.offset_adu", [](group& g, master_recipe& r) {
         for (mframe& f : g.frames) f.omit_keys = {"OFFSET"};
         refresh_digests(g, r);
       }},
      {"unit_missing", error_category::metadata_missing, "master.metadata.unit", [](group& g, master_recipe& r) {
         g.frames[2].omit_keys = {"BUNIT"};
         refresh_digests(g, r);
       }},
      {"imagetyp", error_category::incompatible, "calib.imagetyp_conflict",
       hdr(1, [](fx::calib_header& h) { h.imagetyp = "Light Frame"; })},
      {"phase", error_category::unsupported, "master.input_state",
       hdr(1, [](fx::calib_header& h) { h.phase = "calibrated"; })},
      {"phase_missing", error_category::metadata_missing, "master.metadata.phase",
       hdr(1, [](fx::calib_header& h) { h.phase.clear(); })},
      {"nonlinear", error_category::unsupported, "master.input_state",
       hdr(1, [](fx::calib_header& h) { h.linear_value = false; })},
      {"linearity_missing", error_category::metadata_missing, "master.metadata.linearity",
       hdr(1, [](fx::calib_header& h) { h.with_linear = false; })},
      {"pedestal", error_category::unsupported, "calib.pedestal",
       hdr(1, [](fx::calib_header& h) { h.extra.push_back(fx::card_real("PEDESTAL", 100)); })},
      {"input_variance", error_category::unsupported, "master.input_variance", [](group& g, master_recipe& r) {
         g.frames[1].variance = std::vector<double>(g.frames[1].v.size(), 1.0);
         refresh_digests(g, r);
       }},
      {"frames_insufficient", error_category::invalid_input, "master.frames_insufficient", [](group& g, master_recipe& r) {
         g.frames.resize(1);
         r.frames.resize(1);
       }},
      {"shared_input", error_category::unsupported, "calib.shared_input", [](group& g, master_recipe& r) {
         g.frames[2] = g.frames[0];
         refresh_digests(g, r);
       }},
      {"median", error_category::unsupported, "master.variance_domain",
       [](group&, master_recipe& r) { r.combine = combine_method::median; }},
      {"sigma_clip", error_category::unsupported, "master.variance_domain",
       [](group&, master_recipe& r) { r.rejection = master_rejection::sigma_clip; }},
      {"combine_missing", error_category::invalid_input, "master.param.combine", [](group&, master_recipe& r) { r.combine.reset(); }},
      {"exposure_tol_nan", error_category::invalid_input, "master.param.exposure_rel_tol",
       [](group&, master_recipe& r) { r.exposure_rel_tol = std::numeric_limits<double>::quiet_NaN(); }},
      {"temperature_policy_missing", error_category::invalid_input, "master.param.temperature_policy",
       [](group&, master_recipe& r) { r.temp_policy.reset(); }},
      {"flat_param_on_dark", error_category::invalid_input, "master.param.flat_level_rel_tol",
       [](group&, master_recipe& r) { r.flat_level_rel_tol = 0.1; }},
      {"saturation_fraction_missing", error_category::invalid_input, "master.param.saturated_fraction_max",
       [](group&, master_recipe& r) { r.saturation_adu = 60000; }},
      {"saturation_in_header_only", error_category::invalid_input, "master.param.saturation_adu",
       hdr(0, [](fx::calib_header& h) { h.saturation = 60000; })},
      {"saturation_conflict", error_category::incompatible, "metadata.declared_conflict", [](group& g, master_recipe& r) {
         g.frames[0].hdr.saturation = 60000;
         refresh_digests(g, r);
         r.saturation_adu = 50000;
         r.saturated_fraction_max = 0.1;
       }},
      {"saturated", error_category::invalid_input, "master.saturated", [](group& g, master_recipe& r) {
         std::fill(g.frames[1].v.begin(), g.frames[1].v.begin() + 9, 60000.0);  // 9/32 > 0.25
         refresh_digests(g, r);
         r.saturation_adu = 60000;
         r.saturated_fraction_max = 0.25;
       }},
      {"rgb", error_category::unsupported, "calib.plane_layout", nullptr},
  };
  for (const stop_case& c : cases) {
    SCOPED_TRACE(c.name);
    group g = dark_group();
    master_recipe r = recipe_of(g);
    std::vector<image> frames;
    if (std::string(c.name) == "width" || std::string(c.name) == "height") {
      const bool wide = std::string(c.name) == "width";
      const std::uint32_t ow = wide ? 7 : g.w, oh = wide ? g.h : 3;
      mframe other = g.frames[1];
      other.v.resize(static_cast<std::size_t>(ow) * oh);
      r.frames[1].digest = digest_of(file_of(other, ow, oh));
      frames = images_of(g);
      frames[1] = decode(other, ow, oh);
    } else if (std::string(c.name) == "rgb") {
      frames = images_of(g);
      mframe f = g.frames[1];
      f.v.resize(g.frames[1].v.size() * 3);
      const fx::bytes file = fx::float_frame(g.w, g.h, -64, f.v, fx::calib_cards(f.hdr), 3);
      r.frames[1].digest = digest_of(file);
      frames[1] = decode_fits(file, cal::big_budget);
    } else {
      c.mutate(g, r);
      frames = images_of(g);
    }
    EXPECT_STOP(combine_master(r, std::move(frames)), c.category, c.code);
  }
}

// Campo opzionale assente nel frame di riferimento canonico e discorde fra gli altri due: stop comunque.
TEST(Master, T_M2R2_05_optional_field_checked_even_when_the_reference_lacks_it) {
  struct field_case {
    const char* code;
    std::function<std::string(int)> card;  // card del frame con valore v
  };
  const std::vector<field_case> fields = {
      {"master.mismatch.roi_x", [](int v) { return fx::card_int("XORGSUBF", v); }},
      {"master.mismatch.roi_y", [](int v) { return fx::card_int("YORGSUBF", v); }},
      {"master.mismatch.readout_mode", [](int v) { return fx::card_str("READOUTM", v == 0 ? "fast" : "slow"); }},
      {"master.mismatch.row_order", [](int v) { return fx::card_str("ROWORDER", v == 0 ? "TOP-DOWN" : "BOTTOM-UP"); }},
  };
  for (const field_case& fc : fields) {
    SCOPED_TRACE(fc.code);
    bool reference_lacks = false;
    // I digest decidono l'ordine canonico: si cerca una variante in cui il frame senza campo e' il primo.
    for (int shift = 0; shift < 40 && !reference_lacks; ++shift) {
      for (std::size_t lacking = 0; lacking < 3 && !reference_lacks; ++lacking) {
        group g = dark_group();
        int v = 0;
        for (std::size_t k = 0; k < 3; ++k) {
          g.frames[k].hdr.extra = {fx::card_int("FRAMENO", static_cast<long long>(k) + 3 * shift)};
          if (k != lacking) g.frames[k].hdr.extra.push_back(fc.card(v++));
        }
        master_recipe r = recipe_of(g);
        std::size_t first = 0;
        for (std::size_t k = 1; k < 3; ++k) {
          if (r.frames[k].digest.sha256 < r.frames[first].digest.sha256) first = k;
        }
        if (first != lacking) continue;
        reference_lacks = true;
        EXPECT_STOP(combine_master(r, images_of(g)), error_category::incompatible, fc.code);
      }
    }
    EXPECT_TRUE(reference_lacks) << "nessuna variante con il riferimento privo del campo";
  }
}

TEST(Master, T_M2R2_05_flat_rules_stop_before_combining) {
  std::vector<stop_case> cases = {
      {"filter", error_category::incompatible, "master.mismatch.filter", [](group& g, master_recipe& r) {
         g.frames[2].hdr.filter = "R";
         refresh_digests(g, r);
       }},
      {"median_ratio", error_category::unsupported, "master.variance_domain",
       [](group&, master_recipe& r) { r.flat_norm = flat_normalization::median_ratio; }},
      {"flat_norm_missing", error_category::invalid_input, "master.param.flat_normalization",
       [](group&, master_recipe& r) { r.flat_norm.reset(); }},
      {"level_tol_out_of_domain", error_category::invalid_input, "master.param.flat_level_rel_tol",
       [](group&, master_recipe& r) { r.flat_level_rel_tol = 1.0; }},
      {"min_signal_missing", error_category::invalid_input, "master.param.flat_min_signal_adu",
       [](group&, master_recipe& r) { r.flat_min_signal_adu.reset(); }},
      {"level_beyond_tolerance", error_category::unsupported, "master.variance_domain", [](group& g, master_recipe& r) {
         for (double& v : g.frames[3].v) v = 10 + 1.5 * (v - 10);  // livello +50 %
         refresh_digests(g, r);
       }},
      {"low_signal", error_category::invalid_input, "master.flat_low_signal",
       [](group&, master_recipe& r) { r.flat_min_signal_adu = 5000; }},
      {"no_valid_pixel", error_category::invalid_input, "master.flat_low_signal", [](group& g, master_recipe& r) {
         for (double& v : g.frames[2].v) v = std::numeric_limits<double>::quiet_NaN();
         refresh_digests(g, r);
       }},
      {"flat_saturated", error_category::invalid_input, "master.flat_saturated", [](group& g, master_recipe& r) {
         std::fill(g.frames[0].v.begin(), g.frames[0].v.begin() + 17, 60000.0);
         refresh_digests(g, r);
         r.saturation_adu = 60000;
         r.saturated_fraction_max = 0.5;
       }},
  };
  for (const stop_case& c : cases) {
    SCOPED_TRACE(c.name);
    group g = flat_group();
    master_recipe r = recipe_of(g, 0.01);
    c.mutate(g, r);
    EXPECT_STOP(combine_master(r, images_of(g)), c.category, c.code);
  }
  // Livello esattamente al bordo della tolleranza: accettato (la regola e' `>`).
  group g = make_group(master_role::flat, 4, 4, 2);
  for (double& v : g.frames[0].v) v = 10 + 1024 - 64;
  for (double& v : g.frames[1].v) v = 10 + 1024 + 64;
  const master_result m = combine_master(recipe_of(g, 0.0625), images_of(g));
  EXPECT_EQ(cal::as_double(cal::at(m.report, {"flat_level_mean"})), 1024.0);
}

// ---------------------------------------------------------------- nodo e descrittore (ADR-16, aggiunte di R2)

TEST(Master, T_M2R2_06_node_is_canonical_and_descriptor_derives_from_it) {
  const group g = dark_group();
  master_recipe r = recipe_of(g);
  const node_spec a = make_master_node(r);
  std::reverse(r.frames.begin(), r.frames.end());
  const node_spec b = make_master_node(r);
  EXPECT_EQ(a.key(), b.key());  // l'ordine dei frame nella ricetta non conta: ordine canonico per digest
  ASSERT_EQ(a.inputs().size(), 3u);
  for (std::size_t k = 1; k < a.inputs().size(); ++k) {
    EXPECT_LT(a.inputs()[k - 1].file_digest().sha256, a.inputs()[k].file_digest().sha256);
  }
  const master_descriptor d = master_descriptor_from_node(a);
  EXPECT_EQ(d.role, master_role::dark);
  EXPECT_EQ(d.additive, additive_state::bias_included);
  EXPECT_EQ(d.scaling, scaling_state::unscaled);
  EXPECT_EQ(d.combine_count, 3u);
  EXPECT_EQ(d.combine, combine_method::mean);
  EXPECT_EQ(d.rejection, master_rejection::none);
  EXPECT_EQ(d.estimator, variance_estimator::empirical);
  EXPECT_FALSE(d.flat.has_value());
  const master_descriptor f = master_descriptor_from_node(make_master_node(recipe_of(flat_group())));
  EXPECT_EQ(f.flat, flat_state::raw);
  EXPECT_FALSE(f.additive.has_value());
  r.output_type = sample_type::f32;
  EXPECT_NE(make_master_node(r).key(), a.key());
  calibration_recipe cr = cal::recipe_of(cal::synth_set(4, 4, -64));
  EXPECT_STOP(master_descriptor_from_node(make_calibration_node(cr)), error_category::invalid_input,
              "master.not_master_node");
}

// ---------------------------------------------------------------- job: T-M2R2-06, 07, 08

struct job_case {
  fs::path dir;
  group g;
  master_recipe r;
};

job_case make_job_case(const std::string& name, group g) {
  job_case c{fresh_dir(name), std::move(g), {}};
  c.r = recipe_of(c.g);
  write_frames(c.g, c.r, c.dir, "frame");
  return c;
}

struct master_project {
  std::unique_ptr<sqlite_dag_store> store;
  node_spec node;
};

master_project make_master_project(const fs::path& root, const master_recipe& r, const std::string& slot = "master") {
  master_project p{sqlite_dag_store::create(root), make_master_node(r)};
  p.store->put_node(p.node);
  p.store->create_slot(slot, p.node.key());
  return p;
}

// Gruppo di 16 frame misti: interi a 16 bit (uno con INVALID), float con un NaN.
group mixed_group(std::uint32_t w, std::uint32_t h) {
  group g = make_group(master_role::dark, w, h, 16, 16);
  for (std::size_t k = 0; k < g.frames.size(); ++k) {
    mframe& f = g.frames[k];
    if (k == 5 || k == 9) f.bitpix = -32;
    for (std::size_t i = 0; i < f.v.size(); ++i) f.v[i] = 1000 + static_cast<double>((i * 13 + k * 7) % 97);
  }
  g.frames[9].v[17] = std::numeric_limits<double>::quiet_NaN();
  g.frames[5].v[40] = std::numeric_limits<double>::quiet_NaN();
  g.frames[3].invalid = std::vector<std::uint16_t>(static_cast<std::size_t>(w) * h, 0);
  (*g.frames[3].invalid)[40] = invalid_reason::source_flagged;  // stesso pixel del NaN del frame 5: unione dei motivi
  (*g.frames[3].invalid)[41] = invalid_reason::source_flagged;
  return g;
}

std::uint64_t stack_bytes(const job_case& c) {
  std::uint64_t total = 0;
  for (const mframe& f : c.g.frames) total += plan_fits(file_of(f, c.g.w, c.g.h)).peak_bytes;
  return total;
}

TEST(MasterJob, T_M2R2_07_stack_over_budget_by_row_blocks_is_bit_identical_to_memory) {
  const job_case c = make_job_case("t07", mixed_group(384, 320));
  const master_result memory = combine_master(c.r, images_of(c.g));
  ASSERT_EQ(memory.output.invalid.bits[17], invalid_reason::source_nonfinite);
  ASSERT_EQ(memory.output.invalid.bits[40], invalid_reason::source_nonfinite | invalid_reason::source_flagged);
  ASSERT_EQ(memory.output.invalid.bits[41], invalid_reason::source_flagged);

  // Budget sotto la pila: il minimo dichiarato dallo stop, poi esattamente quel minimo.
  std::uint64_t minimum = 0;
  {
    master_project p = make_master_project(c.dir / "probe", c.r);
    memory_pool tiny(MiB);
    const job_outcome out = run_master_quiet(*p.store, "master", c.r, tiny);
    ASSERT_EQ(out.exit_code, exit_code_stop);
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->category(), error_category::budget);
    EXPECT_EQ(out.stop->code(), "sched.job_exceeds_budget");
    for (const auto& [k, v] : out.stop->context()) {
      if (k == "minimo") minimum = std::stoull(v);
    }
  }
  ASSERT_GT(minimum, 0u);
  ASSERT_LT(minimum, stack_bytes(c)) << "la fixture deve avere la pila oltre il budget minimo";

  std::set<std::string> digests;
  struct variant {
    unsigned workers;
    std::uint32_t rows;
    std::uint64_t budget;
  };
  for (const variant v : {variant{1, 1, minimum}, variant{2, 7, 0}, variant{3, 4, 0}, variant{1, 13, 0}}) {
    SCOPED_TRACE(std::to_string(v.workers) + " worker, " + std::to_string(v.rows) + " righe");
    master_project p = make_master_project(c.dir / ("store-" + std::to_string(v.workers) + "-" + std::to_string(v.rows)), c.r);
    master_job_options o;
    o.worker_count = v.workers;
    o.tile_rows = v.rows;
    const std::uint64_t budget = v.budget != 0 ? v.budget : stack_bytes(c) - 1;
    memory_pool pool(budget);
    const job_outcome out = run_master_quiet(*p.store, "master", c.r, pool, o);
    ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
    const auto roles = by_role(*p.store, p.node.key());
    const image decoded = decode_fits(read_artifact(*p.store, roles.at("primary")), cal::big_budget);
    EXPECT_TRUE(same_bits(decoded, memory.output));
    EXPECT_TRUE(equivalent_content(decoded, memory.output));
    digests.insert(roles.at("primary").sha256);
  }
  EXPECT_EQ(digests.size(), 1u);
}

// C-02.a §8: media e s^2 seguono l'ordine canonico (digest crescente), non quello della ricetta. Somme sensibili
// all'ordine: con 2^53, 1, 1, -2^53 la somma ricorsiva vale 0 in quest'ordine e 2 nell'ordine inverso.
TEST(MasterJob, T_M2R2_07_result_follows_the_canonical_order_not_the_recipe_order) {
  constexpr double big = 9007199254740992.0;  // 2^53
  group g = make_group(master_role::dark, 4, 1, 4);
  const std::array<std::array<double, 4>, 4> px = {{{big, 1, 1, -big}, {1, big, -big, 1}, {-big, 1, 1, big}, {1, 1, big, -big}}};
  for (std::size_t i = 0; i < 4; ++i) {
    for (std::size_t k = 0; k < 4; ++k) g.frames[k].v[i] = px[i][k];
  }
  {
    // I digest dipendono dai campioni: l'ordine si sceglie dopo averli scritti.
    const master_recipe probe = recipe_of(g);
    std::vector<std::size_t> canonical = {0, 1, 2, 3};
    std::sort(canonical.begin(), canonical.end(),
              [&](std::size_t a, std::size_t b) { return probe.frames[a].digest.sha256 < probe.frames[b].digest.sha256; });
    group reordered = g;
    for (std::size_t k = 0; k < 4; ++k) reordered.frames[k] = g.frames[canonical[3 - k]];  // ricetta = inverso del canonico
    g = reordered;
  }
  job_case c = make_job_case("t07order", g);
  const master_result memory = combine_master(c.r, images_of(c.g));
  bool differs = false;
  for (std::size_t i = 0; i < 4; ++i) {
    double canonical = c.g.frames[3].v[i], recipe = c.g.frames[0].v[i];
    for (std::size_t k = 1; k < 4; ++k) {
      canonical += c.g.frames[3 - k].v[i];
      recipe += c.g.frames[k].v[i];
    }
    ASSERT_EQ(memory.output.invalid.bits[i], 0) << i;
    EXPECT_EQ(sample(memory.output, i), canonical / 4) << i;
    differs = differs || recipe / 4 != canonical / 4;
  }
  EXPECT_TRUE(differs) << "la fixture deve distinguere i due ordini";

  // Stessa permutazione su ricetta e immagini, in memoria e nel job: stesso risultato bit per bit.
  group permuted = c.g;
  std::rotate(permuted.frames.begin(), permuted.frames.begin() + 1, permuted.frames.end());
  const master_result again = combine_master(recipe_of(permuted), images_of(permuted));
  EXPECT_TRUE(same_bits(again.output, memory.output));
  master_project p = make_master_project(c.dir / "store", c.r);
  memory_pool pool(256 * MiB);
  ASSERT_EQ(run_master_quiet(*p.store, "master", c.r, pool).exit_code, 0);
  const image decoded = decode_fits(read_artifact(*p.store, by_role(*p.store, p.node.key()).at("primary")), cal::big_budget);
  EXPECT_TRUE(same_bits(decoded, memory.output));
}

TEST(MasterJob, T_M2R2_06_reopen_and_rebuild_give_the_same_node_key_and_digests) {
  const job_case c = make_job_case("t06", mixed_group(24, 20));
  std::string key, image_sha, report_sha;
  {
    master_project p = make_master_project(c.dir / "store", c.r);
    memory_pool pool(256 * MiB);
    ASSERT_EQ(run_master_quiet(*p.store, "master", c.r, pool).exit_code, 0);
    key = p.node.key();
    const auto roles = by_role(*p.store, key);
    image_sha = roles.at("primary").sha256;
    report_sha = roles.at("report").sha256;
  }
  auto reopened = sqlite_dag_store::open(c.dir / "store", open_intent::read_write);
  const std::optional<node_spec> stored = reopened->get_node(key);
  ASSERT_TRUE(stored);
  EXPECT_EQ(stored->key(), make_master_node(c.r).key());
  EXPECT_EQ(node_from_json(stored->to_json()).key(), key);
  const slot_view view = reopened->read_slot("master");
  ASSERT_EQ(view.state, freshness::current);
  EXPECT_EQ(view.artifact->sha256, image_sha);

  // Ricostruzione in un progetto nuovo dalla ricetta con i frame in ordine inverso: stesso nodo, stessi digest.
  master_recipe reversed = c.r;
  std::reverse(reversed.frames.begin(), reversed.frames.end());
  master_project q = make_master_project(c.dir / "rebuild", reversed);
  EXPECT_EQ(q.node.key(), key);
  memory_pool pool(256 * MiB);
  ASSERT_EQ(run_master_quiet(*q.store, "master", reversed, pool).exit_code, 0);
  const auto roles = by_role(*q.store, key);
  EXPECT_EQ(roles.at("primary").sha256, image_sha);
  EXPECT_EQ(roles.at("report").sha256, report_sha);
}

TEST(MasterJob, T_M2R2_08_report_and_provenance_are_complete_and_coherent) {
  group g = flat_group();
  for (std::size_t k = 0; k < g.frames.size(); ++k) {
    g.frames[k].v[6] = 60000;  // pixel saturo in ogni frame
    g.frames[k].v[0] = 2012;   // campioni uguali: piano nullo, rapporto null
    // Esposizione e temperatura diverse entro tolleranza; EGAIN discorde (metadato non comune).
    g.frames[k].hdr.exposure = 5 + 1e-9 * static_cast<double>(k);
    g.frames[k].hdr.temperature = -10 - 0.1 * static_cast<double>(k);
    g.frames[k].hdr.egain = 1.5 + 0.1 * static_cast<double>(k);
  }
  job_case c = make_job_case("t08", g);
  c.r.flat_level_rel_tol = 0.01;
  c.r.saturation_adu = 60000;
  c.r.saturated_fraction_max = 0.1;
  master_project p = make_master_project(c.dir / "store", c.r);
  memory_pool pool(256 * MiB);
  const job_outcome out = run_master_quiet(*p.store, "master", c.r, pool);
  ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");
  EXPECT_EQ(out.provenance.state, job_state::completed);
  EXPECT_EQ(out.provenance.output_digests.size(), 3u);
  const auto roles = by_role(*p.store, p.node.key());
  ASSERT_TRUE(roles.count("primary") && roles.count("report") && roles.count("report_text") && roles.count("provenance"));

  const std::vector<std::uint8_t> fits = read_artifact(*p.store, roles.at("primary"));
  const image m = decode_fits(fits, cal::big_budget);
  EXPECT_EQ(m.type, sample_type::f64);
  ASSERT_TRUE(m.variance);
  EXPECT_EQ(m.phase->value, process_phase::raw);
  EXPECT_EQ(*m.lin, linearity::linear);
  EXPECT_EQ(m.metadata.saturation_adu->value, 60000.0);
  EXPECT_EQ(m.metadata.filter->value, "L");
  EXPECT_TRUE(operator_output_invariants_hold(m));

  // Frame di riferimento canonico: digest minimo.
  std::vector<std::string> sorted;
  std::size_t reference = 0;
  for (std::size_t k = 0; k < c.r.frames.size(); ++k) {
    sorted.push_back(c.r.frames[k].digest.sha256);
    if (c.r.frames[k].digest.sha256 < c.r.frames[reference].digest.sha256) reference = k;
  }
  std::sort(sorted.begin(), sorted.end());
  const image ref = decode(c.g.frames[reference], c.g.w, c.g.h);
  EXPECT_EQ(m.metadata.exposure_s->value, ref.metadata.exposure_s->value);
  EXPECT_EQ(m.metadata.temperature_c->value, ref.metadata.temperature_c->value);
  EXPECT_FALSE(m.metadata.conversion_gain_e_per_adu.has_value()) << "EGAIN discorde: omesso";

  const std::string report_text = cal::read_text(p.store->artifact_path(roles.at("report").rel_path));
  const json report = parse_json(report_text);
  EXPECT_EQ(canonical_json(report), report_text);
  const std::string text = cal::read_text(p.store->artifact_path(roles.at("report_text").rel_path));
  EXPECT_EQ(text, master_report_text(report));
  EXPECT_EQ(cal::at(report, {"outputs", "master_image", "sha256"}).as_string(), roles.at("primary").sha256);
  EXPECT_EQ(static_cast<std::uint64_t>(cal::at(report, {"outputs", "master_image", "size_bytes"}).as_integer()), fits.size());
  // La resa testuale contiene i dati del report: digest dei frame e del master, ruolo, conteggi, stimatore.
  for (const std::string& d : sorted) EXPECT_NE(text.find(d), std::string::npos) << d;
  EXPECT_NE(text.find(roles.at("primary").sha256), std::string::npos);
  for (const char* needle : {"frames_used: 4", "frames_discarded: 0", "role: \"flat\"", "variance_estimator: \"empirical\"",
                             "rejection: \"none\"", "metadata_not_common:", "conversion_gain_e_per_adu", "level_bias_ratio_null: 1"}) {
    EXPECT_NE(text.find(needle), std::string::npos) << needle;
  }

  // Metadati del master: comuni, dal frame di riferimento, omessi (C-02.a, aggiunte di R2).
  EXPECT_EQ(cal::at(report, {"master_metadata", "camera", "source"}).as_string(), "common");
  EXPECT_EQ(cal::at(report, {"master_metadata", "exposure_s", "source"}).as_string(), "reference_frame");
  EXPECT_EQ(cal::as_double(cal::at(report, {"master_metadata", "exposure_s", "value"})), ref.metadata.exposure_s->value);
  EXPECT_EQ(cal::at(report, {"master_metadata", "temperature_c", "source"}).as_string(), "reference_frame");
  EXPECT_EQ(cal::at(report, {"master_metadata", "saturation_adu", "source"}).as_string(), "recipe");
  EXPECT_EQ(cal::at(report, {"master_metadata"}).find("conversion_gain_e_per_adu"), nullptr);
  bool listed = false;
  for (const json& f : cal::at(report, {"metadata_not_common"}).items()) listed = listed || f.as_string() == "conversion_gain_e_per_adu";
  EXPECT_TRUE(listed);
  EXPECT_EQ(cal::at(report, {"reference_frame"}).as_string(), sorted[0]);

  // Frame nell'ordine canonico, tutti usati, livelli del flat coerenti con la media.
  const json& frames = cal::at(report, {"frames"});
  ASSERT_EQ(frames.items().size(), 4u);
  double level_sum = 0;
  for (std::size_t k = 0; k < 4; ++k) {
    const json& f = frames.items()[k];
    EXPECT_EQ(f.find("digest_sha256")->as_string(), sorted[k]);
    EXPECT_EQ(f.find("role")->as_string(), "flat");
    EXPECT_TRUE(f.find("used")->as_bool());
    EXPECT_EQ(f.find("rejected_samples")->as_integer(), 0);
    EXPECT_EQ(cal::as_double(*f.find("saturated_fraction")), 1.0 / 32.0);
    level_sum += cal::as_double(*f.find("flat_level"));
    // m_f: mediana di (x - offset) sui pixel senza bit, calcolata qui sui campioni del frame.
    std::size_t idx = 0;
    while (c.r.frames[idx].digest.sha256 != sorted[k]) ++idx;
    std::vector<double> z;
    for (double v : c.g.frames[idx].v) z.push_back(v - 10.0);
    std::sort(z.begin(), z.end());
    EXPECT_EQ(cal::as_double(*f.find("flat_level")), (z[15] + z[16]) * 0.5) << k;
  }
  const double level_mean = cal::as_double(cal::at(report, {"flat_level_mean"}));
  EXPECT_DOUBLE_EQ(level_mean, level_sum / 4);
  for (const json& f : frames.items()) {
    EXPECT_DOUBLE_EQ(cal::as_double(*f.find("flat_level_rel_dev")), cal::as_double(*f.find("flat_level")) / level_mean - 1);
  }
  EXPECT_EQ(cal::at(report, {"frames_discarded"}).as_integer(), 0);
  EXPECT_EQ(cal::at(report, {"variance", "variance_estimator"}).as_string(), "empirical");
  EXPECT_DOUBLE_EQ(cal::as_double(cal::at(report, {"variance", "variance_cv_gaussian_homoscedastic"})), std::sqrt(2.0 / 3.0));
  EXPECT_EQ(cal::at(report, {"descriptor", "combine_count"}).as_integer(), 4);
  EXPECT_EQ(cal::at(report, {"descriptor", "variance_estimator"}).as_string(), "empirical");

  // Conteggi dei bit uguali alla maschera.
  std::uint64_t saturated = 0;
  for (std::uint16_t b : m.invalid.bits) saturated += (b & invalid_reason::saturated) != 0;
  EXPECT_EQ(saturated, 1u);
  EXPECT_EQ(static_cast<std::uint64_t>(cal::at(report, {"invalid_counts", "saturated"}).as_integer()), saturated);

  // Statistiche per canale dal master stesso; il pixel 0 ha piano nullo: rapporto null, escluso dagli aggregati.
  ASSERT_EQ(plane(m, 0), 0.0);
  const json& ch = cal::at(report, {"channels"}).items()[0];
  std::vector<double> planes, lbe, ratio;
  for (std::size_t i = 0; i < m.invalid.bits.size(); ++i) {
    if (m.invalid.bits[i] != 0) continue;
    planes.push_back(plane(m, i));
    const double a = std::max(sample(m, i) - 10.0, 0.0);
    lbe.push_back(0.01 * 0.01 * a * a / 3.0);
    if (plane(m, i) > 0) ratio.push_back(lbe.back() / plane(m, i));
  }
  ASSERT_EQ(ratio.size() + 1, lbe.size());
  EXPECT_EQ(static_cast<std::size_t>(ch.find("valid_pixels")->as_integer()), planes.size());
  const auto median = [](std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.size() % 2 ? v[v.size() / 2] : (v[v.size() / 2 - 1] + v[v.size() / 2]) * 0.5;
  };
  EXPECT_EQ(cal::as_double(*ch.find("variance_plane_median")), median(planes));
  const json& lb = *ch.find("level_bias_estimate");
  EXPECT_DOUBLE_EQ(cal::as_double(*lb.find("median")), median(lbe));
  EXPECT_DOUBLE_EQ(cal::as_double(*lb.find("max")), *std::max_element(lbe.begin(), lbe.end()));
  EXPECT_DOUBLE_EQ(cal::as_double(*lb.find("ratio_median")), median(ratio));
  EXPECT_DOUBLE_EQ(cal::as_double(*lb.find("ratio_max")), *std::max_element(ratio.begin(), ratio.end()));
  EXPECT_EQ(lb.find("level_bias_ratio_null")->as_integer(), 1);
  EXPECT_EQ(ch.find("pixels_nonpositive_signal")->as_integer(), 0);
  std::set<std::string> hyp;
  for (const json& h : cal::at(report, {"assumptions_not_verified"}).items()) hyp.insert(h.as_string());
  for (const char* h : {"same_illumination_shape", "level_proportional", "additive_nonnegative", "nonnegative_signal"}) {
    EXPECT_TRUE(hyp.count(h)) << h;
  }

  // Provenance: un ingresso osservato per frame, con fase e linearita'.
  const json provenance = provenance_json(out.provenance);
  const json& observed = cal::at(provenance, {"details", "observed_inputs"});
  ASSERT_EQ(observed.items().size(), 4u);
  for (std::size_t k = 0; k < 4; ++k) {
    EXPECT_EQ(observed.items()[k].find("sha256")->as_string(), sorted[k]);
    EXPECT_EQ(observed.items()[k].find("phase")->find("value")->as_string(), "raw");
  }
}

TEST(MasterJob, T_M2R2_08_channel_without_valid_pixels_and_nonfinite_statistic) {
  // Flat CFA RGGB 4x4: i pixel R (x, y pari) sono NaN nel frame 0, quindi il canale R non ha pixel validi.
  group g = make_group(master_role::flat, 4, 4, 3);
  for (std::size_t k = 0; k < 3; ++k) {
    g.frames[k].hdr.bayer = "RGGB";
    for (std::size_t i = 0; i < 16; ++i) g.frames[k].v[i] = 10 + 2000 + static_cast<double>(i % 3) + static_cast<double>(k);
  }
  for (const std::size_t i : {0, 2, 8, 10}) g.frames[0].v[i] = std::numeric_limits<double>::quiet_NaN();
  const master_result m = combine_master(recipe_of(g, 0.01), images_of(g));
  const json* red = nullptr;
  std::size_t seen = 0;
  for (const json& ch : cal::at(m.report, {"channels"}).items()) {
    ++seen;
    if (ch.find("label")->as_string() == "R") red = &ch;
    else EXPECT_EQ(ch.find("valid_pixels")->as_integer(), 4);
  }
  EXPECT_EQ(seen, 4u);
  ASSERT_NE(red, nullptr);
  EXPECT_EQ(red->find("valid_pixels")->as_integer(), 0);
  EXPECT_EQ(red->find("variance_plane_median")->type(), json::kind::null);
  for (const char* k : {"median", "max", "ratio_median", "ratio_max"}) {
    EXPECT_EQ(red->find("level_bias_estimate")->find(k)->type(), json::kind::null) << k;
  }
  EXPECT_EQ(red->find("pixels_nonpositive_signal")->as_integer(), 0);

  // level_bias_estimate in overflow ((M - offset)^2 oltre DBL_MAX con piano nullo): stop numeric, nessun master.
  group big = make_group(master_role::flat, 4, 2, 3);
  for (mframe& f : big.frames) std::fill(f.v.begin(), f.v.end(), 1e200);
  EXPECT_STOP(combine_master(recipe_of(big, 0.01), images_of(big)), error_category::numeric, "master.report_nonfinite");
  job_case c = make_job_case("t08nonfinite", big);
  c.r.flat_level_rel_tol = 0.01;
  master_project p = make_master_project(c.dir / "store", c.r);
  memory_pool pool(256 * MiB);
  const job_outcome out = run_master_quiet(*p.store, "master", c.r, pool);
  ASSERT_TRUE(out.stop);
  EXPECT_EQ(out.stop->category(), error_category::numeric);
  EXPECT_EQ(out.stop->code(), "master.report_nonfinite");
  EXPECT_EQ(by_role(*p.store, p.node.key()).count("primary"), 0u);
}

// Statistiche del report non rappresentabili (§6.3 applicato al report): stop, mai uno zero o un infinito silenzioso.
TEST(Master, T_M2R2_08_unrepresentable_report_statistics_stop) {
  const auto offset_zero = [](group& g) {
    for (mframe& f : g.frames) f.hdr.offset = 0;
  };
  {
    // eps^2 = 2^-1200: underflow; la stima pubblicata sarebbe 0 invece di 1.
    group g = make_group(master_role::flat, 2, 1, 2);
    offset_zero(g);
    for (mframe& f : g.frames) std::fill(f.v.begin(), f.v.end(), std::ldexp(1.0, 600));
    master_recipe r = recipe_of(g, std::ldexp(1.0, -600));
    EXPECT_STOP(combine_master(r, images_of(g)), error_category::numeric, "master.report_nonfinite");
  }
  {
    // Rapporto stima/piano sottonormale: stima 2^-1000 normale, piano circa 2^38. Fondo uguale nei due frame.
    group g = make_group(master_role::flat, 4, 1, 2);
    for (mframe& f : g.frames) {
      f.hdr.offset = std::ldexp(1.0, 20);
      std::fill(f.v.begin(), f.v.end(), std::ldexp(1.0, 20) + 100);
    }
    g.frames[0].v[0] = std::ldexp(1.0, 20) + std::ldexp(1.0, 19);
    g.frames[1].v[0] = std::ldexp(1.0, 19) + std::ldexp(1.0, -30);
    master_recipe r = recipe_of(g, std::ldexp(1.0, -469));
    EXPECT_STOP(combine_master(r, images_of(g)), error_category::numeric, "master.report_nonfinite");
  }
  {
    // Mediana pari di due stime finite 9.8e307: la media dei centrali va in overflow.
    group g = make_group(master_role::flat, 2, 1, 2);
    offset_zero(g);
    for (mframe& f : g.frames) std::fill(f.v.begin(), f.v.end(), 1e154);
    master_recipe r = recipe_of(g, 0.99);
    EXPECT_STOP(combine_master(r, images_of(g)), error_category::numeric, "master.report_nonfinite");
  }
}

// Memoria del piano non rappresentabile in 64 bit (frame larghi 65536, tile_rows fino a 2^32 - 1):
// nessun valore troncato, il job si ferma prima di ogni prenotazione (C-02.a, aggiunte di R2).
TEST(MasterJob, T_M2R2_07_unrepresentable_plan_memory_is_detected_before_any_reservation) {
  // Ingressi del revisore: 2^47 (13106 * 10 + 8) + 13106 * 16 = 2^64 - 2^49 + 209696, rappresentabile e calcolato esatto.
  ASSERT_TRUE(master_task_bytes(13106, std::uint64_t{1} << 31, 65536).has_value());
  EXPECT_EQ(*master_task_bytes(13106, std::uint64_t{1} << 31, 65536),
            (~std::uint64_t{0} - (std::uint64_t{1} << 49) + 1) + 209696);
  // Con tile_rows = 2^32 - 1 il valore supera 2^64: nullopt, mai un valore troncato.
  EXPECT_FALSE(master_task_bytes(13106, 0xFFFFFFFFu, 65536).has_value());
  EXPECT_FALSE(master_task_bytes(std::uint64_t{1} << 62, 1, 1).has_value());
  EXPECT_FALSE(master_task_bytes(1, std::uint64_t{1} << 32, std::uint64_t{1} << 32).has_value());
  // Formula sui valori rappresentabili: n * 16 + n * righe * larghezza * 10 + righe * larghezza * 8.
  ASSERT_TRUE(master_task_bytes(16, 5, 64).has_value());
  EXPECT_EQ(*master_task_bytes(16, 5, 64), 16u * 16 + 16u * 5 * 64 * 10 + 5u * 64 * 8);

  // Nel job: con tile_rows massimo e pochi frame il piano resta rappresentabile ma oltre il budget: stop prima della
  // prenotazione, nessun master.
  const job_case c = make_job_case("t07huge", dark_group());
  master_project p = make_master_project(c.dir / "store", c.r);
  memory_pool pool(64 * MiB);
  master_job_options o;
  o.tile_rows = 0xFFFFFFFFu;
  o.worker_count = 1;
  const job_outcome out = run_master_quiet(*p.store, "master", c.r, pool, o);
  ASSERT_TRUE(out.stop);
  EXPECT_EQ(out.stop->category(), error_category::budget);
  EXPECT_EQ(out.stop->code(), "sched.job_exceeds_budget");
  EXPECT_EQ(by_role(*p.store, p.node.key()).count("primary"), 0u);
}

TEST(MasterJob, T_M2R2_05_a_stopped_job_publishes_no_master) {
  group g = dark_group();
  g.frames[1].hdr.gain = 200;
  job_case c = make_job_case("t05job", g);
  master_project p = make_master_project(c.dir / "store", c.r);
  memory_pool pool(256 * MiB);
  const job_outcome out = run_master_quiet(*p.store, "master", c.r, pool);
  EXPECT_EQ(out.exit_code, exit_code_stop);
  ASSERT_TRUE(out.stop);
  EXPECT_EQ(out.stop->code(), "master.mismatch.gain");
  const auto roles = by_role(*p.store, p.node.key());
  EXPECT_EQ(roles.size(), 1u);
  EXPECT_TRUE(roles.count("provenance"));
  EXPECT_EQ(p.store->read_slot("master").state, freshness::missing);
}

TEST(MasterJob, T_M2R2_07_file_changed_after_the_full_read_stops_the_branch) {
  job_case c = make_job_case("t07changed", dark_group());
  master_project p = make_master_project(c.dir / "store", c.r);
  memory_pool pool(256 * MiB);
  master_job_options o;
  o.hook = [&](std::string_view point) {
    if (point != "frames.read") return;
    mframe f = c.g.frames[1];
    f.v[0] += 1;
    cal::write_bytes(c.r.frames[1].path, file_of(f, c.g.w, c.g.h));
  };
  const job_outcome out = run_master_quiet(*p.store, "master", c.r, pool, o);
  EXPECT_EQ(out.exit_code, exit_code_stop);
  ASSERT_TRUE(out.stop);
  EXPECT_EQ(out.stop->category(), error_category::integrity);
  EXPECT_EQ(by_role(*p.store, p.node.key()).count("primary"), 0u);
}

// ---------------------------------------------------------------- pipeline frame -> master -> Q -> light calibrato

struct file_id {
  std::string sha;
  std::uint64_t ino = 0;
  std::int64_t mtime_ns = 0;
  unsigned mode = 0;
  bool operator==(const file_id&) const = default;
};

file_id id_of(const std::string& path) {
  struct stat st {};
  EXPECT_EQ(::stat(path.c_str(), &st), 0) << path;
  const std::string text = cal::read_text(path);
  return {digest_of(std::vector<std::uint8_t>(text.begin(), text.end())).sha256, st.st_ino,
          static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec, st.st_mode};
}

struct pipeline {
  fs::path dir;
  std::unique_ptr<sqlite_dag_store> store;
  group dark, flat, dark_flat;
  master_recipe rd, rf, rdf;
  cal::frame_def light;
  std::string light_path;
};

// Light, master dark, flat e dark-flat dal modello FIX-SYN-FITS-01 con scarti per frame non dyadic.
pipeline make_pipeline(const std::string& name) {
  pipeline p;
  p.dir = fresh_dir(name);
  p.store = sqlite_dag_store::create(p.dir / "store");
  constexpr std::uint32_t w = 20, h = 12;
  p.dark = make_group(master_role::dark, w, h, 5);
  p.flat = make_group(master_role::flat, w, h, 5);
  p.dark_flat = make_group(master_role::dark_flat, w, h, 5);
  p.light.hdr = cal::header_for("Light Frame", 60);
  p.light.v.assign(w * h, 0);
  p.light.variance = std::vector<double>(w * h, 0);
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * w + x;
      const double b = fx::synth_bias(x, y), d = fx::synth_dark_rate(x, y), q = fx::synth_flat_response(x, y);
      for (std::size_t k = 0; k < 5; ++k) {
        const double jitter = 0.7 * static_cast<double>((i + 3 * k) % 5) - 1.4;
        p.dark.frames[k].v[i] = b + 60 * d + jitter;
        p.dark_flat.frames[k].v[i] = b + 5 * d + 0.5 * jitter;
        p.flat.frames[k].v[i] = b + 5 * d + q + 3 * jitter;
      }
      p.light.v[i] = b + 60 * d + q * fx::synth_sky(x, y, 0) / 8192.0;
      (*p.light.variance)[i] = 2.5 + 0.01 * static_cast<double>(i % 13);
    }
  }
  p.rd = recipe_of(p.dark);
  p.rf = recipe_of(p.flat, 0.01);
  p.rdf = recipe_of(p.dark_flat);
  for (master_recipe* r : {&p.rd, &p.rf, &p.rdf}) r->output_type = sample_type::f32;
  write_frames(p.dark, p.rd, p.dir, "dark");
  write_frames(p.flat, p.rf, p.dir, "flat");
  write_frames(p.dark_flat, p.rdf, p.dir, "dark_flat");
  p.light_path = (p.dir / "light.fits").string();
  cal::write_bytes(p.light_path, cal::file_of(p.light, w, h));
  for (const auto& [slot, r] : {std::pair{"master_dark", &p.rd}, std::pair{"master_flat", &p.rf}, std::pair{"master_dark_flat", &p.rdf}}) {
    const node_spec node = make_master_node(*r);
    p.store->put_node(node);
    p.store->create_slot(slot, node.key());
  }
  return p;
}

calibration_recipe product_recipe(const pipeline& p) {
  calibration_recipe r;
  r.light.path = p.light_path;
  r.light.digest = digest_of(cal::file_of(p.light, 20, 12));
  r.light.variance.source = variance_source::plane;
  r.dark = product_master_input(*p.store, "master_dark");
  r.flat = product_master_input(*p.store, "master_flat");
  r.dark_flat = product_master_input(*p.store, "master_dark_flat");
  r.normalization = normalization_mode::global_median;
  r.q_min = 0.01;
  r.q_rel_sigma_max = 10.0;
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  r.simd = simd_test::requested();
  return r;
}

job_outcome run_cal_quiet(sqlite_dag_store& store, const calibration_recipe& r, memory_pool& pool,
                          const calibration_job_options& o = {}) {
  testing::internal::CaptureStderr();
  job_outcome out = run_calibration_job(store, "cal", r, pool, o, never_cancelled());
  testing::internal::GetCapturedStderr();
  return out;
}

void run_masters(pipeline& p, memory_pool& pool) {
  ASSERT_EQ(run_master_quiet(*p.store, "master_dark", p.rd, pool).exit_code, 0);
  ASSERT_EQ(run_master_quiet(*p.store, "master_flat", p.rf, pool).exit_code, 0);
  ASSERT_EQ(run_master_quiet(*p.store, "master_dark_flat", p.rdf, pool).exit_code, 0);
}

TEST(MasterPipeline, T_M2C_11_raw_frames_untouched_by_frames_to_master_to_calibrated_light) {
  pipeline p = make_pipeline("pipeline");
  std::vector<std::string> paths = {p.light_path};
  for (const master_recipe* r : {&p.rd, &p.rf, &p.rdf}) {
    for (const master_frame_spec& f : r->frames) paths.push_back(f.path);
  }
  std::vector<file_id> before;
  for (const std::string& path : paths) before.push_back(id_of(path));

  memory_pool pool(256 * MiB);
  run_masters(p, pool);
  const calibration_recipe r = product_recipe(p);
  ASSERT_TRUE(r.dark.master_node_key && r.flat.master_node_key && r.dark_flat.master_node_key);
  EXPECT_EQ(r.dark.variance.source, variance_source::plane);
  EXPECT_EQ(*r.dark.descriptor, master_descriptor_from_node(*p.store->get_node(*r.dark.master_node_key)));
  const node_spec cal_node = make_calibration_node(r);
  p.store->put_node(cal_node);
  p.store->create_slot("cal", cal_node.key());
  // La calibrazione e' discendente dei tre master nel DAG.
  for (const char* slot : {"master_dark", "master_flat", "master_dark_flat"}) {
    const std::vector<std::string> desc = p.store->descendants(p.store->get_slot(slot)->target_node_key);
    EXPECT_NE(std::find(desc.begin(), desc.end(), cal_node.key()), desc.end()) << slot;
  }
  calibration_job_options o;
  o.worker_count = 3;
  o.tile_rows = 5;
  const job_outcome out = run_cal_quiet(*p.store, r, pool, o);
  ASSERT_EQ(out.exit_code, 0) << (out.stop ? out.stop->what() : "");

  // T-M2E-16 e T-M2E-12 sui master del prodotto: uscita uguale al calcolo in memoria con i master decodificati.
  const auto roles = by_role(*p.store, cal_node.key());
  const image calibrated = decode_fits(read_artifact(*p.store, roles.at("primary")), cal::big_budget);
  const auto master_image = [&](const calibration_input_spec& s) {
    const std::string text = cal::read_text(s.path);
    return decode_fits(std::vector<std::uint8_t>(text.begin(), text.end()), cal::big_budget);
  };
  calibration_images memory_inputs{cal::decode(p.light, 20, 12), master_image(r.dark), master_image(r.flat),
                                   master_image(r.dark_flat)};
  const calibration_result memory = calibrate(r, std::move(memory_inputs));
  EXPECT_TRUE(same_bits(calibrated, memory.output));
  const json report = parse_json(cal::read_text(p.store->artifact_path(roles.at("report").rel_path)));
  for (const char* slot : {"dark", "flat", "dark_flat"}) {
    const json& st = cal::at(report, {"inputs", slot, "master_state"});
    EXPECT_EQ(st.find("variance_estimator")->as_string(), "empirical") << slot;
    EXPECT_EQ(st.find("rejection")->as_string(), "none") << slot;
    EXPECT_TRUE(st.find("master_node_key") != nullptr) << slot;
    EXPECT_EQ(cal::at(report, {"inputs", slot, "variance", "variance_source"}).as_string(), "plane");
  }
  std::set<std::string> assumptions;
  for (const json& a : cal::at(report, {"assumptions"}).items()) assumptions.insert(a.as_string());
  for (const char* a : {"empirical_master_variance_is_estimate:dark", "empirical_master_variance_is_estimate:flat",
                        "empirical_master_variance_is_estimate:dark_flat", "master_flat_variance_level_bias:flat"}) {
    EXPECT_TRUE(assumptions.count(a)) << a;
  }

  // T-M2C-11: i frame raw e il light restano identici (digest, inode, mtime, permessi).
  for (std::size_t k = 0; k < paths.size(); ++k) EXPECT_EQ(id_of(paths[k]), before[k]) << paths[k];

  // Un master ricalcolato (ricetta diversa) rende la calibrazione non piu' corrente.
  master_recipe changed = p.rd;
  changed.output_type = sample_type::f64;
  const node_spec changed_node = make_master_node(changed);
  p.store->put_node(changed_node);
  p.store->retarget_slot("master_dark", changed_node.key());
  EXPECT_STOP(product_master_input(*p.store, "master_dark"), error_category::invalid_input,
              "calib.product_master_not_current");
}

TEST(MasterPipeline, T_M2E_11_product_master_descriptor_or_file_mismatch_stops) {
  pipeline p = make_pipeline("pipeline-mismatch");
  memory_pool pool(256 * MiB);
  run_masters(p, pool);
  const calibration_recipe good = product_recipe(p);
  const auto prepare = [&](const calibration_recipe& r, const std::string& slot_name) {
    const node_spec node = make_calibration_node(r);
    p.store->put_node(node);
    p.store->create_slot(slot_name, node.key());
    testing::internal::CaptureStderr();
    job_outcome out = run_calibration_job(*p.store, slot_name, r, pool, {}, never_cancelled());
    testing::internal::GetCapturedStderr();
    return out;
  };
  {
    calibration_recipe r = good;
    r.dark.descriptor->combine_count = 4;
    const job_outcome out = prepare(r, "cal-descriptor");
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->code(), "calib.descriptor_mismatch");
  }
  {
    // File diverso dall'artefatto del nodo del master flat: lo stesso master con un campione cambiato.
    calibration_recipe r = good;
    const std::string text = cal::read_text(r.flat.path);
    image other = decode_fits(std::vector<std::uint8_t>(text.begin(), text.end()), cal::big_budget);
    other.data<float>()[0] += 1.0f;
    const std::vector<std::uint8_t> bytes = encode_fits(other);
    r.flat.path = (p.dir / "other_flat.fits").string();
    cal::write_bytes(r.flat.path, bytes);
    r.flat.digest = digest_of(bytes);
    const job_outcome out = prepare(r, "cal-artifact");
    ASSERT_TRUE(out.stop);
    EXPECT_EQ(out.stop->code(), "calib.master_artifact_mismatch");
  }
  {
    calibration_recipe r = good;
    r.dark.variance = cal::noise(2.0);
    EXPECT_STOP(validate_calibration_recipe(r), error_category::invalid_input, "calib.product_master");
  }
  {
    calibration_recipe r = good;
    r.dark.descriptor->estimator.reset();
    EXPECT_STOP(validate_calibration_recipe(r), error_category::invalid_input, "calib.product_master");
  }
}

// T-M2E-20 sui master del prodotto: propagazione sui piani VARIANCE dei master contro l'oracle razionale.
TEST(MasterPipeline, T_M2E_20_product_master_planes_propagate_as_the_rational_oracle) {
  pipeline p = make_pipeline("pipeline-oracle");
  for (master_recipe* r : {&p.rd, &p.rf, &p.rdf}) r->output_type = sample_type::f64;
  const master_result d = combine_master(p.rd, images_of(p.dark));
  const master_result f = combine_master(p.rf, images_of(p.flat));
  const master_result df = combine_master(p.rdf, images_of(p.dark_flat));
  calibration_recipe r;
  r.light.digest = digest_of(cal::file_of(p.light, 20, 12));
  r.light.variance.source = variance_source::plane;
  const std::tuple<calibration_input_spec*, const master_recipe*, const master_result*> slots[3] = {
      {&r.dark, &p.rd, &d}, {&r.flat, &p.rf, &f}, {&r.dark_flat, &p.rdf, &df}};
  for (const auto& [spec, mr, m] : slots) {
    spec->descriptor = master_descriptor_from_node(make_master_node(*mr));
    spec->digest = digest_of(encode_fits(m->output));
    spec->variance.source = variance_source::plane;
  }
  r.normalization = normalization_mode::global_median;
  r.q_min = 0.01;
  r.q_rel_sigma_max = 10.0;
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  r.simd = simd_test::requested();
  const calibration_result out = calibrate(r, calibration_images{cal::decode(p.light, 20, 12), d.output, f.output, df.output});

  const std::size_t n = 240;
  std::vector<rat> fc(n);
  for (std::size_t i = 0; i < n; ++i) fc[i] = exact::from_double(sample(f.output, i)) - exact::from_double(sample(df.output, i));
  const rat big_n = exact::median(fc);
  for (std::size_t i = 0; i < n; ++i) {
    ASSERT_EQ(out.output.invalid.bits[i], 0) << i;
    const rat lc = exact::from_double(p.light.v[i]) - exact::from_double(sample(d.output, i));
    const rat q = fc[i] / big_n;
    const rat vlc = exact::from_double((*p.light.variance)[i]) + exact::from_double(plane(d.output, i));
    const rat vq = (exact::from_double(plane(f.output, i)) + exact::from_double(plane(df.output, i))) / (big_n * big_n);
    const rat q2 = q * q;
    const rat var = vlc / q2 + lc * lc * vq / (q2 * q2);
    // m contato prima della misura (lemma (1+d)^(+-1), Higham cap. 3): Lc 1; N fino a 3 (due Fc, somma; *0.5
    // esatto); Q = Fc/N 5; Q^2 11; Q^4 23; Var(Lc) 1; t1 = Var(Lc)/Q^2 13; Var(Fc) 1; N^2 7; Var(Q) 9; Lc^2 3;
    // Lc^2 Var(Q) 13; t2 = (Lc^2 Var(Q))/Q^4 37; Var(C) = t1 + t2: 38. Uscita f64, nessun termine di uscita.
    EXPECT_TRUE(cal::close_to(var, plane(out.output, i), 38, false)) << i;
  }
}

}  // namespace
