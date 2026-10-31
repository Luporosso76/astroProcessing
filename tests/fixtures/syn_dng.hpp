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

}  // namespace astro_fixture
