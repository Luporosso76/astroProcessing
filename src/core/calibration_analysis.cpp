#include "astro/calibration_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

#include "astro/error.hpp"
#include "astro/operations.hpp"
#include "calibration_internal.hpp"

namespace astro {

namespace {

[[noreturn]] void stop(error_category category, std::string code, const std::string& message,
                       stop_error::context_list context = {}) {
  throw stop_error(category, std::move(code), message, std::move(context));
}

std::uint64_t lcm_with_two(std::uint32_t n) {
  if (n == 0) return 2;
  return n % 2 == 0 ? std::uint64_t{n} : 2 * std::uint64_t{n};
}

// Ingressi che la catena richiede: bias, dark, D'.
struct chain_needs {
  bool bias, dark, dark_prime;
};

chain_needs needs_of(light_chain chain) {
  switch (chain) {
    case light_chain::none: return {false, false, false};
    case light_chain::bias: return {true, false, false};
    case light_chain::dark: return {false, true, false};
    case light_chain::bias_dark_sub: return {true, false, true};
    case light_chain::dark_sub_partial: return {false, false, true};
  }
  stop(error_category::invalid_input, "calib.chain_input", "catena del light sconosciuta");
}

void check_input(bool needed, bool present, const char* name, light_chain chain) {
  if (needed && !present)
    stop(error_category::invalid_input, "calib.chain_input", "ingresso richiesto dalla catena assente",
         {{"chain", to_string(chain)}, {"input", name}});
  if (!needed && present)
    stop(error_category::invalid_input, "calib.chain_input_not_applicable", "ingresso non richiesto dalla catena",
         {{"chain", to_string(chain)}, {"input", name}});
}

std::optional<offset_remover> remover_of(light_chain chain, const light_offset_inputs& in) {
  switch (chain) {
    case light_chain::bias:
    case light_chain::bias_dark_sub: return offset_remover{master_role::bias, *in.bias};
    case light_chain::dark: return offset_remover{master_role::dark, *in.dark};
    case light_chain::none:
    case light_chain::dark_sub_partial: return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

const char* to_string(light_chain chain) noexcept {
  switch (chain) {
    case light_chain::none: return "light.none";
    case light_chain::bias: return "light.bias";
    case light_chain::dark: return "light.dark";
    case light_chain::bias_dark_sub: return "light.bias_dark_sub";
    case light_chain::dark_sub_partial: return "light.dark_sub_partial";
  }
  std::abort();  // enum fuori dai valori dichiarati: memoria corrotta
}

const char* to_string(offset_state state) noexcept {
  switch (state) {
    case offset_state::removed: return "removed";
    case offset_state::known_not_removed: return "known_not_removed";
    case offset_state::unknown: return "unknown";
  }
  std::abort();  // enum fuori dai valori dichiarati: memoria corrotta
}

const char* to_string(black_applied_by_decoder applied) noexcept {
  switch (applied) {
    case black_applied_by_decoder::none: return "none";
  }
  std::abort();  // enum fuori dai valori dichiarati: memoria corrotta
}

decoder_black_period decoder_black_period_of(const decoder_state& s) {
  const std::uint64_t cells = std::uint64_t{s.black_pattern_rows} * s.black_pattern_cols;
  const bool bad_pattern = (s.black_pattern_rows == 0) != (s.black_pattern_cols == 0) || cells > decoder_state::max_black_pattern ||
                           s.black_pattern.size() != cells;
  const bool bad_color =
      std::any_of(s.cfa_color_index.begin(), s.cfa_color_index.end(), [](std::uint8_t c) { return c > 3; });
  const bool bad_window = s.window.width == 0 || s.window.height == 0;
  if (bad_pattern || bad_color || bad_window)
    stop(error_category::invalid_input, "calib.decoder_state", "stato del decoder incoerente per il nero effettivo",
         {{"check", bad_pattern ? "black_pattern" : (bad_color ? "cfa_color_index" : "window")},
          {"window", std::to_string(s.window.width) + "x" + std::to_string(s.window.height)},
          {"pattern_rows", std::to_string(s.black_pattern_rows)},
          {"pattern_cols", std::to_string(s.black_pattern_cols)},
          {"pattern_size", std::to_string(s.black_pattern.size())}});
  decoder_black_period p;
  p.rows = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(lcm_with_two(s.black_pattern_rows), s.window.height));
  p.cols = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(lcm_with_two(s.black_pattern_cols), s.window.width));
  p.values.reserve(std::size_t{p.rows} * p.cols);
  for (std::uint32_t r = 0; r < p.rows; ++r) {
    for (std::uint32_t c = 0; c < p.cols; ++c) {
      const std::uint64_t color = s.black_per_color[s.cfa_color_index[(r % 2) * 2 + (c % 2)]];
      const std::uint64_t pattern =
          cells == 0 ? 0 : s.black_pattern[(r % s.black_pattern_rows) * std::size_t{s.black_pattern_cols} + (c % s.black_pattern_cols)];
      p.values.push_back(std::uint64_t{s.black} + color + pattern);
    }
  }
  return p;
}

light_offset_entry make_light_offset_entry(light_chain chain, const image& light, const light_offset_inputs& inputs) {
  if (light.metadata.pedestal_adu && light.metadata.pedestal_adu->value != 0.0)
    stop(error_category::unsupported, "calib.pedestal", "pedestal diverso da zero non supportato",
         {{"input", "light"}, {"pedestal_adu", std::to_string(light.metadata.pedestal_adu->value)}});
  const chain_needs need = needs_of(chain);
  check_input(need.bias, inputs.bias.has_value(), "bias", chain);
  check_input(need.dark, inputs.dark.has_value(), "dark", chain);
  check_input(need.dark_prime, inputs.dark_prime.has_value(), "dark_prime", chain);
  const bool raw = light.decoder_state.has_value();
  if (raw && !inputs.decoder)
    stop(error_category::invalid_input, "calib.decoder_identity", "light con decoder_state senza identità del decoder",
         {{"chain", to_string(chain)}});
  if (!raw && inputs.decoder)
    stop(error_category::invalid_input, "calib.chain_input_not_applicable",
         "identità del decoder su un light senza decoder_state", {{"chain", to_string(chain)}, {"input", "decoder"}});

  light_offset_entry e;
  e.remover = remover_of(chain, inputs);
  e.state = e.remover ? offset_state::removed : (raw ? offset_state::known_not_removed : offset_state::unknown);
  if (raw) {
    e.black = decoder_black_period_of(*light.decoder_state);
    e.black_applied = black_applied_by_decoder::none;
    e.identity = inputs.decoder;
  }
  return e;
}

// ---------------------------------------------------------------- analisi G-CAL

namespace {

using reason = std::optional<std::string>;

const char* role_text(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  std::abort();
}

// Catene di §b.5 che accettano il ruolo e lo stato dichiarato, in ordine di tabella: le sceglie `additive` per dark e
// dark-flat (`bias_subtracted` → catene di `D'` / `DF'`, altrimenti di `D` / `DF`) e `flat` per il flat
// (`corrected_unnormalized`, `calibrated`, altrimenti le catene del flat `raw`), anche se lo stato è escluso.
std::vector<std::string> chains_for(const master_descriptor& d) {
  switch (d.role) {
    case master_role::bias: return {"light.bias", "light.bias_dark_sub", "flat.bias_dark_flat_sub", "flat.bias"};
    case master_role::dark:
      if (d.additive == additive_state::bias_subtracted) return {"light.bias_dark_sub", "light.dark_sub_partial"};
      return {"light.dark"};
    case master_role::dark_flat:
      if (d.additive == additive_state::bias_subtracted) return {"flat.bias_dark_flat_sub"};
      return {"flat.dark_flat"};
    case master_role::flat:
      if (d.flat == flat_state::corrected_unnormalized) return {"flat.normalize"};
      if (d.flat == flat_state::calibrated) return {"flat.given_q"};
      return {"flat.dark_flat", "flat.bias_dark_flat_sub", "flat.bias"};
  }
  std::abort();
}

bool is_flat_chain(const std::string& chain) { return chain.rfind("flat.", 0) == 0; }

// Stato di §b.2: motivo di esclusione o nessuno.
reason state_reason(const master_descriptor& d) {
  switch (d.role) {
    case master_role::bias:
      if (d.additive || d.scaling || d.flat) return "state_incomplete";
      return std::nullopt;
    case master_role::dark:
    case master_role::dark_flat:
      if (!d.additive || *d.additive == additive_state::unknown) return "state_unknown";
      if (!d.scaling || *d.scaling == scaling_state::unknown) return "state_unknown";
      if (*d.scaling == scaling_state::scaled) return "state_scaled";
      return std::nullopt;
    case master_role::flat:
      if (!d.flat || *d.flat == flat_state::unknown) return "state_unknown";
      if (*d.flat == flat_state::calibrated && !d.flat_normalization_declared) return "state_incomplete";
      return std::nullopt;
  }
  std::abort();
}

// Riga di ADR-03.b §b.14 di un master di stato ammesso e unità richiesta (§b.2).
bool prepared_master(const master_descriptor& d) {
  if (d.role == master_role::dark || d.role == master_role::dark_flat) return d.additive == additive_state::bias_subtracted;
  if (d.role == master_role::flat) return d.flat == flat_state::corrected_unnormalized || d.flat == flat_state::calibrated;
  return false;
}

pixel_unit required_unit(const master_descriptor& d) {
  return d.role == master_role::flat && d.flat == flat_state::calibrated ? pixel_unit::dimensionless : pixel_unit::adu;
}

bool layout_admitted(plane_layout l) { return l == plane_layout::mono || l == plane_layout::cfa; }

// Dominio del master dopo lo stato: fase, linearità, unità, layout (C-01.b §2), poi coverage e piedistallo.
reason domain_reason(const analysis_candidate& c) {
  const image& m = c.img;
  if (!m.phase) return "metadata_missing.phase";
  const operation_kind row =
      prepared_master(c.descriptor) ? operation_kind::calibrate_master_prepared : operation_kind::calibrate_master_raw;
  const std::string code = blocked_code(row, process_state{m.phase->value, classify_linearity(m.lin)});
  if (code == "op.phase_blocked") return "phase";
  if (code == "op.linearity_blocked") return "linearity";
  ASTRO_ASSERT(code.empty());
  if (!m.unit) return "metadata_missing.unit";
  if (*m.unit != required_unit(c.descriptor)) return "unit";
  if (!layout_admitted(m.layout)) return "layout";
  if (m.coverage) return "coverage";
  if (m.metadata.pedestal_adu && m.metadata.pedestal_adu->value != 0.0) return "pedestal";
  return std::nullopt;
}

bool imagetyp_conflicts(const image& img, const std::string& expected, std::vector<std::string>& warnings,
                        const char* input) {
  const auto& t = img.metadata.imagetyp;
  if (!t) return false;
  const std::optional<std::string> role = calib_detail::imagetyp_role(t->value);
  if (!role) {
    warnings.push_back(std::string("imagetyp_unrecognized:") + input + ":" + t->value);
    return false;
  }
  return !(*role == expected || (expected == "dark_flat" && *role == "dark"));
}

// Confronti di ADR-16.a §2 per una coppia, primo violato nell'ordine di `.a`; `additive` = coppia L–D, L–D', L–B, F–DF,
// F–DF', F–B; `exposure` = coppia con dark o dark-flat. `offset` = confronto di `offset_adu`, solo in una coppia senza
// ingressi con stato del decoder (§b.3, §b.7).
struct pair_rule {
  const char* name;
  bool additive, exposure;
};

template <class T>
reason exact_field(const std::optional<metadata_value<T>>& a, const std::optional<metadata_value<T>>& b, const char* f) {
  if (!a || !b) return std::string("metadata_missing.") + f;
  if (!(a->value == b->value)) return std::string("incompatible.") + f;
  return std::nullopt;
}

reason compare_metadata(const image& a, const image& b, const pair_rule& rule, bool offset,
                        const calibration_declarations& decl,
                        std::vector<std::string>& warnings) {
  if (a.geometry.width != b.geometry.width) return "incompatible.width";
  if (a.geometry.height != b.geometry.height) return "incompatible.height";
  for (const auto& [name, x, y] : {std::tuple{"binning_x", a.geometry.binning_x, b.geometry.binning_x},
                                   std::tuple{"binning_y", a.geometry.binning_y, b.geometry.binning_y}}) {
    if (!x || !y) return std::string("metadata_missing.") + name;
    if (*x != *y) return std::string("incompatible.") + name;
  }
  if (!(a.cfa == b.cfa)) return "incompatible.cfa";
  if (reason r = exact_field(a.metadata.camera, b.metadata.camera, "camera")) return r;
  if (rule.additive) {
    if (reason r = exact_field(a.metadata.gain, b.metadata.gain, "gain")) return r;
    if (offset) {
      if (reason r = exact_field(a.metadata.offset_adu, b.metadata.offset_adu, "offset_adu")) return r;
    }
    if (rule.exposure) {
      if (!a.metadata.exposure_s || !b.metadata.exposure_s) return "metadata_missing.exposure_s";
      const double ea = a.metadata.exposure_s->value, eb = b.metadata.exposure_s->value;
      if (!(std::fabs(ea - eb) <= *decl.exposure_rel_tol * std::max(std::fabs(ea), std::fabs(eb))))
        return "incompatible.exposure_s";
    }
    if (*decl.temp_policy == temperature_policy::compare) {
      if (!a.metadata.temperature_c || !b.metadata.temperature_c) return "metadata_missing.temperature_c";
      if (!(std::fabs(a.metadata.temperature_c->value - b.metadata.temperature_c->value) <= *decl.temperature_abs_tol_c))
        return "incompatible.temperature_c";
    } else {
      warnings.push_back(std::string("temperature_comparison_skipped:") + rule.name);
    }
  } else {
    if (reason r = exact_field(a.metadata.filter, b.metadata.filter, "filter")) return r;
  }
  const auto optional_field = [](const auto& x, const auto& y, const char* f) -> reason {
    if (x && y && !(x->value == y->value)) return std::string("incompatible.") + f;
    return std::nullopt;
  };
  if (reason r = optional_field(a.metadata.roi_x, b.metadata.roi_x, "roi_x")) return r;
  if (reason r = optional_field(a.metadata.roi_y, b.metadata.roi_y, "roi_y")) return r;
  if (reason r = optional_field(a.metadata.readout_mode, b.metadata.readout_mode, "readout_mode")) return r;
  if (reason r = optional_field(a.metadata.row_order, b.metadata.row_order, "row_order")) return r;
  return std::nullopt;
}

// Stati del decoder di un ingresso, col nero effettivo già calcolato (gli stati incoerenti fermano prima dei calcoli).
struct decoder_states {
  std::vector<const decoder_state*> states;
  std::vector<decoder_black_period> blacks;
};

decoder_states states_of(const std::vector<const decoder_state*>& states) {
  decoder_states out;
  for (const decoder_state* s : states) {
    out.states.push_back(s);
    out.blacks.push_back(decoder_black_period_of(*s));
  }
  return out;
}

// §b.7: campi strutturali nell'ordine del contratto, poi il nero effettivo per le coppie additive.
reason compare_decoder(const decoder_state& x, const decoder_black_period& bx, const decoder_state& y,
                       const decoder_black_period& by, bool additive, std::uint64_t tol) {
  if (x.raw_width != y.raw_width) return "decoder_state_mismatch.raw_width";
  if (x.raw_height != y.raw_height) return "decoder_state_mismatch.raw_height";
  if (!(x.window == y.window)) return "decoder_state_mismatch.window";
  if (x.cfa_color_index != y.cfa_color_index) return "decoder_state_mismatch.cfa_color_index";
  if (x.black_pattern_rows != y.black_pattern_rows) return "decoder_state_mismatch.black_pattern_rows";
  if (x.black_pattern_cols != y.black_pattern_cols) return "decoder_state_mismatch.black_pattern_cols";
  if (x.linearization != y.linearization) return "decoder_state_mismatch.linearization";
  if (x.curve_sha256 != y.curve_sha256) return "decoder_state_mismatch.curve_sha256";
  if (!additive) return std::nullopt;
  // Finestra e motivo uguali: i due periodi hanno le stesse dimensioni.
  ASTRO_ASSERT(bx.values.size() == by.values.size());
  for (std::size_t i = 0; i < bx.values.size(); ++i) {
    const std::uint64_t u = bx.values[i], v = by.values[i];
    if ((u > v ? u - v : v - u) > tol) return "decoder_black";
  }
  return std::nullopt;
}

reason compare_all_decoder(const decoder_states& a, const decoder_states& b, bool additive, std::uint64_t tol) {
  for (std::size_t j = 0; j < b.states.size(); ++j) {
    for (std::size_t i = 0; i < a.states.size(); ++i) {
      if (reason r = compare_decoder(*a.states[i], a.blacks[i], *b.states[j], b.blacks[j], additive, tol)) return r;
    }
  }
  return std::nullopt;
}

struct candidate_work {
  const analysis_candidate* c = nullptr;
  std::vector<std::string> chains;
  reason own;          // motivo che vale in ogni catena
  reason light_pair;   // L–X: catene del light (per il flat, L–F: tutte le sue catene)
  reason flat_pair;    // F–DF, F–B: catene del flat
  bool raw = false;    // descrittore con `frame_decoder_states`
  decoder_states decoder;
};

void check_nonnegative_param(const std::optional<double>& v, const char* name) {
  const char* why = !v ? "assente" : !std::isfinite(*v) ? "non finito" : *v < 0 ? "deve essere >= 0" : nullptr;
  if (why)
    stop(error_category::invalid_input, std::string("calib.param.") + name, "dichiarazione richiesta da una coppia presente",
         {{"parametro", name}, {"motivo", why}});
}

// Dominio del light (C-01.b §2, §7; ADR-03.b §b.14 riga `calibrate_light`): ogni violazione ferma l'analisi.
void require_light_domain(const image& l, std::vector<std::string>& warnings) {
  if (!l.phase)
    stop(error_category::metadata_missing, "calib.metadata.phase", "fase di processo del light non dichiarata",
         {{"role", "light"}, {"campo", "phase"}});
  require_operation_allowed(operation_kind::calibrate_light, process_state{l.phase->value, classify_linearity(l.lin)},
                            "light");
  if (!l.unit)
    stop(error_category::metadata_missing, "calib.metadata.unit", "unità del light non dichiarata",
         {{"role", "light"}, {"campo", "unit"}});
  if (*l.unit != pixel_unit::adu)
    stop(error_category::unsupported, "calib.unit", "unità del light diversa da adu", {{"role", "light"}});
  if (!layout_admitted(l.layout))
    stop(error_category::unsupported, "calib.plane_layout", "layout del light diverso da mono o cfa", {{"role", "light"}});
  if (l.coverage)
    stop(error_category::unsupported, "calib.coverage_input", "light con coverage fuori dal dominio della calibrazione",
         {{"input", "light"}});
  if (l.metadata.pedestal_adu && l.metadata.pedestal_adu->value != 0.0)
    stop(error_category::unsupported, "calib.pedestal", "pedestal diverso da zero non supportato",
         {{"input", "light"}, {"pedestal_adu", std::to_string(l.metadata.pedestal_adu->value)}});
  if (imagetyp_conflicts(l, "light", warnings, "light"))
    stop(error_category::incompatible, "calib.imagetyp_conflict", "IMAGETYP indica un altro ruolo",
         {{"input", "light"}, {"imagetyp", l.metadata.imagetyp->value}, {"ruolo_atteso", "light"}});
}

}  // namespace

calibration_analysis analyze_calibration(const calibration_analysis_input& in, const fault_injector& fault) {
  std::vector<std::string> warnings;
  require_light_domain(in.light, warnings);
  const bool light_raw = in.light.decoder_state.has_value();
  const decoder_states light_decoder =
      light_raw ? states_of({&*in.light.decoder_state}) : decoder_states{};

  // Descrittori e stati del decoder dei candidati: errori della dichiarazione, prima di ogni calcolo.
  candidate_work work[4];
  const std::pair<master_role, const std::optional<analysis_candidate>*> slots[4] = {
      {master_role::bias, &in.bias}, {master_role::dark, &in.dark}, {master_role::dark_flat, &in.dark_flat},
      {master_role::flat, &in.flat}};
  bool any_raw = light_raw;
  for (int k = 0; k < 4; ++k) {
    const auto& [role, opt] = slots[k];
    if (!*opt) continue;
    const analysis_candidate& c = **opt;
    const master_descriptor& d = c.descriptor;
    if (d.role != role)
      stop(error_category::invalid_input, "calib.role_slot", "master nello slot di un altro ruolo",
           {{"slot", role_text(role)}, {"ruolo_dichiarato", role_text(d.role)}});
    if (d.flat_normalization_declared && !(d.role == master_role::flat && d.flat == flat_state::calibrated))
      stop(error_category::invalid_input, "calib.param_not_applicable.flat_normalization_declared",
           "normalizzazione dichiarata su un master che non è un flat calibrated", {{"role", role_text(role)}});
    work[k].c = &c;
    work[k].chains = chains_for(d);
    work[k].raw = d.frame_decoder_states.has_value();
    any_raw = any_raw || work[k].raw;
    if (work[k].raw) {
      std::vector<const decoder_state*> states;
      for (const frame_decoder_state& f : *d.frame_decoder_states) states.push_back(&f.state);
      work[k].decoder = states_of(states);
    }
  }
  candidate_work& bias = work[0];
  candidate_work& dark = work[1];
  candidate_work& dark_flat = work[2];
  candidate_work& flat = work[3];

  // Dichiarazioni richieste dalle coppie presenti (ADR-16.a §2, §b.7).
  const calibration_declarations& decl = in.declarations;
  if (any_raw && !decl.decoder_black_abs_tol_adu)
    stop(error_category::invalid_input, "calib.param.decoder_black_abs_tol_adu",
         "tolleranza del nero obbligatoria con un ingresso con stato del decoder", {{"parametro", "decoder_black_abs_tol_adu"}});
  if (!any_raw && decl.decoder_black_abs_tol_adu)
    stop(error_category::invalid_input, "calib.param_not_applicable.decoder_black_abs_tol_adu",
         "tolleranza del nero senza alcun ingresso con stato del decoder", {{"parametro", "decoder_black_abs_tol_adu"}});
  const bool flat_pairs = flat.c != nullptr;
  if (dark.c || (dark_flat.c && flat_pairs)) check_nonnegative_param(decl.exposure_rel_tol, "exposure_rel_tol");
  if (dark.c || bias.c || (dark_flat.c && flat_pairs)) {
    if (!decl.temp_policy)
      stop(error_category::invalid_input, "calib.param.temperature_policy", "dichiarazione richiesta da una coppia presente",
           {{"parametro", "temperature_policy"}, {"motivo", "assente"}});
    if (*decl.temp_policy == temperature_policy::compare)
      check_nonnegative_param(decl.temperature_abs_tol_c, "temperature_abs_tol_c");
  }
  // Esiti per candidato (§b.2, C-01.b §2, §7, §b.7), nell'ordine: stato, dominio, IMAGETYP, ingresso condiviso, stato
  // del decoder.
  for (int k = 0; k < 4; ++k) {
    candidate_work& w = work[k];
    if (!w.c) continue;
    const master_descriptor& d = w.c->descriptor;
    w.own = state_reason(d);
    if (!w.own) w.own = domain_reason(*w.c);
    if (!w.own && imagetyp_conflicts(w.c->img, role_text(d.role), warnings, role_text(d.role))) w.own = "imagetyp_conflict";
    if (!w.own) {
      bool shared = w.c->digest == in.light_digest;
      for (int j = 0; j < 4; ++j) shared = shared || (j != k && work[j].c && work[j].c->digest == w.c->digest);
      if (shared) w.own = "shared_input";
    }
    // Elenco vuoto prima del confronto col light: lo stato di un master RAW non si presume (ADR-16.b §b.7).
    if (!w.own && w.raw && w.decoder.states.empty()) w.own = "metadata_missing.frame_decoder_states";
    if (!w.own && w.raw != light_raw) w.own = "decoder_state_mixed";
  }

  // Coppie di §b.3. Una coppia con un candidato escluso per sé non si valuta: il suo motivo vale già in ogni catena.
  const auto probe = [&fault](const char* point) {
    if (!fault) return;
    if (const int err = fault(point); err != 0)
      stop(error_category::io, "calib.analysis.fault", "guasto iniettato nell'analisi",
           {{"point", point}, {"errno", std::to_string(err)}});
  };
  // Il flat di riferimento di F–DF e F–B può essere escluso per sé: senza stato del decoder, o con `frame_decoder_states`
  // vuoto, la coppia non è confrontabile e lo dice il motivo.
  const auto pair = [&](candidate_work& owner, const image& ref, bool ref_raw, const decoder_states& ref_decoder,
                        const pair_rule& rule) -> reason {
    probe("calib.analysis.pair");
    if (owner.own) return std::nullopt;
    // `offset_adu` si confronta solo in una coppia senza ingressi RAW (§b.3, §b.7).
    if (reason r = compare_metadata(ref, owner.c->img, rule, !ref_raw && !owner.raw, decl, warnings)) return r;
    if (ref_raw != owner.raw) return "decoder_state_mixed";
    if (!owner.raw) return std::nullopt;
    if (ref_decoder.states.empty()) return "metadata_missing.frame_decoder_states";
    // `owner.raw`: la tolleranza è obbligatoria ed è già stata verificata presente.
    return compare_all_decoder(ref_decoder, owner.decoder, rule.additive, *decl.decoder_black_abs_tol_adu);
  };
  if (bias.c) bias.light_pair = pair(bias, in.light, light_raw, light_decoder, {"L-B", true, false});
  if (dark.c) dark.light_pair = pair(dark, in.light, light_raw, light_decoder, {"L-D", true, true});
  if (flat.c) flat.light_pair = pair(flat, in.light, light_raw, light_decoder, {"L-F", false, false});
  if (flat_pairs) {
    const image& f = flat.c->img;
    if (dark_flat.c) dark_flat.flat_pair = pair(dark_flat, f, flat.raw, flat.decoder, {"F-DF", true, true});
    if (bias.c) bias.flat_pair = pair(bias, f, flat.raw, flat.decoder, {"F-B", true, false});
  }

  calibration_analysis out;
  for (const candidate_work& w : work) {
    if (!w.c) continue;
    candidate_analysis ca;
    ca.role = w.c->descriptor.role;
    for (const std::string& chain : w.chains) {
      const bool flat_chain = is_flat_chain(chain) && ca.role != master_role::flat;
      const reason r = w.own ? w.own : flat_chain ? w.flat_pair : w.light_pair;
      ca.chains.push_back(chain_verdict{chain, r ? candidate_status::excluded : candidate_status::usable, r});
      if (!r) ca.status = candidate_status::usable;
    }
    out.candidates.push_back(std::move(ca));
  }
  out.warnings = std::move(warnings);
  return out;
}

}  // namespace astro
