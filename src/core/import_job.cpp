#include "astro/import_job.hpp"

#include <algorithm>
#include <sstream>

#include "astro/operations.hpp"
#include "fits_internal.hpp"

namespace astro {

namespace {

constexpr std::uint32_t import_algorithm_version = 1;
constexpr std::uint32_t import_serialization_version = 1;
constexpr const char* io_backend = "cpu_scalar";

[[noreturn]] void fail(error_category c, const char* code, const char* message, stop_error::context_list ctx = {}) {
  throw stop_error(c, code, message, std::move(ctx));
}

json read_options_json(const fits_read_options& o) {
  json j = json::object();
  j.set("declared_bayer_x_offset", o.declared_bayer_x_offset ? json::uinteger(*o.declared_bayer_x_offset) : json::null());
  j.set("declared_bayer_y_offset", o.declared_bayer_y_offset ? json::uinteger(*o.declared_bayer_y_offset) : json::null());
  j.set("declared_phase", o.declared_phase ? json::string(to_string(*o.declared_phase)) : json::null());
  j.set("declared_linearity", o.declared_linearity ? json::string(to_string(*o.declared_linearity)) : json::null());
  return j;
}

json cache_params(const preview_request& request) {
  json p = json::object();
  p.set("block_width", json::uinteger(request.block_width));
  p.set("block_height", json::uinteger(request.block_height));
  p.set("cache_algorithm_version", json::uinteger(thumbnail_cache_algorithm_version));
  return p;
}

node_spec io_node(const char* operator_id, json params, const content_digest& source) {
  std::vector<node_input> inputs;
  inputs.push_back(node_input::from_file("file", source));
  return node_spec(operator_id, import_algorithm_version, import_serialization_version, std::move(params),
                   std::move(inputs), sample_type::f64, io_backend, std::nullopt);
}

const char* origin_text(metadata_origin o) { return o == metadata_origin::header ? "header" : "declared"; }

json phase_json(const metadata_value<process_phase>& phase) {
  json j = json::object();
  j.set("value", json::string(to_string(phase.value)));
  j.set("origin", json::string(origin_text(phase.source.origin)));
  return j;
}

json linearity_json(linearity_class lin, const std::optional<metadata_origin>& origin) {
  json j = json::object();
  j.set("value", json::string(to_string(lin)));
  j.set("origin", origin ? json::string(origin_text(*origin)) : json::null());
  return j;
}

json strings_json(const std::vector<std::string>& values) {
  json a = json::array();
  for (const std::string& v : values) a.push(json::string(v));
  return a;
}

template <class T>
void add_metadata(json& out, const char* name, const std::optional<metadata_value<T>>& field) {
  if (!field) return;
  json e = json::object();
  if constexpr (std::is_same_v<T, std::string>) e.set("value", json::string(field->value));
  else if constexpr (std::is_same_v<T, std::int64_t>) e.set("value", json::integer(field->value));
  else e.set("value", json::number(field->value));
  e.set("origin", json::string(origin_text(field->source.origin)));
  e.set("keyword", json::string(field->source.keyword));
  out.set(name, std::move(e));
}

json metadata_json(const frame_metadata& m) {
  json out = json::object();
  add_metadata(out, "exposure_s", m.exposure_s);
  add_metadata(out, "gain", m.gain);
  add_metadata(out, "conversion_gain_e_per_adu", m.conversion_gain_e_per_adu);
  add_metadata(out, "offset_adu", m.offset_adu);
  add_metadata(out, "temperature_c", m.temperature_c);
  add_metadata(out, "filter", m.filter);
  add_metadata(out, "camera", m.camera);
  add_metadata(out, "imagetyp", m.imagetyp);
  add_metadata(out, "saturation_adu", m.saturation_adu);
  add_metadata(out, "pedestal_adu", m.pedestal_adu);
  add_metadata(out, "roi_x", m.roi_x);
  add_metadata(out, "roi_y", m.roi_y);
  add_metadata(out, "readout_mode", m.readout_mode);
  add_metadata(out, "row_order", m.row_order);
  return out;
}

const char* layout_text(plane_layout l) {
  switch (l) {
    case plane_layout::mono: return "mono";
    case plane_layout::cfa: return "cfa";
    case plane_layout::rgb: return "rgb";
    case plane_layout::multi_plane: return "multi_plane";
  }
  std::abort();
}

const char* pattern_text(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return "RGGB";
    case cfa_pattern::bggr: return "BGGR";
    case cfa_pattern::grbg: return "GRBG";
    case cfa_pattern::gbrg: return "GBRG";
  }
  std::abort();
}

const char* unit_text(pixel_unit u) {
  switch (u) {
    case pixel_unit::adu: return "adu";
    case pixel_unit::electron: return "electron";
    case pixel_unit::dimensionless: return "dimensionless";
  }
  std::abort();
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
    default: out << pad << key << ": " << canonical_json(value) << "\n";
  }
}

std::span<const std::uint8_t> bytes_of(const std::string& s) {
  return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}

// Nodo della richiesta, costruito e confrontato con lo slot da run_job_files prima di `work`.
job_request import_node_request(std::function<node_spec()> node) {
  job_request r;
  r.node = std::move(node);
  r.mismatch_code = "import.node_mismatch";
  return r;
}

// Confronto di stringhe, nessun numero in virgola mobile: ammesso prima del controllo FP del runner.
void require_rebuild_cause(const std::string& cause) {
  if (cause != cache_rebuild_cause_corrupt && cause != cache_rebuild_cause_user) {
    fail(error_category::invalid_input, "cache.rebuild_cause", "causa di ricostruzione non valida", {{"causa", cause}});
  }
}

fits_input open_verified(const std::string& path, const content_digest& expected, memory_pool& pool,
                         const job_hook& hook) {
  fits_input input = open_fits_input(path);
  const content_digest found = digest_fits_input(input, pool);
  if (!(found == expected)) {
    fail(error_category::integrity, "input.digest_mismatch", "digest o dimensione diversi da quelli attesi",
         {{"atteso", expected.sha256}, {"calcolato", found.sha256}});
  }
  if (hook) hook("digest.done");
  return input;
}

json observed_source(const content_digest& digest, const fits_inventory& inv) {
  json o = json::object();
  o.set("input_name", json::string("file"));
  o.set("sha256", json::string(digest.sha256));
  o.set("size_bytes", json::uinteger(digest.size_bytes));
  o.set("phase", inv.phase ? phase_json(*inv.phase) : json::null());
  o.set("linearity", linearity_json(inv.lin_class, inv.lin_origin));
  o.set("uninterpreted_keywords", strings_json(inv.uninterpreted_keywords));
  return o;
}

json cache_details(const char* action, const thumbnail_entry& e) {
  json c = json::object();
  c.set("action", json::string(action));
  c.set("key", json::string(e.key));
  c.set("entry", json::string(e.name));
  c.set("outcome", json::string(to_string(e.outcome)));
  return c;
}

std::string thumbnail_ref_json(const thumbnail_result& r, const content_digest& source) {
  json j = json::object();
  j.set("format", json::string("astro.thumbnail_ref.v1"));
  j.set("source_sha256", json::string(source.sha256));
  j.set("key", json::string(r.entry.key));
  j.set("entry", json::string(r.entry.name));
  j.set("entry_sha256", json::string(r.entry.digest.sha256));
  j.set("entry_size_bytes", json::uinteger(r.entry.digest.size_bytes));
  j.set("outcome", json::string(to_string(r.entry.outcome)));
  j.set("width", json::uinteger(r.preview.width));
  j.set("height", json::uinteger(r.preview.height));
  j.set("planes", json::uinteger(r.preview.planes));
  j.set("block_width", json::uinteger(r.preview.block_width));
  j.set("block_height", json::uinteger(r.preview.block_height));
  return canonical_json(j);
}

}  // namespace

node_spec make_import_node(const import_request& request) {
  json params = json::object();
  params.set("fits_read_options", read_options_json(request.read_options));
  params.set("report_version", json::uinteger(1));
  return io_node(import_operator_id, std::move(params), request.digest);
}

node_spec make_thumbnail_node(const content_digest& source, const preview_request& request) {
  return io_node(thumbnail_operator_id, cache_params(request), source);
}

node_spec make_cache_rebuild_node(const content_digest& source, const preview_request& request, const std::string& cause) {
  require_rebuild_cause(cause);
  json params = cache_params(request);
  params.set("target", json::string("thumbnail"));
  params.set("cause", json::string(cause));
  return io_node(cache_rebuild_operator_id, std::move(params), source);
}

json import_report_json(const fits_inventory& inv, const content_digest& digest) {
  if (!inv.phase) {
    fail(error_category::metadata_missing, "import.phase_missing",
         "la fase di processo non e' dichiarata ne' presente nell'header");
  }
  for (std::size_t i = 0; i < inv.header_cards.size(); ++i) {
    for (const char c : inv.header_cards[i]) {
      if (c < 0x20 || c > 0x7e) {
        fail(error_category::invalid_input, "fits.header_ascii", "card dell'header con caratteri non ASCII stampabili",
             {{"card", std::to_string(i)}});
      }
    }
  }
  json source = json::object();
  source.set("sha256", json::string(digest.sha256));
  source.set("size_bytes", json::uinteger(digest.size_bytes));

  json hdus = json::array();
  for (const hdu_inventory& h : inv.hdus) {
    json e = json::object();
    e.set("index", json::uinteger(h.index));
    e.set("extname", json::string(h.extname));
    e.set("bitpix", json::integer(h.bitpix));
    json axes = json::array();
    for (const std::uint64_t a : h.axes) axes.push(json::uinteger(a));
    e.set("axes", std::move(axes));
    e.set("bscale", json::number(h.bscale));
    e.set("bzero", json::number(h.bzero));
    e.set("header_offset", json::uinteger(h.header_offset));
    e.set("data_offset", json::uinteger(h.data_offset));
    e.set("data_bytes", json::uinteger(h.data_bytes));
    e.set("card_count", json::uinteger(h.card_count));
    hdus.push(std::move(e));
  }

  json image = json::object();
  image.set("width", json::uinteger(inv.plan.width));
  image.set("height", json::uinteger(inv.plan.height));
  image.set("planes", json::uinteger(inv.plan.planes));
  image.set("layout", json::string(layout_text(inv.layout)));
  image.set("sample_type", json::string(inv.plan.type == sample_type::f32 ? "f32" : "f64"));
  if (inv.cfa) {
    json cfa = json::object();
    cfa.set("pattern", json::string(pattern_text(inv.cfa->pattern)));
    cfa.set("x_offset", json::uinteger(inv.cfa->x_offset));
    cfa.set("y_offset", json::uinteger(inv.cfa->y_offset));
    image.set("cfa", std::move(cfa));
  } else {
    image.set("cfa", json::null());
  }
  image.set("binning_x", inv.binning_x ? json::uinteger(*inv.binning_x) : json::null());
  image.set("binning_y", inv.binning_y ? json::uinteger(*inv.binning_y) : json::null());
  image.set("unit", inv.unit ? json::string(unit_text(*inv.unit)) : json::null());

  json state = json::object();
  state.set("phase", phase_json(*inv.phase));
  state.set("linearity", linearity_json(inv.lin_class, inv.lin_origin));

  const process_state ps{inv.phase->value, inv.lin_class};
  json operations = json::array();
  for (const preflight_entry& e : preflight_table(ps)) {
    json o = json::object();
    o.set("operation", json::string(to_string(e.operation)));
    o.set("allowed", json::boolean(e.allowed));
    o.set("code", json::string(e.code));
    operations.push(std::move(o));
  }
  json diagnostics = json::array();
  if (inv.lin_class != linearity_class::linear) {
    json d = json::object();
    d.set("code", json::string("c14.linear_ancestor_missing"));
    d.set("severity", json::string("limit"));
    d.set("message", json::string("linearità non nota o non lineare: antenato radiometricamente lineare verificabile "
                                  "assente (C-14); nessuna inversione dello stretch, nessun operatore sostitutivo"));
    diagnostics.push(std::move(d));
  }
  json preflight = json::object();
  preflight.set("operations", std::move(operations));
  preflight.set("diagnostics", std::move(diagnostics));

  json report = json::object();
  report.set("format", json::string(import_report_format));
  report.set("source", std::move(source));
  report.set("file_bytes", json::uinteger(inv.file_bytes));
  report.set("header_bytes_read", json::uinteger(inv.header_bytes_read));
  report.set("hdus", std::move(hdus));
  report.set("image", std::move(image));
  report.set("metadata", metadata_json(inv.metadata));
  report.set("state", std::move(state));
  report.set("preflight", std::move(preflight));
  report.set("header_cards", strings_json(inv.header_cards));
  report.set("uninterpreted_keywords", strings_json(inv.uninterpreted_keywords));
  return report;
}

std::string import_report_text(const json& report) {
  std::ostringstream out;
  out << "Rapporto di import\n";
  render(out, report, 0, "");
  return out.str();
}

job_outcome run_import_job(sqlite_dag_store& store, const std::string& slot_name, const import_request& request,
                           memory_pool& pool, const cancel_token& cancel, const std::optional<budget_snapshot>& budget) {
  const auto work = [&](const job_record&) -> job_files_result {
    if (cancel.requested()) fail(error_category::cancelled, "sched.cancelled", "annullamento prima dell'import");
    fits_input input = open_verified(request.path, request.digest, pool, request.hook);
    const fits_inventory inv = inventory_fits(input, request.read_options, pool);  // verifica anche che il file non sia cambiato
    const json report = import_report_json(inv, request.digest);
    const std::string report_json = canonical_json(report);
    const std::string report_text = import_report_text(report);
    job_files_result result;
    result.files = store.write_files(
        {artifact_content{bytes_of(report_json), "json", "import_report", std::nullopt, import_report_role},
         artifact_content{bytes_of(report_text), "txt", "import_report_text", std::nullopt, import_report_text_role}});
    json observed = json::array();
    observed.push(observed_source(request.digest, inv));
    json details = json::object();
    details.set("observed_inputs", std::move(observed));
    result.details = std::move(details);
    return result;
  };
  return run_job_files(store, slot_name, work, budget, &cancel,
                       import_node_request([&] { return make_import_node(request); }));
}

job_outcome run_thumbnail_job(sqlite_dag_store& store, const std::string& slot_name, const std::string& path,
                              const content_digest& digest, const preview_request& request, thumbnail_cache& cache,
                              memory_pool& pool, const cancel_token& cancel, const std::optional<budget_snapshot>& budget,
                              const job_hook& hook) {
  const auto work = [&](const job_record&) -> job_files_result {
    fits_input input = open_verified(path, digest, pool, hook);
    const thumbnail_result r = cache.get(input, digest, request, pool, &cancel);
    fits_detail::check_unchanged(input);
    const std::string ref = thumbnail_ref_json(r, digest);
    job_files_result result;
    result.files = store.write_files({artifact_content{bytes_of(ref), "json", "thumbnail_ref", std::nullopt, "primary"}});
    json details = json::object();
    details.set("cache", cache_details("thumbnail", r.entry));
    result.details = std::move(details);
    return result;
  };
  return run_job_files(store, slot_name, work, budget, &cancel,
                       import_node_request([&] { return make_thumbnail_node(digest, request); }));
}

job_outcome run_cache_rebuild_job(sqlite_dag_store& store, const std::string& slot_name, const std::string& path,
                                  const content_digest& digest, const preview_request& request, const std::string& cause,
                                  thumbnail_cache& cache, memory_pool& pool, const cancel_token& cancel,
                                  const std::optional<budget_snapshot>& budget, const job_hook& hook) {
  // Una causa non valida e' un arresto prima del lavoro sullo slot (job.hpp).
  prepare_job_start(store, slot_name, [&] { require_rebuild_cause(cause); });
  const auto work = [&](const job_record&) -> job_files_result {
    fits_input input = open_verified(path, digest, pool, hook);
    std::string replaced_name;
    std::string replaced_found;
    const thumbnail_result r = cache.rebuild(input, digest, request, pool, &cancel, replaced_name, replaced_found);
    fits_detail::check_unchanged(input);
    const std::string ref = thumbnail_ref_json(r, digest);
    job_files_result result;
    result.files = store.write_files({artifact_content{bytes_of(ref), "json", "thumbnail_ref", std::nullopt, "primary"}});
    json c = cache_details("rebuild", r.entry);
    c.set("cause", json::string(cause));
    json replaced = json::object();
    replaced.set("name", json::string(replaced_name));
    replaced.set("found_digest", json::string(replaced_found));
    c.set("replaced_entry", std::move(replaced));
    json details = json::object();
    details.set("cache", std::move(c));
    result.details = std::move(details);
    return result;
  };
  return run_job_files(store, slot_name, work, budget, &cancel,
                       import_node_request([&] { return make_cache_rebuild_node(digest, request, cause); }));
}

}  // namespace astro
