// U10, G-PACK parte 1 (plans/M3a.md, docs/contracts/ADR-08-decoder-pack.md §3.1-§3.4, §4, §6):
// T-M3A-PACK-01, 02, 03, 04, 05, 07, 09 e T-M3A-LIC-01.
//
// Il pack sintetico (seed fisso, componente `synthetic`, licenza `CC0-1.0`) lo scrive questo file con un generatore
// proprio: manifest canonico, SHA-256 (OpenSSL EVP) e file binari sono calcolati qui, non dal prodotto. Codici e
// categorie attesi sono quelli della tabella di §3.4, scritti a mano.
//
// Formato JSON del manifest (campi di §3.1, chiavi dei sotto-oggetti di §3.6):
//   pack_format: intero; pack_id, pack_version, data_file, index_file: stringhe; purpose: [stringa];
//   components[]: {name, source, release, license, license_file, license_evidence, attribution, frame, epoch,
//                  selection: stringhe; bands: [{name, system}]; count: intero};
//   columns[]: {name, type, unit, component: stringhe; nullable: booleano; null_bit: intero, solo se nullable};
//   flag_bits[]: {index: intero, meaning: stringa}; coverage: {order: intero, cells: [intero]}; index_order: intero;
//   files[]: {path, size_bytes, sha256}; count: intero; builder: {name, version, inputs_sha256: [stringa]}.
// Il record di audit di una licenza dichiara «Licenza SPDX: `<id>`», come i record reali di docs/datasets/.
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <netdb.h>
#include <openssl/evp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <set>

#include "astro/pack.hpp"
#include "astro/scheduler.hpp"
#include "store_test_support.hpp"

// ---------------------------------------------------------------------------------------------------------------------
// Osservatore di aperture di file e di chiamate di rete. Le funzioni libc sono interposte da questo eseguibile
// (libastrocore e' statica); registrano solo mentre una `probe::window` e' viva, quindi attorno alla sola chiamata
// di G-PACK. Le aperture dello store fatte dal runner (artifacts/) cadono fuori dalla finestra.
// ---------------------------------------------------------------------------------------------------------------------
namespace probe {

inline std::atomic<bool> active{false};
inline std::mutex mu;
inline std::vector<std::string> opens;
inline std::atomic<int> network_calls{0};
inline thread_local bool busy = false;

inline std::string resolve(int dirfd, const char* path) {
  namespace fs = std::filesystem;
  const fs::path p(path);
  if (p.is_absolute()) return p.lexically_normal().string();
  fs::path base;
  if (dirfd == AT_FDCWD) {
    base = fs::current_path();
  } else {
    char buf[4096];
    const ssize_t n = ::readlink(("/proc/self/fd/" + std::to_string(dirfd)).c_str(), buf, sizeof buf - 1);
    base = n > 0 ? fs::path(std::string(buf, static_cast<std::size_t>(n))) : fs::path("<dirfd-sconosciuto>");
  }
  return (base / p).lexically_normal().string();
}

inline void note(int dirfd, const char* path) {
  if (!active.load() || busy || path == nullptr) return;
  busy = true;
  const std::string r = resolve(dirfd, path);
  {
    std::lock_guard<std::mutex> lock(mu);
    opens.push_back(r);
  }
  busy = false;
}

inline void note_network() {
  if (active.load()) ++network_calls;
}

class window {
 public:
  window() {
    std::lock_guard<std::mutex> lock(mu);
    opens.clear();
    network_calls = 0;
    active = true;
  }
  ~window() { active = false; }
  window(const window&) = delete;
  window& operator=(const window&) = delete;
  std::vector<std::string> seen() const {
    std::lock_guard<std::mutex> lock(mu);
    return opens;
  }
  int network() const { return network_calls.load(); }
};

// O_TMPFILE include O_DIRECTORY: il modo c'e' solo con O_CREAT o con tutti i bit di O_TMPFILE.
inline bool takes_mode(int flags) { return (flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE; }

template <class F>
F next_symbol(const char* name) {
  void* p = ::dlsym(RTLD_NEXT, name);
  if (p == nullptr) {
    std::fprintf(stderr, "simbolo libc non trovato: %s\n", name);
    std::_Exit(70);
  }
  return reinterpret_cast<F>(p);
}

}  // namespace probe

extern "C" {

int open(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  probe::note(AT_FDCWD, path);
  static auto real = probe::next_symbol<int (*)(const char*, int, ...)>("open");
  return real(path, flags, mode);
}

int open64(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  probe::note(AT_FDCWD, path);
  static auto real = probe::next_symbol<int (*)(const char*, int, ...)>("open64");
  return real(path, flags, mode);
}

int openat(int dirfd, const char* path, int flags, ...) {
  mode_t mode = 0;
  if (probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  probe::note(dirfd, path);
  static auto real = probe::next_symbol<int (*)(int, const char*, int, ...)>("openat");
  return real(dirfd, path, flags, mode);
}

int openat64(int dirfd, const char* path, int flags, ...) {
  mode_t mode = 0;
  if (probe::takes_mode(flags)) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }
  probe::note(dirfd, path);
  static auto real = probe::next_symbol<int (*)(int, const char*, int, ...)>("openat64");
  return real(dirfd, path, flags, mode);
}

// Varianti con _FORTIFY_SOURCE (build release del core).
int __open_2(const char* path, int flags) {
  probe::note(AT_FDCWD, path);
  static auto real = probe::next_symbol<int (*)(const char*, int)>("__open_2");
  return real(path, flags);
}

int __open64_2(const char* path, int flags) {
  probe::note(AT_FDCWD, path);
  static auto real = probe::next_symbol<int (*)(const char*, int)>("__open64_2");
  return real(path, flags);
}

int __openat_2(int dirfd, const char* path, int flags) {
  probe::note(dirfd, path);
  static auto real = probe::next_symbol<int (*)(int, const char*, int)>("__openat_2");
  return real(dirfd, path, flags);
}

int __openat64_2(int dirfd, const char* path, int flags) {
  probe::note(dirfd, path);
  static auto real = probe::next_symbol<int (*)(int, const char*, int)>("__openat64_2");
  return real(dirfd, path, flags);
}

FILE* fopen(const char* path, const char* mode) {
  probe::note(AT_FDCWD, path);
  static auto real = probe::next_symbol<FILE* (*)(const char*, const char*)>("fopen");
  return real(path, mode);
}

FILE* fopen64(const char* path, const char* mode) {
  probe::note(AT_FDCWD, path);
  static auto real = probe::next_symbol<FILE* (*)(const char*, const char*)>("fopen64");
  return real(path, mode);
}

DIR* opendir(const char* path) {
  probe::note(AT_FDCWD, path);
  static auto real = probe::next_symbol<DIR* (*)(const char*)>("opendir");
  return real(path);
}

int socket(int domain, int type, int protocol) noexcept {
  probe::note_network();
  static auto real = probe::next_symbol<int (*)(int, int, int)>("socket");
  return real(domain, type, protocol);
}

int connect(int fd, const struct sockaddr* addr, socklen_t len) {
  probe::note_network();
  static auto real = probe::next_symbol<int (*)(int, const struct sockaddr*, socklen_t)>("connect");
  return real(fd, addr, len);
}

int getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res) {
  probe::note_network();
  static auto real = probe::next_symbol<int (*)(const char*, const char*, const struct addrinfo*, struct addrinfo**)>(
      "getaddrinfo");
  return real(node, service, hints, res);
}

}  // extern "C"

namespace {

using namespace store_test;

constexpr int fuzz_iterations = 5000;

// ---------------------------------------------------------------------------------------------------------------------
// Primitive del generatore: SHA-256, scrittura binaria little-endian, generatore pseudocasuale con seed fisso.
// ---------------------------------------------------------------------------------------------------------------------

std::string sha256_of(const std::string& bytes) {
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  if (EVP_Digest(bytes.data(), bytes.size(), md, &len, EVP_sha256(), nullptr) != 1) throw std::runtime_error("EVP_Digest");
  static const char* hex = "0123456789abcdef";
  std::string out;
  for (unsigned int i = 0; i < len; ++i) {
    out += hex[md[i] >> 4];
    out += hex[md[i] & 15];
  }
  return out;
}

void put_le(std::string& out, std::uint64_t v, int bytes) {
  for (int i = 0; i < bytes; ++i) out += static_cast<char>((v >> (8 * i)) & 0xFF);
}
void put_f64(std::string& out, double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, 8);
  put_le(out, u, 8);
}
void put_f32(std::string& out, float v) {
  std::uint32_t u;
  std::memcpy(&u, &v, 4);
  put_le(out, u, 4);
}

struct splitmix {
  std::uint64_t state;
  explicit splitmix(std::uint64_t seed) : state(seed) {}
  std::uint64_t next() {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  std::uint64_t below(std::uint64_t n) { return next() % n; }
  double unit() { return static_cast<double>(next() >> 11) / 9007199254740992.0; }
};

void write_bytes(const fs::path& path, const std::string& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  out.close();
  if (!out) throw std::runtime_error("scrittura fallita: " + path.string());
}

// ---------------------------------------------------------------------------------------------------------------------
// Albero JSON del test e serializzatore canonico (RFC 8785 ristretto a ASCII e interi, che e' tutto cio' che il test usa).
// ---------------------------------------------------------------------------------------------------------------------

struct jv {
  enum class kind { boolean, integer, string, array, object } type = kind::object;
  bool b = false;
  std::int64_t i = 0;
  std::string s;
  std::vector<jv> items;
  std::vector<std::string> keys;

  static jv boolean(bool v) {
    jv x;
    x.type = kind::boolean;
    x.b = v;
    return x;
  }
  static jv num(std::int64_t v) {
    jv x;
    x.type = kind::integer;
    x.i = v;
    return x;
  }
  static jv str(std::string v) {
    jv x;
    x.type = kind::string;
    x.s = std::move(v);
    return x;
  }
  static jv arr() {
    jv x;
    x.type = kind::array;
    return x;
  }
  static jv obj() { return jv(); }

  jv& set(const std::string& k, jv v) {
    for (std::size_t n = 0; n < keys.size(); ++n) {
      if (keys[n] == k) {
        items[n] = std::move(v);
        return *this;
      }
    }
    keys.push_back(k);
    items.push_back(std::move(v));
    return *this;
  }
  jv& push(jv v) {
    items.push_back(std::move(v));
    return *this;
  }
  jv& at(const std::string& k) {
    for (std::size_t n = 0; n < keys.size(); ++n) {
      if (keys[n] == k) return items[n];
    }
    throw std::runtime_error("chiave assente nel test: " + k);
  }
  void erase(const std::string& k) {
    for (std::size_t n = 0; n < keys.size(); ++n) {
      if (keys[n] == k) {
        keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(n));
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(n));
        return;
      }
    }
    throw std::runtime_error("chiave da rimuovere assente nel test: " + k);
  }
};

enum class style { canonical, reversed_keys, pretty };

void emit_string(const std::string& s, std::string& out) {
  out += '"';
  for (const char c : s) {
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
      out += buf;
    } else {
      out += c;
    }
  }
  out += '"';
}

void emit(const jv& v, style st, int depth, std::string& out) {
  const auto newline = [&](int d) {
    if (st != style::pretty) return;
    out += '\n';
    out.append(static_cast<std::size_t>(d) * 2, ' ');
  };
  switch (v.type) {
    case jv::kind::boolean: out += v.b ? "true" : "false"; return;
    case jv::kind::integer: out += std::to_string(v.i); return;
    case jv::kind::string: emit_string(v.s, out); return;
    case jv::kind::array: {
      out += '[';
      for (std::size_t n = 0; n < v.items.size(); ++n) {
        if (n != 0) out += ',';
        newline(depth + 1);
        emit(v.items[n], st, depth + 1, out);
      }
      if (!v.items.empty()) newline(depth);
      out += ']';
      return;
    }
    case jv::kind::object: {
      std::vector<std::size_t> order(v.keys.size());
      for (std::size_t n = 0; n < order.size(); ++n) order[n] = n;
      std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return v.keys[a] < v.keys[b]; });
      if (st == style::reversed_keys) std::reverse(order.begin(), order.end());
      out += '{';
      for (std::size_t n = 0; n < order.size(); ++n) {
        if (n != 0) out += ',';
        newline(depth + 1);
        emit_string(v.keys[order[n]], out);
        out += st == style::pretty ? ": " : ":";
        emit(v.items[order[n]], st, depth + 1, out);
      }
      if (!order.empty()) newline(depth);
      out += '}';
      return;
    }
  }
}

std::string serialize(const jv& v, style st = style::canonical) {
  std::string out;
  emit(v, st, 0, out);
  return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Generatore del pack sintetico.
// ---------------------------------------------------------------------------------------------------------------------

constexpr std::uint64_t pack_seed = 0x5EED0010ULL;
constexpr int pack_cells = 4;
constexpr int rows_per_cell = 3;
constexpr int pack_rows = pack_cells * rows_per_cell;
constexpr const char* builder_marker = "synthetic-pack-builder";

struct comp_spec {
  std::string name;
  std::string license = "CC0-1.0";
  std::string attribution;
  std::string license_file;
  std::string evidence;
  std::optional<std::string> evidence_license;
  std::optional<std::vector<std::string>> bands;
  bool write_evidence = true;
  bool write_license_file = true;
  bool list_license_file = true;
  std::uint64_t count = pack_rows;
};

struct pack_spec {
  std::vector<comp_spec> comps{comp_spec{}};
  std::vector<std::string> purpose{"astrometry"};
  std::vector<std::string> bands{"J", "H", "Ks"};
};

struct synth {
  fs::path dir;
  fs::path evidence_root;
  std::string id = "syn-pack";
  std::string version = "1.0.0";
  jv manifest;
  std::string manifest_bytes;
  std::string digest;

  pack_recipe recipe() const {
    pack_recipe r;
    r.pack_id = id;
    r.pack_version = version;
    r.pack_digest = digest;
    r.directory = dir.string();
    return r;
  }
  pack_audit audit() const {
    pack_audit a;
    a.evidence_root = evidence_root.string();
    return a;
  }
  void write_manifest() {
    manifest_bytes = serialize(manifest);
    write_bytes(dir / "manifest.json", manifest_bytes);
    digest = sha256_of(manifest_bytes);
  }
  // Aggiorna dimensione e SHA-256 della voce di files[] dopo che il file e' stato riscritto su disco.
  void refresh_entry(const std::string& rel) {
    const std::string bytes = read_text(dir / rel);
    for (jv& f : manifest.at("files").items) {
      if (f.at("path").s == rel) {
        f.set("size_bytes", jv::num(static_cast<std::int64_t>(bytes.size())));
        f.set("sha256", jv::str(sha256_of(bytes)));
      }
    }
    write_manifest();
  }
};

jv make_column(const std::string& name, const std::string& type, const std::string& unit, const std::string& comp,
               int null_bit) {
  jv c = jv::obj();
  c.set("name", jv::str(name)).set("type", jv::str(type)).set("unit", jv::str(unit)).set("component", jv::str(comp));
  c.set("nullable", jv::boolean(null_bit >= 0));
  if (null_bit >= 0) c.set("null_bit", jv::num(null_bit));
  return c;
}

std::string data_file_bytes() {
  splitmix g(pack_seed);
  std::string d;
  for (int cell = 0; cell < pack_cells; ++cell) {
    for (int r = 0; r < rows_per_cell; ++r) {
      const bool j_null = g.below(4) == 0;
      const bool h_null = g.below(4) == 0;
      const bool saturated = g.below(5) == 0;
      put_f64(d, 90.0 * g.unit());
      put_f64(d, 41.0 * g.unit());
      put_f32(d, j_null ? std::numeric_limits<float>::quiet_NaN() : static_cast<float>(10.0 + 4.0 * g.unit()));
      put_f32(d, h_null ? std::numeric_limits<float>::quiet_NaN() : static_cast<float>(10.0 + 4.0 * g.unit()));
      put_le(d, g.below(60), 2);
      put_le(d, (j_null ? 1u : 0u) | (h_null ? 2u : 0u) | (saturated ? 4u : 0u), 4);
    }
  }
  return d;
}

std::string index_file_bytes() {
  std::string d;
  for (int cell = 0; cell < pack_cells; ++cell) {
    put_le(d, static_cast<std::uint64_t>(cell), 8);
    put_le(d, static_cast<std::uint64_t>(cell * rows_per_cell), 8);
    put_le(d, rows_per_cell, 8);
  }
  return d;
}

std::string evidence_record(const std::string& license) {
  return "# PUB-SYN: record di audit sintetico del test\n\n- Stato: ACQUISITO. Licenza SPDX: `" + license + "`.\n";
}

std::string license_text(const std::string& name) { return "Testo di licenza sintetico del componente " + name + "\n"; }

synth make_pack(const fs::path& base, const pack_spec& spec = {}) {
  synth p;
  p.dir = base / "pack";
  p.evidence_root = base / "ev";
  fs::create_directories(p.dir);
  fs::create_directories(p.evidence_root);

  std::map<std::string, std::string> files;
  files["data.bin"] = data_file_bytes();
  files["index.bin"] = index_file_bytes();

  jv comps = jv::arr();
  std::uint64_t total = 0;
  for (std::size_t k = 0; k < spec.comps.size(); ++k) {
    comp_spec c = spec.comps[k];
    if (c.name.empty()) c.name = k == 0 ? "synthetic" : "synthetic" + std::to_string(k + 1);
    if (c.license_file.empty()) c.license_file = "licenses/" + c.name + ".txt";
    if (c.evidence.empty()) c.evidence = "docs/datasets/PUB-SYN-0" + std::to_string(k + 1) + ".md";
    if (c.write_license_file) {
      const std::string text = license_text(c.name);
      write_bytes(p.dir / c.license_file, text);
      if (c.list_license_file) files[c.license_file] = text;
    }
    if (c.write_evidence) write_bytes(p.evidence_root / c.evidence, evidence_record(c.evidence_license.value_or(c.license)));
    jv bands = jv::arr();
    for (const std::string& b : c.bands.value_or(spec.bands)) bands.push(jv::obj().set("name", jv::str(b)).set("system", jv::str("2MASS")));
    jv comp = jv::obj();
    comp.set("name", jv::str(c.name))
        .set("source", jv::str("Generatore sintetico del test, https://example.invalid/synthetic"))
        .set("release", jv::str("synthetic-1 2026-10-30"))
        .set("license", jv::str(c.license))
        .set("license_file", jv::str(c.license_file))
        .set("license_evidence", jv::str(c.evidence))
        .set("attribution", jv::str(c.attribution))
        .set("bands", std::move(bands))
        .set("frame", jv::str("ICRS"))
        .set("epoch", jv::str("J2000.0"))
        .set("selection", jv::str("nessuna"))
        .set("count", jv::num(static_cast<std::int64_t>(c.count)));
    comps.push(std::move(comp));
    total += c.count;
  }
  const std::string origin = comps.items[0].at("name").s;

  jv columns = jv::arr();
  columns.push(make_column("ra_deg", "f64", "deg", origin, -1));
  columns.push(make_column("dec_deg", "f64", "deg", origin, -1));
  columns.push(make_column("j_mag", "f32", "mag", origin, 0));
  columns.push(make_column("h_mag", "f32", "mag", origin, 1));
  columns.push(make_column("nobs", "u16", "count", origin, -1));
  columns.push(make_column("flags", "u32", "bitmask", origin, -1));

  jv flag_bits = jv::arr();
  flag_bits.push(jv::obj().set("index", jv::num(2)).set("meaning", jv::str("saturated")));

  jv purpose = jv::arr();
  for (const std::string& s : spec.purpose) purpose.push(jv::str(s));

  jv cells = jv::arr();
  for (int c = 0; c < pack_cells; ++c) cells.push(jv::num(c));

  jv m = jv::obj();
  m.set("pack_format", jv::num(1))
      .set("pack_id", jv::str(p.id))
      .set("pack_version", jv::str(p.version))
      .set("purpose", std::move(purpose))
      .set("components", std::move(comps))
      .set("columns", std::move(columns))
      .set("flag_bits", std::move(flag_bits))
      .set("data_file", jv::str("data.bin"))
      .set("index_file", jv::str("index.bin"))
      .set("coverage", jv::obj().set("order", jv::num(1)).set("cells", std::move(cells)))
      .set("index_order", jv::num(1))
      .set("count", jv::num(static_cast<std::int64_t>(total)))
      .set("builder", jv::obj()
                          .set("name", jv::str(builder_marker))
                          .set("version", jv::str("1"))
                          .set("inputs_sha256", jv::arr().push(jv::str(sha256_of("seed:" + std::to_string(pack_seed))))));
  jv flist = jv::arr();
  for (const auto& [rel, bytes] : files) {
    write_bytes(p.dir / rel, bytes);
    flist.push(jv::obj()
                   .set("path", jv::str(rel))
                   .set("size_bytes", jv::num(static_cast<std::int64_t>(bytes.size())))
                   .set("sha256", jv::str(sha256_of(bytes))));
  }
  m.set("files", std::move(flist));
  p.manifest = std::move(m);
  p.write_manifest();
  return p;
}

pack_use use_of(const std::string& purpose = "astrometry", std::vector<std::string> bands = {"J", "H", "Ks"}) {
  pack_use u;
  u.purpose = purpose;
  u.bands = std::move(bands);
  return u;
}

// Un preflight seguito da un'operazione fittizia che conta le proprie chiamate: negli arresti il contatore resta a zero.
struct gate {
  int operations = 0;

  verified_pack expect_ok(const pack_recipe& r, const pack_use& u, const pack_audit& a, const std::string& label = "") {
    SCOPED_TRACE(label);
    verified_pack v = verify_pack(r, u, a);
    ++operations;
    return v;
  }

  void expect_stop(const pack_recipe& r, const pack_use& u, const pack_audit& a, error_category category,
                   const std::string& code, const std::string& label = "") {
    SCOPED_TRACE(label);
    try {
      (void)verify_pack(r, u, a);
      ++operations;
      ADD_FAILURE() << "stop_error " << code << " atteso, nessuna eccezione";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), category) << e.what();
      EXPECT_EQ(e.code(), code) << e.what();
    }
  }
};

error_category expected_category(const std::string& code) {
  static const std::map<std::string, error_category> table{
      {"pack.missing", error_category::metadata_missing},
      {"pack.manifest_invalid", error_category::integrity},
      {"pack.format", error_category::unsupported},
      {"pack.digest_mismatch", error_category::incompatible},
      {"pack.id_mismatch", error_category::incompatible},
      {"pack.file_missing", error_category::integrity},
      {"pack.size_mismatch", error_category::integrity},
      {"pack.hash_mismatch", error_category::integrity},
      {"pack.unlisted_file", error_category::integrity},
      {"pack.count_mismatch", error_category::integrity},
      {"pack.index_invalid", error_category::integrity},
      {"pack.license_not_allowed", error_category::incompatible},
      {"pack.license_file", error_category::incompatible},
      {"pack.license_evidence", error_category::incompatible},
      {"pack.purpose", error_category::incompatible},
      {"pack.band_missing", error_category::incompatible},
      {"pack.out_of_coverage", error_category::incompatible},
      {"pack.row_invalid", error_category::integrity},
  };
  const auto it = table.find(code);
  if (it == table.end()) throw std::runtime_error("codice fuori dalla tabella di §3.4: " + code);
  return it->second;
}

// ---------------------------------------------------------------------------------------------------------------------
// Nodo del progetto che porta la ricetta del pack, e job del test composto da run_branches (ADR-08.b §3.4).
// ---------------------------------------------------------------------------------------------------------------------

json pack_params(const pack_recipe& r) {
  json p = json::object();
  p.set("pack_id", json::string(r.pack_id));
  p.set("pack_version", json::string(r.pack_version));
  p.set("pack_digest", json::string(r.pack_digest));
  p.set("pack_path", json::string(r.directory));
  return p;
}

recipe_entry pack_node(const pack_recipe& r) {
  return recipe_entry("test.pack_consumer", 1, 1, pack_params(r), {recipe_input{"file", fake_digest('a')}},
                      sample_type::f32, "cpu_scalar", std::nullopt);
}

std::string pack_node_key(const pack_recipe& r) {
  recipe_graph g;
  g.add("p", pack_node(r));
  return g.resolve().front().spec.key();
}

job_files_result one_file(sqlite_dag_store& store, const std::string& content) {
  const auto bytes = bytes_of(content);
  return job_files_result{store.write_files({artifact_content{bytes, "bin", "image", std::nullopt}}), {}};
}

std::vector<std::string> provenance_texts(const sqlite_dag_store& s, const std::string& node_key) {
  std::vector<std::string> texts;
  for (const artifact_record& a : s.artifacts_for(node_key)) {
    if (a.output_role == provenance_output_role) texts.push_back(read_text(s.artifact_path(a.rel_path)));
  }
  return texts;
}

bool inside(const std::string& path, const std::vector<std::string>& roots) {
  for (const std::string& root : roots) {
    if (path == root || path.rfind(root + "/", 0) == 0) return true;
  }
  return false;
}

std::vector<std::string> root_spellings(const fs::path& dir) {
  std::vector<std::string> roots;
  std::string lex = dir.lexically_normal().string();
  while (lex.size() > 1 && lex.back() == '/') lex.pop_back();
  roots.push_back(lex);
  std::string canon = fs::weakly_canonical(dir).string();
  while (canon.size() > 1 && canon.back() == '/') canon.pop_back();
  if (canon != lex) roots.push_back(canon);
  return roots;
}

struct branch_run {
  std::map<std::string, branch_result> by_slot;
  int exit_code = -1;
  int operations = 0;
  int downstream = 0;
  int independent = 0;
  std::vector<std::string> opens;
  int network = -1;
  std::map<std::string, std::string> keys;
  fs::path store_dir;
};

// Ramo del pack (sp), un discendente (sq) e un ramo indipendente (sr), composti da run_branches sullo store SQLite.
// Il lavoro di sp chiama G-PACK come primo passo, dentro la finestra di osservazione, e solo dopo l'operazione fittizia.
branch_run run_pack_branches(const std::string& name, const pack_recipe& r, const pack_audit& audit) {
  branch_run out;
  auto store = sqlite_dag_store::create(fresh_dir(name));
  out.store_dir = store->root();
  recipe_graph g;
  g.add("p", pack_node(r));
  g.add("q", entry("test.pack_downstream", 1, {recipe_input{"in", std::string("p")}}));
  g.add("r", entry("test.pack_independent", 2, {recipe_input{"file", fake_digest('b')}}));
  const auto nodes = g.resolve();
  out.keys = keys_of(nodes);
  store->apply_recipe(nodes, {{"sp", "p"}, {"sq", "q"}, {"sr", "r"}});
  sqlite_dag_store* raw = store.get();

  std::vector<branch_files_job> jobs;
  jobs.push_back(branch_files_job{"sp", [&](const job_record&) -> job_files_result {
                                    verified_pack v;
                                    {
                                      probe::window w;
                                      try {
                                        v = verify_pack(r, use_of(), audit);
                                      } catch (...) {
                                        out.opens = w.seen();
                                        out.network = w.network();
                                        throw;
                                      }
                                      out.opens = w.seen();
                                      out.network = w.network();
                                    }
                                    ++out.operations;
                                    job_files_result result = one_file(*raw, "uscita dell'operazione del pack");
                                    json details = json::object();
                                    details.set("pack", v.provenance);
                                    result.details = details;
                                    return result;
                                  }});
  jobs.push_back(branch_files_job{"sq", [&](const job_record&) {
                                    ++out.downstream;
                                    return one_file(*raw, "discendente");
                                  }});
  jobs.push_back(branch_files_job{"sr", [&](const job_record&) {
                                    ++out.independent;
                                    return one_file(*raw, "indipendente");
                                  }});
  testing::internal::CaptureStderr();
  const std::vector<branch_result> results = run_branches(*store, std::move(jobs));
  testing::internal::GetCapturedStderr();
  for (const branch_result& b : results) out.by_slot[b.slot_name] = b;
  out.exit_code = branches_exit_code(results);
  return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-01
// ---------------------------------------------------------------------------------------------------------------------

TEST(Pack, T_M3A_PACK_01_valid_synthetic_pack_passes_and_reports_manifest_identity) {
  const synth p = make_pack(fresh_dir("pk01"));
  gate g;
  const verified_pack v = g.expect_ok(p.recipe(), use_of(), p.audit());
  EXPECT_EQ(g.operations, 1);
  EXPECT_EQ(v.pack_digest, sha256_of(p.manifest_bytes));
  EXPECT_EQ(v.pack_digest, p.digest);
  EXPECT_EQ(v.manifest.pack_format, 1u);
  EXPECT_EQ(v.manifest.pack_id, "syn-pack");
  EXPECT_EQ(v.manifest.pack_version, "1.0.0");
  EXPECT_EQ(v.manifest.purpose, std::vector<std::string>{"astrometry"});
  EXPECT_EQ(v.manifest.count, static_cast<std::uint64_t>(pack_rows));
  ASSERT_EQ(v.manifest.components.size(), 1u);
  EXPECT_EQ(v.manifest.components[0].name, "synthetic");
  EXPECT_EQ(v.manifest.components[0].license, "CC0-1.0");
  EXPECT_EQ(v.manifest.components[0].count, static_cast<std::uint64_t>(pack_rows));
  EXPECT_EQ(v.manifest.data_file, "data.bin");
  EXPECT_EQ(v.manifest.index_file, "index.bin");
  std::set<std::string> listed;
  for (const pack_file& f : v.manifest.files) listed.insert(f.path);
  EXPECT_EQ(listed, (std::set<std::string>{"data.bin", "index.bin", "licenses/synthetic.txt"}));
  // Forma del digest: 64 caratteri esadecimali; il valore atteso e' lo SHA-256 (OpenSSL) dei byte canonici scritti dal generatore.
  EXPECT_EQ(p.digest.size(), 64u);
  EXPECT_EQ(p.manifest_bytes.find("\": "), std::string::npos);
  EXPECT_EQ(p.manifest_bytes.find(", \""), std::string::npos);
  EXPECT_EQ(p.manifest_bytes.find('\n'), std::string::npos);
  // La verifica e' ripetibile e non cambia l'identita'.
  EXPECT_EQ(g.expect_ok(p.recipe(), use_of(), p.audit()).pack_digest, p.digest);
}

TEST(Pack, T_M3A_PACK_01_each_allowed_license_and_two_components_pass) {
  gate g;
  const fs::path base = fresh_dir("pk01l");
  const std::vector<std::pair<std::string, std::string>> allowed{{"LicenseRef-public-domain", ""},
                                                                 {"CC0-1.0", ""},
                                                                 {"CC-BY-4.0", "Autore Sintetico, Titolo, 2026"},
                                                                 {"GPL-2.0-or-later", ""},
                                                                 {"GPL-3.0-or-later", ""}};
  int n = 0;
  for (const auto& [license, attribution] : allowed) {
    pack_spec spec;
    spec.comps[0].license = license;
    spec.comps[0].attribution = attribution;
    const synth p = make_pack(base / ("c" + std::to_string(n++)), spec);
    g.expect_ok(p.recipe(), use_of(), p.audit(), license);
  }
  pack_spec two;
  two.comps.resize(2);
  two.comps[0].count = 8;
  two.comps[1].count = 4;
  two.comps[1].license = "GPL-3.0-or-later";
  const synth p = make_pack(base / "due", two);
  const verified_pack v = g.expect_ok(p.recipe(), use_of(), p.audit(), "due componenti");
  EXPECT_EQ(v.manifest.components.size(), 2u);
  EXPECT_EQ(v.manifest.count, static_cast<std::uint64_t>(pack_rows));
}

TEST(Pack, T_M3A_PACK_01_pack_digest_is_in_the_node_key_and_in_the_provenance) {
  const synth p = make_pack(fresh_dir("pk01k"));
  const synth other = make_pack(fresh_dir("pk01k2"), [] {
    pack_spec s;
    s.bands = {"J", "H"};
    return s;
  }());
  ASSERT_NE(p.digest, other.digest);
  pack_recipe a = p.recipe();
  pack_recipe b = p.recipe();
  b.pack_digest = other.digest;
  EXPECT_EQ(pack_node_key(a), pack_node_key(p.recipe()));
  EXPECT_NE(pack_node_key(a), pack_node_key(b));

  const branch_run run = run_pack_branches("pk01r", p.recipe(), p.audit());
  EXPECT_EQ(run.by_slot.at("sp").state, branch_state::completed);
  EXPECT_EQ(run.keys.at("p"), pack_node_key(p.recipe()));
  const auto reopened = sqlite_dag_store::open(run.store_dir, open_intent::read_write);
  const auto texts = provenance_texts(*reopened, run.keys.at("p"));
  ASSERT_EQ(texts.size(), 1u);
  EXPECT_NE(texts[0].find("\"state\":\"completed\""), std::string::npos);
  // Il digest compare anche fra i parametri del nodo: la prova e' che il G-PACK lo riporti nei `details` del job.
  EXPECT_NE(texts[0].find("\"details\":{"), std::string::npos);
  EXPECT_NE(texts[0].find("\"pack\":{\"pack_digest\":\"" + p.digest + "\""), std::string::npos);
  EXPECT_NE(texts[0].find("\"pack_id\":\"syn-pack\",\"pack_version\":\"1.0.0\""), std::string::npos);
  EXPECT_EQ(texts[0].find(other.digest), std::string::npos);
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-02 (con il job del test di §3.4, «Prova in M3.a»)
// ---------------------------------------------------------------------------------------------------------------------

TEST(Pack, T_M3A_PACK_02_valid_pack_control_runs_the_operation_and_the_observer_sees_the_pack_files) {
  const synth p = make_pack(fresh_dir("pk02c"));
  const branch_run run = run_pack_branches("pk02cr", p.recipe(), p.audit());
  EXPECT_EQ(run.exit_code, 0);
  for (const char* slot : {"sp", "sq", "sr"}) EXPECT_EQ(run.by_slot.at(slot).state, branch_state::completed) << slot;
  EXPECT_EQ(run.operations, 1);
  EXPECT_EQ(run.downstream, 1);
  EXPECT_EQ(run.independent, 1);
  // Controllo positivo dell'osservatore: la verifica del pack apre almeno un file del pack.
  const std::vector<std::string> roots = root_spellings(p.dir);
  int in_pack = 0;
  for (const std::string& o : run.opens) in_pack += inside(o, roots) ? 1 : 0;
  EXPECT_GE(in_pack, 1);
  EXPECT_EQ(run.network, 0);
}

TEST(Pack, T_M3A_PACK_02_missing_pack_stops_only_the_dependent_branch) {
  const fs::path base = fresh_dir("pk02");
  // Un pack valido accanto e sopra il percorso della ricetta: G-PACK non deve cercarlo ne' leggerlo.
  const synth sibling = make_pack(base / "sibling");
  write_bytes(base / "manifest.json", read_text(sibling.dir / "manifest.json"));
  pack_recipe recipe = sibling.recipe();
  recipe.directory = (base / "absent" / "pack").string();
  const branch_run run = run_pack_branches("pk02r", recipe, sibling.audit());

  const branch_result& sp = run.by_slot.at("sp");
  EXPECT_EQ(sp.state, branch_state::failed);
  ASSERT_TRUE(sp.outcome.has_value());
  ASSERT_TRUE(sp.outcome->stop.has_value());
  EXPECT_EQ(sp.outcome->stop->category(), error_category::metadata_missing);
  EXPECT_EQ(sp.outcome->stop->code(), "pack.missing");
  EXPECT_FALSE(sp.outcome->provenance_not_persisted.has_value());
  EXPECT_EQ(run.by_slot.at("sq").state, branch_state::blocked);
  EXPECT_EQ(run.by_slot.at("sq").blocked_by_slot, "sp");
  EXPECT_EQ(run.by_slot.at("sr").state, branch_state::completed);
  EXPECT_EQ(run.exit_code, 2);
  EXPECT_EQ(run.operations, 0);
  EXPECT_EQ(run.downstream, 0);
  EXPECT_EQ(run.independent, 1);

  // Nessuna apertura fuori dal percorso della ricetta e nessuna chiamata di rete durante G-PACK.
  const std::vector<std::string> roots = root_spellings(recipe.directory);
  for (const std::string& o : run.opens) EXPECT_TRUE(inside(o, roots)) << "apertura fuori dal percorso della ricetta: " << o;
  EXPECT_EQ(run.network, 0);

  // Provenance dell'arresto persistita (OB-02); il progetto si riapre senza errori.
  const auto reopened = sqlite_dag_store::open(run.store_dir, open_intent::read_write);
  const auto failed = provenance_texts(*reopened, run.keys.at("p"));
  ASSERT_EQ(failed.size(), 1u);
  EXPECT_NE(failed[0].find("\"state\":\"failed\""), std::string::npos);
  EXPECT_NE(failed[0].find("pack.missing"), std::string::npos);
  EXPECT_TRUE(provenance_texts(*reopened, run.keys.at("q")).empty());
  EXPECT_EQ(provenance_texts(*reopened, run.keys.at("r")).size(), 1u);
  EXPECT_EQ(reopened->read_slot("sq").state, freshness::missing);
  EXPECT_EQ(reopened->read_slot("sr").state, freshness::current);
}

TEST(Pack, T_M3A_PACK_02_missing_directory_and_missing_manifest_have_the_same_stop) {
  const fs::path base = fresh_dir("pk02b");
  const synth p = make_pack(base / "valid");
  gate g;
  pack_recipe absent = p.recipe();
  absent.directory = (base / "none").string();
  g.expect_stop(absent, use_of(), p.audit(), error_category::metadata_missing, "pack.missing", "directory assente");
  fs::create_directories(base / "empty");
  pack_recipe empty_dir = p.recipe();
  empty_dir.directory = (base / "empty").string();
  g.expect_stop(empty_dir, use_of(), p.audit(), error_category::metadata_missing, "pack.missing", "manifest assente");
  fs::remove(p.dir / "manifest.json");
  g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::metadata_missing, "pack.missing", "manifest rimosso");
  EXPECT_EQ(g.operations, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-03
// ---------------------------------------------------------------------------------------------------------------------

std::string flip_first_byte_after(const std::string& bytes, std::size_t at) {
  std::string out = bytes;
  out[at] = static_cast<char>(out[at] ^ 0x01);
  return out;
}

TEST(Pack, T_M3A_PACK_03_corruption_stops_with_the_codes_of_the_table_and_never_runs_the_operation) {
  struct corruption {
    std::string label;
    std::function<void(synth&)> apply;
    error_category category;
    std::string code;
  };
  const auto flip = [](const char* rel) {
    return [rel](synth& p) { write_bytes(p.dir / rel, flip_first_byte_after(read_text(p.dir / rel), 5)); };
  };
  const auto append = [](const char* rel) {
    return [rel](synth& p) { write_bytes(p.dir / rel, read_text(p.dir / rel) + "x"); };
  };
  const auto truncate_last = [](const char* rel) {
    return [rel](synth& p) {
      std::string b = read_text(p.dir / rel);
      b.pop_back();
      write_bytes(p.dir / rel, b);
    };
  };
  const auto remove = [](const char* rel) {
    return [rel](synth& p) { fs::remove(p.dir / rel); };
  };
  const auto extra = [](const char* rel) {
    return [rel](synth& p) { write_bytes(p.dir / rel, "file non elencato"); };
  };
  const std::vector<corruption> cases{
      {"un byte cambiato nei dati", flip("data.bin"), error_category::integrity, "pack.hash_mismatch"},
      {"un byte cambiato nell'indice", flip("index.bin"), error_category::integrity, "pack.hash_mismatch"},
      {"un byte cambiato nella licenza", flip("licenses/synthetic.txt"), error_category::integrity, "pack.hash_mismatch"},
      {"un byte cambiato nel manifest",
       [](synth& p) {
         std::string b = read_text(p.dir / "manifest.json");
         const std::size_t at = b.find(builder_marker);
         ASSERT_NE(at, std::string::npos);
         b[at + std::strlen(builder_marker) - 1] = 'X';
         write_bytes(p.dir / "manifest.json", b);
       },
       error_category::incompatible, "pack.digest_mismatch"},
      {"dati mancanti", remove("data.bin"), error_category::integrity, "pack.file_missing"},
      {"indice mancante", remove("index.bin"), error_category::integrity, "pack.file_missing"},
      {"licenza mancante", remove("licenses/synthetic.txt"), error_category::integrity, "pack.file_missing"},
      {"file in piu' nella radice", extra("extra.txt"), error_category::integrity, "pack.unlisted_file"},
      {"file nascosto in piu'", extra(".hidden"), error_category::integrity, "pack.unlisted_file"},
      {"file in piu' in licenses/", extra("licenses/extra.txt"), error_category::integrity, "pack.unlisted_file"},
      {"file in piu' in una sottodirectory", extra("sub/dir/x.bin"), error_category::integrity, "pack.unlisted_file"},
      {"dati piu' lunghi di un byte", append("data.bin"), error_category::integrity, "pack.size_mismatch"},
      {"indice piu' corto di un byte", truncate_last("index.bin"), error_category::integrity, "pack.size_mismatch"},
      {"licenza piu' lunga di un byte", append("licenses/synthetic.txt"), error_category::integrity, "pack.size_mismatch"},
      {"dati azzerati in lunghezza", [](synth& p) { write_bytes(p.dir / "data.bin", ""); }, error_category::integrity,
       "pack.size_mismatch"},
  };
  gate g;
  int n = 0;
  const fs::path base = fresh_dir("pk03");
  for (const corruption& c : cases) {
    synth p = make_pack(base / ("c" + std::to_string(n++)));
    g.expect_ok(p.recipe(), use_of(), p.audit(), "controllo positivo: " + c.label);
    const int before = g.operations;
    c.apply(p);
    g.expect_stop(p.recipe(), use_of(), p.audit(), c.category, c.code, c.label);
    EXPECT_EQ(g.operations, before) << c.label;
  }
  EXPECT_EQ(g.operations, static_cast<int>(cases.size()));
}

TEST(Pack, T_M3A_PACK_03_hash_is_recomputed_at_every_preflight_without_cache) {
  synth p = make_pack(fresh_dir("pk03c"));
  gate g;
  g.expect_ok(p.recipe(), use_of(), p.audit());
  const fs::path data = p.dir / "data.bin";
  const auto mtime = fs::last_write_time(data);
  write_bytes(data, flip_first_byte_after(read_text(data), 7));
  fs::last_write_time(data, mtime);
  g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.hash_mismatch", "stessa dimensione e stesso mtime");
  EXPECT_EQ(g.operations, 1);
}

TEST(Pack, T_M3A_PACK_03_wrong_filesystem_types_stop_before_the_operation) {
  const fs::path base = fresh_dir("pk03t");
  gate g;
  {
    const synth p = make_pack(base / "recipe-is-a-file");
    pack_recipe r = p.recipe();
    r.directory = (p.dir / "manifest.json").string();
    g.expect_stop(r, use_of(), p.audit(), error_category::metadata_missing, "pack.missing", "il percorso della ricetta e' un file");
  }
  {
    const synth p = make_pack(base / "manifest-is-a-directory");
    fs::remove(p.dir / "manifest.json");
    fs::create_directories(p.dir / "manifest.json");
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::metadata_missing, "pack.missing", "manifest.json e' una directory");
  }
  for (const char* rel : {"data.bin", "index.bin", "licenses/synthetic.txt"}) {
    const synth p = make_pack(base / (std::string("listed-dir-") + std::to_string(std::strlen(rel))));
    fs::remove(p.dir / rel);
    fs::create_directories(p.dir / rel);
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.file_missing",
                  std::string("file elencato sostituito da una directory: ") + rel);
  }
  EXPECT_EQ(g.operations, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-04
// ---------------------------------------------------------------------------------------------------------------------

jv& column_of(jv& m, const std::string& name) {
  for (jv& c : m.at("columns").items) {
    if (c.at("name").s == name) return c;
  }
  throw std::runtime_error("colonna assente nel test: " + name);
}

struct mutation {
  std::string label;
  std::function<void(jv&)> apply;
};

std::vector<mutation> invalid_schema_mutations() {
  std::vector<mutation> m;
  for (const char* k : {"pack_format", "pack_id", "pack_version", "purpose", "components", "columns", "flag_bits",
                        "data_file", "index_file", "coverage", "index_order", "files", "count", "builder"}) {
    const std::string key = k;
    m.push_back({"tolto " + key, [key](jv& j) { j.erase(key); }});
  }
  for (const char* k : {"pack_id", "pack_version", "data_file", "index_file"}) {
    const std::string key = k;
    m.push_back({"vuoto " + key, [key](jv& j) { j.set(key, jv::str("")); }});
  }
  for (const char* k : {"purpose", "components", "columns", "files"}) {
    const std::string key = k;
    m.push_back({"vuoto " + key, [key](jv& j) { j.set(key, jv::arr()); }});
  }
  for (const char* k : {"coverage", "builder"}) {
    const std::string key = k;
    m.push_back({"vuoto " + key, [key](jv& j) { j.set(key, jv::obj()); }});
  }
  m.push_back({"scopo sconosciuto", [](jv& j) { j.set("purpose", jv::arr().push(jv::str("weather"))); }});
  m.push_back({"coverage senza order", [](jv& j) { j.at("coverage").erase("order"); }});
  m.push_back({"coverage senza cells", [](jv& j) { j.at("coverage").erase("cells"); }});
  m.push_back({"index_order diverso da coverage.order", [](jv& j) { j.set("index_order", jv::num(2)); }});
  m.push_back({"data_file non in files[]", [](jv& j) { j.set("data_file", jv::str("other.bin")); }});
  m.push_back({"index_file non in files[]", [](jv& j) { j.set("index_file", jv::str("other.bin")); }});
  m.push_back({"manifest.json elencato in files[]", [](jv& j) {
                 j.at("files").push(jv::obj()
                                        .set("path", jv::str("manifest.json"))
                                        .set("size_bytes", jv::num(10))
                                        .set("sha256", jv::str(std::string(64, 'a'))));
               }});
  m.push_back({"voce di files[] senza path", [](jv& j) { j.at("files").items[0].erase("path"); }});
  m.push_back({"voce di files[] con path vuoto", [](jv& j) { j.at("files").items[0].set("path", jv::str("")); }});
  m.push_back({"voce di files[] con sha256 vuoto", [](jv& j) { j.at("files").items[0].set("sha256", jv::str("")); }});
  m.push_back({"cells vuoto", [](jv& j) { j.at("coverage").set("cells", jv::arr()); }});
  for (const char* k : {"name", "version", "inputs_sha256"}) {
    const std::string key = k;
    m.push_back({"builder senza " + key, [key](jv& j) { j.at("builder").erase(key); }});
  }
  for (const char* k : {"name", "version"}) {
    const std::string key = k;
    m.push_back({"builder con " + key + " vuoto", [key](jv& j) { j.at("builder").set(key, jv::str("")); }});
  }
  m.push_back({"builder con inputs_sha256 vuoto", [](jv& j) { j.at("builder").set("inputs_sha256", jv::arr()); }});
  m.push_back({"banda con name vuoto", [](jv& j) { j.at("components").items[0].at("bands").items[0].set("name", jv::str("")); }});
  m.push_back({"banda con system vuoto", [](jv& j) { j.at("components").items[0].at("bands").items[0].set("system", jv::str("")); }});
  m.push_back({"flag_bits senza index", [](jv& j) { j.at("flag_bits").items[0].erase("index"); }});
  m.push_back({"flag_bits con meaning vuoto", [](jv& j) { j.at("flag_bits").items[0].set("meaning", jv::str("")); }});
  m.push_back({"voce di files[] senza sha256", [](jv& j) { j.at("files").items[0].erase("sha256"); }});
  m.push_back({"voce di files[] senza size_bytes", [](jv& j) { j.at("files").items[0].erase("size_bytes"); }});

  for (const char* k : {"name", "source", "release", "license", "license_file", "license_evidence", "attribution", "bands",
                        "frame", "epoch", "selection", "count"}) {
    const std::string key = k;
    m.push_back({"componente senza " + key, [key](jv& j) { j.at("components").items[0].erase(key); }});
  }
  for (const char* k : {"name", "source", "release", "license", "license_file", "license_evidence", "frame", "epoch", "selection"}) {
    const std::string key = k;
    m.push_back({"componente con " + key + " vuoto", [key](jv& j) { j.at("components").items[0].set(key, jv::str("")); }});
  }
  m.push_back({"componente con bands vuoto", [](jv& j) { j.at("components").items[0].set("bands", jv::arr()); }});
  m.push_back({"banda senza name", [](jv& j) { j.at("components").items[0].at("bands").items[0].erase("name"); }});
  m.push_back({"banda senza system", [](jv& j) { j.at("components").items[0].at("bands").items[0].erase("system"); }});

  for (const char* k : {"name", "type", "unit", "component", "nullable"}) {
    const std::string key = k;
    m.push_back({"colonna senza " + key, [key](jv& j) { column_of(j, "j_mag").erase(key); }});
  }
  for (const char* k : {"name", "type", "unit", "component"}) {
    const std::string key = k;
    m.push_back({"colonna con " + key + " vuoto", [key](jv& j) { column_of(j, "j_mag").set(key, jv::str("")); }});
  }
  m.push_back({"tipo di colonna sconosciuto", [](jv& j) { column_of(j, "j_mag").set("type", jv::str("f16")); }});
  m.push_back({"colonna nullable senza null_bit", [](jv& j) { column_of(j, "j_mag").erase("null_bit"); }});
  m.push_back({"colonna intera nullable", [](jv& j) {
                 column_of(j, "nobs").set("nullable", jv::boolean(true)).set("null_bit", jv::num(3));
               }});
  m.push_back({"colonna u32 nullable", [](jv& j) {
                 column_of(j, "flags").set("nullable", jv::boolean(true)).set("null_bit", jv::num(3));
               }});
  m.push_back({"null_bit duplicato", [](jv& j) { column_of(j, "h_mag").set("null_bit", jv::num(0)); }});
  m.push_back({"null_bit uguale a un indice di flag_bits", [](jv& j) { column_of(j, "h_mag").set("null_bit", jv::num(2)); }});
  m.push_back({"null_bit 32", [](jv& j) { column_of(j, "h_mag").set("null_bit", jv::num(32)); }});
  m.push_back({"null_bit negativo", [](jv& j) { column_of(j, "h_mag").set("null_bit", jv::num(-1)); }});
  m.push_back({"flag_bits duplicato", [](jv& j) {
                 j.at("flag_bits").push(jv::obj().set("index", jv::num(2)).set("meaning", jv::str("altro")));
               }});
  m.push_back({"flag_bits uguale a un null_bit", [](jv& j) {
                 j.at("flag_bits").push(jv::obj().set("index", jv::num(0)).set("meaning", jv::str("altro")));
               }});
  m.push_back({"flag_bits fuori da 0-31", [](jv& j) { j.at("flag_bits").items[0].set("index", jv::num(32)); }});
  m.push_back({"flag_bits negativo", [](jv& j) { j.at("flag_bits").items[0].set("index", jv::num(-1)); }});
  m.push_back({"flag_bits senza meaning", [](jv& j) { j.at("flag_bits").items[0].erase("meaning"); }});
  m.push_back({"manca ra_deg", [](jv& j) {
                 auto& cols = j.at("columns");
                 for (std::size_t n = 0; n < cols.items.size(); ++n) {
                   if (cols.items[n].at("name").s == "ra_deg") cols.items.erase(cols.items.begin() + static_cast<std::ptrdiff_t>(n));
                 }
               }});
  m.push_back({"manca dec_deg", [](jv& j) {
                 auto& cols = j.at("columns");
                 for (std::size_t n = 0; n < cols.items.size(); ++n) {
                   if (cols.items[n].at("name").s == "dec_deg") cols.items.erase(cols.items.begin() + static_cast<std::ptrdiff_t>(n));
                 }
               }});
  m.push_back({"manca flags", [](jv& j) {
                 auto& cols = j.at("columns");
                 for (std::size_t n = 0; n < cols.items.size(); ++n) {
                   if (cols.items[n].at("name").s == "flags") cols.items.erase(cols.items.begin() + static_cast<std::ptrdiff_t>(n));
                 }
               }});
  m.push_back({"ra_deg f32", [](jv& j) { column_of(j, "ra_deg").set("type", jv::str("f32")); }});
  m.push_back({"dec_deg f32", [](jv& j) { column_of(j, "dec_deg").set("type", jv::str("f32")); }});
  m.push_back({"flags u16", [](jv& j) { column_of(j, "flags").set("type", jv::str("u16")); }});
  m.push_back({"ra_deg nullable", [](jv& j) {
                 column_of(j, "ra_deg").set("nullable", jv::boolean(true)).set("null_bit", jv::num(5));
               }});
  return m;
}

TEST(Pack, T_M3A_PACK_04_schema_violations_stop_as_manifest_invalid) {
  synth p = make_pack(fresh_dir("pk04"));
  const jv baseline = p.manifest;
  gate g;
  g.expect_ok(p.recipe(), use_of(), p.audit(), "baseline");
  const std::vector<mutation> cases = invalid_schema_mutations();
  ASSERT_GE(cases.size(), 95u);
  for (const mutation& m : cases) {
    p.manifest = baseline;
    m.apply(p.manifest);
    p.write_manifest();
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.manifest_invalid", m.label);
  }
  EXPECT_EQ(g.operations, 1);
}

// Voce aggiunta a files[] che punta a un file reale con dimensione e SHA-256 corretti: l'unica violazione e' la forma del
// percorso (assoluto, oppure fuori dal pack con `..`).
TEST(Pack, T_M3A_PACK_04_listed_paths_outside_the_pack_stop_as_manifest_invalid) {
  const fs::path base = fresh_dir("pk04p");
  synth p = make_pack(base);
  const std::string outside = "contenuto fuori dal pack";
  write_bytes(base / "outside.bin", outside);
  const jv baseline = p.manifest;
  gate g;
  const auto with_extra_entry = [&](const std::string& path) {
    p.manifest = baseline;
    p.manifest.at("files").push(jv::obj()
                                    .set("path", jv::str(path))
                                    .set("size_bytes", jv::num(static_cast<std::int64_t>(outside.size())))
                                    .set("sha256", jv::str(sha256_of(outside))));
    p.write_manifest();
  };
  with_extra_entry((base / "outside.bin").string());
  g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.manifest_invalid", "percorso assoluto");
  with_extra_entry("../outside.bin");
  g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.manifest_invalid", "percorso con ..");
  with_extra_entry("licenses/../../outside.bin");
  g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.manifest_invalid", "percorso con .. annidato");
  p.manifest = baseline;
  p.write_manifest();
  g.expect_ok(p.recipe(), use_of(), p.audit(), "baseline");
  EXPECT_EQ(g.operations, 1);
}

TEST(Pack, T_M3A_PACK_04_an_empty_attribution_is_valid) {
  synth p = make_pack(fresh_dir("pk04a"));
  EXPECT_EQ(p.manifest.at("components").items[0].at("attribution").s, "");
  gate g;
  g.expect_ok(p.recipe(), use_of(), p.audit());
}

TEST(Pack, T_M3A_PACK_04_non_canonical_or_malformed_manifest_stops_as_manifest_invalid) {
  synth p = make_pack(fresh_dir("pk04n"));
  const std::string canonical = p.manifest_bytes;
  const std::vector<std::pair<std::string, std::string>> cases{
      {"indentato", serialize(p.manifest, style::pretty)},
      {"chiavi in ordine inverso", serialize(p.manifest, style::reversed_keys)},
      {"a capo finale", canonical + "\n"},
      {"spazio iniziale", " " + canonical},
      {"chiave duplicata", "{\"count\":12," + canonical.substr(1)},
      {"troncato a meta'", canonical.substr(0, canonical.size() / 2)},
      {"vuoto", ""},
      {"non JSON", "questo non e' JSON"},
      {"array al livello superiore", "[]"},
      {"UTF-8 non valido", std::string("{\"pack_id\":\"\xff\xfe\"}")},
  };
  gate g;
  for (const auto& [label, bytes] : cases) {
    write_bytes(p.dir / "manifest.json", bytes);
    pack_recipe r = p.recipe();
    r.pack_digest = sha256_of(bytes);
    g.expect_stop(r, use_of(), p.audit(), error_category::integrity, "pack.manifest_invalid", label);
  }
  EXPECT_EQ(g.operations, 0);
  write_bytes(p.dir / "manifest.json", canonical);
  g.expect_ok(p.recipe(), use_of(), p.audit(), "ripristinato");
}

TEST(Pack, T_M3A_PACK_04_unknown_pack_format_stops_as_unsupported) {
  synth p = make_pack(fresh_dir("pk04f"));
  gate g;
  for (const std::int64_t version : {2, 99}) {
    p.manifest.set("pack_format", jv::num(version));
    p.write_manifest();
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::unsupported, "pack.format", std::to_string(version));
  }
  EXPECT_EQ(g.operations, 0);
}

TEST(Pack, T_M3A_PACK_04_recipe_digest_id_and_version_must_equal_the_manifest) {
  const synth p = make_pack(fresh_dir("pk04d"));
  gate g;
  pack_recipe wrong_digest = p.recipe();
  wrong_digest.pack_digest[63] = wrong_digest.pack_digest[63] == '0' ? '1' : '0';
  g.expect_stop(wrong_digest, use_of(), p.audit(), error_category::incompatible, "pack.digest_mismatch", "digest diverso");
  pack_recipe digest_of_other = p.recipe();
  digest_of_other.pack_digest = sha256_of("altro manifest");
  g.expect_stop(digest_of_other, use_of(), p.audit(), error_category::incompatible, "pack.digest_mismatch", "digest di un altro manifest");
  pack_recipe wrong_id = p.recipe();
  wrong_id.pack_id = "altro-pack";
  g.expect_stop(wrong_id, use_of(), p.audit(), error_category::incompatible, "pack.id_mismatch", "pack_id diverso");
  pack_recipe wrong_version = p.recipe();
  wrong_version.pack_version = "1.0.1";
  g.expect_stop(wrong_version, use_of(), p.audit(), error_category::incompatible, "pack.id_mismatch", "pack_version diverso");
  EXPECT_EQ(g.operations, 0);
  g.expect_ok(p.recipe(), use_of(), p.audit());
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-05 e T-M3A-LIC-01
// ---------------------------------------------------------------------------------------------------------------------

TEST(Pack, T_M3A_PACK_05_licenses_outside_the_list_stop_as_not_allowed) {
  gate g;
  int n = 0;
  const fs::path base = fresh_dir("pk05");
  const std::vector<std::pair<std::string, std::string>> rejected{
      {"CC-BY-NC-4.0", "Autore"}, {"CC-BY-ND-4.0", "Autore"}, {"CC-BY-SA-4.0", "Autore"}, {"CC-BY-NC-SA-4.0", "Autore"},
      {"MIT", ""},               {"Apache-2.0", ""},         {"GPL-3.0-only", ""},       {"GPL-2.0-only", ""},
      {"LicenseRef-proprietary", ""}, {"CC-BY-4.0", ""},
  };
  for (const auto& [license, attribution] : rejected) {
    pack_spec spec;
    spec.comps[0].license = license;
    spec.comps[0].attribution = attribution;
    const synth p = make_pack(base / ("c" + std::to_string(n++)), spec);
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::incompatible, "pack.license_not_allowed",
                  license + (attribution.empty() ? " senza attribuzione" : " con attribuzione"));
  }
  pack_spec second;
  second.comps.resize(2);
  second.comps[0].count = 8;
  second.comps[1].count = 4;
  second.comps[1].license = "CC-BY-NC-4.0";
  const synth p = make_pack(base / "secondo", second);
  g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::incompatible, "pack.license_not_allowed", "secondo componente");
  EXPECT_EQ(g.operations, 0);
}

TEST(Pack, T_M3A_PACK_05_missing_license_file_stops_as_license_file) {
  gate g;
  const fs::path base = fresh_dir("pk05f");
  pack_spec absent;
  absent.comps[0].write_license_file = false;
  const synth a = make_pack(base / "assente", absent);
  g.expect_stop(a.recipe(), use_of(), a.audit(), error_category::incompatible, "pack.license_file", "license_file non scritto e non elencato");

  for (const char* outside : {"LICENSE.txt", "extra/licenses/synthetic.txt"}) {
    pack_spec elsewhere;
    elsewhere.comps[0].license_file = outside;
    const synth e = make_pack(base / (std::string("fuori-") + std::to_string(std::strlen(outside))), elsewhere);
    EXPECT_TRUE(fs::exists(e.dir / outside));
    g.expect_stop(e.recipe(), use_of(), e.audit(), error_category::incompatible, "pack.license_file",
                  std::string("license_file fuori da licenses/: ") + outside);
  }

  pack_spec second;
  second.comps.resize(2);
  second.comps[0].count = 8;
  second.comps[1].count = 4;
  second.comps[1].write_license_file = false;
  const synth s = make_pack(base / "secondo", second);
  g.expect_stop(s.recipe(), use_of(), s.audit(), error_category::incompatible, "pack.license_file", "secondo componente");
  EXPECT_EQ(g.operations, 0);
}

TEST(Pack, T_M3A_LIC_01_evidence_record_must_exist_and_carry_the_manifest_license) {
  gate g;
  const fs::path base = fresh_dir("lic01");
  for (const std::string license : {"CC0-1.0", "LicenseRef-public-domain", "GPL-3.0-or-later"}) {
    pack_spec spec;
    spec.comps[0].license = license;
    const synth ok = make_pack(base / ("ok-" + license), spec);
    g.expect_ok(ok.recipe(), use_of(), ok.audit(), "record coerente " + license);
  }

  pack_spec missing;
  missing.comps[0].write_evidence = false;
  const synth m = make_pack(base / "assente", missing);
  g.expect_stop(m.recipe(), use_of(), m.audit(), error_category::incompatible, "pack.license_evidence", "record assente");

  pack_spec different;
  different.comps[0].evidence_license = "CC-BY-4.0";
  const synth d = make_pack(base / "diversa", different);
  g.expect_stop(d.recipe(), use_of(), d.audit(), error_category::incompatible, "pack.license_evidence", "licenza del record diversa");

  pack_spec outside;
  outside.comps[0].evidence = "other/PUB-SYN-01.md";
  const synth o = make_pack(base / "fuori", outside);
  ASSERT_TRUE(fs::exists(o.evidence_root / "other/PUB-SYN-01.md"));
  g.expect_stop(o.recipe(), use_of(), o.audit(), error_category::incompatible, "pack.license_evidence", "record fuori da docs/datasets/");

  {
    pack_spec traversal;
    traversal.comps[0].evidence = "docs/datasets/../../other/PUB-SYN-01.md";
    traversal.comps[0].write_evidence = false;
    const synth t = make_pack(base / "traversal", traversal);
    write_bytes(t.evidence_root / "other/PUB-SYN-01.md", evidence_record("CC0-1.0"));
    fs::create_directories(t.evidence_root / "docs/datasets");
    g.expect_stop(t.recipe(), use_of(), t.audit(), error_category::incompatible, "pack.license_evidence",
                  "docs/datasets/ seguito da .. verso un record valido");
  }

  pack_spec second;
  second.comps.resize(2);
  second.comps[0].count = 8;
  second.comps[1].count = 4;
  second.comps[1].write_evidence = false;
  const synth s = make_pack(base / "secondo", second);
  g.expect_stop(s.recipe(), use_of(), s.audit(), error_category::incompatible, "pack.license_evidence", "secondo componente senza record");
  EXPECT_EQ(g.operations, 3);
}

TEST(Pack, T_M3A_LIC_01_real_audit_records_of_the_repository_are_read) {
  const std::string root = ASTRO_SOURCE_DIR;
  gate g;
  const fs::path base = fresh_dir("lic01r");
  const auto with_real_record = [&](const std::string& name, const std::string& license, const std::string& attribution,
                                    const std::string& record) {
    pack_spec spec;
    spec.comps[0].license = license;
    spec.comps[0].attribution = attribution;
    spec.comps[0].evidence = record;
    spec.comps[0].write_evidence = false;
    synth p = make_pack(base / name, spec);
    p.evidence_root = root;
    return p;
  };
  ASSERT_TRUE(fs::exists(root + "/docs/datasets/PUB-RAW-01.md"));
  ASSERT_TRUE(fs::exists(root + "/docs/datasets/PUB-LFC-01.md"));
  const synth cc0 = with_real_record("cc0", "CC0-1.0", "", "docs/datasets/PUB-RAW-01.md");
  g.expect_ok(cc0.recipe(), use_of(), cc0.audit(), "PUB-RAW-01 dichiara CC0-1.0");
  const synth by = with_real_record("by", "CC-BY-4.0", "Craig et al.", "docs/datasets/PUB-LFC-01.md");
  g.expect_ok(by.recipe(), use_of(), by.audit(), "PUB-LFC-01 dichiara CC-BY-4.0");
  const synth swapped = with_real_record("scambio", "CC0-1.0", "", "docs/datasets/PUB-LFC-01.md");
  g.expect_stop(swapped.recipe(), use_of(), swapped.audit(), error_category::incompatible, "pack.license_evidence",
                "CC0-1.0 dichiarata, record CC-BY-4.0");
  const synth swapped2 = with_real_record("scambio2", "CC-BY-4.0", "Autore", "docs/datasets/PUB-RAW-01.md");
  g.expect_stop(swapped2.recipe(), use_of(), swapped2.audit(), error_category::incompatible, "pack.license_evidence",
                "CC-BY-4.0 dichiarata, record CC0-1.0");
  const synth absent = with_real_record("assente", "CC0-1.0", "", "docs/datasets/PUB-NON-ESISTE-99.md");
  g.expect_stop(absent.recipe(), use_of(), absent.audit(), error_category::incompatible, "pack.license_evidence", "record inesistente");
  EXPECT_EQ(g.operations, 2);
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-07
// ---------------------------------------------------------------------------------------------------------------------

TEST(Pack, T_M3A_PACK_07_purpose_and_bands_are_checked_against_the_operation) {
  gate g;
  const synth astro_only = make_pack(fresh_dir("pk07"));
  g.expect_ok(astro_only.recipe(), use_of("astrometry"), astro_only.audit(), "astrometria su pack astrometry");
  g.expect_stop(astro_only.recipe(), use_of("photometry"), astro_only.audit(), error_category::incompatible, "pack.purpose",
                "photometry su pack astrometry");

  pack_spec photo;
  photo.purpose = {"photometry"};
  const synth photo_only = make_pack(fresh_dir("pk07p"), photo);
  g.expect_stop(photo_only.recipe(), use_of("astrometry"), photo_only.audit(), error_category::incompatible, "pack.purpose",
                "astrometry su pack photometry");
  g.expect_ok(photo_only.recipe(), use_of("photometry"), photo_only.audit(), "photometry su pack photometry");

  pack_spec both;
  both.purpose = {"astrometry", "photometry"};
  const synth both_pack = make_pack(fresh_dir("pk07b"), both);
  g.expect_ok(both_pack.recipe(), use_of("astrometry"), both_pack.audit(), "astrometry su pack doppio");
  g.expect_ok(both_pack.recipe(), use_of("photometry"), both_pack.audit(), "photometry su pack doppio");

  g.expect_ok(astro_only.recipe(), use_of("astrometry", {"J"}), astro_only.audit(), "una banda presente");
  g.expect_ok(astro_only.recipe(), use_of("astrometry", {}), astro_only.audit(), "nessuna banda richiesta");
  g.expect_stop(astro_only.recipe(), use_of("astrometry", {"J", "r"}), astro_only.audit(), error_category::incompatible,
                "pack.band_missing", "banda r assente");
  g.expect_stop(astro_only.recipe(), use_of("astrometry", {"u"}), astro_only.audit(), error_category::incompatible,
                "pack.band_missing", "banda u assente");
  EXPECT_EQ(g.operations, 6);

  pack_spec split;
  split.comps.resize(2);
  split.comps[0].count = 8;
  split.comps[1].count = 4;
  split.comps[0].bands = std::vector<std::string>{"J", "H"};
  split.comps[1].bands = std::vector<std::string>{"Ks"};
  const synth two = make_pack(fresh_dir("pk07c"), split);
  g.expect_ok(two.recipe(), use_of("astrometry", {"Ks"}), two.audit(), "banda presente solo nel secondo componente");
  g.expect_ok(two.recipe(), use_of("astrometry", {"J", "Ks"}), two.audit(), "bande in componenti diversi");
  g.expect_stop(two.recipe(), use_of("astrometry", {"J", "r"}), two.audit(), error_category::incompatible, "pack.band_missing",
                "banda r assente da tutti i componenti");
  EXPECT_EQ(g.operations, 8);
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-PACK-09: fuzz del lettore di manifest e indice, seed fisso, 5 000 mutazioni per file seme.
// ---------------------------------------------------------------------------------------------------------------------

std::string mutate(const std::string& seed, splitmix& g) {
  std::string s = seed;
  const std::string structural = "{}[]\":,0123456789-\\ntf";
  const std::uint64_t steps = 1 + g.below(3);
  for (std::uint64_t k = 0; k < steps; ++k) {
    switch (g.below(7)) {
      case 0:
        if (!s.empty()) {
          const std::size_t at = g.below(s.size());
          s[at] = static_cast<char>(s[at] ^ (1u << g.below(8)));
        }
        break;
      case 1:
        if (!s.empty()) s[g.below(s.size())] = static_cast<char>(g.below(256));
        break;
      case 2:
        if (!s.empty()) {
          const std::size_t at = g.below(s.size());
          s.erase(at, 1 + g.below(8));
        }
        break;
      case 3: {
        std::string ins;
        for (std::uint64_t n = 0, c = 1 + g.below(8); n < c; ++n) ins += static_cast<char>(g.below(256));
        s.insert(s.empty() ? 0 : g.below(s.size() + 1), ins);
        break;
      }
      case 4:
        if (!s.empty()) {
          const std::size_t at = g.below(s.size());
          const std::string chunk = s.substr(at, 1 + g.below(16));
          s.insert(at, chunk);
        }
        break;
      case 5:
        s.resize(g.below(s.size() + 1));
        break;
      default:
        if (!s.empty()) s[g.below(s.size())] = structural[g.below(structural.size())];
        break;
    }
  }
  return s;
}

struct fuzz_tally {
  int accepted = 0;
  std::map<std::string, int> stops;
};

void run_one_fuzz_case(const pack_recipe& r, const pack_audit& a, fuzz_tally& tally) {
  try {
    (void)verify_pack(r, use_of(), a);
    ++tally.accepted;
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), expected_category(e.code())) << e.what();
    ++tally.stops[e.code()];
  }
}

TEST(Pack, T_M3A_PACK_09_fuzz_of_the_manifest_reader_never_crashes) {
  synth p = make_pack(fresh_dir("pk09m"));
  const std::string seed = p.manifest_bytes;
  splitmix g(0xF0220009ULL);
  fuzz_tally tally;
  pack_recipe r = p.recipe();
  for (int i = 0; i < fuzz_iterations; ++i) {
    const std::string mutated = mutate(seed, g);
    write_bytes(p.dir / "manifest.json", mutated);
    r.pack_digest = sha256_of(mutated);
    run_one_fuzz_case(r, p.audit(), tally);
  }
  int stopped = 0;
  for (const auto& [code, n] : tally.stops) stopped += n;
  EXPECT_EQ(tally.accepted + stopped, fuzz_iterations);
  EXPECT_GE(tally.stops.size(), 2u) << "il fuzz non esercita almeno due stop distinti";
  EXPECT_GT(stopped, 0);
}

// Perimetro in U10: G-PACK non interpreta ancora l'indice (§3.3 punto 5, U11). Il caso prova solo che un file elencato
// riscritto con byte arbitrari, con dimensione e SHA-256 aggiornati nel manifest, non ferma in modo anomalo la verifica
// dei file; il fuzz del lettore dell'indice spetta a U11.
TEST(Pack, T_M3A_PACK_09_fuzz_of_the_index_reader_never_crashes) {
  synth p = make_pack(fresh_dir("pk09i"));
  const std::string seed = read_text(p.dir / "index.bin");
  splitmix g(0xF0220010ULL);
  fuzz_tally tally;
  for (int i = 0; i < fuzz_iterations; ++i) {
    write_bytes(p.dir / "index.bin", mutate(seed, g));
    p.refresh_entry("index.bin");
    run_one_fuzz_case(p.recipe(), p.audit(), tally);
  }
  int stopped = 0;
  for (const auto& [code, n] : tally.stops) stopped += n;
  EXPECT_EQ(tally.accepted + stopped, fuzz_iterations);
}

TEST(Pack, T_M3A_PACK_09_fuzz_seeds_are_valid_packs_and_the_generator_is_deterministic) {
  const fs::path base = fresh_dir("pk09s");
  const synth a = make_pack(base / "a");
  const synth b = make_pack(base / "b");
  EXPECT_EQ(a.digest, b.digest);
  EXPECT_EQ(read_text(a.dir / "data.bin"), read_text(b.dir / "data.bin"));
  gate g;
  g.expect_ok(a.recipe(), use_of(), a.audit());
}

// ---------------------------------------------------------------------------------------------------------------------
// Ogni caso fa scattare da sola una regola di §3.1, §3.3 punti 1, 3 e 4 o §3.6 (percorsi con NUL compresi); codici
// attesi dalla tabella di §3.4.
// ---------------------------------------------------------------------------------------------------------------------

TEST(Pack, T_M3A_PACK_04_further_schema_rules_stop_as_manifest_invalid) {
  synth p = make_pack(fresh_dir("pk04x"));
  const jv baseline = p.manifest;
  const std::vector<mutation> cases{
      {"frame diverso da ICRS", [](jv& j) { j.at("components").items[0].set("frame", jv::str("FK5")); }},
      {"componente ripetuto", [](jv& j) { j.at("components").push(jv(j.at("components").items[0])); }},
      {"colonna di un componente non dichiarato", [](jv& j) { column_of(j, "nobs").set("component", jv::str("altro")); }},
      {"tipo sconosciuto su colonna non nullabile", [](jv& j) { column_of(j, "nobs").set("type", jv::str("f16")); }},
      {"null_bit su colonna non nullabile", [](jv& j) { column_of(j, "nobs").set("null_bit", jv::num(3)); }},
      {"nullable non booleano", [](jv& j) { column_of(j, "nobs").set("nullable", jv::str("false")); }},
      {"colonna ripetuta", [](jv& j) { j.at("columns").push(jv(column_of(j, "nobs"))); }},
      {"scopo ripetuto", [](jv& j) { j.set("purpose", jv::arr().push(jv::str("astrometry")).push(jv::str("astrometry"))); }},
      {"scopo come oggetto", [](jv& j) { j.set("purpose", jv::obj().set("a", jv::str("astrometry"))); }},
      {"coverage.order oltre 29", [](jv& j) {
         j.at("coverage").set("order", jv::num(30));
         j.set("index_order", jv::num(30));
       }},
      {"celle non crescenti", [](jv& j) { j.at("coverage").set("cells", jv::arr().push(jv::num(1)).push(jv::num(0))); }},
      {"cella oltre 12*4^order", [](jv& j) { j.at("coverage").set("cells", jv::arr().push(jv::num(48))); }},
      {"inputs_sha256 non esadecimale", [](jv& j) { j.at("builder").set("inputs_sha256", jv::arr().push(jv::str("xyz"))); }},
      {"percorso ripetuto in files[]", [](jv& j) { j.at("files").push(jv(j.at("files").items[0])); }},
      {"percorso con un segmento `.`", [](jv& j) {
         jv f = j.at("files").items[0];
         f.set("path", jv::str("./" + f.at("path").s));
         j.at("files").push(std::move(f));
       }},
      {"intero come stringa", [](jv& j) { j.set("count", jv::str("12")); }},
      {"stringa come intero", [](jv& j) { j.set("pack_version", jv::num(1)); }},
  };
  gate g;
  for (const mutation& m : cases) {
    p.manifest = baseline;
    m.apply(p.manifest);
    p.write_manifest();
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.manifest_invalid", m.label);
  }
  EXPECT_EQ(g.operations, 0);
}

TEST(Pack, T_M3A_PACK_03_symbolic_links_and_a_nested_manifest_are_never_followed_or_skipped) {
  const fs::path base = fresh_dir("pk03l");
  gate g;
  {
    const synth p = make_pack(base / "dati-link");
    write_bytes(base / "dati-fuori.bin", read_text(p.dir / "data.bin"));
    fs::remove(p.dir / "data.bin");
    fs::create_symlink(base / "dati-fuori.bin", p.dir / "data.bin");
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.file_missing", "file elencato come link");
  }
  {
    const synth p = make_pack(base / "alias");
    fs::create_symlink("data.bin", p.dir / "alias.bin");
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.unlisted_file", "link non elencato");
  }
  {
    const synth p = make_pack(base / "licenze-link");
    fs::rename(p.dir / "licenses", base / "licenze-fuori");
    fs::create_directory_symlink(base / "licenze-fuori", p.dir / "licenses");
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.unlisted_file", "directory come link");
  }
  {
    const synth p = make_pack(base / "manifest-link");
    fs::rename(p.dir / "manifest.json", base / "manifest-fuori.json");
    fs::create_symlink(base / "manifest-fuori.json", p.dir / "manifest.json");
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::metadata_missing, "pack.missing", "manifest come link");
  }
  {
    const synth p = make_pack(base / "annidato");
    write_bytes(p.dir / "sub" / "manifest.json", p.manifest_bytes);
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.unlisted_file", "manifest.json in una sottodirectory");
  }
  EXPECT_EQ(g.operations, 0);
}

TEST(Pack, T_M3A_LIC_01_record_name_size_and_single_license_line) {
  const fs::path base = fresh_dir("lic01x");
  gate g;
  int n = 0;
  for (const char* name : {"docs/datasets/REC-SYN-01.md", "docs/datasets/PUB-SYN-01.txt", "docs/datasets/PUB-SUB/PUB-SYN-01.md",
                           "docs/archive1/PUB-SYN-01.md"}) {
    pack_spec spec;
    spec.comps[0].evidence = name;
    const synth p = make_pack(base / ("nome-" + std::to_string(n++)), spec);
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::incompatible, "pack.license_evidence", name);
  }
  {
    const synth p = make_pack(base / "due-righe");
    write_bytes(p.evidence_root / "docs/datasets/PUB-SYN-01.md", evidence_record("CC0-1.0") + evidence_record("CC-BY-4.0"));
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::incompatible, "pack.license_evidence", "due righe di licenza");
  }
  {
    const synth p = make_pack(base / "directory");
    fs::remove(p.evidence_root / "docs/datasets/PUB-SYN-01.md");
    fs::create_directories(p.evidence_root / "docs/datasets/PUB-SYN-01.md");
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::incompatible, "pack.license_evidence", "record come directory");
  }
  {
    const synth p = make_pack(base / "grande");
    write_bytes(p.evidence_root / "docs/datasets/PUB-SYN-01.md", evidence_record("CC0-1.0") + std::string(std::size_t{1} << 20, 'x'));
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::incompatible, "pack.license_evidence", "record oltre 1 MiB");
  }
  EXPECT_EQ(g.operations, 0);
}

TEST(Pack, T_M3A_PACK_03_a_listed_file_replaced_by_a_non_empty_directory_stops_as_file_missing) {
  const fs::path base = fresh_dir("pk03d");
  gate g;
  int n = 0;
  for (const char* rel : {"data.bin", "index.bin", "licenses/synthetic.txt"}) {
    const synth p = make_pack(base / ("c" + std::to_string(n++)));
    fs::remove(p.dir / rel);
    write_bytes(p.dir / rel / "dentro.bin", "contenuto");
    g.expect_stop(p.recipe(), use_of(), p.audit(), error_category::integrity, "pack.file_missing", rel);
  }
  EXPECT_EQ(g.operations, 0);
}

TEST(Pack, T_M3A_PACK_02_a_nul_in_the_recipe_or_audit_path_never_selects_the_prefix) {
  const fs::path base = fresh_dir("pk02n");
  const synth p = make_pack(base / "valido");
  gate g;
  g.expect_ok(p.recipe(), use_of(), p.audit(), "controllo positivo");
  pack_recipe r = p.recipe();
  r.directory = p.dir.string() + std::string(1, '\0') + "/assente";
  g.expect_stop(r, use_of(), p.audit(), error_category::metadata_missing, "pack.missing", "NUL nel percorso della ricetta");
  write_bytes(base / "record.md", evidence_record("CC0-1.0"));
  pack_audit a = p.audit();
  a.evidence_root = (base / "record.md").string() + std::string(1, '\0');
  g.expect_stop(p.recipe(), use_of(), a, error_category::incompatible, "pack.license_evidence", "NUL nella radice delle evidenze");
  EXPECT_EQ(g.operations, 1);
}

}  // namespace
