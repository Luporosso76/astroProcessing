#include "sqlite_internal.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <algorithm>
#include <cstring>

namespace astro::sqlint {

unique_fd& unique_fd::operator=(unique_fd&& o) noexcept {
  if (this != &o) {
    if (fd_ >= 0) ::close(fd_);
    fd_ = std::exchange(o.fd_, -1);
  }
  return *this;
}

unique_fd::~unique_fd() {
  if (fd_ >= 0) ::close(fd_);
}

void throw_errno(error_category category, const std::string& code, const char* what, int err,
                 stop_error::context_list context) {
  context.emplace_back("errno", std::to_string(err));
  context.emplace_back("errno_text", std::strerror(err));
  throw stop_error(category, code, what, std::move(context));
}

void throw_sql(sqlite3* db, int rc, const char* what) {
  stop_error::context_list context{{"sqlite_rc", std::to_string(rc)},
                                   {"sqlite_msg", db != nullptr ? sqlite3_errmsg(db) : sqlite3_errstr(rc)}};
  const int primary = rc & 0xff;
  if (primary == SQLITE_CORRUPT || primary == SQLITE_NOTADB) {
    throw stop_error(error_category::integrity, "store.corrupt", what, std::move(context));
  }
  if (primary == SQLITE_CONSTRAINT) {
    throw stop_error(error_category::integrity, "store.constraint", what, std::move(context));
  }
  throw stop_error(error_category::io, "store.sqlite", what, std::move(context));
}

statement::statement(sqlite3* db, const char* sql) : db_(db) {
  const int rc = sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr);
  if (rc != SQLITE_OK) throw_sql(db, rc, "preparazione dell'istruzione SQL");
}

statement::~statement() { sqlite3_finalize(stmt_); }

statement& statement::bind(int index, std::string_view text) {
  const int rc = sqlite3_bind_text(stmt_, index, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
  if (rc != SQLITE_OK) throw_sql(db_, rc, "bind del testo");
  return *this;
}

statement& statement::bind(int index, std::int64_t value) {
  const int rc = sqlite3_bind_int64(stmt_, index, value);
  if (rc != SQLITE_OK) throw_sql(db_, rc, "bind dell'intero");
  return *this;
}

statement& statement::bind_u64(int index, std::uint64_t value) {
  if (value > static_cast<std::uint64_t>(INT64_MAX)) {
    throw stop_error(error_category::limit_exceeded, "store.integer_range", "intero oltre 2^63-1",
                     {{"valore", std::to_string(value)}});
  }
  return bind(index, static_cast<std::int64_t>(value));
}

statement& statement::bind_null(int index) {
  const int rc = sqlite3_bind_null(stmt_, index);
  if (rc != SQLITE_OK) throw_sql(db_, rc, "bind del null");
  return *this;
}

statement& statement::bind_opt(int index, const std::optional<std::string>& text) {
  return text ? bind(index, std::string_view(*text)) : bind_null(index);
}

statement& statement::bind_opt_u64(int index, const std::optional<std::uint64_t>& value) {
  return value ? bind_u64(index, *value) : bind_null(index);
}

bool statement::step() {
  const int rc = sqlite3_step(stmt_);
  if (rc == SQLITE_ROW) return true;
  if (rc == SQLITE_DONE) return false;
  throw_sql(db_, rc, "esecuzione dell'istruzione SQL");
}

void statement::run() {
  ASTRO_ASSERT(!step());
  sqlite3_reset(stmt_);
}

std::string statement::text(int column) const {
  const auto* p = sqlite3_column_text(stmt_, column);
  return p == nullptr ? std::string() : std::string(reinterpret_cast<const char*>(p), static_cast<std::size_t>(sqlite3_column_bytes(stmt_, column)));
}

std::int64_t statement::integer(int column) const { return sqlite3_column_int64(stmt_, column); }

std::uint64_t statement::u64(int column) const {
  const std::int64_t v = sqlite3_column_int64(stmt_, column);
  ASTRO_ASSERT(v >= 0);
  return static_cast<std::uint64_t>(v);
}

bool statement::is_null(int column) const { return sqlite3_column_type(stmt_, column) == SQLITE_NULL; }

std::optional<std::uint64_t> statement::opt_u64(int column) const {
  return is_null(column) ? std::nullopt : std::optional<std::uint64_t>(u64(column));
}

std::optional<std::string> statement::opt_text(int column) const {
  return is_null(column) ? std::nullopt : std::optional<std::string>(text(column));
}

transaction::transaction(sqlite3* db) : db_(db) {
  exec(db, "BEGIN IMMEDIATE");
  open_ = true;
}

transaction::~transaction() {
  if (open_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
}

void transaction::commit() {
  exec(db_, "COMMIT");
  open_ = false;
}

void exec(sqlite3* db, const char* sql) {
  const int rc = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
  if (rc != SQLITE_OK) throw_sql(db, rc, sql);
}

void inject(const fault_injector* injector, std::string_view point) {
  if (injector == nullptr || !*injector) return;
  const int err = (*injector)(point);
  if (err != 0) {
    throw_errno(error_category::io, "store.syscall_failed", "chiamata di sistema fallita (iniezione)", err,
                {{"punto", std::string(point)}});
  }
}

namespace {

std::string random_hex(std::size_t bytes) {
  std::vector<unsigned char> raw(bytes);
  std::size_t got = 0;
  while (got < bytes) {
    const ssize_t n = ::getrandom(raw.data() + got, bytes - got, 0);
    if (n < 0) throw_errno(error_category::io, "store.getrandom", "getrandom fallita", errno);
    got += static_cast<std::size_t>(n);
  }
  static constexpr char hex[] = "0123456789abcdef";
  std::string out;
  for (const unsigned char b : raw) {
    out.push_back(hex[b >> 4]);
    out.push_back(hex[b & 0x0f]);
  }
  return out;
}

void write_fully(int fd, std::span<const std::uint8_t> bytes, const std::string& name) {
  std::size_t done = 0;
  while (done < bytes.size()) {
    const ssize_t n = ::write(fd, bytes.data() + done, bytes.size() - done);
    if (n < 0) {
      if (errno == EINTR) continue;  // interruzione da segnale: si riprende lo stesso trasferimento
      throw_errno(error_category::io, "store.write", "scrittura fallita", errno, {{"file", name}});
    }
    done += static_cast<std::size_t>(n);
  }
}

constexpr std::size_t transfer_block = std::size_t{1} << 16;

struct temporary_file {
  content_digest digest;
  unique_fd fd;
};

temporary_file write_temporary(const atomic_options& o, const std::string& tmp, const byte_reader& reader,
                               const std::optional<content_digest>& expected) {
  inject(o.injector, o.fault_prefix + ".create");
  unique_fd fd(::openat(o.dirfd, tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (!fd) throw_errno(error_category::io, "store.open", "creazione del temporaneo fallita", errno, {{"file", tmp}});
  try {
    inject(o.injector, o.fault_prefix + ".write");
    sha256_stream hasher;
    std::vector<std::uint8_t> block(transfer_block);
    while (const std::size_t n = reader(block)) {
      hasher.update({block.data(), n});
      write_fully(fd.get(), {block.data(), n}, tmp);
    }
    const content_digest digest = hasher.finish();
    if (expected && !(*expected == digest)) {
      throw stop_error(error_category::integrity, "store.expected_digest_mismatch",
                       "digest o dimensione diversi da quelli attesi",
                       {{"atteso", expected->sha256}, {"calcolato", digest.sha256}});
    }
    inject(o.injector, o.fault_prefix + ".fsync_file");
    if (::fsync(fd.get()) != 0) throw_errno(error_category::io, "store.fsync", "fsync del file fallita", errno, {{"file", tmp}});
    return temporary_file{digest, std::move(fd)};
  } catch (...) {
    ::unlinkat(o.dirfd, tmp.c_str(), 0);
    throw;
  }
}

bool same_stat(const struct stat& a, const struct stat& b) {
  return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size && a.st_mtim.tv_sec == b.st_mtim.tv_sec &&
         a.st_mtim.tv_nsec == b.st_mtim.tv_nsec && a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}

bool same_existing_content(const atomic_options& o, const std::string& name, const content_digest& digest) {
  unique_fd fd(::openat(o.dirfd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
  if (!fd) {
    if (errno == ELOOP) return false;
    throw_errno(error_category::io, "store.open", "apertura del file esistente fallita", errno, {{"file", name}});
  }
  struct stat st {};
  if (::fstat(fd.get(), &st) != 0) throw_errno(error_category::io, "store.fstat", "fstat fallita", errno, {{"file", name}});
  if (!S_ISREG(st.st_mode) || static_cast<std::uint64_t>(st.st_size) != digest.size_bytes) return false;
  return stream_digest(fd.get(), digest.size_bytes, name) == digest;
}

}  // namespace

std::string random_name(std::size_t bytes) { return random_hex(bytes); }

byte_reader fd_reader(int fd, std::string name) {
  return [fd, name = std::move(name)](std::span<std::uint8_t> buf) -> std::size_t {
    for (;;) {
      const ssize_t n = ::read(fd, buf.data(), buf.size());
      if (n >= 0) return static_cast<std::size_t>(n);
      if (errno == EINTR) continue;  // interruzione da segnale: si riprende la stessa lettura
      throw_errno(error_category::io, "store.read", "lettura fallita", errno, {{"file", name}});
    }
  };
}

content_digest stream_digest(int fd, std::uint64_t max_bytes, std::string_view name) {
  const byte_reader reader = fd_reader(fd, std::string(name));
  sha256_stream hasher;
  std::vector<std::uint8_t> block(transfer_block);
  std::uint64_t total = 0;
  while (const std::size_t n = reader(block)) {
    total += n;
    if (total > max_bytes) {
      throw stop_error(error_category::integrity, "store.size_changed", "il file e' piu' grande del previsto",
                       {{"file", std::string(name)}});
    }
    hasher.update({block.data(), n});
  }
  return hasher.finish();
}

atomic_result atomic_write(const atomic_options& o, std::span<const std::uint8_t> bytes,
                           const std::optional<content_digest>& expected) {
  std::size_t pos = 0;
  const byte_reader reader = [&](std::span<std::uint8_t> buf) {
    const std::size_t n = std::min(buf.size(), bytes.size() - pos);
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(pos), n, buf.begin());
    pos += n;
    return n;
  };
  return atomic_write_stream(o, reader, expected);
}

atomic_result atomic_write_stream(const atomic_options& o, const byte_reader& reader,
                                  const std::optional<content_digest>& expected) {
  const std::string tmp = ".tmp-" + random_hex(16);
  temporary_file written = write_temporary(o, tmp, reader, expected);
  const content_digest digest = written.digest;
  const std::string name = o.final_name.empty() ? digest.sha256 + "." + o.extension : o.final_name;
  try {
    inject(o.injector, o.fault_prefix + ".rename");
  } catch (...) {
    ::unlinkat(o.dirfd, tmp.c_str(), 0);
    throw;
  }
  bool reused = false;
  if (::renameat2(o.dirfd, tmp.c_str(), o.dirfd, name.c_str(), RENAME_NOREPLACE) != 0) {
    const int err = errno;
    if (err != EEXIST) {
      ::unlinkat(o.dirfd, tmp.c_str(), 0);
      throw_errno(error_category::io, "store.rename", "renameat2 fallita", err, {{"file", name}});
    }
    bool same = false;
    try {
      same = same_existing_content(o, name, digest);
    } catch (...) {
      ::unlinkat(o.dirfd, tmp.c_str(), 0);
      throw;
    }
    if (!same) {
      ::unlinkat(o.dirfd, tmp.c_str(), 0);
      throw stop_error(error_category::security, o.conflict_code,
                       "esiste gia' un file con quel nome ma contenuto diverso o non regolare", {{"file", name}});
    }
    if (::unlinkat(o.dirfd, tmp.c_str(), 0) != 0) {
      throw_errno(error_category::io, "store.unlink", "rimozione del temporaneo fallita", errno, {{"file", tmp}});
    }
    reused = true;
  }
  inject(o.injector, o.fault_prefix + ".fsync_dir");
  if (::fsync(o.dirfd) != 0) throw_errno(error_category::io, "store.fsync", "fsync della directory fallita", errno);
  atomic_result result{name, digest, reused, {}};
  const auto changed = [&](const char* why) {
    return stop_error(error_category::integrity, "store.file_identity_changed", why, {{"file", name}});
  };
  // Il digest registrato e' quello del contenuto riletto dal file pubblicato, con identita' uguale prima e dopo.
  unique_fd published(::openat(o.dirfd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
  if (!published) throw_errno(error_category::io, "store.open", "apertura del file pubblicato fallita", errno, {{"file", name}});
  struct stat before {};
  if (::fstat(published.get(), &before) != 0) throw_errno(error_category::io, "store.fstat", "fstat fallita", errno, {{"file", name}});
  if (!S_ISREG(before.st_mode)) throw changed("il file pubblicato non e' regolare");
  if (!reused) {
    struct stat fd_stat {};
    if (::fstat(written.fd.get(), &fd_stat) != 0) throw_errno(error_category::io, "store.fstat", "fstat fallita", errno, {{"file", name}});
    if (!same_stat(fd_stat, before)) throw changed("il file finale non e' quello scritto");
  }
  if (static_cast<std::uint64_t>(before.st_size) != digest.size_bytes) throw changed("dimensione diversa dal digest calcolato");
  const content_digest reread = stream_digest(published.get(), digest.size_bytes, name);
  struct stat after {};
  if (::fstat(published.get(), &after) != 0) throw_errno(error_category::io, "store.fstat", "fstat fallita", errno, {{"file", name}});
  if (!same_stat(before, after)) throw changed("il file e' cambiato durante la rilettura");
  if (!(reread == digest)) throw changed("il contenuto pubblicato differisce dal digest calcolato");
  struct stat named {};
  if (::fstatat(o.dirfd, name.c_str(), &named, AT_SYMLINK_NOFOLLOW) != 0 || !same_stat(named, after)) {
    throw changed("il nome finale non indica piu' il file riletto");
  }
  result.final_stat = after;
  return result;
}

std::vector<std::string> read_directory(int dirfd, const char* code) {
  DIR* stream = ::fdopendir(::dup(dirfd));
  if (stream == nullptr) throw_errno(error_category::io, code, "fdopendir fallita", errno);
  ::rewinddir(stream);  // il dup condivide l'offset del fd originale
  std::vector<std::string> names;
  for (;;) {
    errno = 0;
    const dirent* de = ::readdir(stream);
    if (de == nullptr) {
      const int err = errno;
      ::closedir(stream);
      if (err != 0) throw_errno(error_category::io, code, "readdir fallita", err);
      break;
    }
    const std::string n = de->d_name;
    if (n != "." && n != "..") names.push_back(n);
  }
  return names;
}

void rethrow_as(const stop_error& e, error_category category, const char* code, const char* message) {
  stop_error::context_list context = e.context();
  context.emplace_back("causa", e.code());
  context.emplace_back("causa_categoria", to_string(e.category()));
  context.emplace_back("causa_messaggio", e.message());
  throw stop_error(category, code, message, std::move(context));
}

std::vector<std::uint8_t> read_all(int fd, std::uint64_t max_bytes, const char* code, std::string_view name) {
  struct stat st {};
  if (::fstat(fd, &st) != 0) throw_errno(error_category::io, code, "fstat fallita", errno, {{"file", std::string(name)}});
  if (static_cast<std::uint64_t>(st.st_size) > max_bytes) {
    throw stop_error(error_category::limit_exceeded, code, "file oltre il limite", {{"file", std::string(name)}});
  }
  std::vector<std::uint8_t> out;
  out.reserve(static_cast<std::size_t>(st.st_size));
  std::uint8_t buffer[1 << 16];
  for (;;) {
    const ssize_t n = ::read(fd, buffer, sizeof(buffer));
    if (n < 0) {
      if (errno == EINTR) continue;
      throw_errno(error_category::io, code, "lettura fallita", errno, {{"file", std::string(name)}});
    }
    if (n == 0) break;
    if (out.size() + static_cast<std::size_t>(n) > max_bytes) {
      throw stop_error(error_category::limit_exceeded, code, "file oltre il limite", {{"file", std::string(name)}});
    }
    out.insert(out.end(), buffer, buffer + n);
  }
  return out;
}

void fsync_fd(int fd, const char* code, std::string_view name) {
  if (::fsync(fd) != 0) throw_errno(error_category::io, code, "fsync fallita", errno, {{"file", std::string(name)}});
}

unique_fd open_dir_nofollow(int parent_fd, const char* name, const char* symlink_code) {
  unique_fd fd(::openat(parent_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  if (fd) return fd;
  const int err = errno;
  if (err == ELOOP || err == ENOTDIR) {
    throw stop_error(error_category::security, symlink_code, "la directory e' un symlink o non e' una directory",
                     {{"nome", name}});
  }
  throw_errno(error_category::io, "store.open", "apertura della directory fallita", err, {{"nome", name}});
}

bool valid_extension(std::string_view ext) {
  if (ext.empty() || ext.size() > 16) return false;
  for (const char c : ext) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
  }
  return true;
}

bool valid_artifact_name(std::string_view name) {
  return name.size() > 65 && name[64] == '.' && is_sha256_hex(std::string(name.substr(0, 64))) &&
         valid_extension(name.substr(65));
}

}  // namespace astro::sqlint
