#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "astro/calibration.hpp"

namespace astro::calib_detail {

constexpr double iid_cv_threshold = 0.01;
constexpr std::size_t input_count = 4;

// Criterio di rappresentabilita': un risultato non nullo da operandi non nulli deve essere normale; zero o subnormale
// (underflow) o non finito non sono ammessi (fpclassify, nessun accesso all'ambiente in virgola mobile).
inline bool is_normal(double r) noexcept { return std::fpclassify(r) == FP_NORMAL; }
inline bool is_finite_ok(double r) noexcept { return std::isfinite(r); }
inline bool mul_ok(double a, double b, double r) noexcept { return (a == 0 || b == 0) ? r == 0 : is_normal(r); }
inline bool div_ok(double a, double r) noexcept { return a == 0 ? r == 0 : is_normal(r); }
inline bool add_ok(double a, double b, double r) noexcept { return (a == 0 && b == 0) ? r == 0 : is_normal(r); }

// Vista di un input: campioni, maschera, varianza (piano o modello) e saturazione dichiarata.
struct input_view {
  const void* samples = nullptr;
  const void* variance_plane = nullptr;
  const std::uint16_t* mask = nullptr;
  bool f32 = true;
  bool has_saturation = false;
  double saturation = 0;
  bool plane = true;
  // noise_model: Var(X) = (read_noise^2 + max(X - offset, 0) / gain) / n
  double gain = 1, read_noise_sq = 0, offset = 0, n = 1;

  double value(std::size_t i) const noexcept {
    return f32 ? static_cast<double>(static_cast<const float*>(samples)[i])
               : static_cast<const double*>(samples)[i];
  }
  // NaN se un passo del modello non e' rappresentabile in modo normale (underflow o overflow).
  double variance(std::size_t i, double x) const noexcept {
    if (plane) {
      return f32 ? static_cast<double>(static_cast<const float*>(variance_plane)[i])
                 : static_cast<const double*>(variance_plane)[i];
    }
    const double above = x - offset;
    const double a = above > 0.0 ? above : 0.0;
    const double poisson = a / gain;
    const double sum = read_noise_sq + poisson;
    const double var = sum / n;
    if (!div_ok(a, poisson) || !add_ok(read_noise_sq, poisson, sum) || !div_ok(sum, var) || !std::isfinite(var)) {
      return std::numeric_limits<double>::quiet_NaN();
    }
    return var;
  }
};

}  // namespace astro::calib_detail

namespace astro {

struct prepared_calibration::impl {
  calibration_recipe recipe;
  calibration_images images;
  calib_detail::input_view light, dark, flat, dark_flat;
  std::uint32_t w = 0, h = 0;
  bool per_channel = false;
  std::uint8_t cfa_x = 0, cfa_y = 0;
  double norm[4] = {0, 0, 0, 0};
  double norm_sq[4] = {0, 0, 0, 0};
  double q_min = 0, q_rel_sigma_max = 0;
  image out;
  std::vector<channel_normalization> channels;
  std::vector<std::string> warnings;
  json report = json::object();

  std::size_t channel_of(std::uint32_t x, std::uint32_t y) const noexcept {
    return per_channel ? static_cast<std::size_t>(((y + cfa_y) & 1u) * 2 + ((x + cfa_x) & 1u)) : 0;
  }
};

}  // namespace astro
