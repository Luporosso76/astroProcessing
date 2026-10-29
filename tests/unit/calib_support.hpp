#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "astro/calibration.hpp"
#include "astro/fits.hpp"
#include "astro/json.hpp"
#include "exact_rational.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace cal {

using namespace astro;
namespace fx = astro_fixture;
namespace fs = std::filesystem;
using exact::rat;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 30;

struct frame_def {
  std::vector<double> v;
  fx::calib_header hdr;
  int bitpix = -64;
  std::uint32_t planes = 1;
  std::optional<std::vector<double>> variance;
};

struct set_def {
  std::uint32_t w = 8, h = 4;
  frame_def light, dark, flat, dark_flat;
};

inline double stored(double v, int bitpix) { return bitpix == -32 ? static_cast<double>(static_cast<float>(v)) : v; }

inline fx::bytes file_of(const frame_def& f, std::uint32_t w, std::uint32_t h) {
  if (f.variance) return fx::float_frame_with_variance(w, h, f.bitpix, f.v, *f.variance, fx::calib_cards(f.hdr));
  return fx::float_frame(w, h, f.bitpix, f.v, fx::calib_cards(f.hdr), f.planes);
}

inline image decode(const frame_def& f, std::uint32_t w, std::uint32_t h, const fits_read_options& o = {}) {
  return decode_fits(file_of(f, w, h), big_budget, o);
}

inline content_digest digest_for(const frame_def& f, std::uint32_t w, std::uint32_t h) {
  return digest_of(file_of(f, w, h));
}

inline calibration_images images_of(const set_def& s) {
  return {decode(s.light, s.w, s.h), decode(s.dark, s.w, s.h), decode(s.flat, s.w, s.h),
          decode(s.dark_flat, s.w, s.h)};
}

inline master_descriptor descriptor(master_role role, std::uint32_t count = 4) {
  master_descriptor d;
  d.role = role;
  if (role == master_role::flat) d.flat = flat_state::raw;
  else {
    d.additive = additive_state::bias_included;
    d.scaling = scaling_state::unscaled;
  }
  d.combine_count = count;
  d.combine = combine_method::mean;
  return d;
}

inline variance_spec noise(double read_noise) {
  variance_spec v;
  v.source = variance_source::noise_model;
  v.read_noise_adu = read_noise;
  return v;
}

inline calibration_recipe recipe_of(const set_def& s, double read_noise = 2.0) {
  calibration_recipe r;
  r.light.digest = digest_for(s.light, s.w, s.h);
  r.dark.digest = digest_for(s.dark, s.w, s.h);
  r.flat.digest = digest_for(s.flat, s.w, s.h);
  r.dark_flat.digest = digest_for(s.dark_flat, s.w, s.h);
  r.dark.descriptor = descriptor(master_role::dark);
  r.flat.descriptor = descriptor(master_role::flat);
  r.dark_flat.descriptor = descriptor(master_role::dark_flat);
  for (calibration_input_spec* in : {&r.light, &r.dark, &r.flat, &r.dark_flat}) in->variance = noise(read_noise);
  r.normalization = normalization_mode::global_median;
  r.q_min = 0.01;
  r.q_rel_sigma_max = 10.0;
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  return r;
}

inline fx::calib_header header_for(const char* imagetyp, double exposure, double egain = 2.0) {
  fx::calib_header h;
  h.imagetyp = imagetyp;
  h.exposure = exposure;
  h.egain = egain;
  return h;
}

inline set_def empty_set(std::uint32_t w, std::uint32_t h, int bitpix, double egain = 2.0) {
  set_def s;
  s.w = w;
  s.h = h;
  s.light.hdr = header_for("Light Frame", 60, egain);
  s.dark.hdr = header_for("Dark Frame", 60, egain);
  s.flat.hdr = header_for("Flat Field", 5, egain);
  s.dark_flat.hdr = header_for("Dark Flat", 5, egain);
  for (frame_def* f : {&s.light, &s.dark, &s.flat, &s.dark_flat}) {
    f->bitpix = bitpix;
    f->v.assign(static_cast<std::size_t>(w) * h, 0.0);
  }
  return s;
}

// ---------------------------------------------------------------- oracle razionale

struct oracle_params {
  double egain = 2.0;
  double offset = 10.0;
  double read_noise = 2.0;
  unsigned master_n = 4;
};

struct oracle_pixel {
  rat c, var;
};

inline rat var_model(double x, const oracle_params& p, unsigned n) {
  const rat above = exact::from_double(x) - exact::from_double(p.offset);
  const rat zero = exact::from_u(0);
  const rat clamped = above < zero ? zero : above;
  const rat rn2 = exact::from_double(p.read_noise) * exact::from_double(p.read_noise);
  return (rn2 + clamped / exact::from_double(p.egain)) / exact::from_u(n);
}

// `channel[i]` assegna ogni pixel a un canale di normalizzazione; i pixel non validi per N non sono previsti.
inline std::vector<oracle_pixel> oracle_run(const set_def& s, const oracle_params& p,
                                            const std::vector<int>& channel, int channels) {
  const std::size_t n = static_cast<std::size_t>(s.w) * s.h;
  std::vector<std::vector<rat>> fcs(static_cast<std::size_t>(channels));
  std::vector<rat> fc(n);
  for (std::size_t i = 0; i < n; ++i) {
    fc[i] = exact::from_double(stored(s.flat.v[i], s.flat.bitpix)) - exact::from_double(stored(s.dark_flat.v[i], s.dark_flat.bitpix));
    fcs[static_cast<std::size_t>(channel[i])].push_back(fc[i]);
  }
  std::vector<rat> nn;
  for (auto& v : fcs) nn.push_back(exact::median(v));
  std::vector<oracle_pixel> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double l = stored(s.light.v[i], s.light.bitpix), d = stored(s.dark.v[i], s.dark.bitpix);
    const double f = stored(s.flat.v[i], s.flat.bitpix), df = stored(s.dark_flat.v[i], s.dark_flat.bitpix);
    const rat& big_n = nn[static_cast<std::size_t>(channel[i])];
    const rat lc = exact::from_double(l) - exact::from_double(d);
    const rat q = fc[i] / big_n;
    out[i].c = lc / q;
    const rat vlc = var_model(l, p, 1) + var_model(d, p, p.master_n);
    const rat vfc = var_model(f, p, p.master_n) + var_model(df, p, p.master_n);
    const rat vq = vfc / (big_n * big_n);
    const rat q2 = q * q;
    out[i].var = vlc / q2 + (lc * lc * vq) / (q2 * q2);
  }
  return out;
}

inline std::vector<int> all_zero(const set_def& s) { return std::vector<int>(static_cast<std::size_t>(s.w) * s.h, 0); }

inline rat half_ulp_f32(const rat& value) {
  const long double a = std::fabs(exact::to_ld(value));
  if (a == 0) return exact::from_u(0);
  const int e = std::ilogb(a);
  // Mezzo ulp = 2^(e-24); nei subnormali f32 (|v| < 2^-126) lo spacing e' 2^-149, quindi il minimo e' 2^-150.
  const int k = std::max(e - 24, -150);
  if (k >= 0) return rat{exact::shl(exact::from_u64(1), static_cast<unsigned>(k)), exact::from_u64(1)};
  return rat{exact::from_u64(1), exact::shl(exact::from_u64(1), static_cast<unsigned>(-k))};
}

// Tolleranza derivata: zero se m == 0; altrimenti gamma_m * |esatto|, piu' mezzo ulp f32 per l'uscita f32.
inline ::testing::AssertionResult close_to(const rat& exact_value, double got, unsigned m, bool f32) {
  const rat diff = exact::abs(exact::from_double(got) - exact_value);
  rat tol = exact::from_u(0);
  if (m > 0) tol = exact::gamma(m) * exact::abs(exact_value);
  if (f32) tol = tol + half_ulp_f32(exact_value);
  if (diff <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure() << "scarto " << exact::to_ld(diff) << " oltre la tolleranza " << exact::to_ld(tol)
                                       << " (atteso " << exact::to_ld(exact_value) << ", ottenuto " << got << ")";
}

inline double sample_at(const image& im, std::size_t i) {
  return im.type == sample_type::f32 ? static_cast<double>(im.data<float>()[i]) : im.data<double>()[i];
}

inline double variance_at(const image& im, std::size_t i) {
  return im.type == sample_type::f32 ? static_cast<double>(im.variance_data<float>()[i]) : im.variance_data<double>()[i];
}

inline const json& at(const json& doc, std::initializer_list<const char*> path) {
  const json* cur = &doc;
  for (const char* k : path) {
    cur = cur->find(k);
    if (cur == nullptr) {
      ADD_FAILURE() << "chiave assente nel report: " << k;
      static const json empty = json::null();
      return empty;
    }
  }
  return *cur;
}

inline double as_double(const json& j) {
  return j.type() == json::kind::integer ? static_cast<double>(j.as_integer()) : j.as_number();
}

inline fs::path fresh_dir(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "m2e" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

inline void write_bytes(const fs::path& path, const std::vector<std::uint8_t>& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

inline std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Frame sintetici del modello FIX-SYN-FITS-01 (B, d, q, S, K noti) come valori double esatti.
inline set_def synth_set(std::uint32_t w, std::uint32_t h, int bitpix, double egain = 2.0) {
  set_def s = empty_set(w, h, bitpix, egain);
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * w + x;
      const double b = fx::synth_bias(x, y);
      const double d = fx::synth_dark_rate(x, y);
      const double q = fx::synth_flat_response(x, y);
      const double sky = fx::synth_sky(x, y, 0);
      s.dark.v[i] = b + 60 * d;
      s.light.v[i] = b + 60 * d + q * sky / 8192.0;
      s.dark_flat.v[i] = b + 5 * d;
      s.flat.v[i] = b + 5 * d + q;
    }
  }
  return s;
}

}  // namespace cal
