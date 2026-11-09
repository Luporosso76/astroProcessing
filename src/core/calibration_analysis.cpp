#include "astro/calibration_analysis.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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

const char* to_string(flat_chain chain) noexcept {
  switch (chain) {
    case flat_chain::none: return "flat.none";
    case flat_chain::dark_flat: return "flat.dark_flat";
    case flat_chain::bias_dark_flat_sub: return "flat.bias_dark_flat_sub";
    case flat_chain::bias: return "flat.bias";
    case flat_chain::normalize: return "flat.normalize";
    case flat_chain::given_q: return "flat.given_q";
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

const char* decoder_state_difference(const decoder_state& x, const decoder_black_period& bx, const decoder_state& y,
                                     const decoder_black_period& by, bool additive, std::uint64_t tol) {
  if (x.raw_width != y.raw_width) return "raw_width";
  if (x.raw_height != y.raw_height) return "raw_height";
  if (!(x.window == y.window)) return "window";
  if (x.cfa_color_index != y.cfa_color_index) return "cfa_color_index";
  if (x.black_pattern_rows != y.black_pattern_rows) return "black_pattern_rows";
  if (x.black_pattern_cols != y.black_pattern_cols) return "black_pattern_cols";
  if (x.linearization != y.linearization) return "linearization";
  if (x.curve_sha256 != y.curve_sha256) return "curve_sha256";
  if (!additive) return nullptr;
  // Finestra e motivo uguali: i due periodi hanno le stesse dimensioni.
  ASTRO_ASSERT(bx.values.size() == by.values.size());
  for (std::size_t i = 0; i < bx.values.size(); ++i) {
    const std::uint64_t u = bx.values[i], v = by.values[i];
    if ((u > v ? u - v : v - u) > tol) return "decoder_black";
  }
  return nullptr;
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

reason compare_decoder(const decoder_state& x, const decoder_black_period& bx, const decoder_state& y,
                       const decoder_black_period& by, bool additive, std::uint64_t tol) {
  const char* field = decoder_state_difference(x, bx, y, by, additive, tol);
  if (field == nullptr) return std::nullopt;
  if (std::string_view(field) == "decoder_black") return std::string(field);
  return std::string("decoder_state_mismatch.") + field;
}

reason compare_all_decoder(const decoder_states& a, const decoder_states& b, bool additive, std::uint64_t tol) {
  for (std::size_t j = 0; j < b.states.size(); ++j) {
    for (std::size_t i = 0; i < a.states.size(); ++i) {
      if (reason r = compare_decoder(*a.states[i], a.blacks[i], *b.states[j], b.blacks[j], additive, tol)) return r;
    }
  }
  return std::nullopt;
}

// Esito di una coppia (§b.3, §b.7) per ingressi già validati: metadati, poi stato del decoder.
reason pair_outcome(const pair_rule& rule, const image& ref, bool ref_raw, const decoder_states& ref_decoder,
                    const image& owner_img, bool owner_raw, const decoder_states& owner_decoder,
                    const calibration_declarations& decl, std::vector<std::string>& warnings) {
  // `offset_adu` si confronta solo in una coppia senza ingressi RAW (§b.3, §b.7).
  if (reason r = compare_metadata(ref, owner_img, rule, !ref_raw && !owner_raw, decl, warnings)) return r;
  if (ref_raw != owner_raw) return "decoder_state_mixed";
  if (!owner_raw) return std::nullopt;
  if (ref_decoder.states.empty()) return "metadata_missing.frame_decoder_states";
  // `owner_raw`: la tolleranza è obbligatoria ed è già stata verificata presente.
  return compare_all_decoder(ref_decoder, owner_decoder, rule.additive, *decl.decoder_black_abs_tol_adu);
}

pair_rule rule_of(pair_kind kind) {
  switch (kind) {
    case pair_kind::light_bias: return {"L-B", true, false};
    case pair_kind::light_dark: return {"L-D", true, true};
    case pair_kind::light_flat: return {"L-F", false, false};
    case pair_kind::flat_dark_flat: return {"F-DF", true, true};
    case pair_kind::flat_bias: return {"F-B", true, false};
  }
  std::abort();
}

struct candidate_work {
  const analysis_candidate* c = nullptr;
  std::vector<std::string> chains;
  reason own;          // motivo che vale in ogni catena
  reason light_pair;   // L–X: catene del light (per il flat, L–F: tutte le sue catene)
  reason flat_pair;    // F–DF, F–B: catene del flat
  reason thermal;      // controllo termico: solo la catena `flat.bias` del flat
  bool raw = false;    // descrittore con `frame_decoder_states`
  decoder_states decoder;
};

void check_nonnegative_param(const std::optional<double>& v, const char* name) {
  const char* why = !v ? "assente" : !std::isfinite(*v) ? "non finito" : *v < 0 ? "deve essere >= 0" : nullptr;
  if (why)
    stop(error_category::invalid_input, std::string("calib.param.") + name, "dichiarazione richiesta da una coppia presente",
         {{"parametro", name}, {"motivo", why}});
}

// Politica termica e tolleranza richieste da una coppia additiva. La tolleranza è obbligatoria e nel dominio con
// entrambe le politiche (ADR-16.a, «Parametri obbligatori»): con `not_applicable_declared` non entra nel confronto.
void require_temperature_declarations(const calibration_declarations& decl) {
  if (!decl.temp_policy)
    stop(error_category::invalid_input, "calib.param.temperature_policy", "dichiarazione richiesta da una coppia presente",
         {{"parametro", "temperature_policy"}, {"motivo", "assente"}});
  check_nonnegative_param(decl.temperature_abs_tol_c, "temperature_abs_tol_c");
}

// Dominio di un parametro della ricetta o di una dichiarazione (ADR-16.a §2, §b.5).
enum class param_domain { nonnegative, positive, open_unit };

const char* domain_violation(double v, param_domain d) {
  if (!std::isfinite(v)) return "non finito";
  switch (d) {
    case param_domain::nonnegative: return v < 0 ? "deve essere >= 0" : nullptr;
    case param_domain::positive: return v > 0 ? nullptr : "deve essere > 0";
    case param_domain::open_unit: return v > 0 && v < 1 ? nullptr : "deve essere in (0, 1)";
  }
  std::abort();
}

[[noreturn]] void bad_param(const std::string& name, const char* why) {
  stop(error_category::invalid_input, "calib.param." + name, "parametro della calibrazione fuori regola",
       {{"parametro", name}, {"motivo", why}});
}

// Dichiarazioni non richieste dall'analisi: se presenti, nel dominio; le due termiche insieme.
void check_optional_declarations(const calibration_declarations& d) {
  const auto in_domain = [](const std::optional<double>& v, const char* name, param_domain dom) {
    if (!v) return;
    if (const char* why = domain_violation(*v, dom)) bad_param(name, why);
  };
  in_domain(d.q_min, "q_min", param_domain::positive);
  in_domain(d.q_rel_sigma_max, "q_rel_sigma_max", param_domain::positive);
  if (d.flat_thermal_bound_adu.has_value() != d.flat_thermal_rel_budget.has_value())
    bad_param(d.flat_thermal_bound_adu ? "flat_thermal_rel_budget" : "flat_thermal_bound_adu",
              "assente: le due dichiarazioni termiche vanno insieme");
  in_domain(d.flat_thermal_bound_adu, "flat_thermal_bound_adu", param_domain::nonnegative);
  in_domain(d.flat_thermal_rel_budget, "flat_thermal_rel_budget", param_domain::open_unit);
}

// ---------------------------------------------------------------- aritmetica esatta del controllo termico

// Intero senza segno a parole di 32 bit, dalla meno significativa. 768 bit bastano: i prodotti del controllo termico
// hanno al più 160 bit (tre mantisse di 53 bit, o una mantissa per (2^53 + 1)^2) e l'allineamento di due valori della
// stessa lunghezza in bit sposta al più di quella lunghezza.
class wide_uint {
 public:
  static constexpr std::size_t words = 24;

  wide_uint() = default;
  explicit wide_uint(std::uint64_t v) {
    w_[0] = static_cast<std::uint32_t>(v);
    w_[1] = static_cast<std::uint32_t>(v >> 32);
  }

  int bit_length() const {
    for (std::size_t i = words; i-- > 0;) {
      if (w_[i] != 0) return static_cast<int>(i * 32) + 32 - std::countl_zero(w_[i]);
    }
    return 0;
  }

  wide_uint times(const wide_uint& o) const {
    ASTRO_ASSERT(bit_length() + o.bit_length() <= static_cast<int>(words * 32));
    wide_uint r;
    for (std::size_t i = 0; i < words; ++i) {
      std::uint64_t carry = 0;
      for (std::size_t j = 0; i + j < words; ++j) {
        const std::uint64_t t = std::uint64_t{w_[i]} * o.w_[j] + r.w_[i + j] + carry;
        r.w_[i + j] = static_cast<std::uint32_t>(t);
        carry = t >> 32;
      }
    }
    return r;
  }

  wide_uint shifted_left(int s) const {
    ASTRO_ASSERT(s >= 0 && bit_length() + s <= static_cast<int>(words * 32));
    wide_uint r;
    const std::size_t q = static_cast<std::size_t>(s) / 32;
    const unsigned b = static_cast<unsigned>(s) % 32;
    for (std::size_t i = words; i-- > q;) {
      std::uint64_t v = std::uint64_t{w_[i - q]} << b;
      if (b != 0 && i - q > 0) v |= std::uint64_t{w_[i - q - 1]} >> (32 - b);
      r.w_[i] = static_cast<std::uint32_t>(v);
    }
    return r;
  }

  friend int compare(const wide_uint& a, const wide_uint& b) {
    for (std::size_t i = words; i-- > 0;) {
      if (a.w_[i] != b.w_[i]) return a.w_[i] < b.w_[i] ? -1 : 1;
    }
    return 0;
  }

 private:
  std::array<std::uint32_t, words> w_{};
};

// Valore esatto `m · 2^e`.
struct exact_value {
  wide_uint m;
  int e = 0;
};

// Un double finito e non negativo è `m · 2^(e − 53)` con `m` intero di al più 53 bit (frexp, anche per i subnormali).
exact_value exact_of(double x) {
  ASTRO_ASSERT(std::isfinite(x) && x >= 0);
  if (x == 0) return {};
  int e = 0;
  const double f = std::frexp(x, &e);
  return {wide_uint(static_cast<std::uint64_t>(std::ldexp(f, 53))), e - 53};
}

exact_value operator*(const exact_value& a, const exact_value& b) { return {a.m.times(b.m), a.e + b.e}; }

int compare(const exact_value& a, const exact_value& b) {
  const int la = a.m.bit_length(), lb = b.m.bit_length();
  if (la == 0 || lb == 0) return la == lb ? 0 : (la == 0 ? -1 : 1);
  if (la + a.e != lb + b.e) return la + a.e < lb + b.e ? -1 : 1;
  return a.e >= b.e ? compare(a.m.shifted_left(a.e - b.e), b.m) : compare(a.m, b.m.shifted_left(b.e - a.e));
}

// Vista di un ingresso per la regola dei pixel validi per N di ADR-16.a §6 (`valid_for_n`).
calib_detail::input_view view_of(const image& im) {
  calib_detail::input_view v;
  v.samples = im.samples.data();
  v.mask = im.invalid.bits.data();
  v.f32 = im.type == sample_type::f32;
  if (im.metadata.saturation_adu) {
    v.has_saturation = true;
    v.saturation = im.metadata.saturation_adu->value;
  }
  return v;
}

// Controllo termico di `flat.bias` (§b.5): per ogni canale di normalizzazione `c`, con `N_c` = mediana f64 di
// `Fc = fl(F − B)` sui pixel validi per N, la catena resta ammessa se e solo se, in aritmetica esatta sui valori f64,
// `bound ≤ budget · q_min · N_c` e `bound · (1 + u)^2 < q_min · N_c` (cioè `β⁺_c < 1`, `u = 2^-53`). Un canale senza
// pixel validi, o con `N_c` non positiva o non finita, esclude la catena: `β_c` non è definito o il limite di §b.5 non
// vale (con `N_c ≤ 0` le due disuguaglianze di esclusione sono vere). L'esclusione è della sola catena (§b.1, §b.3).
bool flat_bias_within_budget(const image& flat, const image& bias, const calibration_declarations& d) {
  if (!d.normalization) bad_param("normalization", "assente: richiesta dal controllo termico di flat.bias");
  if (!d.q_min) bad_param("q_min", "assente: richiesto dal controllo termico di flat.bias");
  const bool per_channel = *d.normalization == normalization_mode::cfa_channel_median;
  if (per_channel && (flat.layout != plane_layout::cfa || !flat.cfa))
    stop(error_category::invalid_input, "calib.normalization_requires_cfa", "cfa_channel_median richiede un flat CFA",
         {{"role", "flat"}, {"layout", calib_detail::layout_name(flat.layout)}});
  validate_image(flat);
  validate_image(bias);
  // La coppia F–B ammessa ha larghezza e altezza uguali; layout mono o cfa: un piano.
  ASTRO_ASSERT(flat.geometry.width == bias.geometry.width && flat.geometry.height == bias.geometry.height);
  const calib_detail::input_view f = view_of(flat), b = view_of(bias);
  const std::uint32_t w = flat.geometry.width, h = flat.geometry.height;
  const std::uint32_t cx = per_channel ? flat.cfa->x_offset : 0, cy = per_channel ? flat.cfa->y_offset : 0;
  std::array<std::vector<double>, 4> fc;
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t i = std::size_t{y} * w + x;
      if (!calib_detail::valid_for_n(f, b, i)) continue;
      const std::size_t c = per_channel ? ((y + cy) & 1u) * 2 + ((x + cx) & 1u) : 0;
      fc[c].push_back(f.value(i) - b.value(i));
    }
  }
  const exact_value bound = exact_of(*d.flat_thermal_bound_adu);
  const exact_value budget_q = exact_of(*d.flat_thermal_rel_budget) * exact_of(*d.q_min);
  const wide_uint two53_plus_one((std::uint64_t{1} << 53) + 1);
  const exact_value bound_plus{bound.m.times(two53_plus_one).times(two53_plus_one), bound.e - 106};
  bool within = true;
  for (std::size_t c = 0; c < (per_channel ? 4u : 1u); ++c) {
    if (fc[c].empty()) {
      within = false;
      continue;
    }
    const double n = calib_detail::median_of(fc[c]);
    if (!std::isfinite(n) || !(n > 0)) {
      within = false;
      continue;
    }
    const exact_value q_n = exact_of(*d.q_min) * exact_of(n);
    if (compare(bound, budget_q * exact_of(n)) > 0 || compare(bound_plus, q_n) >= 0) within = false;
  }
  return within;
}

// ---------------------------------------------------------------- catene, insieme ammissibile e default

// Ruoli dei master di una catena, nell'ordine della formula di §b.5 (il flat prima degli altri ingressi del flat).
std::vector<master_role> members_of(light_chain c) {
  switch (c) {
    case light_chain::none: return {};
    case light_chain::bias: return {master_role::bias};
    case light_chain::dark: return {master_role::dark};
    case light_chain::bias_dark_sub: return {master_role::bias, master_role::dark};
    case light_chain::dark_sub_partial: return {master_role::dark};
  }
  std::abort();
}

std::vector<master_role> members_of(flat_chain c) {
  switch (c) {
    case flat_chain::none: return {};
    case flat_chain::dark_flat: return {master_role::flat, master_role::dark_flat};
    case flat_chain::bias_dark_flat_sub: return {master_role::flat, master_role::bias, master_role::dark_flat};
    case flat_chain::bias: return {master_role::flat, master_role::bias};
    case flat_chain::normalize:
    case flat_chain::given_q: return {master_role::flat};
  }
  std::abort();
}

constexpr light_chain all_light[] = {light_chain::none, light_chain::bias, light_chain::dark, light_chain::bias_dark_sub,
                                     light_chain::dark_sub_partial};
constexpr flat_chain all_flat[] = {flat_chain::none, flat_chain::dark_flat, flat_chain::bias_dark_flat_sub,
                                   flat_chain::bias, flat_chain::normalize, flat_chain::given_q};

// Priorità di §b.6, dalla più alta. `flat.given_q` e `flat.normalize` si escludono per lo stato del flat.
constexpr light_chain light_priority[] = {light_chain::dark, light_chain::bias_dark_sub, light_chain::dark_sub_partial,
                                          light_chain::bias, light_chain::none};
constexpr flat_chain flat_priority[] = {flat_chain::given_q, flat_chain::normalize, flat_chain::dark_flat,
                                        flat_chain::bias_dark_flat_sub, flat_chain::bias, flat_chain::none};

offset_state offset_after(light_chain c, bool light_raw) {
  if (c == light_chain::bias || c == light_chain::dark || c == light_chain::bias_dark_sub) return offset_state::removed;
  return light_raw ? offset_state::known_not_removed : offset_state::unknown;
}

const candidate_analysis* find_role(const calibration_analysis& a, master_role r) {
  for (const candidate_analysis& c : a.candidates) {
    if (c.role == r) return &c;
  }
  return nullptr;
}

const chain_verdict* verdict_in(const candidate_analysis& c, const std::string& chain) {
  for (const chain_verdict& v : c.chains) {
    if (v.chain == chain) return &v;
  }
  return nullptr;
}

bool usable_in(const calibration_analysis& a, master_role r, const std::string& chain) {
  const candidate_analysis* c = find_role(a, r);
  if (!c) return false;
  const chain_verdict* v = verdict_in(*c, chain);
  return v && v->status == candidate_status::usable;
}

template <class Chain>
bool chain_admissible(const calibration_analysis& a, Chain c) {
  for (master_role r : members_of(c)) {
    if (!usable_in(a, r, to_string(c))) return false;
  }
  if constexpr (std::is_same_v<Chain, flat_chain>) {
    if (c == flat_chain::bias && !a.flat_thermal_within_budget) return false;
  }
  return true;
}

// Uso dei parametri da parte di una coppia di catene (§b.5, ADR-16.a §2, §b.7).
struct param_use {
  bool exposure, temperature, normalization, division, thermal, decoder;
};

param_use uses_of(light_chain lc, flat_chain fc, bool light_raw) {
  const bool dark = lc == light_chain::dark || lc == light_chain::bias_dark_sub || lc == light_chain::dark_sub_partial;
  const bool bias = lc == light_chain::bias || lc == light_chain::bias_dark_sub || fc == flat_chain::bias_dark_flat_sub ||
                    fc == flat_chain::bias;
  const bool dark_flat = fc == flat_chain::dark_flat || fc == flat_chain::bias_dark_flat_sub;
  const bool computes_n = fc == flat_chain::dark_flat || fc == flat_chain::bias_dark_flat_sub || fc == flat_chain::bias ||
                          fc == flat_chain::normalize;
  return {dark || dark_flat, dark || bias || dark_flat, computes_n, fc != flat_chain::none, fc == flat_chain::bias, light_raw};
}

// Ricetta con i parametri delle sue catene copiati dalle dichiarazioni; assente se ne manca uno (il default non inventa
// una dichiarazione).
std::optional<chain_recipe> recipe_from(const calibration_declarations& d, light_chain lc, flat_chain fc, bool light_raw) {
  const param_use u = uses_of(lc, fc, light_raw);
  chain_recipe r;
  r.light = lc;
  r.flat = fc;
  bool complete = true;
  const auto take = [&complete](bool used, auto& field, const auto& declared) {
    if (!used) return;
    if (!declared) complete = false;
    field = declared;
  };
  take(u.exposure, r.exposure_rel_tol, d.exposure_rel_tol);
  take(u.temperature, r.temp_policy, d.temp_policy);
  take(u.temperature, r.temperature_abs_tol_c, d.temperature_abs_tol_c);
  take(u.normalization, r.normalization, d.normalization);
  take(u.division, r.q_min, d.q_min);
  take(u.division, r.q_rel_sigma_max, d.q_rel_sigma_max);
  take(u.thermal, r.flat_thermal_bound_adu, d.flat_thermal_bound_adu);
  take(u.thermal, r.flat_thermal_rel_budget, d.flat_thermal_rel_budget);
  take(u.decoder, r.decoder_black_abs_tol_adu, d.decoder_black_abs_tol_adu);
  if (!complete) return std::nullopt;
  return r;
}

// Predicati, insieme ammissibile e default (§b.4–§b.6) dall'esito per catena dei candidati.
void complete_analysis(calibration_analysis& a, const candidate_work& dark_flat, const candidate_work& flat) {
  const auto status_of = [&a](master_role r) {
    const candidate_analysis* c = find_role(a, r);
    return c ? std::optional<candidate_status>(c->status) : std::nullopt;
  };
  const bool x = status_of(master_role::dark) == candidate_status::usable;
  const bool y = status_of(master_role::flat) == candidate_status::usable;
  // z: una catena conta solo se i suoi membri sono tutti ammessi; senza flat ammesso le catene del flat non esistono.
  const bool z = usable_in(a, master_role::bias, "light.bias") || usable_in(a, master_role::bias, "light.bias_dark_sub") ||
                 (y && (usable_in(a, master_role::bias, "flat.bias_dark_flat_sub") ||
                        usable_in(a, master_role::bias, "flat.bias")));
  a.predicates.dfb = std::string("P-DFB") + (x ? '1' : '0') + (y ? '1' : '0') + (z ? '1' : '0');
  if (y) a.predicates.flat_available = flat.c->descriptor.flat;
  if (const std::optional<candidate_status> df = status_of(master_role::dark_flat)) {
    if (*df == candidate_status::excluded) a.predicates.df = dark_flat_predicate::bad;
    else
      a.predicates.df = dark_flat.c->descriptor.additive == additive_state::bias_subtracted ? dark_flat_predicate::sub
                                                                                             : dark_flat_predicate::inc;
  }

  for (light_chain lc : all_light) {
    if (!chain_admissible(a, lc)) continue;
    const offset_state off = offset_after(lc, a.light_has_decoder_state);
    for (flat_chain fc : all_flat) {
      if (!chain_admissible(a, fc)) continue;
      if (fc == flat_chain::none || off == offset_state::removed) a.admissible.push_back({lc, fc, false});
      else if (off == offset_state::known_not_removed) a.admissible.push_back({lc, fc, true});
    }
  }

  const calibration_declarations& d = a.declarations;
  const bool raw = a.light_has_decoder_state;
  light_chain light = light_chain::none;
  for (light_chain lc : light_priority) {
    // Le dichiarazioni delle catene del light sono già obbligatorie nell'analisi: qui `recipe_from` non esclude mai una
    // catena ammissibile, e il controllo resta per simmetria col ciclo del flat.
    if (chain_admissible(a, lc) && recipe_from(d, lc, flat_chain::none, raw)) {
      light = lc;
      break;
    }
  }
  flat_chain chosen = flat_chain::none;
  if (offset_after(light, raw) == offset_state::removed) {
    for (flat_chain fc : flat_priority) {
      if (chain_admissible(a, fc) && recipe_from(d, light, fc, raw)) {
        chosen = fc;
        break;
      }
    }
  }
  const std::optional<chain_recipe> recipe = recipe_from(d, light, chosen, raw);
  // `light.none` + `flat.none` usa solo la tolleranza del nero, obbligatoria per un light con stato del decoder.
  ASTRO_ASSERT(recipe.has_value());
  a.default_recipe = *recipe;
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

declaration_needs declaration_needs_of(bool dark, bool bias, bool dark_flat, bool flat, bool any_raw) noexcept {
  declaration_needs n;
  n.exposure = dark || (dark_flat && flat);
  n.temperature = dark || bias || (dark_flat && flat);
  n.decoder = any_raw;
  return n;
}

std::optional<std::string> pair_compatibility(pair_kind kind, const pair_side& reference, const pair_side& candidate,
                                              const calibration_declarations& decl, std::vector<std::string>& warnings) {
  ASTRO_ASSERT(reference.img != nullptr && candidate.img != nullptr);
  const pair_rule rule = rule_of(kind);
  if (rule.exposure) check_nonnegative_param(decl.exposure_rel_tol, "exposure_rel_tol");
  if (rule.additive) require_temperature_declarations(decl);
  const bool ref_raw = reference.states.has_value(), cand_raw = candidate.states.has_value();
  if (ref_raw && cand_raw && !decl.decoder_black_abs_tol_adu)
    stop(error_category::invalid_input, "calib.param.decoder_black_abs_tol_adu",
         "tolleranza del nero obbligatoria con un ingresso con stato del decoder", {{"parametro", "decoder_black_abs_tol_adu"}});
  const auto sides = [](const pair_side& side) {
    std::vector<const decoder_state*> ptrs;
    if (side.states) {
      for (const decoder_state& s : *side.states) ptrs.push_back(&s);
    }
    return states_of(ptrs);
  };
  return pair_outcome(rule, *reference.img, ref_raw, sides(reference), *candidate.img, cand_raw, sides(candidate), decl,
                      warnings);
}

std::optional<std::string> candidate_own_reason(const analysis_candidate& c, bool shared, bool light_raw,
                                                std::vector<std::string>& warnings) {
  const master_descriptor& d = c.descriptor;
  if (reason r = state_reason(d)) return r;
  if (reason r = domain_reason(c)) return r;
  if (imagetyp_conflicts(c.img, role_text(d.role), warnings, role_text(d.role))) return "imagetyp_conflict";
  if (shared) return "shared_input";
  if (c.origin_reason) return c.origin_reason;
  // Elenco vuoto prima del confronto col light: lo stato di un master RAW non si presume (ADR-16.b §b.7).
  if (d.frame_decoder_states && d.frame_decoder_states->empty()) return "metadata_missing.frame_decoder_states";
  if (d.frame_decoder_states.has_value() != light_raw) return "decoder_state_mixed";
  return std::nullopt;
}

std::optional<stop_error> light_domain_stop(const image& light, std::vector<std::string>& warnings) {
  try {
    require_light_domain(light, warnings);
  } catch (const stop_error& e) {
    return e;
  }
  return std::nullopt;
}

std::optional<chain_recipe> recipe_of_pair(const calibration_analysis& a, const admissible_pair& p) {
  std::optional<chain_recipe> r = recipe_from(a.declarations, p.light, p.flat, a.light_has_decoder_state);
  if (r && p.partial) r->flat_division_offset = offset_state::known_not_removed;
  return r;
}

bool recipe_uses_role(const chain_recipe& r, master_role role) noexcept {
  const auto in = [role](const std::vector<master_role>& v) { return std::find(v.begin(), v.end(), role) != v.end(); };
  return in(members_of(r.light)) || in(members_of(r.flat));
}

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
  const declaration_needs needs = declaration_needs_of(dark.c, bias.c, dark_flat.c, flat.c, any_raw);
  if (needs.exposure) check_nonnegative_param(decl.exposure_rel_tol, "exposure_rel_tol");
  if (needs.temperature) require_temperature_declarations(decl);
  check_optional_declarations(decl);
  // Esiti per candidato (§b.2, C-01.b §2, §7, §b.7), nell'ordine: stato, dominio, IMAGETYP, ingresso condiviso, stato
  // del decoder.
  for (int k = 0; k < 4; ++k) {
    candidate_work& w = work[k];
    if (!w.c) continue;
    bool shared = w.c->digest == in.light_digest;
    for (int j = 0; j < 4; ++j) shared = shared || (j != k && work[j].c && work[j].c->digest == w.c->digest);
    w.own = candidate_own_reason(*w.c, shared, light_raw, warnings);
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
    return pair_outcome(rule, ref, ref_raw, ref_decoder, owner.c->img, owner.raw, owner.decoder, decl, warnings);
  };
  if (bias.c) bias.light_pair = pair(bias, in.light, light_raw, light_decoder, {"L-B", true, false});
  if (dark.c) dark.light_pair = pair(dark, in.light, light_raw, light_decoder, {"L-D", true, true});
  if (flat.c) flat.light_pair = pair(flat, in.light, light_raw, light_decoder, {"L-F", false, false});
  if (flat_pairs) {
    const image& f = flat.c->img;
    if (dark_flat.c) dark_flat.flat_pair = pair(dark_flat, f, flat.raw, flat.decoder, {"F-DF", true, true});
    if (bias.c) bias.flat_pair = pair(bias, f, flat.raw, flat.decoder, {"F-B", true, false});
  }

  // Controllo termico di `flat.bias`, solo con le dichiarazioni e con flat `raw` e bias ammessi nella coppia F–B.
  bool thermal_within = false;
  if (decl.flat_thermal_bound_adu && flat.c && !flat.own && !flat.light_pair && flat.c->descriptor.flat == flat_state::raw &&
      bias.c && !bias.own && !bias.flat_pair) {
    probe("calib.analysis.samples");
    const image flat_samples = flat.c->load_samples ? flat.c->load_samples() : image();
    const image bias_samples = bias.c->load_samples ? bias.c->load_samples() : image();
    thermal_within = flat_bias_within_budget(flat.c->load_samples ? flat_samples : flat.c->img,
                                             bias.c->load_samples ? bias_samples : bias.c->img, decl);
    if (!thermal_within) flat.thermal = "flat_thermal_budget";
  }

  calibration_analysis out;
  for (const candidate_work& w : work) {
    if (!w.c) continue;
    candidate_analysis ca;
    ca.role = w.c->descriptor.role;
    for (const std::string& chain : w.chains) {
      const bool flat_chain = is_flat_chain(chain) && ca.role != master_role::flat;
      // Senza flat candidato le catene del flat del bias non esistono (§b.4).
      if (flat_chain && ca.role == master_role::bias && !flat.c) continue;
      reason r = w.own ? w.own : flat_chain ? w.flat_pair : w.light_pair;
      if (!r && ca.role == master_role::flat && chain == "flat.bias") r = w.thermal;
      ca.chains.push_back(chain_verdict{chain, r ? candidate_status::excluded : candidate_status::usable, r});
      if (!r) ca.status = candidate_status::usable;
    }
    out.candidates.push_back(std::move(ca));
  }
  out.warnings = std::move(warnings);
  out.flat_thermal_within_budget = thermal_within;
  out.light_has_decoder_state = light_raw;
  out.declarations = decl;
  complete_analysis(out, dark_flat, flat);
  return out;
}

// ---------------------------------------------------------------- selezione (§b.1 punto 2, §b.9)

bool p_flat_raw(const chain_recipe& r) noexcept {
  return r.flat == flat_chain::dark_flat || r.flat == flat_chain::bias_dark_flat_sub || r.flat == flat_chain::bias;
}

bool p_flat_q(const chain_recipe& r) noexcept { return r.flat == flat_chain::given_q; }

bool p_offset(const chain_recipe& r) noexcept { return r.flat != flat_chain::none; }

namespace {

[[noreturn]] void not_admissible(const std::string& why, const std::string& chain, const char* role) {
  stop(error_category::invalid_input, "calib.recipe_not_admissible", "ricetta fuori dall'insieme ammissibile",
       {{"reason", why}, {"chain", chain}, {"role", role}});
}

bool has_chain(const candidate_analysis& c, const char* chain) { return verdict_in(c, chain) != nullptr; }

// Motivo di §b.10 per una catena che lo stato del master non accetta: lo stato si legge dalle catene del candidato
// (§b.5: `D` → `light.dark`, `D'` → le due catene di `D'`; `DF` / `DF'`; flat `raw`, `corrected_unnormalized`,
// `calibrated`). Le combinazioni senza un motivo proprio nella lista chiusa sono `master_excluded`.
std::string incoherent_reason(const candidate_analysis& c, const std::string& chain) {
  switch (c.role) {
    case master_role::dark:
      if (chain == "light.dark") return "light_offset_not_removed";   // L − D' lascia l'offset
      if (chain == "light.bias_dark_sub") return "double_offset";     // L − B − D
      return "master_excluded";
    case master_role::dark_flat:
      if (chain == "flat.dark_flat") return "dark_flat_sub_requires_bias";  // F − DF' lascia B_F
      return "double_offset";                                               // F − B − DF
    case master_role::flat: {
      const bool raw = has_chain(c, "flat.dark_flat");
      const bool corrected = has_chain(c, "flat.normalize");
      if (chain == "flat.normalize" || chain == "flat.given_q") {
        if (raw) return "flat_offset_not_removable";
        return corrected ? "master_excluded" : "flat_already_corrected";
      }
      return "flat_already_corrected";
    }
    case master_role::bias: return "master_excluded";
  }
  std::abort();
}

bool has_reason(const candidate_analysis& c, const char* why) {
  return std::any_of(c.chains.begin(), c.chains.end(), [why](const chain_verdict& v) { return v.reason == why; });
}

template <class Chain>
void require_chain(const calibration_analysis& a, Chain chain) {
  const std::string name = to_string(chain);
  for (master_role r : members_of(chain)) {
    const char* role = role_text(r);
    const candidate_analysis* c = find_role(a, r);
    if (!c) not_admissible("master_absent", name, role);
    if (has_reason(*c, "state_unknown"))
      stop(error_category::metadata_missing, "calib.master_state_unknown", "master di stato unknown nella ricetta",
           {{"chain", name}, {"role", role}});
    if (has_reason(*c, "state_scaled"))
      stop(error_category::unsupported, "calib.master_state_scaled", "master scaled nella ricetta (F-CAL-05)",
           {{"chain", name}, {"role", role}});
    const chain_verdict* v = verdict_in(*c, name);
    if (!v) not_admissible(incoherent_reason(*c, name), name, role);
    if (v->status == candidate_status::excluded) not_admissible(*v->reason, name, role);
  }
  if constexpr (std::is_same_v<Chain, flat_chain>) {
    if (chain == flat_chain::bias && !a.flat_thermal_within_budget)
      not_admissible("flat_thermal_not_declared", name, "flat");
  }
}

// Parametro di una catena: presente se e solo se la ricetta lo usa, nel dominio, uguale alla dichiarazione
// dell'analisi se questa l'ha.
template <class T>
void require_param(const char* name, bool used, const std::optional<T>& value, const std::optional<T>& declared,
                   std::optional<param_domain> dom = std::nullopt) {
  if (!used) {
    if (value)
      stop(error_category::invalid_input, std::string("calib.param_not_applicable.") + name,
           "parametro di una catena non selezionata", {{"parametro", name}});
    return;
  }
  if (!value) bad_param(name, "assente");
  if constexpr (std::is_same_v<T, double>) {
    if (dom) {
      if (const char* why = domain_violation(*value, *dom)) bad_param(name, why);
    }
  }
  if (declared && !(*value == *declared)) bad_param(name, "diverso dalla dichiarazione dell'analisi");
}

}  // namespace

void require_admissible(const calibration_analysis& a, const chain_recipe& r) {
  require_chain(a, r.light);
  require_chain(a, r.flat);

  // G-OFFSET (P-OFFSET, §b.5): una divisione per Q richiede l'offset del light rimosso o il percorso parziale dichiarato.
  const bool raw = a.light_has_decoder_state;
  const offset_state off = offset_after(r.light, raw);
  const std::string flat_name = to_string(r.flat);
  if (r.flat == flat_chain::none || off == offset_state::removed) {
    if (r.flat_division_offset)
      stop(error_category::invalid_input, "calib.param_not_applicable.flat_division_offset",
           "flat_division_offset senza percorso parziale", {{"parametro", "flat_division_offset"}, {"chain", flat_name}});
  } else if (off == offset_state::unknown || !r.flat_division_offset) {
    stop(error_category::invalid_input, "calib.recipe_not_admissible", "divisione per Q con offset del light non rimosso",
         {{"reason", "light_offset_not_removed"}, {"chain", flat_name}, {"light_offset", to_string(off)}});
  } else if (*r.flat_division_offset != offset_state::known_not_removed) {
    bad_param("flat_division_offset", "il percorso parziale si dichiara con known_not_removed");
  }

  const param_use u = uses_of(r.light, r.flat, raw);
  const calibration_declarations& d = a.declarations;
  require_param("exposure_rel_tol", u.exposure, r.exposure_rel_tol, d.exposure_rel_tol, param_domain::nonnegative);
  require_param("temperature_policy", u.temperature, r.temp_policy, d.temp_policy);
  require_param("temperature_abs_tol_c", u.temperature, r.temperature_abs_tol_c, d.temperature_abs_tol_c,
                param_domain::nonnegative);
  require_param("normalization", u.normalization, r.normalization, d.normalization);
  require_param("q_min", u.division, r.q_min, d.q_min, param_domain::positive);
  require_param("q_rel_sigma_max", u.division, r.q_rel_sigma_max, d.q_rel_sigma_max, param_domain::positive);
  require_param("flat_thermal_bound_adu", u.thermal, r.flat_thermal_bound_adu, d.flat_thermal_bound_adu,
                param_domain::nonnegative);
  require_param("flat_thermal_rel_budget", u.thermal, r.flat_thermal_rel_budget, d.flat_thermal_rel_budget,
                param_domain::open_unit);
  require_param("decoder_black_abs_tol_adu", u.decoder, r.decoder_black_abs_tol_adu, d.decoder_black_abs_tol_adu);
}

// ---------------------------------------------------------------- selezione congelata (F-IO-03 CL-11)

namespace {

[[noreturn]] void decoder_changed(const std::string& field, const std::string& name) {
  stop(error_category::integrity, "calib.decoder_state_changed", "stato o identità del decoder diversi da quelli della selezione",
       {{"field", field}, {"predicate", "P-LOST"}, {"prerequisite", name + ".decoder_state"}});
}

// Primo membro di contenuto diverso nell'ordine di dichiarazione di `decoder_state`; vuoto se uguali (`source` esclusa).
std::string first_state_difference(const decoder_state& a, const decoder_state& b) {
  const auto same = [&a, &b](const auto decoder_state::*member) { return a.*member == b.*member; };
  if (!same(&decoder_state::raw_width)) return "raw_width";
  if (!same(&decoder_state::raw_height)) return "raw_height";
  if (!same(&decoder_state::window)) return "window";
  if (!same(&decoder_state::black)) return "black";
  if (!same(&decoder_state::cfa_color_index)) return "cfa_color_index";
  if (!same(&decoder_state::black_per_color)) return "black_per_color";
  if (!same(&decoder_state::black_pattern_rows)) return "black_pattern_rows";
  if (!same(&decoder_state::black_pattern_cols)) return "black_pattern_cols";
  if (!same(&decoder_state::black_pattern)) return "black_pattern";
  if (!same(&decoder_state::white)) return "white";
  if (!same(&decoder_state::data_maximum)) return "data_maximum";
  if (!same(&decoder_state::linear_max)) return "linear_max";
  if (!same(&decoder_state::linearization)) return "linearization";
  if (!same(&decoder_state::curve_sha256)) return "curve_sha256";
  if (!same(&decoder_state::process_warnings)) return "process_warnings";
  if (!same(&decoder_state::raw_inset_crops)) return "raw_inset_crops";
  if (!same(&decoder_state::flip)) return "flip";
  if (!same(&decoder_state::route)) return "route";
  if (!same(&decoder_state::dng)) return "dng";
  ASTRO_ASSERT(a == b);
  return {};
}

std::string first_identity_difference(const decoder_identity& a, const decoder_identity& b) {
  if (a.libraw_version != b.libraw_version) return "decoder_identity.libraw_version";
  if (a.libraw_version_number != b.libraw_version_number) return "decoder_identity.libraw_version_number";
  if (a.libraw_capabilities != b.libraw_capabilities) return "decoder_identity.libraw_capabilities";
  if (a.libraw_tarball_sha256 != b.libraw_tarball_sha256) return "decoder_identity.libraw_tarball_sha256";
  return {};
}

}  // namespace

void verify_selection(const calibration_selection& selection, const std::vector<selected_decoder_input>& observed) {
  for (const selected_decoder_input& frozen : selection.raw_inputs) {
    const auto now = std::find_if(observed.begin(), observed.end(),
                                  [&frozen](const selected_decoder_input& o) { return o.name == frozen.name; });
    if (now == observed.end()) decoder_changed("observation", frozen.name);
    if (std::string field = first_state_difference(frozen.state, now->state); !field.empty()) decoder_changed(field, frozen.name);
    if (std::string field = first_identity_difference(frozen.identity, now->identity); !field.empty()) {
      decoder_changed(field, frozen.name);
    }
  }
}

}  // namespace astro
