#include "astro/thumbnail_cache.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <set>

#include "astro/json.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;

static_assert(std::endian::native == std::endian::little, "il formato delle voci di cache e' little-endian");

namespace {

constexpr char magic[8] = {'A', 'S', 'T', 'R', 'T', 'H', 'M', '1'};
constexpr std::size_t entry_header_bytes = 8 + 7 * sizeof(std::uint32_t);
constexpr const char* entry_suffix = ".thm";

bool is_hex64(std::string_view s) { return s.size() == 64 && is_sha256_hex(std::string(s)); }

// `<chiave>.<digest>.thm`
bool is_entry_name(std::string_view name) {
  return name.size() == 64 + 1 + 64 + 4 && is_hex64(name.substr(0, 64)) && name[64] == '.' &&
         is_hex64(name.substr(65, 64)) && name.substr(129) == entry_suffix;
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  std::uint8_t b[4];
  std::memcpy(b, &v, 4);
  out.insert(out.end(), b, b + 4);
}

std::vector<std::uint8_t> serialize(const fits_preview& pv) {
  std::vector<std::uint8_t> out(magic, magic + 8);
  for (const std::uint32_t v : {pv.source_width, pv.source_height, pv.planes, pv.block_width, pv.block_height, pv.width, pv.height}) {
    put_u32(out, v);
  }
  const std::size_t n = pv.samples.size();
  out.resize(out.size() + n * 8);
  std::memcpy(out.data() + entry_header_bytes, pv.samples.data(), n * 4);
  std::memcpy(out.data() + entry_header_bytes + n * 4, pv.finite_counts.data(), n * 4);
  return out;
}

[[noreturn]] void corrupt(const std::string& name, const std::string& reason, stop_error::context_list extra = {}) {
  extra.emplace_back("voce", name);
  extra.emplace_back("motivo", reason);
  throw stop_error(error_category::integrity, "cache.corrupt", "voce di cache non valida", std::move(extra));
}

fits_preview parse(const std::string& name, const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < entry_header_bytes || std::memcmp(bytes.data(), magic, 8) != 0) corrupt(name, "intestazione");
  std::uint32_t v[7];
  std::memcpy(v, bytes.data() + 8, sizeof v);
  fits_preview pv;
  pv.source_width = v[0];
  pv.source_height = v[1];
  pv.planes = v[2];
  pv.block_width = v[3];
  pv.block_height = v[4];
  pv.width = v[5];
  pv.height = v[6];
  const auto ok = [](std::uint32_t x) { return x >= 1 && x <= fits_limits::max_axis_length; };
  if (!ok(pv.source_width) || !ok(pv.source_height) || !ok(pv.block_width) || !ok(pv.block_height) || pv.planes < 1 ||
      pv.planes > fits_limits::max_planes ||
      pv.width != (pv.source_width + pv.block_width - 1) / pv.block_width ||
      pv.height != (pv.source_height + pv.block_height - 1) / pv.block_height) {
    corrupt(name, "geometria");
  }
  const std::uint64_t n = std::uint64_t{pv.width} * pv.height * pv.planes;
  if (bytes.size() != entry_header_bytes + n * 8) corrupt(name, "dimensione");
  pv.samples.resize(n);
  pv.finite_counts.resize(n);
  std::memcpy(pv.samples.data(), bytes.data() + entry_header_bytes, n * 4);
  std::memcpy(pv.finite_counts.data(), bytes.data() + entry_header_bytes + n * 4, n * 4);
  return pv;
}

struct opened_entry {
  reservation memory;  // dichiarata prima di `bytes`: si rilascia dopo il buffer
  std::vector<std::uint8_t> bytes;
  std::uint64_t size = 0;
};

}  // namespace

const char* to_string(cache_outcome outcome) noexcept {
  switch (outcome) {
    case cache_outcome::cache_hit: return "cache_hit";
    case cache_outcome::computed: return "computed";
    case cache_outcome::rebuilt: return "rebuilt";
  }
  std::abort();
}

struct thumbnail_cache::impl {
  unique_fd dir;
  cache_options options;

  // Controlli di confinamento su un nome della directory, senza seguire symlink.
  struct stat check(const std::string& name) const {
    struct stat st {};
    if (::fstatat(dir.get(), name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
      throw_errno(error_category::io, "cache.stat", "fstat della voce fallita", errno, {{"voce", name}});
    }
    if (S_ISLNK(st.st_mode)) {
      throw stop_error(error_category::security, "cache.symlink", "la voce e' un symlink", {{"voce", name}});
    }
    if (!S_ISREG(st.st_mode)) {
      throw stop_error(error_category::security, "cache.not_regular", "la voce non e' un file regolare", {{"voce", name}});
    }
    if (st.st_nlink != 1) {
      throw stop_error(error_category::security, "cache.hardlink", "la voce ha piu' di un nome",
                       {{"voce", name}, {"link", std::to_string(st.st_nlink)}});
    }
    return st;
  }

  // Prenota `factor` volte la dimensione della voce (buffer letto, piu' l'anteprima analizzata quando factor = 2).
  opened_entry read_entry(const std::string& name, memory_pool& pool, std::uint64_t factor) const {
    (void)check(name);
    opened_entry e;
    unique_fd fd(::openat(dir.get(), name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
    if (!fd) {
      if (errno == ELOOP) throw stop_error(error_category::security, "cache.symlink", "la voce e' un symlink", {{"voce", name}});
      throw_errno(error_category::io, "cache.open", "apertura della voce fallita", errno, {{"voce", name}});
    }
    struct stat st {};
    if (::fstat(fd.get(), &st) != 0) throw_errno(error_category::io, "cache.stat", "fstat della voce fallita", errno, {{"voce", name}});
    if (!S_ISREG(st.st_mode) || st.st_nlink != 1) {
      throw stop_error(error_category::security, st.st_nlink != 1 ? "cache.hardlink" : "cache.not_regular",
                       "la voce non e' un file regolare con un solo nome", {{"voce", name}});
    }
    // La prenotazione e il limite di lettura sono la dimensione dello stesso fd: una crescita successiva e' uno stop.
    const auto size = static_cast<std::uint64_t>(st.st_size);
    if (size > options.quota_bytes) {
      throw stop_error(error_category::budget, "cache.quota_exceeded", "la voce supera la quota della cache",
                       {{"voce", name}, {"quota", std::to_string(options.quota_bytes)}, {"dimensione", std::to_string(size)}});
    }
    e.memory = reserve_or_stop(pool, factor * size, "cache.entry");
    inject(&options.fault, "cache.entry.read");
    e.bytes = read_all(fd.get(), size, "cache.read", name);
    e.size = e.bytes.size();
    return e;
  }

  std::vector<std::string> entries_with_prefix(const std::string& key) const {
    std::vector<std::string> out;
    for (const std::string& n : read_directory(dir.get(), "cache.readdir", &options.fault)) {
      if (n.size() > 65 && n.compare(0, 64, key) == 0 && n[64] == '.') out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
  }

  // Somma delle dimensioni delle voci, escluse `skip`. Una voce non regolare o con piu' nomi e' security.
  std::uint64_t total_size(const std::set<std::string>& skip) const {
    std::uint64_t total = 0;
    for (const std::string& n : read_directory(dir.get(), "cache.readdir", &options.fault)) {
      if (!is_entry_name(n) || skip.count(n) != 0) continue;
      total += static_cast<std::uint64_t>(check(n).st_size);
    }
    return total;
  }

  void require_quota(std::uint64_t new_bytes, const std::set<std::string>& skip) const {
    const std::uint64_t total = total_size(skip);
    if (total > options.quota_bytes || new_bytes > options.quota_bytes - total) {
      throw stop_error(error_category::budget, "cache.quota_exceeded", "la voce supera la quota della cache",
                       {{"quota", std::to_string(options.quota_bytes)},
                        {"occupato", std::to_string(total)},
                        {"nuova", std::to_string(new_bytes)}});
    }
  }

  atomic_result write(const std::string& key, const std::vector<std::uint8_t>& bytes, bool replace) const {
    atomic_options o;
    o.dirfd = dir.get();
    o.fault_prefix = "cache.entry";
    o.conflict_code = "cache.name_conflict";
    o.final_name = key + "." + digest_of(bytes).sha256 + entry_suffix;
    o.replace = replace;
    o.injector = &options.fault;
    return atomic_write(o, bytes, std::nullopt);
  }
};

thumbnail_cache::thumbnail_cache(const std::filesystem::path& project_root, const cache_options& options)
    : impl_(std::make_unique<impl>()) {
  if (options.quota_bytes == 0) {
    throw stop_error(error_category::invalid_input, "cache.quota", "la quota della cache e' obbligatoria e positiva");
  }
  impl_->options = options;
  unique_fd root(::open(project_root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (!root) {
    throw_errno(error_category::io, "cache.open", "apertura del progetto fallita", errno, {{"progetto", project_root.string()}});
  }
  const auto ensure = [](int parent, const char* name) {
    if (::mkdirat(parent, name, 0755) != 0 && errno != EEXIST) {
      throw_errno(error_category::io, "cache.mkdir", "creazione della directory fallita", errno, {{"nome", name}});
    }
    return open_dir_nofollow(parent, name, "cache.symlink");
  };
  const unique_fd cache = ensure(root.get(), "cache");
  impl_->dir = ensure(cache.get(), "thumbnails");
}

thumbnail_cache::~thumbnail_cache() = default;

std::string thumbnail_cache::key_of(const content_digest& source, const preview_request& request) {
  json o = json::object();
  o.set("source_sha256", json::string(source.sha256));
  o.set("source_size", json::uinteger(source.size_bytes));
  o.set("block_width", json::uinteger(request.block_width));
  o.set("block_height", json::uinteger(request.block_height));
  o.set("algorithm_version", json::uinteger(thumbnail_cache_algorithm_version));
  const std::string text = "astro.cache.thumbnail.v1\n" + canonical_json(o);
  return sha256_hex({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

thumbnail_result thumbnail_cache::get(const fits_input& input, const content_digest& source,
                                      const preview_request& request, memory_pool& pool, const cancel_token* cancel) {
  const std::string key = key_of(source, request);
  const std::vector<std::string> existing = impl_->entries_with_prefix(key);
  if (existing.size() > 1) corrupt(existing.front(), "voci multiple per la stessa chiave", {{"voci", std::to_string(existing.size())}});
  if (existing.size() == 1) {
    const std::string& name = existing.front();
    if (!is_entry_name(name)) corrupt(name, "nome");
    const opened_entry e = impl_->read_entry(name, pool, 2);
    const std::string found = digest_of(e.bytes).sha256;
    const std::string expected = name.substr(65, 64);
    if (found != expected) corrupt(name, "digest", {{"atteso", expected}, {"trovato", found}});
    return thumbnail_result{thumbnail_entry{key, name, content_digest{found, e.size}, cache_outcome::cache_hit}, parse(name, e.bytes)};
  }
  ++computations_;
  fits_preview pv = make_preview(input, request, pool, cancel);
  // L'anteprima restituita e il buffer serializzato vivono insieme: 2 volte la dimensione della voce.
  const reservation serialize_memory = reserve_or_stop(pool, 2 * (entry_header_bytes + pv.samples.size() * 8), "cache.compute");
  const std::vector<std::uint8_t> bytes = serialize(pv);
  impl_->require_quota(bytes.size(), {});
  const atomic_result written = impl_->write(key, bytes, false);
  return thumbnail_result{thumbnail_entry{key, written.name, written.digest, cache_outcome::computed}, std::move(pv)};
}

thumbnail_result thumbnail_cache::rebuild(const fits_input& input, const content_digest& source,
                                          const preview_request& request, memory_pool& pool, const cancel_token* cancel,
                                          std::string& replaced_name, std::string& replaced_found_digest) {
  const std::string key = key_of(source, request);
  const std::vector<std::string> existing = impl_->entries_with_prefix(key);
  if (existing.empty()) {
    throw stop_error(error_category::invalid_input, "cache.rebuild_nothing", "nessuna voce da ricostruire", {{"chiave", key}});
  }
  replaced_name.clear();
  replaced_found_digest.clear();
  for (const std::string& name : existing) {
    const opened_entry e = impl_->read_entry(name, pool, 1);
    replaced_name += (replaced_name.empty() ? "" : ",") + name;
    replaced_found_digest += (replaced_found_digest.empty() ? "" : ",") + digest_of(e.bytes).sha256;
  }
  ++computations_;
  fits_preview pv = make_preview(input, request, pool, cancel);
  const reservation serialize_memory = reserve_or_stop(pool, 2 * (entry_header_bytes + pv.samples.size() * 8), "cache.compute");
  const std::vector<std::uint8_t> bytes = serialize(pv);
  impl_->require_quota(bytes.size(), std::set<std::string>(existing.begin(), existing.end()));
  const atomic_result written = impl_->write(key, bytes, true);
  for (const std::string& name : existing) {
    if (name == written.name) continue;
    inject(&impl_->options.fault, "cache.entry.unlink");
    if (::unlinkat(impl_->dir.get(), name.c_str(), 0) != 0) {
      throw_errno(error_category::io, "cache.unlink", "rimozione della voce sostituita fallita", errno, {{"voce", name}});
    }
  }
  fsync_fd(impl_->dir.get(), "cache.fsync", "cache/thumbnails");
  return thumbnail_result{thumbnail_entry{key, written.name, written.digest, cache_outcome::rebuilt}, std::move(pv)};
}

std::vector<std::string> thumbnail_cache::cleanup_temporaries() {
  std::vector<std::string> temporaries;
  for (const std::string& n : read_directory(impl_->dir.get(), "cache.readdir", &impl_->options.fault)) {
    if (n.rfind(".tmp-", 0) != 0) continue;
    (void)impl_->check(n);
    temporaries.push_back(n);
  }
  std::sort(temporaries.begin(), temporaries.end());
  for (const std::string& n : temporaries) {
    if (::unlinkat(impl_->dir.get(), n.c_str(), 0) != 0) {
      throw_errno(error_category::io, "cache.unlink", "rimozione del temporaneo fallita", errno, {{"voce", n}});
    }
  }
  return temporaries;
}

}  // namespace astro
