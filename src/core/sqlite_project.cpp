#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <map>
#include <set>

#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;
namespace fs = std::filesystem;

namespace {

constexpr const char* schema_v1[] = {
    "CREATE TABLE inputs(id INTEGER PRIMARY KEY, path TEXT NOT NULL, size_bytes INTEGER NOT NULL, "
    "sha256 TEXT NOT NULL CHECK(length(sha256)=64), role TEXT NOT NULL) STRICT",
    "CREATE TABLE nodes(node_key TEXT PRIMARY KEY, operator_id TEXT NOT NULL, algorithm_version INTEGER NOT NULL, "
    "serialization_version INTEGER NOT NULL, params_canonical TEXT NOT NULL, precision TEXT NOT NULL, "
    "backend TEXT NOT NULL, seed TEXT) STRICT",
    "CREATE TABLE node_edges(child_key TEXT NOT NULL REFERENCES nodes(node_key), ordinal INTEGER NOT NULL, "
    "input_name TEXT NOT NULL, parent_key TEXT REFERENCES nodes(node_key), input_sha256 TEXT, input_size INTEGER, "
    "PRIMARY KEY(child_key, ordinal), "
    "CHECK((parent_key IS NOT NULL) <> (input_sha256 IS NOT NULL))) STRICT",
    "CREATE TABLE artifacts(id INTEGER PRIMARY KEY, node_key TEXT NOT NULL REFERENCES nodes(node_key), "
    "generation INTEGER NOT NULL, rel_path TEXT NOT NULL, size_bytes INTEGER NOT NULL, sha256 TEXT NOT NULL, "
    "kind TEXT NOT NULL, present INTEGER NOT NULL CHECK(present IN (0,1)), output_role TEXT NOT NULL) STRICT",
    "CREATE TABLE slots(slot_name TEXT PRIMARY KEY, current_generation INTEGER NOT NULL, "
    "target_node_key TEXT REFERENCES nodes(node_key), artifact_id INTEGER REFERENCES artifacts(id), "
    "publish_role TEXT NOT NULL DEFAULT 'primary', detached INTEGER NOT NULL DEFAULT 0 CHECK(detached IN (0,1)), "
    "CHECK((detached=1) = (target_node_key IS NULL))) STRICT",
    "CREATE TABLE jobs(id INTEGER PRIMARY KEY, slot_name TEXT NOT NULL REFERENCES slots(slot_name), "
    "node_key TEXT NOT NULL REFERENCES nodes(node_key), generation INTEGER NOT NULL, "
    "state TEXT NOT NULL CHECK(state IN ('queued','running','completed','failed','cancelled','interrupted')), "
    "stop_category TEXT, stop_code TEXT NOT NULL DEFAULT '') STRICT",
    "CREATE TABLE snapshots(id INTEGER PRIMARY KEY, parent_id INTEGER REFERENCES snapshots(id)) STRICT",
    "CREATE TABLE snapshot_entries(snapshot_id INTEGER NOT NULL REFERENCES snapshots(id), slot_name TEXT NOT NULL, "
    "node_key TEXT NOT NULL REFERENCES nodes(node_key), publish_role TEXT NOT NULL, PRIMARY KEY(snapshot_id, slot_name)) STRICT",
    "CREATE TABLE history_cursor(id INTEGER PRIMARY KEY CHECK(id=1), "
    "current_snapshot_id INTEGER REFERENCES snapshots(id), redo_snapshot_id INTEGER REFERENCES snapshots(id)) STRICT",
    "CREATE TABLE settings(key TEXT PRIMARY KEY, value_canonical TEXT NOT NULL) STRICT",
    "INSERT INTO history_cursor(id, current_snapshot_id, redo_snapshot_id) VALUES(1, NULL, NULL)",
    "CREATE TRIGGER nodes_immutable_update BEFORE UPDATE ON nodes BEGIN SELECT RAISE(ABORT,'nodes e immutabile'); END",
    "CREATE TRIGGER nodes_immutable_delete BEFORE DELETE ON nodes BEGIN SELECT RAISE(ABORT,'nodes e immutabile'); END",
    "CREATE TRIGGER edges_immutable_update BEFORE UPDATE ON node_edges BEGIN SELECT RAISE(ABORT,'node_edges e immutabile'); END",
    "CREATE TRIGGER edges_immutable_delete BEFORE DELETE ON node_edges BEGIN SELECT RAISE(ABORT,'node_edges e immutabile'); END",
    "CREATE TRIGGER snapshots_immutable_update BEFORE UPDATE ON snapshots BEGIN SELECT RAISE(ABORT,'snapshots e immutabile'); END",
    "CREATE TRIGGER snapshots_immutable_delete BEFORE DELETE ON snapshots BEGIN SELECT RAISE(ABORT,'snapshots e immutabile'); END",
    "CREATE TRIGGER entries_immutable_update BEFORE UPDATE ON snapshot_entries BEGIN SELECT RAISE(ABORT,'snapshot_entries e immutabile'); END",
    "CREATE TRIGGER entries_immutable_delete BEFORE DELETE ON snapshot_entries BEGIN SELECT RAISE(ABORT,'snapshot_entries e immutabile'); END",
};

struct header_fields {
  std::uint32_t application_id = 0;
  std::uint32_t user_version = 0;
};

std::uint32_t be32(const unsigned char* p) {
  return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) | p[3];
}

[[noreturn]] void not_a_project(const char* why, const fs::path& root) {
  throw stop_error(error_category::invalid_input, "store.not_a_project", why, {{"progetto", root.string()}});
}

header_fields read_header(int root_fd, const fs::path& root) {
  const std::string db_path = (root / "project.db").string();
  unique_fd fd(::openat(root_fd, "project.db", O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
  if (!fd) {
    if (errno == ENOENT || errno == ELOOP) not_a_project("project.db assente o symlink", root);
    throw_errno(error_category::io, "store.open", "apertura di project.db fallita", errno, {{"file", db_path}});
  }
  unsigned char buf[100];
  const ssize_t n = ::pread(fd.get(), buf, sizeof(buf), 0);
  if (n < 0) throw_errno(error_category::io, "store.read", "lettura dell'header fallita", errno, {{"file", db_path}});
  if (n < 100 || std::memcmp(buf, "SQLite format 3", 16) != 0 || be32(buf + 68) != project_application_id) {
    not_a_project("project.db non e' un database di progetto", root);
  }
  return header_fields{be32(buf + 68), be32(buf + 60)};
}

std::string uri_for_readonly(const fs::path& path) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out = "file:";
  for (const unsigned char c : fs::absolute(path).string()) {
    if (std::isalnum(c) != 0 || c == '/' || c == '-' || c == '.' || c == '_' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 0x0f]);
    }
  }
  return out + "?mode=ro&immutable=1";
}

std::string pragma_text(sqlite3* db, const char* sql) {
  statement st(db, sql);
  if (!st.step()) throw stop_error(error_category::io, "store.sqlite", "pragma senza risultato", {{"pragma", sql}});
  return st.text(0);
}

void check_integrity(sqlite3* db) {
  statement st(db, "PRAGMA quick_check");
  std::vector<std::string> rows;
  while (st.step()) rows.push_back(st.text(0));
  if (rows.size() != 1 || rows[0] != "ok") {
    throw stop_error(error_category::integrity, "store.corrupt", "quick_check non riuscito",
                     {{"primo_messaggio", rows.empty() ? std::string() : rows[0]}});
  }
}

void make_dir(const fs::path& path) {
  if (::mkdir(path.c_str(), 0755) != 0) {
    if (errno == EEXIST) {
      throw stop_error(error_category::security, "store.target_exists", "il progetto esiste gia'",
                       {{"percorso", path.string()}});
    }
    throw_errno(error_category::io, "store.mkdir", "mkdir fallita", errno, {{"percorso", path.string()}});
  }
}

void lock_exclusive(int fd, const fs::path& root) {
  if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
    if (errno == EWOULDBLOCK) {
      throw stop_error(error_category::io, "store.locked", "progetto in uso da un altro processo",
                       {{"progetto", root.string()}});
    }
    throw_errno(error_category::io, "store.flock", "flock fallita", errno, {{"progetto", root.string()}});
  }
}

void make_subdir(int root_fd, const char* name) {
  if (::mkdirat(root_fd, name, 0755) != 0) {
    throw_errno(error_category::io, "store.mkdir", "mkdirat fallita", errno, {{"nome", name}});
  }
}

void fsync_dir(int dirfd) { fsync_fd(dirfd, "store.fsync", "directory"); }

void check_copy_integrity(const std::string& path) {
  sqlite3* check = nullptr;
  const int rc = sqlite3_open_v2(uri_for_readonly(path).c_str(), &check, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI | SQLITE_OPEN_NOMUTEX, nullptr);
  if (rc != SQLITE_OK) {
    const stop_error e(error_category::io, "store.sqlite", "apertura del backup fallita",
                       {{"sqlite_rc", std::to_string(rc)}, {"sqlite_msg", check != nullptr ? sqlite3_errmsg(check) : ""}});
    sqlite3_close(check);
    throw e;
  }
  try {
    check_integrity(check);
  } catch (...) {
    sqlite3_close(check);
    throw;
  }
  sqlite3_close(check);
}

}  // namespace

sqlite_dag_store::sqlite_dag_store(fs::path root, open_mode mode)
    : root_(std::move(root)), mode_(mode), store_id_(random_name(16)) {}

sqlite_dag_store::~sqlite_dag_store() {
  if (db_ != nullptr) sqlite3_close(db_);
  if (lock_fd_ >= 0) ::close(lock_fd_);
  if (root_fd_ >= 0) ::close(root_fd_);
}

void sqlite_dag_store::set_fault_injector(fault_injector injector) {
  std::lock_guard lock(mutex_);
  fault_ = std::move(injector);
}

void sqlite_dag_store::check_fault(std::string_view point) const { inject(&fault_, point); }

void sqlite_dag_store::require_writable() const {
  if (mode_ != open_mode::read_write) {
    throw stop_error(error_category::schema, "store.read_only", "progetto aperto in sola lettura");
  }
}

bool sqlite_dag_store::wal_pending() const {
  struct stat st {};
  if (::fstatat(root_fd_, "project.db-wal", &st, AT_SYMLINK_NOFOLLOW) != 0) {
    if (errno == ENOENT) return false;
    throw_errno(error_category::io, "store.stat", "stat del WAL fallita", errno, {{"progetto", root_.string()}});
  }
  return st.st_size != 0;
}

fs::path sqlite_dag_store::artifact_path(const std::string& rel_path) const { return root_ / "artifacts" / rel_path; }

std::unique_ptr<sqlite_dag_store> sqlite_dag_store::create(const fs::path& root) {
  make_dir(root);
  std::unique_ptr<sqlite_dag_store> s(new sqlite_dag_store(root, open_mode::read_write));
  s->root_fd_ = ::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (s->root_fd_ < 0) throw_errno(error_category::io, "store.open", "apertura del progetto fallita", errno);
  make_subdir(s->root_fd_, "artifacts");
  make_subdir(s->root_fd_, "backups");
  s->lock_fd_ = ::openat(s->root_fd_, "project.lock", O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (s->lock_fd_ < 0) throw_errno(error_category::io, "store.open", "creazione di project.lock fallita", errno);
  lock_exclusive(s->lock_fd_, root);
  const std::string db_path = (root / "project.db").string();
  const int rc = sqlite3_open_v2(db_path.c_str(), &s->db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);
  if (rc != SQLITE_OK) throw_sql(s->db_, rc, "creazione di project.db fallita");
  if (pragma_text(s->db_, "PRAGMA journal_mode = WAL") != "wal") {
    throw stop_error(error_category::integrity, "store.journal_mode", "impossibile attivare il WAL");
  }
  {
    transaction tx(s->db_);
    for (const char* sql : schema_v1) exec(s->db_, sql);
    exec(s->db_, ("PRAGMA application_id = " + std::to_string(project_application_id)).c_str());
    exec(s->db_, ("PRAGMA user_version = " + std::to_string(project_schema_version)).c_str());
    tx.commit();
  }
  exec(s->db_, "PRAGMA synchronous = FULL");
  exec(s->db_, "PRAGMA foreign_keys = ON");
  sqlite3_busy_timeout(s->db_, 0);
  statement ck(s->db_, "PRAGMA wal_checkpoint(TRUNCATE)");
  if (!ck.step() || ck.integer(0) != 0) {
    throw stop_error(error_category::io, "store.checkpoint", "wal_checkpoint(TRUNCATE) non completato");
  }
  fsync_dir(s->root_fd_);
  return s;
}

std::unique_ptr<sqlite_dag_store> sqlite_dag_store::open(const fs::path& root, open_intent intent,
                                                         const open_options& options) {
  std::unique_ptr<sqlite_dag_store> s(new sqlite_dag_store(root, open_mode::read_write));
  s->fault_ = options.fault;
  s->root_fd_ = ::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (s->root_fd_ < 0) {
    if (errno == ENOENT || errno == ENOTDIR) not_a_project("directory del progetto assente", root);
    throw_errno(error_category::io, "store.open", "apertura del progetto fallita", errno, {{"progetto", root.string()}});
  }
  s->lock_fd_ = ::openat(s->root_fd_, "project.lock", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (s->lock_fd_ < 0) {
    if (errno == ENOENT || errno == ENOTDIR || errno == ELOOP) not_a_project("project.lock assente", root);
    throw_errno(error_category::io, "store.open", "apertura di project.lock fallita", errno, {{"progetto", root.string()}});
  }
  lock_exclusive(s->lock_fd_, root);
  s->open_database(intent, options);
  return s;
}

void sqlite_dag_store::open_database(open_intent intent, const open_options& options) {
  const header_fields header = read_header(root_fd_, root_);
  const bool wal = wal_pending();
  auto connect_readonly = [&](open_mode mode) {
    if (wal) {
      throw stop_error(error_category::schema, "store.wal_pending_read_only", "WAL non vuoto: aprire in lettura-scrittura",
                       {{"progetto", root_.string()}});
    }
    mode_ = mode;
    const int rc = sqlite3_open_v2(uri_for_readonly(root_ / "project.db").c_str(), &db_,
                                   SQLITE_OPEN_READONLY | SQLITE_OPEN_URI | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) throw_sql(db_, rc, "apertura in sola lettura fallita");
    check_integrity(db_);
  };
  auto refuse_future_with_wal = [&] {
    throw stop_error(error_category::schema, "store.wal_pending_future_schema",
                     "schema piu' recente con WAL non vuoto: nessuna apertura che possa applicarlo",
                     {{"progetto", root_.string()}});
  };
  if (header.user_version > options.schema_version) {
    if (wal) refuse_future_with_wal();
    connect_readonly(open_mode::read_only_unknown_schema);
    return;
  }
  if (intent == open_intent::read_only) {
    connect_readonly(open_mode::read_only_requested);
    return;
  }
  const std::string db_path = (root_ / "project.db").string();
  const int rc = sqlite3_open_v2(db_path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, nullptr);
  if (rc != SQLITE_OK) throw_sql(db_, rc, "apertura in lettura-scrittura fallita");
  // Finche' la versione non e' nota, la chiusura non deve applicare il WAL al database.
  sqlite3_db_config(db_, SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE, 1, nullptr);
  exec(db_, "PRAGMA synchronous = FULL");
  exec(db_, "PRAGMA foreign_keys = ON");
  sqlite3_busy_timeout(db_, 0);
  if (pragma_text(db_, "PRAGMA journal_mode") != "wal") {
    throw stop_error(error_category::integrity, "store.journal_mode", "journal_mode diverso da wal");
  }
  check_integrity(db_);
  const auto version = static_cast<std::uint32_t>(std::stoul(pragma_text(db_, "PRAGMA user_version")));
  if (version > options.schema_version) {
    if (wal) refuse_future_with_wal();
    sqlite3_close(db_);
    db_ = nullptr;
    connect_readonly(open_mode::read_only_unknown_schema);
    return;
  }
  sqlite3_db_config(db_, SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE, 0, nullptr);
  mode_ = open_mode::read_write;
  if (version < options.schema_version) migrate(version, options);
  recover();
}

void sqlite_dag_store::migrate(std::uint32_t from_version, const open_options& options) {
  const std::string final_name = "project.v" + std::to_string(from_version) + ".db";
  const std::string partial_name = ".tmp-" + random_name(16);
  try {
    const unique_fd backups = open_dir_nofollow(root_fd_, "backups", "store.symlink");
    const unique_fd copy_fd(::openat(backups.get(), partial_name.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (!copy_fd) throw_errno(error_category::io, "store.open", "creazione esclusiva del backup fallita", errno);
    struct stat created {};
    if (::fstat(copy_fd.get(), &created) != 0) throw_errno(error_category::io, "store.fstat", "fstat del backup fallita", errno);
    const std::string partial = "/proc/self/fd/" + std::to_string(backups.get()) + "/" + partial_name;
    check_fault("backup.after_create");
    {
      // SQLITE_OPEN_NOFOLLOW rifiuta anche /proc/self/fd: l'identita' del nome si controlla prima dell'apertura.
      struct stat now {};
      if (::fstatat(backups.get(), partial_name.c_str(), &now, AT_SYMLINK_NOFOLLOW) != 0 || now.st_dev != created.st_dev ||
          now.st_ino != created.st_ino) {
        throw stop_error(error_category::integrity, "store.backup_identity", "il temporaneo del backup e' stato sostituito",
                         {{"nome", partial_name}});
      }
    }
    sqlite3* copy = nullptr;
    int rc = sqlite3_open_v2(partial.c_str(), &copy, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, nullptr);
    if (rc != SQLITE_OK) {
      const std::string msg = copy != nullptr ? sqlite3_errmsg(copy) : "";
      sqlite3_close(copy);
      throw stop_error(error_category::io, "store.sqlite", "apertura del backup fallita",
                       {{"sqlite_rc", std::to_string(rc)}, {"sqlite_msg", msg}});
    }
    sqlite3_backup* b = sqlite3_backup_init(copy, "main", db_, "main");
    if (b == nullptr) {
      const stop_error e(error_category::io, "store.sqlite", "sqlite3_backup_init fallita",
                         {{"sqlite_rc", std::to_string(sqlite3_errcode(copy))}, {"sqlite_msg", sqlite3_errmsg(copy)}});
      sqlite3_close(copy);
      throw e;
    }
    rc = sqlite3_backup_step(b, -1);
    sqlite3_backup_finish(b);
    sqlite3_close(copy);
    if (rc != SQLITE_DONE) throw_sql(nullptr, rc, "copia di backup incompleta");
    fsync_fd(copy_fd.get(), "store.fsync", partial_name);
    check_copy_integrity(partial);
    if (::fstat(copy_fd.get(), &created) != 0) throw_errno(error_category::io, "store.fstat", "fstat del backup fallita", errno);
    check_fault("backup.before_publish");
    auto same_file = [&](const std::string& name, bool with_ctime) {
      struct stat st {};
      return ::fstatat(backups.get(), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0 && st.st_dev == created.st_dev &&
             st.st_ino == created.st_ino && st.st_size == created.st_size && st.st_mtim.tv_sec == created.st_mtim.tv_sec &&
             st.st_mtim.tv_nsec == created.st_mtim.tv_nsec &&
             (!with_ctime || (st.st_ctim.tv_sec == created.st_ctim.tv_sec && st.st_ctim.tv_nsec == created.st_ctim.tv_nsec));
    };
    const auto changed = [&] {
      return stop_error(error_category::integrity, "store.backup_identity", "il backup non e' piu' il file creato", {{"nome", partial_name}});
    };
    if (!same_file(partial_name, true)) throw changed();
    if (::renameat(backups.get(), partial_name.c_str(), backups.get(), final_name.c_str()) != 0) {
      throw_errno(error_category::io, "store.rename", "rename del backup fallita", errno);
    }
    if (!same_file(final_name, false)) throw changed();  // il rename aggiorna il ctime
    fsync_dir(backups.get());
  } catch (const stop_error& e) {
    rethrow_as(e, error_category::schema, "store.backup_failed", "backup pre-migrazione fallito");
  }
  try {
    transaction tx(db_);
    std::uint32_t version = from_version;
    while (version < options.schema_version) {
      const auto it = std::find_if(options.migrations.begin(), options.migrations.end(),
                                   [&](const schema_migration& m) { return m.from_version == version; });
      if (it == options.migrations.end()) {
        throw stop_error(error_category::schema, "store.migration_missing", "nessuna migrazione dalla versione",
                         {{"versione", std::to_string(version)}});
      }
      for (const std::string& sql : it->statements) {
        exec(db_, sql.c_str());
        check_fault("migration.statement");
      }
      ++version;
    }
    exec(db_, ("PRAGMA user_version = " + std::to_string(options.schema_version)).c_str());
    tx.commit();
  } catch (const stop_error& e) {
    rethrow_as(e, error_category::schema, "store.migration_failed", "migrazione fallita, versione precedente intatta");
  }
}

void sqlite_dag_store::recover() {
  struct found {
    bool temporary = false;
    std::uint64_t size = 0;
  };
  std::map<std::string, found> entries;
  const unique_fd dir = open_dir_nofollow(root_fd_, "artifacts", "store.symlink");
  for (const std::string& name : read_directory(dir.get(), "store.opendir")) {
    struct stat st {};
    if (::fstatat(dir.get(), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
      throw_errno(error_category::io, "store.stat", "fstatat fallita", errno, {{"file", name}});
    }
    if (S_ISLNK(st.st_mode)) {
      throw stop_error(error_category::security, "store.symlink", "symlink in artifacts/: recovery fermata", {{"file", name}});
    }
    const bool temporary = name.rfind(".tmp-", 0) == 0;
    if (!S_ISREG(st.st_mode) || (!temporary && !valid_artifact_name(name))) {
      throw stop_error(error_category::security, "store.unexpected_entry", "voce inattesa in artifacts/", {{"file", name}});
    }
    entries[name] = found{temporary, static_cast<std::uint64_t>(st.st_size)};
  }

  std::set<std::string> referenced;
  {
    transaction tx(db_);
    statement running(db_, "SELECT id FROM jobs WHERE state IN ('running','queued') ORDER BY id");
    while (running.step()) recovery_.interrupted_jobs.push_back(running.u64(0));
    exec(db_, "UPDATE jobs SET state='interrupted' WHERE state IN ('running','queued')");
    statement rows(db_, "SELECT id, rel_path, size_bytes, present FROM artifacts ORDER BY id");
    statement mark(db_, "UPDATE artifacts SET present=0 WHERE id=?1");
    while (rows.step()) {
      const std::string rel = rows.text(1);
      referenced.insert(rel);
      const auto it = entries.find(rel);
      const bool intact = it != entries.end() && !it->second.temporary && it->second.size == rows.u64(2);
      if (!intact && rows.integer(3) == 1) {
        mark.bind(1, rows.integer(0));
        mark.run();
        recovery_.artifacts_marked_absent.push_back(rows.u64(0));
      }
    }
    tx.commit();
  }
  for (const auto& [name, info] : entries) {
    if (!info.temporary && referenced.count(name) != 0) continue;
    if (::unlinkat(dir.get(), name.c_str(), 0) != 0) {
      throw_errno(error_category::io, "store.unlink", "rimozione dell'orfano fallita", errno, {{"file", name}});
    }
    (info.temporary ? recovery_.removed_temporaries : recovery_.removed_orphans).push_back(name);
  }
  if (!entries.empty()) fsync_fd(dir.get(), "store.fsync", "artifacts");
}

}  // namespace astro
