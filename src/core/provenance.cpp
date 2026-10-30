#include "astro/provenance.hpp"

#include <fitsio.h>
#include <openssl/crypto.h>
#include <sqlite3.h>

#include <cstdlib>

#include "astro/build_info.hpp"

namespace astro {

namespace {

[[noreturn]] void bad(const char* message) {
  throw stop_error(error_category::invalid_input, "dag.bad_provenance", message);
}

const char* state_name(job_state state) {
  switch (state) {
    case job_state::running: return "running";
    case job_state::completed: return "completed";
    case job_state::failed: return "failed";
    case job_state::cancelled: return "cancelled";
    case job_state::interrupted: return "interrupted";
  }
  std::abort();
}

json digest_json(const content_digest& d) {
  json o = json::object();
  o.set("sha256", json::string(d.sha256));
  o.set("size_bytes", json::uinteger(d.size_bytes));
  return o;
}

json stop_json(const stop_error& e) {
  json context = json::array();
  for (const auto& [key, value] : e.context()) {
    json item = json::object();
    item.set("key", json::string(key));
    item.set("value", json::string(value));
    context.push(std::move(item));
  }
  json o = json::object();
  o.set("category", json::string(to_string(e.category())));
  o.set("code", json::string(e.code()));
  o.set("message", json::string(e.message()));
  o.set("context", std::move(context));
  return o;
}

void validate(const provenance_record& r) {
  switch (r.state) {
    case job_state::running:
    case job_state::interrupted: bad("stato finale richiesto");
    case job_state::completed:
      if (r.stop) bad("job completed con arresto");
      return;
    case job_state::failed:
    case job_state::cancelled:
      if (!r.stop) bad("job fermato senza arresto");
      if (!r.output_digests.empty()) bad("job fermato con output");
      if ((r.state == job_state::cancelled) != (r.stop->category() == error_category::cancelled)) {
        bad("stato incoerente con la categoria dell'arresto");
      }
      return;
  }
}

json versions_json(const library_versions& v) {
  if (v.source_revision.empty()) bad("revisione del sorgente assente");
  json versions = json::object();
  versions.set("product", json::string(v.product));
  versions.set("source_revision", json::string(v.source_revision));
  versions.set("source_dirty", v.source_dirty ? json::boolean(*v.source_dirty) : json::null());
  versions.set("cfitsio", json::string(v.cfitsio));
  versions.set("sqlite", json::string(v.sqlite));
  versions.set("openssl", json::string(v.openssl));
  return versions;
}

}  // namespace

library_versions current_library_versions() {
  const std::string cfitsio = std::to_string(CFITSIO_MAJOR) + "." + std::to_string(CFITSIO_MINOR) + "." +
                              std::to_string(CFITSIO_MICRO);
  const BuildInfo info = build_info();
  return library_versions{info.version, info.revision, info.dirty, cfitsio, sqlite3_libversion(), OpenSSL_version(OPENSSL_VERSION)};
}

json provenance_json(const provenance_record& r) {
  validate(r);
  json versions = versions_json(r.versions);
  json inputs = json::array();
  for (const auto& in : r.spec.inputs()) {
    if (in.is_file()) inputs.push(digest_json(in.file_digest()));
  }
  json warnings = json::array();
  for (const auto& w : r.warnings) warnings.push(json::string(w));
  json outputs = json::array();
  for (const auto& d : r.output_digests) outputs.push(digest_json(d));
  json doc = json::object();
  doc.set("node_spec", r.spec.to_json());
  doc.set("node_key", json::string(r.spec.key()));
  doc.set("input_digests", std::move(inputs));
  doc.set("versions", std::move(versions));
  doc.set("generation", json::uinteger(r.generation));
  doc.set("state", json::string(state_name(r.state)));
  doc.set("warnings", std::move(warnings));
  if (r.state == job_state::completed) doc.set("output_digests", std::move(outputs));
  if (r.stop) doc.set("stop", stop_json(*r.stop));
  if (r.budget) doc.set("budget", budget_json(*r.budget));
  if (r.details) doc.set("details", *r.details);
  if (r.resume) doc.set("resume", *r.resume);
  return doc;
}

std::string provenance_canonical(const provenance_record& record) { return canonical_json(provenance_json(record)); }

json stop_before_work_json(const stop_before_work_record& r) {
  if (r.state != job_state::failed && r.state != job_state::cancelled) bad("job fermato prima del lavoro senza stato finale di arresto");
  if ((r.state == job_state::cancelled) != (r.stop.category() == error_category::cancelled)) {
    bad("stato incoerente con la categoria dell'arresto");
  }
  json doc = json::object();
  doc.set("node_key", json::string(r.node_key));
  doc.set("versions", versions_json(r.versions));
  doc.set("generation", json::uinteger(r.generation));
  doc.set("state", json::string(state_name(r.state)));
  doc.set("stop", stop_json(r.stop));
  doc.set("stop_before_work", json::boolean(true));
  if (r.resume) doc.set("resume", *r.resume);
  return doc;
}

std::string stop_before_work_canonical(const stop_before_work_record& record) {
  return canonical_json(stop_before_work_json(record));
}

}  // namespace astro
