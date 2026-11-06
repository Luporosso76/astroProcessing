#include "astro/calibration_analysis.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "astro/error.hpp"

namespace astro {

namespace {

[[noreturn]] void stop(error_category category, const char* code, const std::string& message,
                       stop_error::context_list context = {}) {
  throw stop_error(category, code, message, std::move(context));
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
  return "light.unknown";
}

const char* to_string(offset_state state) noexcept {
  switch (state) {
    case offset_state::removed: return "removed";
    case offset_state::known_not_removed: return "known_not_removed";
    case offset_state::unknown: return "unknown";
  }
  return "unknown_state";
}

const char* to_string(black_applied_by_decoder applied) noexcept {
  switch (applied) {
    case black_applied_by_decoder::none: return "none";
  }
  return "unknown_black_applied";
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

}  // namespace astro
