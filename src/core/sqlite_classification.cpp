#include <algorithm>
#include <map>

#include "astro/json.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;

namespace {

constexpr const char* declarations_format = "astro.input_declarations.v1";

[[noreturn]] void reject(const char* code, const char* what, const classification_record& r, const std::string& why = "",
                       const std::string& detail = "") {
  stop_error::context_list context = {{"input_id", std::to_string(r.input_id)}, {"role", r.role}};
  if (!why.empty()) context.emplace_back("causa", why);
  if (!detail.empty()) context.emplace_back("dettaglio", detail);
  throw stop_error(error_category::invalid_input, code, what, std::move(context));
}

bool is_calibration_role(const std::string& role) {
  return role == "bias" || role == "dark" || role == "dark_flat" || role == "flat";
}

bool one_of(const std::string& v, std::initializer_list<const char*> allowed) {
  return std::any_of(allowed.begin(), allowed.end(), [&](const char* a) { return v == a; });
}

// Documento JSON canonico di cui `text` è la forma JCS esatta; altrimenti nullopt, `why` è il codice del parser o
// `not_canonical` e `detail` il messaggio del parser.
std::optional<json> canonical_document(const std::string& text, std::string& why, std::string& detail) {
  try {
    json doc = parse_json(text);
    if (canonical_json(doc) == text) return doc;
    why = "not_canonical";
  } catch (const stop_error& e) {
    if (e.category() != error_category::invalid_input && e.category() != error_category::limit_exceeded) throw;
    why = e.code();
    detail = e.message();
  }
  return std::nullopt;
}

// Ruolo, origine e motivo (CL-02): `unclassified` porta sempre un motivo, `user` per l'utente; gli altri ruoli non ne hanno.
void validate_role(const classification_record& r) {
  if (!one_of(r.role, {"light", "bias", "dark", "dark_flat", "flat", "unclassified"})) reject("classify.role", "ruolo fuori elenco", r);
  if (r.source != "rule" && r.source != "user") reject("classify.role", "origine fuori elenco", r);
  if ((r.source == "rule") != r.rule_id.has_value() || (r.rule_id && r.rule_id->empty())) {
    reject("classify.role", "rule_id incoerente con l'origine", r);
  }
  std::string parse_why, parse_detail;
  const std::optional<json> evidence = canonical_document(r.evidence_canonical, parse_why, parse_detail);
  if (!evidence || evidence->type() != json::kind::object) reject("classify.role", "evidenza non canonica", r, parse_why, parse_detail);
  const json* reason = evidence->find("reason");
  if (r.role != "unclassified") {
    if (reason != nullptr) reject("classify.role", "motivo su un ruolo classificato", r);
    return;
  }
  if (reason == nullptr || reason->type() != json::kind::string) reject("classify.role", "unclassified senza motivo", r);
  const std::string& why = reason->as_string();
  if (r.source == "user" ? why != "user"
                         : !one_of(why, {"imagetyp_unrecognized", "imagetyp_absent", "raw_no_type_metadata", "import_stopped"})) {
    reject("classify.role", "motivo non ammesso per l'origine", r);
  }
}

bool positive_uint32(const json& v) {
  return v.type() == json::kind::integer && v.as_integer() >= 1 && v.as_integer() <= 4294967295LL;
}

bool valid_declared_field(const std::string& key, const json& v) {
  if (key == "binning_x" || key == "binning_y") return positive_uint32(v);
  if (key == "unit") return v.type() == json::kind::string && one_of(v.as_string(), {"adu", "electron", "dimensionless"});
  if (key == "linearity") return v.type() == json::kind::string && one_of(v.as_string(), {"linear", "nonlinear"});
  return false;
}

bool valid_frame_metadata_field(const std::string& key, const json& v) {
  const bool number = v.type() == json::kind::integer || v.type() == json::kind::number;
  if (one_of(key, {"exposure_s", "gain", "conversion_gain_e_per_adu", "offset_adu", "temperature_c", "saturation_adu", "pedestal_adu"})) {
    return number;
  }
  if (key == "roi_x" || key == "roi_y") return v.type() == json::kind::integer;
  if (one_of(key, {"filter", "camera", "imagetyp", "readout_mode", "row_order"})) return v.type() == json::kind::string;
  return false;
}

// Descrittore dichiarato di un master esterno (ADR-16.a): ruolo uguale a quello della revisione, campi opzionali nei valori
// di `descriptor_json` (calibration_job.cpp), nessuna chiave sconosciuta.
bool valid_master_descriptor(const json& d, const std::string& role) {
  if (d.type() != json::kind::object) return false;
  const json* r = d.find("role");
  if (r == nullptr || r->type() != json::kind::string || r->as_string() != role) return false;
  for (std::size_t i = 0; i < d.keys().size(); ++i) {
    const std::string& key = d.keys()[i];
    const json& v = d.items()[i];
    if (key == "role" || v.type() == json::kind::null) continue;
    const bool text = v.type() == json::kind::string;
    const std::string& s = text ? v.as_string() : key;
    if (key == "additive" ? !(text && one_of(s, {"bias_included", "bias_subtracted", "unknown"}))
        : key == "scaling" ? !(text && one_of(s, {"unscaled", "scaled", "unknown"}))
        : key == "flat" ? !(text && one_of(s, {"raw", "corrected_unnormalized", "calibrated", "unknown"}))
        : key == "combine_method" ? !(text && one_of(s, {"mean", "median", "other"}))
        : key == "rejection" ? !(text && one_of(s, {"none", "sigma_clip"}))
        : key == "variance_estimator" ? !(text && s == "empirical")
        : key == "combine_count" ? !positive_uint32(v)
                                 : true) {
      return false;
    }
  }
  return true;
}

void validate_declarations(const classification_record& r) {
  std::string why, detail;
  const auto bad = [&](const char* what) { reject("classify.declaration", what, r, why, detail); };
  const std::optional<json> doc = canonical_document(r.declarations_canonical, why, detail);
  if (!doc || doc->type() != json::kind::object) bad("dichiarazioni non canoniche");
  const json* format = doc->find("format");
  if (format == nullptr || format->type() != json::kind::string || format->as_string() != declarations_format) bad("formato errato");
  for (const std::string& key : doc->keys()) {
    if (key != "format" && key != "declared_image_fields" && key != "frame_metadata" && key != "master_descriptor") {
      bad("sezione sconosciuta");
    }
  }
  const json* fields = doc->find("declared_image_fields");
  if (fields == nullptr || fields->type() != json::kind::object) bad("declared_image_fields assente o non oggetto");
  for (std::size_t i = 0; i < fields->keys().size(); ++i) {
    if (!valid_declared_field(fields->keys()[i], fields->items()[i])) bad("campo dichiarato non valido");
  }
  if (const json* meta = doc->find("frame_metadata")) {
    if (meta->type() != json::kind::object) bad("frame_metadata non oggetto");
    for (std::size_t i = 0; i < meta->keys().size(); ++i) {
      if (!valid_frame_metadata_field(meta->keys()[i], meta->items()[i])) bad("campo di frame_metadata non valido");
    }
  }
  if (const json* descriptor = doc->find("master_descriptor")) {
    if (!is_calibration_role(r.role) || !valid_master_descriptor(*descriptor, r.role)) bad("master_descriptor non valido");
  }
}

classification_record row_from(const statement& st) {
  classification_record r;
  r.input_id = st.u64(0);
  r.revision = st.u64(1);
  r.role = st.text(2);
  r.source = st.text(3);
  r.rule_id = st.opt_text(4);
  r.evidence_canonical = st.text(5);
  r.declarations_canonical = st.text(6);
  return r;
}

}  // namespace

std::vector<classification_record> sqlite_dag_store::append_classifications(const std::vector<classification_record>& records) {
  require_writable();
  std::lock_guard lock(mutex_);
  transaction tx(db_);
  std::map<std::uint64_t, std::uint64_t> base;     // input_id -> ultima revisione prima di questa chiamata
  std::map<std::uint64_t, std::uint64_t> running;  // aggiornata riga per riga
  for (const classification_record& r : records) {
    if (base.count(r.input_id) == 0) {
      statement known(db_, "SELECT 1 FROM inputs WHERE id=?1");
      if (!known.bind_u64(1, r.input_id).step()) reject("classify.unknown_input", "input non registrato", r);
      statement top(db_, "SELECT coalesce(max(revision), 0) FROM input_classifications WHERE input_id=?1");
      if (!top.bind_u64(1, r.input_id).step()) throw stop_error(error_category::integrity, "store.corrupt", "revisione non leggibile");
      base[r.input_id] = running[r.input_id] = top.u64(0);
    }
    validate_role(r);
    if (r.source == "rule" && running[r.input_id] != 0) reject("classify.role", "le regole scrivono solo la revisione 1", r);
    validate_declarations(r);
    ++running[r.input_id];
  }
  std::map<std::uint64_t, std::uint64_t> next = base;
  std::vector<classification_record> written;
  for (classification_record r : records) {
    r.revision = ++next[r.input_id];
    check_fault("classify.insert");
    statement ins(db_, "INSERT INTO input_classifications(input_id, revision, role, source, rule_id, evidence_canonical, "
                       "declarations_canonical, record_version) VALUES(?1,?2,?3,?4,?5,?6,?7,1)");
    ins.bind_u64(1, r.input_id).bind_u64(2, r.revision).bind(3, r.role).bind(4, r.source).bind_opt(5, r.rule_id);
    ins.bind(6, r.evidence_canonical).bind(7, r.declarations_canonical).run();
    written.push_back(std::move(r));
  }
  tx.commit();
  return written;
}

std::vector<classification_record> sqlite_dag_store::classifications(std::uint64_t input_id) const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT input_id, revision, role, source, rule_id, evidence_canonical, declarations_canonical "
                    "FROM input_classifications WHERE input_id=?1 ORDER BY revision");
  st.bind_u64(1, input_id);
  std::vector<classification_record> out;
  while (st.step()) out.push_back(row_from(st));
  return out;
}

}  // namespace astro
