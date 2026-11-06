#include "astro/operations.hpp"

#include <cstdlib>

#include "astro/error.hpp"

namespace astro {

const char* to_string(operation_kind kind) noexcept {
  switch (kind) {
    case operation_kind::inventory: return "inventory";
    case operation_kind::preview: return "preview";
    case operation_kind::export_bundle: return "export_bundle";
    case operation_kind::master_create: return "master_create";
    case operation_kind::calibrate: return "calibrate";
    case operation_kind::linear_ancestor_required: return "linear_ancestor_required";
    case operation_kind::calibrate_light: return "calibrate_light";
    case operation_kind::calibrate_master_raw: return "calibrate_master_raw";
    case operation_kind::calibrate_master_prepared: return "calibrate_master_prepared";
  }
  std::abort();
}

const char* to_string(process_phase phase) noexcept {
  switch (phase) {
    case process_phase::raw: return "raw";
    case process_phase::calibrated: return "calibrated";
    case process_phase::prestacked: return "prestacked";
  }
  std::abort();
}

const char* to_string(linearity_class lin) noexcept {
  switch (lin) {
    case linearity_class::linear: return "linear";
    case linearity_class::nonlinear: return "nonlinear";
    case linearity_class::unknown: return "unknown";
  }
  std::abort();
}

linearity_class classify_linearity(const std::optional<linearity>& lin) noexcept {
  if (!lin) return linearity_class::unknown;
  return *lin == linearity::linear ? linearity_class::linear : linearity_class::nonlinear;
}

std::string blocked_code(operation_kind kind, const process_state& state) {
  switch (kind) {
    case operation_kind::inventory:
    case operation_kind::preview:
    case operation_kind::export_bundle: return "";
    case operation_kind::master_create:
    case operation_kind::calibrate:
    case operation_kind::calibrate_light:
    case operation_kind::calibrate_master_raw:
      if (state.phase != process_phase::raw) return "op.phase_blocked";
      if (state.lin != linearity_class::linear) return "op.linearity_blocked";
      return "";
    case operation_kind::calibrate_master_prepared:
      if (state.phase != process_phase::raw && state.phase != process_phase::calibrated) return "op.phase_blocked";
      if (state.lin != linearity_class::linear) return "op.linearity_blocked";
      return "";
    case operation_kind::linear_ancestor_required:
      return state.lin == linearity_class::linear ? "" : "op.linear_ancestor_missing";
  }
  std::abort();
}

namespace {

void require_allowed(operation_kind kind, const process_state& state, stop_error::context_list extra) {
  const std::string code = blocked_code(kind, state);
  if (code.empty()) return;
  stop_error::context_list context = {
      {"operazione", to_string(kind)}, {"fase", to_string(state.phase)}, {"linearita", to_string(state.lin)}};
  for (auto& kv : extra) context.push_back(std::move(kv));
  throw stop_error(error_category::incompatible, code, "operazione non ammessa per la fase o la linearità dell'input",
                   std::move(context));
}

}  // namespace

void require_operation_allowed(operation_kind kind, const process_state& state) { require_allowed(kind, state, {}); }

void require_operation_allowed(operation_kind kind, const process_state& state, std::string_view role) {
  require_allowed(kind, state, {{"role", std::string(role)}});
}

std::vector<preflight_entry> preflight_table(const process_state& state) {
  std::vector<preflight_entry> out;
  for (const operation_kind kind :
       {operation_kind::inventory, operation_kind::preview, operation_kind::export_bundle, operation_kind::master_create,
        operation_kind::calibrate, operation_kind::linear_ancestor_required}) {
    const std::string code = blocked_code(kind, state);
    out.push_back(preflight_entry{kind, code.empty(), code});
  }
  return out;
}

}  // namespace astro
