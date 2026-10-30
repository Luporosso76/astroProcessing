#pragma once

// Generatore del pack sintetico dei test di U11 (G-PACK parte 2: conteggi, indice, righe; ADR-08.b §3.1, §3.3 punto 5).
// Indipendente dal prodotto: manifest canonico, SHA-256 (OpenSSL EVP), file dati e indice li scrive questo file.
// Primitive (SHA-256, interi little-endian, albero JSON e serializzatore canonico) copiate dal generatore di
// tests/unit/test_pack.cpp (U10); lo schema del manifest è quello di §3.1 con le chiavi di §3.6.
//
// File dati: record little-endian senza padding nell'ordine delle colonne dello schema del layout (di base le sette
// colonne di `columns()`, 38 byte), ordinati per cella; file indice: voci di 24 byte {cella, prima riga, righe} in u64 little-endian, una per cella di `coverage`.
#include <openssl/evp.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "healpix_oracle.hpp"

namespace syn_pack {

namespace fs = std::filesystem;

inline std::string sha256_of(const std::string& bytes) {
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

inline void put_le(std::string& out, std::uint64_t v, int bytes) {
  for (int i = 0; i < bytes; ++i) out += static_cast<char>((v >> (8 * i)) & 0xFF);
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

inline void write_bytes(const fs::path& path, const std::string& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  out.close();
  if (!out) throw std::runtime_error("scrittura fallita: " + path.string());
}

inline std::string read_bytes(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("apertura fallita: " + path.string());
  std::string out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) throw std::runtime_error("lettura fallita: " + path.string());
  return out;
}

// Albero JSON del test e serializzatore canonico (RFC 8785 ristretto ad ASCII e interi).
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
  const jv& at(const std::string& k) const { return const_cast<jv*>(this)->at(k); }
};

inline void emit_string(const std::string& s, std::string& out) {
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

inline void emit(const jv& v, std::string& out) {
  switch (v.type) {
    case jv::kind::boolean: out += v.b ? "true" : "false"; return;
    case jv::kind::integer: out += std::to_string(v.i); return;
    case jv::kind::string: emit_string(v.s, out); return;
    case jv::kind::array:
      out += '[';
      for (std::size_t n = 0; n < v.items.size(); ++n) {
        if (n != 0) out += ',';
        emit(v.items[n], out);
      }
      out += ']';
      return;
    case jv::kind::object: {
      std::vector<std::size_t> order(v.keys.size());
      for (std::size_t n = 0; n < order.size(); ++n) order[n] = n;
      std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return v.keys[a] < v.keys[b]; });
      out += '{';
      for (std::size_t n = 0; n < order.size(); ++n) {
        if (n != 0) out += ',';
        emit_string(v.keys[order[n]], out);
        out += ':';
        emit(v.items[order[n]], out);
      }
      out += '}';
      return;
    }
  }
}

inline std::string serialize(const jv& v) {
  std::string out;
  emit(v, out);
  return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Colonne e righe.
// ---------------------------------------------------------------------------------------------------------------------

struct column_spec {
  const char* name;
  const char* type;
  const char* unit;
  int null_bit;  // -1: non nullabile
};

// Tabella delle colonne, indicizzata da `col`. Lo schema di base le usa in quest'ordine (flags non in coda, f64 dopo
// f32): l'offset di ogni campo dipende da columns[] del manifest.
inline const std::vector<column_spec>& columns() {
  static const std::vector<column_spec> c{
      {"ra_deg", "f64", "deg", -1}, {"flags", "u32", "bitmask", -1}, {"j_mag", "f32", "mag", 0},
      {"dec_deg", "f64", "deg", -1}, {"nobs", "u16", "count", -1},   {"h_mag", "f32", "mag", 1},
      {"pm_mas", "f64", "mas/yr", 3}};
  return c;
}

enum col : std::size_t { c_ra, c_flags, c_j, c_dec, c_nobs, c_h, c_pm, column_count };

constexpr std::uint32_t j_null_bit = 1u << 0, h_null_bit = 1u << 1, saturated_bit = 1u << 2, pm_null_bit = 1u << 3;
constexpr std::uint32_t saturated_index = 2;
// Schema: colonne del manifest, nell'ordine del record, come indici di `columns()`.
using schema = std::vector<std::size_t>;
inline schema default_schema() { return {c_ra, c_flags, c_j, c_dec, c_nobs, c_h, c_pm}; }
// Variante: colonne permutate, senza nobs e pm_mas (record di 28 byte, bit 3 non dichiarato).
inline schema permuted_schema() { return {c_dec, c_h, c_flags, c_ra, c_j}; }

inline std::size_t field_bytes(std::size_t c) {
  const std::string t = columns()[c].type;
  return t == "f64" ? 8 : t == "u16" ? 2 : 4;
}
inline std::size_t record_size(const schema& s) {
  std::size_t n = 0;
  for (const std::size_t c : s) n += field_bytes(c);
  return n;
}

inline std::uint64_t bits64(double v) {
  std::uint64_t u;
  std::memcpy(&u, &v, 8);
  return u;
}
inline std::uint32_t bits32(float v) {
  std::uint32_t u;
  std::memcpy(&u, &v, 4);
  return u;
}
inline double from64(std::uint64_t u) {
  double v;
  std::memcpy(&v, &u, 8);
  return v;
}
inline double from32(std::uint32_t u) {
  float v;
  std::memcpy(&v, &u, 4);
  return static_cast<double>(v);
}

constexpr std::uint32_t f32_quiet_nan = 0x7FC00000u;
constexpr std::uint64_t f64_quiet_nan = 0x7FF8000000000000ULL;

// Riga come bit grezzi dei campi: i test vi scrivono NaN con payload, infiniti e bit di flags non dichiarati.
struct row {
  std::uint64_t ra = 0;
  std::uint32_t flags = 0;
  std::uint32_t j = 0;
  std::uint64_t dec = 0;
  std::uint16_t nobs = 0;
  std::uint32_t h = 0;
  std::uint64_t pm = 0;
};

inline std::uint64_t field_bits(const row& r, std::size_t c) {
  switch (c) {
    case c_ra: return r.ra;
    case c_flags: return r.flags;
    case c_j: return r.j;
    case c_dec: return r.dec;
    case c_nobs: return r.nobs;
    case c_h: return r.h;
    case c_pm: return r.pm;
    default: throw std::logic_error("colonna fuori dallo schema del test");
  }
}

inline std::string encode(const row& r, const schema& s = default_schema()) {
  std::string out;
  for (const std::size_t c : s) put_le(out, field_bits(r, c), static_cast<int>(field_bytes(c)));
  return out;
}

// Riga dai byte del record che inizia a `at` (gli oracoli del fuzz decodificano i byte mutati).
inline row decode(const std::string& bytes, std::size_t at, const schema& s = default_schema()) {
  row r;
  for (const std::size_t c : s) {
    std::uint64_t v = 0;
    for (std::size_t i = field_bytes(c); i-- > 0;) v = (v << 8) | static_cast<unsigned char>(bytes[at + i]);
    at += field_bytes(c);
    switch (c) {
      case c_ra: r.ra = v; break;
      case c_flags: r.flags = static_cast<std::uint32_t>(v); break;
      case c_j: r.j = static_cast<std::uint32_t>(v); break;
      case c_dec: r.dec = v; break;
      case c_nobs: r.nobs = static_cast<std::uint16_t>(v); break;
      case c_h: r.h = static_cast<std::uint32_t>(v); break;
      case c_pm: r.pm = v; break;
      default: throw std::logic_error("colonna fuori dallo schema del test");
    }
  }
  return r;
}

// Valore della colonna come lo restituisce il lettore secondo l'interfaccia attesa: double senza perdita (f32
// allargato, interi esatti); un NaN resta NaN.
inline double value(const row& r, std::size_t c) {
  switch (c) {
    case c_ra: return from64(r.ra);
    case c_flags: return static_cast<double>(r.flags);
    case c_j: return from32(r.j);
    case c_dec: return from64(r.dec);
    case c_nobs: return static_cast<double>(r.nobs);
    case c_h: return from32(r.h);
    case c_pm: return from64(r.pm);
    default: throw std::logic_error("colonna fuori dallo schema del test");
  }
}

// Riga valida numero k (globale) della cella `cell`: posizione dentro la cella secondo l'oracolo, assenze e flag di
// qualità a schema fisso.
inline row make_row(int order, std::uint64_t cell, std::uint64_t k, const healpix_oracle::vec3& center) {
  using namespace healpix_oracle;
  const double ra = phi_of(center) / deg + 1e-4 * static_cast<double>(k % 7);
  const double dec = std::asin(z_of(center)) / deg;
  if (ang2pix_nest(order, from_radec_deg(ra, dec)) != cell) throw std::logic_error("riga del generatore fuori dalla sua cella");
  row r;
  r.ra = bits64(ra);
  r.dec = bits64(dec);
  r.nobs = static_cast<std::uint16_t>(7 * k + 1);
  const bool j_null = k % 4 == 1, h_null = k % 5 == 2, pm_null = k % 3 == 0, saturated = k % 6 == 5;
  r.j = j_null ? f32_quiet_nan : bits32(10.0f + 0.25f * static_cast<float>(k));
  r.h = h_null ? f32_quiet_nan : bits32(11.5f + 0.5f * static_cast<float>(k));
  r.pm = pm_null ? f64_quiet_nan : bits64(-3.5 + 1.25 * static_cast<double>(k));
  r.flags = (j_null ? j_null_bit : 0u) | (h_null ? h_null_bit : 0u) | (pm_null ? pm_null_bit : 0u) |
            (saturated ? saturated_bit : 0u);
  return r;
}

// Riga valida senza assenze né flag, per le iniezioni di un solo difetto.
inline row clean_row(const row& base) {
  row r = base;
  r.flags = 0;
  r.j = bits32(12.25f);
  r.h = bits32(13.5f);
  r.pm = bits64(4.75);
  return r;
}

// ---------------------------------------------------------------------------------------------------------------------
// Layout del pack e scrittura.
// ---------------------------------------------------------------------------------------------------------------------

struct layout {
  int order = 1;
  std::vector<std::pair<std::uint64_t, std::size_t>> cells;  // cella NESTED crescente, numero di righe
  schema cols = default_schema();
};

// Pack di base dei test: ordine 1, sei celle (quattro figli della cella di base 0, una equatoriale, una a sud).
inline layout default_layout() { return layout{1, {{0, 3}, {1, 2}, {2, 1}, {3, 4}, {21, 3}, {46, 2}}, default_schema()}; }

struct index_entry {
  std::uint64_t cell, first, rows;
};

inline std::string index_bytes(const std::vector<index_entry>& entries) {
  std::string out;
  for (const index_entry& e : entries) {
    put_le(out, e.cell, 8);
    put_le(out, e.first, 8);
    put_le(out, e.rows, 8);
  }
  return out;
}

inline std::string data_bytes(const std::vector<row>& rows, const schema& s = default_schema()) {
  std::string out;
  for (const row& r : rows) out += encode(r, s);
  return out;
}

constexpr const char* data_name = "data.bin";
constexpr const char* index_name = "index.bin";

struct synth {
  fs::path dir;
  fs::path evidence_root;
  std::string id = "syn-pack-u11";
  std::string version = "2.0.0";
  layout lay;
  std::vector<row> rows;              // nell'ordine del file dati
  std::vector<index_entry> index;     // come scritto in index.bin
  jv manifest;
  std::string manifest_bytes;
  std::string digest;

  void write_manifest() {
    manifest_bytes = serialize(manifest);
    write_bytes(dir / "manifest.json", manifest_bytes);
    digest = sha256_of(manifest_bytes);
  }
  // Riscrive un file elencato e ne aggiorna dimensione e SHA-256 nel manifest (il pack resta integro per §3.3.3).
  void rewrite(const std::string& rel, const std::string& bytes) {
    write_bytes(dir / rel, bytes);
    for (jv& f : manifest.at("files").items) {
      if (f.at("path").s == rel) {
        f.set("size_bytes", jv::num(static_cast<std::int64_t>(bytes.size())));
        f.set("sha256", jv::str(sha256_of(bytes)));
      }
    }
    write_manifest();
  }
  void rewrite_rows(const std::vector<row>& r) { rewrite(data_name, data_bytes(r, lay.cols)); }
  void rewrite_index(const std::vector<index_entry>& e) { rewrite(index_name, index_bytes(e)); }
  void set_counts(std::int64_t manifest_count, std::int64_t component_count) {
    manifest.set("count", jv::num(manifest_count));
    manifest.at("components").items[0].set("count", jv::num(component_count));
    write_manifest();
  }
  // Righe della cella nel file dati, secondo l'indice scritto dal generatore.
  std::vector<row> rows_of(std::uint64_t cell) const {
    for (const index_entry& e : index) {
      if (e.cell == cell) return {rows.begin() + static_cast<std::ptrdiff_t>(e.first),
                                  rows.begin() + static_cast<std::ptrdiff_t>(e.first + e.rows)};
    }
    throw std::logic_error("cella fuori dal layout del test");
  }
  std::vector<std::uint64_t> coverage() const {
    std::vector<std::uint64_t> out;
    for (const auto& [cell, n] : lay.cells) out.push_back(cell);
    return out;
  }
};

inline jv make_column(const column_spec& c, const std::string& comp) {
  jv o = jv::obj();
  o.set("name", jv::str(c.name)).set("type", jv::str(c.type)).set("unit", jv::str(c.unit)).set("component", jv::str(comp));
  o.set("nullable", jv::boolean(c.null_bit >= 0));
  if (c.null_bit >= 0) o.set("null_bit", jv::num(c.null_bit));
  return o;
}

inline std::string evidence_record(const std::string& license) {
  return "# PUB-SYN: record di audit sintetico del test\n\n- Stato: ACQUISITO. Licenza SPDX: `" + license + "`.\n";
}

inline jv component(const std::string& name, const std::string& license_file, const std::string& evidence,
                    std::uint64_t count) {
  jv bands = jv::arr();
  for (const char* b : {"J", "H"}) bands.push(jv::obj().set("name", jv::str(b)).set("system", jv::str("2MASS")));
  jv c = jv::obj();
  c.set("name", jv::str(name))
      .set("source", jv::str("Generatore sintetico del test, https://example.invalid/synthetic"))
      .set("release", jv::str("synthetic-2 2026-10-30"))
      .set("license", jv::str("CC0-1.0"))
      .set("license_file", jv::str(license_file))
      .set("license_evidence", jv::str(evidence))
      .set("attribution", jv::str(""))
      .set("bands", std::move(bands))
      .set("frame", jv::str("ICRS"))
      .set("epoch", jv::str("J2000.0"))
      .set("selection", jv::str("nessuna"))
      .set("count", jv::num(static_cast<std::int64_t>(count)));
  return c;
}

inline jv file_entry(const std::string& rel, const std::string& bytes) {
  return jv::obj()
      .set("path", jv::str(rel))
      .set("size_bytes", jv::num(static_cast<std::int64_t>(bytes.size())))
      .set("sha256", jv::str(sha256_of(bytes)));
}

inline synth make_pack(const fs::path& base, const layout& lay = default_layout()) {
  using namespace healpix_oracle;
  synth p;
  p.dir = base / "pack";
  p.evidence_root = base / "ev";
  p.lay = lay;
  fs::create_directories(p.dir);
  fs::create_directories(p.evidence_root);

  const std::vector<vec3> centers = nest_centers(lay.order);
  for (const auto& [cell, n] : lay.cells) {
    p.index.push_back(index_entry{cell, p.rows.size(), n});
    for (std::size_t r = 0; r < n; ++r) p.rows.push_back(make_row(lay.order, cell, p.rows.size(), centers[cell]));
  }
  // Il bit di assenza di una colonna nullabile fuori dallo schema non è dichiarato: vale 0 in ogni riga (§3.1).
  for (std::size_t c = 0; c < columns().size(); ++c) {
    if (columns()[c].null_bit < 0 || std::find(lay.cols.begin(), lay.cols.end(), c) != lay.cols.end()) continue;
    for (row& r : p.rows) r.flags &= ~(1u << columns()[c].null_bit);
  }

  std::map<std::string, std::string> files;
  files[data_name] = data_bytes(p.rows, lay.cols);
  files[index_name] = index_bytes(p.index);
  files["licenses/synthetic.txt"] = "Testo di licenza sintetico del componente synthetic\n";
  write_bytes(p.evidence_root / "docs/datasets/PUB-SYN-01.md", evidence_record("CC0-1.0"));

  jv cols = jv::arr();
  for (const std::size_t c : lay.cols) cols.push(make_column(columns()[c], "synthetic"));
  jv cells = jv::arr();
  for (const auto& [cell, n] : lay.cells) cells.push(jv::num(static_cast<std::int64_t>(cell)));

  jv m = jv::obj();
  m.set("pack_format", jv::num(1))
      .set("pack_id", jv::str(p.id))
      .set("pack_version", jv::str(p.version))
      .set("purpose", jv::arr().push(jv::str("astrometry")))
      .set("components", jv::arr().push(component("synthetic", "licenses/synthetic.txt", "docs/datasets/PUB-SYN-01.md",
                                                  p.rows.size())))
      .set("columns", std::move(cols))
      .set("flag_bits", jv::arr().push(jv::obj().set("index", jv::num(saturated_index)).set("meaning", jv::str("saturated"))))
      .set("data_file", jv::str(data_name))
      .set("index_file", jv::str(index_name))
      .set("coverage", jv::obj().set("order", jv::num(lay.order)).set("cells", std::move(cells)))
      .set("index_order", jv::num(lay.order))
      .set("count", jv::num(static_cast<std::int64_t>(p.rows.size())))
      .set("builder", jv::obj()
                          .set("name", jv::str("synthetic-pack-builder-u11"))
                          .set("version", jv::str("1"))
                          .set("inputs_sha256", jv::arr().push(jv::str(sha256_of("layout-u11")))));
  jv flist = jv::arr();
  for (const auto& [rel, bytes] : files) {
    write_bytes(p.dir / rel, bytes);
    flist.push(file_entry(rel, bytes));
  }
  m.set("files", std::move(flist));
  p.manifest = std::move(m);
  p.write_manifest();
  return p;
}

// Aggiunge un secondo componente valido (licenza, file di licenza elencato e record di audit) con il conteggio dato.
inline void add_component(synth& p, const std::string& name, std::uint64_t count) {
  const std::string lic = "licenses/" + name + ".txt";
  const std::string ev = "docs/datasets/PUB-SYN-02.md";
  const std::string text = "Testo di licenza sintetico del componente " + name + "\n";
  write_bytes(p.dir / lic, text);
  write_bytes(p.evidence_root / ev, evidence_record("CC0-1.0"));
  p.manifest.at("components").push(component(name, lic, ev, count));
  p.manifest.at("files").push(file_entry(lic, text));
  std::sort(p.manifest.at("files").items.begin(), p.manifest.at("files").items.end(),
            [](const jv& a, const jv& b) { return a.at("path").s < b.at("path").s; });
  p.write_manifest();
}

}  // namespace syn_pack
