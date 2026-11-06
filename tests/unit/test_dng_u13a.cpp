// U13a (M3.a): instradamento fra LibRaw e decoder DNG del progetto, parser DNG e stop sui tag. T-U13-01, T-U13-02,
// T-U13-06 (docs/contracts/U13-dng-decoder.md §3, §4, §6, §8, §12; plans/M3a.md, U13).
// Fixture FIX-SYN-DNG-02: DNG scritti da tests/fixtures/syn_dng.hpp (`write_tiff`, `make_dng32_ifd`), indipendente da
// astrocore e da LibRaw; segmenti deflate compressi qui con zlib (`compress2`, flusso zlib completo come chiede la nota
// tecnica Adobe del contratto §1). Valori attesi: dal contratto (decoder, categoria, codice) o dalla fixture (campi
// scritti); l'esito di LibRaw sui BigTIFF è quello misurato sulla LibRaw del progetto (§13: «esito di LibRaw registrato»).
// Tolleranze: nessuna, tutti i criteri sono binari.
//
// API presupposta (nomi proposti dal contratto §3 o scelti dall'autore dei test, da riportare nel codice):
//   astro/raw_decode.hpp
//     enum class raw_decoder_kind { libraw, astro_dng };
//     struct dng_route_result {
//       raw_decoder_kind decoder;
//       std::string reason;  // per libraw: "not_tiff", "tiff_not_dng", "dng_libraw_compression"; per astro_dng: vuoto
//       // Campi dell'IFD raw scelto (§8, «Instradamento»): presenti solo per un DNG con un IFD raw.
//       std::optional<std::uint32_t> compression, sample_format, bits_per_sample;
//     };
//     dng_route_result dng_route(std::span<const std::uint8_t> bytes);  // funzione pura; stop di §3 punti 5-6
//     raw_decode_hooks::after_route    std::function<void(const dng_route_result&)>: decoder scelto, una volta per decodifica
//     raw_decode_hooks::inflate_return std::function<int(std::size_t segment, int rc)>: codice di `inflate` del segmento,
//                                      restituisce quello su cui il prodotto decide (come `libraw_return`)
//   Contesto degli stop del decoder del progetto (§8, provenance dell'arresto): `decoder` = astro_dng, `compression`,
//   `sample_format`, `bits_per_sample` (chiavi in raw_record_keys.hpp, interi decimali). Stop di struttura (§3 punto 6,
//   §4): `funzione` (`dng_route` per le parti lette dall'instradamento) e `tag` (numero decimale) oppure `offset`.
//   Stop su Compression 52546 (§6): anche `photometric_interpretation` e `samples_per_pixel`. Stop di `inflate` (§5):
//   `funzione` = inflate, `codice` (valore restituito), `segmento` (indice).
// Contatori: `open_buffer` dal gancio esistente `libraw_return`, `inflate` da `inflate_return`.
#include <gtest/gtest.h>
#include <libraw/libraw_const.h>
#include <zlib.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "astro/raw_import_job.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "raw_record_keys.hpp"
#include "syn_dng.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;
namespace rk = raw_record;
namespace tt = astro_fixture::tiff_type;

using bytes_t = std::vector<std::uint8_t>;

bytes_t deflate(const bytes_t& in) {
  uLongf n = compressBound(static_cast<uLong>(in.size()));
  bytes_t out(n);
  const int rc = compress2(out.data(), &n, in.data(), static_cast<uLong>(in.size()), 6);
  if (rc != Z_OK) {
    ADD_FAILURE() << "compress2 della fixture fallita: " << rc;
    return {};
  }
  out.resize(n);
  return out;
}
bytes_t stored(const bytes_t& in) { return in; }

// DNG con il solo IFD0 raw della fixture; Compression 8 compressa, le altre lasciate come scritte.
bytes_t one_ifd(const fx::dng32_spec& s, fx::tiff_order order = fx::tiff_order::ii, bool bigtiff = false,
                const std::function<void(fx::tiff_ifd&)>& mutate = {}) {
  fx::tiff_ifd ifd = s.compression == 8 ? fx::make_dng32_ifd(s, order, deflate) : fx::make_dng32_ifd(s, order, stored);
  if (mutate) mutate(ifd);
  fx::tiff_file f;
  f.order = order;
  f.bigtiff = bigtiff;
  f.chain.push_back(std::move(ifd));
  return fx::write_tiff(f).bytes;
}

fx::dng32_spec spec16(std::uint16_t compression) {
  fx::dng32_spec s;
  s.bits = 16;
  s.compression = compression;
  s.white = 65535;
  if (compression == 52546) s.backward_version = std::array<std::uint8_t, 4>{1, 7, 0, 0};  // p. 125
  return s;
}

// IFD0 anteprima (NewSubFileType 1, RGB 8 bit 4 × 4) con DNGVersion e DNGBackwardVersion: struttura dei DNG con l'IFD raw
// in `SubIFDs`.
fx::tiff_ifd preview_ifd0() {
  fx::tiff_ifd p;
  p.entries = {fx::ent(254, tt::long_, {1}),        fx::ent(256, tt::long_, {4}),       fx::ent(257, tt::long_, {4}),
               fx::ent(258, tt::short_, {8, 8, 8}), fx::ent(259, tt::short_, {1}),      fx::ent(262, tt::short_, {2}),
               fx::ent(277, tt::short_, {3}),       fx::ent(278, tt::long_, {4}),       fx::ent(284, tt::short_, {1}),
               fx::ent(50706, tt::byte_, {1, 7, 1, 0}), fx::ent(50707, tt::byte_, {1, 4, 0, 0})};
  fx::tiff_entry so = fx::ent(273, tt::long_, {0});
  so.segment_offsets = true;
  fx::tiff_entry sc = fx::ent(279, tt::long_, {0});
  sc.segment_counts = true;
  p.entries.push_back(so);
  p.entries.push_back(sc);
  p.segments = {bytes_t(48, 0x40)};
  return p;
}

// Miniatura di catena (IFD1) senza NewSubFileType e senza segmenti: per §3 punto 3 non è un IFD raw.
fx::tiff_ifd chain_thumbnail(std::optional<std::uint32_t> new_subfile_type = std::nullopt) {
  fx::tiff_ifd t;
  if (new_subfile_type) t.entries.push_back(fx::ent(254, tt::long_, {*new_subfile_type}));
  t.entries.push_back(fx::ent(256, tt::long_, {4}));
  t.entries.push_back(fx::ent(257, tt::long_, {4}));
  t.entries.push_back(fx::ent(258, tt::short_, {16}));
  t.entries.push_back(fx::ent(259, tt::short_, {1}));
  t.entries.push_back(fx::ent(262, tt::short_, {32803}));
  t.entries.push_back(fx::ent(277, tt::short_, {1}));
  return t;
}

// IFD raw deflate a 32 bit in `SubIFDs`: DNGVersion sta in IFD0.
fx::tiff_ifd sub_raw(const fx::dng32_spec& s = {}) {
  fx::tiff_ifd raw = fx::make_dng32_ifd(s, fx::tiff_order::ii, deflate);
  fx::remove_entry(raw, 50706);
  fx::remove_entry(raw, 50707);
  return raw;
}

std::optional<std::string> ctx(const stop_error& e, std::string_view key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

struct observed {
  std::optional<image> img;
  std::optional<stop_error> stop;
  int open_buffer_calls = 0;
  int inflate_calls = 0;
  std::vector<dng_route_result> routes;
};

observed run(const bytes_t& bytes, const std::function<int(std::string_view, int)>& libraw_rc = {},
             const std::function<int(std::size_t, int)>& inflate_rc = {}) {
  observed o;
  raw_decode_hooks hooks;
  hooks.libraw_return = [&](std::string_view function, int rc) {
    if (function == "open_buffer") ++o.open_buffer_calls;
    return libraw_rc ? libraw_rc(function, rc) : rc;
  };
  hooks.inflate_return = [&](std::size_t segment, int rc) {
    ++o.inflate_calls;
    return inflate_rc ? inflate_rc(segment, rc) : rc;
  };
  hooks.after_route = [&](const dng_route_result& r) { o.routes.push_back(r); };
  try {
    o.img.emplace(decode_raw_buffer(bytes, hooks));
  } catch (const stop_error& e) {
    o.stop.emplace(e);
  }
  return o;
}

std::string describe(const observed& o) {
  if (o.stop) return o.stop->what();
  if (o.img) return "immagine " + std::to_string(o.img->geometry.width) + "x" + std::to_string(o.img->geometry.height);
  return "nessun esito";
}

const char* name_of(raw_decoder_kind k) { return k == raw_decoder_kind::libraw ? "libraw" : "astro_dng"; }

void expect_stop(const observed& o, error_category category, std::string_view code) {
  ASSERT_TRUE(o.stop.has_value()) << "atteso lo stop " << code << ", esito: " << describe(o);
  EXPECT_EQ(o.stop->category(), category) << o.stop->what();
  EXPECT_EQ(o.stop->code(), code) << o.stop->what();
  EXPECT_FALSE(o.img.has_value());
}

// §8: lo stop del decoder del progetto porta il decoder e i campi dell'instradamento scritti nella fixture.
void expect_route_context(const stop_error& e, std::uint32_t compression, std::uint32_t sample_format,
                          std::uint32_t bits) {
  EXPECT_EQ(ctx(e, rk::decoder), std::string(rk::decoder_astro_dng)) << e.what();
  EXPECT_EQ(ctx(e, rk::compression), std::to_string(compression)) << e.what();
  EXPECT_EQ(ctx(e, rk::sample_format), std::to_string(sample_format)) << e.what();
  EXPECT_EQ(ctx(e, rk::bits_per_sample), std::to_string(bits)) << e.what();
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-01: instradamento (§3), decoder scelto e zero chiamate all'altro.
// ---------------------------------------------------------------------------------------------------------------------

struct route_case {
  std::string name;
  bytes_t bytes;
  raw_decoder_kind decoder;
  std::string reason;  // vuoto per astro_dng
  std::optional<std::array<std::uint32_t, 3>> fields;  // Compression, SampleFormat, BitsPerSample dell'IFD raw scelto
};

std::vector<route_case> route_cases() {
  std::vector<route_case> c;
  {
    std::mt19937 rng(0x13A01u);
    bytes_t noise(4096);
    for (auto& b : noise) b = static_cast<std::uint8_t>(rng() & 0xFF);
    c.push_back({"byte casuali", noise, raw_decoder_kind::libraw, "not_tiff", std::nullopt});
  }
  {
    // ISO/IEC 18181-2: firma del contenitore JPEG XL (box `JXL `) seguita dal box `ftyp` (ISO BMFF).
    const bytes_t bmff = {0, 0, 0, 0x0C, 'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A, 0,   0,   0,   0x14,
                          'f', 't', 'y', 'p', 'j', 'x', 'l', ' ', 0,    0,    0,    0,    'j', 'x', 'l', ' '};
    c.push_back({"intestazione ISO BMFF", bmff, raw_decoder_kind::libraw, "not_tiff", std::nullopt});
  }
  c.push_back({"7 byte con intestazione TIFF", {'I', 'I', 42, 0, 8, 0, 0}, raw_decoder_kind::libraw, "not_tiff",
               std::nullopt});
  {
    fx::dng32_spec s = spec16(1);
    c.push_back({"TIFF senza DNGVersion", one_ifd(s, fx::tiff_order::ii, false,
                                                  [](fx::tiff_ifd& i) {
                                                    fx::remove_entry(i, 50706);
                                                    fx::remove_entry(i, 50707);
                                                  }),
                 raw_decoder_kind::libraw, "tiff_not_dng", std::nullopt});
  }
  for (std::uint16_t comp : {1, 7, 9}) {
    c.push_back({"DNG Compression " + std::to_string(comp), one_ifd(spec16(comp)), raw_decoder_kind::libraw,
                 "dng_libraw_compression", std::array<std::uint32_t, 3>{comp, 1, 16}});
  }
  {
    fx::dng32_spec s;
    s.sample_format = 3;
    c.push_back({"DNG Compression 8 in virgola mobile a 32 bit", one_ifd(s), raw_decoder_kind::libraw,
                 "dng_libraw_compression", std::array<std::uint32_t, 3>{8, 3, 32}});
  }
  for (const bool tiles : {true, false})
    for (const fx::tiff_order order : {fx::tiff_order::ii, fx::tiff_order::mm}) {
      fx::dng32_spec s;
      s.tiles = tiles;
      c.push_back({std::string("DNG deflate intero 32 bit, ") + (tiles ? "tasselli" : "strisce") +
                       (order == fx::tiff_order::ii ? ", II" : ", MM"),
                   one_ifd(s, order), raw_decoder_kind::astro_dng, "", std::array<std::uint32_t, 3>{8, 1, 32}});
    }
  {
    fx::dng32_spec s;
    s.write_sample_format = false;
    c.push_back({"DNG deflate 32 bit senza SampleFormat (predefinito 1)", one_ifd(s), raw_decoder_kind::astro_dng, "",
                 std::array<std::uint32_t, 3>{8, 1, 32}});
  }
  c.push_back({"DNG deflate intero 16 bit", one_ifd(spec16(8)), raw_decoder_kind::astro_dng, "",
               std::array<std::uint32_t, 3>{8, 1, 16}});
  c.push_back({"DNG JPEG XL CFA", one_ifd(spec16(52546)), raw_decoder_kind::astro_dng, "",
               std::array<std::uint32_t, 3>{52546, 1, 16}});
  {
    fx::dng32_spec s = spec16(52546);
    s.photometric = 34892;
    s.samples_per_pixel = 3;
    c.push_back({"DNG JPEG XL LinearRaw", one_ifd(s), raw_decoder_kind::astro_dng, "",
                 std::array<std::uint32_t, 3>{52546, 1, 16}});
  }
  {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate));
    f.chain.push_back(chain_thumbnail());
    c.push_back({"IFD1 di catena senza NewSubFileType accanto all'IFD raw deflate", fx::write_tiff(f).bytes,
                 raw_decoder_kind::astro_dng, "", std::array<std::uint32_t, 3>{8, 1, 32}});
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    ifd0.sub_ifds.push_back(sub_raw());
    f.chain.push_back(std::move(ifd0));
    c.push_back({"IFD0 anteprima (NewSubFileType 1) e IFD raw deflate in SubIFDs", fx::write_tiff(f).bytes,
                 raw_decoder_kind::astro_dng, "", std::array<std::uint32_t, 3>{8, 1, 32}});
  }
  return c;
}

void expect_route(const dng_route_result& r, const route_case& c) {
  EXPECT_EQ(name_of(r.decoder), std::string(name_of(c.decoder)));
  EXPECT_EQ(r.reason, c.reason);
  if (c.fields) {
    EXPECT_EQ(r.compression, std::optional<std::uint32_t>((*c.fields)[0]));
    EXPECT_EQ(r.sample_format, std::optional<std::uint32_t>((*c.fields)[1]));
    EXPECT_EQ(r.bits_per_sample, std::optional<std::uint32_t>((*c.fields)[2]));
  } else {
    EXPECT_FALSE(r.compression.has_value());
    EXPECT_FALSE(r.sample_format.has_value());
    EXPECT_FALSE(r.bits_per_sample.has_value());
  }
}

// Ogni caso della tabella: `dng_route` sceglie il decoder atteso; la decodifica lo sceglie una volta e non chiama l'altro.
TEST(DngRoute, T_U13_01_route_table_chooses_the_expected_decoder_and_never_calls_the_other) {
  for (const route_case& c : route_cases()) {
    SCOPED_TRACE(c.name);
    ASSERT_GE(c.bytes.size(), 4u);
    if (c.name == "byte casuali") {
      const bool tiff_magic = (c.bytes[0] == 'I' && c.bytes[1] == 'I') || (c.bytes[0] == 'M' && c.bytes[1] == 'M');
      ASSERT_FALSE(tiff_magic) << "il seme deve dare byte senza intestazione TIFF";
    }
    expect_route(dng_route(c.bytes), c);
    const observed o = run(c.bytes);
    ASSERT_EQ(o.routes.size(), 1u) << describe(o);
    expect_route(o.routes.front(), c);
    if (c.decoder == raw_decoder_kind::libraw) {
      EXPECT_EQ(o.open_buffer_calls, 1) << describe(o);
      EXPECT_EQ(o.inflate_calls, 0) << describe(o);
    } else {
      EXPECT_EQ(o.open_buffer_calls, 0) << describe(o);
    }
  }
}

// Percorso LibRaw: gli esiti di ADR-08.b restano (§3 punto 5, «con gli esiti di ADR-08.b»).
TEST(DngRoute, T_U13_01_libraw_route_keeps_the_adr08_outcomes) {
  for (const fx::tiff_order order : {fx::tiff_order::ii, fx::tiff_order::mm}) {
    SCOPED_TRACE(order == fx::tiff_order::ii ? "Compression 1, II: immagine" : "Compression 1, MM: immagine");
    const observed o = run(one_ifd(spec16(1), order));
    EXPECT_TRUE(o.img.has_value()) << describe(o);
  }
  {
    // Esito attuale misurato dalla sonda (tmp/u13a-probe/probe-log.txt): LibRaw decodifica il file della fixture, i cui
    // segmenti non sono codificati in JPEG senza perdita.
    SCOPED_TRACE("Compression 7: immagine");
    const observed o = run(one_ifd(spec16(7)));
    EXPECT_TRUE(o.img.has_value()) << describe(o);
  }
  {
    SCOPED_TRACE("Compression 8 in virgola mobile: raw.float_data (ADR-08.b §2.3)");
    fx::dng32_spec s;
    s.sample_format = 3;
    const observed o = run(one_ifd(s));
    expect_stop(o, error_category::unsupported, "raw.float_data");
  }
  {
    SCOPED_TRACE("Compression 9 (VC-5): raw.file_unsupported da get_decoder_info (ADR-08.b §2.3)");
    const observed o = run(one_ifd(spec16(9)));
    expect_stop(o, error_category::unsupported, "raw.file_unsupported");
    if (o.stop) {
      EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("get_decoder_info"));
    }
  }
}

// DNG del perimetro del progetto fermati dal decoder del progetto: nessuna chiamata a LibRaw né a inflate (§4, §6).
TEST(DngRoute, T_U13_01_project_route_stops_without_libraw) {
  {
    SCOPED_TRACE("deflate intero 16 bit: raw.dng_nonconformant");
    const observed o = run(one_ifd(spec16(8)));
    expect_stop(o, error_category::unsupported, "raw.dng_nonconformant");
    EXPECT_EQ(o.open_buffer_calls, 0);
    EXPECT_EQ(o.inflate_calls, 0);
    if (o.stop) expect_route_context(*o.stop, 8, 1, 16);
  }
  {
    SCOPED_TRACE("JPEG XL CFA: raw.dng_nonconformant");
    const observed o = run(one_ifd(spec16(52546)));
    expect_stop(o, error_category::unsupported, "raw.dng_nonconformant");
    EXPECT_EQ(o.open_buffer_calls, 0);
    EXPECT_EQ(o.inflate_calls, 0);
  }
  {
    SCOPED_TRACE("JPEG XL LinearRaw: raw.not_cfa");
    fx::dng32_spec s = spec16(52546);
    s.photometric = 34892;
    s.samples_per_pixel = 3;
    const observed o = run(one_ifd(s));
    expect_stop(o, error_category::unsupported, "raw.not_cfa");
    EXPECT_EQ(o.open_buffer_calls, 0);
    EXPECT_EQ(o.inflate_calls, 0);
  }
}

// §3 punto 5: più IFD raw, almeno uno nell'insieme del progetto: stop raw.multi_image dall'instradamento.
TEST(DngRoute, T_U13_01_two_raw_ifds_with_one_in_the_project_set_stop_with_multi_image) {
  std::vector<std::pair<std::string, bytes_t>> files;
  {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate));
    fx::tiff_ifd second = fx::make_dng32_ifd(spec16(1), fx::tiff_order::ii, stored);
    fx::remove_entry(second, 50706);
    fx::remove_entry(second, 50707);
    f.chain.push_back(std::move(second));
    files.emplace_back("IFD0 deflate e IFD1 di catena con NewSubFileType 0", fx::write_tiff(f).bytes);
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate);
    fx::dng32_spec plain = spec16(1);
    plain.write_new_subfile_type = false;  // in SubIFDs il tag assente vale 0
    fx::tiff_ifd sub = fx::make_dng32_ifd(plain, fx::tiff_order::ii, stored);
    fx::remove_entry(sub, 50706);
    fx::remove_entry(sub, 50707);
    ifd0.sub_ifds.push_back(std::move(sub));
    f.chain.push_back(std::move(ifd0));
    files.emplace_back("IFD0 deflate e SubIFD non compresso senza NewSubFileType", fx::write_tiff(f).bytes);
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = fx::make_dng32_ifd(spec16(1), fx::tiff_order::ii, stored);
    ifd0.sub_ifds.push_back(sub_raw());
    f.chain.push_back(std::move(ifd0));
    files.emplace_back("IFD0 non compresso e SubIFD deflate", fx::write_tiff(f).bytes);
  }
  for (const auto& [name, bytes] : files) {
    SCOPED_TRACE(name);
    try {
      (void)dng_route(bytes);
      ADD_FAILURE() << "dng_route: atteso lo stop raw.multi_image";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::unsupported) << e.what();
      EXPECT_EQ(e.code(), "raw.multi_image") << e.what();
    }
    const observed o = run(bytes);
    expect_stop(o, error_category::unsupported, "raw.multi_image");
    EXPECT_EQ(o.open_buffer_calls, 0);
    EXPECT_EQ(o.inflate_calls, 0);
  }
}

// §3 punto 1 e §13: un BigTIFF va a LibRaw (motivo not_tiff). Esito di LibRaw 0.22.3 registrato con la sonda dell'autore
// dei test (tmp/u13a-probe/, 5 novembre 2026): `open_buffer` restituisce LIBRAW_FILE_UNSUPPORTED per ogni BigTIFF (deflate
// intero a 16 e a 32 bit, JPEG XL CFA): unsupported, raw.file_unsupported, funzione open_buffer.
TEST(DngRoute, T_U13_01_bigtiff_goes_to_libraw_with_the_recorded_libraw_outcome) {
  struct file_case {
    std::string name;
    std::string property;  // nome della proprietà gtest con l'esito registrato
    bytes_t bytes;
  };
  std::vector<file_case> files;
  files.push_back({"BigTIFF deflate intero 16 bit", "libraw_bigtiff_deflate16", one_ifd(spec16(8), fx::tiff_order::ii, true)});
  files.push_back(
      {"BigTIFF deflate intero 32 bit", "libraw_bigtiff_deflate32", one_ifd(fx::dng32_spec{}, fx::tiff_order::ii, true)});
  files.push_back({"BigTIFF JPEG XL CFA", "libraw_bigtiff_jxl_cfa", one_ifd(spec16(52546), fx::tiff_order::ii, true)});
  for (const auto& [name, property, bytes] : files) {
    SCOPED_TRACE(name);
    const dng_route_result r = dng_route(bytes);
    EXPECT_EQ(r.decoder, raw_decoder_kind::libraw);
    EXPECT_EQ(r.reason, "not_tiff");
    const observed o = run(bytes);
    EXPECT_EQ(o.open_buffer_calls, 1);
    EXPECT_EQ(o.inflate_calls, 0);
    expect_stop(o, error_category::unsupported, "raw.file_unsupported");
    if (o.stop) {
      EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("open_buffer"));
      EXPECT_EQ(ctx(*o.stop, "valore"), std::to_string(static_cast<int>(LIBRAW_FILE_UNSUPPORTED)));
      ::testing::Test::RecordProperty(property, o.stop->what());
    }
  }
}

// Guasto iniettato in inflate su un file del progetto: stop col codice del guasto, nessuna chiamata a LibRaw.
TEST(DngRoute, T_U13_01_fault_in_inflate_stays_on_the_project_decoder) {
  for (const bool tiles : {true, false}) {
    SCOPED_TRACE(tiles ? "tasselli" : "strisce");
    fx::dng32_spec s;
    s.tiles = tiles;
    const observed o = run(one_ifd(s), {}, [](std::size_t segment, int rc) { return segment == 0 ? Z_DATA_ERROR : rc; });
    expect_stop(o, error_category::integrity, "raw.data_error");
    EXPECT_EQ(o.open_buffer_calls, 0);
    EXPECT_EQ(o.inflate_calls, 1) << "nessun segmento dopo il guasto";
    if (o.stop) {
      EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("inflate"));
      EXPECT_EQ(ctx(*o.stop, "codice"), std::to_string(Z_DATA_ERROR));
      EXPECT_EQ(ctx(*o.stop, "segmento"), std::string("0"));
    }
  }
}

// Codice di ritorno iniettato in open_buffer su un file LibRaw: stop della tabella di ADR-08.b §2.5, nessun inflate.
TEST(DngRoute, T_U13_01_fault_in_open_buffer_stays_on_libraw) {
  const observed o = run(one_ifd(spec16(1)), [](std::string_view f, int rc) {
    return f == "open_buffer" ? static_cast<int>(LIBRAW_DATA_ERROR) : rc;
  });
  expect_stop(o, error_category::integrity, "raw.data_error");
  EXPECT_EQ(o.open_buffer_calls, 1);
  EXPECT_EQ(o.inflate_calls, 0);
  ASSERT_EQ(o.routes.size(), 1u);
  EXPECT_EQ(o.routes.front().decoder, raw_decoder_kind::libraw);
  if (o.stop) {
    EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("open_buffer"));
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-02: strutture invalide. integrity raw.data_error, contesto con funzione e tag; nessuna immagine; zero chiamate
// a LibRaw (e a inflate: i controlli dei tag precedono i segmenti, §4).
// ---------------------------------------------------------------------------------------------------------------------

void expect_structure_stop(const observed& o, const std::set<std::string>& tags, bool route_function) {
  expect_stop(o, error_category::integrity, "raw.data_error");
  EXPECT_EQ(o.open_buffer_calls, 0);
  EXPECT_EQ(o.inflate_calls, 0);
  if (!o.stop) return;
  const auto function = ctx(*o.stop, "funzione");
  ASSERT_TRUE(function.has_value() && !function->empty()) << o.stop->what();
  if (route_function) {
    EXPECT_EQ(*function, "dng_route") << o.stop->what();
  }
  const auto tag = ctx(*o.stop, "tag");
  if (!tags.empty()) {
    ASSERT_TRUE(tag.has_value()) << o.stop->what();
    EXPECT_EQ(tags.count(*tag), 1u) << "tag " << *tag << ": " << o.stop->what();
  } else {
    EXPECT_TRUE(tag.has_value() || ctx(*o.stop, "offset").has_value()) << "contesto senza tag né offset: "
                                                                        << o.stop->what();
  }
}

// Fixture del troncamento: IFD0 anteprima con DNGVersion, SubIFD raw deflate a tasselli (offset e conteggi fuori riga),
// IFD1 di catena senza NewSubFileType. Dati: anteprima, poi i tasselli dell'IFD raw (ultimi).
fx::tiff_layout truncation_fixture() {
  fx::tiff_file f;
  fx::tiff_ifd ifd0 = preview_ifd0();
  ifd0.sub_ifds.push_back(sub_raw());
  f.chain.push_back(std::move(ifd0));
  f.chain.push_back(chain_thumbnail());
  return fx::write_tiff(f);
}

TEST(DngStructure, T_U13_02_truncation_at_every_byte_of_the_tables_and_in_the_data) {
  const fx::tiff_layout full = truncation_fixture();
  ASSERT_EQ(dng_route(full.bytes).decoder, raw_decoder_kind::astro_dng) << "la fixture intera va al decoder del progetto";
  ASSERT_GT(full.route_end, 8u);
  ASSERT_LE(full.route_end, full.tables_end);
  ASSERT_LT(full.last_data_begin, full.last_data_end);
  ASSERT_LE(full.last_data_end, full.bytes.size());
  // Sotto 8 byte: LibRaw (§3 punto 1), senza errore dell'instradamento.
  for (std::size_t n = 0; n < 8; ++n) {
    SCOPED_TRACE("lunghezza " + std::to_string(n));
    const bytes_t cut(full.bytes.begin(), full.bytes.begin() + static_cast<std::ptrdiff_t>(n));
    EXPECT_EQ(dng_route(cut).decoder, raw_decoder_kind::libraw);
  }
  std::vector<std::size_t> lengths;
  for (std::size_t n = 8; n < full.tables_end; ++n) lengths.push_back(n);
  const std::size_t data = full.last_data_end - full.last_data_begin;
  for (std::size_t n : {full.tables_end, full.last_data_begin, full.last_data_begin + 1, full.last_data_begin + data / 3,
                        full.last_data_begin + data / 2, full.last_data_end - 1})
    lengths.push_back(n);
  for (const std::size_t n : lengths) {
    SCOPED_TRACE("lunghezza " + std::to_string(n) + " di " + std::to_string(full.bytes.size()) + " (tabelle fino a " +
                 std::to_string(full.tables_end) + ", instradamento fino a " + std::to_string(full.route_end) + ")");
    const bytes_t cut(full.bytes.begin(), full.bytes.begin() + static_cast<std::ptrdiff_t>(n));
    const bool in_route = n < full.route_end;
    if (in_route) {
      try {
        (void)dng_route(cut);
        ADD_FAILURE() << "dng_route: atteso raw.data_error";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::integrity) << e.what();
        EXPECT_EQ(e.code(), "raw.data_error") << e.what();
        EXPECT_EQ(ctx(e, "funzione"), std::string("dng_route")) << e.what();
      }
    }
    // Nei dati: segmento oltre il buffer (riga «Segmenti» di §4), tag degli offset o dei conteggi dei tasselli.
    const std::set<std::string> tags = n >= full.tables_end ? std::set<std::string>{"324", "325"} : std::set<std::string>{};
    const observed o = run(cut);
    expect_structure_stop(o, tags, in_route);
    if (::testing::Test::HasFailure()) return;  // il primo troncamento che fallisce basta a mostrare il difetto
  }
}

struct structure_case {
  std::string name;
  bytes_t bytes;
  std::set<std::string> tags;  // vuoto: basta `tag` o `offset` nel contesto
  bool route_function;         // parte letta dall'instradamento: funzione dng_route
};

std::vector<std::uint64_t> srationals(std::size_t n, std::int32_t first_numerator) {
  std::vector<std::uint64_t> v;
  for (std::size_t k = 0; k < n; ++k) {
    v.push_back(static_cast<std::uint32_t>(k == 0 ? first_numerator : 0));
    v.push_back(1);
  }
  return v;
}

fx::tiff_entry with_count(fx::tiff_entry e, std::uint64_t count) {
  e.count = count;
  return e;
}

std::vector<structure_case> structure_cases() {
  std::vector<structure_case> c;
  const fx::dng32_spec tiles;
  fx::dng32_spec strips;
  strips.tiles = false;
  const auto mutated = [](const fx::dng32_spec& s, const std::function<void(fx::tiff_ifd&)>& m) {
    return one_ifd(s, fx::tiff_order::ii, false, m);
  };
  {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(tiles, fx::tiff_order::ii, deflate));
    f.first_ifd = 1u << 20;
    c.push_back({"offset di IFD0 oltre il buffer", fx::write_tiff(f).bytes, {}, true});
  }
  {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(tiles, fx::tiff_order::ii, deflate));
    f.last_next = 1u << 20;
    c.push_back({"offset del prossimo IFD della catena oltre il buffer", fx::write_tiff(f).bytes, {}, true});
  }
  {
    // SubIFDs con due offset (8 byte, fuori riga) il cui offset dei valori è oltre il buffer.
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    fx::tiff_entry sub = fx::ent(330, tt::long_, {0, 0});
    sub.value_offset = 1u << 20;
    ifd0.entries.push_back(sub);
    f.chain.push_back(std::move(ifd0));
    c.push_back({"SubIFDs con offset dei valori oltre il buffer", fx::write_tiff(f).bytes, {"330"}, true});
  }
  c.push_back({"BitsPerSample con conteggio oltre 2^32 byte", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, with_count(fx::ent(258, tt::short_, {32}), 0x80000001u));
               }),
               {"258"}, true});
  c.push_back({"StripOffsets con conteggio oltre 2^32 byte", mutated(strips, [](fx::tiff_ifd& i) {
                 fx::tiff_entry e = fx::ent(273, tt::long_, {0, 0, 0, 0});
                 e.count = 0x40000001u;
                 fx::set_entry(i, e);
               }),
               {"273"}, false});
  c.push_back({"TileOffsets con offset dei valori oltre il buffer", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::tiff_entry e = fx::ent(324, tt::long_, {0, 0, 0, 0});
                 e.value_offset = 1u << 20;
                 fx::set_entry(i, e);
               }),
               {"324"}, false});
  for (const std::size_t target : {std::size_t{0}, std::size_t{1}}) {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(tiles, fx::tiff_order::ii, deflate));
    f.chain.push_back(chain_thumbnail());
    f.last_next_to = target;
    c.push_back({target == 0 ? "ciclo nella catena (IFD1 → IFD0)" : "ciclo nella catena (IFD1 → IFD1)",
                 fx::write_tiff(f).bytes, {}, true});
  }
  c.push_back({"DNGVersion SHORT[4]", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(50706, tt::short_, {1, 7, 1, 0}));
               }),
               {"50706"}, true});
  c.push_back({"DNGVersion BYTE[3]", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(50706, tt::byte_, {1, 7, 1}));
               }),
               {"50706"}, true});
  c.push_back({"Compression di tipo ASCII", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(259, tt::ascii, {'8', 0}));
               }),
               {"259"}, true});
  c.push_back({"NewSubFileType di tipo BYTE", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(254, tt::byte_, {0}));
               }),
               {"254"}, true});
  c.push_back({"numero di tasselli errato (3 di 4)", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::tiff_entry o = fx::ent(324, tt::long_, {0, 0, 0});
                 o.segment_offsets = true;
                 fx::tiff_entry n = fx::ent(325, tt::long_, {0, 0, 0});
                 n.segment_counts = true;
                 fx::set_entry(i, o);
                 fx::set_entry(i, n);
               }),
               {"324", "325"}, false});
  c.push_back({"numero di strisce errato (3 di 4)", mutated(strips, [](fx::tiff_ifd& i) {
                 fx::tiff_entry o = fx::ent(273, tt::long_, {0, 0, 0});
                 o.segment_offsets = true;
                 fx::tiff_entry n = fx::ent(279, tt::long_, {0, 0, 0});
                 n.segment_counts = true;
                 fx::set_entry(i, o);
                 fx::set_entry(i, n);
               }),
               {"273", "279"}, false});
  c.push_back({"strisce e tasselli insieme", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::tiff_entry o = fx::ent(273, tt::long_, {0, 0, 0, 0});
                 o.segment_offsets = true;
                 fx::tiff_entry n = fx::ent(279, tt::long_, {0, 0, 0, 0});
                 n.segment_counts = true;
                 fx::set_entry(i, o);
                 fx::set_entry(i, fx::ent(278, tt::long_, {8}));
                 fx::set_entry(i, n);
               }),
               {}, false});
  c.push_back({"ImageWidth assente", mutated(tiles, [](fx::tiff_ifd& i) { fx::remove_entry(i, 256); }), {"256"}, false});
  c.push_back({"ImageLength 0", mutated(tiles, [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(257, tt::long_, {0})); }),
               {"257"}, false});
  c.push_back({"RowsPerStrip 0", mutated(strips, [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(278, tt::long_, {0})); }),
               {"278"}, false});
  c.push_back({"TileWidth 0", mutated(tiles, [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(322, tt::long_, {0})); }),
               {"322"}, false});
  c.push_back({"TileLength 0", mutated(tiles, [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(323, tt::long_, {0})); }),
               {"323"}, false});
  c.push_back({"nessun tag di strisce né di tasselli", mutated(tiles, [](fx::tiff_ifd& i) {
                 for (std::uint16_t t : {322, 323, 324, 325}) fx::remove_entry(i, t);
               }),
               {}, false});
  c.push_back({"TileOffsets senza TileByteCounts", mutated(tiles, [](fx::tiff_ifd& i) { fx::remove_entry(i, 325); }),
               {"325"}, false});
  c.push_back({"WhiteLevel con 2 valori", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(50717, tt::long_, {1u << 24, 1u << 24}));
               }),
               {"50717"}, false});
  c.push_back({"LinearizationTable con 0 voci", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(50712, tt::short_, {}));
               }),
               {"50712"}, false});
  c.push_back({"LinearizationTable con 65 537 voci", mutated(tiles, [](fx::tiff_ifd& i) {
                 std::vector<std::uint64_t> v(65537);
                 for (std::size_t k = 0; k < v.size(); ++k) v[k] = k & 0xFFFF;
                 fx::set_entry(i, fx::ent(50712, tt::short_, v));
               }),
               {"50712"}, false});
  c.push_back({"LinearizationTable di tipo LONG", mutated(tiles, [](fx::tiff_ifd& i) {
                 std::vector<std::uint64_t> v(16);
                 for (std::size_t k = 0; k < v.size(); ++k) v[k] = k * 1000;
                 fx::set_entry(i, fx::ent(50712, tt::long_, v));
               }),
               {"50712"}, false});
  c.push_back({"BlackLevel con 3 valori per BlackLevelRepeatDim 2 × 2", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(50713, tt::short_, {2, 2}));
                 fx::set_entry(i, fx::ent(50714, tt::short_, {10, 11, 12}));
               }),
               {"50714"}, false});
  c.push_back({"BlackLevelRepeatDim 65535 × 65535 con BlackLevel di 65535² valori", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(50713, tt::short_, {65535, 65535}));
                 fx::set_entry(i, with_count(fx::ent(50714, tt::short_, {0}), 65535ull * 65535ull));
               }),
               {"50714"}, false});
  // Tag ripetuto nella tabella (U13 §4, errore di struttura): letto dall'instradamento (IFD0) o dal parser dei tag.
  for (const std::uint16_t tag : {254, 259, 50706}) {
    c.push_back({"tag " + std::to_string(tag) + " ripetuto in IFD0", mutated(tiles, [tag](fx::tiff_ifd& i) {
                   for (const fx::tiff_entry& e : std::vector<fx::tiff_entry>(i.entries))
                     if (e.tag == tag) i.entries.push_back(e);
                 }),
                 {std::to_string(tag)}, true});
  }
  for (const std::uint16_t tag : {256, 33421}) {
    c.push_back({"tag " + std::to_string(tag) + " ripetuto nell'IFD raw", mutated(tiles, [tag](fx::tiff_ifd& i) {
                   for (const fx::tiff_entry& e : std::vector<fx::tiff_entry>(i.entries))
                     if (e.tag == tag) i.entries.push_back(e);
                 }),
                 {std::to_string(tag)}, false});
  }
  // SamplesPerPixel è 1: BitsPerSample e SampleFormat hanno un solo valore.
  c.push_back({"BitsPerSample con 2 valori", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(258, tt::short_, {32, 32}));
               }),
               {"258"}, false});
  c.push_back({"SampleFormat con 2 valori", mutated(tiles, [](fx::tiff_ifd& i) {
                 fx::set_entry(i, fx::ent(339, tt::short_, {1, 1}));
               }),
               {"339"}, false});
  for (const std::vector<std::uint64_t>& cell : {std::vector<std::uint64_t>{0, 1, 2}, {0, 1, 1, 2, 0}}) {
    c.push_back({"CFAPattern con " + std::to_string(cell.size()) + " voci per 2 × 2", mutated(tiles, [cell](fx::tiff_ifd& i) {
                   fx::set_entry(i, fx::ent(33422, tt::byte_, cell));
                 }),
                 {"33422"}, false});
  }
  // DefaultScale: il denominatore nullo si ferma prima del confronto (con {1/0, 1/0} il prodotto incrociato è uguale).
  for (const std::vector<std::uint64_t>& v : {std::vector<std::uint64_t>{1, 0, 1, 0}, {1, 1, 1, 0}, {1, 0, 1, 1}}) {
    c.push_back({"DefaultScale con denominatore nullo (" + std::to_string(v[1]) + ", " + std::to_string(v[3]) + ")",
                 mutated(tiles, [v](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(50718, tt::rational, v)); }),
                 {"50718"}, false});
  }
  // BlackLevelRepeatDim con 0: senza BlackLevel il prodotto R · C nullo non farebbe fallire altro.
  for (const std::vector<std::uint64_t>& v : {std::vector<std::uint64_t>{0, 2}, {2, 0}, {0, 0}}) {
    c.push_back({"BlackLevelRepeatDim " + std::to_string(v[0]) + " × " + std::to_string(v[1]) + " senza BlackLevel",
                 mutated(tiles, [v](fx::tiff_ifd& i) {
                   fx::remove_entry(i, 50714);
                   fx::set_entry(i, fx::ent(50713, tt::short_, v));
                 }),
                 {"50713"}, false});
  }
  // BlackLevelDeltaH: conteggio = larghezza di ActiveArea (28, non i 32 dell'immagine); BlackLevelDeltaV: altezza.
  for (const std::size_t n : {std::size_t{27}, std::size_t{29}, std::size_t{32}, std::size_t{1}}) {
    c.push_back({"BlackLevelDeltaH con " + std::to_string(n) + " valori", mutated(tiles, [n](fx::tiff_ifd& i) {
                   fx::set_entry(i, fx::ent(50715, tt::srational, srationals(n, 0)));
                 }),
                 {"50715"}, false});
    c.push_back({"BlackLevelDeltaV con " + std::to_string(n) + " valori", mutated(tiles, [n](fx::tiff_ifd& i) {
                   fx::set_entry(i, fx::ent(50716, tt::srational, srationals(n, 0)));
                 }),
                 {"50716"}, false});
  }
  for (const std::uint16_t tag : {50715, 50716}) {
    // Un numeratore non nullo all'indice 0 e un denominatore nullo all'indice 3: vale il denominatore (data_error).
    for (const std::size_t zero_at : {std::size_t{0}, std::size_t{3}, std::size_t{27}}) {
      c.push_back({"BlackLevelDelta " + std::to_string(tag) + " con denominatore nullo all'indice " + std::to_string(zero_at),
                   mutated(tiles, [tag, zero_at](fx::tiff_ifd& i) {
                     std::vector<std::uint64_t> v = srationals(28, 1);
                     v[2 * zero_at + 1] = 0;
                     fx::set_entry(i, fx::ent(tag, tt::srational, v));
                   }),
                   {std::to_string(tag)}, false});
    }
  }
  return c;
}

TEST(DngStructure, T_U13_02_invalid_structures_stop_with_data_error_and_no_libraw) {
  for (const structure_case& c : structure_cases()) {
    SCOPED_TRACE(c.name);
    if (c.route_function) {
      try {
        (void)dng_route(c.bytes);
        ADD_FAILURE() << "dng_route: atteso raw.data_error";
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::integrity) << e.what();
        EXPECT_EQ(e.code(), "raw.data_error") << e.what();
      }
    }
    expect_structure_stop(run(c.bytes), c.tags, c.route_function);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-06: stop di §4 e §6 su un DNG altrimenti valido, uno alla volta; ordine dei controlli; provenance dell'arresto.
// ---------------------------------------------------------------------------------------------------------------------

struct tag_case {
  std::string name;
  std::function<void(fx::dng32_spec&)> spec;
  std::function<void(fx::tiff_ifd&)> ifd;
  error_category category;
  std::string code;
};

bytes_t tag_case_file(const tag_case& c, fx::dng32_spec base = {}) {
  if (c.spec) c.spec(base);
  return one_ifd(base, fx::tiff_order::ii, false, c.ifd);
}

const auto unsupported_ = error_category::unsupported;
const auto integrity_ = error_category::integrity;

std::vector<tag_case> tag_cases() {
  using s_t = fx::dng32_spec;
  using i_t = fx::tiff_ifd;
  std::vector<tag_case> c;
  c.push_back({"DNGBackwardVersion 1.8.0.0", {}, [](i_t& i) { fx::set_entry(i, fx::ent(50707, tt::byte_, {1, 8, 0, 0})); },
               unsupported_, "raw.dng_version"});
  c.push_back({"DNGVersion 1.8.0.0 senza DNGBackwardVersion",
               [](s_t& s) {
                 s.dng_version = {1, 8, 0, 0};
                 s.backward_version.reset();
               },
               {}, unsupported_, "raw.dng_version"});
  c.push_back({"LinearRaw e SamplesPerPixel 3",
               [](s_t& s) {
                 s.photometric = 34892;
                 s.samples_per_pixel = 3;
               },
               {}, unsupported_, "raw.not_cfa"});
  c.push_back({"CFA con SamplesPerPixel 3", [](s_t& s) { s.samples_per_pixel = 3; }, {}, unsupported_, "raw.not_cfa"});
  c.push_back({"CFARepeatPatternDim 6 × 6", {},
               [](i_t& i) {
                 fx::set_entry(i, fx::ent(33421, tt::short_, {6, 6}));
                 fx::set_entry(i, fx::ent(33422, tt::byte_, {1, 1, 0, 1, 1, 2, 2, 1, 1, 0, 1, 1, 1, 2, 1, 1, 0, 1,
                                                             1, 1, 2, 1, 1, 0, 0, 1, 1, 1, 2, 1, 1, 0, 1, 2, 1, 1}));
               },
               unsupported_, "raw.cfa_pattern"});
  c.push_back({"cella senza blu", [](s_t& s) { s.cfa = {0, 1, 1, 1}; }, {}, unsupported_, "raw.cfa_pattern"});
  // Cella con un 0, un 2 e due 1 ma non Bayer (i due 1 non sono in diagonale): U13 §4, riga CFA.
  for (const std::array<std::uint8_t, 4>& cell : {std::array<std::uint8_t, 4>{0, 2, 1, 1}, {1, 1, 0, 2}, {2, 0, 1, 1}, {1, 1, 2, 0}})
    c.push_back({"cella CFA non Bayer {" + std::to_string(cell[0]) + ", " + std::to_string(cell[1]) + ", " +
                     std::to_string(cell[2]) + ", " + std::to_string(cell[3]) + "}",
                 [cell](s_t& s) { s.cfa = cell; }, {}, unsupported_, "raw.cfa_pattern"});
  c.push_back({"CFAPlaneColor (0, 2, 1)", {}, [](i_t& i) { fx::set_entry(i, fx::ent(50710, tt::byte_, {0, 2, 1})); },
               unsupported_, "raw.cfa_pattern"});
  c.push_back({"CFALayout 2", {}, [](i_t& i) { fx::set_entry(i, fx::ent(50711, tt::short_, {2})); }, unsupported_,
               "raw.dng_layout"});
  c.push_back({"PlanarConfiguration 2", {}, [](i_t& i) { fx::set_entry(i, fx::ent(284, tt::short_, {2})); },
               unsupported_, "raw.dng_layout"});
  c.push_back({"RowInterleaveFactor 2", {}, [](i_t& i) { fx::set_entry(i, fx::ent(50975, tt::short_, {2})); },
               unsupported_, "raw.dng_layout"});
  c.push_back({"ColumnInterleaveFactor 2", {}, [](i_t& i) { fx::set_entry(i, fx::ent(52547, tt::short_, {2})); },
               unsupported_, "raw.dng_layout"});
  c.push_back({"SubTileBlockSize (2, 2)", {}, [](i_t& i) { fx::set_entry(i, fx::ent(50974, tt::short_, {2, 2})); },
               unsupported_, "raw.dng_layout"});
  c.push_back({"TileWidth 24", [](s_t& s) { s.tile_width = 24; }, {}, unsupported_, "raw.dng_nonconformant"});
  c.push_back({"TileLength 24", [](s_t& s) { s.tile_length = 24; }, {}, unsupported_, "raw.dng_nonconformant"});
  c.push_back({"DefaultScale (1, 2)", {}, [](i_t& i) { fx::set_entry(i, fx::ent(50718, tt::rational, {1, 1, 2, 1})); },
               unsupported_, "raw.geometry"});
  c.push_back({"BlackLevel RATIONAL 129/2", {}, [](i_t& i) { fx::set_entry(i, fx::ent(50714, tt::rational, {129, 2})); },
               unsupported_, "raw.black_level"});
  c.push_back({"BlackLevel RATIONAL con denominatore nullo", {},
               [](i_t& i) { fx::set_entry(i, fx::ent(50714, tt::rational, {129, 0})); }, integrity_, "raw.data_error"});
  // Conteggi della specifica DNG 1.7.1.0: BlackLevelDeltaH «Count: ActiveArea width» (p. 28), BlackLevelDeltaV «Count:
  // ActiveArea length» (p. 29). ActiveArea della fixture: 28 colonne e 28 righe.
  c.push_back({"BlackLevelDeltaH non nullo", {},
               [](i_t& i) { fx::set_entry(i, fx::ent(50715, tt::srational, srationals(28, 1))); }, unsupported_,
               "raw.black_level"});
  c.push_back({"BlackLevelDeltaV non nullo", {},
               [](i_t& i) { fx::set_entry(i, fx::ent(50716, tt::srational, srationals(28, -1))); }, unsupported_,
               "raw.black_level"});
  c.push_back({"ActiveArea fuori dall'immagine", {},
               [](i_t& i) { fx::set_entry(i, fx::ent(50829, tt::long_, {2, 2, 33, 30})); }, integrity_,
               "raw.data_error"});
  return c;
}

// Compression 52546 (§6): stop sui tag prima dei segmenti, per PhotometricInterpretation.
struct jxl_case {
  std::uint16_t photometric;
  std::uint16_t samples_per_pixel;
  std::string code;
};
const std::vector<jxl_case>& jxl_cases() {
  static const std::vector<jxl_case> c = {{32803, 1, "raw.dng_nonconformant"},
                                          {34892, 3, "raw.not_cfa"},
                                          {1, 1, "raw.not_cfa"},
                                          {32844, 1, "raw.dng_nonconformant"}};
  return c;
}

TEST(DngTags, T_U13_06_tag_stops_one_at_a_time) {
  for (const tag_case& c : tag_cases()) {
    SCOPED_TRACE(c.name);
    fx::dng32_spec written;
    if (c.spec) c.spec(written);
    const observed o = run(tag_case_file(c));
    expect_stop(o, c.category, c.code);
    EXPECT_EQ(o.inflate_calls, 0) << "inflate chiamata prima dello stop sui tag";
    EXPECT_EQ(o.open_buffer_calls, 0);
    if (o.stop) expect_route_context(*o.stop, 8, 1, written.bits);
  }
}

TEST(DngTags, T_U13_06_jpeg_xl_stops_on_the_tags_before_the_segments) {
  for (const jxl_case& j : jxl_cases()) {
    SCOPED_TRACE("PhotometricInterpretation " + std::to_string(j.photometric));
    fx::dng32_spec s = spec16(52546);
    s.photometric = j.photometric;
    s.samples_per_pixel = j.samples_per_pixel;
    const observed o = run(one_ifd(s));
    expect_stop(o, error_category::unsupported, j.code);
    EXPECT_EQ(o.inflate_calls, 0);
    EXPECT_EQ(o.open_buffer_calls, 0);
    if (!o.stop) continue;
    expect_route_context(*o.stop, 52546, 1, 16);
    EXPECT_EQ(ctx(*o.stop, "photometric_interpretation"), std::to_string(j.photometric)) << o.stop->what();
    EXPECT_EQ(ctx(*o.stop, "samples_per_pixel"), std::to_string(j.samples_per_pixel)) << o.stop->what();
  }
}

// Varianti accettate dai controlli dei tag: la decodifica arriva ai segmenti (inflate chiamata, §4: i controlli dei tag
// precedono ogni segmento). L'immagine prodotta è di T-U13-03 (U13b).
TEST(DngTags, T_U13_06_accepted_variants_reach_the_segments) {
  std::vector<std::pair<std::string, bytes_t>> files;
  files.emplace_back("fixture di base", one_ifd(fx::dng32_spec{}));
  {
    fx::dng32_spec s;
    s.backward_version.reset();  // predefinito: DNGVersion 1.7.1.0 con gli ultimi due byte a 0, 1.7.0.0
    files.emplace_back("DNGVersion 1.7.1.0 senza DNGBackwardVersion", one_ifd(s));
  }
  for (const std::array<std::uint8_t, 4>& cell : {std::array<std::uint8_t, 4>{2, 1, 1, 0}, {1, 0, 2, 1}, {1, 2, 0, 1}}) {
    fx::dng32_spec s;
    s.cfa = cell;
    files.emplace_back("cella CFA Bayer {" + std::to_string(cell[0]) + ", " + std::to_string(cell[1]) + ", " +
                           std::to_string(cell[2]) + ", " + std::to_string(cell[3]) + "}",
                       one_ifd(s));
  }
  // Conteggi di p. 28 e 29 (larghezza e altezza di ActiveArea, 28).
  files.emplace_back("BlackLevelDeltaH e BlackLevelDeltaV nulli",
                     one_ifd(fx::dng32_spec{}, fx::tiff_order::ii, false, [](fx::tiff_ifd& i) {
                       fx::set_entry(i, fx::ent(50715, tt::srational, srationals(28, 0)));
                       fx::set_entry(i, fx::ent(50716, tt::srational, srationals(28, 0)));
                     }));
  for (const auto& [name, bytes] : files) {
    SCOPED_TRACE(name);
    const observed o = run(bytes);
    EXPECT_GE(o.inflate_calls, 1) << describe(o);
    EXPECT_EQ(o.open_buffer_calls, 0);
  }
}

// Ordine dei controlli (§4): struttura, Compression 52546, poi le righe della tabella; vale il primo stop.
TEST(DngTags, T_U13_06_order_of_checks_first_stop_wins) {
  {
    SCOPED_TRACE("errore di struttura e DNGBackwardVersion 1.8.0.0: raw.data_error");
    const observed o = run(one_ifd(fx::dng32_spec{}, fx::tiff_order::ii, false, [](fx::tiff_ifd& i) {
      fx::set_entry(i, fx::ent(50707, tt::byte_, {1, 8, 0, 0}));
      fx::tiff_entry area = fx::ent(50829, tt::long_, {2, 2, 30, 30});
      area.value_offset = 1u << 20;  // ActiveArea (16 byte, fuori riga) letta oltre il buffer
      fx::set_entry(i, area);
    }));
    expect_stop(o, error_category::integrity, "raw.data_error");
    EXPECT_EQ(o.inflate_calls, 0);
  }
  {
    SCOPED_TRACE("Compression 52546 CFA e DNGBackwardVersion 1.8.0.0: raw.dng_nonconformant");
    const observed o = run(one_ifd(spec16(52546), fx::tiff_order::ii, false, [](fx::tiff_ifd& i) {
      fx::set_entry(i, fx::ent(50707, tt::byte_, {1, 8, 0, 0}));
    }));
    expect_stop(o, error_category::unsupported, "raw.dng_nonconformant");
    EXPECT_EQ(o.inflate_calls, 0);
  }
  {
    SCOPED_TRACE("PlanarConfiguration 2 e TileWidth 24: raw.dng_layout (riga precedente a «Segmenti»)");
    fx::dng32_spec s;
    s.tile_width = 24;
    const observed o = run(one_ifd(s, fx::tiff_order::ii, false,
                                   [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(284, tt::short_, {2})); }));
    expect_stop(o, error_category::unsupported, "raw.dng_layout");
    EXPECT_EQ(o.inflate_calls, 0);
  }
  {
    SCOPED_TRACE("CFALayout 2 e ActiveArea fuori dall'immagine: raw.dng_layout");
    const observed o = run(one_ifd(fx::dng32_spec{}, fx::tiff_order::ii, false, [](fx::tiff_ifd& i) {
      fx::set_entry(i, fx::ent(50711, tt::short_, {2}));
      fx::set_entry(i, fx::ent(50829, tt::long_, {2, 2, 33, 30}));
    }));
    expect_stop(o, error_category::unsupported, "raw.dng_layout");
    EXPECT_EQ(o.inflate_calls, 0);
  }
}

// Provenance dell'arresto (§8): il job `io.raw_import` fermato dal decoder del progetto registra lo stop con `decoder` =
// astro_dng e i campi dell'instradamento.
TEST(DngTags, T_U13_06_stop_provenance_names_the_project_decoder_and_the_route) {
  struct file_case {
    std::string name;
    bytes_t bytes;
    std::string code;
    std::array<std::uint32_t, 3> fields;
  };
  std::vector<file_case> files;
  files.push_back({"planar2", one_ifd(fx::dng32_spec{}, fx::tiff_order::ii, false,
                                      [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(284, tt::short_, {2})); }),
                   "raw.dng_layout", {8, 1, 32}});
  files.push_back({"jxl_cfa", one_ifd(spec16(52546)), "raw.dng_nonconformant", {52546, 1, 16}});
  for (const file_case& fc : files) {
    SCOPED_TRACE(fc.name);
    const fs::path dir = fs::absolute(fs::path(ASTRO_TEST_TMP) / "u13a" / fc.name);
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path file = dir / "file.dng";
    std::ofstream(file, std::ios::binary)
        .write(reinterpret_cast<const char*>(fc.bytes.data()), static_cast<std::streamsize>(fc.bytes.size()));
    const raw_import_request request = make_raw_import_request(file.string(), digest_of(fc.bytes));
    ASSERT_TRUE(request.identification_stop.has_value()) << "la decodifica della richiesta deve fermarsi";
    std::unique_ptr<sqlite_dag_store> store = sqlite_dag_store::create(dir / "store");
    store->put_node(request.node);
    store->create_slot("s", request.node.key());
    memory_pool pool(std::uint64_t{256} << 20);
    cancel_token cancel;
    testing::internal::CaptureStderr();
    const job_outcome outcome = run_raw_import_job(*store, "s", request, pool, cancel);
    testing::internal::GetCapturedStderr();
    ASSERT_TRUE(outcome.stop.has_value());
    EXPECT_EQ(outcome.stop->code(), fc.code) << outcome.stop->what();
    ASSERT_TRUE(outcome.provenance.stop.has_value()) << "provenance senza arresto";
    const stop_error& recorded = *outcome.provenance.stop;
    EXPECT_EQ(recorded.code(), fc.code) << recorded.what();
    expect_route_context(recorded, fc.fields[0], fc.fields[1], fc.fields[2]);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Rilievi del giro 1 della review di U13a: tetto di IFD (§3 punto 2), TIFF non DNG con catena rotta, BlackLevelRepeatDim.
// ---------------------------------------------------------------------------------------------------------------------

// IFD0 raw deflate con `sub_ifds` miniature in SubIFDs (NewSubFileType 1: non raw) e `chain_ifds` miniature di catena
// (senza NewSubFileType: non raw). IFD letti in tutto: 1 + sub_ifds + chain_ifds.
fx::tiff_layout ifd_count_layout(std::size_t sub_ifds, std::size_t chain_ifds,
                                 std::optional<std::uint64_t> last_next = std::nullopt) {
  fx::tiff_file f;
  fx::tiff_ifd ifd0 = fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate);
  for (std::size_t k = 0; k < sub_ifds; ++k) ifd0.sub_ifds.push_back(chain_thumbnail(1));
  f.chain.push_back(std::move(ifd0));
  for (std::size_t k = 0; k < chain_ifds; ++k) f.chain.push_back(chain_thumbnail());
  f.last_next = last_next;
  return fx::write_tiff(f);
}

bytes_t ifd_count_file(std::size_t sub_ifds, std::size_t chain_ifds, std::optional<std::uint64_t> last_next = std::nullopt) {
  return ifd_count_layout(sub_ifds, chain_ifds, last_next).bytes;
}

TEST(DngRoute, T_U13_01_more_than_256_ifds_stop_with_max_ifds_and_exactly_256_do_not) {
  struct count_case {
    std::size_t sub_ifds, chain_ifds;
  };
  // Contano IFD0, SubIFDs e catena: 257 si fermano, 256 no, comunque ripartiti.
  for (const count_case& c : {count_case{0, 256}, {256, 0}, {100, 156}, {1, 255}, {255, 1}}) {
    SCOPED_TRACE(std::to_string(c.sub_ifds) + " SubIFDs e " + std::to_string(c.chain_ifds) + " IFD di catena oltre IFD0");
    const fx::tiff_layout over_layout = ifd_count_layout(c.sub_ifds, c.chain_ifds);
    const bytes_t& over = over_layout.bytes;
    // Ordine di lettura: IFD0, SubIFDs, catena. Il 257° è l'ultimo IFD della catena, o l'ultimo dei SubIFDs se la catena
    // oltre IFD0 è vuota; il suo offset è quello della sua tabella nel file.
    const std::uint64_t offset_257 = c.chain_ifds > 0 ? over_layout.chain_tables.back() : over_layout.sub_tables.back();
    try {
      (void)dng_route(over);
      ADD_FAILURE() << "dng_route: atteso raw.max_ifds con 257 IFD";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::limit_exceeded) << e.what();
      EXPECT_EQ(e.code(), "raw.max_ifds") << e.what();
      EXPECT_EQ(ctx(e, "funzione"), std::string("dng_route")) << e.what();
      EXPECT_EQ(ctx(e, "limite"), std::string("256")) << e.what();
      EXPECT_EQ(ctx(e, "offset"), std::to_string(offset_257)) << e.what();
    }
    const observed o = run(over);
    expect_stop(o, error_category::limit_exceeded, "raw.max_ifds");
    if (o.stop) {
      EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("dng_route")) << o.stop->what();
      EXPECT_EQ(ctx(*o.stop, "limite"), std::string("256")) << o.stop->what();
      EXPECT_EQ(ctx(*o.stop, "offset"), std::to_string(offset_257)) << o.stop->what();
    }
    EXPECT_EQ(o.open_buffer_calls, 0);
    EXPECT_EQ(o.inflate_calls, 0);
    EXPECT_TRUE(o.routes.empty()) << "decoder scelto prima dello stop";

    // Un IFD in meno: nessuno stop per il tetto, il file va al decoder del progetto.
    const std::size_t sub = c.sub_ifds > 0 ? c.sub_ifds - 1 : 0;
    const std::size_t chain = c.sub_ifds > 0 ? c.chain_ifds : c.chain_ifds - 1;
    const bytes_t at_limit = ifd_count_file(sub, chain);
    dng_route_result r;
    ASSERT_NO_THROW(r = dng_route(at_limit));
    EXPECT_EQ(r.decoder, raw_decoder_kind::astro_dng);
    const observed ok = run(at_limit);
    EXPECT_EQ(ok.open_buffer_calls, 0);
    ASSERT_EQ(ok.routes.size(), 1u);
    EXPECT_GE(ok.inflate_calls, 1) << describe(ok);
    if (ok.stop) {
      EXPECT_NE(ok.stop->code(), "raw.max_ifds") << ok.stop->what();
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Giro 2 della review di U13a, arbitraggio del coordinatore: un offset di IFD già letto non è un errore (U13 §3 punto 2).
// L'IFD si legge e si conta una volta sola, per il tetto e per i punti 3-5; un solo IFD raw raggiunto per due vie resta un
// solo IFD raw.
// ---------------------------------------------------------------------------------------------------------------------

fx::tiff_sub_ref chain_ref(std::size_t index) { return fx::tiff_sub_ref{true, index}; }
fx::tiff_sub_ref child_ref(std::size_t index) { return fx::tiff_sub_ref{false, index}; }

struct repeat_case {
  std::string name;
  bytes_t bytes;
};

// Per ogni via di ripetizione, una variante con l'IFD ripetuto raw (l'unico IFD raw del file) e una con l'IFD ripetuto non
// raw (NewSubFileType 1; in `SubIFDs` un tag assente varrebbe 0).
std::vector<repeat_case> repeated_offset_cases() {
  std::vector<repeat_case> c;
  const auto raw_dng = [] { return fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate); };
  {
    // SubIFDs di IFD0 = IFD1 della catena, che è l'IFD raw.
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    ifd0.sub_refs.push_back(chain_ref(1));
    f.chain.push_back(std::move(ifd0));
    f.chain.push_back(sub_raw());
    c.push_back({"SubIFDs = IFD della catena, ripetuto raw", fx::write_tiff(f).bytes});
  }
  {
    // IFD raw della catena senza NewSubFileType, raggiunto prima da SubIFDs: conta come IFD di SubIFDs, quindi raw (§3
    // punto 2); letto prima dalla catena sarebbe un IFD di catena senza il tag, non raw.
    fx::dng32_spec no_tag;
    no_tag.write_new_subfile_type = false;
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    ifd0.sub_refs.push_back(chain_ref(1));
    f.chain.push_back(std::move(ifd0));
    f.chain.push_back(sub_raw(no_tag));
    c.push_back({"SubIFDs = IFD della catena senza NewSubFileType, raw per SubIFDs", fx::write_tiff(f).bytes});
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = raw_dng();
    ifd0.sub_refs.push_back(chain_ref(1));
    f.chain.push_back(std::move(ifd0));
    f.chain.push_back(chain_thumbnail(1));
    c.push_back({"SubIFDs = IFD della catena, ripetuto non raw", fx::write_tiff(f).bytes});
  }
  {
    // Lo stesso offset due volte dentro SubIFDs.
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    ifd0.sub_ifds.push_back(sub_raw());
    ifd0.sub_refs.push_back(child_ref(0));
    f.chain.push_back(std::move(ifd0));
    c.push_back({"offset ripetuto in SubIFDs, ripetuto raw", fx::write_tiff(f).bytes});
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = raw_dng();
    ifd0.sub_ifds.push_back(chain_thumbnail(1));
    ifd0.sub_refs.push_back(child_ref(0));
    f.chain.push_back(std::move(ifd0));
    c.push_back({"offset ripetuto in SubIFDs, ripetuto non raw", fx::write_tiff(f).bytes});
  }
  {
    // SubIFDs che punta a IFD0, che è l'IFD raw.
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = raw_dng();
    ifd0.sub_refs.push_back(chain_ref(0));
    f.chain.push_back(std::move(ifd0));
    c.push_back({"SubIFDs -> IFD0, IFD0 raw", fx::write_tiff(f).bytes});
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    ifd0.sub_ifds.push_back(sub_raw());
    ifd0.sub_refs.push_back(chain_ref(0));
    f.chain.push_back(std::move(ifd0));
    c.push_back({"SubIFDs -> IFD0, IFD0 non raw", fx::write_tiff(f).bytes});
  }
  return c;
}

// Esito del file con un solo IFD raw e nessuna ripetizione: lo stesso atteso da ogni file dei casi sotto.
void expect_single_project_raw(const bytes_t& bytes, const observed& single) {
  dng_route_result r;
  ASSERT_NO_THROW(r = dng_route(bytes));
  EXPECT_EQ(r.decoder, raw_decoder_kind::astro_dng);
  EXPECT_EQ(r.reason, "");
  EXPECT_EQ(r.compression, std::optional<std::uint32_t>(8));
  EXPECT_EQ(r.sample_format, std::optional<std::uint32_t>(1));
  EXPECT_EQ(r.bits_per_sample, std::optional<std::uint32_t>(32));
  const observed o = run(bytes);
  ASSERT_EQ(o.routes.size(), 1u) << describe(o);
  EXPECT_EQ(o.routes.front().decoder, raw_decoder_kind::astro_dng);
  EXPECT_EQ(o.open_buffer_calls, 0);
  if (o.stop) {
    EXPECT_NE(o.stop->code(), "raw.multi_image") << o.stop->what();
    EXPECT_NE(o.stop->code(), "raw.data_error") << o.stop->what();
    EXPECT_NE(o.stop->code(), "raw.max_ifds") << o.stop->what();
  }
  EXPECT_EQ(o.inflate_calls, single.inflate_calls) << describe(o);
  EXPECT_EQ(o.stop.has_value(), single.stop.has_value()) << describe(o);
  if (o.stop && single.stop) {
    EXPECT_EQ(o.stop->code(), single.stop->code()) << o.stop->what();
  }
  EXPECT_EQ(o.img.has_value(), single.img.has_value()) << describe(o);
}

TEST(DngRoute, T_U13_01_a_repeated_ifd_offset_is_read_once_and_one_raw_ifd_stays_one_raw_ifd) {
  const observed single = run(one_ifd(fx::dng32_spec{}));
  ASSERT_GE(single.inflate_calls, 1) << describe(single);
  for (const repeat_case& c : repeated_offset_cases()) {
    SCOPED_TRACE(c.name);
    expect_single_project_raw(c.bytes, single);
  }
}

// Con un secondo IFD raw distinto accanto all'offset ripetuto, §3 punto 5 vale ancora: il conteggio non si perde.
TEST(DngRoute, T_U13_01_a_repeated_offset_does_not_hide_a_second_distinct_raw_ifd) {
  fx::tiff_file f;
  fx::tiff_ifd ifd0 = preview_ifd0();
  ifd0.sub_ifds.push_back(sub_raw());
  ifd0.sub_refs.push_back(child_ref(0));
  f.chain.push_back(std::move(ifd0));
  fx::tiff_ifd second = fx::make_dng32_ifd(spec16(1), fx::tiff_order::ii, stored);
  fx::remove_entry(second, 50706);
  fx::remove_entry(second, 50707);
  f.chain.push_back(std::move(second));
  expect_stop(run(fx::write_tiff(f).bytes), error_category::unsupported, "raw.multi_image");
}

// Tetto di 256: gli IFD ripetuti si contano una volta. Ogni file ha IFD0 più `distinct` IFD non raw distinti; tutti tranne
// l'ultimo sono richiamati una seconda volta, e l'ultimo, nuovo, si legge dopo le ripetizioni: contando le ripetizioni
// due volte il tetto scatterebbe su quell'ultimo, contate una volta no.
bytes_t repeated_thumbnails_file(std::size_t distinct, bool via_chain) {
  fx::tiff_file f;
  fx::tiff_ifd ifd0 = fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate);
  if (via_chain) {
    // SubIFDs legge per primo gli IFD 1..distinct-1 della catena; la catena li ripete, poi legge l'ultimo.
    for (std::size_t k = 0; k + 1 < distinct; ++k) ifd0.sub_refs.push_back(chain_ref(1 + k));
  } else {
    for (std::size_t k = 0; k + 1 < distinct; ++k) ifd0.sub_ifds.push_back(chain_thumbnail(1));
    for (std::size_t k = 0; k + 1 < distinct; ++k) ifd0.sub_refs.push_back(child_ref(k));
  }
  f.chain.push_back(std::move(ifd0));
  if (via_chain) {
    for (std::size_t k = 0; k < distinct; ++k) f.chain.push_back(chain_thumbnail(1));
  } else {
    f.chain.push_back(chain_thumbnail(1));
  }
  return fx::write_tiff(f).bytes;
}

TEST(DngRoute, T_U13_01_repeated_ifds_count_once_for_the_256_ifd_limit) {
  const observed single = run(one_ifd(fx::dng32_spec{}));
  ASSERT_GE(single.inflate_calls, 1) << describe(single);
  for (const bool via_chain : {true, false}) {
    SCOPED_TRACE(via_chain ? "SubIFDs ripetono IFD della catena" : "SubIFDs ripetono i propri offset");
    // 1 + 130 IFD distinti (131), 129 dei quali ripetuti: l'ultimo si legge dopo le ripetizioni e, con ogni offset
    // contato due volte, sarebbe il 1 + 130 + 129 = 260° > 256.
    static_assert(1 + 130 < 256 && 1 + 130 + 129 > 256);
    expect_single_project_raw(repeated_thumbnails_file(130, via_chain), single);
    // 1 + 255 = 256 IFD distinti, ognuno ripetuto: sul tetto, nessuno stop.
    expect_single_project_raw(repeated_thumbnails_file(255, via_chain), single);
    // 1 + 256 = 257 IFD distinti: il tetto vale ancora, anche con le ripetizioni.
    const observed over = run(repeated_thumbnails_file(256, via_chain));
    expect_stop(over, error_category::limit_exceeded, "raw.max_ifds");
    if (over.stop) {
      EXPECT_EQ(ctx(*over.stop, "limite"), std::string("256")) << over.stop->what();
    }
  }
}

// Le ripetizioni vengono prima degli IFD nuovi: IFD0 raw con `repeats` voci di SubIFDs che richiamano lo stesso IFD (IFD0
// stesso, oppure un unico figlio non raw), poi `chain_ifds` IFD di catena non raw, tutti distinti. IFD distinti letti:
// 1 + chain_ifds, più 1 se le ripetizioni puntano a un figlio.
bytes_t repeats_before_new_file(std::size_t repeats, std::size_t chain_ifds, bool to_ifd0) {
  fx::tiff_file f;
  fx::tiff_ifd ifd0 = fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate);
  if (!to_ifd0) ifd0.sub_ifds.push_back(chain_thumbnail(1));
  for (std::size_t k = 0; k < repeats; ++k) ifd0.sub_refs.push_back(to_ifd0 ? chain_ref(0) : child_ref(0));
  f.chain.push_back(std::move(ifd0));
  for (std::size_t k = 0; k < chain_ifds; ++k) f.chain.push_back(chain_thumbnail(1));
  return fx::write_tiff(f).bytes;
}

TEST(DngRoute, T_U13_01_repeats_read_before_new_ifds_do_not_use_up_the_256_ifd_limit) {
  const observed single = run(one_ifd(fx::dng32_spec{}));
  ASSERT_GE(single.inflate_calls, 1) << describe(single);
  for (const bool to_ifd0 : {true, false}) {
    SCOPED_TRACE(to_ifd0 ? "ripetizioni di IFD0 in SubIFDs" : "ripetizioni di un figlio di SubIFDs");
    const std::size_t extra = to_ifd0 ? 0 : 1;  // il figlio è un IFD distinto in più
    // 200 ripetizioni e 1 + extra + 200 IFD distinti: contando le ripetizioni sarebbero oltre 256, contate una volta no.
    static_assert(1 + 1 + 200 < 256 && 1 + 200 + 200 > 256);
    expect_single_project_raw(repeats_before_new_file(200, 200, to_ifd0), single);
    // Esattamente 256 IFD distinti: nessuno stop; 257: il tetto.
    expect_single_project_raw(repeats_before_new_file(200, 255 - extra, to_ifd0), single);
    const observed over = run(repeats_before_new_file(200, 256 - extra, to_ifd0));
    expect_stop(over, error_category::limit_exceeded, "raw.max_ifds");
    if (over.stop) {
      EXPECT_EQ(ctx(*over.stop, "limite"), std::string("256")) << over.stop->what();
    }
  }
}

TEST(DngRoute, T_U13_01_the_257th_ifd_stops_before_its_table_is_read) {
  // 256 IFD, il «prossimo IFD» dell'ultimo punta oltre il buffer: il 257° dà il tetto, non l'errore di struttura.
  const observed capped = run(ifd_count_file(0, 255, 1u << 20));
  expect_stop(capped, error_category::limit_exceeded, "raw.max_ifds");
  if (capped.stop) {
    EXPECT_EQ(ctx(*capped.stop, "limite"), std::string("256")) << capped.stop->what();
    EXPECT_EQ(ctx(*capped.stop, "offset"), std::to_string(1u << 20)) << capped.stop->what();
  }
  // Con 255 IFD lo stesso offset è l'errore di struttura (256° IFD letto fuori dal buffer).
  expect_stop(run(ifd_count_file(0, 254, 1u << 20)), error_category::integrity, "raw.data_error");
}

TEST(DngRoute, T_U13_01_non_dng_tiff_with_broken_chain_goes_to_libraw) {
  struct broken_case {
    std::string name;
    bytes_t bytes;
  };
  std::vector<broken_case> cases;
  const auto non_dng = [](fx::tiff_ifd& i) {
    fx::remove_entry(i, 50706);
    fx::remove_entry(i, 50707);
  };
  {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate));
    non_dng(f.chain.back());
    f.chain.push_back(chain_thumbnail());
    f.last_next = 1u << 20;
    cases.push_back({"«prossimo IFD» oltre il buffer", fx::write_tiff(f).bytes});
  }
  {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate));
    non_dng(f.chain.back());
    f.chain.push_back(chain_thumbnail());
    f.last_next_to = 0;
    cases.push_back({"ciclo nella catena", fx::write_tiff(f).bytes});
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    non_dng(ifd0);
    fx::tiff_entry sub = fx::ent(330, tt::long_, {0, 0});
    sub.value_offset = 1u << 20;
    ifd0.entries.push_back(sub);
    f.chain.push_back(std::move(ifd0));
    cases.push_back({"SubIFDs con offset dei valori oltre il buffer", fx::write_tiff(f).bytes});
  }
  {
    fx::tiff_file f;
    fx::tiff_ifd ifd0 = preview_ifd0();
    non_dng(ifd0);
    fx::tiff_entry sub = fx::ent(330, tt::long_, {1u << 20});
    ifd0.entries.push_back(sub);
    f.chain.push_back(std::move(ifd0));
    cases.push_back({"SubIFDs con un IFD oltre il buffer", fx::write_tiff(f).bytes});
  }
  {
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate));
    non_dng(f.chain.back());
    for (int k = 0; k < 300; ++k) f.chain.push_back(chain_thumbnail());
    cases.push_back({"300 IFD di catena (oltre il tetto)", fx::write_tiff(f).bytes});
  }
  for (const broken_case& c : cases) {
    SCOPED_TRACE(c.name);
    dng_route_result r;
    ASSERT_NO_THROW(r = dng_route(c.bytes));
    EXPECT_EQ(r.decoder, raw_decoder_kind::libraw);
    EXPECT_EQ(r.reason, "tiff_not_dng");
    const observed o = run(c.bytes);
    ASSERT_EQ(o.routes.size(), 1u);
    EXPECT_EQ(o.routes.front().decoder, raw_decoder_kind::libraw);
    EXPECT_EQ(o.routes.front().reason, "tiff_not_dng");
    EXPECT_EQ(o.open_buffer_calls, 1);
    EXPECT_EQ(o.inflate_calls, 0);
    // L'esito è quello di LibRaw (funzione open_buffer), mai quello dell'instradamento.
    if (o.stop) {
      EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("open_buffer")) << o.stop->what();
    }
  }
}

}  // namespace
