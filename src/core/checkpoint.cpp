#include "astro/checkpoint.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>

#include "astro/json.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t checkpoint_max_bytes = std::uint64_t{64} << 20;

std::string file_name(const std::string& name) {
  const bool valid = !name.empty() && name.size() <= 64 &&
                     std::all_of(name.begin(), name.end(), [](char c) {
                       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                              c == '-';
                     });
  if (!valid) {
    throw stop_error(error_category::invalid_input, "sched.checkpoint_name", "nome del checkpoint non valido",
                     {{"nome", name}});
  }
  return name + ".ckpt.json";
}

unique_fd open_checkpoint_dir(const fs::path& dir) {
  unique_fd fd(::open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  if (!fd) {
    throw_errno(error_category::io, "sched.checkpoint_dir", "directory dei checkpoint inutilizzabile", errno,
                {{"dir", dir.string()}});
  }
  return fd;
}

[[noreturn]] void malformed(const char* why, const std::string& name) {
  throw stop_error(error_category::integrity, "sched.checkpoint_malformed", why, {{"file", name}});
}

const json& field(const json& doc, const char* key, json::kind kind, const std::string& name) {
  const json* v = doc.find(key);
  if (v == nullptr || v->type() != kind) malformed("campo mancante o di tipo errato", name);
  return *v;
}

std::string hex_field(const json& doc, const char* key, const std::string& name) {
  const std::string& v = field(doc, key, json::kind::string, name).as_string();
  if (!is_sha256_hex(v)) malformed("digest non valido", name);
  return v;
}

}  // namespace

std::string recipe_digest(const node_spec& spec, const job_plan& plan) {
  json spill = json::null();
  if (plan.spill) {
    spill = json::object();
    spill.set("scratch_dir", json::string(plan.spill->scratch_dir.string()));
    spill.set("quota_bytes", json::uinteger(plan.spill->quota_bytes));
  }
  json doc = json::object();
  doc.set("node_spec", spec.to_json());
  doc.set("tile_rows", json::uinteger(plan.tile_rows));
  doc.set("worker_count", json::uinteger(plan.worker_count));
  doc.set("spill", std::move(spill));
  doc.set("checkpoint_every", json::uinteger(plan.checkpoint_every));
  const std::string text = canonical_json(doc);
  return sha256_hex({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

void write_checkpoint(const fs::path& dir, const std::string& checkpoint_name, const checkpoint_record& record,
                      const fault_injector* injector) {
  const std::string name = file_name(checkpoint_name);
  if (record.completed.size() != record.partial_digests.size()) {
    throw stop_error(error_category::invalid_input, "sched.checkpoint_inconsistent",
                     "indici completati e digest parziali di lunghezza diversa");
  }
  json completed = json::array();
  for (const std::uint64_t i : record.completed) completed.push(json::uinteger(i));
  json digests = json::array();
  for (const std::string& d : record.partial_digests) digests.push(json::string(d));
  json doc = json::object();
  doc.set("format", json::string(checkpoint_format));
  doc.set("node_key", json::string(record.node_key));
  doc.set("recipe_digest", json::string(record.recipe_digest));
  doc.set("backend", json::string(record.backend));
  doc.set("task_count", json::uinteger(record.task_count));
  doc.set("completed", std::move(completed));
  doc.set("partial_digests", std::move(digests));
  const std::string text = canonical_json(doc);
  if (text.size() > checkpoint_max_bytes) {
    throw stop_error(error_category::limit_exceeded, "sched.checkpoint_size",
                     "il checkpoint supera il limite di lettura; il precedente resta invariato",
                     {{"byte", std::to_string(text.size())}, {"limite", std::to_string(checkpoint_max_bytes)}});
  }
  const unique_fd dirfd = open_checkpoint_dir(dir);
  atomic_options options;
  options.dirfd = dirfd.get();
  options.fault_prefix = "checkpoint";
  options.conflict_code = "sched.checkpoint_conflict";
  options.final_name = name;
  options.replace = true;
  options.injector = injector;
  atomic_write(options, {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()}, std::nullopt);
}

std::optional<checkpoint_record> read_checkpoint(const fs::path& dir, const std::string& checkpoint_name) {
  const std::string name = file_name(checkpoint_name);
  const unique_fd dirfd = open_checkpoint_dir(dir);
  unique_fd fd(::openat(dirfd.get(), name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
  if (!fd) {
    if (errno == ENOENT) return std::nullopt;
    throw_errno(error_category::io, "sched.checkpoint_read", "apertura del checkpoint fallita", errno, {{"file", name}});
  }
  std::vector<std::uint8_t> bytes;
  try {
    bytes = read_all(fd.get(), checkpoint_max_bytes, "sched.checkpoint_read", name);
  } catch (const stop_error& e) {
    if (e.category() != error_category::limit_exceeded) throw;
    rethrow_as(e, error_category::limit_exceeded, "sched.checkpoint_size", "checkpoint oltre il limite di lettura");
  }
  json doc;
  try {
    doc = parse_json({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
  } catch (const stop_error& e) {
    rethrow_as(e, error_category::integrity, "sched.checkpoint_malformed", "checkpoint non interpretabile");
  }
  if (doc.type() != json::kind::object || field(doc, "format", json::kind::string, name).as_string() != checkpoint_format) {
    malformed("formato non riconosciuto", name);
  }
  checkpoint_record record;
  record.node_key = hex_field(doc, "node_key", name);
  record.recipe_digest = hex_field(doc, "recipe_digest", name);
  record.backend = field(doc, "backend", json::kind::string, name).as_string();
  const std::int64_t task_count = field(doc, "task_count", json::kind::integer, name).as_integer();
  if (task_count < 0) malformed("task_count negativo", name);
  record.task_count = static_cast<std::uint64_t>(task_count);
  const json& completed = field(doc, "completed", json::kind::array, name);
  const json& digests = field(doc, "partial_digests", json::kind::array, name);
  if (completed.items().size() != digests.items().size()) malformed("liste di lunghezza diversa", name);
  for (std::size_t i = 0; i < completed.items().size(); ++i) {
    const json& index = completed.items()[i];
    const json& digest = digests.items()[i];
    if (index.type() != json::kind::integer || index.as_integer() < 0) malformed("indice non valido", name);
    const auto value = static_cast<std::uint64_t>(index.as_integer());
    if (value >= record.task_count || (!record.completed.empty() && value <= record.completed.back())) {
      malformed("indici fuori intervallo o non crescenti", name);
    }
    if (digest.type() != json::kind::string || !is_sha256_hex(digest.as_string())) malformed("digest non valido", name);
    record.completed.push_back(value);
    record.partial_digests.push_back(digest.as_string());
  }
  return record;
}

void remove_checkpoint(const fs::path& dir, const std::string& checkpoint_name) {
  const std::string name = file_name(checkpoint_name);
  const unique_fd dirfd = open_checkpoint_dir(dir);
  if (::unlinkat(dirfd.get(), name.c_str(), 0) != 0 && errno != ENOENT) {
    throw_errno(error_category::io, "sched.checkpoint_remove", "rimozione del checkpoint fallita", errno, {{"file", name}});
  }
  fsync_fd(dirfd.get(), "store.fsync", "checkpoint");
}

}  // namespace astro
