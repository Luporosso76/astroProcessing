// F-IO-08 (docs/contracts/F-IO-08-ricollegamento.md): stato degli originali, slot bloccati, ricollegamento.
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>

#include "astro/input_format.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;
namespace fs = std::filesystem;

namespace {

constexpr const char* status_format = "astro.input_status.v1";

// Posizione e identità registrate di un originale.
struct input_row {
  std::uint64_t id = 0;
  std::string path;
  std::uint64_t size_bytes = 0;
  std::string sha256;
  std::uint64_t dev = 0;
  std::uint64_t ino = 0;
  std::string format;
  std::string role;
};

std::vector<input_row> load_rows(sqlite3* db, const std::optional<std::uint64_t>& id) {
  statement st(db, id ? "SELECT id, path, size_bytes, sha256, dev, ino, format, role FROM inputs WHERE id=?1 ORDER BY id"
                      : "SELECT id, path, size_bytes, sha256, dev, ino, format, role FROM inputs ORDER BY id");
  if (id) st.bind_u64(1, *id);
  std::vector<input_row> rows;
  while (st.step()) {
    rows.push_back(input_row{st.u64(0), st.text(1), st.u64(2), st.text(3), st.u64(4), st.u64(5), st.text(6), st.text(7)});
  }
  return rows;
}

// RL-02: un solo stat sul percorso registrato; è l'unica classificazione dello stato (usata anche da relink_input).
input_state observe(const input_row& r) {
  input_state s;
  s.input_id = r.id;
  s.path = r.path;
  s.sha256 = r.sha256;
  s.size_bytes = r.size_bytes;
  s.dev = r.dev;
  s.ino = r.ino;
  struct stat st {};
  if (::stat(r.path.c_str(), &st) != 0) {
    const int e = errno;
    if (e == ENOENT || e == ENOTDIR) {
      s.status = "missing";
      s.reason = "input.missing";
    } else {
      s.status = "inaccessible";
      s.reason = "input.stat_failed";
      s.os_errno = e;
      s.os_message = std::strerror(e);
    }
    return s;
  }
  s.found_dev = static_cast<std::uint64_t>(st.st_dev);
  s.found_ino = static_cast<std::uint64_t>(st.st_ino);
  if (!S_ISREG(st.st_mode)) {
    s.status = "changed";
    s.reason = "input.not_regular";
  } else if (*s.found_dev != r.dev || *s.found_ino != r.ino) {
    s.status = "changed";
    s.reason = "input.modified";
  } else if (static_cast<std::uint64_t>(st.st_size) != r.size_bytes) {
    s.status = "changed";
    s.reason = "input.size_changed";
  } else {
    s.status = "present";
  }
  return s;
}

json u64_array(const std::vector<std::uint64_t>& values) {
  json a = json::array();
  for (const std::uint64_t v : values) a.push(json::uinteger(v));
  return a;
}

json decimal(std::uint64_t value) { return json::string(std::to_string(value)); }

json opt_decimal(const std::optional<std::uint64_t>& value) { return value ? decimal(*value) : json::null(); }

// Directory di `parent_fd` senza seguire symlink; assente: descrittore non valido.
unique_fd open_optional_dir(int parent_fd, const char* name) {
  unique_fd fd(::openat(parent_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  if (fd || errno == ENOENT) return fd;
  throw_errno(error_category::io, "store.open", "apertura della directory fallita", errno, {{"nome", name}});
}

void add_identity(std::set<std::pair<std::uint64_t, std::uint64_t>>& out, int dirfd, const std::string& name) {
  struct stat st {};
  if (::fstatat(dirfd, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
    if (errno == ENOENT) return;
    throw_errno(error_category::io, "store.stat", "stat di un file gestito fallita", errno, {{"file", name}});
  }
  out.insert({static_cast<std::uint64_t>(st.st_dev), static_cast<std::uint64_t>(st.st_ino)});
}

bool is_inside(const fs::path& path, const fs::path& base) {
  const fs::path rel = path.lexically_relative(base);
  return !rel.empty() && *rel.begin() != "..";
}

}  // namespace

std::string input_status_canonical(const std::vector<input_state>& states, const std::vector<slot_block>& blocks) {
  json inputs = json::array();
  for (const input_state& s : states) {
    json o = json::object();
    o.set("input_id", json::uinteger(s.input_id));
    o.set("path", json::string(s.path));
    o.set("sha256", json::string(s.sha256));
    o.set("size_bytes", json::uinteger(s.size_bytes));
    o.set("dev", decimal(s.dev));
    o.set("ino", decimal(s.ino));
    o.set("status", json::string(s.status));
    o.set("reason", json::string(s.reason));
    o.set("os_errno", s.os_errno ? json::integer(*s.os_errno) : json::null());
    o.set("os_message", json::string(s.os_message));
    o.set("found_dev", opt_decimal(s.found_dev));
    o.set("found_ino", opt_decimal(s.found_ino));
    inputs.push(std::move(o));
  }
  json slots = json::array();
  for (const slot_block& b : blocks) {
    json edges = json::array();
    for (const blocked_edge& e : b.edges) {
      json blockers = json::array();
      for (const blocked_input& x : e.blockers) {
        json o = json::object();
        o.set("input_id", json::uinteger(x.input_id));
        o.set("path", json::string(x.path));
        o.set("reason", json::string(x.reason));
        blockers.push(std::move(o));
      }
      json o = json::object();
      o.set("sha256", json::string(e.sha256));
      o.set("size_bytes", json::uinteger(e.size_bytes));
      o.set("available", json::boolean(e.available));
      o.set("available_ids", u64_array(e.available_ids));
      o.set("blockers", std::move(blockers));
      edges.push(std::move(o));
    }
    json o = json::object();
    o.set("slot", json::string(b.slot));
    o.set("node_key", json::string(b.node_key));
    o.set("blocked", json::boolean(b.blocked));
    o.set("edges", std::move(edges));
    slots.push(std::move(o));
  }
  json doc = json::object();
  doc.set("format", json::string(status_format));
  doc.set("inputs", std::move(inputs));
  doc.set("slots", std::move(slots));
  return canonical_json(doc);
}

std::vector<input_state> sqlite_dag_store::input_status() const {
  std::vector<input_row> rows;
  {
    std::lock_guard lock(mutex_);
    rows = load_rows(db_, std::nullopt);
  }
  std::vector<input_state> out;
  out.reserve(rows.size());
  for (const input_row& r : rows) out.push_back(observe(r));
  return out;
}

std::vector<slot_block> sqlite_dag_store::blocked_slots(const std::vector<input_state>& states) const {
  std::map<std::uint64_t, const input_state*> by_id;
  for (const input_state& s : states) by_id[s.input_id] = &s;

  std::lock_guard lock(mutex_);
  std::map<std::pair<std::string, std::uint64_t>, std::vector<std::uint64_t>> by_digest;
  for (const input_row& r : load_rows(db_, std::nullopt)) by_digest[{r.sha256, r.size_bytes}].push_back(r.id);

  std::vector<std::pair<std::string, std::string>> slots;
  {
    statement st(db_, "SELECT slot_name, target_node_key FROM slots WHERE detached=0 AND target_node_key IS NOT NULL "
                      "ORDER BY slot_name");
    while (st.step()) slots.emplace_back(st.text(0), st.text(1));
  }
  std::vector<slot_block> out;
  for (const auto& [slot, node_key] : slots) {
    slot_block block;
    block.slot = slot;
    block.node_key = node_key;
    statement edges(db_, "WITH RECURSIVE anc(k) AS (SELECT ?1 UNION SELECT e.parent_key FROM node_edges e JOIN anc "
                         "ON e.child_key = anc.k WHERE e.parent_key IS NOT NULL) "
                         "SELECT DISTINCT input_sha256, input_size FROM node_edges "
                         "WHERE child_key IN (SELECT k FROM anc) AND input_sha256 IS NOT NULL "
                         "ORDER BY input_sha256, input_size");
    edges.bind(1, node_key);
    while (edges.step()) {
      blocked_edge edge;
      edge.sha256 = edges.text(0);
      edge.size_bytes = edges.u64(1);
      const auto registered = by_digest.find({edge.sha256, edge.size_bytes});
      if (registered == by_digest.end()) {
        edge.blockers.push_back(blocked_input{0, "", "input.unregistered"});
      } else {
        for (const std::uint64_t id : registered->second) {
          const auto state = by_id.find(id);
          if (state == by_id.end()) {
            throw stop_error(error_category::invalid_input, "relink.state_missing", "input registrato senza stato",
                             {{"input_id", std::to_string(id)}});
          }
          if (state->second->status == "present") {
            edge.available_ids.push_back(id);
          } else {
            edge.blockers.push_back(blocked_input{id, state->second->path, state->second->reason});
          }
        }
      }
      edge.available = !edge.available_ids.empty();
      if (edge.available) edge.blockers.clear();
      block.blocked = block.blocked || !edge.available;
      block.edges.push_back(std::move(edge));
    }
    out.push_back(std::move(block));
  }
  return out;
}

input_record sqlite_dag_store::relink_input(std::uint64_t input_id, const std::string& path, std::span<std::byte> work,
                                            const std::function<void()>& check_cancel) {
  require_writable();
  input_row row;
  {
    std::lock_guard lock(mutex_);
    std::vector<input_row> rows = load_rows(db_, input_id);
    if (rows.empty()) {
      throw stop_error(error_category::invalid_input, "relink.unknown_input", "input non registrato",
                       {{"input_id", std::to_string(input_id)}});
    }
    row = std::move(rows.front());
  }
  if (path.empty() || path.front() != '/' || path.find('\0') != std::string::npos) {
    throw stop_error(error_category::invalid_input, "relink.path_not_absolute", "il percorso del ricollegamento non è assoluto",
                     {{"percorso", path}});
  }

  // Percorso dentro il progetto, dopo la risoluzione dei symlink del prefisso esistente.
  std::error_code ec;
  const fs::path project = fs::canonical("/proc/self/fd/" + std::to_string(root_fd_), ec);
  if (ec) throw_errno(error_category::io, "store.stat", "percorso del progetto non risolvibile", ec.value());
  const fs::path target = fs::weakly_canonical(fs::path(path), ec);
  if (ec) throw_errno(error_category::io, "relink.path_unresolvable", "percorso del candidato non risolvibile", ec.value(), {{"percorso", path}});
  if (target == project || is_inside(target, project)) {
    throw stop_error(error_category::security, "relink.target_forbidden", "il candidato sta dentro il progetto",
                     {{"percorso", path}});
  }

  // Identità del candidato prima di ogni lettura: mai quella di un file gestito dal progetto.
  struct stat cand {};
  if (::stat(path.c_str(), &cand) != 0) {
    const int e = errno;
    if (e == ENOENT || e == ENOTDIR) {
      throw stop_error(error_category::io, "input.missing", "candidato assente", {{"percorso", path}});
    }
    throw_errno(error_category::io, "input.stat_failed", "stat del candidato fallita", e, {{"percorso", path}});
  }
  const std::pair<std::uint64_t, std::uint64_t> identity{static_cast<std::uint64_t>(cand.st_dev),
                                                          static_cast<std::uint64_t>(cand.st_ino)};
  {
    std::set<std::pair<std::uint64_t, std::uint64_t>> managed;
    for (const char* name : {"project.lock", "project.db", "project.db-wal", "project.db-shm"}) add_identity(managed, root_fd_, name);
    const unique_fd artifacts = open_dir_nofollow(root_fd_, "artifacts", "store.symlink");
    for (const std::string& name : read_directory(artifacts.get(), "store.opendir")) add_identity(managed, artifacts.get(), name);
    const unique_fd cache = open_optional_dir(root_fd_, "cache");
    if (cache) {
      const unique_fd thumbnails = open_optional_dir(cache.get(), "thumbnails");
      if (thumbnails) {
        for (const std::string& name : read_directory(thumbnails.get(), "store.opendir")) add_identity(managed, thumbnails.get(), name);
      }
    }
    if (managed.count(identity) != 0) {
      throw stop_error(error_category::security, "fs.original_inode", "il candidato è un file gestito dal progetto",
                       {{"percorso", path}});
    }
  }

  // Verifica nel confine, con il formato dichiarato dell'input.
  const auto& formats = input_formats();
  const auto format = std::find_if(formats.begin(), formats.end(), [&](const input_format* f) { return row.format == f->prefix; });
  if (format == formats.end()) {
    throw stop_error(error_category::invalid_input, "store.bad_input", "formato registrato non ammesso",
                     {{"input_id", std::to_string(input_id)}, {"formato", row.format}});
  }
  const auto verified = reverify_input(path, **format, nullptr, content_digest{row.sha256, row.size_bytes}, work, check_cancel);
  if (verified != identity) {
    throw stop_error(error_category::integrity, "relink.identity_changed", "il candidato è stato sostituito dopo lo stat",
                     {{"percorso", path}});
  }

  std::lock_guard lock(mutex_);
  transaction tx(db_);
  std::vector<input_row> current = load_rows(db_, input_id);
  if (current.empty()) {
    throw stop_error(error_category::invalid_input, "relink.unknown_input", "input non registrato",
                     {{"input_id", std::to_string(input_id)}});
  }
  const input_row& old = current.front();
  const input_state previous = observe(old);
  std::int64_t revision = 1;
  {
    statement st(db_, "SELECT coalesce(max(revision), 0) + 1 FROM input_relinks WHERE input_id=?1");
    st.bind_u64(1, input_id);
    if (!st.step()) throw stop_error(error_category::integrity, "store.corrupt", "revisione della storia non calcolabile");
    revision = st.integer(0);
  }
  {
    statement ins(db_, "INSERT INTO input_relinks(input_id, revision, old_path, old_dev, old_ino, new_path, new_dev, new_ino, "
                       "previous_status, record_version) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,1)");
    ins.bind_u64(1, input_id).bind(2, revision).bind(3, old.path).bind_u64(4, old.dev).bind_u64(5, old.ino);
    ins.bind(6, path).bind_u64(7, identity.first).bind_u64(8, identity.second);
    ins.bind(9, previous.status == "present" ? std::string("present") : previous.reason).run();
  }
  check_fault("relink.inserted");
  {
    statement upd(db_, "UPDATE inputs SET path=?2, dev=?3, ino=?4 WHERE id=?1");
    upd.bind_u64(1, input_id).bind(2, path).bind_u64(3, identity.first).bind_u64(4, identity.second).run();
  }
  if (sqlite3_changes(db_) != 1) {
    throw stop_error(error_category::integrity, "store.corrupt", "aggiornamento della posizione non riuscito",
                     {{"input_id", std::to_string(input_id)}});
  }
  check_fault("relink.commit");
  check_fault("relink.before_commit");  // solo per i test di kill: nessuna azione del prodotto lo distingue da relink.commit
  tx.commit();
  check_fault("relink.after_commit");  // dopo il COMMIT: un guasto iniettato qui lascia il ricollegamento già registrato
  return input_record{input_id, path, old.size_bytes, old.sha256, old.role, identity.first, identity.second, old.format};
}

}  // namespace astro
