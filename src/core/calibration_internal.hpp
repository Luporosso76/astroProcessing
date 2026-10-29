#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <optional>
#include <type_traits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/json.hpp"

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

// Nomi, metadati, rappresentabilita' e mediana condivisi da calibrazione e master (C-02.a, ADR-16).

inline const char* role_name(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* additive_name(additive_state s) {
  switch (s) {
    case additive_state::bias_included: return "bias_included";
    case additive_state::bias_subtracted: return "bias_subtracted";
    case additive_state::unknown: return "unknown";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* scaling_name(scaling_state s) {
  switch (s) {
    case scaling_state::unscaled: return "unscaled";
    case scaling_state::scaled: return "scaled";
    case scaling_state::unknown: return "unknown";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* flat_name(flat_state s) {
  switch (s) {
    case flat_state::raw: return "raw";
    case flat_state::corrected_unnormalized: return "corrected_unnormalized";
    case flat_state::calibrated: return "calibrated";
    case flat_state::unknown: return "unknown";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* combine_name(combine_method m) {
  switch (m) {
    case combine_method::mean: return "mean";
    case combine_method::median: return "median";
    case combine_method::other: return "other";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* rejection_name(master_rejection r) {
  switch (r) {
    case master_rejection::none: return "none";
    case master_rejection::sigma_clip: return "sigma_clip";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* estimator_name(variance_estimator e) {
  switch (e) {
    case variance_estimator::empirical: return "empirical";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* layout_name(plane_layout l) {
  switch (l) {
    case plane_layout::mono: return "mono";
    case plane_layout::cfa: return "cfa";
    case plane_layout::rgb: return "rgb";
    case plane_layout::multi_plane: return "multi_plane";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline const char* pattern_name(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return "rggb";
    case cfa_pattern::bggr: return "bggr";
    case cfa_pattern::grbg: return "grbg";
    case cfa_pattern::gbrg: return "gbrg";
  }
  ASTRO_ASSERT(false);
  return "";
}

inline std::string cfa_text(const std::optional<cfa_layout>& c) {
  if (!c) return "none";
  return std::string(pattern_name(c->pattern)) + "@" + std::to_string(c->x_offset) + "," + std::to_string(c->y_offset);
}

// Etichette dei quattro canali nell'ordine delle posizioni (x, y) della cella 2x2; G1 precede G2 in ordine raster.
inline std::array<const char*, 4> channel_labels(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return {"R", "G1", "G2", "B"};
    case cfa_pattern::bggr: return {"B", "G1", "G2", "R"};
    case cfa_pattern::grbg: return {"G1", "R", "B", "G2"};
    case cfa_pattern::gbrg: return {"G1", "B", "R", "G2"};
  }
  ASTRO_ASSERT(false);
  return {"", "", "", ""};
}

// Campi di image_fields assenti dall'header e riempiti dalla ricetta (origine `declared` nel report).
struct field_origins {
  bool binning_x = false, binning_y = false, unit = false, lin = false;
};

inline field_origins apply_declared(image& im, const frame_metadata& d, const declared_image_fields& f, const char* name) {
  const auto apply = [&](auto& field, const auto& declared, const char* field_name) {
    if (!declared) return;
    if (declared->source.origin != metadata_origin::declared) {
      throw stop_error(error_category::invalid_input, "dag.metadata_not_declared", "la ricetta accetta solo valori dichiarati",
           {{"input", name}, {"campo", field_name}});
    }
    declare_metadata(field, declared->value, field_name);
  };
  frame_metadata& m = im.metadata;
  apply(m.exposure_s, d.exposure_s, "exposure_s");
  apply(m.gain, d.gain, "gain");
  apply(m.conversion_gain_e_per_adu, d.conversion_gain_e_per_adu, "conversion_gain_e_per_adu");
  apply(m.offset_adu, d.offset_adu, "offset_adu");
  apply(m.temperature_c, d.temperature_c, "temperature_c");
  apply(m.filter, d.filter, "filter");
  apply(m.camera, d.camera, "camera");
  apply(m.imagetyp, d.imagetyp, "imagetyp");
  apply(m.saturation_adu, d.saturation_adu, "saturation_adu");
  apply(m.pedestal_adu, d.pedestal_adu, "pedestal_adu");
  apply(m.roi_x, d.roi_x, "roi_x");
  apply(m.roi_y, d.roi_y, "roi_y");
  apply(m.readout_mode, d.readout_mode, "readout_mode");
  apply(m.row_order, d.row_order, "row_order");

  const auto declare_plain = [&](auto& field, const auto& value, const char* field_name) {
    if (!value) return;
    if (field && *field != *value) {
      throw stop_error(error_category::incompatible, "metadata.declared_conflict", "valore dichiarato diverso da quello presente",
           {{"input", name}, {"campo", field_name}});
    }
    field = *value;
  };
  field_origins origins;
  origins.binning_x = f.binning_x && !im.geometry.binning_x;
  origins.binning_y = f.binning_y && !im.geometry.binning_y;
  origins.unit = f.unit && !im.unit;
  origins.lin = (f.lin && !im.lin) || (im.lin && im.lin_origin.value == metadata_origin::declared);
  declare_plain(im.geometry.binning_x, f.binning_x, "binning_x");
  declare_plain(im.geometry.binning_y, f.binning_y, "binning_y");
  declare_plain(im.unit, f.unit, "unit");
  declare_plain(im.lin, f.lin, "linearity");
  if (origins.lin) im.lin_origin.value = metadata_origin::declared;
  return origins;
}

inline std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// Ruolo indicato da un IMAGETYP riconosciuto; nullopt per un valore non riconosciuto.
inline std::optional<std::string> imagetyp_role(const std::string& value) {
  const std::string v = lower(value);
  if (v == "light" || v == "light frame") return "light";
  if (v == "dark" || v == "dark frame") return "dark";
  if (v == "flat" || v == "flat field" || v == "flat frame") return "flat";
  if (v == "bias" || v == "bias frame") return "bias";
  if (v == "dark flat" || v == "flat dark") return "dark_flat";
  return std::nullopt;
}

inline std::string num_text(double v) {
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", v);
  return b;
}

inline double median_of(std::vector<double>& v) {
  const std::size_t n = v.size();
  const std::size_t mid = n / 2;
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
  const double hi = v[mid];
  if (n % 2 == 1) return hi;
  const double lo = *std::max_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid));
  return (lo + hi) * 0.5;
}

inline json metadata_report(const image& im) {
  json out = json::object();
  const auto add = [&](const char* name, const auto& field, auto to_json) {
    if (!field) return;
    json e = json::object();
    e.set("value", to_json(field->value));
    e.set("origin", json::string(field->source.origin == metadata_origin::header ? "header" : "declared"));
    if (!field->source.keyword.empty()) e.set("keyword", json::string(field->source.keyword));
    out.set(name, std::move(e));
  };
  const auto d = [](double v) { return json::number(v); };
  const auto s = [](const std::string& v) { return json::string(v); };
  const auto i = [](std::int64_t v) { return json::integer(v); };
  const frame_metadata& m = im.metadata;
  add("exposure_s", m.exposure_s, d);
  add("gain", m.gain, d);
  add("conversion_gain_e_per_adu", m.conversion_gain_e_per_adu, d);
  add("offset_adu", m.offset_adu, d);
  add("temperature_c", m.temperature_c, d);
  add("filter", m.filter, s);
  add("camera", m.camera, s);
  add("imagetyp", m.imagetyp, s);
  add("saturation_adu", m.saturation_adu, d);
  add("pedestal_adu", m.pedestal_adu, d);
  add("roi_x", m.roi_x, i);
  add("roi_y", m.roi_y, i);
  add("readout_mode", m.readout_mode, s);
  add("row_order", m.row_order, s);
  return out;
}


template <class T>
inline bool convert_finite(double v, T& out) {
  if (!std::isfinite(v)) return false;
  if constexpr (std::is_same_v<T, float>) {
    if (std::fabs(v) > static_cast<double>(FLT_MAX)) return false;
  }
  out = static_cast<T>(v);
  // Valore non nullo che nel tipo di uscita diventa zero o subnormale: non rappresentabile in modo normale.
  return v == 0 || std::fpclassify(out) == FP_NORMAL;
}

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
