#pragma once

#include <sqlite3.h>
#include <sys/stat.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro::sqlint {

// Descrittore posseduto; chiusura nel distruttore.
class unique_fd {
 public:
  unique_fd() = default;
  explicit unique_fd(int fd) noexcept : fd_(fd) {}
  unique_fd(unique_fd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
  unique_fd& operator=(unique_fd&& o) noexcept;
  unique_fd(const unique_fd&) = delete;
  unique_fd& operator=(const unique_fd&) = delete;
  ~unique_fd();
  int get() const noexcept { return fd_; }
  explicit operator bool() const noexcept { return fd_ >= 0; }

 private:
  int fd_ = -1;
};

[[noreturn]] void throw_errno(error_category category, const std::string& code, const char* what, int err,
                              stop_error::context_list context = {});
[[noreturn]] void throw_sql(sqlite3* db, int rc, const char* what);

class statement {
 public:
  statement(sqlite3* db, const char* sql);
  ~statement();
  statement(const statement&) = delete;
  statement& operator=(const statement&) = delete;

  statement& bind(int index, std::string_view text);
  statement& bind(int index, std::int64_t value);
  statement& bind_u64(int index, std::uint64_t value);
  statement& bind_null(int index);
  statement& bind_opt(int index, const std::optional<std::string>& text);
  statement& bind_opt_u64(int index, const std::optional<std::uint64_t>& value);

  // true se c'e' una riga; false a fine; altro: stop_error.
  bool step();
  void run();  // esegue e richiede la fine senza righe
  std::string text(int column) const;
  std::int64_t integer(int column) const;
  std::uint64_t u64(int column) const;
  bool is_null(int column) const;
  std::optional<std::uint64_t> opt_u64(int column) const;
  std::optional<std::string> opt_text(int column) const;

 private:
  sqlite3* db_;
  sqlite3_stmt* stmt_ = nullptr;
};

// BEGIN IMMEDIATE; rollback nel distruttore se commit() non e' stato chiamato.
class transaction {
 public:
  explicit transaction(sqlite3* db);
  ~transaction();
  transaction(const transaction&) = delete;
  transaction& operator=(const transaction&) = delete;
  void commit();

 private:
  sqlite3* db_;
  bool open_ = false;
};

void exec(sqlite3* db, const char* sql);

// Sorgente di byte a blocchi: restituisce i byte scritti nel buffer, 0 a fine flusso.
using byte_reader = std::function<std::size_t(std::span<std::uint8_t>)>;

// Passi 1-5 del protocollo atomico dentro `dirfd`, con digest e scrittura a memoria costante.
// `final_name` vuoto: il nome e' <sha256>.<estensione>. `fault_prefix` precede i nomi dei punti
// (create, write, fsync_file, rename, fsync_dir). `expected` diverso dal digest letto: integrity
// store.expected_digest_mismatch, il temporaneo viene rimosso prima di ogni rename.
struct atomic_options {
  int dirfd = -1;
  std::string fault_prefix;
  std::string conflict_code;
  std::string final_name;
  std::string extension;
  // Il nome finale esistente viene sostituito (rename atomico) invece di essere un conflitto.
  bool replace = false;
  const fault_injector* injector = nullptr;
};
struct atomic_result {
  std::string name;
  content_digest digest;
  bool reused = false;
  // Stato del file al nome finale, verificato uguale a quello del fd scritto (stesso inode, stesse size/mtime/ctime).
  struct stat final_stat {};
};
atomic_result atomic_write_stream(const atomic_options& options, const byte_reader& reader,
                                  const std::optional<content_digest>& expected);
atomic_result atomic_write(const atomic_options& options, std::span<const std::uint8_t> bytes,
                           const std::optional<content_digest>& expected);

byte_reader fd_reader(int fd, std::string name);
// Digest in streaming di un file aperto, a partire dall'offset corrente; EOF atteso entro `max_bytes`.
content_digest stream_digest(int fd, std::uint64_t max_bytes, std::string_view name);

std::string random_name(std::size_t bytes);


// Voci di una directory (senza . e ..); errore di readdir distinto dalla fine.
std::vector<std::string> read_directory(int dirfd, const char* code);

// Rilancia `e` con categoria/codice nuovi conservando contesto (incluso errno, sqlite_rc, sqlite_msg) e causa.
[[noreturn]] void rethrow_as(const stop_error& e, error_category category, const char* code, const char* message);

void inject(const fault_injector* injector, std::string_view point);

// Legge un file regolare gia' aperto (fstat, dimensione attesa, nessun file piu' grande di `max_bytes`).
std::vector<std::uint8_t> read_all(int fd, std::uint64_t max_bytes, const char* code, std::string_view name);
void fsync_fd(int fd, const char* code, std::string_view name);
unique_fd open_dir_nofollow(int parent_fd, const char* name, const char* symlink_code);

bool valid_extension(std::string_view ext);
bool valid_artifact_name(std::string_view name);

}  // namespace astro::sqlint
