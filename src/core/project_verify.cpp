#include <dirent.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <map>
#include <algorithm>
#include <set>

#include "astro/sqlite_dag_store.hpp"
#include "sqlite_internal.hpp"

namespace astro {

using namespace sqlint;
namespace fs = std::filesystem;

namespace {

constexpr std::size_t max_rel_path = 4096;
constexpr std::size_t max_component = 255;

[[noreturn]] void bad_path(const std::string& rel, const char* why) {
  throw stop_error(error_category::security, "export.manifest_path", why, {{"rel_path", rel.substr(0, 200)}});
}

[[noreturn]] void malformed(const char* why, stop_error::context_list context = {}) {
  throw stop_error(error_category::invalid_input, "export.manifest_malformed", why, std::move(context));
}

[[noreturn]] void incomplete(const std::string& why, const std::string& what) {
  throw stop_error(error_category::integrity, "export.incomplete", "export incompleto o alterato",
                   {{"motivo", why}, {"dettaglio", what}});
}

void check_rel_path(const std::string& rel, std::set<std::string>& seen) {
  if (rel.empty() || rel.size() > max_rel_path) bad_path(rel, "rel_path vuoto o troppo lungo");
  if (rel.find('\0') != std::string::npos) bad_path(rel, "rel_path con NUL");
  if (rel.front() == '/' || rel.back() == '/') bad_path(rel, "rel_path con '/' iniziale o finale");
  std::size_t start = 0;
  for (;;) {
    const std::size_t slash = rel.find('/', start);
    const std::string part = rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
    if (part.empty() || part == "." || part == ".." || part.size() > max_component) {
      bad_path(rel, "componente di rel_path non valido");
    }
    if (slash == std::string::npos) break;
    start = slash + 1;
  }
  if (rel == "manifest.json" || rel == "COMMIT" || rel == export_report_name) bad_path(rel, "rel_path riservato");
  if (!seen.insert(rel).second) bad_path(rel, "rel_path duplicato");
}

const json& field(const json& o, const char* name, json::kind kind) {
  const json* f = o.find(name);
  if (f == nullptr || f->type() != kind) malformed("campo assente o di tipo errato", {{"campo", name}});
  return *f;
}

std::uint64_t unsigned_field(const json& o, const char* name) {
  const std::int64_t v = field(o, name, json::kind::integer).as_integer();
  if (v < 0) malformed("intero negativo", {{"campo", name}});
  return static_cast<std::uint64_t>(v);
}

const std::string& hex_field(const json& o, const char* name) {
  const std::string& v = field(o, name, json::kind::string).as_string();
  if (!is_sha256_hex(v)) malformed("digest non valido", {{"campo", name}});
  return v;
}

export_file_entry parse_file(const json& f) {
  if (f.type() != json::kind::object || f.keys().size() != 6) malformed("voce di file non valida");
  return export_file_entry{field(f, "rel_path", json::kind::string).as_string(), unsigned_field(f, "size_bytes"),
                           hex_field(f, "sha256"), field(f, "slot_name", json::kind::string).as_string(),
                           unsigned_field(f, "generation"), hex_field(f, "node_key")};
}

void check_versions(const json& v) {
  if (v.keys().size() != 6) malformed("versions deve avere 6 campi");
  for (const char* name : {"product", "source_revision", "cfitsio", "sqlite", "openssl"}) {
    if (field(v, name, json::kind::string).as_string().empty()) malformed("versione vuota", {{"campo", name}});
  }
  const json* dirty = v.find("source_dirty");
  if (dirty == nullptr || (dirty->type() != json::kind::boolean && dirty->type() != json::kind::null)) {
    malformed("source_dirty non valido");
  }
}

node_spec parse_node(const json& n) {
  if (n.type() != json::kind::object || n.keys().size() != 4) malformed("voce di nodo non valida");
  field(n, "correction_ledger", json::kind::null);
  node_spec spec = node_from_json(field(n, "node_spec", json::kind::object));
  if (spec.key() != hex_field(n, "node_key")) {
    throw stop_error(error_category::integrity, "export.node_key_mismatch", "la chiave del nodo non corrisponde ai suoi campi",
                     {{"dichiarata", n.find("node_key")->as_string()}, {"calcolata", spec.key()}});
  }
  const json& declared = field(n, "input_digests", json::kind::array);
  std::vector<content_digest> expected;
  for (const auto& in : spec.inputs()) {
    if (in.is_file()) expected.push_back(in.file_digest());
  }
  if (declared.items().size() != expected.size()) malformed("input_digests incoerente con il nodo");
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const json& d = declared.items()[i];
    if (d.type() != json::kind::object || d.keys().size() != 2 || hex_field(d, "sha256") != expected[i].sha256 ||
        unsigned_field(d, "size_bytes") != expected[i].size_bytes) {
      malformed("input_digests incoerente con il nodo");
    }
  }
  return spec;
}

export_manifest parse_manifest(const std::string& text) {
  const json doc = parse_json(text);
  if (doc.type() != json::kind::object || doc.keys().size() != 5) malformed("il manifest deve avere 5 campi");
  if (field(doc, "format", json::kind::string).as_string() != export_format) malformed("formato sconosciuto");
  check_versions(field(doc, "versions", json::kind::object));
  export_manifest out;
  const json& report = field(doc, "report", json::kind::object);
  if (report.keys().size() != 3 || field(report, "rel_path", json::kind::string).as_string() != export_report_name) {
    malformed("campo report non valido");
  }
  out.report = content_digest{hex_field(report, "sha256"), unsigned_field(report, "size_bytes")};
  std::set<std::string> seen;
  for (const auto& f : field(doc, "files", json::kind::array).items()) {
    out.files.push_back(parse_file(f));
    check_rel_path(out.files.back().rel_path, seen);
  }
  std::set<std::string> node_keys;
  for (const auto& n : field(doc, "nodes", json::kind::array).items()) {
    out.nodes.push_back(parse_node(n));
    if (!node_keys.insert(out.nodes.back().key()).second) {
      malformed("nodo ripetuto", {{"node_key", out.nodes.back().key()}});
    }
  }
  for (const auto& n : out.nodes) {
    for (const auto& in : n.inputs()) {
      if (!in.is_file() && node_keys.count(in.ref()) == 0) {
        malformed("antenato assente dal manifest", {{"node_key", n.key()}, {"padre", in.ref()}});
      }
    }
  }
  for (const auto& f : out.files) {
    if (node_keys.count(f.node_key) == 0) malformed("file con nodo assente dal manifest", {{"node_key", f.node_key}});
  }
  return out;
}

[[noreturn]] void report_mismatch(const std::string& why) {
  throw stop_error(error_category::integrity, "export.report_mismatch", "il report di export non corrisponde al manifest",
                   {{"motivo", why}});
}

// Report di export (ADR-04, estensione R4): JSON canonico, formato noto, stesso elenco del manifest campo per campo.
void check_report(const std::string& text, const export_manifest& manifest) {
  json doc;
  try {
    doc = parse_json(text);
  } catch (const stop_error& e) {
    report_mismatch("JSON non valido: " + e.code());
  }
  if (canonical_json(doc) != text) report_mismatch("report non canonico");
  const auto get = [](const json& o, const char* key, json::kind kind) -> const json& {
    const json* f = o.type() == json::kind::object ? o.find(key) : nullptr;
    if (f == nullptr || f->type() != kind) report_mismatch(std::string("campo assente o di tipo errato: ") + key);
    return *f;
  };
  if (doc.type() != json::kind::object || doc.keys().size() != 4) report_mismatch("il report deve avere 4 campi");
  if (get(doc, "format", json::kind::string).as_string() != export_report_format) report_mismatch("formato sconosciuto");
  if (!get(doc, "non_representable", json::kind::array).items().empty()) {
    report_mismatch("metadati non rappresentabili dichiarati per una copia byte per byte");
  }
  if (get(doc, "non_representable_reason", json::kind::string).as_string().empty()) report_mismatch("motivo vuoto");
  const std::vector<json>& files = get(doc, "files", json::kind::array).items();
  if (files.size() != manifest.files.size()) report_mismatch("numero di file diverso dal manifest");
  for (std::size_t i = 0; i < files.size(); ++i) {
    const json& f = files[i];
    const export_file_entry& m = manifest.files[i];
    if (f.type() != json::kind::object || f.keys().size() != 8) report_mismatch("voce di file non valida");
    const auto u64 = [&](const char* key) {
      const std::int64_t v = get(f, key, json::kind::integer).as_integer();
      if (v < 0) report_mismatch(std::string("intero negativo: ") + key);
      return static_cast<std::uint64_t>(v);
    };
    if (get(f, "rel_path", json::kind::string).as_string() != m.rel_path || u64("size_bytes") != m.size_bytes ||
        get(f, "sha256", json::kind::string).as_string() != m.sha256 ||
        get(f, "slot_name", json::kind::string).as_string() != m.slot_name || u64("generation") != m.generation ||
        get(f, "node_key", json::kind::string).as_string() != m.node_key) {
      report_mismatch("voce diversa dal manifest: " + m.rel_path);
    }
    if (get(f, "kind", json::kind::string).as_string().empty()) report_mismatch("kind vuoto: " + m.rel_path);
    if (get(f, "representation", json::kind::string).as_string() != "artifact_copy") {
      report_mismatch("rappresentazione diversa dalla copia dell'artefatto: " + m.rel_path);
    }
  }
}

// Apre `rel` sotto `dirfd`. -ENOENT/-ENOTDIR: assente. Symlink o uscita dalla directory: security.
int open_beneath(int dirfd, const std::string& rel) {
  open_how how{};
  how.flags = O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY;
  how.resolve = RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS | RESOLVE_NO_MAGICLINKS;
  const long fd = ::syscall(SYS_openat2, dirfd, rel.c_str(), &how, sizeof(how));
  if (fd >= 0) return static_cast<int>(fd);
  const int err = errno;
  if (err == ENOENT || err == ENOTDIR) return -ENOENT;
  if (err == ELOOP || err == EXDEV) bad_path(rel, "symlink o percorso fuori dalla directory");
  throw_errno(error_category::io, "export.open", "openat2 fallita", err, {{"rel_path", rel.substr(0, 200)}});
}

struct identity {
  dev_t dev = 0;
  ino_t ino = 0;
  off_t size = 0;
  timespec mtim{};
  timespec ctim{};
  bool operator==(const identity& o) const {
    return dev == o.dev && ino == o.ino && size == o.size && mtim.tv_sec == o.mtim.tv_sec && mtim.tv_nsec == o.mtim.tv_nsec &&
           ctim.tv_sec == o.ctim.tv_sec && ctim.tv_nsec == o.ctim.tv_nsec;
  }
};

identity identity_of(const struct stat& st) { return identity{st.st_dev, st.st_ino, st.st_size, st.st_mtim, st.st_ctim}; }

struct opened_regular {
  unique_fd fd;
  identity id;
};

std::optional<opened_regular> open_regular(int dirfd, const std::string& rel) {
  const int raw = open_beneath(dirfd, rel);
  if (raw < 0) return std::nullopt;
  opened_regular r{unique_fd(raw), {}};
  struct stat st {};
  if (::fstat(r.fd.get(), &st) != 0) throw_errno(error_category::io, "export.open", "fstat fallita", errno);
  if (!S_ISREG(st.st_mode)) bad_path(rel, "il file non e' regolare");
  r.id = identity_of(st);
  return r;
}

void changed(const std::string& rel) {
  throw stop_error(error_category::integrity, "export.changed_during_verify", "il file e' cambiato durante la verifica",
                   {{"rel_path", rel}});
}

void expect_unchanged(int dirfd, const std::string& rel, const opened_regular& before) {
  struct stat now {};
  if (::fstat(before.fd.get(), &now) != 0 || !(identity_of(now) == before.id)) changed(rel);
  const auto again = open_regular(dirfd, rel);
  if (!again || !(again->id == before.id)) changed(rel);
}

bool has_extra_entries(int dirfd, const std::string& prefix, const std::set<std::string>& files,
                       const std::set<std::string>& dirs, std::string& found, const fault_injector& hook) {
  for (const auto& n : read_directory(dirfd, "export.open", &hook)) {
    const std::string rel = prefix + n;
    struct stat st {};
    if (::fstatat(dirfd, n.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
      throw_errno(error_category::io, "export.open", "fstatat fallita", errno, {{"file", rel}});
    }
    if (S_ISREG(st.st_mode) && files.count(rel) != 0) continue;
    if (S_ISDIR(st.st_mode) && dirs.count(rel) != 0) {
      const unique_fd sub = open_dir_nofollow(dirfd, n.c_str(), "export.manifest_path");
      if (has_extra_entries(sub.get(), rel + "/", files, dirs, found, hook)) return true;
      continue;
    }
    found = rel;
    return true;
  }
  return false;
}

using listings = std::map<std::string, std::vector<std::string>>;

// Elenco ordinato di ogni directory del bundle che il manifest dichiara (radice inclusa).
void scan_listings(int dirfd, const std::string& prefix, const std::set<std::string>& dirs, listings& out,
                   const fault_injector& hook) {
  std::vector<std::string> names = read_directory(dirfd, "export.open", &hook);
  std::sort(names.begin(), names.end());
  for (const auto& n : names) {
    struct stat st {};
    if (::fstatat(dirfd, n.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(st.st_mode) && dirs.count(prefix + n) != 0) {
      const unique_fd sub = open_dir_nofollow(dirfd, n.c_str(), "export.manifest_path");
      scan_listings(sub.get(), prefix + n + "/", dirs, out, hook);
    }
  }
  out[prefix] = std::move(names);
}

}  // namespace

export_manifest verify_export(const fs::path& dir, const fault_injector& hook) {
  struct stat dst {};
  if (::lstat(dir.c_str(), &dst) != 0) {
    throw_errno(error_category::io, "export.open", "directory dell'export non apribile", errno, {{"directory", dir.string()}});
  }
  const unique_fd root = open_dir_nofollow(AT_FDCWD, dir.c_str(), "export.manifest_path");
  struct stat root_before {};
  if (::fstat(root.get(), &root_before) != 0) throw_errno(error_category::io, "export.open", "fstat fallita", errno);
  const auto manifest_file = open_regular(root.get(), "manifest.json");
  if (!manifest_file) incomplete("manifest.json assente", "manifest.json");
  if (static_cast<std::uint64_t>(manifest_file->id.size) > export_manifest_max_bytes) {
    throw stop_error(error_category::limit_exceeded, "export.manifest_size", "manifest.json oltre 64 MiB");
  }
  const std::vector<std::uint8_t> manifest_bytes =
      read_all(manifest_file->fd.get(), export_manifest_max_bytes, "export.manifest_size", "manifest.json");
  export_manifest manifest;
  try {
    manifest = parse_manifest(std::string(manifest_bytes.begin(), manifest_bytes.end()));
  } catch (const stop_error& e) {
    if (e.code().rfind("export.", 0) == 0) throw;
    malformed("manifest non valido", {{"causa", e.code()}, {"dettaglio", e.message()}});
  }
  manifest.manifest_sha256 = sha256_hex(manifest_bytes);

  std::vector<std::pair<const export_file_entry*, opened_regular>> opened;
  std::string missing;
  for (const auto& f : manifest.files) {
    auto r = open_regular(root.get(), f.rel_path);
    if (r) opened.emplace_back(&f, std::move(*r)); else if (missing.empty()) missing = f.rel_path;
  }
  const auto commit = open_regular(root.get(), "COMMIT");
  if (!commit) incomplete("COMMIT assente", "COMMIT");
  const std::vector<std::uint8_t> commit_bytes = read_all(commit->fd.get(), 128, "export.commit_read", "COMMIT");
  if (std::string(commit_bytes.begin(), commit_bytes.end()) != manifest.manifest_sha256) {
    incomplete("COMMIT non corrisponde a manifest.json", "COMMIT");
  }
  if (!missing.empty()) incomplete("file mancante", missing);
  const auto report_file = open_regular(root.get(), export_report_name);
  if (!report_file) incomplete("report di export assente", export_report_name);
  if (static_cast<std::uint64_t>(report_file->id.size) != manifest.report.size_bytes) {
    incomplete("dimensione diversa", export_report_name);
  }
  const std::vector<std::uint8_t> report_bytes =
      read_all(report_file->fd.get(), export_manifest_max_bytes, "export.manifest_size", export_report_name);
  if (!(digest_of(report_bytes) == manifest.report)) incomplete("digest diverso", export_report_name);
  check_report(std::string(report_bytes.begin(), report_bytes.end()), manifest);
  for (auto& [entry, file] : opened) {
    if (static_cast<std::uint64_t>(file.id.size) != entry->size_bytes) incomplete("dimensione diversa", entry->rel_path);
    const content_digest actual = stream_digest(file.fd.get(), entry->size_bytes, entry->rel_path);
    if (actual.size_bytes != entry->size_bytes || actual.sha256 != entry->sha256) incomplete("digest diverso", entry->rel_path);
  }
  std::set<std::string> files{"manifest.json", "COMMIT", export_report_name};
  std::set<std::string> dirs;
  for (const auto& f : manifest.files) {
    files.insert(f.rel_path);
    for (std::size_t slash = f.rel_path.find('/'); slash != std::string::npos; slash = f.rel_path.find('/', slash + 1)) {
      dirs.insert(f.rel_path.substr(0, slash));
    }
  }
  std::string extra;
  if (has_extra_entries(root.get(), "", files, dirs, extra, hook)) incomplete("file in piu'", extra);
  listings scanned;
  scan_listings(root.get(), "", dirs, scanned, hook);
  inject(&hook, "verify.after_read");
  {
    const unique_fd again = open_dir_nofollow(AT_FDCWD, dir.c_str(), "export.manifest_path");
    struct stat root_after {};
    if (::fstat(again.get(), &root_after) != 0) throw_errno(error_category::io, "export.open", "fstat fallita", errno);
    if (root_after.st_dev != root_before.st_dev || root_after.st_ino != root_before.st_ino) changed(".");
    listings rescanned;
    scan_listings(again.get(), "", dirs, rescanned, hook);
    if (rescanned != scanned) changed("elenco delle directory");
    std::string late_extra;
    if (has_extra_entries(again.get(), "", files, dirs, late_extra, hook)) incomplete("file in piu'", late_extra);
  }
  expect_unchanged(root.get(), "manifest.json", *manifest_file);
  expect_unchanged(root.get(), "COMMIT", *commit);
  expect_unchanged(root.get(), export_report_name, *report_file);
  for (const auto& [entry, file] : opened) expect_unchanged(root.get(), entry->rel_path, file);
  return manifest;
}

}  // namespace astro
