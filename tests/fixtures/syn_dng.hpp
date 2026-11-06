#pragma once

// FIX-SYN-DNG-01: scrittore TIFF/DNG minimale, little-endian, non compresso. Scritto a mano (struttura TIFF 6.0 e tag
// di Adobe DNG Specification 1.4.0.0): non condivide codice con astrocore, con LibRaw ne' con libtiff, cosi' i valori
// attesi dei test sono una verita' indipendente dal parser sotto prova.
//
// Convenzione dei dati: `pixels` e' nelle coordinate del buffer grezzo (origine = angolo del TIFF); `active_area`
// seleziona l'area visibile dentro quel buffer. Adobe DNG Specification 1.6.0.0 (pp. 21, 26, 42, 84; ADR-08.b §2.3): CFAPattern
// e BlackLevel (con BlackLevelRepeatDim) sono ancorati all'angolo in alto a sinistra di ActiveArea. Il colore del
// campione grezzo (r, c) e' quindi cfa[((r - top) % cfa_rows) * cfa_cols + (c - left) % cfa_cols], con top = left = 0
// se ActiveArea e' assente.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace astro_fixture {

struct dng_active_area {
  std::uint32_t top = 0;
  std::uint32_t left = 0;
  std::uint32_t bottom = 0;
  std::uint32_t right = 0;
};

struct dng_spec {
  std::uint32_t width = 0;   // buffer grezzo
  std::uint32_t height = 0;  // buffer grezzo
  std::uint16_t samples_per_pixel = 1;
  std::uint16_t bits = 16;
  std::uint16_t sample_format = 1;     // 1 intero senza segno, 3 virgola mobile
  std::uint16_t compression = 1;       // tag 259: 1 nessuna; 9 VC-5, 52546 JPEG XL (dati non compressi, solo per le intestazioni)
  std::uint16_t photometric = 32803;   // 32803 CFA, 34892 LinearRaw
  std::uint16_t cfa_rows = 2;
  std::uint16_t cfa_cols = 2;
  std::vector<std::uint8_t> cfa = {0, 1, 1, 2};  // 0 R, 1 G, 2 B; riga per riga
  std::optional<dng_active_area> active_area;
  // BlackLevel scalare se black_dim e' assente; con black_dim, black_dim.first * black_dim.second valori.
  std::optional<std::pair<std::uint16_t, std::uint16_t>> black_dim;
  std::vector<std::uint16_t> black;
  std::optional<std::uint32_t> white;
  std::vector<std::uint16_t> linearization;  // vuota = tag assente
  std::optional<std::array<std::uint32_t, 4>> crop;  // DefaultCropOrigin x, y e DefaultCropSize w, h
  std::uint16_t orientation = 1;
  // IFD EXIF, scritto se almeno uno dei tre e' valorizzato.
  std::optional<std::pair<std::uint32_t, std::uint32_t>> exposure_time;  // numeratore, denominatore
  std::optional<std::uint16_t> iso;
  std::string datetime_original;  // "AAAA:MM:GG hh:mm:ss"; vuota = assente
  std::string make = "AstroTest";
  std::string model = "SYN-DNG-01";
  std::vector<std::uint8_t> pixels;  // little-endian, bits/8 byte per campione, riga per riga
};

inline std::vector<std::uint8_t> pixels_u16(const std::vector<std::uint16_t>& v) {
  std::vector<std::uint8_t> out;
  out.reserve(v.size() * 2);
  for (std::uint16_t s : v) {
    out.push_back(static_cast<std::uint8_t>(s & 0xFF));
    out.push_back(static_cast<std::uint8_t>(s >> 8));
  }
  return out;
}

namespace dng_detail {

enum : std::uint16_t { t_byte = 1, t_ascii = 2, t_short = 3, t_long = 4, t_rational = 5, t_srational = 10 };

struct entry {
  std::uint16_t tag;
  std::uint16_t type;
  std::uint32_t count;
  std::vector<std::uint8_t> data;
};

inline void le(std::vector<std::uint8_t>& out, std::uint64_t v, int n) {
  for (int i = 0; i < n; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

inline entry e_shorts(std::uint16_t tag, const std::vector<std::uint16_t>& v) {
  entry e{tag, t_short, static_cast<std::uint32_t>(v.size()), {}};
  for (auto x : v) le(e.data, x, 2);
  return e;
}
inline entry e_longs(std::uint16_t tag, const std::vector<std::uint32_t>& v) {
  entry e{tag, t_long, static_cast<std::uint32_t>(v.size()), {}};
  for (auto x : v) le(e.data, x, 4);
  return e;
}
inline entry e_bytes(std::uint16_t tag, const std::vector<std::uint8_t>& v) {
  return entry{tag, t_byte, static_cast<std::uint32_t>(v.size()), v};
}
inline entry e_ascii(std::uint16_t tag, const std::string& s) {
  entry e{tag, t_ascii, static_cast<std::uint32_t>(s.size() + 1), {}};
  e.data.assign(s.begin(), s.end());
  e.data.push_back(0);
  return e;
}
inline entry e_rationals(std::uint16_t tag, std::uint16_t type, const std::vector<std::int32_t>& num_den) {
  entry e{tag, type, static_cast<std::uint32_t>(num_den.size() / 2), {}};
  for (auto x : num_den) le(e.data, static_cast<std::uint32_t>(x), 4);
  return e;
}

struct ifd_position {
  std::uint32_t ifd_offset;
  std::size_t next_field;  // da correggere se segue un altro IFD
};

// Scrive in coda valori fuori riga e IFD con le voci ordinate per tag.
inline ifd_position write_entries(std::vector<std::uint8_t>& out, std::vector<entry> es) {
  std::vector<std::size_t> order(es.size());
  for (std::size_t i = 0; i < es.size(); ++i) order[i] = i;
  for (std::size_t i = 1; i < order.size(); ++i)
    for (std::size_t j = i; j > 0 && es[order[j]].tag < es[order[j - 1]].tag; --j) std::swap(order[j], order[j - 1]);

  std::vector<std::uint32_t> value_offset(es.size(), 0);
  for (std::size_t i : order) {
    if (es[i].data.size() > 4) {
      if (out.size() % 2) out.push_back(0);
      value_offset[i] = static_cast<std::uint32_t>(out.size());
      out.insert(out.end(), es[i].data.begin(), es[i].data.end());
    }
  }
  if (out.size() % 2) out.push_back(0);
  const std::uint32_t ifd_offset = static_cast<std::uint32_t>(out.size());
  le(out, es.size(), 2);
  for (std::size_t i : order) {
    le(out, es[i].tag, 2);
    le(out, es[i].type, 2);
    le(out, es[i].count, 4);
    if (es[i].data.size() > 4) {
      le(out, value_offset[i], 4);
    } else {
      std::vector<std::uint8_t> inl = es[i].data;
      inl.resize(4, 0);
      out.insert(out.end(), inl.begin(), inl.end());
    }
  }
  const std::size_t next_field = out.size();
  le(out, 0, 4);
  return {ifd_offset, next_field};
}

// Aggiunge in coda l'IFD di `s` (dati dei pixel, IFD EXIF se richiesto, valori fuori riga, voci ordinate per tag).
inline ifd_position append_ifd(std::vector<std::uint8_t>& out, const dng_spec& s) {
  if (out.size() % 2) out.push_back(0);
  const std::uint32_t pixel_offset = static_cast<std::uint32_t>(out.size());
  out.insert(out.end(), s.pixels.begin(), s.pixels.end());
  if (out.size() % 2) out.push_back(0);

  std::vector<entry> es;
  es.push_back(e_longs(254, {0}));
  es.push_back(e_longs(256, {s.width}));
  es.push_back(e_longs(257, {s.height}));
  es.push_back(e_shorts(258, std::vector<std::uint16_t>(s.samples_per_pixel, s.bits)));
  es.push_back(e_shorts(259, {s.compression}));
  es.push_back(e_shorts(262, {s.photometric}));
  es.push_back(e_ascii(271, s.make));
  es.push_back(e_ascii(272, s.model));
  es.push_back(e_longs(273, {pixel_offset}));
  es.push_back(e_shorts(274, {s.orientation}));
  es.push_back(e_shorts(277, {s.samples_per_pixel}));
  es.push_back(e_longs(278, {s.height}));
  es.push_back(e_longs(279, {static_cast<std::uint32_t>(s.pixels.size())}));
  es.push_back(e_shorts(284, {1}));
  if (s.sample_format != 1) es.push_back(e_shorts(339, std::vector<std::uint16_t>(s.samples_per_pixel, s.sample_format)));
  if (s.photometric == 32803) {
    es.push_back(e_shorts(33421, {s.cfa_rows, s.cfa_cols}));
    es.push_back(e_bytes(33422, s.cfa));
  }
  es.push_back(e_bytes(50706, {1, 4, 0, 0}));
  es.push_back(e_bytes(50707, {1, 1, 0, 0}));
  es.push_back(e_ascii(50708, s.make + " " + s.model));
  if (!s.linearization.empty()) es.push_back(e_shorts(50712, s.linearization));
  if (s.black_dim) {
    es.push_back(e_shorts(50713, {s.black_dim->first, s.black_dim->second}));
    es.push_back(e_shorts(50714, s.black));
  } else if (!s.black.empty()) {
    es.push_back(e_shorts(50714, s.black));
  }
  if (s.white) es.push_back(e_longs(50717, {*s.white}));
  if (s.crop) {
    es.push_back(e_longs(50719, {(*s.crop)[0], (*s.crop)[1]}));
    es.push_back(e_longs(50720, {(*s.crop)[2], (*s.crop)[3]}));
  }
  if (s.exposure_time || s.iso || !s.datetime_original.empty()) {
    std::vector<entry> ex;
    if (s.exposure_time)
      ex.push_back(e_rationals(33434, t_rational,
                               {static_cast<std::int32_t>(s.exposure_time->first),
                                static_cast<std::int32_t>(s.exposure_time->second)}));
    if (s.iso) ex.push_back(e_shorts(34855, {*s.iso}));
    if (!s.datetime_original.empty()) ex.push_back(e_ascii(36867, s.datetime_original));
    const ifd_position ep = write_entries(out, std::move(ex));
    es.push_back(e_longs(34665, {ep.ifd_offset}));
  }
  es.push_back(e_rationals(50721, t_srational, {1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1}));
  es.push_back(e_rationals(50728, t_rational, {1, 1, 1, 1, 1, 1}));
  es.push_back(e_shorts(50778, {21}));
  if (s.active_area) {
    const auto& a = *s.active_area;
    es.push_back(e_longs(50829, {a.top, a.left, a.bottom, a.right}));
  }
  return write_entries(out, std::move(es));
}

}  // namespace dng_detail

// Uno o piu' IFD concatenati; l'intestazione (8 byte) punta al primo.
inline std::vector<std::uint8_t> make_dng(const std::vector<dng_spec>& images) {
  using namespace dng_detail;
  std::vector<std::uint8_t> out = {'I', 'I', 42, 0, 0, 0, 0, 0};
  std::size_t prev_next = 4;  // campo che punta al prossimo IFD (l'intestazione per il primo)
  for (const auto& s : images) {
    const ifd_position pos = append_ifd(out, s);
    for (int i = 0; i < 4; ++i) out[prev_next + i] = static_cast<std::uint8_t>(pos.ifd_offset >> (8 * i));
    prev_next = pos.next_field;
  }
  return out;
}

inline std::vector<std::uint8_t> make_dng(const dng_spec& s) { return make_dng(std::vector<dng_spec>{s}); }

// FIX-SYN-DNG-02 (U13, docs/contracts/U13-dng-decoder.md §12): scrittore TIFF generico per i DNG del decoder del progetto.
// Ordine dei byte `II` o `MM`, TIFF classico o BigTIFF (TIFF 6.0; DNG 1.7.1.0 p. 18), catena di IFD, `SubIFDs` (330),
// IFD EXIF (34665), segmenti (strisce o tasselli) i cui offset e conteggi riempie lo scrittore. Ogni voce si scrive come
// dichiarata, anche non conforme (tipo, conteggio, offset), per i test di struttura. Disposizione del file: intestazione,
// poi per ogni IFD i figli, i valori fuori riga e la tabella; infine i dati dei segmenti in ordine anticipato (IFD0, i
// suoi `SubIFDs`, gli IFD successivi della catena). Ogni byte prima di `tables_end` è struttura, ogni byte dopo è dato.

enum class tiff_order { ii, mm };

namespace tiff_type {
enum : std::uint16_t {
  byte_ = 1, ascii = 2, short_ = 3, long_ = 4, rational = 5, sbyte = 6, undefined = 7, sshort = 8, slong = 9,
  srational = 10, float_ = 11, double_ = 12, ifd = 13, long8 = 16, slong8 = 17, ifd8 = 18
};
}  // namespace tiff_type

struct tiff_entry {
  std::uint16_t tag = 0;
  std::uint16_t type = 0;
  // Un elemento per valore; RATIONAL e SRATIONAL: numeratore e denominatore alternati; ASCII e BYTE: un byte ciascuno.
  std::vector<std::uint64_t> items;
  std::optional<std::uint64_t> count;         // conteggio scritto al posto di quello degli elementi
  std::optional<std::uint64_t> value_offset;  // offset scritto al posto di quello reale dei valori fuori riga
  bool segment_offsets = false;               // elementi = offset dei segmenti dell'IFD, riempiti dallo scrittore
  bool segment_counts = false;                // elementi = byte dei segmenti dell'IFD
};

// Voce di `SubIFDs` che punta a una tabella già scritta o da scrivere, invece di un figlio proprio: `chain` = IFD della
// catena `index` (IFD0 compreso); altrimenti il figlio `index` di `sub_ifds` dello stesso IFD (offset ripetuto).
struct tiff_sub_ref {
  bool chain = false;
  std::size_t index = 0;
};

struct tiff_ifd {
  std::vector<tiff_entry> entries;
  std::vector<std::vector<std::uint8_t>> segments;  // dati dei segmenti, nell'ordine dei loro offset
  std::vector<tiff_ifd> sub_ifds;                   // puntati dal tag 330, aggiunto dallo scrittore se non vuoto
  std::vector<tiff_sub_ref> sub_refs;               // altre voci del tag 330, dopo i figli, che ripetono una tabella
  std::vector<tiff_ifd> exif;                       // al più uno, puntato dal tag 34665
};

struct tiff_file {
  tiff_order order = tiff_order::ii;
  bool bigtiff = false;
  std::vector<tiff_ifd> chain;               // IFD0, IFD1, ...
  std::optional<std::size_t> last_next_to;   // il «prossimo IFD» dell'ultimo punta a chain[i] (ciclo); assente: 0
  std::optional<std::uint64_t> last_next;    // offset scritto nel «prossimo IFD» dell'ultimo, al posto di 0
  std::optional<std::uint64_t> first_ifd;    // offset del primo IFD scritto al posto di quello reale
};

struct tiff_layout {
  std::vector<std::uint8_t> bytes;
  std::size_t tables_end = 0;  // primo byte dei dati dei segmenti
  // Fine dell'ultima struttura letta dall'instradamento (U13 §3, punti 2-3): intestazione, tabelle della catena e dei
  // `SubIFDs` di IFD0, valori fuori riga di 254, 258, 259, 330, 339, 50706 in quelle tabelle.
  std::size_t route_end = 0;
  std::size_t last_data_begin = 0;  // primo byte dei segmenti dell'ultimo IFD con segmenti, nell'ordine dei dati
  std::size_t last_data_end = 0;    // fine dell'ultimo segmento scritto, senza il byte di allineamento
  std::vector<std::uint64_t> chain_tables;  // offset della tabella di ogni IFD della catena
  std::vector<std::uint64_t> sub_tables;    // offset della tabella di ogni figlio di `sub_ifds` di IFD0
};

namespace tiff_detail {

inline std::size_t item_size(std::uint16_t type) {
  switch (type) {
    case tiff_type::byte_: case tiff_type::ascii: case tiff_type::sbyte: case tiff_type::undefined: return 1;
    case tiff_type::short_: case tiff_type::sshort: return 2;
    case tiff_type::long_: case tiff_type::slong: case tiff_type::float_: case tiff_type::ifd:
    case tiff_type::rational: case tiff_type::srational: return 4;
    default: return 8;
  }
}

inline std::uint64_t natural_count(const tiff_entry& e) {
  const bool pairs = e.type == tiff_type::rational || e.type == tiff_type::srational;
  return pairs ? e.items.size() / 2 : e.items.size();
}

struct writer {
  tiff_order order = tiff_order::ii;
  bool big = false;
  std::vector<std::uint8_t> out;
  struct patch {
    std::size_t pos;
    std::size_t size;
    std::size_t owner;
    std::size_t index;
    bool counts;
  };
  std::vector<patch> patches;
  std::vector<const tiff_ifd*> owners;  // IFD con segmenti, nell'ordine dei loro dati
  struct ref_patch {
    std::size_t pos;
    std::size_t size;
    std::size_t chain_index;
  };
  std::vector<ref_patch> ref_patches;   // voci di `SubIFDs` verso IFD della catena, risolte a fine scrittura
  std::vector<std::uint64_t> sub_tables;
  std::size_t route_end = 0;

  void put_at(std::size_t pos, std::uint64_t v, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      const std::size_t shift = order == tiff_order::ii ? i : n - 1 - i;
      out[pos + i] = static_cast<std::uint8_t>(v >> (8 * shift));
    }
  }
  void put(std::uint64_t v, std::size_t n) {
    out.resize(out.size() + n);
    put_at(out.size() - n, v, n);
  }
  void align() {
    if (out.size() % 2) out.push_back(0);
  }
  std::size_t offset_size() const { return big ? 8 : 4; }

  // Scrive figli, valori fuori riga e tabella di `ifd`; restituisce l'offset della tabella e la posizione del campo
  // «prossimo IFD». `route`: l'instradamento legge la tabella; `route_children`: legge anche quelle dei `SubIFDs`.
  std::pair<std::uint64_t, std::size_t> write_ifd(const tiff_ifd& ifd, bool route, bool route_children) {
    std::optional<std::size_t> sub_index;
    std::vector<std::pair<std::size_t, std::size_t>> pending;  // (posizione fra i valori di 330, IFD della catena)
    const std::size_t owner = owners.size();
    if (!ifd.segments.empty()) owners.push_back(&ifd);
    std::vector<tiff_entry> es = ifd.entries;
    if (!ifd.sub_ifds.empty() || !ifd.sub_refs.empty()) {
      tiff_entry sub{330, big ? tiff_type::ifd8 : tiff_type::long_, {}, {}, {}, false, false};
      std::vector<std::uint64_t> child_tables;
      for (const tiff_ifd& child : ifd.sub_ifds) {
        child_tables.push_back(write_ifd(child, route_children, false).first);
        sub.items.push_back(child_tables.back());
        if (route_children) sub_tables.push_back(child_tables.back());
      }
      for (const tiff_sub_ref& ref : ifd.sub_refs) {
        if (ref.chain) {
          pending.emplace_back(sub.items.size(), ref.index);
          sub.items.push_back(0);
        } else {
          sub.items.push_back(child_tables.at(ref.index));
        }
      }
      sub_index = es.size();
      es.push_back(sub);
    }
    if (!ifd.exif.empty()) {
      const std::uint64_t ex = write_ifd(ifd.exif.front(), false, false).first;
      es.push_back(tiff_entry{34665, big ? tiff_type::ifd8 : tiff_type::long_, {ex}, {}, {}, false, false});
    }
    std::vector<std::size_t> idx(es.size());
    for (std::size_t i = 0; i < es.size(); ++i) idx[i] = i;
    for (std::size_t i = 1; i < idx.size(); ++i)
      for (std::size_t j = i; j > 0 && es[idx[j]].tag < es[idx[j - 1]].tag; --j) std::swap(idx[j], idx[j - 1]);
    const auto route_tag = [](std::uint16_t t) {
      return t == 254 || t == 258 || t == 259 || t == 330 || t == 339 || t == 50706;
    };
    std::vector<std::optional<std::size_t>> value_pos(es.size());
    const std::size_t inline_cap = offset_size();
    for (std::size_t i : idx) {
      const tiff_entry& e = es[i];
      const std::size_t n = item_size(e.type);
      if (e.items.size() * n > inline_cap) {
        align();
        value_pos[i] = out.size();
        for (const std::uint64_t v : e.items) put(v, n);
        if (route && route_tag(e.tag)) route_end = std::max(route_end, out.size());
      }
    }
    align();
    const std::uint64_t table = out.size();
    put(es.size(), big ? 8 : 2);
    for (std::size_t i : idx) {
      const tiff_entry& e = es[i];
      const std::size_t n = item_size(e.type);
      put(e.tag, 2);
      put(e.type, 2);
      put(e.count.value_or(natural_count(e)), big ? 8 : 4);
      const std::size_t field = out.size();
      if (value_pos[i]) {
        put(e.value_offset.value_or(*value_pos[i]), inline_cap);
      } else {
        out.resize(out.size() + inline_cap, 0);
        for (std::size_t k = 0; k < e.items.size(); ++k) put_at(field + k * n, e.items[k], n);
      }
      if (sub_index && i == *sub_index) {
        const std::size_t base = value_pos[i] ? *value_pos[i] : field;
        for (const auto& [k, chain_index] : pending) ref_patches.push_back({base + k * n, n, chain_index});
      }
      if (e.segment_offsets || e.segment_counts) {
        const std::size_t base = value_pos[i] ? *value_pos[i] : field;
        for (std::size_t k = 0; k < e.items.size(); ++k) patches.push_back({base + k * n, n, owner, k, e.segment_counts});
      }
    }
    const std::size_t next = out.size();
    put(0, offset_size());
    if (route) route_end = std::max(route_end, out.size());
    return {table, next};
  }
};

}  // namespace tiff_detail

inline tiff_layout write_tiff(const tiff_file& f) {
  tiff_detail::writer w;
  w.order = f.order;
  w.big = f.bigtiff;
  if (f.order == tiff_order::ii) {
    w.out = {'I', 'I'};
  } else {
    w.out = {'M', 'M'};
  }
  w.put(f.bigtiff ? 43 : 42, 2);
  if (f.bigtiff) {
    w.put(8, 2);
    w.put(0, 2);
  }
  const std::size_t first_field = w.out.size();
  w.put(0, w.offset_size());
  w.route_end = w.out.size();
  std::vector<std::pair<std::uint64_t, std::size_t>> tables;
  for (std::size_t i = 0; i < f.chain.size(); ++i) tables.push_back(w.write_ifd(f.chain[i], true, i == 0));
  if (!tables.empty()) {
    w.put_at(first_field, f.first_ifd.value_or(tables.front().first), w.offset_size());
    for (std::size_t i = 0; i + 1 < tables.size(); ++i) w.put_at(tables[i].second, tables[i + 1].first, w.offset_size());
    if (f.last_next_to) w.put_at(tables.back().second, tables.at(*f.last_next_to).first, w.offset_size());
    if (f.last_next) w.put_at(tables.back().second, *f.last_next, w.offset_size());
  }
  for (const auto& p : w.ref_patches) w.put_at(p.pos, tables.at(p.chain_index).first, p.size);
  w.align();
  tiff_layout layout;
  for (const auto& t : tables) layout.chain_tables.push_back(t.first);
  layout.sub_tables = w.sub_tables;
  layout.tables_end = w.out.size();
  layout.route_end = w.route_end;
  std::vector<std::vector<std::uint64_t>> seg_offsets(w.owners.size());
  for (std::size_t o = 0; o < w.owners.size(); ++o) {
    layout.last_data_begin = w.out.size();
    for (const auto& seg : w.owners[o]->segments) {
      seg_offsets[o].push_back(w.out.size());
      w.out.insert(w.out.end(), seg.begin(), seg.end());
      layout.last_data_end = w.out.size();
      w.align();
    }
  }
  for (const auto& p : w.patches) {
    const tiff_ifd& owner = *w.owners.at(p.owner);
    // Più elementi dichiarati che segmenti scritti: gli elementi in più restano al valore dichiarato.
    if (p.index >= owner.segments.size()) continue;
    w.put_at(p.pos, p.counts ? owner.segments[p.index].size() : seg_offsets[p.owner][p.index], p.size);
  }
  layout.bytes = std::move(w.out);
  return layout;
}

inline tiff_entry ent(std::uint16_t tag, std::uint16_t type, std::vector<std::uint64_t> items) {
  tiff_entry e;
  e.tag = tag;
  e.type = type;
  e.items = std::move(items);
  return e;
}

// IFD raw di un DNG con campioni interi (di norma Compression 8, SampleFormat 1, 32 bit) per il decoder del progetto.
// Campioni da un generatore a seme fisso, con 0 e il massimo sempre presenti (riga 0, colonne 0 e 1; massimo 2^24 a 32
// bit, 2^N − 1 a N < 32 bit); ogni segmento non
// compresso passa da `encode` (i test lo comprimono con zlib, oppure lo lasciano com'è per Compression 1 e 52546).
struct dng32_spec {
  std::uint32_t width = 32;
  std::uint32_t height = 32;
  std::uint16_t bits = 32;
  std::uint16_t compression = 8;
  std::uint16_t sample_format = 1;
  bool write_sample_format = true;
  std::uint16_t photometric = 32803;
  std::uint16_t samples_per_pixel = 1;
  std::uint32_t new_subfile_type = 0;
  bool write_new_subfile_type = true;
  bool tiles = true;
  std::uint32_t tile_width = 16;
  std::uint32_t tile_length = 16;
  std::uint32_t rows_per_strip = 8;
  std::array<std::uint8_t, 4> dng_version{1, 7, 1, 0};
  std::optional<std::array<std::uint8_t, 4>> backward_version = std::array<std::uint8_t, 4>{1, 4, 0, 0};
  std::array<std::uint8_t, 4> cfa{0, 1, 1, 2};
  dng_active_area active_area{2, 2, 30, 30};
  std::uint32_t white = 1u << 24;
  std::uint32_t seed = 0x13A0u;
};

inline std::uint32_t dng32_sample(const dng32_spec& s, std::uint32_t r, std::uint32_t c) {
  const std::uint32_t top = s.bits >= 32 ? (1u << 24) : ((1u << s.bits) - 1u);
  if (r == 0 && c == 0) return 0;
  if (r == 0 && c == 1) return top;
  std::uint32_t x = s.seed ^ (r * 0x9E3779B1u) ^ (c * 0x85EBCA77u);
  x ^= x >> 15;
  x *= 0x2C1B3C6Du;
  x ^= x >> 12;
  return x % (top + 1u);
}

// Segmenti non compressi nell'ordine dei byte del file: strisce di `rows_per_strip` righe (l'ultima con le righe
// rimanenti) o tasselli `tile_width × tile_length` con il riempimento di bordo dallo stesso generatore (TIFF 6.0 p. 19,
// p. 66-67).
inline std::vector<std::vector<std::uint8_t>> dng32_segments(const dng32_spec& s, tiff_order order) {
  const std::uint32_t bytes_per_sample = s.bits / 8u;
  const auto push = [&](std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (std::uint32_t i = 0; i < bytes_per_sample; ++i) {
      const std::uint32_t shift = order == tiff_order::ii ? i : bytes_per_sample - 1 - i;
      out.push_back(static_cast<std::uint8_t>(v >> (8 * shift)));
    }
  };
  std::vector<std::vector<std::uint8_t>> segs;
  if (s.tiles) {
    const std::uint32_t across = (s.width + s.tile_width - 1) / s.tile_width;
    const std::uint32_t down = (s.height + s.tile_length - 1) / s.tile_length;
    for (std::uint32_t ty = 0; ty < down; ++ty)
      for (std::uint32_t tx = 0; tx < across; ++tx) {
        std::vector<std::uint8_t> seg;
        for (std::uint32_t r = 0; r < s.tile_length; ++r)
          for (std::uint32_t c = 0; c < s.tile_width; ++c)
            for (std::uint16_t k = 0; k < s.samples_per_pixel; ++k)
              push(seg, dng32_sample(s, ty * s.tile_length + r, tx * s.tile_width + c));
        segs.push_back(std::move(seg));
      }
  } else {
    for (std::uint32_t r0 = 0; r0 < s.height; r0 += s.rows_per_strip) {
      std::vector<std::uint8_t> seg;
      for (std::uint32_t r = r0; r < std::min(s.height, r0 + s.rows_per_strip); ++r)
        for (std::uint32_t c = 0; c < s.width; ++c)
          for (std::uint16_t k = 0; k < s.samples_per_pixel; ++k) push(seg, dng32_sample(s, r, c));
      segs.push_back(std::move(seg));
    }
  }
  return segs;
}

template <typename Encode>
tiff_ifd make_dng32_ifd(const dng32_spec& s, tiff_order order, Encode encode) {
  using namespace tiff_type;
  tiff_ifd ifd;
  auto& e = ifd.entries;
  if (s.write_new_subfile_type) e.push_back(ent(254, long_, {s.new_subfile_type}));
  e.push_back(ent(256, long_, {s.width}));
  e.push_back(ent(257, long_, {s.height}));
  e.push_back(ent(258, short_, std::vector<std::uint64_t>(s.samples_per_pixel, s.bits)));
  e.push_back(ent(259, short_, {s.compression}));
  e.push_back(ent(262, short_, {s.photometric}));
  e.push_back(ent(277, short_, {s.samples_per_pixel}));
  e.push_back(ent(284, short_, {1}));
  e.push_back(ent(317, short_, {1}));
  if (s.write_sample_format)
    e.push_back(ent(339, short_, std::vector<std::uint64_t>(s.samples_per_pixel, s.sample_format)));
  for (auto& seg : dng32_segments(s, order)) ifd.segments.push_back(encode(seg));
  const std::vector<std::uint64_t> zeros(ifd.segments.size(), 0);
  if (s.tiles) {
    e.push_back(ent(322, long_, {s.tile_width}));
    e.push_back(ent(323, long_, {s.tile_length}));
    e.push_back({324, long_, zeros, {}, {}, true, false});
    e.push_back({325, long_, zeros, {}, {}, false, true});
  } else {
    e.push_back({273, long_, zeros, {}, {}, true, false});
    e.push_back(ent(278, long_, {s.rows_per_strip}));
    e.push_back({279, long_, zeros, {}, {}, false, true});
  }
  if (s.photometric == 32803) {
    e.push_back(ent(33421, short_, {2, 2}));
    e.push_back(ent(33422, byte_, {s.cfa[0], s.cfa[1], s.cfa[2], s.cfa[3]}));
  }
  e.push_back(ent(50706, byte_, {s.dng_version[0], s.dng_version[1], s.dng_version[2], s.dng_version[3]}));
  if (s.backward_version) {
    const auto& b = *s.backward_version;
    e.push_back(ent(50707, byte_, {b[0], b[1], b[2], b[3]}));
  }
  e.push_back(ent(50717, long_, {s.white}));
  e.push_back(ent(50829, long_, {s.active_area.top, s.active_area.left, s.active_area.bottom, s.active_area.right}));
  return ifd;
}

// Sostituisce la voce con lo stesso tag (o la aggiunge), toglie una voce, cerca una voce.
inline void set_entry(tiff_ifd& ifd, const tiff_entry& e) {
  for (tiff_entry& x : ifd.entries)
    if (x.tag == e.tag) {
      x = e;
      return;
    }
  ifd.entries.push_back(e);
}
inline void remove_entry(tiff_ifd& ifd, std::uint16_t tag) {
  std::vector<tiff_entry> kept;
  for (const tiff_entry& x : ifd.entries)
    if (x.tag != tag) kept.push_back(x);
  ifd.entries = std::move(kept);
}

}  // namespace astro_fixture
