#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <initializer_list>
#include <set>

#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

namespace sqlint {

void check_managed_file(const struct stat& st, const inode_set* originals, std::string_view name) {
  if (originals != nullptr &&
      originals->count({static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino)}) != 0) {
    throw stop_error(error_category::security, "fs.original_inode", "il file e' lo stesso inode di un originale registrato",
                     {{"file", std::string(name)}, {"inode", std::to_string(st.st_ino)}});
  }
  if (st.st_nlink != 1) {
    throw stop_error(error_category::security, "fs.hardlink", "file gestito dal progetto con piu' di un collegamento",
                     {{"file", std::string(name)}, {"nlink", std::to_string(st.st_nlink)}});
  }
}

inode_set original_inodes(sqlite3* db) {
  statement st(db, "SELECT dev, ino FROM inputs");
  inode_set out;
  while (st.step()) out.insert({st.u64(0), st.u64(1)});
  return out;
}

std::map<std::string, artifact_entry> scan_artifacts(int dirfd) {
  std::map<std::string, artifact_entry> entries;
  for (const std::string& name : read_directory(dirfd, "store.opendir")) {
    struct stat st {};
    if (::fstatat(dirfd, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
      throw_errno(error_category::io, "store.stat", "fstatat fallita", errno, {{"file", name}});
    }
    if (S_ISLNK(st.st_mode)) {
      throw stop_error(error_category::security, "store.symlink", "symlink in artifacts/: recovery fermata", {{"file", name}});
    }
    const bool temporary = name.rfind(".tmp-", 0) == 0;
    if (!S_ISREG(st.st_mode) || (!temporary && !valid_artifact_name(name))) {
      throw stop_error(error_category::security, "store.unexpected_entry", "voce inattesa in artifacts/", {{"file", name}});
    }
    check_managed_file(st, nullptr, name);
    entries[name] = artifact_entry{temporary, static_cast<std::uint64_t>(st.st_size), static_cast<std::uint64_t>(st.st_dev),
                                   static_cast<std::uint64_t>(st.st_ino)};
  }
  return entries;
}

}  // namespace sqlint

using namespace sqlint;

namespace {

constexpr const char* recovery_format = "astro.recovery.v1";

[[noreturn]] void bad_report(const std::string& why) {
  throw stop_error(error_category::invalid_input, "store.recovery_report_malformed", "report di recovery non interpretabile",
                   {{"motivo", why}});
}

json u64_array(const std::vector<std::uint64_t>& values) {
  json a = json::array();
  for (const std::uint64_t v : values) a.push(json::uinteger(v));
  return a;
}

json text_array(const std::vector<std::string>& values) {
  json a = json::array();
  for (const std::string& v : values) a.push(json::string(v));
  return a;
}

const char* cause_text(phase_defect_cause c) { return c == phase_defect_cause::absent ? "absent" : "digest"; }

json phase_json(const phase_record& p) {
  json defects = json::array();
  for (const phase_defect& d : p.defects) {
    json o = json::object();
    o.set("artifact_id", json::uinteger(d.artifact_id));
    o.set("rel_path", json::string(d.rel_path));
    o.set("cause", json::string(cause_text(d.cause)));
    o.set("expected_sha256", json::string(d.expected_sha256));
    o.set("found_sha256", json::string(d.found_sha256));
    defects.push(std::move(o));
  }
  json o = json::object();
  o.set("slot_name", json::string(p.slot_name));
  o.set("node_key", json::string(p.node_key));
  o.set("generation", json::uinteger(p.generation));
  o.set("job_id", p.job_id ? json::uinteger(*p.job_id) : json::null());
  o.set("artifact_ids", u64_array(p.artifact_ids));
  o.set("defects", std::move(defects));
  return o;
}

const json& member(const json& o, const char* key, json::kind kind) {
  const json* f = o.find(key);
  if (f == nullptr || f->type() != kind) bad_report(std::string("campo assente o di tipo errato: ") + key);
  return *f;
}

void expect_object(const json& o, std::initializer_list<const char*> keys) {
  if (o.type() != json::kind::object || o.keys().size() != keys.size()) bad_report("oggetto con campi inattesi");
  for (const char* k : keys) {
    if (o.find(k) == nullptr) bad_report(std::string("campo assente: ") + k);
  }
}

std::uint64_t u64_of(const json& j) {
  if (j.type() != json::kind::integer || j.as_integer() < 0) bad_report("intero non negativo atteso");
  return static_cast<std::uint64_t>(j.as_integer());
}

std::vector<std::uint64_t> u64_list(const json& o, const char* key) {
  std::vector<std::uint64_t> out;
  for (const json& j : member(o, key, json::kind::array).items()) out.push_back(u64_of(j));
  return out;
}

std::vector<std::string> text_list(const json& o, const char* key) {
  std::vector<std::string> out;
  for (const json& j : member(o, key, json::kind::array).items()) {
    if (j.type() != json::kind::string) bad_report(std::string("stringa attesa in ") + key);
    out.push_back(j.as_string());
  }
  return out;
}

phase_record parse_phase(const json& o) {
  expect_object(o, {"slot_name", "node_key", "generation", "job_id", "artifact_ids", "defects"});
  phase_record p;
  p.slot_name = member(o, "slot_name", json::kind::string).as_string();
  p.node_key = member(o, "node_key", json::kind::string).as_string();
  p.generation = u64_of(*o.find("generation"));
  if (o.find("job_id")->type() != json::kind::null) p.job_id = u64_of(*o.find("job_id"));
  p.artifact_ids = u64_list(o, "artifact_ids");
  for (const json& d : member(o, "defects", json::kind::array).items()) {
    expect_object(d, {"artifact_id", "rel_path", "cause", "expected_sha256", "found_sha256"});
    const std::string& cause = member(d, "cause", json::kind::string).as_string();
    if (cause != "absent" && cause != "digest") bad_report("causa ignota");
    p.defects.push_back(phase_defect{u64_of(*d.find("artifact_id")), member(d, "rel_path", json::kind::string).as_string(),
                                     cause == "absent" ? phase_defect_cause::absent : phase_defect_cause::digest,
                                     member(d, "expected_sha256", json::kind::string).as_string(),
                                     member(d, "found_sha256", json::kind::string).as_string()});
  }
  return p;
}

}  // namespace

std::string recovery_report_canonical(const recovery_report& r) {
  json incomplete = json::array();
  for (const incomplete_result& i : r.incomplete_results) {
    json o = json::object();
    o.set("job_id", json::uinteger(i.job_id));
    o.set("slot_name", json::string(i.slot_name));
    o.set("node_key", json::string(i.node_key));
    o.set("generation", json::uinteger(i.generation));
    incomplete.push(std::move(o));
  }
  json safe = json::array();
  for (const phase_record& p : r.safe_phases) safe.push(phase_json(p));
  json damaged = json::array();
  for (const phase_record& p : r.damaged_phases) damaged.push(phase_json(p));
  json doc = json::object();
  doc.set("format", json::string(recovery_format));
  doc.set("interrupted_jobs", u64_array(r.interrupted_jobs));
  doc.set("removed_temporaries", text_array(r.removed_temporaries));
  doc.set("removed_orphans", text_array(r.removed_orphans));
  doc.set("artifacts_marked_absent", u64_array(r.artifacts_marked_absent));
  doc.set("incomplete_results", std::move(incomplete));
  doc.set("safe_phases", std::move(safe));
  doc.set("damaged_phases", std::move(damaged));
  return canonical_json(doc);
}

recovery_report parse_recovery_report(std::string_view text) {
  json doc;
  try {
    doc = parse_json(text);
  } catch (const stop_error& e) {
    bad_report("JSON non valido: " + e.code());
  }
  expect_object(doc, {"format", "interrupted_jobs", "removed_temporaries", "removed_orphans", "artifacts_marked_absent",
                      "incomplete_results", "safe_phases", "damaged_phases"});
  if (member(doc, "format", json::kind::string).as_string() != recovery_format) bad_report("formato sconosciuto");
  recovery_report r;
  r.interrupted_jobs = u64_list(doc, "interrupted_jobs");
  r.removed_temporaries = text_list(doc, "removed_temporaries");
  r.removed_orphans = text_list(doc, "removed_orphans");
  r.artifacts_marked_absent = u64_list(doc, "artifacts_marked_absent");
  for (const json& i : member(doc, "incomplete_results", json::kind::array).items()) {
    expect_object(i, {"job_id", "slot_name", "node_key", "generation"});
    r.incomplete_results.push_back(incomplete_result{u64_of(*i.find("job_id")), member(i, "slot_name", json::kind::string).as_string(),
                                                     member(i, "node_key", json::kind::string).as_string(),
                                                     u64_of(*i.find("generation"))});
  }
  for (const json& p : member(doc, "safe_phases", json::kind::array).items()) r.safe_phases.push_back(parse_phase(p));
  for (const json& p : member(doc, "damaged_phases", json::kind::array).items()) r.damaged_phases.push_back(parse_phase(p));
  if (canonical_json(doc) != text) bad_report("testo non canonico");
  return r;
}

void sqlite_dag_store::set_resume_context(std::optional<json> context) {
  std::lock_guard lock(mutex_);
  resume_context_ = std::move(context);
}

std::optional<json> sqlite_dag_store::resume_context() const {
  std::lock_guard lock(mutex_);
  return resume_context_;
}

std::vector<recovery_report> sqlite_dag_store::recovery_reports() const {
  std::lock_guard lock(mutex_);
  statement st(db_, "SELECT report_canonical FROM recovery_reports ORDER BY id");
  std::vector<recovery_report> out;
  while (st.step()) out.push_back(parse_recovery_report(st.text(0)));
  return out;
}

std::optional<phase_record> sqlite_dag_store::check_phase(const std::string& slot_name) const {
  std::lock_guard lock(mutex_);
  const unique_fd dir = open_dir_nofollow(root_fd_, "artifacts", "store.symlink");
  return check_phase_locked(slot_name, dir.get());
}

std::optional<phase_record> sqlite_dag_store::check_phase_locked(const std::string& slot_name, int artifacts_fd) const {
  phase_record phase;
  std::uint64_t published = 0;
  {
    statement s(db_,
                "SELECT a.id, a.node_key, a.generation, a.job_id FROM slots s JOIN artifacts a ON a.id = s.artifact_id "
                "WHERE s.slot_name = ?1 AND s.detached = 0 AND a.node_key = s.target_node_key");
    s.bind(1, slot_name);
    if (!s.step()) return std::nullopt;
    published = s.u64(0);
    phase.slot_name = slot_name;
    phase.node_key = s.text(1);
    phase.generation = s.u64(2);
    phase.job_id = s.opt_u64(3);
  }
  const inode_set originals = original_inodes(db_);
  statement out(db_, phase.job_id ? "SELECT id, rel_path, size_bytes, sha256, present FROM artifacts WHERE job_id = ?1 ORDER BY id"
                                  : "SELECT id, rel_path, size_bytes, sha256, present FROM artifacts WHERE id = ?1");
  out.bind_u64(1, phase.job_id ? *phase.job_id : published);
  while (out.step()) {
    const std::uint64_t id = out.u64(0);
    const std::string rel = out.text(1);
    const std::uint64_t size = out.u64(2);
    const std::string sha = out.text(3);
    const bool present = out.integer(4) == 1;
    unique_fd fd(::openat(artifacts_fd, rel.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
    if (!fd) {
      const int err = errno;
      if (err == ENOENT) {
        if (present) {
          throw stop_error(error_category::io, "store.artifact_missing", "file di un artefatto present assente",
                           {{"rel_path", rel}, {"slot", slot_name}});
        }
        phase.defects.push_back(phase_defect{id, rel, phase_defect_cause::absent, sha, ""});
        continue;
      }
      if (err == ELOOP) {
        throw stop_error(error_category::security, "store.symlink", "symlink in artifacts/", {{"rel_path", rel}});
      }
      throw_errno(error_category::io, "store.open", "apertura dell'artefatto fallita", err, {{"rel_path", rel}});
    }
    struct stat st {};
    if (::fstat(fd.get(), &st) != 0) throw_errno(error_category::io, "store.fstat", "fstat fallita", errno, {{"rel_path", rel}});
    if (!S_ISREG(st.st_mode)) {
      throw stop_error(error_category::security, "store.unexpected_entry", "voce inattesa in artifacts/", {{"rel_path", rel}});
    }
    check_managed_file(st, &originals, rel);
    const content_digest found = stream_digest(fd.get(), static_cast<std::uint64_t>(st.st_size), rel);
    const bool same = found.sha256 == sha && found.size_bytes == size;
    if (same && present) {
      phase.artifact_ids.push_back(id);
    } else if (same) {
      // Riga present = 0 con il contenuto registrato: il nome si riusa alla ricostruzione.
      phase.defects.push_back(phase_defect{id, rel, phase_defect_cause::absent, sha, found.sha256});
    } else {
      phase.defects.push_back(phase_defect{id, rel, phase_defect_cause::digest, sha, found.sha256});
    }
  }
  return phase;
}

void sqlite_dag_store::check_database_links() const {
  struct stat st {};
  if (::fstat(lock_fd_, &st) != 0) throw_errno(error_category::io, "store.fstat", "fstat di project.lock fallita", errno);
  check_managed_file(st, nullptr, "project.lock");
  for (const char* name : {"project.db", "project.db-wal", "project.db-shm"}) {
    if (::fstatat(root_fd_, name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
      if (errno == ENOENT) continue;
      throw_errno(error_category::io, "store.stat", "stat del database fallita", errno, {{"file", name}});
    }
    if (S_ISREG(st.st_mode)) check_managed_file(st, nullptr, name);
  }
}

void sqlite_dag_store::recover() {
  const unique_fd dir = open_dir_nofollow(root_fd_, "artifacts", "store.symlink");
  const std::map<std::string, artifact_entry> entries = scan_artifacts(dir.get());
  const inode_set originals = original_inodes(db_);
  for (const auto& [name, e] : entries) {
    if (originals.count({e.dev, e.ino}) != 0) {
      throw stop_error(error_category::security, "fs.original_inode", "un originale registrato e' in artifacts/",
                       {{"file", name}});
    }
  }

  // Analisi in sola lettura (ADR-04, Report di recovery, passo 1).
  std::vector<std::uint64_t> running;
  {
    statement st(db_, "SELECT id FROM jobs WHERE state IN ('running','queued') ORDER BY id");
    while (st.step()) running.push_back(st.u64(0));
  }
  std::set<std::string> referenced;
  std::vector<std::uint64_t> to_mark;
  {
    statement rows(db_, "SELECT id, rel_path, size_bytes, present FROM artifacts ORDER BY id");
    while (rows.step()) {
      const std::string rel = rows.text(1);
      referenced.insert(rel);
      const auto it = entries.find(rel);
      const bool intact = it != entries.end() && !it->second.temporary && it->second.size == rows.u64(2);
      if (!intact && rows.integer(3) == 1) to_mark.push_back(rows.u64(0));
    }
  }

  // Rimozioni (passo 2): il report elenca solo quelle riuscite.
  std::vector<std::string> removed_temporaries;
  std::vector<std::string> removed_orphans;
  for (const auto& [name, info] : entries) {
    if (!info.temporary && referenced.count(name) != 0) continue;
    if (::unlinkat(dir.get(), name.c_str(), 0) != 0) {
      throw_errno(error_category::io, "store.unlink", "rimozione dell'orfano fallita", errno, {{"file", name}});
    }
    (info.temporary ? removed_temporaries : removed_orphans).push_back(name);
  }
  if (!removed_temporaries.empty() || !removed_orphans.empty()) fsync_fd(dir.get(), "store.fsync", "artifacts");

  // Transazione (passo 3): marcature, analisi delle fasi, report.
  transaction tx(db_);
  exec(db_, "UPDATE jobs SET state='interrupted' WHERE state IN ('running','queued')");
  {
    statement mark(db_, "UPDATE artifacts SET present=0 WHERE id=?1");
    for (const std::uint64_t id : to_mark) {
      mark.bind_u64(1, id);
      mark.run();
    }
  }
  recovery_report report;
  report.interrupted_jobs = std::move(running);
  report.removed_temporaries = std::move(removed_temporaries);
  report.removed_orphans = std::move(removed_orphans);
  report.artifacts_marked_absent = std::move(to_mark);
  {
    statement jobs(db_, "SELECT id, slot_name, node_key, generation FROM jobs WHERE state='interrupted' ORDER BY id");
    while (jobs.step()) {
      const incomplete_result r{jobs.u64(0), jobs.text(1), jobs.text(2), jobs.u64(3)};
      statement current(db_,
                        "SELECT a.node_key FROM slots s JOIN artifacts a ON a.id = s.artifact_id WHERE s.slot_name = ?1 "
                        "AND s.detached = 0 AND a.present = 1 AND a.node_key = s.target_node_key");
      current.bind(1, r.slot_name);
      const bool published = current.step() && current.text(0) == r.node_key;
      if (!published) report.incomplete_results.push_back(r);
    }
  }
  std::vector<std::string> slots;
  {
    statement st(db_, "SELECT slot_name FROM slots WHERE detached = 0 ORDER BY slot_name");
    while (st.step()) slots.push_back(st.text(0));
  }
  for (const std::string& slot : slots) {
    std::optional<phase_record> phase = check_phase_locked(slot, dir.get());
    if (!phase) continue;
    (phase->defects.empty() ? report.safe_phases : report.damaged_phases).push_back(std::move(*phase));
  }
  const bool notable = !report.interrupted_jobs.empty() || !report.removed_temporaries.empty() ||
                       !report.removed_orphans.empty() || !report.artifacts_marked_absent.empty() ||
                       !report.incomplete_results.empty() || !report.damaged_phases.empty();
  if (notable) {
    const std::string canonical = recovery_report_canonical(report);
    bool same_as_last = false;
    {
      statement last(db_, "SELECT report_canonical FROM recovery_reports ORDER BY id DESC LIMIT 1");
      same_as_last = last.step() && last.text(0) == canonical;
    }
    if (!same_as_last) {
      statement ins(db_, "INSERT INTO recovery_reports(report_canonical) VALUES(?1)");
      ins.bind(1, canonical).run();
    }
  }
  tx.commit();
  recovery_ = std::move(report);
}

}  // namespace astro
