#include "astro/master_analysis.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include "astro/error.hpp"
#include "astro/fits.hpp"
#include "astro/operations.hpp"
#include "calibration_internal.hpp"

namespace astro {

namespace {

using calib_detail::channel_labels;
using calib_detail::imagetyp_role;
using calib_detail::layout_name;
using calib_detail::median_of;
using calib_detail::num_text;
using calib_detail::role_name;

[[noreturn]] void throw_cancelled() {
  throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto");
}

bool starts_with(const std::string& s, std::string_view prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

// Errore di decodifica del frame, per codice: struttura o contenuto FITS non decodificabili (`fits.*`, `image.*`, compreso
// `fits.checksum`). Restano stop `input.*` (OB-11), I/O, budget (`sched.*`) e annullamento.
bool is_decode_error(const stop_error& e) { return starts_with(e.code(), "fits.") || starts_with(e.code(), "image."); }

master_frame_stats decode_failed(const stop_error& e) {
  master_frame_stats out;
  out.decode_failure = master_decode_failure{e.code(), e.what()};
  return out;
}

double value_at(const image& im, std::size_t i) {
  return im.type == sample_type::f32 ? static_cast<double>(im.data<float>()[i]) : im.data<double>()[i];
}

// Statistiche sull'intera immagine: campioni validi (bit di invalidità a zero) con `>= saturation_adu`; totale = tutti i
// pixel; m_(f,c) = mediana dei campioni validi del canale meno `request.offset_adu`, canali nell'ordine dei siti.
// I campioni non finiti hanno il bit di invalidità: lo marca `decode_fits`, il lettore non li ricontrolla.
master_frame_stats stats_of(const image& im, const master_stats_request& request) {
  master_frame_stats out;
  const std::uint32_t w = im.geometry.width;
  const std::uint32_t h = im.geometry.height;
  const std::size_t px = static_cast<std::size_t>(w) * h;
  out.pixel_count = px;
  if (request.saturation_adu) {
    for (std::size_t i = 0; i < px; ++i) {
      if (im.invalid.bits[i] == 0 && value_at(im, i) >= *request.saturation_adu) ++out.saturated_samples;
    }
  }
  const bool single_plane = im.layout == plane_layout::mono || im.layout == plane_layout::cfa;
  if (request.flat_levels && single_plane) {
    ASTRO_ASSERT(request.offset_adu.has_value());
    const double offset = *request.offset_adu;
    const std::size_t channels = im.cfa ? 4 : 1;
    const std::uint32_t cfa_x = im.cfa ? im.cfa->x_offset : 0;
    const std::uint32_t cfa_y = im.cfa ? im.cfa->y_offset : 0;
    std::array<std::vector<double>, 4> z;
    for (std::size_t c = 0; c < channels; ++c) z[c].reserve(px / channels + 1);
    for (std::uint32_t y = 0; y < h; ++y) {
      for (std::uint32_t x = 0; x < w; ++x) {
        const std::size_t i = static_cast<std::size_t>(y) * w + x;
        if (im.invalid.bits[i] != 0) continue;
        const std::size_t c = im.cfa ? static_cast<std::size_t>(((y + cfa_y) & 1u) * 2 + ((x + cfa_x) & 1u)) : 0;
        z[c].push_back(value_at(im, i) - offset);
      }
    }
    for (std::size_t c = 0; c < channels; ++c) {
      out.levels.push_back(z[c].empty() ? std::nullopt : std::optional<double>(median_of(z[c])));
    }
  }
  return out;
}

const char* unit_name(pixel_unit u) {
  switch (u) {
    case pixel_unit::adu: return "adu";
    case pixel_unit::electron: return "electron";
    case pixel_unit::dimensionless: return "dimensionless";
  }
  ASTRO_ASSERT(false);
  return "";
}

json object_of(std::initializer_list<std::pair<const char*, json>> fields) {
  json o = json::object();
  for (const auto& [k, v] : fields) o.set(k, v);
  return o;
}

bool is_raw(const master_analysis_frame& f) { return f.observation.observed_image().decoder_state.has_value(); }

// Parametri richiesti dal ruolo e assenti (L4). Ordine fisso.
std::vector<std::string> missing_params_of(master_role role, bool any_raw, const master_analysis_params& p) {
  std::vector<std::string> missing;
  const auto need = [&missing](bool present, const char* name) {
    if (!present) missing.push_back(name);
  };
  need(p.exposure_rel_tol.has_value(), "exposure_rel_tol");
  if (!p.temp_policy || *p.temp_policy == temperature_policy::compare) need(p.temperature_abs_tol_c.has_value(), "temperature_abs_tol_c");
  need(p.temp_policy.has_value(), "temperature_policy");
  if (p.saturation_adu) need(p.saturated_fraction_max.has_value(), "saturated_fraction_max");
  if (role == master_role::flat) {
    need(p.flat_min_signal_adu.has_value(), "flat_min_signal_adu");
    need(p.flat_level_rel_tol.has_value(), "flat_level_rel_tol");
  }
  if (any_raw) need(p.decoder_black_abs_tol_adu.has_value(), "decoder_black_abs_tol_adu");
  if (any_raw && role == master_role::flat) need(p.offset_adu.has_value(), "offset_adu");
  return missing;
}

struct reason {
  std::string name;
  json values;
};

// Primo motivo della lista chiusa di C-02.b §b.2 (punto 1), nel suo ordine; `first_copy` = primo digest nell'ordine canonico.
std::optional<reason> first_reason(master_role role, const master_analysis_frame& f, const master_analysis_params& p,
                                   const master_analysis_frame* first_copy) {
  const image& im = f.observation.observed_image();
  if (const auto& conflict = f.observation.declared_conflict()) {
    return reason{"declared_conflict", object_of({{"campo", json::string(*conflict)}})};
  }
  if (im.layout != plane_layout::mono && im.layout != plane_layout::cfa) {
    return reason{"layout", object_of({{"layout", json::string(layout_name(im.layout))}})};
  }
  if (!im.phase) return reason{"metadata_missing.phase", object_of({{"campo", json::string("phase")}})};
  if (!blocked_code(operation_kind::master_create, process_state{im.phase->value, linearity_class::linear}).empty()) {
    return reason{"phase", object_of({{"fase", json::string(to_string(im.phase->value))}})};
  }
  if (!im.lin) return reason{"metadata_missing.linearity", object_of({{"campo", json::string("linearity")}})};
  const linearity_class lin = classify_linearity(im.lin);
  if (lin != linearity_class::linear) return reason{"linearity", object_of({{"linearita", json::string(to_string(lin))}})};
  if (!im.unit) return reason{"metadata_missing.unit", object_of({{"campo", json::string("unit")}})};
  if (*im.unit != pixel_unit::adu) return reason{"unit", object_of({{"unita", json::string(unit_name(*im.unit))}})};
  if (im.metadata.pedestal_adu && im.metadata.pedestal_adu->value != 0.0) {
    return reason{"pedestal", object_of({{"pedestal_adu", json::number(im.metadata.pedestal_adu->value)}})};
  }
  if (im.variance) return reason{"input_variance", json::object()};

  const auto missing = [](const char* field) { return reason{std::string("metadata_missing.") + field, object_of({{"campo", json::string(field)}})}; };
  if (!im.metadata.camera) return missing("camera");
  if (!im.metadata.gain) return missing("gain");
  if (!im.metadata.offset_adu && !im.decoder_state) return missing("offset_adu");
  if (!im.metadata.exposure_s) return missing("exposure_s");
  if (*p.temp_policy == temperature_policy::compare && !im.metadata.temperature_c) return missing("temperature_c");
  if (role == master_role::flat && !im.metadata.filter) return missing("filter");
  if (!im.geometry.binning_x) return missing("binning_x");
  if (!im.geometry.binning_y) return missing("binning_y");

  if (im.metadata.imagetyp) {
    const std::optional<std::string> declared = imagetyp_role(im.metadata.imagetyp->value);
    const std::string expected = role_name(role);
    if (declared && !(*declared == expected || (role == master_role::dark_flat && *declared == "dark"))) {
      return reason{"imagetyp_conflict", object_of({{"imagetyp", json::string(im.metadata.imagetyp->value)},
                                                    {"ruolo_atteso", json::string(expected)}})};
    }
  }
  if (first_copy != nullptr && first_copy != &f) {
    return reason{"duplicate_digest", object_of({{"primo", json::uinteger(first_copy->input_id)}})};
  }
  if (f.stats && f.stats->decode_failure) {
    return reason{"decode_failed", object_of({{"codice", json::string(f.stats->decode_failure->code)},
                                              {"dettaglio", json::string(f.stats->decode_failure->detail)}})};
  }
  if (im.metadata.saturation_adu) {
    const double frame_threshold = im.metadata.saturation_adu->value;
    ASTRO_ASSERT(p.saturation_adu.has_value());  // altrimenti l'analisi si è già fermata
    if (frame_threshold != *p.saturation_adu) {
      return reason{"saturation_conflict", object_of({{"analisi", json::number(*p.saturation_adu)},
                                                      {"soglia_frame", json::number(frame_threshold)}})};
    }
  }
  if (p.saturation_adu) {
    ASTRO_ASSERT(f.stats.has_value() && f.stats->pixel_count > 0);
    const double fraction = static_cast<double>(f.stats->saturated_samples) / static_cast<double>(f.stats->pixel_count);
    if (fraction > *p.saturated_fraction_max) {
      return reason{"saturated_fraction", object_of({{"frazione", json::number(fraction)},
                                                     {"massimo", json::number(*p.saturated_fraction_max)},
                                                     {"saturi", json::uinteger(f.stats->saturated_samples)},
                                                     {"totale", json::uinteger(f.stats->pixel_count)}})};
    }
  }
  if (role == master_role::flat) {
    ASTRO_ASSERT(f.stats.has_value());
    const std::size_t channels = im.cfa ? 4 : 1;
    ASTRO_ASSERT(f.stats->levels.size() == channels);
    for (std::size_t c = 0; c < channels; ++c) {
      const std::optional<double>& level = f.stats->levels[c];
      const char* why = !level ? "assente" : !std::isfinite(*level) ? "non_finito" : *level < *p.flat_min_signal_adu ? "sotto_minimo" : nullptr;
      if (why == nullptr) continue;
      json values = json::object();
      if (im.cfa) values.set("canale", json::string(channel_labels(im.cfa->pattern)[c]));
      values.set("livello", level && std::isfinite(*level) ? json::number(*level) : json::null());
      values.set("minimo", json::number(*p.flat_min_signal_adu));
      values.set("motivo", json::string(why));
      return reason{"flat_low_signal", std::move(values)};
    }
  }
  return std::nullopt;
}

[[noreturn]] void bad_param(const char* name, const char* why) {
  throw stop_error(error_category::invalid_input, std::string("master.param.") + name,
                   "parametro d'analisi non finito o fuori dominio", {{"parametro", name}, {"motivo", why}});
}

void check_finite(double v, const char* name) {
  if (!std::isfinite(v)) bad_param(name, "non finito");
}

// Domini della ricetta del master (master.cpp), dopo `params_missing`: un parametro fuori dominio ferma l'analisi.
void check_param_domains(master_role role, bool any_raw, const master_analysis_params& p) {
  if (role == master_role::flat) {
    check_finite(*p.flat_level_rel_tol, "flat_level_rel_tol");
    if (!(*p.flat_level_rel_tol > 0 && *p.flat_level_rel_tol < 1)) bad_param("flat_level_rel_tol", "deve stare in (0, 1)");
    check_finite(*p.flat_min_signal_adu, "flat_min_signal_adu");
    if (!(*p.flat_min_signal_adu > 0)) bad_param("flat_min_signal_adu", "deve essere > 0");
    if (any_raw) check_finite(*p.offset_adu, "offset_adu");
  }
  if (p.saturation_adu) {
    check_finite(*p.saturation_adu, "saturation_adu");
    check_finite(*p.saturated_fraction_max, "saturated_fraction_max");
    if (!(*p.saturated_fraction_max >= 0 && *p.saturated_fraction_max <= 1)) bad_param("saturated_fraction_max", "deve stare in [0, 1]");
  }
  check_finite(*p.exposure_rel_tol, "exposure_rel_tol");
  if (*p.exposure_rel_tol < 0) bad_param("exposure_rel_tol", "deve essere >= 0");
  if (p.temperature_abs_tol_c) {
    check_finite(*p.temperature_abs_tol_c, "temperature_abs_tol_c");
    if (*p.temperature_abs_tol_c < 0) bad_param("temperature_abs_tol_c", "deve essere >= 0");
  }
}

bool canonical_before(const master_analysis_frame* a, const master_analysis_frame* b) {
  if (a->digest.sha256 != b->digest.sha256) return a->digest.sha256 < b->digest.sha256;
  if (a->digest.size_bytes != b->digest.size_bytes) return a->digest.size_bytes < b->digest.size_bytes;
  return a->input_id < b->input_id;
}

}  // namespace

master_frame_stats read_master_frame_stats(const std::string& path, const content_digest& digest, const master_stats_request& request,
                                           memory_pool& pool, const cancel_token& cancel) {
  if (cancel.requested()) throw_cancelled();
  const fits_input input = open_fits_input(path);
  std::optional<fits_plan> plan;
  try {
    plan = plan_fits(input);
  } catch (const stop_error& e) {
    if (!is_decode_error(e)) throw;
    // Un piano non leggibile è un'esclusione solo se il file è quello atteso: altrimenti è un'alterazione.
    reservation block = pool.reserve(sha256_chunk_bytes, "master.analysis.digest", &cancel);
    pool_buffer buffer(block, sha256_chunk_bytes);
    const content_digest found = digest_input(input, {buffer.data(), buffer.size()}, [&cancel] {
      if (cancel.requested()) throw_cancelled();
    });
    require_input_digest(found, digest, path);
    return decode_failed(e);
  }
  const std::vector<std::uint8_t> bytes = read_verified_input(input, path, digest, pool.budget_bytes(), &*plan);
  try {
    return stats_of(decode_fits(bytes, pool.budget_bytes()), request);
  } catch (const stop_error& e) {
    if (!is_decode_error(e)) throw;
    return decode_failed(e);
  }
}

master_screening screen_master_frames(master_role role, const std::vector<master_analysis_frame>& frames,
                                      const master_analysis_params& params, const fault_injector& fault) {
  master_screening out;
  const bool any_raw = std::any_of(frames.begin(), frames.end(), [](const master_analysis_frame& f) { return is_raw(f); });
  std::vector<const master_analysis_frame*> order;
  for (const master_analysis_frame& f : frames) order.push_back(&f);
  std::sort(order.begin(), order.end(), canonical_before);

  // Soglia di saturazione propria di un frame con `saturation_adu` assente: stop dell'analisi, su ogni frame e prima di
  // `params_missing` e delle esclusioni.
  if (!params.saturation_adu) {
    for (const master_analysis_frame* f : order) {
      const auto& threshold = f->observation.observed_image().metadata.saturation_adu;
      if (threshold) {
        throw stop_error(error_category::invalid_input, "master.param.saturation_adu",
                         "soglia di saturazione nel frame e assente dai parametri d'analisi",
                         {{"frame", f->digest.sha256}, {"valore", num_text(threshold->value)}});
      }
    }
  }
  out.missing_params = missing_params_of(role, any_raw, params);
  if (!out.missing_params.empty()) return out;
  check_param_domains(role, any_raw, params);

  json exclusions = json::array();
  const master_analysis_frame* first_copy = nullptr;
  for (const master_analysis_frame* f : order) {
    if (first_copy == nullptr || first_copy->digest != f->digest) first_copy = f;
    std::optional<reason> why = first_reason(role, *f, params, first_copy);
    if (!why) {
      out.usable.push_back(f->input_id);
      continue;
    }
    exclusions.push(object_of({{"digest", json::string(f->digest.sha256)},
                               {"input_id", json::uinteger(f->input_id)},
                               {"motivo", json::string(why->name)},
                               {"valori", why->values}}));
    out.excluded.push_back(master_exclusion{f->input_id, f->digest, std::move(why->name), std::move(why->values)});
  }
  if (out.usable.empty()) {
    throw stop_error(error_category::invalid_input, "master.no_usable_frames", "nessun frame utilizzabile nel candidato",
                     {{"ruolo", role_name(role)}, {"esclusioni", canonical_json(exclusions)}});
  }
  if (fault) {
    if (const int err = fault("master.analysis.groups"); err != 0) {
      throw stop_error(error_category::io, "master.analysis.fault", "guasto iniettato nell'analisi dei master",
                       {{"point", "master.analysis.groups"}, {"errno", std::to_string(err)}});
    }
  }
  return out;
}

}  // namespace astro
