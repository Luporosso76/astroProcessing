#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <limits>

#include "astro/calibration.hpp"
#include "astro/error.hpp"
#include "calibration_internal.hpp"

namespace astro {

using calib_detail::input_view;

namespace {

[[noreturn]] void stop(error_category category, std::string code, std::string message,
                       stop_error::context_list context = {}) {
  throw stop_error(category, std::move(code), std::move(message), std::move(context));
}

constexpr const char* input_names[4] = {"light", "dark", "flat", "dark_flat"};

struct slot {
  const char* name;
  const calibration_input_spec& spec;
  std::optional<master_role> expected;
};

std::array<slot, 4> slots_of(const calibration_recipe& r) {
  return {slot{"light", r.light, std::nullopt}, slot{"dark", r.dark, master_role::dark},
          slot{"flat", r.flat, master_role::flat}, slot{"dark_flat", r.dark_flat, master_role::dark_flat}};
}

const char* role_name(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  ASTRO_ASSERT(false);
  return "";
}

const char* additive_name(additive_state s) {
  switch (s) {
    case additive_state::bias_included: return "bias_included";
    case additive_state::bias_subtracted: return "bias_subtracted";
    case additive_state::unknown: return "unknown";
  }
  ASTRO_ASSERT(false);
  return "";
}

const char* scaling_name(scaling_state s) {
  switch (s) {
    case scaling_state::unscaled: return "unscaled";
    case scaling_state::scaled: return "scaled";
    case scaling_state::unknown: return "unknown";
  }
  ASTRO_ASSERT(false);
  return "";
}

const char* flat_name(flat_state s) {
  switch (s) {
    case flat_state::raw: return "raw";
    case flat_state::corrected_unnormalized: return "corrected_unnormalized";
    case flat_state::calibrated: return "calibrated";
    case flat_state::unknown: return "unknown";
  }
  ASTRO_ASSERT(false);
  return "";
}

const char* combine_name(combine_method m) {
  switch (m) {
    case combine_method::mean: return "mean";
    case combine_method::median: return "median";
    case combine_method::other: return "other";
  }
  ASTRO_ASSERT(false);
  return "";
}

const char* layout_name(plane_layout l) {
  switch (l) {
    case plane_layout::mono: return "mono";
    case plane_layout::cfa: return "cfa";
    case plane_layout::rgb: return "rgb";
    case plane_layout::multi_plane: return "multi_plane";
  }
  ASTRO_ASSERT(false);
  return "";
}

const char* pattern_name(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return "rggb";
    case cfa_pattern::bggr: return "bggr";
    case cfa_pattern::grbg: return "grbg";
    case cfa_pattern::gbrg: return "gbrg";
  }
  ASTRO_ASSERT(false);
  return "";
}

std::string cfa_text(const std::optional<cfa_layout>& c) {
  if (!c) return "none";
  return std::string(pattern_name(c->pattern)) + "@" + std::to_string(c->x_offset) + "," + std::to_string(c->y_offset);
}

// Etichette dei quattro canali nell'ordine delle posizioni (x, y) della cella 2x2; G1 precede G2 in ordine raster.
std::array<const char*, 4> channel_labels(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return {"R", "G1", "G2", "B"};
    case cfa_pattern::bggr: return {"B", "G1", "G2", "R"};
    case cfa_pattern::grbg: return {"G1", "R", "B", "G2"};
    case cfa_pattern::gbrg: return {"G1", "B", "R", "G2"};
  }
  ASTRO_ASSERT(false);
  return {"", "", "", ""};
}

// ---------------------------------------------------------------- validazione della ricetta

[[noreturn]] void bad_param(const char* name, const char* why) {
  stop(error_category::invalid_input, std::string("calib.param.") + name, "parametro assente, non finito o fuori dominio",
       {{"parametro", name}, {"motivo", why}});
}

void check_positive(const std::optional<double>& v, const char* name) {
  if (!v) bad_param(name, "assente");
  if (!std::isfinite(*v)) bad_param(name, "non finito");
  if (!(*v > 0)) bad_param(name, "deve essere > 0");
}

void check_nonnegative(const std::optional<double>& v, const char* name) {
  if (!v) bad_param(name, "assente");
  if (!std::isfinite(*v)) bad_param(name, "non finito");
  if (*v < 0) bad_param(name, "deve essere >= 0");
}

void validate_params(const calibration_recipe& r) {
  if (!r.normalization) bad_param("normalization", "assente");
  check_positive(r.q_min, "q_min");
  check_positive(r.q_rel_sigma_max, "q_rel_sigma_max");
  check_nonnegative(r.exposure_rel_tol, "exposure_rel_tol");
  check_nonnegative(r.temperature_abs_tol_c, "temperature_abs_tol_c");
  if (!r.temp_policy) bad_param("temperature_policy", "assente");
}

void validate_masters(const calibration_recipe& r) {
  for (const slot& s : slots_of(r)) {
    if (!s.expected) {
      if (s.spec.descriptor) {
        stop(error_category::invalid_input, "calib.role_slot", "il light non ha un descrittore di master",
             {{"slot", s.name}});
      }
      continue;
    }
    if (!s.spec.descriptor) {
      stop(error_category::metadata_missing, "calib.master_state_unknown", "stato del master non dichiarato",
           {{"slot", s.name}});
    }
    const master_descriptor& d = *s.spec.descriptor;
    if (d.role != *s.expected) {
      stop(error_category::invalid_input, "calib.role_slot", "master nello slot di un altro ruolo",
           {{"slot", s.name}, {"ruolo_dichiarato", role_name(d.role)}, {"ruolo_atteso", role_name(*s.expected)}});
    }
  }
  for (const slot& s : slots_of(r)) {
    if (!s.expected) continue;
    const master_descriptor& d = *s.spec.descriptor;
    const auto unknown = [&](const char* field) {
      stop(error_category::metadata_missing, "calib.master_state_unknown", "stato del master ignoto o non dichiarato",
           {{"slot", s.name}, {"campo", field}});
    };
    const auto unsupported = [&](const char* field, const char* value) {
      stop(error_category::unsupported, "calib.master_state_m2", "stato del master non previsto dalla ricetta M2",
           {{"slot", s.name}, {"campo", field}, {"valore", value}});
    };
    if (*s.expected == master_role::flat) {
      if (!d.flat || *d.flat == flat_state::unknown) unknown("flat");
    } else {
      if (!d.additive || *d.additive == additive_state::unknown) unknown("additive");
      if (!d.scaling || *d.scaling == scaling_state::unknown) unknown("scaling");
    }
    if (*s.expected == master_role::flat) {
      if (*d.flat != flat_state::raw) unsupported("flat", flat_name(*d.flat));
    } else {
      if (*d.additive != additive_state::bias_included) unsupported("additive", additive_name(*d.additive));
      if (*d.scaling != scaling_state::unscaled) unsupported("scaling", scaling_name(*d.scaling));
    }
  }
}

void validate_shared(const calibration_recipe& r) {
  const auto s = slots_of(r);
  for (std::size_t i = 0; i < s.size(); ++i) {
    for (std::size_t j = i + 1; j < s.size(); ++j) {
      if (s[i].spec.digest == s[j].spec.digest) {
        stop(error_category::unsupported, "calib.shared_input",
             "due input con lo stesso digest: la covarianza non e' modellata in M2",
             {{"primo", s[i].name}, {"secondo", s[j].name}, {"sha256", s[i].spec.digest.sha256}});
      }
    }
  }
}

[[noreturn]] void var_source_missing(const char* slot_name, const char* field) {
  stop(error_category::metadata_missing, std::string("var.source.") + field, "sorgente o parametro di varianza assente",
       {{"input", slot_name}, {"campo", field}});
}

[[noreturn]] void var_param_bad(const char* slot_name, const char* name, const char* why) {
  stop(error_category::invalid_input, std::string("var.param.") + name, "parametro di varianza non finito o fuori dominio",
       {{"input", slot_name}, {"parametro", name}, {"motivo", why}});
}

void validate_variance_specs(const calibration_recipe& r) {
  for (const slot& s : slots_of(r)) {
    const variance_spec& v = s.spec.variance;
    if (!v.source) var_source_missing(s.name, "variance_source");
    if (*v.source != variance_source::noise_model) continue;
    if (!v.read_noise_adu) var_source_missing(s.name, "read_noise_adu");
    if (!std::isfinite(*v.read_noise_adu)) var_param_bad(s.name, "read_noise_adu", "non finito");
    if (*v.read_noise_adu < 0) var_param_bad(s.name, "read_noise_adu", "deve essere >= 0");
    if (!s.expected) continue;
    const master_descriptor& d = *s.spec.descriptor;
    if (!d.combine_count) var_source_missing(s.name, "combine_count");
    if (!d.combine) var_source_missing(s.name, "combine_method");
    if (*d.combine_count < 1) var_param_bad(s.name, "combine_count", "deve essere >= 1");
    if (*d.combine != combine_method::mean) {
      stop(error_category::unsupported, "var.combine_method",
           "il modello di rumore richiede un master combinato con la media",
           {{"input", s.name}, {"combine_method", combine_name(*d.combine)}});
    }
  }
}

// ---------------------------------------------------------------- preflight sulle immagini

struct frame {
  const char* name;
  image* img;
};

// Campi di image_fields assenti dall'header e riempiti dalla ricetta (origine `declared` nel report).
struct field_origins {
  bool binning_x = false, binning_y = false, unit = false, lin = false;
};

field_origins apply_declared(image& im, const calibration_input_spec& spec, const char* name) {
  const frame_metadata& d = spec.declared;
  const auto apply = [&](auto& field, const auto& declared, const char* field_name) {
    if (!declared) return;
    if (declared->source.origin != metadata_origin::declared) {
      stop(error_category::invalid_input, "dag.metadata_not_declared", "la ricetta accetta solo valori dichiarati",
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

  const declared_image_fields& f = spec.declared_fields;
  const auto declare_plain = [&](auto& field, const auto& value, const char* field_name) {
    if (!value) return;
    if (field && *field != *value) {
      stop(error_category::incompatible, "metadata.declared_conflict", "valore dichiarato diverso da quello presente",
           {{"input", name}, {"campo", field_name}});
    }
    field = *value;
  };
  field_origins origins;
  origins.binning_x = f.binning_x && !im.geometry.binning_x;
  origins.binning_y = f.binning_y && !im.geometry.binning_y;
  origins.unit = f.unit && !im.unit;
  origins.lin = f.lin && !im.lin;
  declare_plain(im.geometry.binning_x, f.binning_x, "binning_x");
  declare_plain(im.geometry.binning_y, f.binning_y, "binning_y");
  declare_plain(im.unit, f.unit, "unit");
  declare_plain(im.lin, f.lin, "linearity");
  return origins;
}

template <class T>
bool all_finite_or_flagged(const image& im) {
  const T* s = im.data<T>();
  const std::size_t n = im.samples.size() / sizeof(T);
  for (std::size_t i = 0; i < n; ++i) {
    if (!std::isfinite(s[i]) && im.invalid.bits[i] == 0) return false;
  }
  return true;
}

void check_image(const frame& f) {
  if (f.img->variance && f.img->variance->samples.size() != f.img->samples.size()) {
    stop(error_category::invalid_input, "var.plane_geometry", "il piano di varianza non ha la geometria dell'immagine",
         {{"input", f.name}});
  }
  validate_image(*f.img);
  const bool ok = f.img->type == sample_type::f32 ? all_finite_or_flagged<float>(*f.img)
                                                  : all_finite_or_flagged<double>(*f.img);
  if (!ok) {
    stop(error_category::invalid_input, "calib.nonfinite_unflagged", "campione non finito senza bit di invalidita'",
         {{"input", f.name}});
  }
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// Ruolo indicato da un IMAGETYP riconosciuto; nullopt per un valore non riconosciuto.
std::optional<std::string> imagetyp_role(const std::string& value) {
  const std::string v = lower(value);
  if (v == "light" || v == "light frame") return "light";
  if (v == "dark" || v == "dark frame") return "dark";
  if (v == "flat" || v == "flat field" || v == "flat frame") return "flat";
  if (v == "bias" || v == "bias frame") return "bias";
  if (v == "dark flat" || v == "flat dark") return "dark_flat";
  return std::nullopt;
}

void check_imagetyp(const frame& f, const char* slot_role, std::vector<std::string>& warnings) {
  const auto& t = f.img->metadata.imagetyp;
  if (!t) return;
  const std::optional<std::string> role = imagetyp_role(t->value);
  if (!role) {
    warnings.push_back(std::string("imagetyp_unrecognized:") + f.name + ":" + t->value);
    return;
  }
  const std::string expected = slot_role;
  const bool ok = *role == expected || (expected == "dark_flat" && *role == "dark");
  if (!ok) {
    stop(error_category::incompatible, "calib.imagetyp_conflict", "IMAGETYP indica un altro ruolo",
         {{"input", f.name}, {"imagetyp", t->value}, {"ruolo_atteso", expected}});
  }
}

struct compat_state {
  json records = json::array();
  std::vector<std::string> hypotheses;
  std::vector<std::string> warnings;
};

json record_of(const char* pair, const char* field, json left, json right, const char* rule, json tolerance,
               const char* result = "pass") {
  json r = json::object();
  r.set("pair", json::string(pair));
  r.set("field", json::string(field));
  r.set("left", std::move(left));
  r.set("right", std::move(right));
  r.set("rule", json::string(rule));
  r.set("tolerance", std::move(tolerance));
  r.set("result", json::string(result));
  return r;
}

[[noreturn]] void stop_mismatch(const char* pair, const char* field, const std::string& left, const std::string& right,
                           const std::string& tolerance) {
  stop(error_category::incompatible, std::string("calib.mismatch.") + field, "input incompatibili",
       {{"coppia", pair}, {"campo", field}, {"sinistra", left}, {"destra", right}, {"tolleranza", tolerance}});
}

std::string num_text(double v) {
  char b[40];
  std::snprintf(b, sizeof b, "%.17g", v);
  return b;
}

template <class T>
const T& need(const std::optional<metadata_value<T>>& field, const char* pair, const char* input, const char* name) {
  if (!field) {
    stop(error_category::metadata_missing, std::string("calib.metadata.") + name,
         "metadato richiesto assente (dichiarabile nella ricetta)", {{"coppia", pair}, {"input", input}, {"campo", name}});
  }
  return field->value;
}

const std::uint32_t& need_binning(const std::optional<std::uint32_t>& v, const char* pair, const char* input,
                                  const char* name) {
  if (!v) {
    stop(error_category::metadata_missing, std::string("calib.metadata.") + name,
         "metadato richiesto assente (dichiarabile nella ricetta)", {{"coppia", pair}, {"input", input}, {"campo", name}});
  }
  return *v;
}

void exact_double(compat_state& cs, const char* pair, const frame& a, const frame& b, const char* name,
                  const std::optional<metadata_value<double>>& fa, const std::optional<metadata_value<double>>& fb) {
  const double x = need(fa, pair, a.name, name), y = need(fb, pair, b.name, name);
  if (x != y) stop_mismatch(pair, name, num_text(x), num_text(y), "esatta");
  cs.records.push(record_of(pair, name, json::number(x), json::number(y), "exact", json::null()));
}

void compare_pair(compat_state& cs, const char* pair, const frame& a, const frame& b, bool matched,
                  const calibration_recipe& r) {
  const image& ia = *a.img;
  const image& ib = *b.img;
  if (ia.geometry.width != ib.geometry.width) {
    stop_mismatch(pair, "width", std::to_string(ia.geometry.width), std::to_string(ib.geometry.width), "esatta");
  }
  cs.records.push(record_of(pair, "width", json::uinteger(ia.geometry.width), json::uinteger(ib.geometry.width),
                            "exact", json::null()));
  if (ia.geometry.height != ib.geometry.height) {
    stop_mismatch(pair, "height", std::to_string(ia.geometry.height), std::to_string(ib.geometry.height), "esatta");
  }
  cs.records.push(record_of(pair, "height", json::uinteger(ia.geometry.height), json::uinteger(ib.geometry.height),
                            "exact", json::null()));
  const std::pair<const char*, std::pair<const std::optional<std::uint32_t>&, const std::optional<std::uint32_t>&>> bins[2] = {
      {"binning_x", {ia.geometry.binning_x, ib.geometry.binning_x}},
      {"binning_y", {ia.geometry.binning_y, ib.geometry.binning_y}}};
  for (const auto& [name, values] : bins) {
    const std::uint32_t x = need_binning(values.first, pair, a.name, name);
    const std::uint32_t y = need_binning(values.second, pair, b.name, name);
    if (x != y) stop_mismatch(pair, name, std::to_string(x), std::to_string(y), "esatta");
    cs.records.push(record_of(pair, name, json::uinteger(x), json::uinteger(y), "exact", json::null()));
  }
  if (!(ia.cfa == ib.cfa)) stop_mismatch(pair, "cfa", cfa_text(ia.cfa), cfa_text(ib.cfa), "esatta");
  cs.records.push(record_of(pair, "cfa", json::string(cfa_text(ia.cfa)), json::string(cfa_text(ib.cfa)), "exact",
                            json::null()));

  const std::string ca = need(ia.metadata.camera, pair, a.name, "camera");
  const std::string cb = need(ib.metadata.camera, pair, b.name, "camera");
  if (ca != cb) stop_mismatch(pair, "camera", ca, cb, "esatta");
  cs.records.push(record_of(pair, "camera", json::string(ca), json::string(cb), "exact", json::null()));

  if (matched) {
    exact_double(cs, pair, a, b, "gain", ia.metadata.gain, ib.metadata.gain);
    exact_double(cs, pair, a, b, "offset_adu", ia.metadata.offset_adu, ib.metadata.offset_adu);
    const double ea = need(ia.metadata.exposure_s, pair, a.name, "exposure_s");
    const double eb = need(ib.metadata.exposure_s, pair, b.name, "exposure_s");
    const double tol = *r.exposure_rel_tol;
    if (!(std::fabs(ea - eb) <= tol * std::max(std::fabs(ea), std::fabs(eb)))) {
      stop_mismatch(pair, "exposure_s", num_text(ea), num_text(eb), "relativa " + num_text(tol));
    }
    cs.records.push(record_of(pair, "exposure_s", json::number(ea), json::number(eb), "relative_to_max",
                              json::number(tol)));
    if (*r.temp_policy == temperature_policy::compare) {
      const double ta = need(ia.metadata.temperature_c, pair, a.name, "temperature_c");
      const double tb = need(ib.metadata.temperature_c, pair, b.name, "temperature_c");
      const double tt = *r.temperature_abs_tol_c;
      if (!(std::fabs(ta - tb) <= tt)) stop_mismatch(pair, "temperature_c", num_text(ta), num_text(tb), "assoluta " + num_text(tt));
      cs.records.push(record_of(pair, "temperature_c", json::number(ta), json::number(tb), "absolute",
                                json::number(tt)));
    } else {
      cs.warnings.push_back(std::string("temperature_comparison_skipped:") + pair);
      cs.records.push(record_of(pair, "temperature_c", json::null(), json::null(), "skipped_not_applicable_declared",
                                json::null(), "skipped"));
    }
  } else {
    const std::string fa = need(ia.metadata.filter, pair, a.name, "filter");
    const std::string fb = need(ib.metadata.filter, pair, b.name, "filter");
    if (fa != fb) stop_mismatch(pair, "filter", fa, fb, "esatta");
    cs.records.push(record_of(pair, "filter", json::string(fa), json::string(fb), "exact", json::null()));
  }

  const auto optional_field = [&](const char* name, auto fa, auto fb, auto to_text, auto to_json) {
    if (!fa || !fb) {
      std::string absent;
      if (!fa) absent += a.name;
      if (!fb) absent += std::string(absent.empty() ? "" : ",") + b.name;
      cs.hypotheses.push_back(std::string("not_verified:") + pair + ":" + name + ":absent_in=" + absent);
      return;
    }
    if (!(fa->value == fb->value)) stop_mismatch(pair, name, to_text(fa->value), to_text(fb->value), "esatta");
    cs.records.push(record_of(pair, name, to_json(fa->value), to_json(fb->value), "exact", json::null()));
  };
  const auto int_text = [](std::int64_t v) { return std::to_string(v); };
  const auto int_json = [](std::int64_t v) { return json::integer(v); };
  const auto str_text = [](const std::string& v) { return v; };
  const auto str_json = [](const std::string& v) { return json::string(v); };
  optional_field("roi_x", ia.metadata.roi_x, ib.metadata.roi_x, int_text, int_json);
  optional_field("roi_y", ia.metadata.roi_y, ib.metadata.roi_y, int_text, int_json);
  optional_field("readout_mode", ia.metadata.readout_mode, ib.metadata.readout_mode, str_text, str_json);
  optional_field("row_order", ia.metadata.row_order, ib.metadata.row_order, str_text, str_json);
}

// ---------------------------------------------------------------- vista e varianza degli input

input_view make_view(const frame& f, const calibration_input_spec& spec) {
  const image& im = *f.img;
  input_view v;
  v.samples = im.samples.data();
  v.mask = im.invalid.bits.data();
  v.f32 = im.type == sample_type::f32;
  if (im.metadata.saturation_adu) {
    v.has_saturation = true;
    v.saturation = im.metadata.saturation_adu->value;
  }
  const variance_spec& vs = spec.variance;
  if (*vs.source == variance_source::plane) {
    if (!im.variance) {
      stop(error_category::metadata_missing, "var.source.variance_plane", "HDU VARIANCE assente nell'input",
           {{"input", f.name}});
    }
    if (im.variance->samples.size() != im.samples.size()) {
      stop(error_category::invalid_input, "var.plane_geometry", "il piano di varianza non ha la geometria dell'immagine",
           {{"input", f.name}});
    }
    v.plane = true;
    v.variance_plane = im.variance->samples.data();
    const std::size_t n = im.samples.size() / sample_size(im.type);
    for (std::size_t i = 0; i < n; ++i) {
      if (v.mask[i] != 0) continue;
      const double var = v.f32 ? static_cast<double>(static_cast<const float*>(v.variance_plane)[i])
                               : static_cast<const double*>(v.variance_plane)[i];
      if (!std::isfinite(var) || var < 0) {
        stop(error_category::invalid_input, "var.plane_values", "varianza non finita o negativa su un pixel valido",
             {{"input", f.name}, {"indice", std::to_string(i)}});
      }
    }
  } else {
    v.plane = false;
    const char* who = f.name;
    if (!im.metadata.conversion_gain_e_per_adu) var_source_missing(who, "conversion_gain_e_per_adu");
    const double g = im.metadata.conversion_gain_e_per_adu->value;
    const double off = im.metadata.offset_adu->value;
    if (!std::isfinite(g)) var_param_bad(who, "conversion_gain_e_per_adu", "non finito");
    if (!(g > 0)) var_param_bad(who, "conversion_gain_e_per_adu", "deve essere > 0");
    if (!std::isfinite(off)) var_param_bad(who, "offset_adu", "non finito");
    v.gain = g;
    v.offset = off;
    v.read_noise_sq = *vs.read_noise_adu * *vs.read_noise_adu;
    if (!calib_detail::mul_ok(*vs.read_noise_adu, *vs.read_noise_adu, v.read_noise_sq) || !std::isfinite(v.read_noise_sq)) {
      var_param_bad(who, "read_noise_adu", "il quadrato non e' rappresentabile in modo normale");
    }
    v.n = spec.descriptor ? static_cast<double>(*spec.descriptor->combine_count) : 1.0;
  }
  return v;
}

double median_of(std::vector<double>& v) {
  const std::size_t n = v.size();
  const std::size_t mid = n / 2;
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
  const double hi = v[mid];
  if (n % 2 == 1) return hi;
  const double lo = *std::max_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid));
  return (lo + hi) * 0.5;
}

json metadata_report(const image& im) {
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

json input_report(const slot& s, const image& im, const input_view& view, const field_origins& origins) {
  json o = json::object();
  o.set("digest_sha256", json::string(s.spec.digest.sha256));
  o.set("size_bytes", json::uinteger(s.spec.digest.size_bytes));
  o.set("width", json::uinteger(im.geometry.width));
  o.set("height", json::uinteger(im.geometry.height));
  o.set("layout", json::string(layout_name(im.layout)));
  o.set("cfa", json::string(cfa_text(im.cfa)));
  o.set("sample_type", json::string(im.type == sample_type::f32 ? "f32" : "f64"));
  {
    const auto field = [](json value, bool declared) {
      json e = json::object();
      e.set("value", std::move(value));
      e.set("origin", json::string(declared ? "declared" : "header"));
      return e;
    };
    const auto opt_u = [](const std::optional<std::uint32_t>& v) { return v ? json::uinteger(*v) : json::null(); };
    json f = json::object();
    f.set("binning_x", field(opt_u(im.geometry.binning_x), origins.binning_x));
    f.set("binning_y", field(opt_u(im.geometry.binning_y), origins.binning_y));
    f.set("unit", field(json::string(*im.unit == pixel_unit::adu ? "adu" : *im.unit == pixel_unit::electron ? "electron" : "dimensionless"),
                        origins.unit));
    f.set("linearity", field(json::string(*im.lin == linearity::linear ? "linear" : "nonlinear"), origins.lin));
    o.set("image_fields", std::move(f));
  }
  o.set("metadata", metadata_report(im));
  if (s.spec.descriptor) {
    const master_descriptor& d = *s.spec.descriptor;
    json st = json::object();
    st.set("role", json::string(role_name(d.role)));
    st.set("state_source", json::string("declared"));
    if (d.additive) st.set("additive", json::string(additive_name(*d.additive)));
    if (d.scaling) st.set("scaling", json::string(scaling_name(*d.scaling)));
    if (d.flat) st.set("flat", json::string(flat_name(*d.flat)));
    if (d.combine_count) st.set("combine_count", json::uinteger(*d.combine_count));
    if (d.combine) st.set("combine_method", json::string(combine_name(*d.combine)));
    o.set("master_state", std::move(st));
  }
  json var = json::object();
  var.set("variance_source", json::string(view.plane ? "plane" : "noise_model"));
  if (!view.plane) {
    var.set("conversion_gain_e_per_adu", json::number(view.gain));
    var.set("read_noise_adu", json::number(*s.spec.variance.read_noise_adu));
    var.set("offset_adu", json::number(view.offset));
    var.set("n", json::number(view.n));
  }
  o.set("variance", std::move(var));
  o.set("saturation_declared", json::boolean(view.has_saturation));
  return o;
}

}  // namespace

// ---------------------------------------------------------------- interfaccia

void validate_calibration_recipe(const calibration_recipe& recipe) {
  validate_params(recipe);
  validate_masters(recipe);
  validate_shared(recipe);
  validate_variance_specs(recipe);
}

prepared_calibration::prepared_calibration(std::unique_ptr<impl> i) : impl_(std::move(i)) {}
prepared_calibration::prepared_calibration(prepared_calibration&&) noexcept = default;
prepared_calibration& prepared_calibration::operator=(prepared_calibration&&) noexcept = default;
prepared_calibration::~prepared_calibration() = default;

std::uint32_t prepared_calibration::width() const noexcept { return impl_->w; }
std::uint32_t prepared_calibration::height() const noexcept { return impl_->h; }
const std::vector<channel_normalization>& prepared_calibration::normalization() const noexcept { return impl_->channels; }
const std::vector<std::string>& prepared_calibration::warnings() const noexcept { return impl_->warnings; }

std::uint64_t prepared_calibration::row_scratch_bytes() const noexcept {
  return std::uint64_t{impl_->w} * (sizeof(double) * 2 + sizeof(std::uint16_t));
}

prepared_calibration prepare_calibration(const calibration_recipe& recipe, calibration_images images) {
  validate_calibration_recipe(recipe);
  auto p = std::make_unique<prepared_calibration::impl>();
  p->recipe = recipe;
  p->images = std::move(images);
  p->q_min = *recipe.q_min;
  p->q_rel_sigma_max = *recipe.q_rel_sigma_max;
  calibration_images& im = p->images;
  const std::array<frame, 4> frames = {frame{"light", &im.light}, frame{"dark", &im.dark},
                                       frame{"flat", &im.flat}, frame{"dark_flat", &im.dark_flat}};
  const auto specs = slots_of(recipe);

  for (std::size_t i = 0; i < frames.size(); ++i) {
    const frame& f = frames[i];
    if (f.img->layout != plane_layout::mono && f.img->layout != plane_layout::cfa) {
      stop(error_category::unsupported, "calib.plane_layout", "la ricetta M2 accetta solo mono e CFA",
           {{"input", f.name}, {"layout", layout_name(f.img->layout)}});
    }
  }
  std::array<field_origins, 4> origins;
  for (std::size_t i = 0; i < frames.size(); ++i) origins[i] = apply_declared(*frames[i].img, specs[i].spec, frames[i].name);
  for (const frame& f : frames) check_image(f);

  for (const frame& f : frames) {
    const image& x = *f.img;
    if (!x.lin) {
      stop(error_category::metadata_missing, "calib.metadata.linearity", "linearita' non dichiarata",
           {{"input", f.name}});
    }
    if (*x.lin != linearity::linear) {
      stop(error_category::unsupported, "calib.linearity", "la ricetta M2 richiede dati lineari", {{"input", f.name}});
    }
    if (!x.unit) {
      stop(error_category::metadata_missing, "calib.metadata.unit", "unita' non dichiarata", {{"input", f.name}});
    }
    if (*x.unit != pixel_unit::adu) {
      stop(error_category::unsupported, "calib.unit", "la ricetta M2 richiede ADU", {{"input", f.name}});
    }
    if (x.metadata.pedestal_adu && x.metadata.pedestal_adu->value != 0.0) {
      stop(error_category::unsupported, "calib.pedestal", "pedestal diverso da zero non supportato",
           {{"input", f.name}, {"pedestal_adu", num_text(x.metadata.pedestal_adu->value)}});
    }
  }
  if (*recipe.normalization == normalization_mode::cfa_channel_median && im.light.layout != plane_layout::cfa) {
    stop(error_category::invalid_input, "calib.normalization_requires_cfa",
         "cfa_channel_median richiede un'immagine CFA", {{"layout", layout_name(im.light.layout)}});
  }

  std::vector<std::string> warnings;
  check_imagetyp(frames[0], "light", warnings);
  check_imagetyp(frames[1], "dark", warnings);
  check_imagetyp(frames[2], "flat", warnings);
  check_imagetyp(frames[3], "dark_flat", warnings);

  compat_state cs;
  compare_pair(cs, "light-dark", frames[0], frames[1], true, recipe);
  compare_pair(cs, "flat-dark_flat", frames[2], frames[3], true, recipe);
  compare_pair(cs, "light-flat", frames[0], frames[2], false, recipe);
  for (std::string& w : cs.warnings) warnings.push_back(std::move(w));

  p->light = make_view(frames[0], specs[0].spec);
  p->dark = make_view(frames[1], specs[1].spec);
  p->flat = make_view(frames[2], specs[2].spec);
  p->dark_flat = make_view(frames[3], specs[3].spec);
  for (const input_view* v : {&p->light, &p->flat}) {
    if (!v->has_saturation) warnings.push_back(std::string("saturation_not_evaluated:") + (v == &p->light ? "light" : "flat"));
  }

  p->w = im.light.geometry.width;
  p->h = im.light.geometry.height;
  p->per_channel = *recipe.normalization == normalization_mode::cfa_channel_median;
  if (im.light.cfa) {
    p->cfa_x = im.light.cfa->x_offset;
    p->cfa_y = im.light.cfa->y_offset;
  }
  const std::size_t channels = p->per_channel ? 4 : 1;
  std::array<const char*, 4> labels = {"all", "", "", ""};
  if (p->per_channel) labels = channel_labels(im.light.cfa->pattern);

  // Normalizzazione N: mediana di Fc sui pixel validi per N, prima di ogni tile.
  std::vector<std::vector<double>> fc(channels);
  std::vector<std::uint64_t> nonpositive(channels, 0);
  std::vector<double> mean(channels, 0.0), m2(channels, 0.0);
  const auto valid_for_n = [&](std::size_t i) {
    if ((p->flat.mask[i] | p->dark_flat.mask[i]) != 0) return false;
    if (p->flat.has_saturation && p->flat.value(i) >= p->flat.saturation) return false;
    return std::isfinite(p->flat.value(i) - p->dark_flat.value(i));
  };
  for (std::uint32_t y = 0; y < p->h; ++y) {
    for (std::uint32_t x = 0; x < p->w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * p->w + x;
      if (!valid_for_n(i)) continue;
      const std::size_t c = p->channel_of(x, y);
      const double v = p->flat.value(i) - p->dark_flat.value(i);
      if (v <= 0) ++nonpositive[c];
      fc[c].push_back(v);
      const double delta = v - mean[c];
      mean[c] += delta / static_cast<double>(fc[c].size());
      m2[c] += delta * (v - mean[c]);
    }
  }
  p->channels.resize(channels);
  for (std::size_t c = 0; c < channels; ++c) {
    channel_normalization& ch = p->channels[c];
    ch.label = labels[c];
    ch.n_valid = fc[c].size();
    ch.fc_nonpositive = nonpositive[c];
    if (fc[c].empty()) {
      stop(error_category::numeric, "calib.normalization_invalid", "nessun pixel valido per la normalizzazione",
           {{"canale", ch.label}});
    }
    if (ch.n_valid >= 2 && mean[c] != 0.0) {
      ch.fc_cv = std::sqrt(m2[c] / static_cast<double>(ch.n_valid - 1)) / std::fabs(mean[c]);
      if (!std::isfinite(*ch.fc_cv)) ch.fc_cv.reset();  // non calcolabile: nel report resta null e l'ipotesi iid non e' verificata
    }
    ch.iid_not_verified = !ch.fc_cv || !(*ch.fc_cv <= calib_detail::iid_cv_threshold);
    ch.n = median_of(fc[c]);
    if (!std::isfinite(ch.n) || !(ch.n > 0)) {
      stop(error_category::numeric, "calib.normalization_invalid", "normalizzazione non positiva o non finita",
           {{"canale", ch.label}, {"N", num_text(ch.n)}});
    }
    p->norm[c] = ch.n;
    p->norm_sq[c] = ch.n * ch.n;
    if (!calib_detail::is_normal(p->norm_sq[c])) {
      stop(error_category::numeric, "calib.variance_overflow", "N al quadrato non finito o nullo: Var(Q) non calcolabile",
           {{"canale", ch.label}, {"N", num_text(ch.n)}});
    }
    std::vector<double>().swap(fc[c]);
  }
  std::vector<std::vector<double>> vfc(channels);
  for (std::uint32_t y = 0; y < p->h; ++y) {
    for (std::uint32_t x = 0; x < p->w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * p->w + x;
      if (!valid_for_n(i)) continue;
      const double f = p->flat.value(i), d = p->dark_flat.value(i);
      const double term = p->flat.variance(i, f) + p->dark_flat.variance(i, d);
      if (!std::isfinite(term)) {
        stop(error_category::numeric, "calib.variance_overflow", "varianza di Fc non finita: mediana non calcolabile",
             {{"x", std::to_string(x)}, {"y", std::to_string(y)}});
      }
      vfc[p->channel_of(x, y)].push_back(term);
    }
  }
  constexpr double half_pi = 1.5707963267948966;
  for (std::size_t c = 0; c < channels; ++c) {
    channel_normalization& ch = p->channels[c];
    const double median_var = median_of(vfc[c]);
    const double denominator = static_cast<double>(ch.n_valid) * p->norm_sq[c];
    ch.rel_var_estimate = half_pi * median_var / denominator;
    if (!std::isfinite(median_var) || !calib_detail::is_normal(denominator) ||
        !calib_detail::div_ok(median_var, ch.rel_var_estimate)) {
      stop(error_category::numeric, "calib.variance_overflow", "stima della varianza di N non calcolabile in modo finito",
           {{"canale", ch.label}});
    }
    std::vector<double>().swap(vfc[c]);
  }

  // Uscita: stessa geometria e tipo del light, maschera azzerata, varianza dello stesso tipo.
  p->out = make_image(im.light.geometry, im.light.type, im.light.layout, im.light.cfa);
  p->out.unit = im.light.unit;
  p->out.lin = im.light.lin;
  p->out.metadata = im.light.metadata;
  p->out.header_cards = im.light.header_cards;
  p->out.uninterpreted_keywords = im.light.uninterpreted_keywords;
  p->out.variance = variance_plane{aligned_buffer(p->out.samples.size())};

  p->warnings = std::move(warnings);

  json inputs = json::object();
  const std::array<const input_view*, 4> views = {&p->light, &p->dark, &p->flat, &p->dark_flat};
  for (std::size_t i = 0; i < frames.size(); ++i) {
    inputs.set(frames[i].name, input_report(specs[i], *frames[i].img, *views[i], origins[i]));
  }
  json recipe_json = json::object();
  recipe_json.set("name", json::string("dark_matched_flat_dark_flat"));
  recipe_json.set("normalization", json::string(p->per_channel ? "cfa_channel_median" : "global_median"));
  recipe_json.set("q_min", json::number(p->q_min));
  recipe_json.set("q_rel_sigma_max", json::number(p->q_rel_sigma_max));
  recipe_json.set("exposure_rel_tol", json::number(*recipe.exposure_rel_tol));
  recipe_json.set("temperature_abs_tol_c", json::number(*recipe.temperature_abs_tol_c));
  recipe_json.set("temperature_policy", json::string(*recipe.temp_policy == temperature_policy::compare
                                                         ? "compare" : "not_applicable_declared"));
  p->report.set("recipe", std::move(recipe_json));
  p->report.set("inputs", std::move(inputs));
  p->report.set("compatibility", std::move(cs.records));

  json hyp = json::array();
  for (const char* h : {"additive_term_stable_between_light_and_dark", "flat_multiplicative", "no_dark_scaling",
                        "amp_glow_not_modeled", "masters_share_no_source_frames_not_verified",
                        "pixel_covariance_zero", "normalization_treated_as_constant"}) {
    hyp.push(json::string(h));
  }
  for (const std::string& h : cs.hypotheses) hyp.push(json::string(h));
  p->report.set("assumptions", std::move(hyp));
  return prepared_calibration(std::move(p));
}

// ---------------------------------------------------------------- calcolo per righe

namespace {

template <class T>
bool convert_finite(double v, T& out) {
  if (!std::isfinite(v)) return false;
  if constexpr (std::is_same_v<T, float>) {
    if (std::fabs(v) > static_cast<double>(FLT_MAX)) return false;
  }
  out = static_cast<T>(v);
  // Valore non nullo che nel tipo di uscita diventa zero o subnormale: non rappresentabile in modo normale.
  return v == 0 || std::fpclassify(out) == FP_NORMAL;
}

template <class T>
void rows_kernel(prepared_calibration::impl& p, std::uint32_t y0, std::uint32_t y1, double* qrow, double* varq,
                 std::uint16_t* qbits) {
  const std::uint32_t w = p.w;
  T* out = p.out.data<T>();
  T* outv = p.out.variance_data<T>();
  std::uint16_t* out_mask = p.out.invalid.bits.data();
  const input_view &L = p.light, &D = p.dark, &F = p.flat, &DF = p.dark_flat;
  constexpr T nan = std::numeric_limits<T>::quiet_NaN();
  for (std::uint32_t y = y0; y < y1; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * w + x;
      qbits[x] = 0;
      qrow[x] = std::numeric_limits<double>::quiet_NaN();
      varq[x] = std::numeric_limits<double>::quiet_NaN();
      const bool f_saturated = F.has_saturation && F.value(i) >= F.saturation;
      if ((F.mask[i] | DF.mask[i]) != 0 || f_saturated) continue;
      const double f = F.value(i), df = DF.value(i);
      const double n = p.norm[p.channel_of(x, y)];
      const double fc = f - df;
      const double q = fc / n;
      qrow[x] = q;
      if (!calib_detail::div_ok(fc, q) && std::isfinite(q)) {
        qbits[x] = invalid_reason::result_nonfinite;  // Q zero o subnormale da Fc non nullo
      } else if (!std::isfinite(q)) {
        qbits[x] = invalid_reason::q_nonfinite;
      } else if (q <= 0) {
        qbits[x] = invalid_reason::q_nonpositive;
      } else {
        const double vf = F.variance(i, f), vdf = DF.variance(i, df);
        const double vfc = vf + vdf;
        const double vq = vfc / p.norm_sq[p.channel_of(x, y)];
        varq[x] = vq;
        if (std::isnan(vf) || std::isnan(vdf) || !calib_detail::add_ok(vf, vdf, vfc) || !calib_detail::div_ok(vfc, vq)) {
          qbits[x] = invalid_reason::result_nonfinite;  // passo del modello di varianza non rappresentabile
          continue;
        }
        if (q < p.q_min) qbits[x] |= invalid_reason::q_below_min;
        if (!std::isfinite(vq) || std::sqrt(vq) / q > p.q_rel_sigma_max) qbits[x] |= invalid_reason::q_unreliable;
      }
    }
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * w + x;
      const std::uint16_t master_bits = static_cast<std::uint16_t>(D.mask[i] | F.mask[i] | DF.mask[i]);
      std::uint16_t bits = static_cast<std::uint16_t>(L.mask[i] | master_bits | qbits[x]);
      if (master_bits != 0) bits |= invalid_reason::master_invalid;
      if (L.has_saturation && L.value(i) >= L.saturation) bits |= invalid_reason::saturated;
      if (F.has_saturation && F.value(i) >= F.saturation) bits |= invalid_reason::saturated;
      if (bits == 0) {
        const double l = L.value(i), d = D.value(i);
        const double lc = l - d;
        const double q = qrow[x];
        const double c = lc / q;
        const double vl = L.variance(i, l), vd = D.variance(i, d);
        const double vlc = vl + vd;
        const double q2 = q * q;
        const double q4 = q2 * q2;
        const double lc2 = lc * lc;
        const double term_q = lc2 * varq[x];
        const double t1 = vlc / q2;
        const double t2 = term_q / q4;
        const double var_c = t1 + t2;
        // Ogni prodotto, quoziente e somma deve essere finito e, con operandi non nulli, normale (ne' zero ne'
        // subnormale per underflow, ne' infinito): altrimenti il pixel e' result_nonfinite.
        using namespace calib_detail;
        const bool intermediates_ok = !std::isnan(vl) && !std::isnan(vd) && add_ok(vl, vd, vlc) && is_finite_ok(vlc) &&
                                      div_ok(lc, c) && mul_ok(q, q, q2) && mul_ok(q2, q2, q4) && q2 > 0 && q4 > 0 &&
                                      mul_ok(lc, lc, lc2) && mul_ok(lc2, varq[x], term_q) && div_ok(vlc, t1) &&
                                      div_ok(term_q, t2) && add_ok(t1, t2, var_c);
        T cs{}, vs{};
        if (intermediates_ok && convert_finite<T>(c, cs) && convert_finite<T>(var_c, vs)) {
          out[i] = cs;
          outv[i] = vs;
          continue;
        }
        bits = invalid_reason::result_nonfinite;
      }
      out[i] = nan;
      outv[i] = nan;
      out_mask[i] = bits;
    }
  }
}

}  // namespace

void prepared_calibration::calibrate_rows(std::uint32_t row_begin, std::uint32_t row_end, void* scratch) {
  ASTRO_ASSERT(row_begin <= row_end && row_end <= impl_->h);
  auto* qrow = static_cast<double*>(scratch);
  double* varq = qrow + impl_->w;
  auto* qbits = reinterpret_cast<std::uint16_t*>(varq + impl_->w);
  if (impl_->out.type == sample_type::f32) rows_kernel<float>(*impl_, row_begin, row_end, qrow, varq, qbits);
  else rows_kernel<double>(*impl_, row_begin, row_end, qrow, varq, qbits);
}

std::string prepared_calibration::rows_digest(std::uint32_t row_begin, std::uint32_t row_end) const {
  ASTRO_ASSERT(row_begin <= row_end && row_end <= impl_->h);
  const std::size_t elem = sample_size(impl_->out.type);
  const std::size_t first = static_cast<std::size_t>(row_begin) * impl_->w;
  const std::size_t count = static_cast<std::size_t>(row_end - row_begin) * impl_->w;
  sha256_stream s;
  const auto bytes = [](const void* p, std::size_t n) {
    return std::span<const std::uint8_t>(static_cast<const std::uint8_t*>(p), n);
  };
  s.update(bytes(impl_->out.samples.data() + first * elem, count * elem));
  s.update(bytes(impl_->out.invalid.bits.data() + first, count * sizeof(std::uint16_t)));
  s.update(bytes(impl_->out.variance->samples.data() + first * elem, count * elem));
  return s.finish().sha256;
}

calibration_result calibrate(const calibration_recipe& recipe, calibration_images images) {
  prepared_calibration prepared = prepare_calibration(recipe, std::move(images));
  std::vector<std::byte> scratch(prepared.row_scratch_bytes());
  prepared.calibrate_rows(0, prepared.height(), scratch.data());
  return finish_calibration(std::move(prepared));
}

}  // namespace astro
