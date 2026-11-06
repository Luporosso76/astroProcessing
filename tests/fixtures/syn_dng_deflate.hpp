#pragma once

// FIX-SYN-DNG-02, segmenti deflate dei DNG interi a 32 bit (docs/contracts/U13-dng-decoder.md §12). Il contenitore lo
// scrive syn_dng.hpp (`make_dng32_ifd`, `write_tiff`); qui si codificano i segmenti, senza codice di astrocore:
//   - Predictor 1 e 2: libtiff 4.7.2 (`TIFFClientOpen` in memoria, ADR-08.b §1), che applica da sé la differenza
//     orizzontale e l'ordine dei byte del file; i segmenti compressi si rileggono con `TIFFReadRawStrip`/`TIFFReadRawTile`.
//     È l'implementazione del predittore indipendente dal prodotto che conferma o smentisce H1 per il Predictor 2 (§13).
//   - Predictor 34892 e 34893: differenza con il campione 2 o 4 posizioni a sinistra, riga per riga del segmento, modulo
//     2^32 (DNG 1.7.1.0 p. 22), poi zlib (`compress2`, un flusso zlib completo per segmento).
//   - Ogni altro Predictor (3, 34894, 34895, usati solo per gli stop): campioni non trasformati, poi zlib.
// Un errore di libtiff o di zlib lancia std::runtime_error con il messaggio della libreria: la fixture non si costruisce.

#include <tiffio.h>
#include <zlib.h>

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "syn_dng.hpp"

namespace astro_fixture {

// Campioni di un segmento nell'ordine di riga, riempimento di bordo dei tasselli compreso (TIFF 6.0 p. 66-67).
struct dng32_segment {
  std::vector<std::uint32_t> values;
  std::uint32_t row_width = 0;
  std::uint32_t rows = 0;
};

// Segmenti di `s` (strisce dall'alto, ultima con le righe rimanenti; tasselli per righe di tasselli) dai valori di
// `dng32_sample`. `RowsPerStrip` oltre `height` (per esempio 2^32 − 1, predefinito del TIFF) dà una sola striscia.
inline std::vector<dng32_segment> dng32_segment_values(const dng32_spec& s) {
  std::vector<dng32_segment> out;
  if (s.tiles) {
    const std::uint32_t across = (s.width + s.tile_width - 1) / s.tile_width;
    const std::uint32_t down = (s.height + s.tile_length - 1) / s.tile_length;
    for (std::uint32_t ty = 0; ty < down; ++ty)
      for (std::uint32_t tx = 0; tx < across; ++tx) {
        dng32_segment seg{{}, s.tile_width, s.tile_length};
        seg.values.reserve(std::size_t{s.tile_width} * s.tile_length);
        for (std::uint32_t r = 0; r < s.tile_length; ++r)
          for (std::uint32_t c = 0; c < s.tile_width; ++c)
            seg.values.push_back(dng32_sample(s, ty * s.tile_length + r, tx * s.tile_width + c));
        out.push_back(std::move(seg));
      }
    return out;
  }
  for (std::uint64_t r0 = 0; r0 < s.height; r0 += s.rows_per_strip) {
    const std::uint32_t rows = static_cast<std::uint32_t>(std::min<std::uint64_t>(s.rows_per_strip, s.height - r0));
    dng32_segment seg{{}, s.width, rows};
    for (std::uint32_t r = 0; r < rows; ++r)
      for (std::uint32_t c = 0; c < s.width; ++c) seg.values.push_back(dng32_sample(s, static_cast<std::uint32_t>(r0) + r, c));
    out.push_back(std::move(seg));
  }
  return out;
}

// Predictor 34892 (step 2) e 34893 (step 4), DNG 1.7.1.0 p. 22: dalla colonna `step` di ogni riga si memorizza la
// differenza con il campione `step` posizioni a sinistra, modulo 2^32. Da destra a sinistra, sui valori originali.
inline void horizontal_difference(std::vector<std::uint32_t>& v, std::uint32_t row_width, std::uint32_t step) {
  for (std::size_t base = 0; base + row_width <= v.size(); base += row_width)
    for (std::uint32_t c = row_width; c-- > step;) v[base + c] -= v[base + c - step];
}

inline std::vector<std::uint8_t> to_file_bytes(const std::vector<std::uint32_t>& v, tiff_order order) {
  std::vector<std::uint8_t> out;
  out.reserve(v.size() * 4);
  for (const std::uint32_t x : v)
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(x >> (8 * (order == tiff_order::ii ? i : 3 - i))));
  return out;
}

inline std::vector<std::uint8_t> zlib_stream(const std::vector<std::uint8_t>& in, int level = 6) {
  uLongf n = compressBound(static_cast<uLong>(in.size()));
  std::vector<std::uint8_t> out(n);
  const int rc = compress2(out.data(), &n, in.data(), static_cast<uLong>(in.size()), level);
  if (rc != Z_OK) throw std::runtime_error("compress2 della fixture fallita: " + std::to_string(rc));
  out.resize(n);
  return out;
}

namespace libtiff_detail {

struct memory_file {
  std::vector<std::uint8_t> data;
  toff_t pos = 0;
};

inline tmsize_t read_proc(thandle_t h, void* buf, tmsize_t n) {
  auto* f = static_cast<memory_file*>(h);
  if (f->pos >= f->data.size()) return 0;
  const std::size_t k = std::min<std::size_t>(static_cast<std::size_t>(n), f->data.size() - f->pos);
  std::memcpy(buf, f->data.data() + f->pos, k);
  f->pos += k;
  return static_cast<tmsize_t>(k);
}
inline tmsize_t write_proc(thandle_t h, void* buf, tmsize_t n) {
  auto* f = static_cast<memory_file*>(h);
  const std::size_t k = static_cast<std::size_t>(n);
  if (f->pos + k > f->data.size()) f->data.resize(f->pos + k);
  std::memcpy(f->data.data() + f->pos, buf, k);
  f->pos += k;
  return n;
}
inline toff_t seek_proc(thandle_t h, toff_t off, int whence) {
  auto* f = static_cast<memory_file*>(h);
  toff_t base = 0;
  if (whence == SEEK_CUR) base = f->pos;
  if (whence == SEEK_END) base = f->data.size();
  f->pos = base + off;
  return f->pos;
}
inline int close_proc(thandle_t) { return 0; }
inline toff_t size_proc(thandle_t h) { return static_cast<memory_file*>(h)->data.size(); }
inline int map_proc(thandle_t, void**, toff_t*) { return 0; }
inline void unmap_proc(thandle_t, void*, toff_t) {}

// Messaggi di libtiff: errori e avvisi rendono la fixture non valida (nessun avviso ignorato).
inline std::string& diagnostics() {
  static thread_local std::string text;
  return text;
}
inline void collect(const char* module, const char* fmt, va_list ap) {
  char buf[512];
  std::vsnprintf(buf, sizeof buf, fmt, ap);
  diagnostics() += std::string(module != nullptr ? module : "libtiff") + ": " + buf + "; ";
}
inline void on_message(const char* module, const char* fmt, va_list ap) { collect(module, fmt, ap); }

struct handlers {
  TIFFErrorHandler error;
  TIFFErrorHandler warning;
  handlers() : error(TIFFSetErrorHandler(on_message)), warning(TIFFSetWarningHandler(on_message)) { diagnostics().clear(); }
  ~handlers() {
    TIFFSetErrorHandler(error);
    TIFFSetWarningHandler(warning);
  }
};

inline void require(bool ok, const char* what) {
  if (!ok || !diagnostics().empty()) throw std::runtime_error(std::string("libtiff, ") + what + ": " + diagnostics());
}
// Dopo un'operazione senza codice di ritorno (TIFFClose): nessun messaggio di errore o di avviso.
inline void require_no_messages(const char* what) { require(true, what); }

inline TIFF* open(memory_file& f, const char* mode) {
  return TIFFClientOpen("syn_dng_deflate", mode, &f, read_proc, write_proc, seek_proc, close_proc, size_proc, map_proc,
                        unmap_proc);
}

}  // namespace libtiff_detail

// Versione di libtiff che codifica i segmenti (third_party/libtiff, 4.7.2).
inline std::string libtiff_version() { return TIFFGetVersion(); }

// Segmenti deflate di `s` codificati da libtiff con Predictor `predictor` (1 o 2), nell'ordine dei byte `order`.
inline std::vector<std::vector<std::uint8_t>> libtiff_deflate_segments(const dng32_spec& s, tiff_order order,
                                                                       std::uint16_t predictor) {
  using namespace libtiff_detail;
  const handlers scope;
  const std::vector<dng32_segment> segs = dng32_segment_values(s);
  memory_file file;
  TIFF* w = open(file, order == tiff_order::ii ? "wl" : "wb");
  require(w != nullptr, "TIFFClientOpen in scrittura");
  bool ok = TIFFSetField(w, TIFFTAG_IMAGEWIDTH, s.width) == 1 && TIFFSetField(w, TIFFTAG_IMAGELENGTH, s.height) == 1 &&
            TIFFSetField(w, TIFFTAG_BITSPERSAMPLE, 32) == 1 && TIFFSetField(w, TIFFTAG_SAMPLESPERPIXEL, 1) == 1 &&
            TIFFSetField(w, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT) == 1 &&
            TIFFSetField(w, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK) == 1 &&
            TIFFSetField(w, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) == 1 &&
            TIFFSetField(w, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE) == 1 &&
            TIFFSetField(w, TIFFTAG_PREDICTOR, predictor) == 1;
  if (s.tiles) {
    ok = ok && TIFFSetField(w, TIFFTAG_TILEWIDTH, s.tile_width) == 1 &&
         TIFFSetField(w, TIFFTAG_TILELENGTH, s.tile_length) == 1;
  } else {
    ok = ok && TIFFSetField(w, TIFFTAG_ROWSPERSTRIP, std::min(s.rows_per_strip, s.height)) == 1;
  }
  if (!ok) TIFFClose(w);
  require(ok, "TIFFSetField");
  for (std::size_t k = 0; k < segs.size(); ++k) {
    // libtiff trasforma il buffer in posto (predittore, ordine dei byte): una copia per segmento.
    std::vector<std::uint32_t> native = segs[k].values;
    const tmsize_t n = static_cast<tmsize_t>(native.size() * 4);
    const tmsize_t rc = s.tiles ? TIFFWriteEncodedTile(w, static_cast<std::uint32_t>(k), native.data(), n)
                                : TIFFWriteEncodedStrip(w, static_cast<std::uint32_t>(k), native.data(), n);
    if (rc != n) TIFFClose(w);
    require(rc == n, "scrittura del segmento");
  }
  TIFFClose(w);
  require_no_messages("TIFFClose");

  file.pos = 0;
  TIFF* r = open(file, "r");
  require(r != nullptr, "TIFFClientOpen in lettura");
  const std::uint32_t count = s.tiles ? TIFFNumberOfTiles(r) : TIFFNumberOfStrips(r);
  std::uint64_t* byte_counts = nullptr;
  const bool have_counts = TIFFGetField(r, s.tiles ? TIFFTAG_TILEBYTECOUNTS : TIFFTAG_STRIPBYTECOUNTS, &byte_counts) == 1;
  if (count != segs.size() || !have_counts) TIFFClose(r);
  require(count == segs.size() && have_counts, "numero di segmenti riletti");
  std::vector<std::vector<std::uint8_t>> out;
  for (std::uint32_t k = 0; k < count; ++k) {
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(byte_counts[k]));
    const tmsize_t n = static_cast<tmsize_t>(raw.size());
    const tmsize_t rc = s.tiles ? TIFFReadRawTile(r, k, raw.data(), n) : TIFFReadRawStrip(r, k, raw.data(), n);
    if (rc != n) TIFFClose(r);
    require(rc == n, "lettura grezza del segmento");
    out.push_back(std::move(raw));
  }
  TIFFClose(r);
  require_no_messages("TIFFClose in lettura");
  return out;
}

// Segmenti deflate di `s` secondo `s.predictor` (vedi l'intestazione del file).
inline std::vector<std::vector<std::uint8_t>> deflate_segments(const dng32_spec& s, tiff_order order) {
  if (s.predictor == 1 || s.predictor == 2) return libtiff_deflate_segments(s, order, s.predictor);
  std::vector<std::vector<std::uint8_t>> out;
  for (dng32_segment& seg : dng32_segment_values(s)) {
    if (s.predictor == 34892 || s.predictor == 34893)
      horizontal_difference(seg.values, seg.row_width, s.predictor == 34892 ? 2 : 4);
    out.push_back(zlib_stream(to_file_bytes(seg.values, order)));
  }
  return out;
}

// IFD raw deflate di `s` (Compression 8), con i segmenti di `deflate_segments` o quelli dati.
inline tiff_ifd make_deflate_ifd(const dng32_spec& s, tiff_order order,
                                 std::vector<std::vector<std::uint8_t>> segments) {
  std::size_t next = 0;
  tiff_ifd ifd = make_dng32_ifd(s, order, [&](const std::vector<std::uint8_t>&) { return segments.at(next++); });
  if (next != segments.size()) throw std::runtime_error("numero di segmenti della fixture diverso da quello del contenitore");
  return ifd;
}
inline tiff_ifd make_deflate_ifd(const dng32_spec& s, tiff_order order) {
  return make_deflate_ifd(s, order, deflate_segments(s, order));
}

// DNG con il solo IFD0 raw deflate; `mutate` modifica l'IFD prima della scrittura.
inline std::vector<std::uint8_t> make_deflate_dng(const dng32_spec& s, tiff_order order = tiff_order::ii,
                                                  const std::function<void(tiff_ifd&)>& mutate = {}) {
  tiff_file f;
  f.order = order;
  f.chain.push_back(make_deflate_ifd(s, order));
  if (mutate) mutate(f.chain.front());
  return write_tiff(f).bytes;
}

}  // namespace astro_fixture
