#include <fcntl.h>
#include <sys/stat.h>

#include <algorithm>
#include <set>

#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;

namespace {

bool valid_role(const std::string& role) {
  if (role.empty() || role.size() > 32) return false;
  return std::all_of(role.begin(), role.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

stored_file::identity file_identity(const struct stat& st) {
  return stored_file::identity{static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino),
                               static_cast<std::uint64_t>(st.st_size),
                               static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec,
                               static_cast<std::int64_t>(st.st_ctim.tv_sec) * 1000000000 + st.st_ctim.tv_nsec};
}

}  // namespace

void sqlite_dag_store::check_identity(const stored_file& f) const {
  const auto changed = [&](const char* why) {
    return stop_error(error_category::integrity, "store.file_identity_changed", why, {{"rel_path", f.rel_path()}});
  };
  if (f.store_id_ != store_id_) throw changed("il file e' stato prodotto da un altro store");
  const unique_fd dir = open_dir_nofollow(root_fd_, "artifacts", "store.symlink");
  struct stat st {};
  if (::fstatat(dir.get(), f.rel_path().c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
    if (errno == ENOENT) throw changed("il file scritto non esiste piu'");
    throw_errno(error_category::io, "store.fstat", "fstatat fallita", errno, {{"rel_path", f.rel_path()}});
  }
  if (!S_ISREG(st.st_mode) || !(file_identity(st) == f.id_)) throw changed("il file scritto e' stato sostituito o modificato");
}

stored_file sqlite_dag_store::require_written_locked(const artifact_record& a) const {
  std::optional<stop_error> stale_token;
  for (const auto& [token, file] : written_) {
    if (file.rel_path() != a.rel_path || file.digest().sha256 != a.sha256 || file.digest().size_bytes != a.size_bytes ||
        file.role() != a.output_role) {
      continue;
    }
    try {
      check_identity(file);
      return file;
    } catch (const stop_error& e) {
      if (e.category() != error_category::integrity) throw;
      stale_token = e;  // un altro token dello stesso nome puo' essere ancora valido
    }
  }
  if (stale_token) throw *stale_token;
  throw stop_error(error_category::invalid_input, "store.file_not_written_by_store",
                   "il file non e' stato prodotto da write_files di questo store", {{"rel_path", a.rel_path}});
}

std::vector<stored_file> sqlite_dag_store::write_files(const std::vector<artifact_content>& files) {
  require_writable();
  std::lock_guard lock(mutex_);
  std::set<std::string> roles;
  for (const auto& f : files) {
    if (!valid_extension(f.extension)) {
      throw stop_error(error_category::invalid_input, "store.bad_extension", "estensione non valida",
                       {{"estensione", f.extension}});
    }
    if (!valid_role(f.role) || !roles.insert(f.role).second) {
      throw stop_error(error_category::invalid_input, "store.bad_role", "ruolo non valido o ripetuto", {{"ruolo", f.role}});
    }
  }
  const unique_fd dir = open_dir_nofollow(root_fd_, "artifacts", "store.symlink");
  std::vector<stored_file> out;
  for (const auto& f : files) {
    atomic_options options;
    options.dirfd = dir.get();
    options.fault_prefix = "artifact";
    options.conflict_code = "store.digest_name_conflict";
    options.extension = f.extension;
    options.injector = &fault_;
    const atomic_result r = atomic_write(options, f.bytes, f.expected);
    const std::uint64_t token = next_token_++;
    out.push_back(stored_file(r.name, r.digest, f.kind, f.role, store_id_, file_identity(r.final_stat), token));
    written_.emplace(token, out.back());
  }
  return out;
}

std::vector<artifact_record> sqlite_dag_store::commit_job(std::uint64_t job_id, const std::vector<stored_file>& files) {
  require_writable();
  std::lock_guard lock(mutex_);
  return commit_locked(job_id, files, nullptr);
}

std::vector<artifact_record> sqlite_dag_store::commit_locked(std::uint64_t job_id, const std::vector<stored_file>& files,
                                                             const std::string* expected_node) {
  if (files.empty()) throw stop_error(error_category::invalid_input, "store.no_outputs", "commit senza file");
  std::set<std::string> committed_roles;
  for (const auto& f : files) {
    if (written_.count(f.token_) == 0) {
      throw stop_error(error_category::invalid_input, "store.file_not_written_by_store",
                       "token gia' consumato o non prodotto da questo store", {{"rel_path", f.rel_path()}});
    }
    check_identity(f);
    if (!committed_roles.insert(f.role()).second) {
      throw stop_error(error_category::invalid_input, "dag.duplicate_output_role", "ruolo di output ripetuto nel commit",
                       {{"ruolo", f.role()}});
    }
  }
  check_fault("commit.begin");
  transaction tx(db_);
  statement js(db_, "SELECT node_key, generation, state, slot_name FROM jobs WHERE id=?1");
  if (!js.bind_u64(1, job_id).step()) {
    throw stop_error(error_category::invalid_input, "dag.unknown_job", "job inesistente", {{"job", std::to_string(job_id)}});
  }
  if (js.text(2) != "running") {
    throw stop_error(error_category::invalid_input, "dag.job_not_running", "il job non e' in esecuzione",
                     {{"job", std::to_string(job_id)}});
  }
  const std::string node_key = js.text(0);
  const std::uint64_t generation = js.u64(1);
  const std::string slot_name = js.text(3);
  if (expected_node != nullptr && *expected_node != node_key) {
    throw stop_error(error_category::invalid_input, "dag.artifact_node_mismatch", "l'artefatto non appartiene al nodo del job",
                     {{"job_node", node_key}, {"artefatto_node", *expected_node}});
  }
  const slot_record slot = require_slot_locked(slot_name);
  statement role_q(db_, "SELECT publish_role FROM slots WHERE slot_name=?1");
  role_q.bind(1, slot_name).step();
  const std::string publish_role = role_q.text(0);
  std::vector<artifact_record> records;
  statement ins(db_, "INSERT INTO artifacts(node_key, generation, rel_path, size_bytes, sha256, kind, present, output_role) "
                     "VALUES(?1,?2,?3,?4,?5,?6,1,?7)");
  const artifact_record* published = nullptr;
  const bool current = !slot.detached && slot.current_generation == generation;
  for (const auto& f : files) {
    if (!current && f.role() == provenance_output_role) continue;
    ins.bind(1, node_key).bind_u64(2, generation).bind(3, f.rel_path()).bind_u64(4, f.digest().size_bytes);
    ins.bind(5, f.digest().sha256).bind(6, f.kind()).bind(7, f.role()).run();
    artifact_record a;
    a.id = static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_));
    a.node_key = node_key;
    a.generation = generation;
    a.sha256 = f.digest().sha256;
    a.size_bytes = f.digest().size_bytes;
    a.kind = f.kind();
    a.rel_path = f.rel_path();
    a.output_role = f.role();
    records.push_back(std::move(a));
    check_fault("commit.row");
  }
  for (const auto& a : records) {
    if (a.output_role == publish_role) published = &a;
  }
  if (published == nullptr) {
    throw stop_error(error_category::invalid_input, "store.role_not_produced", "nessun output ha il ruolo pubblicato dallo slot",
                     {{"slot", slot_name}, {"ruolo", publish_role}});
  }
  check_fault("commit.inserted");
  if (current) {
    statement pub(db_, "UPDATE slots SET artifact_id=?2 WHERE slot_name=?1");
    pub.bind(1, slot_name).bind_u64(2, published->id).run();
    statement fin(db_, "UPDATE jobs SET state='completed', stop_category=NULL, stop_code='' WHERE id=?1");
    fin.bind_u64(1, job_id).run();
  } else {
    statement fin(db_, "UPDATE jobs SET state='failed', stop_category='stale_generation', "
                       "stop_code='dag.stale_publish' WHERE id=?1");
    fin.bind_u64(1, job_id).run();
  }
  check_fault("commit.before_commit");
  tx.commit();
  for (const auto& f : files) written_.erase(f.token_);
  check_fault("commit.after_commit");
  if (!current) {
    throw stop_error(error_category::stale_generation, "dag.stale_publish",
                     "la generazione dello slot e' cambiata durante il job",
                     {{"slot", slot_name},
                      {"generazione_job", std::to_string(generation)},
                      {"generazione_slot", std::to_string(slot.current_generation)}});
  }
  return records;
}

}  // namespace astro
