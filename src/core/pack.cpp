#include "astro/pack.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <limits>
#include <map>
#include <set>

#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "sqlite_internal.hpp"

namespace astro {

namespace {

using sqlint::throw_errno;
using sqlint::unique_fd;

constexpr std::uint64_t max_manifest_bytes = std::uint64_t{16} << 20;
constexpr std::uint64_t max_record_bytes = std::uint64_t{1} << 20;
constexpr std::int64_t max_healpix_order = 29;
constexpr std::int64_t any_int = std::numeric_limits<std::int64_t>::max();
constexpr const char* io_code = "pack.io";

[[noreturn]] void stop(error_category category, const char* code, const char* message,
                       stop_error::context_list context = {}) {
  throw stop_error(category, code, message, std::move(context));
}

[[noreturn]] void invalid(const std::string& where, const char* why) {
  stop(error_category::integrity, "pack.manifest_invalid", why, {{"campo", where}});
}

// Percorso relativo in forma normale: segmenti non vuoti, nessun `.` o `..`, nessun NUL.
bool normal_relative(const std::string& p) {
  if (p.find('\0') != std::string::npos) return false;
  const std::string framed = "/" + p + "/";
  return framed.find("//") == std::string::npos && framed.find("/./") == std::string::npos &&
         framed.find("/../") == std::string::npos;
}

const std::string top = "manifest", coverage_where = "coverage", builder_where = "builder";

// Su un valore che non e' un oggetto `find` non trova nulla: anche quel caso e' un campo assente.
const json& member(const json& o, const std::string& where, const char* key) {
  const json* v = o.find(key);
  if (v == nullptr) invalid(where + "." + key, "campo obbligatorio assente");
  return *v;
}

std::string text(const json& o, const std::string& where, const char* key, bool may_be_empty = false) {
  const json& v = member(o, where, key);
  if (v.type() != json::kind::string) invalid(where + "." + key, "stringa attesa");
  if (!may_be_empty && v.as_string().empty()) invalid(where + "." + key, "stringa vuota");
  return v.as_string();
}

std::int64_t integer(const json& o, const std::string& where, const char* key, std::int64_t lo, std::int64_t hi) {
  const json& v = member(o, where, key);
  if (v.type() != json::kind::integer || v.as_integer() < lo || v.as_integer() > hi) {
    invalid(where + "." + key, "intero fuori dal dominio");
  }
  return v.as_integer();
}

const std::vector<json>& list(const json& o, const std::string& where, const char* key) {
  const json& v = member(o, where, key);
  if (v.type() != json::kind::array || v.items().empty()) invalid(where + "." + key, "lista non vuota attesa");
  return v.items();
}

std::string at(const std::string& where, std::size_t i) { return where + "[" + std::to_string(i) + "]"; }

std::string hex_text(const json& v, const std::string& where) {
  if (v.type() != json::kind::string || !is_sha256_hex(v.as_string())) invalid(where, "SHA-256 esadecimale atteso");
  return v.as_string();
}

std::vector<pack_component> parse_components(const json& m) {
  std::vector<pack_component> out;
  std::set<std::string> names;
  const auto& items = list(m, top, "components");
  for (std::size_t i = 0; i < items.size(); ++i) {
    const std::string w = at("components", i);
    const json& c = items[i];
    pack_component pc{text(c, w, "name"), text(c, w, "source"), text(c, w, "release"), text(c, w, "license"),
                      text(c, w, "license_file"), text(c, w, "license_evidence"), text(c, w, "attribution", true),
                      {}, text(c, w, "frame"), text(c, w, "epoch"), text(c, w, "selection"),
                      static_cast<std::uint64_t>(integer(c, w, "count", 0, any_int))};
    const auto& bands = list(c, w, "bands");
    for (std::size_t k = 0; k < bands.size(); ++k) {
      const std::string wb = at(w + ".bands", k);
      const json& b = bands[k];
      pc.bands.push_back(pack_band{text(b, wb, "name"), text(b, wb, "system")});
    }
    if (pc.frame != "ICRS") invalid(w + ".frame", "frame diverso da ICRS");
    if (!names.insert(pc.name).second) invalid(w + ".name", "componente duplicato");
    out.push_back(std::move(pc));
  }
  return out;
}

// Colonne e bit di `flags` (§3.1): tipi ammessi, nullabili solo f32/f64, null_bit e flag_bits unici in 0-31.
void parse_columns(const json& m, pack_manifest& out) {
  std::set<std::string> components;
  for (const pack_component& c : out.components) components.insert(c.name);
  std::set<std::int64_t> bits;
  std::map<std::string, const pack_column*> by_name;
  const auto& cols = list(m, top, "columns");
  out.columns.reserve(cols.size());
  for (std::size_t i = 0; i < cols.size(); ++i) {
    const std::string w = at("columns", i);
    const json& c = cols[i];
    pack_column pc{text(c, w, "name"), text(c, w, "type"), text(c, w, "unit"), text(c, w, "component"), false, {}};
    const json& nullable = member(c, w, "nullable");
    if (nullable.type() != json::kind::boolean) invalid(w + ".nullable", "booleano atteso");
    pc.nullable = nullable.as_bool();
    if (pc.type != "f64" && pc.type != "f32" && pc.type != "u32" && pc.type != "u16") invalid(w + ".type", "tipo sconosciuto");
    if (components.count(pc.component) == 0) invalid(w + ".component", "componente non dichiarato");
    if (pc.nullable) {
      if (pc.type != "f32" && pc.type != "f64") invalid(w + ".nullable", "colonna intera nullabile");
      const std::int64_t bit = integer(c, w, "null_bit", 0, 31);
      if (!bits.insert(bit).second) invalid(w + ".null_bit", "bit gia' usato");
      pc.null_bit = static_cast<std::uint32_t>(bit);
    } else if (c.find("null_bit") != nullptr) {
      invalid(w + ".null_bit", "null_bit su colonna non nullabile");
    }
    out.columns.push_back(std::move(pc));
    if (!by_name.emplace(out.columns.back().name, &out.columns.back()).second) invalid(w + ".name", "colonna duplicata");
  }
  for (const auto& [name, type] : {std::pair{"ra_deg", "f64"}, std::pair{"dec_deg", "f64"}, std::pair{"flags", "u32"}}) {
    const auto it = by_name.find(name);
    if (it == by_name.end() || it->second->type != type || it->second->nullable) {
      invalid(std::string("columns.") + name, "colonna obbligatoria assente, di tipo diverso o nullabile");
    }
  }
  const auto& flags = list(m, top, "flag_bits");
  for (std::size_t i = 0; i < flags.size(); ++i) {
    const std::string w = at("flag_bits", i);
    const json& f = flags[i];
    const std::int64_t bit = integer(f, w, "index", 0, 31);
    if (!bits.insert(bit).second) invalid(w + ".index", "bit gia' usato");
    out.flag_bits.push_back(pack_flag_bit{static_cast<std::uint32_t>(bit), text(f, w, "meaning")});
  }
}

void parse_files(const json& m, pack_manifest& out) {
  std::set<std::string> paths;
  const auto& files = list(m, top, "files");
  for (std::size_t i = 0; i < files.size(); ++i) {
    const std::string w = at("files", i);
    const json& f = files[i];
    pack_file pf{text(f, w, "path"), static_cast<std::uint64_t>(integer(f, w, "size_bytes", 0, any_int)),
                 hex_text(member(f, w, "sha256"), w + ".sha256")};
    if (!normal_relative(pf.path)) invalid(w + ".path", "percorso assoluto, con `.` o `..`, o non normale");
    if (pf.path == "manifest.json") invalid(w + ".path", "manifest.json non va in files[]");
    if (!paths.insert(pf.path).second) invalid(w + ".path", "percorso duplicato");
    out.files.push_back(std::move(pf));
  }
  out.data_file = text(m, top, "data_file");
  out.index_file = text(m, top, "index_file");
  if (paths.count(out.data_file) == 0) invalid("data_file", "data_file non elencato in files[]");
  if (paths.count(out.index_file) == 0) invalid("index_file", "index_file non elencato in files[]");
}

pack_manifest parse_manifest(const json& m) {
  pack_manifest out;
  out.pack_format = 1;
  out.pack_id = text(m, top, "pack_id");
  out.pack_version = text(m, top, "pack_version");
  for (const json& p : list(m, top, "purpose")) {
    if (p.type() != json::kind::string || (p.as_string() != "astrometry" && p.as_string() != "photometry") ||
        std::count(out.purpose.begin(), out.purpose.end(), p.as_string()) != 0) {
      invalid("purpose", "scopo sconosciuto o ripetuto");
    }
    out.purpose.push_back(p.as_string());
  }
  out.components = parse_components(m);
  parse_columns(m, out);
  const json& cov = member(m, top, "coverage");
  out.coverage_order = static_cast<std::uint32_t>(integer(cov, "coverage", "order", 0, max_healpix_order));
  const std::int64_t cells = std::int64_t{12} << (2 * out.coverage_order);
  for (const json& c : list(cov, coverage_where, "cells")) {
    if (c.type() != json::kind::integer || c.as_integer() < 0 || c.as_integer() >= cells ||
        (!out.coverage_cells.empty() && static_cast<std::uint64_t>(c.as_integer()) <= out.coverage_cells.back())) {
      invalid("coverage.cells", "cella fuori dall'ordine o non crescente");
    }
    out.coverage_cells.push_back(static_cast<std::uint64_t>(c.as_integer()));
  }
  out.index_order = static_cast<std::uint32_t>(integer(m, top, "index_order", 0, max_healpix_order));
  if (out.index_order != out.coverage_order) invalid("index_order", "index_order diverso da coverage.order");
  parse_files(m, out);
  out.count = static_cast<std::uint64_t>(integer(m, top, "count", 0, any_int));
  const json& b = member(m, top, "builder");
  out.builder.name = text(b, "builder", "name");
  out.builder.version = text(b, "builder", "version");
  for (const json& in : list(b, builder_where, "inputs_sha256")) out.builder.inputs_sha256.push_back(hex_text(in, "builder.inputs_sha256"));
  return out;
}

// Dimensione prima dell'hash; hash completo dei byte letti; dimensione riletta a fine lettura.
void verify_file(int fd, const pack_file& f, std::vector<std::uint8_t>& buffer) {
  struct stat st {};
  if (::fstat(fd, &st) != 0) throw_errno(error_category::io, io_code, "fstat fallita", errno, {{"file", f.path}});
  if (!S_ISREG(st.st_mode)) stop(error_category::integrity, "pack.file_missing", "file elencato non regolare", {{"file", f.path}});
  const auto size_stop = [&] { stop(error_category::integrity, "pack.size_mismatch", "dimensione diversa dal manifest", {{"file", f.path}}); };
  if (static_cast<std::uint64_t>(st.st_size) != f.size_bytes) size_stop();
  sha256_stream hash;
  for (;;) {
    const ssize_t n = ::read(fd, buffer.data(), buffer.size());
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) throw_errno(error_category::io, io_code, "lettura fallita", errno, {{"file", f.path}});
    if (n == 0) break;
    hash.update({buffer.data(), static_cast<std::size_t>(n)});
  }
  const content_digest d = hash.finish();
  if (d.size_bytes != f.size_bytes) size_stop();
  if (d.sha256 != f.sha256) stop(error_category::integrity, "pack.hash_mismatch", "SHA-256 diverso dal manifest", {{"file", f.path}});
}

struct walk_state {
  std::map<std::string, const pack_file*> listed;
  std::set<std::string> verified;
  std::vector<std::uint8_t> buffer = std::vector<std::uint8_t>(sha256_chunk_bytes);
};

// Visita l'albero senza seguire link simbolici, in ordine di nome: ogni voce non directory, salvo manifest.json alla
// radice, deve essere elencata e regolare.
void walk(int dirfd, const std::string& prefix, walk_state& s) {
  std::vector<std::string> names = sqlint::read_directory(dirfd, io_code);
  std::sort(names.begin(), names.end());
  for (const std::string& name : names) {
    const std::string rel = prefix + name;
    if (rel == "manifest.json") continue;
    struct stat st {};
    if (::fstatat(dirfd, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
      throw_errno(error_category::io, io_code, "fstatat fallita", errno, {{"file", rel}});
    }
    const auto it = s.listed.find(rel);
    if (it != s.listed.end() && !S_ISREG(st.st_mode)) stop(error_category::integrity, "pack.file_missing", "file elencato non regolare", {{"file", rel}});
    if (S_ISDIR(st.st_mode)) {
      const unique_fd sub(::openat(dirfd, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
      if (!sub) throw_errno(error_category::io, io_code, "apertura della directory fallita", errno, {{"file", rel}});
      walk(sub.get(), rel + "/", s);
      continue;
    }
    if (it == s.listed.end()) stop(error_category::integrity, "pack.unlisted_file", "file non elencato nel manifest", {{"file", rel}});
    const unique_fd fd(::openat(dirfd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
    if (!fd) throw_errno(error_category::io, io_code, "apertura fallita", errno, {{"file", rel}});
    verify_file(fd.get(), *it->second, s.buffer);
    s.verified.insert(rel);
  }
}

[[noreturn]] void evidence_stop(const pack_component& c, const char* why) {
  stop(error_category::incompatible, "pack.license_evidence", why,
       {{"componente", c.name}, {"record", c.license_evidence}});
}

// Record `docs/datasets/PUB-*.md` sotto la radice delle evidenze, con una sola riga «Licenza SPDX: `<id>`» uguale a
// `license` (§3.6).
void verify_evidence(const pack_audit& audit, const pack_component& c) {
  const std::string prefix = "docs/datasets/";
  const std::string& rel = c.license_evidence;
  const std::string name = rel.rfind(prefix, 0) == 0 ? rel.substr(prefix.size()) : std::string();
  if (!normal_relative(rel) || name.find('/') != std::string::npos || name.size() <= 7 || name.rfind("PUB-", 0) != 0 ||
      name.compare(name.size() - 3, 3, ".md") != 0) {
    evidence_stop(c, "record fuori da docs/datasets/PUB-*.md");
  }
  if (audit.evidence_root.find('\0') != std::string::npos) evidence_stop(c, "radice delle evidenze con un NUL");
  const std::string path = audit.evidence_root + "/" + rel;
  const unique_fd fd(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
  if (!fd && (errno == ENOENT || errno == ENOTDIR || errno == ELOOP)) evidence_stop(c, "record di audit assente");
  if (!fd) throw_errno(error_category::io, io_code, "apertura del record fallita", errno, {{"file", path}});
  struct stat st {};
  if (::fstat(fd.get(), &st) != 0) throw_errno(error_category::io, io_code, "fstat fallita", errno, {{"file", path}});
  if (!S_ISREG(st.st_mode)) evidence_stop(c, "record di audit non regolare");
  if (static_cast<std::uint64_t>(st.st_size) > max_record_bytes) evidence_stop(c, "record di audit oltre il limite");
  const std::vector<std::uint8_t> bytes = sqlint::read_all(fd.get(), max_record_bytes, io_code, path);
  const std::string record(bytes.begin(), bytes.end());
  const std::string marker = "Licenza SPDX: `";
  const std::size_t found = record.find(marker);
  if (found == std::string::npos || record.find(marker, found + 1) != std::string::npos) {
    evidence_stop(c, "il record non ha una e una sola riga «Licenza SPDX»");
  }
  const std::size_t start = found + marker.size();
  const std::size_t end = record.find('`', start);
  if (end == std::string::npos || record.substr(start, end - start) != c.license) {
    evidence_stop(c, "licenza del record diversa da quella del manifest");
  }
}

// Licenze ammesse per un componente distribuito (§4 punto 3); CC-BY-4.0 solo con attribuzione.
void verify_licenses(const pack_manifest& m, const walk_state& files, const pack_audit& audit) {
  static const std::set<std::string> allowed{"LicenseRef-public-domain", "CC0-1.0", "CC-BY-4.0", "GPL-2.0-or-later",
                                             "GPL-3.0-or-later"};
  for (const pack_component& c : m.components) {
    if (allowed.count(c.license) == 0 || (c.license == "CC-BY-4.0" && c.attribution.empty())) {
      stop(error_category::incompatible, "pack.license_not_allowed", "licenza fuori dalla lista ammessa",
           {{"componente", c.name}, {"licenza", c.license}});
    }
    if (c.license_file.rfind("licenses/", 0) != 0 || files.listed.count(c.license_file) == 0) {
      stop(error_category::incompatible, "pack.license_file", "file di licenza assente da licenses/ e da files[]",
           {{"componente", c.name}, {"file", c.license_file}});
    }
    verify_evidence(audit, c);
  }
}

verified_pack verify(const pack_recipe& recipe, const pack_use& use, const pack_audit& audit) {
  // Un NUL troncherebbe il percorso in un prefisso diverso da quello della ricetta: nessuna directory ha quel nome.
  if (recipe.directory.find('\0') != std::string::npos) stop(error_category::metadata_missing, "pack.missing", "percorso del pack con un NUL");
  const unique_fd dir(::open(recipe.directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (!dir && (errno == ENOENT || errno == ENOTDIR)) stop(error_category::metadata_missing, "pack.missing", "directory del pack assente");
  if (!dir) throw_errno(error_category::io, io_code, "apertura della directory del pack fallita", errno);
  const unique_fd mf(::openat(dir.get(), "manifest.json", O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK | O_NOCTTY));
  if (!mf && (errno == ENOENT || errno == ELOOP)) stop(error_category::metadata_missing, "pack.missing", "manifest.json assente");
  if (!mf) throw_errno(error_category::io, io_code, "apertura di manifest.json fallita", errno);
  struct stat st {};
  if (::fstat(mf.get(), &st) != 0) throw_errno(error_category::io, io_code, "fstat fallita", errno);
  if (!S_ISREG(st.st_mode)) stop(error_category::metadata_missing, "pack.missing", "manifest.json non e' un file regolare");
  if (static_cast<std::uint64_t>(st.st_size) > max_manifest_bytes) invalid("manifest", "manifest oltre il limite di lettura");
  const std::vector<std::uint8_t> bytes = sqlint::read_all(mf.get(), max_manifest_bytes, io_code, "manifest.json");
  const std::string text_bytes(bytes.begin(), bytes.end());

  json doc;
  try {
    doc = parse_json(text_bytes);
  } catch (const stop_error& e) {
    sqlint::rethrow_as(e, error_category::integrity, "pack.manifest_invalid", "manifest non interpretabile come JSON");
  }
  if (canonical_json(doc) != text_bytes) invalid("manifest", "manifest non in forma canonica (RFC 8785)");
  const std::int64_t format = integer(doc, top, "pack_format", -any_int, any_int);
  if (format != 1) stop(error_category::unsupported, "pack.format", "pack_format non supportato", {{"pack_format", std::to_string(format)}});
  verified_pack out;
  out.manifest = parse_manifest(doc);
  out.pack_digest = sha256_hex(bytes);
  if (out.pack_digest != recipe.pack_digest) {
    stop(error_category::incompatible, "pack.digest_mismatch", "digest del manifest diverso dalla ricetta",
         {{"atteso", recipe.pack_digest}, {"trovato", out.pack_digest}});
  }
  if (out.manifest.pack_id != recipe.pack_id || out.manifest.pack_version != recipe.pack_version) {
    stop(error_category::incompatible, "pack.id_mismatch", "pack_id o pack_version diversi dalla ricetta",
         {{"pack_id", out.manifest.pack_id}, {"pack_version", out.manifest.pack_version}});
  }

  walk_state files;
  for (const pack_file& f : out.manifest.files) files.listed.emplace(f.path, &f);
  walk(dir.get(), "", files);
  for (const pack_file& f : out.manifest.files) {
    if (files.verified.count(f.path) == 0) stop(error_category::integrity, "pack.file_missing", "file elencato assente", {{"file", f.path}});
  }
  verify_licenses(out.manifest, files, audit);

  const auto& purpose = out.manifest.purpose;
  if (std::find(purpose.begin(), purpose.end(), use.purpose) == purpose.end()) {
    stop(error_category::incompatible, "pack.purpose", "scopo dell'operazione assente dal pack", {{"scopo", use.purpose}});
  }
  std::set<std::string> bands;
  for (const pack_component& c : out.manifest.components) {
    for (const pack_band& b : c.bands) bands.insert(b.name);
  }
  for (const std::string& b : use.bands) {
    if (bands.count(b) == 0) stop(error_category::incompatible, "pack.band_missing", "banda richiesta assente dal pack", {{"banda", b}});
  }
  out.provenance = json::object();
  out.provenance.set("pack_digest", json::string(out.pack_digest));
  out.provenance.set("pack_id", json::string(out.manifest.pack_id));
  out.provenance.set("pack_version", json::string(out.manifest.pack_version));
  return out;
}

}  // namespace

verified_pack verify_pack(const pack_recipe& recipe, const pack_use& use, const pack_audit& audit) {
  try {
    return verify(recipe, use, audit);
  } catch (stop_error& e) {
    e.add_context("pack", recipe.directory);
    throw;
  }
}

}  // namespace astro
