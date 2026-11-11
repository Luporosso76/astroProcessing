#include "astro/batch_classification.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <type_traits>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/import_job.hpp"
#include "astro/input_format.hpp"
#include "astro/inventory.hpp"
#include "astro/master.hpp"
#include "astro/master_analysis.hpp"
#include "astro/node.hpp"
#include "astro/operations.hpp"
#include "astro/raw_decode.hpp"
#include "astro/raw_import_job.hpp"
#include "calibration_internal.hpp"
#include "sqlite_internal.hpp"

namespace astro {

namespace {

constexpr const char* declarations_format = "astro.input_declarations.v1";
constexpr const char* batch_input_role = "batch";
constexpr const char* fits_first_card = "SIMPLE  =";

classification_proposal unclassified(const char* rule_id, const char* reason, json evidence) {
  return classification_proposal{"unclassified", rule_id, reason, std::move(evidence)};
}

// FITS 4.0 §4.4.1.1: la prima card obbligatoria è `SIMPLE  =`. Il contenuto decide il formato, mai l'estensione.
bool starts_with_simple_card(const std::string& path) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
  if (fd < 0) {
    if (errno == ENOENT || errno == ENOTDIR) throw stop_error(error_category::io, "input.missing", "originale assente", {{"path", path}});
    sqlint::throw_errno(error_category::io, "batch.open", "apertura dell'originale fallita", errno, {{"path", path}});
  }
  struct stat st {};
  if (::fstat(fd, &st) != 0) {
    const int err = errno;
    ::close(fd);
    sqlint::throw_errno(error_category::io, "batch.fstat", "fstat dell'originale fallita", err, {{"path", path}});
  }
  if (!S_ISREG(st.st_mode)) {
    ::close(fd);
    throw stop_error(error_category::invalid_input, "store.bad_input", "l'originale non è un file regolare", {{"path", path}});
  }
  const std::size_t want = std::strlen(fits_first_card);
  char head[16] = {};
  std::size_t got = 0;
  while (got < want) {
    const ssize_t n = ::read(fd, head + got, want - got);
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) {
      const int err = errno;
      ::close(fd);
      sqlint::throw_errno(error_category::io, "batch.read", "lettura dell'originale fallita", err, {{"path", path}});
    }
    if (n == 0) break;
    got += static_cast<std::size_t>(n);
  }
  ::close(fd);
  return got == want && std::memcmp(head, fits_first_card, want) == 0;
}

struct planned_file {
  std::string path;
  const input_format* format = nullptr;
  content_digest digest;
};

content_digest digest_of_file(const std::string& path, const input_format& format, memory_pool& pool, const cancel_token& cancel) {
  const fits_input input = open_input(path, format);
  reservation block = pool.reserve(sha256_chunk_bytes, "batch.digest", &cancel);
  pool_buffer buffer(block, sha256_chunk_bytes);
  return digest_input(input, {buffer.data(), buffer.size()}, [&cancel] {
    if (cancel.requested()) throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto");
  });
}

json default_declarations() {
  json doc = json::object();
  doc.set("format", json::string(declarations_format));
  doc.set("declared_image_fields", json::object());
  return doc;
}

classification_record rule_revision(std::uint64_t input_id, const classification_proposal& p) {
  json evidence = p.evidence;
  if (p.role == "unclassified") evidence.set("reason", json::string(p.reason));
  classification_record r;
  r.input_id = input_id;
  r.role = p.role;
  r.source = "rule";
  r.rule_id = p.rule_id;
  r.evidence_canonical = canonical_json(evidence);
  r.declarations_canonical = canonical_json(default_declarations());
  return r;
}

// Report di import del job completato, riletto dall'artefatto pubblicato e verificato contro il suo digest.
json read_import_report(const sqlite_dag_store& store, const std::string& node_key) {
  for (const artifact_record& a : store.artifacts_for(node_key)) {
    if (a.kind != "import_report" || a.output_role != import_report_role || !a.present) continue;
    std::ifstream in(store.artifact_path(a.rel_path), std::ios::binary);
    if (!in) throw stop_error(error_category::io, "batch.report_read", "report di import non apribile", {{"node", node_key}});
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) throw stop_error(error_category::io, "batch.report_read", "report di import non leggibile", {{"node", node_key}});
    if (sha256_hex({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()}) != a.sha256) {
      throw stop_error(error_category::integrity, "batch.report_digest", "report di import diverso dal suo digest",
                       {{"node", node_key}});
    }
    return parse_json(text);
  }
  throw stop_error(error_category::integrity, "batch.report_missing", "il job completato non ha il report di import",
                   {{"node", node_key}});
}

}  // namespace

classification_proposal propose_classification(const json& fits_import_report) {
  const json* metadata = fits_import_report.find("metadata");
  const json* field = metadata != nullptr ? metadata->find("imagetyp") : nullptr;
  if (field == nullptr) return unclassified("imagetyp.v1", "imagetyp_absent", json::object());
  const json* value = field->find("value");
  if (value == nullptr || value->type() != json::kind::string) {
    throw stop_error(error_category::integrity, "batch.report_malformed", "IMAGETYP del report non è una stringa");
  }
  json evidence = json::object();
  evidence.set("imagetyp", json::string(value->as_string()));
  const std::optional<std::string> role = calib_detail::imagetyp_role(value->as_string());
  if (!role) return unclassified("imagetyp.v1", "imagetyp_unrecognized", std::move(evidence));
  return classification_proposal{*role, "imagetyp.v1", "", std::move(evidence)};
}

classification_proposal propose_classification_raw() {
  return unclassified("raw_no_type.v1", "raw_no_type_metadata", json::object());
}

classification_proposal propose_classification_stopped(error_category category, const std::string& code) {
  json evidence = json::object();
  evidence.set("category", json::string(to_string(category)));
  evidence.set("code", json::string(code));
  return unclassified("import_stopped.v1", "import_stopped", std::move(evidence));
}

std::vector<batch_import_result> import_batch(sqlite_dag_store& store, const std::vector<std::string>& absolute_paths,
                                              memory_pool& pool, const cancel_token& cancel) {
  for (const std::string& path : absolute_paths) require_absolute_input_path(path);
  std::vector<planned_file> plan;
  for (const std::string& path : absolute_paths) {
    planned_file f;
    f.path = path;
    f.format = starts_with_simple_card(path) ? &fits_input_format() : &raw_input_format();
    f.digest = digest_of_file(path, *f.format, pool, cancel);
    plan.push_back(std::move(f));
  }

  std::vector<batch_import_result> results;
  std::vector<import_request> fits_requests(plan.size());
  std::vector<std::optional<raw_import_request>> raw_requests(plan.size());
  std::vector<resolved_node> nodes;
  std::map<std::string, std::string> slot_targets;
  for (std::size_t i = 0; i < plan.size(); ++i) {
    batch_import_result r;
    r.input = store.add_input(input_record{0, plan[i].path, plan[i].digest.size_bytes, plan[i].digest.sha256, batch_input_role, 0, 0,
                                           plan[i].format->prefix});
    r.slot_name = "batch.input." + std::to_string(r.input.id);
    if (plan[i].format == &fits_input_format()) {
      r.job_type = import_operator_id;
      fits_requests[i].path = plan[i].path;
      fits_requests[i].digest = plan[i].digest;
      nodes.push_back(resolved_node{r.slot_name, make_import_node(fits_requests[i])});
    } else {
      r.job_type = raw_import_operator_id;
      raw_requests[i] = make_raw_import_request(plan[i].path, plan[i].digest);
      nodes.push_back(resolved_node{r.slot_name, raw_requests[i]->node});
    }
    slot_targets.emplace(r.slot_name, r.slot_name);
    results.push_back(std::move(r));
  }
  store.apply_recipe(nodes, slot_targets);

  std::vector<branch_files_job> jobs;
  for (std::size_t i = 0; i < plan.size(); ++i) {
    jobs.push_back(raw_requests[i] ? make_raw_import_branch_job(store, results[i].slot_name, *raw_requests[i], pool, cancel)
                                   : make_import_branch_job(store, results[i].slot_name, fits_requests[i], pool, cancel));
  }
  const std::vector<branch_result> branches = run_branches(store, std::move(jobs), std::nullopt, &cancel);
  ASTRO_ASSERT(branches.size() == results.size());

  std::vector<classification_record> revisions;
  for (std::size_t i = 0; i < plan.size(); ++i) {
    batch_import_result& r = results[i];
    const branch_result& branch = branches[i];
    if (branch.state == branch_state::cancelled) {
      throw branch.outcome && branch.outcome->stop
          ? *branch.outcome->stop
          : stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto");
    }
    // Senza archi i rami girano in ordine di indice e il primo `cancelled` lancia sopra: nessun ramo resta
    // `not_started` o `blocked`, e completed e failed portano sempre l'esito.
    ASTRO_ASSERT(branch.state != branch_state::not_started && branch.outcome);
    const job_outcome& out = *branch.outcome;
    classification_proposal proposal;
    if (out.exit_code == 0) {
      r.state = branch_state::completed;
      proposal = raw_requests[i] ? propose_classification_raw()
                                 : propose_classification(read_import_report(store, out.job.node_key));
    } else {
      if (!out.stop) {
        throw stop_error(error_category::integrity, "batch.stop_missing", "job d'import fallito senza causa",
                         {{"slot", r.slot_name}, {"exit_code", std::to_string(out.exit_code)}});
      }
      r.state = branch_state::failed;
      r.stop = out.stop;
      proposal = propose_classification_stopped(out.stop->category(), out.stop->code());
    }
    revisions.push_back(rule_revision(r.input.id, proposal));
  }
  const std::vector<classification_record> written = store.append_classifications(revisions);
  for (std::size_t i = 0; i < results.size(); ++i) results[i].classification = written[i];
  return results;
}

// ---------------------------------------------------------------- osservazioni (CL-06)

struct input_observation::data {
  image img;
  std::optional<std::string> conflict;
  std::optional<decoder_identity> identity;
  std::string fingerprint;
};

namespace {

[[noreturn]] void bad_declaration(const char* why) {
  throw stop_error(error_category::invalid_input, "classify.declaration", "dichiarazioni non conformi ad ADR-03",
                   {{"motivo", why}});
}

[[noreturn]] void report_malformed(const char* what) {
  throw stop_error(error_category::integrity, "batch.report_malformed", "report di import non leggibile", {{"campo", what}});
}

struct declared_values {
  frame_metadata meta;
  declared_image_fields fields;
};

double number_of(const json& v) {
  if (v.type() == json::kind::integer) return static_cast<double>(v.as_integer());
  if (v.type() == json::kind::number) return v.as_number();
  bad_declaration("valore numerico atteso");
}

std::uint32_t positive_u32(const json& v) {
  if (v.type() != json::kind::integer || v.as_integer() < 1 || v.as_integer() > 4294967295LL) bad_declaration("binning non valido");
  return static_cast<std::uint32_t>(v.as_integer());
}

template <class T>
void declare(std::optional<metadata_value<T>>& field, T value) {
  field = metadata_value<T>{std::move(value), {metadata_origin::declared, {}}};
}

const std::string& text_of(const json& v) {
  if (v.type() != json::kind::string) bad_declaration("stringa attesa");
  return v.as_string();
}

declared_values parse_declarations(const std::string& text) {
  const json doc = parse_json(text);
  if (doc.type() != json::kind::object) bad_declaration("documento non oggetto");
  const json* format = doc.find("format");
  if (format == nullptr || format->type() != json::kind::string || format->as_string() != declarations_format) {
    bad_declaration("formato errato");
  }
  declared_values out;
  const json* fields = doc.find("declared_image_fields");
  if (fields == nullptr || fields->type() != json::kind::object) bad_declaration("declared_image_fields assente");
  for (std::size_t i = 0; i < fields->keys().size(); ++i) {
    const std::string& key = fields->keys()[i];
    const json& v = fields->items()[i];
    if (key == "binning_x") out.fields.binning_x = positive_u32(v);
    else if (key == "binning_y") out.fields.binning_y = positive_u32(v);
    else if (key == "unit") {
      const std::string& u = text_of(v);
      if (u == "adu") out.fields.unit = pixel_unit::adu;
      else if (u == "electron") out.fields.unit = pixel_unit::electron;
      else if (u == "dimensionless") out.fields.unit = pixel_unit::dimensionless;
      else bad_declaration("unità sconosciuta");
    } else if (key == "linearity") {
      const std::string& l = text_of(v);
      if (l == "linear") out.fields.lin = linearity::linear;
      else if (l == "nonlinear") out.fields.lin = linearity::nonlinear;
      else bad_declaration("linearità sconosciuta");
    } else {
      bad_declaration("campo dichiarato sconosciuto");
    }
  }
  if (const json* meta = doc.find("frame_metadata")) {
    if (meta->type() != json::kind::object) bad_declaration("frame_metadata non oggetto");
    frame_metadata& m = out.meta;
    for (std::size_t i = 0; i < meta->keys().size(); ++i) {
      const std::string& key = meta->keys()[i];
      const json& v = meta->items()[i];
      if (key == "exposure_s") declare(m.exposure_s, number_of(v));
      else if (key == "gain") declare(m.gain, number_of(v));
      else if (key == "conversion_gain_e_per_adu") declare(m.conversion_gain_e_per_adu, number_of(v));
      else if (key == "offset_adu") declare(m.offset_adu, number_of(v));
      else if (key == "temperature_c") declare(m.temperature_c, number_of(v));
      else if (key == "saturation_adu") declare(m.saturation_adu, number_of(v));
      else if (key == "pedestal_adu") declare(m.pedestal_adu, number_of(v));
      else if (key == "roi_x" || key == "roi_y") {
        if (v.type() != json::kind::integer) bad_declaration("intero atteso");
        declare(key == "roi_x" ? m.roi_x : m.roi_y, v.as_integer());
      } else if (key == "filter") declare(m.filter, text_of(v));
      else if (key == "camera") declare(m.camera, text_of(v));
      else if (key == "imagetyp") declare(m.imagetyp, text_of(v));
      else if (key == "readout_mode") declare(m.readout_mode, text_of(v));
      else if (key == "row_order") declare(m.row_order, text_of(v));
      else bad_declaration("campo di frame_metadata sconosciuto");
    }
  }
  return out;
}

const char* unit_text(pixel_unit u) {
  switch (u) {
    case pixel_unit::adu: return "adu";
    case pixel_unit::electron: return "electron";
    case pixel_unit::dimensionless: return "dimensionless";
  }
  std::abort();
}

json integers_json(const auto& values) {
  json a = json::array();
  for (const auto v : values) a.push(json::integer(static_cast<std::int64_t>(v)));
  return a;
}

json rational_pair_json(const std::optional<std::array<dng_rational, 2>>& v) {
  if (!v) return json::null();
  json a = json::array();
  for (const dng_rational& x : *v) a.push(integers_json(std::array<std::uint32_t, 2>{x.num, x.den}));
  return a;
}

json strings_json(const std::vector<std::string>& values) {
  json a = json::array();
  for (const std::string& v : values) a.push(json::string(v));
  return a;
}

// Campi del decoder del progetto (U13 §8), tutti i membri di `dng_decoder_fields`.
json dng_json(const dng_decoder_fields& f) {
  json o = json::object();
  o.set("dng_version", integers_json(f.dng_version));
  o.set("dng_backward_version", integers_json(f.dng_backward_version));
  o.set("tiles", json::boolean(f.tiles));
  o.set("rows_per_strip", json::uinteger(f.rows_per_strip));
  o.set("tile_width", json::uinteger(f.tile_width));
  o.set("tile_length", json::uinteger(f.tile_length));
  o.set("segment_count", json::uinteger(f.segment_count));
  o.set("predictor", json::uinteger(f.predictor));
  o.set("little_endian", json::boolean(f.little_endian));
  o.set("zlib_runtime_version", json::string(f.zlib_runtime_version));
  o.set("orientation", f.orientation ? json::uinteger(*f.orientation) : json::null());
  o.set("default_crop_origin", rational_pair_json(f.default_crop_origin));
  o.set("default_crop_size", rational_pair_json(f.default_crop_size));
  o.set("metadata_rejected", strings_json(f.metadata_rejected));
  o.set("opcode_lists", strings_json(f.opcode_lists));
  return o;
}

json state_json(const decoder_state& s) {
  json o = json::object();
  o.set("raw_width", json::uinteger(s.raw_width));
  o.set("raw_height", json::uinteger(s.raw_height));
  o.set("window", integers_json(std::array<std::uint32_t, 4>{s.window.x0, s.window.y0, s.window.width, s.window.height}));
  o.set("black", json::uinteger(s.black));
  o.set("cfa_color_index", integers_json(s.cfa_color_index));
  o.set("black_per_color", integers_json(s.black_per_color));
  o.set("black_pattern_rows", json::uinteger(s.black_pattern_rows));
  o.set("black_pattern_cols", json::uinteger(s.black_pattern_cols));
  o.set("black_pattern", integers_json(s.black_pattern));
  o.set("white", json::uinteger(s.white));
  o.set("data_maximum", s.data_maximum ? json::uinteger(*s.data_maximum) : json::null());
  o.set("linear_max", s.linear_max ? integers_json(*s.linear_max) : json::null());
  o.set("linearization", json::string(s.linearization == decoder_linearization::decoder_curve ? "decoder_curve" : "identity"));
  json curve = json::null();
  if (s.curve_sha256) curve = integers_json(*s.curve_sha256);
  o.set("curve_sha256", std::move(curve));
  o.set("process_warnings", json::uinteger(s.process_warnings));
  json insets = json::array();
  for (const auto& c : s.raw_inset_crops) insets.push(integers_json(c));
  o.set("raw_inset_crops", std::move(insets));
  o.set("flip", json::integer(s.flip));
  json route = json::object();
  route.set("decoder", json::string(s.route.decoder == raw_decoder_kind::astro_dng ? "astro_dng" : "libraw"));
  route.set("reason", json::string(s.route.reason));
  route.set("compression", s.route.compression ? json::uinteger(*s.route.compression) : json::null());
  route.set("sample_format", s.route.sample_format ? json::uinteger(*s.route.sample_format) : json::null());
  route.set("bits_per_sample", s.route.bits_per_sample ? json::uinteger(*s.route.bits_per_sample) : json::null());
  o.set("route", std::move(route));
  o.set("dng", s.dng ? dng_json(*s.dng) : json::null());
  return o;
}

json identity_json(const decoder_identity& id) {
  json o = json::object();
  o.set("libraw_version", json::string(id.libraw_version));
  o.set("libraw_version_number", json::uinteger(id.libraw_version_number));
  o.set("libraw_capabilities", json::uinteger(id.libraw_capabilities));
  o.set("libraw_tarball_sha256", json::string(id.libraw_tarball_sha256));
  return o;
}

// Campi osservati di un'immagine, con la loro origine: l'uguaglianza di due osservazioni.
std::string fingerprint_of(const input_observation::data& d) {
  const image& im = d.img;
  json o = json::object();
  o.set("width", json::uinteger(im.geometry.width));
  o.set("height", json::uinteger(im.geometry.height));
  o.set("planes", json::uinteger(im.geometry.planes));
  o.set("binning_x", im.geometry.binning_x ? json::uinteger(*im.geometry.binning_x) : json::null());
  o.set("binning_y", im.geometry.binning_y ? json::uinteger(*im.geometry.binning_y) : json::null());
  o.set("layout", json::string(calib_detail::layout_name(im.layout)));
  o.set("cfa", json::string(calib_detail::cfa_text(im.cfa)));
  o.set("unit", im.unit ? json::string(unit_text(*im.unit)) : json::null());
  o.set("linearity", im.lin ? json::string(*im.lin == linearity::linear ? "linear" : "nonlinear") : json::null());
  o.set("linearity_origin", im.lin_origin.value ? json::string(to_string(*im.lin_origin.value)) : json::null());
  if (im.phase) {
    json phase = json::object();
    phase.set("value", json::string(to_string(im.phase->value)));
    phase.set("origin", json::string(to_string(im.phase->source.origin)));
    o.set("phase", std::move(phase));
  } else {
    o.set("phase", json::null());
  }
  o.set("metadata", calib_detail::metadata_report(im));
  o.set("coverage", json::boolean(im.coverage.has_value()));
  o.set("decoder_state", im.decoder_state ? state_json(*im.decoder_state) : json::null());
  o.set("identity", d.identity ? identity_json(*d.identity) : json::null());
  o.set("declared_conflict", d.conflict ? json::string(*d.conflict) : json::null());
  return canonical_json(o);
}

}  // namespace

const image& input_observation::observed_image() const {
  ASTRO_ASSERT(data_ != nullptr);
  return data_->img;
}

const std::optional<std::string>& input_observation::declared_conflict() const {
  ASTRO_ASSERT(data_ != nullptr);
  return data_->conflict;
}

const std::optional<decoder_identity>& input_observation::identity() const {
  ASTRO_ASSERT(data_ != nullptr);
  return data_->identity;
}

bool operator==(const input_observation& a, const input_observation& b) noexcept {
  if (!a.data_ || !b.data_) return !a.data_ && !b.data_;
  return a.data_->fingerprint == b.data_->fingerprint;
}

input_observation make_observation(const image& img, const std::string& declarations_canonical,
                                   const std::optional<decoder_identity>& identity) {
  const declared_values declared = parse_declarations(declarations_canonical);
  if (img.decoder_state && !identity) {
    throw stop_error(error_category::integrity, "batch.decoder_identity_missing", "immagine con stato del decoder senza identità");
  }
  if (!img.decoder_state && identity) {
    throw stop_error(error_category::invalid_input, "batch.decoder_identity_unexpected",
                     "identità del decoder per un'immagine senza stato del decoder");
  }
  auto d = std::make_shared<input_observation::data>();
  d->img = img;
  d->identity = identity;
  try {
    calib_detail::apply_declared(d->img, declared.meta, declared.fields, "input");
  } catch (const stop_error& e) {
    if (e.code() != "metadata.declared_conflict") throw;
    d->img = img;
    std::string field;
    for (const auto& [key, value] : e.context()) {
      if (key == "campo") field = value;
    }
    d->conflict = field;
  }
  d->fingerprint = fingerprint_of(*d);
  input_observation out;
  out.data_ = std::move(d);
  return out;
}

input_observation make_observation(const image& img, const std::string& declarations_canonical) {
  return make_observation(img, declarations_canonical,
                          img.decoder_state ? std::optional<decoder_identity>(current_decoder_identity()) : std::nullopt);
}

// ---------------------------------------------------------------- analisi del batch (CL-05, CL-07…CL-09)

namespace {

constexpr master_role calibration_roles[] = {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat};

const char* role_text(master_role r) { return calib_detail::role_name(r); }

std::optional<master_role> calibration_role_of(const std::string& text) {
  for (const master_role r : calibration_roles) {
    if (text == role_text(r)) return r;
  }
  return std::nullopt;
}

bool is_raw(const image& img) { return img.decoder_state.has_value(); }

// Candidato di un ruolo (CL-06): master esterno (chiave = digest) o slot `current` (chiave = chiave del nodo).
struct candidate {
  master_role role = master_role::dark;
  std::string key;
  bool external = false;  // input registrato (chiave = digest); falso: master di uno slot (chiave = chiave del nodo)
  analysis_candidate ac;
  std::optional<std::vector<content_digest>> proposed_frames;  // solo un nodo proposto (CL-06 c)
  std::optional<std::vector<decoder_state>> states;  // da `frame_decoder_states`
};

pair_side side_of(const candidate& c) { return pair_side{&c.ac.img, c.states}; }

candidate make_candidate(master_role role, std::string key, content_digest digest, master_descriptor descriptor,
                         const input_observation& obs, bool external, std::function<image()> load_samples) {
  candidate c;
  c.role = role;
  c.key = std::move(key);
  c.external = external;
  c.ac.img = obs.observed_image();
  c.ac.load_samples = std::move(load_samples);
  c.ac.digest = std::move(digest);
  // Un master esterno prodotto da un RAW non porta `frame_decoder_states` (ADR-16.b §b.7): è escluso per la sua origine,
  // qualunque descrittore dichiarato.
  if (external && is_raw(c.ac.img)) {
    descriptor.frame_decoder_states.reset();
    c.ac.origin_reason = "decoder_state_mixed";
  }
  c.ac.descriptor = std::move(descriptor);
  if (c.ac.descriptor.frame_decoder_states) {
    std::vector<decoder_state> states;
    for (const frame_decoder_state& f : *c.ac.descriptor.frame_decoder_states) states.push_back(f.state);
    c.states = std::move(states);
  }
  return c;
}

// Un input analizzabile dopo la fusione dei duplicati (CL-05): digest, input_id crescenti, rappresentante.
struct entity {
  content_digest digest;
  std::vector<std::uint64_t> ids;
  const batch_input* rep = nullptr;
};

struct light_entity {
  entity e;
  bool raw = false;
};

std::string digest_key(const content_digest& d) { return d.sha256; }

// Chiave esatta del gruppo (CL-07 punto 1): «assente» è un valore.
std::string exact_key(const image& im) {
  const auto opt = [](const auto& field) -> std::string {
    if (!field) return "\x01";
    if constexpr (std::is_same_v<std::decay_t<decltype(field->value)>, std::string>) return field->value;
    else return calib_detail::num_text(static_cast<double>(field->value));
  };
  std::string k;
  const auto add = [&k](const std::string& part) { k += part; k += '\x1f'; };
  add(is_raw(im) ? "1" : "0");
  add(std::to_string(im.geometry.width));
  add(std::to_string(im.geometry.height));
  add(im.geometry.binning_x ? std::to_string(*im.geometry.binning_x) : "\x01");
  add(im.geometry.binning_y ? std::to_string(*im.geometry.binning_y) : "\x01");
  add(calib_detail::cfa_text(im.cfa));
  add(opt(im.metadata.camera));
  add(opt(im.metadata.gain));
  add(opt(im.metadata.filter));
  add(opt(im.metadata.roi_x));
  add(opt(im.metadata.roi_y));
  add(opt(im.metadata.readout_mode));
  add(opt(im.metadata.row_order));
  return k;
}

// Stati del decoder di un light RAW come lato di una coppia.
pair_side light_side(const image& im) {
  pair_side side{&im, std::nullopt};
  if (im.decoder_state) side.states = std::vector<decoder_state>{*im.decoder_state};
  return side;
}

// Esito del light verso un candidato (CL-07 punto 3): motivo del candidato per sé o della coppia, oppure `usable`. Il
// ruolo del dark-flat non ha coppia col light.
std::string light_pair_code(const image& light, const candidate& c, const calibration_declarations& decl) {
  std::vector<std::string> scratch;
  if (const std::optional<std::string> own = candidate_own_reason(c.ac, false, is_raw(light), scratch)) return *own;
  const pair_kind kind = c.role == master_role::bias   ? pair_kind::light_bias
                         : c.role == master_role::dark ? pair_kind::light_dark
                                                       : pair_kind::light_flat;
  if (const std::optional<std::string> r = pair_compatibility(kind, light_side(light), side_of(c), decl, scratch)) return *r;
  return "usable";
}

struct batch_context {
  std::vector<candidate> candidates;  // per ruolo e chiave
  const batch_analysis_request* request = nullptr;
  const fault_injector* fault = nullptr;

  std::vector<const candidate*> of(master_role r) const {
    std::vector<const candidate*> v;
    for (const candidate& c : candidates) {
      if (c.role == r) v.push_back(&c);
    }
    return v;
  }
};

// Il candidato del ruolo ha sicuramente lo stato del decoder prima del confronto (CL-07): nessun candidato è idoneo per sé
// e il primo, che entra nell'analisi come escluso, lo ha. Con un candidato idoneo la scelta dipende dalle dichiarazioni.
bool role_has_decoder_state(const std::vector<const candidate*>& all, bool light_raw) {
  if (all.empty()) return false;
  std::vector<std::string> scratch;
  bool any_possible = false;
  for (const candidate* c : all) {
    if (!candidate_own_reason(c->ac, false, light_raw, scratch)) any_possible = true;
  }
  return !any_possible && all.front()->ac.descriptor.frame_decoder_states.has_value();
}

// Dichiarazioni d'analisi assenti per un light (CL-07, `params_missing`): stessa regola di `analyze_calibration`, per
// l'analisi del gruppo. Le tolleranze di esposizione e temperatura dipendono dai ruoli presenti; quella del nero dal
// light, o da un ruolo i cui candidati sono tutti esclusi per sé (il primo entra nell'analisi). Un candidato con stato del
// decoder accanto a un light senza è escluso per sé. Il caso che dipende dalla scelta del candidato (nessuno idoneo nel
// confronto, il primo ha lo stato del decoder) lo valuta `chosen_has_decoder_state` dopo la scelta.
std::vector<std::string> missing_params(const image& light, const batch_context& ctx) {
  const calibration_declarations& p = ctx.request->params;
  const bool light_raw = is_raw(light);
  bool any_raw = light_raw;
  bool present[4] = {false, false, false, false};
  for (const candidate& c : ctx.candidates) present[static_cast<int>(c.role)] = true;
  for (const master_role r : calibration_roles) any_raw = any_raw || role_has_decoder_state(ctx.of(r), light_raw);
  const declaration_needs needs = declaration_needs_of(present[static_cast<int>(master_role::dark)], present[static_cast<int>(master_role::bias)],
                                                       present[static_cast<int>(master_role::dark_flat)],
                                                       present[static_cast<int>(master_role::flat)], any_raw);
  std::vector<std::string> missing;
  if (needs.exposure && !p.exposure_rel_tol) missing.push_back("exposure_rel_tol");
  if (needs.temperature && !p.temp_policy) missing.push_back("temperature_policy");
  if (needs.temperature && !p.temperature_abs_tol_c) {
    missing.push_back("temperature_abs_tol_c");
  }
  if (needs.decoder && !p.decoder_black_abs_tol_adu) missing.push_back("decoder_black_abs_tol_adu");
  return missing;
}

// Gruppi: chiave esatta, esito del dominio, esiti delle coppie col light (o le dichiarazioni mancanti).
struct light_key {
  std::string text;
  std::optional<stop_error> domain;
  std::vector<std::string> missing;
};

light_key key_of_light(const image& light, const batch_context& ctx) {
  light_key k;
  k.text = exact_key(light);
  std::vector<std::string> scratch;
  k.domain = light_domain_stop(light, scratch);
  if (k.domain) {
    k.text += "D:" + std::string(to_string(k.domain->category())) + ":" + k.domain->code();
    return k;
  }
  k.missing = missing_params(light, ctx);
  if (!k.missing.empty()) {
    k.text += "M:";
    for (const std::string& m : k.missing) k.text += m + ",";
    return k;
  }
  k.text += "P:";
  for (const master_role r : {master_role::bias, master_role::dark, master_role::flat}) {
    for (const candidate& c : ctx.candidates) {
      if (c.role == r) k.text += std::string(role_text(r)) + "/" + c.key + "=" + light_pair_code(light, c, ctx.request->params) + ";";
    }
  }
  return k;
}

// ---------------------------------------------------------------- candidati per ruolo (CL-08)

struct role_result {
  role_choice choice;
  const candidate* chosen = nullptr;  // quello che entra nell'analisi
};

struct selection_result {
  std::vector<candidate_report> reports;
  std::vector<role_result> roles;  // nell'ordine di `calibration_roles`
  bool needs_choice = false;
};

selection_result choose_candidates(const image& light, const batch_context& ctx) {
  const calibration_declarations& decl = ctx.request->params;
  const bool light_raw = is_raw(light);
  std::map<const candidate*, std::string> reason;  // vuoto = idoneo
  std::vector<std::string> scratch;
  const auto own = [&](const candidate& c) { return candidate_own_reason(c.ac, false, light_raw, scratch); };
  const auto pair = [&](pair_kind kind, const pair_side& ref, const candidate& c) {
    return pair_compatibility(kind, ref, side_of(c), decl, scratch);
  };

  const std::vector<const candidate*> flats = ctx.of(master_role::flat);
  std::vector<const candidate*> eligible_flats;
  for (const candidate* f : flats) {
    std::optional<std::string> r = own(*f);
    if (!r) r = pair(pair_kind::light_flat, light_side(light), *f);
    reason[f] = r.value_or("");
    if (!r) eligible_flats.push_back(f);
  }
  for (const candidate* d : ctx.of(master_role::dark)) {
    std::optional<std::string> r = own(*d);
    if (!r) r = pair(pair_kind::light_dark, light_side(light), *d);
    reason[d] = r.value_or("");
  }
  for (const candidate* b : ctx.of(master_role::bias)) {
    std::optional<std::string> r = own(*b);
    if (r) {
      reason[b] = *r;
      continue;
    }
    const std::optional<std::string> light_pair = pair(pair_kind::light_bias, light_side(light), *b);
    bool eligible = !light_pair;
    for (const candidate* f : eligible_flats) {
      if (eligible) break;
      eligible = !pair(pair_kind::flat_bias, side_of(*f), *b);
    }
    reason[b] = eligible ? "" : *light_pair;
  }
  for (const candidate* df : ctx.of(master_role::dark_flat)) {
    std::optional<std::string> r = own(*df);
    if (r) {
      reason[df] = *r;
      continue;
    }
    // Il dark-flat si confronta col flat idoneo; se nessuno lo è, coi flat candidati anche esclusi; senza flat basta lo stato.
    const std::vector<const candidate*>& against = eligible_flats.empty() ? flats : eligible_flats;
    std::string first;
    bool eligible = against.empty();
    for (const candidate* f : against) {
      const std::optional<std::string> p = pair(pair_kind::flat_dark_flat, side_of(*f), *df);
      if (!p) {
        eligible = true;
        break;
      }
      if (first.empty()) first = *p;
    }
    reason[df] = eligible ? "" : first;
  }

  selection_result out;
  for (const master_role role : calibration_roles) {
    role_result rr;
    rr.choice.role = role;
    const std::vector<const candidate*> all = ctx.of(role);
    std::vector<const candidate*> eligible;
    for (const candidate* c : all) {
      out.reports.push_back(candidate_report{role, c->key, reason[c].empty() ? candidate_status::usable : candidate_status::excluded,
                                             reason[c]});
      if (reason[c].empty()) {
        eligible.push_back(c);
        rr.choice.eligible.push_back(c->key);
      }
    }
    if (all.empty()) {
      rr.choice.kind = choice_kind::absent;
    } else if (eligible.empty()) {
      rr.choice.kind = choice_kind::excluded_only;
      rr.chosen = all.front();
    } else if (eligible.size() == 1) {
      rr.choice.kind = choice_kind::only_compatible;
      rr.chosen = eligible.front();
    } else {
      rr.choice.kind = choice_kind::required;
      const auto given = ctx.request->choices.find(role);
      if (given == ctx.request->choices.end()) {
        out.needs_choice = true;
      } else {
        for (const candidate* c : all) {
          if (c->key == given->second) rr.chosen = c;
        }
        ASTRO_ASSERT(rr.chosen != nullptr);  // la scelta è stata validata contro i candidati del ruolo
        rr.choice.chosen = rr.chosen->key;
      }
    }
    if (rr.choice.kind == choice_kind::only_compatible) rr.choice.chosen = rr.chosen->key;
    out.roles.push_back(std::move(rr));
  }
  return out;
}

// ---------------------------------------------------------------- gruppo pronto: analisi, ricette, omissioni (CL-09)

std::vector<selected_decoder_input> frozen_raw_inputs(const std::vector<light_entity>& members, const std::vector<const batch_input*>& reps) {
  std::vector<selected_decoder_input> out;
  for (std::size_t i = 0; i < members.size(); ++i) {
    const input_observation& obs = reps[i]->observation;
    if (!obs.observed_image().decoder_state) continue;
    ASTRO_ASSERT(obs.identity().has_value());  // make_observation lo garantisce per un'immagine con stato del decoder
    out.push_back(selected_decoder_input{"light." + digest_key(members[i].e.digest), *obs.observed_image().decoder_state, *obs.identity()});
  }
  return out;
}

// Il light o un candidato scelto per l'analisi ha lo stato del decoder.
bool chosen_has_decoder_state(const image& light, const selection_result& sel) {
  bool any_raw = is_raw(light);
  for (const role_result& rr : sel.roles) {
    if (rr.chosen != nullptr) any_raw = any_raw || rr.chosen->ac.descriptor.frame_decoder_states.has_value();
  }
  return any_raw;
}

std::vector<selected_candidate> chosen_candidates(const selection_result& sel) {
  std::vector<selected_candidate> out;
  for (const role_result& rr : sel.roles) {
    if (rr.chosen == nullptr) continue;
    selected_candidate c;
    c.role = rr.choice.role;
    if (rr.chosen->external) c.input = rr.chosen->ac.digest;
    else c.node_key = rr.chosen->key;
    c.proposed_frames = rr.chosen->proposed_frames;
    out.push_back(std::move(c));
  }
  return out;
}

bool same_chains(const chain_recipe& r, const admissible_pair& p) { return r.light == p.light && r.flat == p.flat && !p.partial; }

void complete_ready_group(group_proposal& g, const image& light, const content_digest& light_digest, const selection_result& sel,
                          const batch_context& ctx) {
  calibration_analysis_input in;
  in.light = light;
  in.light_digest = light_digest;
  const bool any_raw = chosen_has_decoder_state(light, sel);
  for (const role_result& rr : sel.roles) {
    if (rr.chosen == nullptr) continue;
    switch (rr.choice.role) {
      case master_role::bias: in.bias = rr.chosen->ac; break;
      case master_role::dark: in.dark = rr.chosen->ac; break;
      case master_role::dark_flat: in.dark_flat = rr.chosen->ac; break;
      case master_role::flat: in.flat = rr.chosen->ac; break;
    }
  }
  in.declarations = ctx.request->params;
  // Un gruppo senza ingressi con stato del decoder non riceve la tolleranza del nero (nessun ingresso la usa).
  if (!any_raw) in.declarations.decoder_black_abs_tol_adu.reset();
  g.analysis = analyze_calibration(in, ctx.fault != nullptr ? *ctx.fault : fault_injector{});
  const calibration_analysis& a = *g.analysis;
  g.recipes.push_back(a.default_recipe);
  for (const admissible_pair& p : a.admissible) {
    if (same_chains(a.default_recipe, p)) continue;
    if (const std::optional<chain_recipe> r = recipe_of_pair(a, p)) g.recipes.push_back(*r);
  }
  for (const master_role role : calibration_roles) {
    if (recipe_uses_role(a.default_recipe, role)) continue;
    const auto found = std::find_if(a.candidates.begin(), a.candidates.end(), [role](const candidate_analysis& c) { return c.role == role; });
    if (found == a.candidates.end()) g.omissions.push_back(role_omission{role, "master_absent"});
    else if (found->status == candidate_status::excluded) g.omissions.push_back(role_omission{role, "master_excluded"});
  }
}

// ---------------------------------------------------------------- proposta in JSON canonico

const char* temperature_policy_text(temperature_policy p) {
  return p == temperature_policy::compare ? "compare" : "not_applicable_declared";
}

const char* normalization_text(normalization_mode m) {
  return m == normalization_mode::global_median ? "global_median" : "cfa_channel_median";
}

const char* flat_state_text(flat_state s) { return calib_detail::flat_name(s); }

json optional_number(const std::optional<double>& v) { return v ? json::number(*v) : json::null(); }

json recipe_json(const chain_recipe& r) {
  json o = json::object();
  o.set("light", json::string(to_string(r.light)));
  o.set("flat", json::string(to_string(r.flat)));
  o.set("exposure_rel_tol", optional_number(r.exposure_rel_tol));
  o.set("temperature_abs_tol_c", optional_number(r.temperature_abs_tol_c));
  o.set("temperature_policy", r.temp_policy ? json::string(temperature_policy_text(*r.temp_policy)) : json::null());
  o.set("normalization", r.normalization ? json::string(normalization_text(*r.normalization)) : json::null());
  o.set("q_min", optional_number(r.q_min));
  o.set("q_rel_sigma_max", optional_number(r.q_rel_sigma_max));
  o.set("flat_thermal_bound_adu", optional_number(r.flat_thermal_bound_adu));
  o.set("flat_thermal_rel_budget", optional_number(r.flat_thermal_rel_budget));
  o.set("decoder_black_abs_tol_adu", r.decoder_black_abs_tol_adu ? json::uinteger(*r.decoder_black_abs_tol_adu) : json::null());
  o.set("flat_division_offset", r.flat_division_offset ? json::string(to_string(*r.flat_division_offset)) : json::null());
  return o;
}

const char* status_text(candidate_status s) { return s == candidate_status::usable ? "usable" : "excluded"; }

const char* df_text(dark_flat_predicate p) {
  switch (p) {
    case dark_flat_predicate::absent: return "absent";
    case dark_flat_predicate::inc: return "inc";
    case dark_flat_predicate::sub: return "sub";
    case dark_flat_predicate::bad: return "bad";
  }
  std::abort();
}

json analysis_json(const calibration_analysis& a) {
  json o = json::object();
  json candidates = json::array();
  for (const candidate_analysis& c : a.candidates) {
    json e = json::object();
    e.set("role", json::string(role_text(c.role)));
    e.set("status", json::string(status_text(c.status)));
    json chains = json::array();
    for (const chain_verdict& v : c.chains) {
      json x = json::object();
      x.set("chain", json::string(v.chain));
      x.set("status", json::string(status_text(v.status)));
      x.set("reason", v.reason ? json::string(*v.reason) : json::null());
      chains.push(std::move(x));
    }
    e.set("chains", std::move(chains));
    candidates.push(std::move(e));
  }
  o.set("candidates", std::move(candidates));
  json warnings = json::array();
  for (const std::string& w : a.warnings) warnings.push(json::string(w));
  o.set("warnings", std::move(warnings));
  json predicates = json::object();
  predicates.set("dfb", json::string(a.predicates.dfb));
  predicates.set("df", json::string(df_text(a.predicates.df)));
  predicates.set("flat_available", a.predicates.flat_available ? json::string(flat_state_text(*a.predicates.flat_available)) : json::null());
  o.set("predicates", std::move(predicates));
  o.set("flat_thermal_within_budget", json::boolean(a.flat_thermal_within_budget));
  if (a.flat_thermal_pending_master) o.set("flat_thermal_pending_master", json::boolean(true));
  o.set("light_has_decoder_state", json::boolean(a.light_has_decoder_state));
  json admissible = json::array();
  for (const admissible_pair& p : a.admissible) {
    json x = json::object();
    x.set("light", json::string(to_string(p.light)));
    x.set("flat", json::string(to_string(p.flat)));
    x.set("partial", json::boolean(p.partial));
    admissible.push(std::move(x));
  }
  o.set("admissible", std::move(admissible));
  o.set("default_recipe", recipe_json(a.default_recipe));
  return o;
}

const char* outcome_text(group_outcome o) {
  switch (o) {
    case group_outcome::ready: return "ready";
    case group_outcome::candidate_choice_required: return "candidate_choice_required";
    case group_outcome::light_blocked: return "light_blocked";
    case group_outcome::params_missing: return "params_missing";
  }
  std::abort();
}

const char* choice_text(choice_kind k) {
  switch (k) {
    case choice_kind::absent: return "absent";
    case choice_kind::excluded_only: return "excluded_only";
    case choice_kind::only_compatible: return "only_compatible";
    case choice_kind::required: return "required";
  }
  std::abort();
}

json group_json(const group_proposal& g) {
  json o = json::object();
  o.set("id", json::string(g.id));
  json members = json::array();
  for (const batch_member& m : g.members) members.push(json::string(m.digest.sha256));
  o.set("members", std::move(members));
  o.set("outcome", json::string(outcome_text(g.outcome)));
  if (g.outcome == group_outcome::light_blocked) {
    o.set("block_category", json::string(to_string(g.block_category)));
    o.set("block_code", json::string(g.block_code));
  }
  json missing = json::array();
  for (const std::string& m : g.missing_params) missing.push(json::string(m));
  o.set("missing_params", std::move(missing));
  json candidates = json::array();
  for (const candidate_report& c : g.candidates) {
    json x = json::object();
    x.set("role", json::string(role_text(c.role)));
    x.set("key", json::string(c.key));
    x.set("status", json::string(status_text(c.status)));
    x.set("reason", json::string(c.reason));
    candidates.push(std::move(x));
  }
  o.set("candidates", std::move(candidates));
  json choices = json::array();
  for (const role_choice& c : g.choices) {
    json x = json::object();
    x.set("role", json::string(role_text(c.role)));
    x.set("kind", json::string(choice_text(c.kind)));
    json eligible = json::array();
    for (const std::string& k : c.eligible) eligible.push(json::string(k));
    x.set("eligible", std::move(eligible));
    x.set("chosen", c.chosen ? json::string(*c.chosen) : json::null());
    choices.push(std::move(x));
  }
  o.set("choices", std::move(choices));
  o.set("analysis", g.analysis ? analysis_json(*g.analysis) : json::null());
  json recipes = json::array();
  for (const chain_recipe& r : g.recipes) recipes.push(recipe_json(r));
  o.set("recipes", std::move(recipes));
  json omissions = json::array();
  for (const role_omission& m : g.omissions) {
    json x = json::object();
    x.set("role", json::string(role_text(m.role)));
    x.set("reason", json::string(m.reason));
    omissions.push(std::move(x));
  }
  o.set("omissions", std::move(omissions));
  return o;
}

}  // namespace

std::string batch_proposal::canonical() const {
  json doc = json::object();
  doc.set("format", json::string("astro.batch_proposal.v1"));
  json gs = json::array();
  for (const group_proposal& g : groups) gs.push(group_json(g));
  doc.set("groups", std::move(gs));
  json ex = json::array();
  for (const excluded_input& e : excluded) {
    json x = json::object();
    x.set("digest", json::string(e.digest.sha256));
    x.set("size_bytes", json::uinteger(e.digest.size_bytes));
    x.set("reason", json::string(e.reason));
    x.set("evidence", e.evidence_canonical.empty() ? json::null() : parse_json(e.evidence_canonical));
    ex.push(std::move(x));
  }
  doc.set("excluded", std::move(ex));
  json pending = json::object();
  for (const auto& [role, keys] : frame_pending_master) {
    json a = json::array();
    for (const std::string& k : keys) a.push(json::string(k));
    pending.set(role_text(role), std::move(a));
  }
  doc.set("frame_pending_master", std::move(pending));
  return canonical_json(doc);
}

namespace {

// ---------------------------------------------------------------- insiemi per ruolo e esclusioni (CL-05)

std::string exclusion_reason(const batch_input& in) {
  if (in.role.empty()) return "classification_missing";
  if (in.role == "unclassified") return in.unclassified_reason == "import_stopped" ? "import_stopped" : "unclassified." + in.unclassified_reason;
  if (!in.observation.empty() && in.observation.declared_conflict()) return "declared_conflict";
  return {};
}

}  // namespace

batch_proposal analyze_batch(const std::vector<batch_input>& inputs, const std::vector<slot_candidate>& slots,
                             const batch_analysis_request& request, const fault_injector& fault) {
  batch_proposal out;
  using digest_id = std::pair<std::string, std::uint64_t>;
  std::map<digest_id, std::vector<const batch_input*>> by_digest;
  for (const batch_input& in : inputs) by_digest[{in.digest.sha256, in.digest.size_bytes}].push_back(&in);

  std::vector<light_entity> lights;
  std::vector<const batch_input*> light_reps;
  batch_context ctx;
  ctx.request = &request;
  ctx.fault = &fault;
  for (auto& [id, copies] : by_digest) {
    std::sort(copies.begin(), copies.end(), [](const batch_input* a, const batch_input* b) { return a->input_id < b->input_id; });
    entity e;
    e.digest = content_digest{id.first, id.second};
    for (const batch_input* c : copies) e.ids.push_back(c->input_id);
    e.rep = copies.front();
    const bool agree = std::all_of(copies.begin(), copies.end(), [&](const batch_input* c) {
      return c->role == e.rep->role && c->declarations_canonical == e.rep->declarations_canonical;
    });
    std::string why = agree ? exclusion_reason(*e.rep) : "duplicate_digest_conflict";
    if (!why.empty()) {
      out.excluded.push_back(excluded_input{e.digest, e.ids, why, agree ? e.rep->evidence_canonical : std::string()});
      continue;
    }
    if (e.rep->role == "light") {
      lights.push_back(light_entity{e, is_raw(e.rep->observation.observed_image())});
      light_reps.push_back(e.rep);
      continue;
    }
    const std::optional<master_role> role = calibration_role_of(e.rep->role);
    if (!role) {
      throw stop_error(error_category::invalid_input, "classify.role", "ruolo fuori elenco", {{"ruolo", e.rep->role}});
    }
    if (!e.rep->descriptor) {
      out.frame_pending_master[*role].push_back(e.digest.sha256);
      continue;
    }
    ctx.candidates.push_back(make_candidate(*role, e.digest.sha256, e.digest, *e.rep->descriptor, e.rep->observation, true,
                                       e.rep->load_samples));
  }
  for (const slot_candidate& s : slots) {
    candidate c = make_candidate(s.role, s.node_key, content_digest{s.node_key, 0}, s.descriptor, s.observation, false, s.load_samples);
    c.proposed_frames = s.proposed_frames;
    c.ac.pending_master = s.proposed_frames.has_value();
    ctx.candidates.push_back(std::move(c));
  }
  {
    std::set<std::pair<int, std::string>> seen;
    for (const candidate& c : ctx.candidates) {
      if (!seen.emplace(static_cast<int>(c.role), c.key).second) {
        throw stop_error(error_category::invalid_input, "classify.duplicate_candidate", "due candidati con la stessa chiave nello stesso ruolo",
                         {{"ruolo", role_text(c.role)}, {"node_key", c.key}});
      }
    }
  }
  std::sort(ctx.candidates.begin(), ctx.candidates.end(), [](const candidate& a, const candidate& b) {
    return std::pair(static_cast<int>(a.role), a.key) < std::pair(static_cast<int>(b.role), b.key);
  });
  for (const auto& [role, key] : request.choices) {
    const bool known = std::any_of(ctx.candidates.begin(), ctx.candidates.end(),
                                   [&, role = role, key = key](const candidate& c) { return c.role == role && c.key == key; });
    if (!known) {
      throw stop_error(error_category::invalid_input, "classify.choice_unknown_candidate", "candidato inesistente per il ruolo",
                       {{"role", role_text(role)}, {"candidate", key}});
    }
  }

  // Gruppi (CL-07): chiave esatta, dominio ed esiti delle coppie.
  std::map<std::string, std::vector<std::size_t>> by_key;
  std::vector<light_key> keys;
  for (std::size_t i = 0; i < lights.size(); ++i) {
    keys.push_back(key_of_light(light_reps[i]->observation.observed_image(), ctx));
    by_key[keys.back().text].push_back(i);
  }
  std::vector<group_proposal> groups;
  for (const auto& [text, members] : by_key) {
    // Membri in ordine di digest (le entità escono già ordinate per digest).
    const std::size_t ref = members.front();
    group_proposal g;
    g.id = lights[ref].e.digest.sha256;
    std::vector<light_entity> ms;
    std::vector<const batch_input*> reps;
    for (const std::size_t i : members) {
      g.members.push_back(batch_member{lights[i].e.digest, lights[i].e.ids});
      ms.push_back(lights[i]);
      reps.push_back(light_reps[i]);
    }
    const image& light = reps.front()->observation.observed_image();
    const light_key& key = keys[ref];
    if (key.domain) {
      g.outcome = group_outcome::light_blocked;
      g.block_category = key.domain->category();
      g.block_code = key.domain->code();
    } else if (!key.missing.empty()) {
      g.outcome = group_outcome::params_missing;
      g.missing_params = key.missing;
    } else {
      const selection_result sel = choose_candidates(light, ctx);
      if (!sel.needs_choice && chosen_has_decoder_state(light, sel) && !request.params.decoder_black_abs_tol_adu) {
        g.outcome = group_outcome::params_missing;
        g.missing_params = {"decoder_black_abs_tol_adu"};
      } else {
        g.candidates = sel.reports;
        for (const role_result& rr : sel.roles) g.choices.push_back(rr.choice);
        if (sel.needs_choice) {
          g.outcome = group_outcome::candidate_choice_required;
        } else {
          g.outcome = group_outcome::ready;
          complete_ready_group(g, light, lights[ref].e.digest, sel, ctx);
          g.raw_inputs = frozen_raw_inputs(ms, reps);
          g.chosen = chosen_candidates(sel);
        }
      }
    }
    groups.push_back(std::move(g));
  }
  std::sort(groups.begin(), groups.end(), [](const group_proposal& a, const group_proposal& b) { return a.id < b.id; });
  out.groups = std::move(groups);
  std::sort(out.excluded.begin(), out.excluded.end(), [](const excluded_input& a, const excluded_input& b) {
    return std::pair(a.digest.sha256, a.digest.size_bytes) < std::pair(b.digest.sha256, b.digest.size_bytes);
  });
  return out;
}

calibration_selection select_recipe(const group_proposal& group, const chain_recipe& recipe) {
  switch (group.outcome) {
    case group_outcome::candidate_choice_required:
      throw stop_error(error_category::invalid_input, "classify.choice_required", "ruolo ambiguo senza scelta del candidato", {{"gruppo", group.id}});
    case group_outcome::light_blocked:
      throw stop_error(group.block_category, group.block_code, "gruppo di light bloccato dal suo dominio", {{"gruppo", group.id}});
    case group_outcome::params_missing:
      throw stop_error(error_category::invalid_input, "calib.param." + group.missing_params.front(),
                       "dichiarazione d'analisi assente", {{"gruppo", group.id}, {"parametro", group.missing_params.front()}});
    case group_outcome::ready: break;
  }
  ASTRO_ASSERT(group.analysis.has_value());
  require_admissible(*group.analysis, recipe);
  calibration_selection selection;
  selection.recipe = recipe;
  selection.raw_inputs = group.raw_inputs;
  for (const batch_member& m : group.members) selection.lights.push_back(m.digest);
  for (const selected_candidate& c : group.chosen) {
    if (recipe_uses_role(recipe, c.role)) selection.candidates.push_back(c);
  }
  selection.declarations = group.analysis->declarations;
  return selection;
}

// ---------------------------------------------------------------- analisi sugli ingressi persistiti (CL-12)

namespace {

constexpr const char* analysis_params_key = "batch.analysis_params";
constexpr const char* analysis_params_format = "astro.batch_analysis_params.v1";
constexpr const char* input_slot_prefix = "batch.input.";

[[noreturn]] void bad_params(const std::string& why) {
  throw stop_error(error_category::invalid_input, "batch.analysis_params", "parametri d'analisi non validi", {{"motivo", why}});
}

double param_number(const json& v, const std::string& key) {
  if (v.type() == json::kind::integer) return static_cast<double>(v.as_integer());
  if (v.type() != json::kind::number) bad_params(key + ": numero atteso");
  return v.as_number();
}

// Membro `master` del documento: solo i parametri propri dei master (le tolleranze comuni stanno al livello superiore).
void parse_master_member(const json& member, master_analysis_params& m) {
  if (member.type() != json::kind::object) bad_params("master: oggetto atteso");
  for (std::size_t i = 0; i < member.keys().size(); ++i) {
    const std::string& name = member.keys()[i];
    const std::string key = "master." + name;
    const json& v = member.items()[i];
    if (name == "saturation_adu") m.saturation_adu = param_number(v, key);
    else if (name == "saturated_fraction_max") m.saturated_fraction_max = param_number(v, key);
    else if (name == "flat_min_signal_adu") m.flat_min_signal_adu = param_number(v, key);
    else if (name == "flat_level_rel_tol") m.flat_level_rel_tol = param_number(v, key);
    else if (name == "offset_adu") m.offset_adu = param_number(v, key);
    else bad_params(key + ": chiave sconosciuta");
  }
}

// `settings` `batch.analysis_params`: assente = nessuna dichiarazione (i parametri richiesti risultano `params_missing`).
// Parser unico: le chiavi dei light vanno in `d`, il membro `master` in `m`.
void parse_analysis_params(const sqlite_dag_store& store, calibration_declarations& d, master_analysis_params& m) {
  const std::optional<std::string> text = store.get_setting(analysis_params_key);
  if (!text) return;
  const json doc = parse_json(*text);
  const json* format = doc.type() == json::kind::object ? doc.find("format") : nullptr;
  if (format == nullptr || format->type() != json::kind::string || format->as_string() != analysis_params_format) {
    bad_params("formato errato");
  }
  for (std::size_t i = 0; i < doc.keys().size(); ++i) {
    const std::string& key = doc.keys()[i];
    const json& v = doc.items()[i];
    if (key == "format") continue;
    if (key == "master") parse_master_member(v, m);
    else if (key == "exposure_rel_tol") d.exposure_rel_tol = param_number(v, key);
    else if (key == "temperature_abs_tol_c") d.temperature_abs_tol_c = param_number(v, key);
    else if (key == "q_min") d.q_min = param_number(v, key);
    else if (key == "q_rel_sigma_max") d.q_rel_sigma_max = param_number(v, key);
    else if (key == "flat_thermal_bound_adu") d.flat_thermal_bound_adu = param_number(v, key);
    else if (key == "flat_thermal_rel_budget") d.flat_thermal_rel_budget = param_number(v, key);
    else if (key == "decoder_black_abs_tol_adu") {
      if (v.type() != json::kind::integer || v.as_integer() < 0 || v.as_integer() > 4294967295LL) bad_params(key + ": intero non negativo atteso");
      d.decoder_black_abs_tol_adu = static_cast<std::uint32_t>(v.as_integer());
    } else if (key == "temperature_policy") {
      if (v.type() != json::kind::string) bad_params(key + ": stringa attesa");
      if (v.as_string() == "compare") d.temp_policy = temperature_policy::compare;
      else if (v.as_string() == "not_applicable_declared") d.temp_policy = temperature_policy::not_applicable_declared;
      else bad_params(key + ": valore sconosciuto");
    } else if (key == "normalization") {
      if (v.type() != json::kind::string) bad_params(key + ": stringa attesa");
      if (v.as_string() == "global_median") d.normalization = normalization_mode::global_median;
      else if (v.as_string() == "cfa_channel_median") d.normalization = normalization_mode::cfa_channel_median;
      else bad_params(key + ": valore sconosciuto");
    } else {
      bad_params(key + ": chiave sconosciuta");
    }
  }
}

calibration_declarations read_analysis_params(const sqlite_dag_store& store) {
  calibration_declarations d;
  master_analysis_params ignored;
  parse_analysis_params(store, d, ignored);
  return d;
}

const json& report_member(const json& obj, const char* key) {
  const json* v = obj.type() == json::kind::object ? obj.find(key) : nullptr;
  if (v == nullptr) report_malformed(key);
  return *v;
}

std::uint32_t report_u32(const json& obj, const char* key) {
  const json& v = report_member(obj, key);
  if (v.type() != json::kind::integer || v.as_integer() < 0 || v.as_integer() > 4294967295LL) report_malformed(key);
  return static_cast<std::uint32_t>(v.as_integer());
}

std::optional<std::uint32_t> report_optional_u32(const json& obj, const char* key) {
  const json& v = report_member(obj, key);
  if (v.type() == json::kind::null) return std::nullopt;
  return report_u32(obj, key);
}

const std::string& report_text(const json& obj, const char* key) {
  const json& v = report_member(obj, key);
  if (v.type() != json::kind::string) report_malformed(key);
  return v.as_string();
}

metadata_origin origin_of_text(const std::string& t) {
  for (const metadata_origin o : {metadata_origin::header, metadata_origin::declared, metadata_origin::decoder}) {
    if (t == to_string(o)) return o;
  }
  report_malformed("origin");
}

template <class T>
void put_field(std::optional<metadata_value<T>>& field, const json& entry, T value) {
  field = metadata_value<T>{std::move(value), {origin_of_text(report_text(entry, "origin")), report_text(entry, "keyword")}};
}

// Immagine di sole intestazioni (nessun campione) dal report `astro.import_report.v1`.
image image_of_report(const json& report) {
  const json& im = report_member(report, "image");
  image out;
  out.geometry = image_geometry(report_u32(im, "width"), report_u32(im, "height"), report_u32(im, "planes"));
  out.geometry.binning_x = report_optional_u32(im, "binning_x");
  out.geometry.binning_y = report_optional_u32(im, "binning_y");
  const std::string& layout = report_text(im, "layout");
  bool known = false;
  for (const plane_layout l : {plane_layout::mono, plane_layout::cfa, plane_layout::rgb, plane_layout::multi_plane}) {
    if (layout == calib_detail::layout_name(l)) {
      out.layout = l;
      known = true;
    }
  }
  if (!known) report_malformed("layout");
  const std::string& type_text = report_text(im, "sample_type");
  if (type_text == "f32") out.type = sample_type::f32;
  else if (type_text == "f64") out.type = sample_type::f64;
  else report_malformed("sample_type");
  const json& cfa = report_member(im, "cfa");
  if (cfa.type() == json::kind::object) {
    const std::string& p = report_text(cfa, "pattern");
    cfa_layout c{cfa_pattern::rggb, static_cast<std::uint8_t>(report_u32(cfa, "x_offset")), static_cast<std::uint8_t>(report_u32(cfa, "y_offset"))};
    bool found = false;
    for (const cfa_pattern q : {cfa_pattern::rggb, cfa_pattern::bggr, cfa_pattern::grbg, cfa_pattern::gbrg}) {
      std::string upper = calib_detail::pattern_name(q);
      for (char& ch : upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      if (p == upper) {
        c.pattern = q;
        found = true;
      }
    }
    if (!found) report_malformed("cfa.pattern");
    out.cfa = c;
  }
  const json& unit = report_member(im, "unit");
  if (unit.type() == json::kind::string) {
    if (unit.as_string() == "adu") out.unit = pixel_unit::adu;
    else if (unit.as_string() == "electron") out.unit = pixel_unit::electron;
    else if (unit.as_string() == "dimensionless") out.unit = pixel_unit::dimensionless;
    else report_malformed("unit");
  }
  const json& state = report_member(report, "state");
  const json& phase = report_member(state, "phase");
  const std::string& phase_text = report_text(phase, "value");
  bool phase_known = false;
  for (const process_phase p : {process_phase::raw, process_phase::calibrated, process_phase::prestacked}) {
    if (phase_text == to_string(p)) {
      out.phase = metadata_value<process_phase>{p, {origin_of_text(report_text(phase, "origin")), {}}};
      phase_known = true;
    }
  }
  if (!phase_known) report_malformed("phase");
  const json& lin = report_member(state, "linearity");
  const std::string& lin_text = report_text(lin, "value");
  if (lin_text == "linear") out.lin = linearity::linear;
  else if (lin_text == "nonlinear") out.lin = linearity::nonlinear;
  else if (lin_text != "unknown") report_malformed("linearity");
  const json& lin_origin = report_member(lin, "origin");
  if (lin_origin.type() == json::kind::string) out.lin_origin.value = origin_of_text(lin_origin.as_string());
  const json& meta = report_member(report, "metadata");
  frame_metadata& m = out.metadata;
  for (std::size_t i = 0; i < meta.keys().size(); ++i) {
    const std::string& key = meta.keys()[i];
    const json& e = meta.items()[i];
    const json& v = report_member(e, "value");
    const auto number = [&]() {
      if (v.type() == json::kind::integer) return static_cast<double>(v.as_integer());
      if (v.type() != json::kind::number) report_malformed("metadata");
      return v.as_number();
    };
    const auto string = [&]() {
      if (v.type() != json::kind::string) report_malformed("metadata");
      return v.as_string();
    };
    const auto integer = [&]() {
      if (v.type() != json::kind::integer) report_malformed("metadata");
      return v.as_integer();
    };
    if (key == "exposure_s") put_field(m.exposure_s, e, number());
    else if (key == "gain") put_field(m.gain, e, number());
    else if (key == "conversion_gain_e_per_adu") put_field(m.conversion_gain_e_per_adu, e, number());
    else if (key == "offset_adu") put_field(m.offset_adu, e, number());
    else if (key == "temperature_c") put_field(m.temperature_c, e, number());
    else if (key == "saturation_adu") put_field(m.saturation_adu, e, number());
    else if (key == "pedestal_adu") put_field(m.pedestal_adu, e, number());
    else if (key == "filter") put_field(m.filter, e, string());
    else if (key == "camera") put_field(m.camera, e, string());
    else if (key == "imagetyp") put_field(m.imagetyp, e, string());
    else if (key == "readout_mode") put_field(m.readout_mode, e, string());
    else if (key == "row_order") put_field(m.row_order, e, string());
    else if (key == "roi_x") put_field(m.roi_x, e, integer());
    else if (key == "roi_y") put_field(m.roi_y, e, integer());
    else report_malformed("metadata");
  }
  return out;
}

json default_declarations_json() { return default_declarations(); }

// Immagine completa di un file FITS verificato contro `digest` (OB-11), per il controllo termico.
image load_full_image(const std::string& path, const content_digest& digest, memory_pool& pool, const cancel_token& cancel) {
  if (cancel.requested()) throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto");
  const fits_input input = open_fits_input(path);
  const fits_plan plan = plan_fits(input);
  const std::vector<std::uint8_t> bytes = read_verified_input(input, path, digest, pool.budget_bytes(), &plan);
  return decode_fits(bytes, pool.budget_bytes());
}

const artifact_record& published_image(const std::vector<artifact_record>& artifacts, const std::string& node_key) {
  for (const artifact_record& a : artifacts) {
    if (a.kind == "image" && a.output_role == raw_import_image_role && a.present) return a;
  }
  throw stop_error(error_category::integrity, "batch.image_missing", "il nodo non ha l'immagine pubblicata", {{"node", node_key}});
}

// Immagine osservata di un artefatto FITS pubblicato: solo intestazioni.
image image_of_artifact(const sqlite_dag_store& store, const artifact_record& a, memory_pool& pool, const cancel_token& cancel) {
  const std::string path = store.artifact_path(a.rel_path).string();
  const fits_input input = open_fits_input(path);
  const fits_inventory inv = inventory_fits(input, fits_read_options{}, io_memory(pool), &cancel);
  return image_of_report(import_report_json(inv, content_digest{a.sha256, a.size_bytes}));
}

// Stato del decoder del record, con la coerenza del motivo del nero: `black.pattern` ha `pattern_rows × pattern_cols` valori.
decoder_state checked_decoder_state(const json& record) {
  decoder_state state = decoder_state_of_record(record);
  if (std::uint64_t{state.black_pattern_rows} * state.black_pattern_cols != state.black_pattern.size()) report_malformed("black.pattern");
  return state;
}

decoder_identity identity_of_record(const json& record) {
  const json& version_number = report_member(record, "libraw_version_number");
  const json& capabilities = report_member(record, "libraw_capabilities");
  if (version_number.type() != json::kind::integer || capabilities.type() != json::kind::integer) report_malformed("decoder");
  return decoder_identity{report_text(record, "libraw_version"), static_cast<std::uint32_t>(version_number.as_integer()),
                          static_cast<std::uint32_t>(capabilities.as_integer()), report_text(record, "libraw_tarball_sha256")};
}

std::string reason_of_evidence(const std::string& evidence_canonical) {
  const json doc = parse_json(evidence_canonical);
  const json* r = doc.type() == json::kind::object ? doc.find("reason") : nullptr;
  if (r == nullptr || r->type() != json::kind::string) {
    throw stop_error(error_category::integrity, "batch.report_malformed", "l'evidenza della classificazione non ha `reason`");
  }
  return r->as_string();
}

master_descriptor descriptor_of_declarations(const json& d) {
  master_descriptor out;
  const std::string& role = report_text(d, "role");
  const std::optional<master_role> r = calibration_role_of(role);
  if (!r) report_malformed("master_descriptor.role");
  out.role = *r;
  for (std::size_t i = 0; i < d.keys().size(); ++i) {
    const std::string& key = d.keys()[i];
    const json& v = d.items()[i];
    if (key == "role" || v.type() == json::kind::null) continue;
    const std::string s = v.type() == json::kind::string ? v.as_string() : std::string();
    const auto pick = [&](std::initializer_list<const char*> allowed) {
      for (const char* a : allowed) {
        if (s == a) return;
      }
      report_malformed("master_descriptor");
    };
    if (key == "additive") {
      pick({"bias_included", "bias_subtracted", "unknown"});
      out.additive = s == "bias_included" ? additive_state::bias_included : s == "bias_subtracted" ? additive_state::bias_subtracted : additive_state::unknown;
    } else if (key == "scaling") {
      pick({"unscaled", "scaled", "unknown"});
      out.scaling = s == "unscaled" ? scaling_state::unscaled : s == "scaled" ? scaling_state::scaled : scaling_state::unknown;
    } else if (key == "flat") {
      pick({"raw", "corrected_unnormalized", "calibrated", "unknown"});
      out.flat = s == "raw" ? flat_state::raw : s == "corrected_unnormalized" ? flat_state::corrected_unnormalized : s == "calibrated" ? flat_state::calibrated : flat_state::unknown;
    } else if (key == "combine_count") {
      out.combine_count = static_cast<std::uint32_t>(v.as_integer());
    } else if (key == "combine_method") {
      pick({"mean", "median", "other"});
      out.combine = s == "mean" ? combine_method::mean : s == "median" ? combine_method::median : combine_method::other;
    } else if (key == "rejection") {
      pick({"none", "sigma_clip"});
      out.rejection = s == "sigma_clip" ? master_rejection::sigma_clip : master_rejection::none;
    } else if (key == "variance_estimator") {
      pick({"empirical"});
      out.estimator = variance_estimator::empirical;
    }
    else report_malformed("master_descriptor");
  }
  return out;
}

bool is_calibration_role_text(const std::string& role) { return calibration_role_of(role).has_value(); }

// Ingresso registrato con la sua classificazione effettiva, il suo report e, per un RAW, il record del decoder.
batch_input input_from_store(const sqlite_dag_store& store, const input_record& rec, memory_pool& pool,
                             const cancel_token& cancel) {
  batch_input in;
  in.input_id = rec.id;
  in.digest = content_digest{rec.sha256, rec.size_bytes};
  const std::vector<classification_record> history = store.classifications(rec.id);
  if (history.empty()) return in;
  const classification_record& cur = history.back();
  in.role = cur.role;
  in.declarations_canonical = cur.declarations_canonical;
  in.evidence_canonical = cur.evidence_canonical;
  // Un import fermato non ha report né immagine, anche se una revisione successiva lo classifica (o lo lascia
  // `unclassified`): l'input si esclude col motivo dell'import, con categoria e codice dello stop.
  if (history.front().role == "unclassified" && reason_of_evidence(history.front().evidence_canonical) == "import_stopped") {
    in.role = "unclassified";
    in.unclassified_reason = "import_stopped";
    in.declarations_canonical = history.front().declarations_canonical;
    in.evidence_canonical = history.front().evidence_canonical;
    return in;
  }
  if (cur.role == "unclassified") {
    in.unclassified_reason = reason_of_evidence(cur.evidence_canonical);
    return in;
  }
  if (is_calibration_role_text(cur.role)) {
    const json decl = parse_json(cur.declarations_canonical);
    if (const json* d = decl.find("master_descriptor")) in.descriptor = descriptor_of_declarations(*d);
  }
  const std::string slot_name = input_slot_prefix + std::to_string(rec.id);
  const std::optional<slot_record> slot = store.get_slot(slot_name);
  if (!slot) {
    throw stop_error(error_category::integrity, "batch.input_slot_missing", "input classificato senza il suo slot d'import",
                     {{"input_id", std::to_string(rec.id)}});
  }
  if (rec.format == fits_input_format().prefix) {
    in.observation = make_observation(image_of_report(read_import_report(store, slot->target_node_key)), cur.declarations_canonical, std::nullopt);
    // I campioni si leggono solo se il controllo termico di `flat.bias` li chiede (CL-12).
    if (cur.role == "flat" || cur.role == "bias") {
      in.load_samples = [path = rec.path, digest = in.digest, declarations = cur.declarations_canonical, &pool, &cancel]() {
        return make_observation(load_full_image(path, digest, pool, cancel), declarations, std::nullopt).observed_image();
      };
    }
    return in;
  }
  const std::optional<node_spec> node = store.get_node(slot->target_node_key);
  const json* record = node ? node->params().find("decoder") : nullptr;
  if (record == nullptr || record->type() != json::kind::object) {
    throw stop_error(error_category::integrity, "batch.decoder_record_missing", "nodo d'import RAW senza il record del decoder",
                     {{"input_id", std::to_string(rec.id)}});
  }
  image img = image_of_artifact(store, published_image(store.artifacts_for(slot->target_node_key), slot->target_node_key), pool, cancel);
  img.decoder_state = checked_decoder_state(*record);
  in.observation = make_observation(img, cur.declarations_canonical, identity_of_record(*record));
  return in;
}

}  // namespace

master_analysis_params read_master_analysis_params(const sqlite_dag_store& store) {
  calibration_declarations d;
  master_analysis_params m;
  parse_analysis_params(store, d, m);
  m.exposure_rel_tol = d.exposure_rel_tol;
  m.temperature_abs_tol_c = d.temperature_abs_tol_c;
  m.temp_policy = d.temp_policy;
  m.decoder_black_abs_tol_adu = d.decoder_black_abs_tol_adu;
  return m;
}

batch_proposal analyze_batch(const sqlite_dag_store& store, const candidate_choices& choices, memory_pool& pool,
                             const cancel_token& cancel, const fault_injector& fault) {
  batch_analysis_request request;
  request.params = read_analysis_params(store);
  request.choices = choices;
  std::vector<batch_input> inputs;
  for (const input_record& rec : store.inputs()) inputs.push_back(input_from_store(store, rec, pool, cancel));

  std::vector<slot_candidate> slots;
  for (const slot_record& s : store.slots()) {
    if (s.detached) continue;
    const std::optional<node_spec> node = store.get_node(s.target_node_key);
    if (!node) {
      throw stop_error(error_category::integrity, "batch.slot_node_missing", "slot senza il suo nodo", {{"slot", s.slot_name}});
    }
    if (node->operator_id() != master_operator_id) continue;
    const slot_view view = store.read_slot(s.slot_name);
    if (view.state != freshness::current || !view.artifact) continue;
    slot_candidate c;
    c.node_key = s.target_node_key;
    c.descriptor = master_descriptor_from_node(*node);
    c.role = c.descriptor.role;
    c.observation = make_observation(image_of_artifact(store, *view.artifact, pool, cancel),
                                     canonical_json(default_declarations_json()), std::nullopt);
    if (c.role == master_role::flat || c.role == master_role::bias) {
      c.load_samples = [&store, artifact = *view.artifact, &pool, &cancel]() {
        return make_observation(load_full_image(store.artifact_path(artifact.rel_path).string(),
                                                content_digest{artifact.sha256, artifact.size_bytes}, pool, cancel),
                                canonical_json(default_declarations_json()), std::nullopt)
            .observed_image();
      };
    }
    slots.push_back(std::move(c));
  }
  return analyze_batch(inputs, slots, request, fault);
}

}  // namespace astro
