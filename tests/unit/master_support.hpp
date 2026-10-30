#pragma once

// Supporto dei test dei master del prodotto (C-02.a, R2): frame sintetici scritti dal writer della fixture,
// ricetta, oracle razionale della media e del piano s^2/n.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/master.hpp"
#include "calib_support.hpp"
#include "exact_rational.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace mst {

using namespace astro;
namespace fx = astro_fixture;
namespace fs = std::filesystem;
using exact::rat;

struct mframe {
  std::vector<double> v;
  fx::calib_header hdr;
  int bitpix = -64;  // -64, -32 oppure 16 (BZERO 32768, valori interi in [0, 65535])
  std::optional<std::vector<std::uint16_t>> invalid;  // HDU INVALID
  std::optional<std::vector<double>> variance;        // HDU VARIANCE (solo float)
  std::vector<std::string> omit_keys;                  // card di calib_cards da togliere (metadato assente)
};

inline std::vector<std::string> cards_of(const mframe& f) {
  std::vector<std::string> out;
  for (const std::string& c : fx::calib_cards(f.hdr)) {
    const std::string key = c.substr(0, c.find_first_of(" ="));
    if (std::find(f.omit_keys.begin(), f.omit_keys.end(), key) == f.omit_keys.end()) out.push_back(c);
  }
  return out;
}

struct group {
  std::uint32_t w = 8, h = 4;
  master_role role = master_role::dark;
  std::vector<mframe> frames;
};

inline const char* imagetyp_of(master_role r) {
  switch (r) {
    case master_role::bias: return "Bias Frame";
    case master_role::dark: return "Dark Frame";
    case master_role::flat: return "Flat Field";
    case master_role::dark_flat: return "Dark Flat";
  }
  return "";
}

inline fx::bytes file_of(const mframe& f, std::uint32_t w, std::uint32_t h) {
  if (f.bitpix != 16) {
    if (f.invalid) ADD_FAILURE() << "INVALID solo sui frame a 16 bit nella fixture";
    if (f.variance) return fx::float_frame_with_variance(w, h, f.bitpix, f.v, *f.variance, cards_of(f));
    return fx::float_frame(w, h, f.bitpix, f.v, cards_of(f));
  }
  fx::raw_hdu p;
  p.bitpix = 16;
  p.axes = {w, h};
  p.cards = {fx::card_real("BSCALE", 1.0), fx::card_real("BZERO", 32768.0)};
  for (const std::string& c : cards_of(f)) p.cards.push_back(c);
  for (double v : f.v) {
    if (v != std::floor(v) || v < 0 || v > 65535) ADD_FAILURE() << "valore non intero a 16 bit: " << v;
    const auto raw = static_cast<std::uint16_t>(static_cast<std::int32_t>(v) - 32768);
    fx::put_be(p.data, raw, 2);
  }
  std::vector<fx::raw_hdu> hdus = {p};
  if (f.invalid) {
    fx::raw_hdu e;
    e.primary = false;
    e.bitpix = 16;
    e.axes = {w, h};
    e.cards = {fx::card_str("EXTNAME", "INVALID"), fx::card_real("BSCALE", 1.0), fx::card_real("BZERO", 32768.0)};
    for (std::uint16_t b : *f.invalid) fx::put_be(e.data, static_cast<std::uint16_t>(b ^ 0x8000u), 2);
    hdus.push_back(e);
  }
  return fx::build_fits(hdus);
}

inline image decode(const mframe& f, std::uint32_t w, std::uint32_t h, const fits_read_options& o = {}) {
  return decode_fits(file_of(f, w, h), cal::big_budget, o);
}

inline mframe frame_for(master_role role, std::uint32_t w, std::uint32_t h, int bitpix = -64) {
  mframe f;
  f.bitpix = bitpix;
  f.hdr.imagetyp = imagetyp_of(role);
  f.hdr.exposure = role == master_role::flat || role == master_role::dark_flat ? 5 : 60;
  f.v.assign(static_cast<std::size_t>(w) * h, 0.0);
  return f;
}

inline group make_group(master_role role, std::uint32_t w, std::uint32_t h, std::size_t n, int bitpix = -64) {
  group g;
  g.w = w;
  g.h = h;
  g.role = role;
  // Una card per frame: file e digest distinti anche con campioni uguali (calib.shared_input non scatta).
  for (std::size_t k = 0; k < n; ++k) {
    g.frames.push_back(frame_for(role, w, h, bitpix));
    g.frames.back().hdr.extra = {fx::card_int("FRAMENO", static_cast<long long>(k))};
  }
  return g;
}

inline std::vector<image> images_of(const group& g) {
  std::vector<image> out;
  for (const mframe& f : g.frames) out.push_back(decode(f, g.w, g.h));
  return out;
}

// Ricetta valida per il ruolo: media senza rejection; per il flat livelli entro flat_tol e segnale minimo 1 ADU.
inline master_recipe recipe_of(const group& g, double flat_tol = 0.25) {
  master_recipe r;
  r.role = g.role;
  for (const mframe& f : g.frames) {
    master_frame_spec s;
    s.digest = digest_of(file_of(f, g.w, g.h));
    r.frames.push_back(s);
  }
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  if (g.role == master_role::flat) {
    r.flat_norm = flat_normalization::none;
    r.flat_level_rel_tol = flat_tol;
    r.flat_min_signal_adu = 1.0;
  }
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  r.simd = simd_test::requested();
  r.output_type = sample_type::f64;
  return r;
}

// Valori memorizzati nel file (arrotondati al tipo del BITPIX).
inline double stored(const mframe& f, std::size_t i) {
  return f.bitpix == -32 ? static_cast<double>(static_cast<float>(f.v[i])) : f.v[i];
}

struct oracle_pixel {
  rat mean, plane, sum_abs;
};

// Media e piano s^2/n esatti in aritmetica razionale dai valori memorizzati.
inline std::vector<oracle_pixel> oracle(const group& g) {
  const std::size_t px = static_cast<std::size_t>(g.w) * g.h;
  const std::size_t n = g.frames.size();
  std::vector<oracle_pixel> out(px);
  for (std::size_t i = 0; i < px; ++i) {
    rat sum = exact::from_u(0), sabs = exact::from_u(0);
    for (const mframe& f : g.frames) {
      sum = sum + exact::from_double(stored(f, i));
      sabs = sabs + exact::abs(exact::from_double(stored(f, i)));
    }
    const rat mean = sum / exact::from_u(n);
    rat ss = exact::from_u(0);
    for (const mframe& f : g.frames) {
      const rat d = exact::from_double(stored(f, i)) - mean;
      ss = ss + d * d;
    }
    out[i] = {mean, ss / exact::from_u(n - 1) / exact::from_u(n), sabs};
  }
  return out;
}

inline rat half_ulp_out(const rat& exact_value, double got, bool f32) {
  if (!f32) return exact::from_u(0);
  const rat a = exact::abs(exact_value), b = exact::abs(exact::from_double(got));
  return cal::half_ulp_f32(a < b ? b : a);
}

// T-M2R2-02: |M - mean| <= gamma_n sum|x| / n (+ mezzo ulp f32).
inline ::testing::AssertionResult mean_close(const oracle_pixel& o, double got, std::size_t n, bool f32) {
  const rat tol = exact::gamma(static_cast<unsigned>(n)) * o.sum_abs / exact::from_u(n) + half_ulp_out(o.mean, got, f32);
  const rat diff = exact::abs(exact::from_double(got) - o.mean);
  if (diff <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "media: scarto " << exact::to_ld(diff) << " oltre " << exact::to_ld(tol);
}

// T-M2R2-02, C-02.a §6.3: |V - V_esatto| <= gamma_(n+4) V + (1 + gamma_(n+4)) Delta^2 / (n - 1) (+ mezzo ulp f32).
inline ::testing::AssertionResult plane_close(const oracle_pixel& o, double got, std::size_t n, bool f32) {
  const rat g = exact::gamma(static_cast<unsigned>(n + 4));
  const rat delta = exact::gamma(static_cast<unsigned>(n)) * o.sum_abs / exact::from_u(n);
  const rat one = exact::from_u(1);
  const rat tol = g * o.plane + (one + g) * delta * delta / exact::from_u(n - 1) + half_ulp_out(o.plane, got, f32);
  const rat diff = exact::abs(exact::from_double(got) - o.plane);
  if (diff <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "piano: scarto " << exact::to_ld(diff) << " oltre " << exact::to_ld(tol);
}

inline double sample(const image& im, std::size_t i) { return cal::sample_at(im, i); }
inline double plane(const image& im, std::size_t i) { return cal::variance_at(im, i); }

inline fs::path fresh_dir(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / simd_test::tmp_subdir() / "r2" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

// Scrive i frame su disco e completa i percorsi della ricetta.
inline void write_frames(const group& g, master_recipe& r, const fs::path& dir, const std::string& prefix) {
  for (std::size_t k = 0; k < g.frames.size(); ++k) {
    const fs::path p = dir / (prefix + "-" + std::to_string(k) + ".fits");
    cal::write_bytes(p, file_of(g.frames[k], g.w, g.h));
    r.frames[k].path = p.string();
  }
}

inline const cancel_token& never_cancelled() {
  static const cancel_token token;
  return token;
}

inline job_outcome run_master_quiet(sqlite_dag_store& store, const std::string& slot, const master_recipe& r,
                                    memory_pool& pool, const master_job_options& o = {}) {
  testing::internal::CaptureStderr();
  try {
    job_outcome out = run_master_job(store, slot, r, pool, o, never_cancelled());
    testing::internal::GetCapturedStderr();
    return out;
  } catch (...) {
    testing::internal::GetCapturedStderr();
    throw;
  }
}

inline std::vector<std::uint8_t> read_artifact(const sqlite_dag_store& store, const artifact_record& a) {
  const std::string text = cal::read_text(store.artifact_path(a.rel_path));
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

inline std::map<std::string, artifact_record> by_role(const sqlite_dag_store& store, const std::string& key) {
  std::map<std::string, artifact_record> out;
  for (const artifact_record& a : store.artifacts_for(key)) out[a.output_role] = a;
  return out;
}

// Campioni, maschera e varianza bit per bit.
inline bool same_bits(const image& a, const image& b) {
  if (a.type != b.type || a.samples.size() != b.samples.size() || !a.variance || !b.variance) return false;
  return std::memcmp(a.samples.data(), b.samples.data(), a.samples.size()) == 0 && a.invalid.bits == b.invalid.bits &&
         std::memcmp(a.variance->samples.data(), b.variance->samples.data(), a.samples.size()) == 0;
}

}  // namespace mst
