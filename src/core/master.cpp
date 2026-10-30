#include "astro/master.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>

#include "astro/error.hpp"
#include "astro/operations.hpp"
#include "astro/scheduler.hpp"
#include "calibration_internal.hpp"
#include "cpu_kernels.hpp"

namespace astro {

using namespace calib_detail;

namespace {

[[noreturn]] void stop(error_category category, std::string code, std::string message,
                       stop_error::context_list context = {}) {
  throw stop_error(category, std::move(code), std::move(message), std::move(context));
}

[[noreturn]] void bad_param(const char* name, const char* why) {
  stop(error_category::invalid_input, std::string("master.param.") + name, "parametro assente, non finito o fuori dominio",
       {{"parametro", name}, {"motivo", why}});
}

[[noreturn]] void outside_variance_domain(const char* name, const char* value) {
  stop(error_category::unsupported, "master.variance_domain",
       "metodo fuori dal dominio della varianza derivata in C-02.a (appartiene a F-CAL-01.b)",
       {{"parametro", name}, {"valore", value}});
}

const char* flat_norm_name(flat_normalization f) { return f == flat_normalization::none ? "none" : "median_ratio"; }
const char* type_name(sample_type t) { return t == sample_type::f32 ? "f32" : "f64"; }
const char* policy_name(temperature_policy p) {
  return p == temperature_policy::compare ? "compare" : "not_applicable_declared";
}

constexpr std::array<const char*, 9> bit_names = {"source_nonfinite", "source_flagged", "saturated",
                                                  "master_invalid",   "q_nonpositive",  "q_nonfinite",
                                                  "q_below_min",      "q_unreliable",   "result_nonfinite"};

std::string frame_name(std::size_t k) {
  char b[32];
  std::snprintf(b, sizeof b, "frame_%03zu", k);
  return b;
}

// Ordine canonico (C-02.a §8): digest SHA-256 crescente; esadecimale minuscolo, quindi ordine dei byte.
std::vector<std::size_t> canonical_order(const master_recipe& r) {
  std::vector<std::size_t> order(r.frames.size());
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return r.frames[a].digest.sha256 < r.frames[b].digest.sha256; });
  return order;
}

void check_finite(const std::optional<double>& v, const char* name) {
  if (!v) bad_param(name, "assente");
  if (!std::isfinite(*v)) bad_param(name, "non finito");
}

void validate_params(const master_recipe& r) {
  if (!r.combine) bad_param("combine", "assente");
  if (*r.combine != combine_method::mean) outside_variance_domain("combine", combine_name(*r.combine));
  if (!r.rejection) bad_param("rejection", "assente");
  if (*r.rejection != master_rejection::none) outside_variance_domain("rejection", rejection_name(*r.rejection));
  if (r.role == master_role::flat) {
    if (!r.flat_norm) bad_param("flat_normalization", "assente");
    if (*r.flat_norm != flat_normalization::none) outside_variance_domain("flat_normalization", flat_norm_name(*r.flat_norm));
    check_finite(r.flat_level_rel_tol, "flat_level_rel_tol");
    if (!(*r.flat_level_rel_tol > 0 && *r.flat_level_rel_tol < 1)) bad_param("flat_level_rel_tol", "deve stare in (0, 1)");
    check_finite(r.flat_min_signal_adu, "flat_min_signal_adu");
    if (!(*r.flat_min_signal_adu > 0)) bad_param("flat_min_signal_adu", "deve essere > 0");
  } else {
    if (r.flat_norm) bad_param("flat_normalization", "solo per il ruolo flat");
    if (r.flat_level_rel_tol) bad_param("flat_level_rel_tol", "solo per il ruolo flat");
    if (r.flat_min_signal_adu) bad_param("flat_min_signal_adu", "solo per il ruolo flat");
  }
  if (r.saturation_adu) {
    if (!std::isfinite(*r.saturation_adu)) bad_param("saturation_adu", "non finito");
    check_finite(r.saturated_fraction_max, "saturated_fraction_max");
    if (!(*r.saturated_fraction_max >= 0 && *r.saturated_fraction_max <= 1)) {
      bad_param("saturated_fraction_max", "deve stare in [0, 1]");
    }
  } else if (r.saturated_fraction_max) {
    bad_param("saturated_fraction_max", "dichiarato senza saturation_adu");
  }
  check_finite(r.exposure_rel_tol, "exposure_rel_tol");
  if (*r.exposure_rel_tol < 0) bad_param("exposure_rel_tol", "deve essere >= 0");
  check_finite(r.temperature_abs_tol_c, "temperature_abs_tol_c");
  if (*r.temperature_abs_tol_c < 0) bad_param("temperature_abs_tol_c", "deve essere >= 0");
  if (!r.temp_policy) bad_param("temperature_policy", "assente");
  if (!r.simd) bad_param("simd", "assente");
}

json fields_json(const declared_image_fields& f) {
  json o = json::object();
  o.set("binning_x", f.binning_x ? json::uinteger(*f.binning_x) : json::null());
  o.set("binning_y", f.binning_y ? json::uinteger(*f.binning_y) : json::null());
  o.set("unit", f.unit ? json::string(*f.unit == pixel_unit::adu ? "adu" : *f.unit == pixel_unit::electron ? "electron" : "dimensionless")
                       : json::null());
  o.set("linearity", f.lin ? json::string(*f.lin == linearity::linear ? "linear" : "nonlinear") : json::null());
  return o;
}

json read_options_json(const fits_read_options& o) {
  json r = json::object();
  r.set("declared_bayer_x_offset", o.declared_bayer_x_offset ? json::uinteger(*o.declared_bayer_x_offset) : json::null());
  r.set("declared_bayer_y_offset", o.declared_bayer_y_offset ? json::uinteger(*o.declared_bayer_y_offset) : json::null());
  r.set("declared_phase", o.declared_phase ? json::string(to_string(*o.declared_phase)) : json::null());
  r.set("declared_linearity", o.declared_linearity ? json::string(to_string(*o.declared_linearity)) : json::null());
  return r;
}

json descriptor_json(const master_descriptor& d) {
  json o = json::object();
  o.set("role", json::string(role_name(d.role)));
  o.set("additive", d.additive ? json::string(additive_name(*d.additive)) : json::null());
  o.set("scaling", d.scaling ? json::string(scaling_name(*d.scaling)) : json::null());
  o.set("flat", d.flat ? json::string(flat_name(*d.flat)) : json::null());
  o.set("combine_count", json::uinteger(*d.combine_count));
  o.set("combine_method", json::string(combine_name(*d.combine)));
  o.set("rejection", json::string(rejection_name(*d.rejection)));
  o.set("variance_estimator", json::string(estimator_name(*d.estimator)));
  return o;
}

// ---------------------------------------------------------------- preflight (C-02.a §2)

struct frame_state {
  std::string name;
  const master_frame_spec* spec = nullptr;
  image header;  // geometria e metadati; nessun campione
  field_origins origins;
  std::uint64_t saturated = 0;
  double saturated_fraction = 0;
  std::optional<double> level;  // m_f, solo flat
};

image header_of(const image& im) {
  image h;
  h.geometry = im.geometry;
  h.layout = im.layout;
  h.type = im.type;
  h.unit = im.unit;
  h.lin = im.lin;
  h.lin_origin = im.lin_origin;
  h.phase = im.phase;
  h.cfa = im.cfa;
  h.metadata = im.metadata;
  h.uninterpreted_keywords = im.uninterpreted_keywords;
  return h;
}

struct master_plan {
  master_recipe recipe;
  std::vector<std::size_t> order;
  std::vector<frame_state> frames;  // ordine canonico
  std::size_t n = 0;
  std::uint32_t w = 0, h = 0;
  bool flat = false, has_sat = false;
  double sat = 0, offset = 0, eps = 0, level_mean = 0;
  std::uint8_t cfa_x = 0, cfa_y = 0;
  bool per_channel = false;
  std::array<const char*, 4> labels = {"all", "", "", ""};
  cpu_path path{};  // risolto nel preflight dalla richiesta `simd` (ADR-07)
  image out;
  std::vector<double> lbe, ratio;
  std::vector<std::uint8_t> nonpositive;
  std::vector<std::string> warnings, hypotheses;
  json compat = json::array();
  json master_metadata = json::object();
  json not_common = json::array();
  master_descriptor descriptor;

  std::size_t channel_of(std::uint32_t x, std::uint32_t y) const noexcept {
    return per_channel ? static_cast<std::size_t>(((y + cfa_y) & 1u) * 2 + ((x + cfa_x) & 1u)) : 0;
  }
};

master_plan make_plan(const master_recipe& r) {
  // Prima della validazione: i confronti sui parametri sono gia' operazioni in virgola mobile (ADR-07, D23).
  require_fp_environment();
  validate_master_recipe(r);
  master_plan p;
  p.path = resolve_cpu_path(*r.simd, read_cpu_capabilities());
  p.recipe = r;
  p.order = canonical_order(r);
  p.n = r.frames.size();
  p.flat = r.role == master_role::flat;
  p.has_sat = r.saturation_adu.has_value();
  if (p.has_sat) p.sat = *r.saturation_adu;
  if (p.flat) p.eps = *r.flat_level_rel_tol;
  p.frames.resize(p.n);
  for (std::size_t k = 0; k < p.n; ++k) {
    p.frames[k].name = frame_name(k);
    p.frames[k].spec = &p.recipe.frames[p.order[k]];
  }
  p.descriptor = master_descriptor_from_node(make_master_node(r));
  return p;
}

// Fase e linearita' dai metadati, prima di ogni accesso ai campioni (ADR-03, R1).
void require_phase(const std::string& name, const std::optional<metadata_value<process_phase>>& phase) {
  if (!phase) stop(error_category::metadata_missing, "master.metadata.phase", "fase di processo non dichiarata", {{"frame", name}});
  if (phase->value != process_phase::raw) {
    stop(error_category::unsupported, "master.input_state", "un master si combina da frame raw",
         {{"frame", name}, {"campo", "phase"}, {"valore", to_string(phase->value)}});
  }
}

void require_linear(const std::string& name, const std::optional<linearity>& lin) {
  if (!lin) stop(error_category::metadata_missing, "master.metadata.linearity", "linearita' non dichiarata", {{"frame", name}});
  if (*lin != linearity::linear) {
    stop(error_category::unsupported, "master.input_state", "un master si combina da frame lineari",
         {{"frame", name}, {"campo", "linearity"}, {"valore", "nonlinear"}});
  }
}

// Linearita' dall'header e dalla dichiarazione della ricetta: conflitto = incompatible metadata.declared_conflict.
std::optional<linearity> merged_linearity(const std::string& name, const std::optional<linearity>& header,
                                          const std::optional<linearity>& declared) {
  if (header && declared && *header != *declared) {
    stop(error_category::incompatible, "metadata.declared_conflict", "valore dichiarato diverso da quello presente",
         {{"input", name}, {"campo", "linearity"}});
  }
  return declared ? declared : header;
}

template <class T>
const T& need(const std::optional<metadata_value<T>>& field, const std::string& frame, const char* name) {
  if (!field) {
    stop(error_category::metadata_missing, std::string("master.metadata.") + name,
         "metadato richiesto assente (dichiarabile nella ricetta)", {{"frame", frame}, {"campo", name}});
  }
  return field->value;
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

double value_at(const image& im, std::size_t i) {
  return im.type == sample_type::f32 ? static_cast<double>(im.data<float>()[i]) : im.data<double>()[i];
}

// Controlli del singolo frame, statistiche sull'intera immagine (frazione satura, m_f) e copia dei metadati.
void inspect_frame(master_plan& p, std::size_t k, image& im) {
  frame_state& fs = p.frames[k];
  const std::string& name = fs.name;
  if (im.layout != plane_layout::mono && im.layout != plane_layout::cfa) {
    stop(error_category::unsupported, "calib.plane_layout", "un master accetta solo frame mono e CFA",
         {{"frame", name}, {"layout", layout_name(im.layout)}});
  }
  require_phase(name, im.phase);
  fs.origins = apply_declared(im, fs.spec->declared, fs.spec->declared_fields, name.c_str());
  require_linear(name, im.lin);
  if (!im.unit) stop(error_category::metadata_missing, "master.metadata.unit", "unita' non dichiarata", {{"frame", name}});
  if (*im.unit != pixel_unit::adu) {
    stop(error_category::unsupported, "master.input_state", "un master si combina da frame in ADU",
         {{"frame", name}, {"campo", "unit"}});
  }
  if (im.metadata.pedestal_adu && im.metadata.pedestal_adu->value != 0.0) {
    stop(error_category::unsupported, "calib.pedestal", "pedestal diverso da zero non supportato",
         {{"frame", name}, {"pedestal_adu", num_text(im.metadata.pedestal_adu->value)}});
  }
  if (im.variance) {
    stop(error_category::unsupported, "master.input_variance",
         "piano VARIANCE in un frame: la varianza del master e' empirica e il piano non avrebbe uso", {{"frame", name}});
  }
  validate_image(im);
  const bool finite_ok = im.type == sample_type::f32 ? all_finite_or_flagged<float>(im) : all_finite_or_flagged<double>(im);
  if (!finite_ok) {
    stop(error_category::invalid_input, "calib.nonfinite_unflagged", "campione non finito senza bit di invalidita'",
         {{"frame", name}});
  }
  const master_recipe& r = p.recipe;
  if (im.metadata.saturation_adu) {
    if (!r.saturation_adu) {
      stop(error_category::invalid_input, "master.param.saturation_adu",
           "soglia di saturazione nell'header del frame e assente dalla ricetta",
           {{"frame", name}, {"valore", num_text(im.metadata.saturation_adu->value)}});
    }
    if (im.metadata.saturation_adu->value != *r.saturation_adu) {
      stop(error_category::incompatible, "metadata.declared_conflict", "valore dichiarato diverso da quello presente",
           {{"frame", name}, {"campo", "saturation_adu"}, {"header", num_text(im.metadata.saturation_adu->value)},
            {"ricetta", num_text(*r.saturation_adu)}});
    }
  }
  if (const auto& t = im.metadata.imagetyp) {
    const std::optional<std::string> role = imagetyp_role(t->value);
    const std::string expected = role_name(r.role);
    if (!role) {
      p.warnings.push_back("imagetyp_unrecognized:" + name + ":" + t->value);
    } else if (!(*role == expected || (expected == "dark_flat" && *role == "dark"))) {
      stop(error_category::incompatible, "calib.imagetyp_conflict", "IMAGETYP indica un altro ruolo",
           {{"frame", name}, {"imagetyp", t->value}, {"ruolo_atteso", expected}});
    }
  }
  (void)need(im.metadata.camera, name, "camera");
  (void)need(im.metadata.gain, name, "gain");
  const double offset = need(im.metadata.offset_adu, name, "offset_adu");
  (void)need(im.metadata.exposure_s, name, "exposure_s");
  if (*r.temp_policy == temperature_policy::compare) (void)need(im.metadata.temperature_c, name, "temperature_c");
  if (p.flat) (void)need(im.metadata.filter, name, "filter");
  for (const auto& [field, v] : {std::pair{"binning_x", &im.geometry.binning_x}, std::pair{"binning_y", &im.geometry.binning_y}}) {
    if (!*v) {
      stop(error_category::metadata_missing, std::string("master.metadata.") + field,
           "metadato richiesto assente (dichiarabile nella ricetta)", {{"frame", name}, {"campo", field}});
    }
  }

  const std::size_t px = static_cast<std::size_t>(im.geometry.width) * im.geometry.height;
  if (p.has_sat) {
    for (std::size_t i = 0; i < px; ++i) {
      if (im.invalid.bits[i] == 0 && value_at(im, i) >= p.sat) ++fs.saturated;
    }
    fs.saturated_fraction = static_cast<double>(fs.saturated) / static_cast<double>(px);
    if (fs.saturated_fraction > *r.saturated_fraction_max) {
      stop(error_category::invalid_input, p.flat ? "master.flat_saturated" : "master.saturated",
           "frazione di campioni saturi oltre saturated_fraction_max",
           {{"frame", name}, {"frazione", num_text(fs.saturated_fraction)}, {"massimo", num_text(*r.saturated_fraction_max)}});
    }
  }
  if (p.flat) {
    std::vector<double> z;
    z.reserve(px);
    for (std::size_t i = 0; i < px; ++i) {
      if (im.invalid.bits[i] == 0) z.push_back(value_at(im, i) - offset);
    }
    if (z.empty()) {
      stop(error_category::invalid_input, "master.flat_low_signal", "nessun pixel valido per il livello del flat",
           {{"frame", name}});
    }
    const double level = median_of(z);
    if (!std::isfinite(level) || level < *r.flat_min_signal_adu) {
      stop(error_category::invalid_input, "master.flat_low_signal", "livello del flat sotto flat_min_signal_adu o non finito",
           {{"frame", name}, {"m_f", num_text(level)}, {"minimo", num_text(*r.flat_min_signal_adu)}});
    }
    fs.level = level;
  }
  fs.header = header_of(im);
}

json record_of(const std::string& pair, const char* field, json left, json right, const char* rule, json tolerance,
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

[[noreturn]] void mismatch(const std::string& pair, const char* field, const std::string& left, const std::string& right,
                           const std::string& tolerance) {
  stop(error_category::incompatible, std::string("master.mismatch.") + field, "frame del gruppo incompatibili",
       {{"coppia", pair}, {"campo", field}, {"sinistra", left}, {"destra", right}, {"tolleranza", tolerance}});
}

// Compatibilita' del frame k con il frame di riferimento canonico (C-02.a §2).
void compare_with_reference(master_plan& p, std::size_t k) {
  if (k == 0) return;
  const image& a = p.frames[0].header;
  const image& b = p.frames[k].header;
  const std::string pair = p.frames[0].name + "-" + p.frames[k].name;
  const auto exact_u = [&](const char* field, std::uint32_t x, std::uint32_t y) {
    if (x != y) mismatch(pair, field, std::to_string(x), std::to_string(y), "esatta");
    p.compat.push(record_of(pair, field, json::uinteger(x), json::uinteger(y), "exact", json::null()));
  };
  exact_u("width", a.geometry.width, b.geometry.width);
  exact_u("height", a.geometry.height, b.geometry.height);
  exact_u("binning_x", *a.geometry.binning_x, *b.geometry.binning_x);
  exact_u("binning_y", *a.geometry.binning_y, *b.geometry.binning_y);
  if (!(a.cfa == b.cfa)) mismatch(pair, "cfa", cfa_text(a.cfa), cfa_text(b.cfa), "esatta");
  p.compat.push(record_of(pair, "cfa", json::string(cfa_text(a.cfa)), json::string(cfa_text(b.cfa)), "exact", json::null()));
  const frame_metadata &ma = a.metadata, &mb = b.metadata;
  if (ma.camera->value != mb.camera->value) mismatch(pair, "camera", ma.camera->value, mb.camera->value, "esatta");
  p.compat.push(record_of(pair, "camera", json::string(ma.camera->value), json::string(mb.camera->value), "exact", json::null()));
  for (const auto& [field, fa, fb] : {std::tuple{"gain", &ma.gain, &mb.gain}, std::tuple{"offset_adu", &ma.offset_adu, &mb.offset_adu}}) {
    const double x = (*fa)->value, y = (*fb)->value;
    if (x != y) mismatch(pair, field, num_text(x), num_text(y), "esatta");
    p.compat.push(record_of(pair, field, json::number(x), json::number(y), "exact", json::null()));
  }
  {
    const double x = ma.exposure_s->value, y = mb.exposure_s->value, tol = *p.recipe.exposure_rel_tol;
    if (!(std::fabs(x - y) <= tol * std::max(std::fabs(x), std::fabs(y)))) {
      mismatch(pair, "exposure_s", num_text(x), num_text(y), "relativa " + num_text(tol));
    }
    p.compat.push(record_of(pair, "exposure_s", json::number(x), json::number(y), "relative_to_max", json::number(tol)));
  }
  if (*p.recipe.temp_policy == temperature_policy::compare) {
    const double x = ma.temperature_c->value, y = mb.temperature_c->value, tol = *p.recipe.temperature_abs_tol_c;
    if (!(std::fabs(x - y) <= tol)) mismatch(pair, "temperature_c", num_text(x), num_text(y), "assoluta " + num_text(tol));
    p.compat.push(record_of(pair, "temperature_c", json::number(x), json::number(y), "absolute", json::number(tol)));
  } else {
    p.compat.push(record_of(pair, "temperature_c", json::null(), json::null(), "skipped_not_applicable_declared", json::null(),
                            "skipped"));
  }
  if (p.flat) {
    if (ma.filter->value != mb.filter->value) mismatch(pair, "filter", ma.filter->value, mb.filter->value, "esatta");
    p.compat.push(record_of(pair, "filter", json::string(ma.filter->value), json::string(mb.filter->value), "exact", json::null()));
  }
}

// Campi opzionali (roi_x, roi_y, readout_mode, row_order): ogni valore presente si confronta con il primo frame
// canonico che lo porta, anche quando il frame di riferimento non lo ha; ogni assenza resta un'ipotesi del report.
void compare_optional_fields(master_plan& p, std::size_t k) {
  const auto field = [&](const char* name, auto member, auto to_text, auto to_json) {
    const auto& mine = p.frames[k].header.metadata.*member;
    if (!mine) {
      p.hypotheses.push_back(std::string("not_verified:") + name + ":absent_in=" + p.frames[k].name);
      return;
    }
    for (std::size_t j = 0; j < k; ++j) {
      const auto& first = p.frames[j].header.metadata.*member;
      if (!first) continue;
      const std::string pair = p.frames[j].name + "-" + p.frames[k].name;
      if (!(first->value == mine->value)) mismatch(pair, name, to_text(first->value), to_text(mine->value), "esatta");
      p.compat.push(record_of(pair, name, to_json(first->value), to_json(mine->value), "exact", json::null()));
      return;
    }
  };
  const auto int_text = [](std::int64_t v) { return std::to_string(v); };
  const auto int_json = [](std::int64_t v) { return json::integer(v); };
  const auto str_text = [](const std::string& v) { return v; };
  const auto str_json = [](const std::string& v) { return json::string(v); };
  field("roi_x", &frame_metadata::roi_x, int_text, int_json);
  field("roi_y", &frame_metadata::roi_y, int_text, int_json);
  field("readout_mode", &frame_metadata::readout_mode, str_text, str_json);
  field("row_order", &frame_metadata::row_order, str_text, str_json);
}

// Metadati del master (C-02.a, aggiunte di R2): comuni, dal frame di riferimento (esposizione e temperatura), omessi.
frame_metadata master_metadata_of(master_plan& p) {
  frame_metadata m;
  const auto field = [&](auto member, const char* name, auto to_json) {
    const auto& ref = p.frames[0].header.metadata.*member;
    bool common = true;
    for (const frame_state& f : p.frames) common = common && f.header.metadata.*member == ref;
    const bool tolerant = std::string(name) == "exposure_s" || std::string(name) == "temperature_c";
    if (!common && !tolerant) {
      p.not_common.push(json::string(name));
      return;
    }
    if (!ref) return;
    m.*member = ref;
    json e = json::object();
    e.set("value", to_json(ref->value));
    e.set("source", json::string(common ? "common" : "reference_frame"));
    p.master_metadata.set(name, std::move(e));
  };
  const auto d = [](double v) { return json::number(v); };
  const auto s = [](const std::string& v) { return json::string(v); };
  const auto i = [](std::int64_t v) { return json::integer(v); };
  field(&frame_metadata::exposure_s, "exposure_s", d);
  field(&frame_metadata::gain, "gain", d);
  field(&frame_metadata::conversion_gain_e_per_adu, "conversion_gain_e_per_adu", d);
  field(&frame_metadata::offset_adu, "offset_adu", d);
  field(&frame_metadata::temperature_c, "temperature_c", d);
  field(&frame_metadata::filter, "filter", s);
  field(&frame_metadata::camera, "camera", s);
  field(&frame_metadata::imagetyp, "imagetyp", s);
  field(&frame_metadata::pedestal_adu, "pedestal_adu", d);
  field(&frame_metadata::roi_x, "roi_x", i);
  field(&frame_metadata::roi_y, "roi_y", i);
  field(&frame_metadata::readout_mode, "readout_mode", s);
  field(&frame_metadata::row_order, "row_order", s);
  m.saturation_adu.reset();
  if (p.has_sat) {
    m.saturation_adu = metadata_value<double>{p.sat, {metadata_origin::declared, {}}};
    json e = json::object();
    e.set("value", json::number(p.sat));
    e.set("source", json::string("recipe"));
    p.master_metadata.set("saturation_adu", std::move(e));
  }
  return m;
}

// Dopo l'ispezione di tutti i frame: livelli del flat, uscita allocata, metadati del master.
void finish_preflight(master_plan& p) {
  const image& ref = p.frames[0].header;
  p.w = ref.geometry.width;
  p.h = ref.geometry.height;
  p.offset = ref.metadata.offset_adu->value;
  if (p.flat) {
    double sum = *p.frames[0].level;
    for (std::size_t k = 1; k < p.n; ++k) sum += *p.frames[k].level;
    p.level_mean = sum / static_cast<double>(p.n);
    if (!std::isfinite(p.level_mean)) {
      stop(error_category::numeric, "master.report_nonfinite", "media dei livelli del flat non finita");
    }
    for (const frame_state& f : p.frames) {
      const double rel = *f.level / p.level_mean - 1;
      if (std::fabs(rel) > p.eps) {
        stop(error_category::unsupported, "master.variance_domain", "livello del flat oltre flat_level_rel_tol",
             {{"frame", f.name}, {"m_f", num_text(*f.level)}, {"m_media", num_text(p.level_mean)}, {"tolleranza", num_text(p.eps)}});
      }
    }
  }
  if (!p.has_sat) p.warnings.push_back("saturation_not_evaluated");
  if (*p.recipe.temp_policy == temperature_policy::not_applicable_declared && p.n > 1) {
    p.warnings.push_back("temperature_not_compared");
  }
  image_geometry g(p.w, p.h);
  g.binning_x = ref.geometry.binning_x;
  g.binning_y = ref.geometry.binning_y;
  p.out = make_image(g, p.recipe.output_type, ref.layout, ref.cfa);
  p.out.unit = pixel_unit::adu;
  p.out.lin = linearity::linear;
  p.out.lin_origin.value = metadata_origin::header;
  p.out.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::header, "ASTRPHAS"}};
  p.out.metadata = master_metadata_of(p);
  p.out.variance = variance_plane{aligned_buffer(p.out.samples.size())};
  const std::size_t px = static_cast<std::size_t>(p.w) * p.h;
  if (p.flat) {
    p.lbe.assign(px, std::numeric_limits<double>::quiet_NaN());
    p.ratio.assign(px, std::numeric_limits<double>::quiet_NaN());
    p.nonpositive.assign(px, 0);
  }
  if (ref.cfa) {
    p.per_channel = true;
    p.cfa_x = ref.cfa->x_offset;
    p.cfa_y = ref.cfa->y_offset;
    p.labels = channel_labels(ref.cfa->pattern);
  }
}

// ---------------------------------------------------------------- combinazione per pixel (C-02.a §6, §6.3, §7)

// `vals[f]` e `bits[f]`: righe [y0, y0 + rows) del frame canonico f. Kernel del percorso risolto nel preflight
// (ADR-07); ogni banda e' l'avvio di un task per il controllo dell'ambiente FP. Righe disgiunte: thread-safe.
void combine_rows(master_plan& p, std::uint32_t y0, std::uint32_t rows, const double* const* vals,
                  const std::uint16_t* const* bits) {
  kernels::master_band b;
  b.n = p.n;
  b.w = p.w;
  b.y0 = y0;
  b.rows = rows;
  b.vals = vals;
  b.bits = bits;
  b.has_sat = p.has_sat;
  b.sat = p.sat;
  b.f32_out = p.out.type == sample_type::f32;
  b.out = p.out.samples.data();
  b.outv = p.out.variance->samples.data();
  b.mask = p.out.invalid.bits.data();
  b.flat = p.flat;
  b.offset = p.offset;
  b.eps = p.eps;
  b.lbe = p.lbe.data();
  b.ratio = p.ratio.data();
  b.nonpositive = p.nonpositive.data();
  begin_kernel_task();
  if (p.path == cpu_path::avx2) kernels::master_band_avx2(b);
  else kernels::master_band_scalar(b);
}

std::string rows_digest(const image& out, std::uint32_t y0, std::uint32_t y1) {
  const std::size_t elem = sample_size(out.type);
  const std::size_t first = static_cast<std::size_t>(y0) * out.geometry.width;
  const std::size_t count = static_cast<std::size_t>(y1 - y0) * out.geometry.width;
  const auto bytes = [](const void* q, std::size_t n) {
    return std::span<const std::uint8_t>(static_cast<const std::uint8_t*>(q), n);
  };
  sha256_stream s;
  s.update(bytes(out.samples.data() + first * elem, count * elem));
  s.update(bytes(out.invalid.bits.data() + first, count * sizeof(std::uint16_t)));
  s.update(bytes(out.variance->samples.data() + first * elem, count * elem));
  return s.finish().sha256;
}

// ---------------------------------------------------------------- report (C-02.a §7)

json number_or_null(const std::optional<double>& v) { return v ? json::number(*v) : json::null(); }

json frame_report(const master_plan& p, const frame_state& f) {
  json o = json::object();
  o.set("name", json::string(f.name));
  o.set("digest_sha256", json::string(f.spec->digest.sha256));
  o.set("size_bytes", json::uinteger(f.spec->digest.size_bytes));
  o.set("role", json::string(role_name(p.recipe.role)));
  o.set("used", json::boolean(true));
  o.set("rejected_samples", json::uinteger(0));
  o.set("saturated_samples", p.has_sat ? json::uinteger(f.saturated) : json::null());
  o.set("saturated_fraction", p.has_sat ? json::number(f.saturated_fraction) : json::null());
  if (p.flat) {
    o.set("flat_level", json::number(*f.level));
    o.set("flat_level_rel_dev", json::number(*f.level / p.level_mean - 1));
  }
  const image& im = f.header;
  json fields = json::object();
  const auto field = [](json value, bool declared) {
    json e = json::object();
    e.set("value", std::move(value));
    e.set("origin", json::string(declared ? "declared" : "header"));
    return e;
  };
  fields.set("binning_x", field(json::uinteger(*im.geometry.binning_x), f.origins.binning_x));
  fields.set("binning_y", field(json::uinteger(*im.geometry.binning_y), f.origins.binning_y));
  fields.set("unit", field(json::string("adu"), f.origins.unit));
  fields.set("linearity", field(json::string("linear"), f.origins.lin));
  fields.set("phase", field(json::string(to_string(im.phase->value)), im.phase->source.origin == metadata_origin::declared));
  o.set("image_fields", std::move(fields));
  o.set("layout", json::string(layout_name(im.layout)));
  o.set("cfa", json::string(cfa_text(im.cfa)));
  o.set("sample_type", json::string(type_name(im.type)));
  o.set("metadata", metadata_report(im));
  json keywords = json::array();
  for (const std::string& k : im.uninterpreted_keywords) keywords.push(json::string(k));
  o.set("uninterpreted_keywords", std::move(keywords));
  return o;
}

json recipe_report(const master_recipe& r) {
  json o = json::object();
  o.set("name", json::string("master_mean"));
  o.set("role", json::string(role_name(r.role)));
  o.set("combine", json::string("mean"));
  o.set("rejection", json::string("none"));
  o.set("flat_normalization", r.flat_norm ? json::string(flat_norm_name(*r.flat_norm)) : json::null());
  o.set("flat_level_rel_tol", number_or_null(r.flat_level_rel_tol));
  o.set("flat_min_signal_adu", number_or_null(r.flat_min_signal_adu));
  o.set("saturation_adu", number_or_null(r.saturation_adu));
  o.set("saturated_fraction_max", number_or_null(r.saturated_fraction_max));
  o.set("exposure_rel_tol", json::number(*r.exposure_rel_tol));
  o.set("temperature_abs_tol_c", json::number(*r.temperature_abs_tol_c));
  o.set("temperature_policy", json::string(policy_name(*r.temp_policy)));
  o.set("output_type", json::string(type_name(r.output_type)));
  o.set("simd", json::string(simd_request_name(*r.simd)));
  return o;
}

// La mediana di un numero pari di valori finiti puo' andare in overflow nella media dei due centrali.
json median_or_null(std::vector<double>& v) {
  if (v.empty()) return json::null();
  const double m = median_of(v);
  if (!std::isfinite(m)) stop(error_category::numeric, "master.report_nonfinite", "mediana di una statistica del report non finita");
  return json::number(m);
}
json max_or_null(const std::vector<double>& v) {
  return v.empty() ? json::null() : json::number(*std::max_element(v.begin(), v.end()));
}

json channel_report(const master_plan& p) {
  const std::size_t channels = p.per_channel ? 4 : 1;
  std::vector<std::vector<double>> planes(channels), lbe(channels), ratio(channels);
  std::vector<std::uint64_t> ratio_null(channels, 0), nonpositive(channels, 0), valid(channels, 0);
  for (std::uint32_t y = 0; y < p.h; ++y) {
    for (std::uint32_t x = 0; x < p.w; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * p.w + x;
      if (p.out.invalid.bits[i] != 0) continue;
      const std::size_t c = p.channel_of(x, y);
      ++valid[c];
      const double v = p.out.type == sample_type::f32 ? static_cast<double>(p.out.variance_data<float>()[i])
                                                      : p.out.variance_data<double>()[i];
      planes[c].push_back(v);
      if (!p.flat) continue;
      lbe[c].push_back(p.lbe[i]);
      if (std::isnan(p.ratio[i])) ++ratio_null[c];
      else ratio[c].push_back(p.ratio[i]);
      nonpositive[c] += p.nonpositive[i];
    }
  }
  json list = json::array();
  for (std::size_t c = 0; c < channels; ++c) {
    json o = json::object();
    o.set("label", json::string(p.labels[c]));
    o.set("valid_pixels", json::uinteger(valid[c]));
    o.set("variance_plane_median", median_or_null(planes[c]));
    if (p.flat) {
      json lb = json::object();
      lb.set("median", median_or_null(lbe[c]));
      lb.set("max", max_or_null(lbe[c]));
      lb.set("ratio_median", median_or_null(ratio[c]));
      lb.set("ratio_max", max_or_null(ratio[c]));
      lb.set("level_bias_ratio_null", json::uinteger(ratio_null[c]));
      lb.set("ratio_null_reason", json::string("variance_zero"));
      o.set("level_bias_estimate", std::move(lb));
      o.set("pixels_nonpositive_signal", json::uinteger(nonpositive[c]));
    }
    list.push(std::move(o));
  }
  return list;
}

json build_report(master_plan& p) {
  json report = json::object();
  report.set("recipe", recipe_report(p.recipe));
  json frames = json::array();
  for (const frame_state& f : p.frames) frames.push(frame_report(p, f));
  report.set("frames", std::move(frames));
  report.set("frames_used", json::uinteger(p.n));
  report.set("frames_discarded", json::uinteger(0));
  report.set("rejection", json::string("none"));
  report.set("reference_frame", json::string(p.frames[0].spec->digest.sha256));
  report.set("compatibility", p.compat);
  if (p.flat) report.set("flat_level_mean", json::number(p.level_mean));
  report.set("master_metadata", p.master_metadata);
  report.set("metadata_not_common", p.not_common);
  json var = json::object();
  var.set("variance_estimator", json::string("empirical"));
  var.set("variance_source", json::string("plane"));
  var.set("plane", json::string("s2_over_n"));
  var.set("variance_cv_gaussian_homoscedastic", json::number(std::sqrt(2.0 / static_cast<double>(p.n - 1))));
  report.set("variance", std::move(var));
  report.set("channels", channel_report(p));
  std::array<std::uint64_t, 9> counts{};
  std::uint64_t invalid = 0;
  for (const std::uint16_t b : p.out.invalid.bits) {
    if (b != 0) ++invalid;
    for (std::size_t k = 0; k < counts.size(); ++k) counts[k] += (b >> k) & 1u;
  }
  json bitcounts = json::object();
  for (std::size_t k = 0; k < counts.size(); ++k) bitcounts.set(bit_names[k], json::uinteger(counts[k]));
  report.set("invalid_counts", std::move(bitcounts));
  report.set("invalid_pixels", json::uinteger(invalid));
  json hyp = json::array();
  for (const char* h : {"frames_independent", "outliers_sparse"}) hyp.push(json::string(h));
  if (p.flat) {
    for (const char* h : {"illumination_stable", "same_illumination_shape", "level_proportional", "additive_nonnegative",
                          "nonnegative_signal"}) {
      hyp.push(json::string(h));
    }
  } else {
    hyp.push(json::string("stationary_mean"));
  }
  for (const std::string& h : p.hypotheses) hyp.push(json::string(h));
  report.set("assumptions_not_verified", std::move(hyp));
  json warnings = json::array();
  for (const std::string& w : p.warnings) warnings.push(json::string(w));
  report.set("warnings", std::move(warnings));
  report.set("descriptor", descriptor_json(p.descriptor));
  return report;
}

master_result finish_result(master_plan& p) {
  master_result r;
  r.report = build_report(p);
  r.report_text = master_report_text(r.report);
  r.warnings = p.warnings;
  r.level_bias_estimate = std::move(p.lbe);
  r.output = std::move(p.out);
  return r;
}

json observed_inputs(const master_plan& p) {
  json list = json::array();
  for (const frame_state& f : p.frames) {
    json fr = frame_report(p, f);
    const json& fields = *fr.find("image_fields");
    json o = json::object();
    o.set("input_name", json::string(f.name));
    o.set("sha256", json::string(f.spec->digest.sha256));
    o.set("size_bytes", json::uinteger(f.spec->digest.size_bytes));
    o.set("phase", *fields.find("phase"));
    o.set("linearity", *fields.find("linearity"));
    o.set("uninterpreted_keywords", *fr.find("uninterpreted_keywords"));
    list.push(std::move(o));
  }
  return list;
}

void render(std::ostringstream& out, const json& value, int depth, const std::string& key) {
  const std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
  switch (value.type()) {
    case json::kind::object: {
      if (!key.empty()) out << pad << key << ":\n";
      std::vector<std::size_t> order(value.keys().size());
      for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
      std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return value.keys()[a] < value.keys()[b]; });
      for (const std::size_t i : order) render(out, value.items()[i], key.empty() ? depth : depth + 1, value.keys()[i]);
      return;
    }
    case json::kind::array:
      out << pad << key << ":" << (value.items().empty() ? " []" : "") << "\n";
      for (const json& item : value.items()) {
        if (item.type() == json::kind::object || item.type() == json::kind::array) {
          out << pad << "  -\n";
          render(out, item, depth + 2, "");
        } else {
          out << pad << "  - " << canonical_json(item) << "\n";
        }
      }
      return;
    default:
      out << pad << key << ": " << canonical_json(value) << "\n";
  }
}

// Header di uscita: al piu' tre HDU con max_header_cards card ciascuno, piu' il riempimento dei blocchi.
constexpr std::uint64_t output_header_bound = 3 * (std::uint64_t{fits_limits::max_header_cards} * 80 + 2 * 2880);

}  // namespace

// ---------------------------------------------------------------- interfaccia

void validate_master_recipe(const master_recipe& r) {
  if (r.frames.size() < 2) {
    stop(error_category::invalid_input, "master.frames_insufficient", "un master richiede almeno 2 frame",
         {{"frame", std::to_string(r.frames.size())}, {"minimo", "2"}});
  }
  validate_params(r);
  const std::vector<std::size_t> order = canonical_order(r);
  for (std::size_t k = 1; k < order.size(); ++k) {
    const master_frame_spec &a = r.frames[order[k - 1]], &b = r.frames[order[k]];
    if (a.digest.sha256 == b.digest.sha256) {
      stop(error_category::unsupported, "calib.shared_input", "due frame con lo stesso digest",
           {{"primo", std::to_string(order[k - 1])}, {"secondo", std::to_string(order[k])}, {"sha256", a.digest.sha256}});
    }
  }
}

node_spec make_master_node(const master_recipe& r) {
  require_fp_environment();  // prima dei confronti sui parametri (ADR-07, D23)
  validate_master_recipe(r);
  const std::vector<std::size_t> order = canonical_order(r);
  json params = recipe_report(r);
  json frames = json::array();
  std::vector<node_input> inputs;
  for (std::size_t k = 0; k < order.size(); ++k) {
    const master_frame_spec& f = r.frames[order[k]];
    json o = json::object();
    o.set("declared_metadata", declared_metadata_json(f.declared));
    o.set("declared_fields", fields_json(f.declared_fields));
    o.set("fits_read_options", read_options_json(f.read_options));
    frames.push(std::move(o));
    inputs.push_back(node_input::from_file(frame_name(k), f.digest));
  }
  params.set("frames", std::move(frames));
  return node_spec(master_operator_id, master_algorithm_version, master_serialization_version, std::move(params),
                   std::move(inputs), sample_type::f64, backend_name(resolve_cpu_path(*r.simd, read_cpu_capabilities())),
                   std::nullopt);
}

master_descriptor master_descriptor_from_node(const node_spec& node) {
  const auto bad = [&](const char* why) {
    stop(error_category::invalid_input, "master.not_master_node", "il nodo non e' un master del prodotto",
         {{"operatore", node.operator_id()}, {"motivo", why}});
  };
  if (node.operator_id() != master_operator_id) bad("operatore diverso");
  const json* role = node.params().find("role");
  const json* combine = node.params().find("combine");
  const json* rejection = node.params().find("rejection");
  const auto is_text = [](const json* j, const char* v) { return j && j->type() == json::kind::string && j->as_string() == v; };
  if (!is_text(combine, "mean") || !is_text(rejection, "none")) bad("combinazione diversa dalla media senza rejection");
  master_descriptor d;
  if (is_text(role, "bias")) d.role = master_role::bias;
  else if (is_text(role, "dark")) d.role = master_role::dark;
  else if (is_text(role, "dark_flat")) d.role = master_role::dark_flat;
  else if (is_text(role, "flat")) d.role = master_role::flat;
  else bad("ruolo assente o non riconosciuto");
  if (d.role == master_role::flat) {
    d.flat = flat_state::raw;
  } else if (d.role != master_role::bias) {
    d.additive = additive_state::bias_included;
    d.scaling = scaling_state::unscaled;
  }
  d.combine_count = static_cast<std::uint32_t>(node.inputs().size());
  d.combine = combine_method::mean;
  d.rejection = master_rejection::none;
  d.estimator = variance_estimator::empirical;
  return d;
}

master_result combine_master(const master_recipe& recipe, std::vector<image> frames) {
  master_plan p = make_plan(recipe);
  if (frames.size() != recipe.frames.size()) {
    stop(error_category::invalid_input, "master.param.frames", "serve un'immagine per ogni frame della ricetta",
         {{"immagini", std::to_string(frames.size())}, {"frame", std::to_string(recipe.frames.size())}});
  }
  for (std::size_t k = 0; k < p.n; ++k) {
    inspect_frame(p, k, frames[p.order[k]]);
    compare_with_reference(p, k);
    compare_optional_fields(p, k);
  }
  finish_preflight(p);
  constexpr std::uint32_t band_rows = 64;
  const std::size_t band = static_cast<std::size_t>(band_rows) * p.w;
  std::vector<std::vector<double>> vals(p.n, std::vector<double>(band));
  std::vector<std::vector<std::uint16_t>> bits(p.n, std::vector<std::uint16_t>(band));
  std::vector<const double*> vp(p.n);
  std::vector<const std::uint16_t*> bp(p.n);
  for (std::size_t f = 0; f < p.n; ++f) {
    vp[f] = vals[f].data();
    bp[f] = bits[f].data();
  }
  for (std::uint32_t y0 = 0; y0 < p.h; y0 += band_rows) {
    const std::uint32_t rows = std::min(band_rows, p.h - y0);
    for (std::size_t f = 0; f < p.n; ++f) {
      const image& im = frames[p.order[f]];
      const std::size_t first = static_cast<std::size_t>(y0) * p.w;
      for (std::size_t j = 0; j < static_cast<std::size_t>(rows) * p.w; ++j) {
        vals[f][j] = value_at(im, first + j);
        bits[f][j] = im.invalid.bits[first + j];
      }
    }
    combine_rows(p, y0, rows, vp.data(), bp.data());
  }
  return finish_result(p);
}

std::string master_report_text(const json& report) {
  std::ostringstream out;
  out << "Rapporto del master\n";
  render(out, report, 0, "");
  return out.str();
}

std::optional<std::uint64_t> master_task_bytes(std::uint64_t frames, std::uint64_t rows, std::uint64_t width) {
  std::uint64_t pointers = 0, cells = 0, per_frame = 0, bands = 0, scratch = 0, sum = 0;
  if (__builtin_mul_overflow(frames, sizeof(double*) + sizeof(std::uint16_t*), &pointers) ||
      __builtin_mul_overflow(rows, width, &cells) ||
      __builtin_mul_overflow(cells, sizeof(double) + sizeof(std::uint16_t), &per_frame) ||
      __builtin_mul_overflow(frames, per_frame, &bands) || __builtin_mul_overflow(cells, sizeof(double), &scratch) ||
      __builtin_add_overflow(pointers, bands, &sum) || __builtin_add_overflow(sum, scratch, &sum)) {
    return std::nullopt;
  }
  return sum;
}

calibration_input_spec product_master_input(const sqlite_dag_store& store, const std::string& slot_name) {
  const std::optional<slot_record> slot = store.get_slot(slot_name);
  const slot_view view = store.read_slot(slot_name);
  if (!slot || view.state != freshness::current || !view.artifact) {
    const char* state = !slot ? "absent" : view.state == freshness::stale ? "stale" : view.state == freshness::missing ? "missing" : "current";
    stop(error_category::invalid_input, "calib.product_master_not_current", "lo slot del master non ha un artefatto corrente",
         {{"slot", slot_name}, {"stato", state}});
  }
  const std::optional<node_spec> node = store.get_node(slot->target_node_key);
  if (!node) {
    stop(error_category::graph, "dag.missing_node", "nodo del master assente dallo store",
         {{"slot", slot_name}, {"nodo", slot->target_node_key}});
  }
  calibration_input_spec in;
  in.path = store.artifact_path(view.artifact->rel_path).string();
  in.digest = content_digest{view.artifact->sha256, view.artifact->size_bytes};
  in.descriptor = master_descriptor_from_node(*node);
  in.variance.source = variance_source::plane;
  in.master_node_key = node->key();
  return in;
}

job_outcome run_master_job(sqlite_dag_store& store, const std::string& slot_name, const master_recipe& recipe,
                           memory_pool& pool, const master_job_options& options, const cancel_token& cancel,
                           const std::optional<budget_snapshot>& budget) {
  require_fp_environment();  // prima di serializzare le risorse nel budget e di avviare il job (ADR-07)
  const auto work = [&](const job_record& job) -> job_files_result {
    master_plan p = make_plan(recipe);
    const node_spec node = make_master_node(recipe);
    if (node.key() != job.node_key) {
      stop(error_category::invalid_input, "master.node_mismatch", "la ricetta non corrisponde al nodo dello slot",
           {{"nodo_slot", job.node_key}, {"nodo_ricetta", node.key()}});
    }
    // Apertura, fase e linearita' dai soli header, piano dei file: nessun campione letto.
    std::vector<fits_input> inputs;
    std::vector<fits_plan> plans;
    inputs.reserve(p.n);
    for (std::size_t k = 0; k < p.n; ++k) {
      const master_frame_spec& f = *p.frames[k].spec;
      inputs.push_back(open_fits_input(f.path));
      const header_peek peek = peek_header(inputs.back(), f.read_options);
      require_phase(p.frames[k].name, peek.phase);
      require_linear(p.frames[k].name, merged_linearity(p.frames[k].name, peek.lin, f.declared_fields.lin));
      plans.push_back(plan_fits(inputs.back()));
      if (plans.back().planes != 1) {
        stop(error_category::unsupported, "calib.plane_layout", "un master accetta solo frame mono e CFA",
             {{"frame", p.frames[k].name}, {"planes", std::to_string(plans.back().planes)}});
      }
    }
    std::vector<fits_row_layout> layouts;
    for (const fits_input& in : inputs) layouts.push_back(plan_fits_rows(in));

    // Piano del job (ADR-06, estensione R2.r): un frame letto per intero alla volta, uscita, statistiche, codifica;
    // per task i blocchi di righe di tutti i frame, mai la pila. Il pianificatore ferma un piano oltre il budget o non
    // rappresentabile prima di ogni prenotazione.
    std::uint64_t frame_peak = 0;
    for (const fits_plan& fp : plans) frame_peak = std::max(frame_peak, fp.peak_bytes);
    const std::uint64_t w = plans[0].width, h = plans[0].height;
    const resource_config& config = options.resources;
    const std::size_t cpus = config.cpu_set ? config.cpu_set->size() : cpu_count();
    planner_input in{pool.budget_bytes(), cpus, plans[0].width, plans[0].height, sample_size(recipe.output_type),
                     frame_peak, output_header_bound};
    in.frame_count = p.n;
    const resource_plan chosen = plan_resources(in, config);
    job_plan plan;
    plan.shared_bytes = chosen.shared_bytes;
    plan.per_task_bytes = chosen.per_task_bytes;
    plan.worker_count = chosen.worker_count;
    plan.queue_capacity = chosen.task_queue_capacity;
    plan.tile_rows = chosen.tile_rows;
    const std::uint64_t rows = chosen.tile_rows;
    const std::uint64_t tasks = chosen.task_count;

    json observed = json::array();
    std::vector<stored_file> files;
    std::vector<std::string> warnings;
    std::optional<master_result> result;

    job_spec spec(node, tasks, [&](task_context& ctx) -> std::string {
      const std::uint32_t y0 = static_cast<std::uint32_t>(ctx.index * rows);
      const std::uint32_t cnt = static_cast<std::uint32_t>(std::min<std::uint64_t>(h, y0 + rows) - y0);
      pool_buffer buffer(ctx.memory, static_cast<std::size_t>(plan.per_task_bytes));
      auto** vp = reinterpret_cast<const double**>(buffer.data());
      auto** bp = reinterpret_cast<const std::uint16_t**>(vp + p.n);
      auto* vals = reinterpret_cast<double*>(bp + p.n);
      auto* bits = reinterpret_cast<std::uint16_t*>(vals + p.n * rows * w);
      auto* raw = reinterpret_cast<std::byte*>(bits + p.n * rows * w);
      for (std::size_t f = 0; f < p.n; ++f) {
        ctx.check_cancel();
        double* v = vals + f * rows * w;
        std::uint16_t* b = bits + f * rows * w;
        read_fits_rows(inputs[f], layouts[f], y0, cnt, raw, v, b);
        vp[f] = v;
        bp[f] = b;
      }
      combine_rows(p, y0, cnt, vp, bp);
      return rows_digest(p.out, y0, y0 + cnt);
    });

    spec.prepare = [&](job_context& jc) {
      const std::uint64_t budget_bytes = jc.shared.bytes();
      for (std::size_t k = 0; k < p.n; ++k) {
        if (jc.cancel.requested() || jc.stopping.requested()) {
          stop(error_category::cancelled, "sched.cancelled", "annullamento durante la lettura dei frame");
        }
        const master_frame_spec& f = *p.frames[k].spec;
        image im;
        {
          const std::vector<std::uint8_t> bytes = read_input_bytes(inputs[k], budget_bytes);
          const content_digest got = digest_of(bytes);
          if (!(got == f.digest)) {
            stop(error_category::integrity, "input.digest_mismatch", "digest del frame diverso da quello della ricetta",
                 {{"frame", p.frames[k].name}, {"atteso", f.digest.sha256}, {"trovato", got.sha256}});
          }
          im = decode_fits(bytes, budget_bytes, f.read_options);
        }
        if (im.geometry.width != layouts[k].width || im.geometry.height != layouts[k].height) {
          stop(error_category::integrity, "input.geometry_changed", "la geometria decodificata differisce da quella del piano",
               {{"frame", p.frames[k].name}});
        }
        inspect_frame(p, k, im);
        compare_with_reference(p, k);
        compare_optional_fields(p, k);
      }
      finish_preflight(p);
      observed = observed_inputs(p);
      if (options.hook) options.hook("frames.read");
    };

    spec.finalize = [&](job_context& jc) {
      // I blocchi letti appartengono al file verificato: stessa identita' fstat e stesso digest dell'intero file.
      pool_buffer chunk(jc.shared, static_cast<std::size_t>(master_digest_chunk_bytes));
      for (std::size_t k = 0; k < p.n; ++k) {
        try {
          fits_detail::check_unchanged(inputs[k]);
          sha256_stream hasher;
          for (std::uint64_t done = 0; done < inputs[k].size_bytes();) {
            const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(master_digest_chunk_bytes, inputs[k].size_bytes() - done));
            fits_detail::read_exact(inputs[k], done, chunk.data(), n);
            hasher.update({reinterpret_cast<const std::uint8_t*>(chunk.data()), n});
            done += n;
          }
          const content_digest got = hasher.finish();
          if (!(got == p.frames[k].spec->digest)) {
            stop(error_category::integrity, "input.digest_mismatch", "digest del frame cambiato durante il job",
                 {{"atteso", p.frames[k].spec->digest.sha256}, {"trovato", got.sha256}});
          }
        } catch (stop_error& e) {
          e.add_context("frame", p.frames[k].name);
          e.add_context("fase", "riverifica prima della pubblicazione");
          throw;
        }
      }
      warnings = p.warnings;
      result.emplace(finish_result(p));
      const std::vector<std::uint8_t> fits = encode_fits(result->output);
      const content_digest fits_digest = digest_of(fits);
      json image_out = json::object();
      image_out.set("sha256", json::string(fits_digest.sha256));
      image_out.set("size_bytes", json::uinteger(fits_digest.size_bytes));
      json outputs = json::object();
      outputs.set("master_image", std::move(image_out));
      result->report.set("outputs", std::move(outputs));
      const std::string report_json = canonical_json(result->report);
      const std::string report_text = master_report_text(result->report);
      const auto span = [](const std::string& s) {
        return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
      };
      files = store.write_files(
          {artifact_content{fits, "fits", "image", std::nullopt, master_image_role},
           artifact_content{span(report_json), "json", "master_report", std::nullopt, master_report_role},
           artifact_content{span(report_text), "txt", "master_report_text", std::nullopt, master_report_text_role}});
    };

    if (options.on_job_plan) options.on_job_plan(plan);
    job_scheduler scheduler(pool, system_affinity, scheduler_tuning_of(config));
    scheduler.run(plan, spec, cancel);
    json details = json::object();
    details.set("observed_inputs", std::move(observed));
    details.set("resource_plan", resource_plan_json(chosen));
    const cpu_capabilities caps = read_cpu_capabilities();
    details.set("cpu", cpu_dispatch_json(*recipe.simd, caps, resolve_cpu_path(*recipe.simd, caps)));
    return job_files_result{std::move(files), std::move(warnings), std::move(details)};
  };
  std::optional<budget_snapshot> recorded = budget;
  if (recorded) recorded->resources = resource_config_json(options.resources);
  return run_job_files(store, slot_name, work, recorded, &cancel);
}

}  // namespace astro
