#include <algorithm>
#include <array>
#include <sstream>

#include "astro/calibration.hpp"
#include "calibration_internal.hpp"

namespace astro {

namespace {

constexpr std::array<const char*, 9> bit_names = {"source_nonfinite", "source_flagged", "saturated",
                                                  "master_invalid",   "q_nonpositive",  "q_nonfinite",
                                                  "q_below_min",      "q_unreliable",   "result_nonfinite"};

void render(std::ostringstream& out, const json& value, int depth, const std::string& key) {
  const std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
  switch (value.type()) {
    case json::kind::object:
      if (!key.empty()) out << pad << key << ":\n";
      {
        // Ordine canonico delle chiavi: lo stesso del JSON serializzato.
        std::vector<std::size_t> order(value.keys().size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return value.keys()[a] < value.keys()[b]; });
        for (const std::size_t i : order) render(out, value.items()[i], key.empty() ? depth : depth + 1, value.keys()[i]);
      }
      return;
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

}  // namespace

std::string calibration_report_text(const json& report) {
  std::ostringstream out;
  out << "Rapporto di calibrazione\n";
  render(out, report, 0, "");
  return out.str();
}

json calib_detail::calibration_report(const prepared_calibration::impl& p, const std::array<std::uint64_t, 9>& counts,
                                      std::uint64_t invalid_pixels) {
  const std::uint64_t total = static_cast<std::uint64_t>(p.w) * p.h;

  json mask = json::object();
  json by_bit = json::object();
  for (std::size_t b = 0; b < counts.size(); ++b) by_bit.set(bit_names[b], json::uinteger(counts[b]));
  mask.set("invalid_pixels", json::uinteger(invalid_pixels));
  mask.set("valid_pixels", json::uinteger(total - invalid_pixels));
  mask.set("pixels_with_bit", std::move(by_bit));

  json norm = json::array();
  for (const channel_normalization& c : p.channels) {
    json e = json::object();
    e.set("channel", json::string(c.label));
    e.set("N", json::number(c.n));
    e.set("n_valid", json::uinteger(c.n_valid));
    e.set("fc_nonpositive_in_N", json::uinteger(c.fc_nonpositive));
    e.set("normalization_rel_var_estimate", json::number(c.rel_var_estimate));
    e.set("estimate_kind", json::string("estimate_not_bound"));
    e.set("hypothesis", json::string("iid_gaussian_fc"));
    e.set("fc_cv", c.fc_cv ? json::number(*c.fc_cv) : json::null());
    e.set("iid_cv_threshold", json::number(calib_detail::iid_cv_threshold));
    e.set("iid_assumption", json::string(c.iid_not_verified ? "not_verified" : "cv_below_threshold"));
    norm.push(std::move(e));
  }

  const auto& inputs = p.report;
  json variance = json::object();
  variance.set("covariance_model", json::string("diagonal_independent"));
  variance.set("shot_estimator", json::string("observed_clamped"));
  variance.set("independence", json::string("distinct digests verified; absence of shared source frames not verified"));
  json shared = json::array();
  for (const char* name : {"dark", "dark_flat", "flat"}) {
    shared.push(json::string(inputs.find("inputs")->find(name)->find("digest_sha256")->as_string()));
  }
  variance.set("shared_master_digests", std::move(shared));

  json ledger = json::object();
  json entries = json::array();
  const auto entry = [&](const char* step, const char* status, json digest, const char* reason) {
    json e = json::object();
    e.set("step", json::string(step));
    e.set("status", json::string(status));
    e.set("master_digest", std::move(digest));
    e.set("reason", json::string(reason));
    entries.push(std::move(e));
  };
  const auto digest_of_input = [&](const char* name) {
    return json::string(inputs.find("inputs")->find(name)->find("digest_sha256")->as_string());
  };
  entry("dark_subtraction", "applied", digest_of_input("dark"), "dark_matched_bias_included");
  entry("dark_flat_subtraction", "applied", digest_of_input("dark_flat"), "dark_flat_matched_bias_included");
  entry("flat_division", "applied", digest_of_input("flat"), "flat_normalized_by_declared_N");
  entry("bias_subtraction", "not_applicable", json::null(), "bias_in_matched_dark");
  json ledger_warnings = json::array();
  for (const std::string& w : p.warnings) ledger_warnings.push(json::string(w));
  ledger.set("entries", std::move(entries));
  ledger.set("warnings", std::move(ledger_warnings));

  json report = json::object();
  for (const char* key : {"recipe", "inputs", "compatibility", "assumptions"}) report.set(key, *inputs.find(key));
  report.set("correction_ledger", std::move(ledger));
  report.set("normalization", std::move(norm));
  report.set("mask", std::move(mask));
  report.set("variance_model", std::move(variance));
  report.set("sample_type", json::string(p.out.type == sample_type::f32 ? "f32" : "f64"));
  report.set("width", json::uinteger(p.w));
  report.set("height", json::uinteger(p.h));
  return report;
}

calibration_result finish_calibration(prepared_calibration&& prepared) {
  prepared_calibration::impl& p = *prepared.impl_;
  std::array<std::uint64_t, 9> counts{};
  std::uint64_t invalid_pixels = 0;
  for (const std::uint16_t bits : p.out.invalid.bits) {
    if (bits != 0) ++invalid_pixels;
    for (std::size_t b = 0; b < counts.size(); ++b) {
      if ((bits >> b) & 1u) ++counts[b];
    }
  }
  json report = calib_detail::calibration_report(p, counts, invalid_pixels);

  calibration_result result;
  result.report_text = calibration_report_text(report);
  result.report = std::move(report);
  result.warnings = p.warnings;
  result.output = std::move(p.out);
  return result;
}

}  // namespace astro
