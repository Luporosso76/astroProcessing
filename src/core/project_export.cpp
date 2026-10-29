#include <dirent.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <map>
#include <set>


#include "astro/provenance.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;
namespace fs = std::filesystem;

namespace {

json digest_object(const content_digest& d) {
  json o = json::object();
  o.set("sha256", json::string(d.sha256));
  o.set("size_bytes", json::uinteger(d.size_bytes));
  return o;
}

bool valid_slot_name(const std::string& name) {
  if (name.empty() || name.size() > 200 || name.front() == '.') return false;
  return std::all_of(name.begin(), name.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
  });
}

struct export_item {
  std::string slot_name;
  artifact_record artifact;
  std::string rel_path;
};

fs::path canonical_target(const fs::path& dir) { return fs::weakly_canonical(fs::absolute(dir)); }

fs::path path_of_fd(int fd) { return fs::canonical("/proc/self/fd/" + std::to_string(fd)); }

bool is_inside(const fs::path& path, const fs::path& base) {
  const auto rel = path.lexically_relative(base);
  return !rel.empty() && *rel.begin() != "..";
}

// Il controllo opera sul fd del padre gia' aperto: la stessa directory serve poi a mkdirat.
void check_destination(int parent_fd, const std::string& name, const fs::path& dir, int root_fd,
                       const std::vector<input_record>& inputs) {
  const fs::path target = path_of_fd(parent_fd) / name;
  const fs::path project = path_of_fd(root_fd);
  if (target == project || is_inside(target, project)) {
    throw stop_error(error_category::security, "export.target_forbidden", "destinazione dentro il progetto",
                     {{"destinazione", dir.string()}});
  }
  for (const auto& in : inputs) {
    if (canonical_target(in.path) == target) {
      throw stop_error(error_category::security, "export.target_forbidden", "destinazione uguale a un input registrato",
                       {{"destinazione", dir.string()}});
    }
  }
  struct stat st {};
  if (::fstatat(parent_fd, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0) {
    for (const auto& in : inputs) {
      if (in.dev == static_cast<std::uint64_t>(st.st_dev) && in.ino == static_cast<std::uint64_t>(st.st_ino)) {
        throw stop_error(error_category::security, "fs.original_inode", "la destinazione e' lo stesso inode di un originale",
                         {{"destinazione", dir.string()}, {"originale", in.path}});
      }
    }
    throw stop_error(error_category::security, "export.target_exists", "la destinazione esiste gia'",
                     {{"destinazione", dir.string()}});
  }
  if (errno != ENOENT) throw_errno(error_category::io, "export.write", "stat della destinazione fallita", errno);
}

void add_ancestors(const sqlite_dag_store& store, const std::string& key, std::map<std::string, node_spec>& chain) {
  if (chain.count(key) != 0) return;
  const std::optional<node_spec> node = store.get_node(key);
  if (!node) throw stop_error(error_category::graph, "dag.missing_node", "nodo assente dallo store", {{"nodo", key}});
  chain.emplace(key, *node);
  for (const auto& in : node->inputs()) {
    if (!in.is_file()) add_ancestors(store, in.ref(), chain);
  }
}

json node_entry(const node_spec& spec) {
  json digests = json::array();
  for (const auto& in : spec.inputs()) {
    if (in.is_file()) digests.push(digest_object(in.file_digest()));
  }
  json o = json::object();
  o.set("node_key", json::string(spec.key()));
  o.set("node_spec", spec.to_json());
  o.set("input_digests", std::move(digests));
  o.set("correction_ledger", json::null());
  return o;
}

json versions_object() {
  const library_versions v = current_library_versions();
  json o = json::object();
  o.set("product", json::string(v.product));
  o.set("source_revision", json::string(v.source_revision));
  o.set("source_dirty", v.source_dirty ? json::boolean(*v.source_dirty) : json::null());
  o.set("cfitsio", json::string(v.cfitsio));
  o.set("sqlite", json::string(v.sqlite));
  o.set("openssl", json::string(v.openssl));
  return o;
}

// Report di export (ADR-04, estensione R4): ogni file e' la copia byte per byte dell'artefatto, nessun metadato perso.
std::string report_text(const std::vector<export_item>& items) {
  json files = json::array();
  for (const auto& it : items) {
    json f = json::object();
    f.set("rel_path", json::string(it.rel_path));
    f.set("size_bytes", json::uinteger(it.artifact.size_bytes));
    f.set("sha256", json::string(it.artifact.sha256));
    f.set("slot_name", json::string(it.slot_name));
    f.set("generation", json::uinteger(it.artifact.generation));
    f.set("node_key", json::string(it.artifact.node_key));
    f.set("kind", json::string(it.artifact.kind));
    f.set("representation", json::string("artifact_copy"));
    files.push(std::move(f));
  }
  json doc = json::object();
  doc.set("format", json::string(export_report_format));
  doc.set("files", std::move(files));
  doc.set("non_representable", json::array());
  doc.set("non_representable_reason",
          json::string("nessuna conversione di formato: ogni file è la copia byte per byte dell'artefatto, con maschere, "
                       "varianza e metadati dell'artefatto"));
  return canonical_json(doc);
}

std::string manifest_text(const std::vector<export_item>& items, const std::map<std::string, node_spec>& chain,
                          const content_digest& report) {
  json files = json::array();
  for (const auto& it : items) {
    json f = json::object();
    f.set("rel_path", json::string(it.rel_path));
    f.set("size_bytes", json::uinteger(it.artifact.size_bytes));
    f.set("sha256", json::string(it.artifact.sha256));
    f.set("slot_name", json::string(it.slot_name));
    f.set("generation", json::uinteger(it.artifact.generation));
    f.set("node_key", json::string(it.artifact.node_key));
    files.push(std::move(f));
  }
  json nodes = json::array();
  for (const auto& [key, spec] : chain) {
    (void)key;
    nodes.push(node_entry(spec));
  }
  json doc = json::object();
  doc.set("format", json::string(export_format));
  doc.set("versions", versions_object());
  doc.set("files", std::move(files));
  doc.set("nodes", std::move(nodes));
  json report_entry = json::object();
  report_entry.set("rel_path", json::string(export_report_name));
  report_entry.set("sha256", json::string(report.sha256));
  report_entry.set("size_bytes", json::uinteger(report.size_bytes));
  doc.set("report", std::move(report_entry));
  return canonical_json(doc);
}

void write_export_file(int dirfd, const std::string& prefix, const std::string& name,
                       std::span<const std::uint8_t> bytes, const fault_injector& injector) {
  atomic_options options;
  options.dirfd = dirfd;
  options.fault_prefix = prefix;
  options.conflict_code = "export.digest_name_conflict";
  options.final_name = name;
  options.injector = &injector;
  try {
    atomic_write(options, bytes, std::nullopt);
  } catch (const stop_error& e) {
    if (e.category() != error_category::io) throw;
    rethrow_as(e, error_category::io, "export.write", "scrittura dell'export fallita");
  }
}

// Copia in streaming l'artefatto nel bundle; il digest del record e' verificato mentre si scrive.
void copy_artifact(int artifacts_fd, int out_fd, const export_item& item, const inode_set& originals,
                   const fault_injector& injector) {
  unique_fd src(::openat(artifacts_fd, item.artifact.rel_path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
  if (!src) throw_errno(error_category::io, "store.artifact_missing", "apertura dell'artefatto fallita", errno, {{"rel_path", item.artifact.rel_path}});
  struct stat st {};
  if (::fstat(src.get(), &st) != 0) {
    throw_errno(error_category::io, "export.artifact_stat", "fstat dell'artefatto fallita", errno, {{"rel_path", item.artifact.rel_path}});
  }
  if (S_ISREG(st.st_mode)) check_managed_file(st, &originals, item.artifact.rel_path);
  if (!S_ISREG(st.st_mode) || static_cast<std::uint64_t>(st.st_size) != item.artifact.size_bytes) {
    throw stop_error(error_category::integrity, "export.artifact_corrupt", "il file dell'artefatto non corrisponde al record",
                     {{"rel_path", item.artifact.rel_path}});
  }
  atomic_options options;
  options.dirfd = out_fd;
  options.fault_prefix = "export.file";
  options.conflict_code = "export.digest_name_conflict";
  options.final_name = item.rel_path;
  options.injector = &injector;
  try {
    atomic_write_stream(options, fd_reader(src.get(), item.artifact.rel_path),
                        content_digest{item.artifact.sha256, item.artifact.size_bytes});
  } catch (const stop_error& e) {
    if (e.code() == "store.expected_digest_mismatch") {
      rethrow_as(e, error_category::integrity, "export.artifact_corrupt", "il file dell'artefatto non corrisponde al digest");
    }
    if (e.category() != error_category::io) throw;
    rethrow_as(e, error_category::io, "export.write", "scrittura dell'export fallita");
  }
}

}  // namespace

void sqlite_dag_store::export_slots(const fs::path& dir, const std::vector<std::string>& slot_names) {
  if (slot_names.empty()) throw stop_error(error_category::invalid_input, "export.no_slots", "nessuno slot da esportare");
  std::set<std::string> unique_names(slot_names.begin(), slot_names.end());
  for (const auto& name : unique_names) {
    if (!valid_slot_name(name)) {
      throw stop_error(error_category::invalid_input, "export.slot_name", "nome di slot non esportabile", {{"slot", name}});
    }
  }
  if (unique_names.size() != slot_names.size()) {
    throw stop_error(error_category::invalid_input, "export.slot_name", "slot ripetuto");
  }
  const std::string leaf = dir.filename().string();
  if (leaf.empty() || leaf == "." || leaf == "..") {
    throw stop_error(error_category::invalid_input, "export.target_invalid", "la destinazione deve avere un nome",
                     {{"destinazione", dir.string()}});
  }
  const fs::path parent = dir.has_parent_path() ? dir.parent_path() : fs::path(".");
  const unique_fd parent_fd(::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (!parent_fd) throw_errno(error_category::io, "export.write", "apertura della directory padre fallita", errno, {{"padre", parent.string()}});
  const std::vector<input_record> registered = inputs();
  inode_set originals;
  for (const auto& in : registered) originals.insert({in.dev, in.ino});
  check_destination(parent_fd.get(), leaf, dir, root_fd_, registered);
  const unique_fd artifacts = open_dir_nofollow(root_fd_, "artifacts", "store.symlink");
  std::vector<export_item> items;
  std::map<std::string, node_spec> chain;
  for (const auto& name : unique_names) {
    const slot_view view = read_slot(name);
    if (view.state != freshness::current) {
      throw stop_error(error_category::stale_generation, "export.not_current", "lo slot non e' current",
                       {{"slot", name}, {"stato", view.state == freshness::stale ? "stale" : "missing"}});
    }
    export_item item{name, *view.artifact, name + view.artifact->rel_path.substr(64)};
    if (item.rel_path == "manifest.json" || item.rel_path == "COMMIT" || item.rel_path == export_report_name) {
      throw stop_error(error_category::invalid_input, "export.slot_name", "il nome dello slot collide con un file riservato",
                       {{"slot", name}});
    }
    add_ancestors(*this, item.artifact.node_key, chain);
    items.push_back(std::move(item));
  }
  // Regola hardlink su ogni artefatto da copiare, prima di creare la destinazione.
  for (const auto& it : items) {
    struct stat st {};
    if (::fstatat(artifacts.get(), it.artifact.rel_path.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
      throw_errno(error_category::io, "store.artifact_missing", "stat dell'artefatto fallita", errno,
                  {{"rel_path", it.artifact.rel_path}});
    }
    if (S_ISREG(st.st_mode)) check_managed_file(st, &originals, it.artifact.rel_path);
  }
  const std::string report = report_text(items);
  const std::span<const std::uint8_t> report_bytes(reinterpret_cast<const std::uint8_t*>(report.data()), report.size());
  const std::string manifest = manifest_text(items, chain, digest_of(report_bytes));
  const std::span<const std::uint8_t> manifest_bytes(reinterpret_cast<const std::uint8_t*>(manifest.data()), manifest.size());
  const std::string manifest_digest = sha256_hex(manifest_bytes);

  try {
    check_fault("export.mkdir");
  } catch (const stop_error& e) {
    rethrow_as(e, error_category::io, "export.write", "mkdir della destinazione fallita");
  }
  if (::mkdirat(parent_fd.get(), leaf.c_str(), 0755) != 0) {
    if (errno == EEXIST) {
      throw stop_error(error_category::security, "export.target_exists", "la destinazione esiste gia'", {{"destinazione", dir.string()}});
    }
    throw_errno(error_category::io, "export.write", "mkdir della destinazione fallita", errno, {{"destinazione", dir.string()}});
  }
  const unique_fd out = open_dir_nofollow(parent_fd.get(), leaf.c_str(), "export.target_exists");
  for (const auto& it : items) copy_artifact(artifacts.get(), out.get(), it, originals, fault_);
  write_export_file(out.get(), "export.report", export_report_name, report_bytes, fault_);
  write_export_file(out.get(), "export.manifest", "manifest.json", manifest_bytes, fault_);
  try {
    write_export_file(out.get(), "export.commit", "COMMIT",
                      {reinterpret_cast<const std::uint8_t*>(manifest_digest.data()), manifest_digest.size()}, fault_);
  } catch (stop_error& e) {
    // Un export fallito non deve risultare completo: un guasto dopo il rename (fsync della directory) toglie COMMIT.
    if (::unlinkat(out.get(), "COMMIT", 0) != 0 && errno != ENOENT) e.add_context("commit_rimozione_errno", std::to_string(errno));
    throw;
  }
}

}  // namespace astro
