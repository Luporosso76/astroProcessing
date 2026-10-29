// R1: inventario dei soli header, anteprima a blocchi, fase e linearita' (ADR-03, aggiunte di R1).
// T-M2R1-01, T-M2R1-02, T-M2R1-03, T-M2R1-05 (stato), T-M2R1-10.
#include <gtest/gtest.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <limits>
#include <random>

#include "astro/fits.hpp"
#include "astro/inventory.hpp"
#include "fits_internal.hpp"
#include "astro/memory_pool.hpp"
#include "astro/operations.hpp"
#include "import_support.hpp"

namespace {

using namespace imp;

constexpr std::uint64_t big = std::uint64_t{1} << 30;

std::string context_value(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "";
}

double rep(const gen& g, double v) { return g.bitpix == -32 ? static_cast<double>(static_cast<float>(v)) : v; }

struct expected_preview {
  std::uint32_t width = 0, height = 0;
  std::vector<long double> mean;
  std::vector<std::uint32_t> count;
  std::vector<long double> max_abs;
};

// Oracle del test: media per blocco in long double dai valori del generatore, campioni finiti soltanto.
expected_preview oracle(const gen& g, std::uint32_t bw, std::uint32_t bh) {
  expected_preview e;
  e.width = (g.w + bw - 1) / bw;
  e.height = (g.h + bh - 1) / bh;
  for (std::uint32_t p = 0; p < g.planes; ++p) {
    for (std::uint32_t by = 0; by < e.height; ++by) {
      for (std::uint32_t bx = 0; bx < e.width; ++bx) {
        long double sum = 0;
        long double max_abs = 0;
        std::uint32_t n = 0;
        for (std::uint32_t y = by * bh; y < std::min(g.h, (by + 1) * bh); ++y) {
          for (std::uint32_t x = bx * bw; x < std::min(g.w, (bx + 1) * bw); ++x) {
            const double v = rep(g, g.value(p, x, y));
            if (!std::isfinite(v)) continue;
            sum += v;
            max_abs = std::max<long double>(max_abs, std::fabs(v));
            ++n;
          }
        }
        e.count.push_back(n);
        e.max_abs.push_back(max_abs);
        e.mean.push_back(n == 0 ? std::numeric_limits<long double>::quiet_NaN() : sum / n);
      }
    }
  }
  return e;
}

long double gamma_k(std::uint32_t k) {
  const long double u = std::ldexp(1.0L, -53);
  return k * u / (1 - k * u);
}

// Tolleranza derivata: gamma_k * max|x| piu' mezzo ulp f32 sull'arrotondamento finale; zero se esatto per costruzione.
void expect_preview(const gen& g, const fits_preview& got, std::uint32_t bw, std::uint32_t bh, bool exact) {
  const expected_preview e = oracle(g, bw, bh);
  ASSERT_EQ(got.width, e.width);
  ASSERT_EQ(got.height, e.height);
  ASSERT_EQ(got.planes, g.planes);
  ASSERT_EQ(got.samples.size(), e.mean.size());
  ASSERT_EQ(got.finite_counts, e.count);
  for (std::size_t i = 0; i < e.mean.size(); ++i) {
    if (e.count[i] == 0) {
      EXPECT_TRUE(std::isnan(got.samples[i])) << "blocco " << i;
      continue;
    }
    const float nearest = static_cast<float>(e.mean[i]);
    if (exact) {
      EXPECT_EQ(got.samples[i], nearest) << "blocco " << i;
      continue;
    }
    const long double half_ulp = (std::nextafterf(std::fabs(nearest), INFINITY) - std::fabs(nearest)) / 2.0L;
    const long double tol = gamma_k(e.count[i]) * e.max_abs[i] + half_ulp;
    EXPECT_LE(std::fabs(static_cast<long double>(got.samples[i]) - e.mean[i]), tol) << "blocco " << i;
  }
}

struct on_disk {
  fs::path path;
  fx::bytes bytes;
};

on_disk put(const std::string& dir, const std::string& name, const fx::bytes& b) {
  on_disk f{fresh(dir) / name, b};
  write_file(f.path, b);
  return f;
}

// ---------------------------------------------------------------- T-M2R1-01

TEST(Inventory, T_M2R1_01_reads_only_headers_even_when_every_pixel_offset_fails) {
  gen g;
  g.w = 100;
  g.h = 60;
  g.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return static_cast<double>((x * 3 + y) % 200); };
  g.cards = {fx::card_str("OBSERVER", "tester"), phase_card("raw")};
  const on_disk f = put("t01", "a.fits", make_fits(g, true));
  // Layout del file: header di 2880 byte per HDU; dati del primario 12000 byte (rimpiegati a 14400) poi l'estensione.
  const std::uint64_t primary_data = 2880, primary_data_end = 2880 + 14400;
  const std::uint64_t ext_header = primary_data_end, ext_data = ext_header + 2880;
  ASSERT_EQ(f.bytes.size(), ext_data + 14400);

  fits_input in = open_fits_input(f.path.string());
  std::uint64_t bytes_read = 0;
  std::uint64_t calls = 0;
  in.set_pread_observer([&](std::uint64_t offset, std::size_t length) {
    ++calls;
    bytes_read += length;
    const bool in_primary_data = offset < primary_data_end && offset + length > primary_data;
    const bool in_ext_data = offset + length > ext_data;
    return (in_primary_data || in_ext_data) ? EIO : 0;
  });
  memory_pool pool(big);
  const fits_inventory inv = inventory_fits(in, {}, pool);
  EXPECT_LE(bytes_read, 2 * 2880u + 2880u);
  EXPECT_EQ(bytes_read, 2 * 2880u);
  EXPECT_EQ(inv.header_bytes_read, bytes_read);
  EXPECT_EQ(calls, 2u);
  ASSERT_EQ(inv.hdus.size(), 2u);
  EXPECT_EQ(inv.hdus[0].bitpix, 16);
  EXPECT_EQ(inv.hdus[0].axes, (std::vector<std::uint64_t>{100, 60}));
  EXPECT_EQ(inv.hdus[0].data_offset, primary_data);
  EXPECT_EQ(inv.hdus[1].extname, "INVALID");
  EXPECT_EQ(inv.hdus[1].header_offset, ext_header);
  EXPECT_EQ(inv.plan.width, 100u);
  ASSERT_TRUE(inv.phase.has_value());
  EXPECT_EQ(inv.phase->value, process_phase::raw);
  EXPECT_EQ(inv.uninterpreted_keywords, (std::vector<std::string>{"OBSERVER"}));

  // L'osservatore colpisce davvero i pixel: l'anteprima, che li legge, si ferma con EIO.
  EXPECT_STOP(make_preview(in, {10, 10}, pool), error_category::io, "fits.read");
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

// ---------------------------------------------------------------- T-M2R1-02

std::uint64_t formula_inventory(std::uint64_t file_bytes) {
  return 4096 + std::min<std::uint64_t>(file_bytes / 80, 8 * 10000) * 128;
}

TEST(Inventory, T_M2R1_02_reservation_matches_the_formula_and_a_budget_below_the_minimum_stops_with_it) {
  gen g;
  g.w = 200;
  g.h = 50;
  g.planes = 2;
  g.value = [](std::uint32_t p, std::uint32_t x, std::uint32_t y) { return static_cast<double>(p * 1000 + x + y); };
  const on_disk f = put("t02", "a.fits", make_fits(g));
  ASSERT_EQ(f.bytes.size(), 43200u);
  const fits_input in = open_fits_input(f.path.string());

  const std::uint64_t inv_need = formula_inventory(43200);
  {
    memory_pool pool(big);
    const fits_inventory inv = inventory_fits(in, {}, pool);
    EXPECT_EQ(inv.reserved_bytes, inv_need);
    EXPECT_EQ(pool.peak_reserved_bytes(), inv_need);
    EXPECT_EQ(pool.reserved_bytes(), 0u);
  }
  {
    memory_pool pool(inv_need - 1);
    try {
      (void)inventory_fits(in, {}, pool);
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::budget);
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
      EXPECT_EQ(context_value(e, "minimo"), std::to_string(inv_need));
      EXPECT_EQ(context_value(e, "budget"), std::to_string(inv_need - 1));
    }
    EXPECT_EQ(pool.peak_reserved_bytes(), 0u);
    memory_pool exact_pool(inv_need);
    EXPECT_NO_THROW((void)inventory_fits(in, {}, exact_pool));
  }

  const preview_request req{16, 8};
  const std::uint64_t band = 200ull * 8 * 2, sums = 13ull * 12, out = 13ull * 7 * 2 * 8;
  const std::uint64_t need = inv_need + band + sums + out;
  {
    memory_pool pool(big);
    const fits_preview pv = make_preview(in, req, pool);
    EXPECT_EQ(pv.reserved_bytes, need);
    EXPECT_EQ(pool.peak_reserved_bytes(), need);
    EXPECT_LE(pv.observed_peak_bytes, pv.reserved_bytes);
    EXPECT_GE(pv.observed_peak_bytes, band + sums + out);
    EXPECT_EQ(pool.reserved_bytes(), 0u);
  }
  {
    memory_pool pool(need - 1);
    try {
      (void)make_preview(in, req, pool);
      ADD_FAILURE() << "stop atteso";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::budget);
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
      EXPECT_EQ(context_value(e, "minimo"), std::to_string(need));
      EXPECT_EQ(context_value(e, "budget"), std::to_string(need - 1));
    }
    memory_pool exact_pool(need);
    EXPECT_NO_THROW((void)make_preview(in, req, exact_pool));
    EXPECT_EQ(exact_pool.peak_reserved_bytes(), need);
  }
  memory_pool pool(big);
  EXPECT_STOP(make_preview(in, {0, 4}, pool), error_category::invalid_input, "preview.block");
  EXPECT_STOP(make_preview(in, {4, 0}, pool), error_category::invalid_input, "preview.block");
  EXPECT_STOP(make_preview(in, {65537, 4}, pool), error_category::invalid_input, "preview.block");
}

// ---------------------------------------------------------------- T-M2R1-03

TEST(Inventory, T_M2R1_03_preview_matches_the_block_mean_oracle) {
  memory_pool pool(big);
  const auto run = [&](const gen& g, std::uint32_t bw, std::uint32_t bh, bool exact, const std::string& name) {
    SCOPED_TRACE(name);
    const on_disk f = put("t03_" + name, "a.fits", make_fits(g));
    const fits_input in = open_fits_input(f.path.string());
    const fits_preview pv = make_preview(in, {bw, bh}, pool);
    EXPECT_EQ(pv.source_width, g.w);
    EXPECT_EQ(pv.source_height, g.h);
    expect_preview(g, pv, bw, bh, exact);
  };
  gen dyadic;
  dyadic.w = 64;
  dyadic.h = 32;
  dyadic.bzero = 32768;
  dyadic.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return 1000.0 + ((x * 37 + y * 11) % 5000); };
  run(dyadic, 8, 4, true, "dyadic16");

  gen bytes8;
  bytes8.bitpix = 8;
  bytes8.w = 33;
  bytes8.h = 20;
  bytes8.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return static_cast<double>((x * 5 + y * 3) % 256); };
  run(bytes8, 1, 1, true, "identity1x1");
  run(bytes8, 100, 100, false, "block_larger_than_image");

  gen f32;
  f32.bitpix = -32;
  f32.w = 53;
  f32.h = 37;
  f32.planes = 3;
  f32.value = [](std::uint32_t p, std::uint32_t x, std::uint32_t y) { return ((x * 131 + y * 17 + p * 7) % 1013) / 7.0 - 50.0; };
  run(f32, 7, 5, false, "f32_three_planes");

  gen f64;
  f64.bitpix = -64;
  f64.w = 30;
  f64.h = 21;
  f64.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) {
    if (x < 5 && y < 4) return std::numeric_limits<double>::quiet_NaN();  // blocco 5x4 tutto non finito
    if (x == 9 && y == 6) return std::numeric_limits<double>::infinity();
    if (x == 12 && y == 6) return -std::numeric_limits<double>::infinity();
    return std::ldexp(static_cast<double>((x * 41 + y * 13) % 997), -3) + 1e9;
  };
  run(f64, 5, 4, false, "f64_nonfinite");

  gen i32;
  i32.bitpix = 32;
  i32.bscale = 0.5;
  i32.bzero = 100;
  i32.w = 17;
  i32.h = 13;
  i32.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return 100.0 + 0.5 * static_cast<double>(x * 100000 + y * 7); };
  run(i32, 5, 3, false, "int32_scaled");
}

// ---------------------------------------------------------------- T-M2R1-05 (stato, dichiarazioni e tabella)

TEST(Phase, T_M2R1_05_operation_table_matrix) {
  const process_phase phases[] = {process_phase::raw, process_phase::calibrated, process_phase::prestacked};
  const linearity_class lins[] = {linearity_class::linear, linearity_class::nonlinear, linearity_class::unknown};
  for (const process_phase ph : phases) {
    for (const linearity_class ln : lins) {
      SCOPED_TRACE(std::string(to_string(ph)) + "/" + to_string(ln));
      const process_state st{ph, ln};
      for (const operation_kind free_op : {operation_kind::inventory, operation_kind::preview, operation_kind::export_bundle}) {
        EXPECT_NO_THROW(require_operation_allowed(free_op, st));
      }
      for (const operation_kind op : {operation_kind::master_create, operation_kind::calibrate}) {
        if (ph == process_phase::raw && ln == linearity_class::linear) {
          EXPECT_NO_THROW(require_operation_allowed(op, st));
        } else if (ph != process_phase::raw) {
          EXPECT_STOP(require_operation_allowed(op, st), error_category::incompatible, "op.phase_blocked");
        } else {
          EXPECT_STOP(require_operation_allowed(op, st), error_category::incompatible, "op.linearity_blocked");
        }
      }
      if (ln == linearity_class::linear) {
        EXPECT_NO_THROW(require_operation_allowed(operation_kind::linear_ancestor_required, st));
      } else {
        EXPECT_STOP(require_operation_allowed(operation_kind::linear_ancestor_required, st), error_category::incompatible,
                    "op.linear_ancestor_missing");
      }
      EXPECT_EQ(preflight_table(st).size(), operation_count);
    }
  }
  EXPECT_EQ(classify_linearity(std::nullopt), linearity_class::unknown);
  EXPECT_EQ(classify_linearity(linearity::linear), linearity_class::linear);
  EXPECT_EQ(classify_linearity(linearity::nonlinear), linearity_class::nonlinear);
}

gen small_gen(std::vector<std::string> cards) {
  gen g;
  g.bitpix = -32;
  g.w = 8;
  g.h = 4;
  g.cards = std::move(cards);
  g.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return static_cast<double>(x + 8 * y); };
  return g;
}

fits_inventory inventory_of(const gen& g, const fits_read_options& o = {}) {
  const on_disk f = put("phase", "a.fits", make_fits(g));
  const fits_input in = open_fits_input(f.path.string());
  memory_pool pool(big);
  return inventory_fits(in, o, pool);
}

TEST(Phase, T_M2R1_05_declarations_against_the_header) {
  // Fase: header, dichiarata, assente, in conflitto, valore ignoto.
  {
    const fits_inventory inv = inventory_of(small_gen({phase_card("calibrated")}));
    ASSERT_TRUE(inv.phase);
    EXPECT_EQ(inv.phase->value, process_phase::calibrated);
    EXPECT_EQ(inv.phase->source.origin, metadata_origin::header);
  }
  {
    fits_read_options o;
    o.declared_phase = process_phase::calibrated;
    const fits_inventory inv = inventory_of(small_gen({phase_card("Calibrated")}), o);
    EXPECT_EQ(inv.phase->source.origin, metadata_origin::header);
  }
  {
    fits_read_options o;
    o.declared_phase = process_phase::prestacked;
    const fits_inventory inv = inventory_of(small_gen({}), o);
    ASSERT_TRUE(inv.phase);
    EXPECT_EQ(inv.phase->value, process_phase::prestacked);
    EXPECT_EQ(inv.phase->source.origin, metadata_origin::declared);
  }
  EXPECT_FALSE(inventory_of(small_gen({})).phase.has_value());
  {
    fits_read_options o;
    o.declared_phase = process_phase::raw;
    EXPECT_STOP(inventory_of(small_gen({phase_card("calibrated")}), o), error_category::incompatible, "metadata.declared_conflict");
  }
  EXPECT_STOP(inventory_of(small_gen({phase_card("bogus")})), error_category::unsupported, "fits.phase");

  // Linearita': LINEAR presente o assente, dichiarata o no.
  const auto lin_inv = [&](std::vector<std::string> cards, std::optional<linearity_class> declared) {
    fits_read_options o;
    o.declared_linearity = declared;
    return inventory_of(small_gen(std::move(cards)), o);
  };
  {
    const fits_inventory inv = lin_inv({}, std::nullopt);
    EXPECT_EQ(inv.lin_class, linearity_class::unknown);
    EXPECT_FALSE(inv.lin_origin.has_value());
    EXPECT_FALSE(inv.lin.has_value());
  }
  {
    const fits_inventory inv = lin_inv({}, linearity_class::unknown);
    EXPECT_EQ(inv.lin_class, linearity_class::unknown);
    ASSERT_TRUE(inv.lin_origin);
    EXPECT_EQ(*inv.lin_origin, metadata_origin::declared);
    EXPECT_FALSE(inv.lin.has_value());
  }
  {
    const fits_inventory inv = lin_inv({}, linearity_class::nonlinear);
    EXPECT_EQ(inv.lin_class, linearity_class::nonlinear);
    EXPECT_EQ(*inv.lin_origin, metadata_origin::declared);
  }
  {
    const fits_inventory inv = lin_inv({fx::card_bool("LINEAR", true)}, std::nullopt);
    EXPECT_EQ(inv.lin_class, linearity_class::linear);
    EXPECT_EQ(*inv.lin_origin, metadata_origin::header);
  }
  {
    const fits_inventory inv = lin_inv({fx::card_bool("LINEAR", false)}, linearity_class::nonlinear);
    EXPECT_EQ(inv.lin_class, linearity_class::nonlinear);
    EXPECT_EQ(*inv.lin_origin, metadata_origin::header);
  }
  EXPECT_STOP(lin_inv({fx::card_bool("LINEAR", true)}, linearity_class::nonlinear), error_category::incompatible,
              "metadata.declared_conflict");
  EXPECT_STOP(lin_inv({fx::card_bool("LINEAR", true)}, linearity_class::unknown), error_category::incompatible,
              "metadata.declared_conflict");
  EXPECT_STOP(lin_inv({fx::card_bool("LINEAR", false)}, linearity_class::linear), error_category::incompatible,
              "metadata.declared_conflict");
}

TEST(Phase, T_M2R1_05_decoded_image_carries_the_declared_phase_and_roundtrips) {
  const gen g = small_gen({fx::card_bool("LINEAR", true)});
  fits_read_options o;
  o.declared_phase = process_phase::prestacked;
  const image img = decode_fits(make_fits(g), big, o);
  ASSERT_TRUE(img.phase);
  EXPECT_EQ(img.phase->value, process_phase::prestacked);
  EXPECT_EQ(img.phase->source.origin, metadata_origin::declared);
  const fx::bytes out = encode_fits(img);
  const image back = decode_fits(out, big);
  ASSERT_TRUE(back.phase);
  EXPECT_EQ(back.phase->value, process_phase::prestacked);
  EXPECT_EQ(back.phase->source.origin, metadata_origin::header);
  EXPECT_TRUE(equivalent_content(back, img));
  EXPECT_EQ(back.phase, img.phase);
  EXPECT_EQ(encode_fits(back), out);
  EXPECT_FALSE(decode_fits(make_fits(g), big).phase.has_value());
}


TEST(Inventory, T_M2R1_03_overflow_of_the_sum_or_of_the_f32_mean_stops_instead_of_averaging) {
  memory_pool pool(big);
  const auto preview_of = [&](const std::vector<double>& values, std::uint32_t bw) {
    gen g;
    g.bitpix = -64;
    g.w = static_cast<std::uint32_t>(values.size());
    g.h = 1;
    g.value = [&](std::uint32_t, std::uint32_t x, std::uint32_t) { return values[x]; };
    const on_disk f = put("t03_overflow", "a.fits", make_fits(g));
    const fits_input in = open_fits_input(f.path.string());
    return make_preview(in, {bw, 1}, pool);
  };
  EXPECT_STOP(preview_of({1e308, 1e308, -1e308, -1e308}, 4), error_category::numeric, "preview.sum_overflow");
  EXPECT_STOP(preview_of({1e300, 1e300}, 2), error_category::numeric, "preview.f32_overflow");
  const fits_preview ok = preview_of({1e308, -1e308, 1e308, -1e308}, 4);
  EXPECT_EQ(ok.samples, (std::vector<float>{0.0f}));
  EXPECT_EQ(ok.finite_counts, (std::vector<std::uint32_t>{4}));
}

TEST(Inventory, T_M2R1_10_a_file_changed_after_opening_is_detected_by_every_reader) {
  gen g = small_gen({phase_card("raw")});
  const on_disk f = put("t10_changed", "a.fits", make_fits(g));
  memory_pool pool(big);
  {
    fits_input in = open_fits_input(f.path.string());
    ASSERT_NO_THROW((void)digest_fits_input(in, pool));
    fx::bytes grown = f.bytes;
    grown.push_back(0);
    write_file(f.path, grown);
    EXPECT_STOP(inventory_fits(in, {}, pool), error_category::integrity, "input.size_changed");
    EXPECT_STOP(make_preview(in, {4, 4}, pool), error_category::integrity, "input.size_changed");
    EXPECT_STOP(digest_fits_input(in, pool), error_category::integrity, "input.size_changed");
  }
  write_file(f.path, f.bytes);
  {
    fits_input in = open_fits_input(f.path.string());
    ASSERT_NO_THROW((void)inventory_fits(in, {}, pool));
    const struct timespec times[2] = {{0, UTIME_OMIT}, {1000, 0}};
    ASSERT_EQ(::utimensat(AT_FDCWD, f.path.c_str(), times, 0), 0);
    EXPECT_STOP(inventory_fits(in, {}, pool), error_category::integrity, "input.modified");
    EXPECT_STOP(make_preview(in, {4, 4}, pool), error_category::integrity, "input.modified");
    EXPECT_STOP(digest_fits_input(in, pool), error_category::integrity, "input.modified");
  }
}

TEST(Phase, T_M2R1_05_equivalent_content_distinguishes_the_phase_but_not_its_origin) {
  const gen g = small_gen({fx::card_bool("LINEAR", true)});
  fits_read_options raw;
  raw.declared_phase = process_phase::raw;
  fits_read_options stacked;
  stacked.declared_phase = process_phase::prestacked;
  const image a = decode_fits(make_fits(g), big, raw);
  const image b = decode_fits(make_fits(g), big, stacked);
  const image none = decode_fits(make_fits(g), big);
  EXPECT_FALSE(equivalent_content(a, b));
  EXPECT_FALSE(equivalent_content(a, none));
  image header_origin = a;
  header_origin.phase->source.origin = metadata_origin::header;
  EXPECT_TRUE(equivalent_content(a, header_origin));
  EXPECT_FALSE(none.lin_origin.value.has_value() && !none.lin.has_value());
}


TEST(Phase, T_M2R1_05_origin_of_the_linearity_exists_only_when_the_linearity_does) {
  fits_read_options unknown;
  unknown.declared_linearity = linearity_class::unknown;
  const image u = decode_fits(make_fits(small_gen({})), big, unknown);
  EXPECT_FALSE(u.lin.has_value());
  EXPECT_FALSE(u.lin_origin.value.has_value());
  fits_read_options linear;
  linear.declared_linearity = linearity_class::linear;
  const image d = decode_fits(make_fits(small_gen({})), big, linear);
  ASSERT_TRUE(d.lin_origin.value.has_value());
  EXPECT_EQ(*d.lin_origin.value, metadata_origin::declared);
  const image h = decode_fits(make_fits(small_gen({fx::card_bool("LINEAR", true)})), big);
  EXPECT_EQ(*h.lin_origin.value, metadata_origin::header);
}

// ---------------------------------------------------------------- T-M2R1-10

fx::bytes with_cards(std::uint64_t n_cards) {
  gen g = small_gen({});
  for (std::uint64_t i = 0; i < n_cards; ++i) g.cards.push_back(fx::card_text("COMMENT", "x"));
  return make_fits(g);
}

TEST(InventoryFuzz, T_M2R1_10_limits_stop_inventory_and_preview_without_allocating_samples) {
  memory_pool pool(big);
  const std::uint64_t allocated = allocated_sample_bytes();
  const auto stops = [&](const fx::bytes& b, error_category cat, const char* code) {
    SCOPED_TRACE(code);
    const on_disk f = put("t10_limits", "a.fits", b);
    const fits_input in = open_fits_input(f.path.string());
    EXPECT_STOP(inventory_fits(in, {}, pool), cat, code);
    EXPECT_STOP(make_preview(in, {4, 4}, pool), cat, code);
  };
  {
    fx::raw_hdu h;
    h.bitpix = 16;
    h.axes = {2, 2, 2, 2};
    stops(fx::build_fits({h}), error_category::limit_exceeded, "fits.max_naxis");
  }
  {
    fx::raw_hdu h;
    h.bitpix = 8;
    h.axes = {65537, 1};
    stops(fx::build_fits({h}), error_category::limit_exceeded, "fits.max_axis_length");
  }
  {
    fx::raw_hdu h;
    h.bitpix = 8;
    h.axes = {2, 2, 17};
    stops(fx::build_fits({h}), error_category::limit_exceeded, "fits.max_planes");
  }
  {
    fx::raw_hdu h;
    h.bitpix = 8;
    h.axes = {65536, 4097};
    stops(fx::build_fits({h}), error_category::limit_exceeded, "fits.max_samples");
  }
  stops(with_cards(10000), error_category::limit_exceeded, "fits.max_header_cards");
  {
    std::vector<fx::raw_hdu> hdus;
    for (int i = 0; i < 9; ++i) {
      fx::raw_hdu h;
      h.primary = i == 0;
      h.bitpix = 8;
      h.axes = {2, 2};
      if (i > 0) h.cards = {fx::card_str("EXTNAME", "INVALID")};
      h.data.assign(4, 0);
      hdus.push_back(h);
    }
    stops(fx::build_fits(hdus), error_category::limit_exceeded, "fits.max_hdus");
  }
  {
    // Immagine compressa a tile: tabella binaria con ZIMAGE. Non e' un HDU immagine: rifiutata.
    fx::raw_hdu primary;
    primary.bitpix = 8;
    primary.axes = {2, 2};
    primary.data.assign(4, 0);
    fx::raw_hdu table;
    table.primary = false;
    table.xtension = "BINTABLE";
    table.bitpix = 8;
    table.axes = {8, 2};
    table.cards = {fx::card_bool("ZIMAGE", true)};
    table.data.assign(16, 0);
    stops(fx::build_fits({primary, table}), error_category::unsupported, "fits.hdu_kind");
  }
  EXPECT_EQ(allocated_sample_bytes(), allocated);
}

TEST(InventoryFuzz, T_M2R1_10_a_read_past_the_end_of_the_file_is_a_controlled_stop) {
  const on_disk f = put("t10_read", "a.fits", make_fits(small_gen({})));
  const fits_input in = open_fits_input(f.path.string());
  std::uint8_t buffer[16];
  const std::uint64_t size = in.size_bytes();
  EXPECT_NO_THROW(fits_detail::read_exact(in, size - 16, buffer, 16));
  EXPECT_STOP(fits_detail::read_exact(in, size - 8, buffer, 16), error_category::invalid_input, "fits.malformed");
  EXPECT_STOP(fits_detail::read_exact(in, size + 1, buffer, 1), error_category::invalid_input, "fits.malformed");
  EXPECT_STOP(fits_detail::read_exact(in, 0, buffer, std::numeric_limits<std::size_t>::max()), error_category::invalid_input,
              "fits.malformed");
}

TEST(InventoryFuzz, T_M2R1_10_corrupted_files_stop_in_a_controlled_way) {
  gen g = small_gen({fx::card_str("OBSERVER", "fuzz"), phase_card("raw"), fx::card_bool("LINEAR", true)});
  g.bitpix = 16;
  g.w = 40;
  g.h = 20;
  g.value = [](std::uint32_t, std::uint32_t x, std::uint32_t y) { return static_cast<double>((x * 7 + y) % 300); };
  const fx::bytes base = make_fits(g, true);
  const fs::path path = fresh("t10_fuzz") / "a.fits";
  memory_pool pool(big);
  std::mt19937_64 rng(0x5eed);
  std::size_t stopped = 0, passed = 0;
  for (int iter = 0; iter < 800; ++iter) {
    fx::bytes b = base;
    switch (iter % 4) {
      case 0: {
        const int flips = 1 + static_cast<int>(rng() % 3);
        for (int i = 0; i < flips; ++i) b[rng() % 2880] = static_cast<std::uint8_t>(rng());
        break;
      }
      case 1: b.resize(rng() % b.size()); break;
      case 2: {
        const std::size_t card = rng() % 36;
        for (std::size_t i = 0; i < 80; ++i) b[card * 80 + i] = static_cast<std::uint8_t>(0x20 + rng() % 95);
        break;
      }
      default: {
        const std::size_t hdr2 = 5760;  // header dell'estensione INVALID
        for (int i = 0; i < 2; ++i) b[hdr2 + rng() % 2880] = static_cast<std::uint8_t>(rng());
        break;
      }
    }
    write_file(path, b);
    try {
      fits_input in = open_fits_input(path.string());
      (void)inventory_fits(in, {}, pool);
      (void)make_preview(in, {8, 4}, pool);
      ++passed;
    } catch (const stop_error&) {
      ++stopped;
    }
  }
  EXPECT_GE(stopped, 100u);
  EXPECT_GE(passed, 100u);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

}  // namespace
