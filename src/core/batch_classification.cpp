#include "astro/batch_classification.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/import_job.hpp"
#include "astro/input_format.hpp"
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

}  // namespace astro
