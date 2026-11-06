// U13b (M3.a): decoder DNG del progetto, immagine dai segmenti deflate interi a 32 bit. T-U13-03, 04, 05, 06 (limite del
// motivo del nero), 07, 08, 12, 24
// (docs/contracts/U13-dng-decoder.md §4, §5, §7, §8, §10, §12; plans/M3a.md, U13). T-U13-04 (tetto e picco di zlib) e
// T-U13-09 stanno in test_dng_u13b_hooks.cpp. T-U13-10 è raw_threads_dng.cpp, T-U13-11 fuzz_dng_decode.cpp.
//
// Fixture FIX-SYN-DNG-02: contenitore di tests/fixtures/syn_dng.hpp, segmenti di tests/fixtures/syn_dng_deflate.hpp
// (Predictor 1 e 2 da libtiff 4.7.2, 34892 e 34893 dal generatore dei test, zlib). Oracolo: i valori scritti dal
// generatore a seme fisso (`dng32_sample`, con 0 e il massimo sempre presenti nell'area importata), i valori del
// contratto (date, digest delle curve di ADR-08.b §2.4) e, per T-U13-05, il percorso LibRaw sul gemello non compresso a 16
// bit, che non condivide codice con il decoder del progetto. Mai l'uscita del decoder sotto prova. Tolleranze: zero.
//
// API presupposta, oltre a quella di U13a:
//   - Immagine del decoder del progetto (§4): `decoder_state.raw_width`/`raw_height` = ImageWidth/ImageLength; `window` =
//     ActiveArea esatta; `cfa` = {pattern di CFAPattern, 0, 0}; nero scalare in `black` con motivo vuoto (0 × 0), motivo
//     R × C in `black_pattern_rows`/`cols`/`black_pattern`; `white` = WhiteLevel; `linearization`/`curve_sha256` con la
//     regola di ADR-08.b §2.4; nessuna maschera di invalidità impostata, nessuna varianza.
//   - Metadati (§4): `exposure_s`, `gain`, `camera` (Make e Model uniti come nel percorso LibRaw: «Make Model»), ora in
//     `capture_time_libraw` (stesso campo e stessa semantica `camera_no_zone` di ADR-08.b §2.6), origine `decoder`.
//   - Record (§8): `raw_decoder_record(img)` con le chiavi di `raw_record::astro_dng_keys` (raw_record_keys.hpp); il record
//     delle decodifiche LibRaw ha anche `decoder` e `route_reason`.
#include <gtest/gtest.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "astro/raw_import_job.hpp"
#include "raw_record_keys.hpp"
#include "record_roundtrip.hpp"
#include "syn_dng.hpp"
#include "syn_dng_deflate.hpp"
#include "dng_u13b_support.hpp"
#include "pub_raw_01.hpp"

// T-U13-07: `mktime` interposto come `pthread_create` in T-M3A-RAW-08 (ADR-08.b §2.7): conta le chiamate e inoltra a
// quella reale. Nessuna chiamata durante la decodifica del progetto (§4: ora con `timegm` dei campi scritti).
namespace {
std::atomic<std::uint64_t> g_mktime_calls{0};
std::uint64_t mktime_calls() { return g_mktime_calls.load(std::memory_order_relaxed); }
}  // namespace
extern "C" std::time_t mktime(std::tm* t) {
  using real_fn = std::time_t (*)(std::tm*);
  static const real_fn real = reinterpret_cast<real_fn>(::dlsym(RTLD_NEXT, "mktime"));
  g_mktime_calls.fetch_add(1, std::memory_order_relaxed);
  return real(t);
}

namespace {

using namespace astro;
namespace fx = astro_fixture;
namespace rk = raw_record;
namespace tt = astro_fixture::tiff_type;

using namespace u13b;

const json* member(const json& o, std::string_view key) {
  return o.type() == json::kind::object ? o.find(std::string(key)) : nullptr;
}
std::optional<std::string> string_of(const json& o, std::string_view key) {
  const json* m = member(o, key);
  if (m == nullptr || m->type() != json::kind::string) return std::nullopt;
  return m->as_string();
}
std::optional<std::int64_t> integer_of(const json& o, std::string_view key) {
  const json* m = member(o, key);
  if (m == nullptr || m->type() != json::kind::integer) return std::nullopt;
  return m->as_integer();
}
std::vector<std::string> strings_of(const json& o, std::string_view key) {
  std::vector<std::string> out;
  const json* m = member(o, key);
  if (m == nullptr || m->type() != json::kind::array) return {"<non è un array>"};
  for (const json& v : m->items()) out.push_back(v.type() == json::kind::string ? v.as_string() : "<non stringa>");
  return out;
}
bool is_null(const json& o, std::string_view key) {
  const json* m = member(o, key);
  return m != nullptr && m->type() == json::kind::null;
}

std::string dotted(const std::array<std::uint8_t, 4>& v) {
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." + std::to_string(v[3]);
}

// Record completo del decoder del progetto (§8) con i valori scritti nella fixture.
void expect_project_record(const image& img, const fx::dng32_spec& s, fx::tiff_order order) {
  const json rec = raw_decoder_record(img);
  ASSERT_EQ(rec.type(), json::kind::object);
  // Insieme esatto: nessuna chiave propria di LibRaw (`route_reason`, `process_warnings`, `raw_inset_crops`, `flip`).
  record_roundtrip::expect_exact_keys(rec, rk::astro_dng_keys);
  EXPECT_EQ(string_of(rec, rk::decoder), std::string(rk::decoder_astro_dng));
  // ADR-08.b §2.9, riga «Libreria»: identità del pacchetto di decoder anche nel record del decoder del progetto.
  EXPECT_EQ(string_of(rec, rk::libraw_version), std::string(rk::expected_libraw_version));
  EXPECT_EQ(integer_of(rec, rk::libraw_version_number), rk::expected_libraw_version_number);
  EXPECT_EQ(integer_of(rec, rk::libraw_capabilities), rk::expected_libraw_capabilities);
  EXPECT_EQ(string_of(rec, rk::libraw_tarball_sha256), std::string(rk::expected_libraw_tarball_sha256));
  EXPECT_EQ(integer_of(rec, rk::compression), 8);
  EXPECT_EQ(integer_of(rec, rk::sample_format), 1);
  EXPECT_EQ(integer_of(rec, rk::bits_per_sample), 32);
  EXPECT_EQ(string_of(rec, rk::dng_spec), std::string(rk::spec_dng_1_7_1_0));
  EXPECT_EQ(string_of(rec, rk::dng_version), dotted(s.dng_version));
  const std::array<std::uint8_t, 4> backward =
      s.backward_version ? *s.backward_version : std::array<std::uint8_t, 4>{s.dng_version[0], s.dng_version[1], 0, 0};
  EXPECT_EQ(string_of(rec, rk::dng_backward_version), dotted(backward));
  EXPECT_EQ(integer_of(rec, rk::predictor), s.predictor);
  EXPECT_EQ(string_of(rec, rk::byte_order), order_name(order));
  EXPECT_EQ(string_of(rec, rk::zlib_version_field), std::string(zlibVersion()));
  if (const json* seg = member(rec, rk::segments); seg != nullptr) {
    EXPECT_EQ(string_of(*seg, "kind"), std::string(s.tiles ? "tiles" : "strips"));
    EXPECT_EQ(integer_of(*seg, "count"), static_cast<std::int64_t>(segment_count(s)));
    if (s.tiles) {
      EXPECT_EQ(integer_of(*seg, "tile_width"), s.tile_width);
      EXPECT_EQ(integer_of(*seg, "tile_length"), s.tile_length);
    } else {
      EXPECT_EQ(integer_of(*seg, "rows_per_strip"), static_cast<std::int64_t>(s.rows_per_strip));
    }
  }
  const fx::dng_active_area& a = s.active_area;
  if (const json* crop = member(rec, rk::crop); crop != nullptr) {
    EXPECT_EQ(integer_of(*crop, "x0"), a.left);
    EXPECT_EQ(integer_of(*crop, "y0"), a.top);
    EXPECT_EQ(integer_of(*crop, "w"), a.right - a.left);
    EXPECT_EQ(integer_of(*crop, "h"), a.bottom - a.top);
  }
  if (const json* parity = member(rec, rk::margin_parity); parity != nullptr) {
    ASSERT_EQ(parity->type(), json::kind::array);
    ASSERT_EQ(parity->items().size(), 2u);
    EXPECT_EQ(parity->items()[0].as_integer(), a.left % 2) << "parità dichiarata anche dispari";
    EXPECT_EQ(parity->items()[1].as_integer(), a.top % 2);
  }
  ASSERT_TRUE(img.cfa.has_value());
  EXPECT_EQ(string_of(rec, rk::cfa_pattern), std::string(pattern_name(img.cfa->pattern)));
  EXPECT_EQ(integer_of(rec, rk::white), s.white);
  // §4: `data_maximum` e `linear_max` assenti; nel record, come nel percorso LibRaw quando mancano, valore null.
  EXPECT_TRUE(is_null(rec, rk::data_maximum));
  EXPECT_TRUE(is_null(rec, rk::linear_max));
  for (std::string_view absent : {rk::sensor, rk::pixel_size_um, rk::pixel_scale_arcsec, rk::wcs})
    EXPECT_TRUE(is_null(rec, absent)) << absent;
  EXPECT_EQ(string_of(rec, rk::absent_reason), std::string("not_provided_by_decoder"));
  EXPECT_EQ(string_of(rec, rk::capture_time_clock), std::string("camera_no_zone"));
  const json* opcodes = member(rec, rk::opcode_lists);
  ASSERT_NE(opcodes, nullptr);
  EXPECT_EQ(opcodes->type(), json::kind::array);
  const json* rejected = member(rec, rk::metadata_rejected);
  ASSERT_NE(rejected, nullptr);
  EXPECT_EQ(rejected->type(), json::kind::array);
}

// T-U13-03: deflate a 32 bit, Predictor 1, 2, 34892, 34893 × strisce e tasselli × II e MM × dimensioni non multiple;
// ActiveArea alle origini del contratto e i quattro pattern. Esatta: 0 differenze.
// ---------------------------------------------------------------------------------------------------------------------

TEST(DngImage, T_U13_03_fixture_segments_come_from_the_project_libtiff) {
  EXPECT_NE(fx::libtiff_version().find("Version 4.7.2"), std::string::npos) << fx::libtiff_version();
  ::testing::Test::RecordProperty("libtiff", fx::libtiff_version());
}

TEST(DngImage, T_U13_03_predictors_layouts_and_byte_orders_decode_exactly) {
  std::size_t i = 0;
  for (const std::uint16_t predictor : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{34892}, std::uint16_t{34893}})
    for (const layout& l : layouts())
      for (const fx::tiff_order order : orders()) {
        fx::dng32_spec s = spec32(predictor, l);
        const auto [x, y] = origins()[i % origins().size()];
        s.cfa = bayer_cells()[(i / origins().size()) % bayer_cells().size()].first;
        set_area(s, x, y);
        ++i;
        SCOPED_TRACE(case_name(s, l.name, order));
        const observed o = run(fx::make_deflate_dng(s, order));
        expect_project_image(o, s);
        if (o.img) expect_project_record(*o.img, s, order);
      }
}

// Dimensioni multiple dei tasselli e della cella (32 × 32, la fixture di base): stesso criterio.
TEST(DngImage, T_U13_03_dimensions_multiple_of_the_tile_decode_exactly) {
  for (const std::uint16_t predictor : {std::uint16_t{1}, std::uint16_t{2}, std::uint16_t{34892}, std::uint16_t{34893}})
    for (const fx::tiff_order order : orders()) {
      fx::dng32_spec s = spec32(predictor, layouts()[4], 32, 32);
      set_area(s, 2, 2);
      SCOPED_TRACE(case_name(s, layouts()[4].name, order));
      const observed o = run(fx::make_deflate_dng(s, order));
      expect_project_image(o, s);
    }
}

// Ogni origine × ogni pattern, su due codifiche: crop, parità e pattern esatti, nessun arrotondamento dell'origine.
TEST(DngImage, T_U13_03_active_area_origins_and_patterns_without_rounding) {
  const std::vector<std::tuple<std::uint16_t, std::size_t, fx::tiff_order>> encodings = {
      {2, 4, fx::tiff_order::mm}, {34893, 1, fx::tiff_order::ii}};
  for (const auto& [predictor, layout_index, order] : encodings)
    for (const auto& [x, y] : origins())
      for (const auto& [cell, pattern] : bayer_cells()) {
        fx::dng32_spec s = spec32(predictor, layouts()[layout_index]);
        s.cfa = cell;
        set_area(s, x, y);
        SCOPED_TRACE(case_name(s, layouts()[layout_index].name, order));
        const observed o = run(fx::make_deflate_dng(s, order));
        expect_project_image(o, s);
        if (o.img) {
          ASSERT_TRUE(o.img->cfa.has_value());
          EXPECT_EQ(o.img->cfa->pattern, pattern);
          expect_project_record(*o.img, s, order);
        }
      }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-04: errori deflate (§5), predittori non ammessi con interi, campione oltre 2^24 senza tabella (§4).
// Tetto di zlib iniettato e picco delle allocazioni: test_dng_u13b_hooks.cpp.
// ---------------------------------------------------------------------------------------------------------------------

fx::dng32_spec error_spec() {
  fx::dng32_spec s = spec32(34893, layouts()[4]);  // 3 × 2 tasselli 16 × 16 su 37 × 29
  set_area(s, 2, 2);
  return s;
}

bytes_t with_segments(const fx::dng32_spec& s, std::vector<bytes_t> segments) {
  fx::tiff_file f;
  f.chain.push_back(fx::make_deflate_ifd(s, fx::tiff_order::ii, std::move(segments)));
  return fx::write_tiff(f).bytes;
}

// Byte non compressi di un segmento (tassello 16 × 16 della fixture, Predictor 34893 applicato) per costruirne varianti.
bytes_t plain_tile(const fx::dng32_spec& s, std::size_t k) {
  fx::dng32_segment seg = fx::dng32_segment_values(s).at(k);
  fx::horizontal_difference(seg.values, seg.row_width, 4);
  return fx::to_file_bytes(seg.values, fx::tiff_order::ii);
}

void expect_inflate_stop(const observed& o, std::size_t segment, std::optional<int> code,
                         std::optional<std::string> msg = std::nullopt) {
  expect_stop(o, error_category::integrity, "raw.data_error");
  if (!o.stop) return;
  EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("inflate")) << o.stop->what();
  EXPECT_EQ(ctx(*o.stop, "segmento"), std::to_string(segment)) << o.stop->what();
  ASSERT_TRUE(ctx(*o.stop, "codice").has_value()) << o.stop->what();
  ASSERT_TRUE(ctx(*o.stop, "msg").has_value()) << o.stop->what();
  if (code) {
    EXPECT_EQ(ctx(*o.stop, "codice"), std::to_string(*code)) << o.stop->what();
  }
  if (msg) {
    EXPECT_EQ(ctx(*o.stop, "msg"), *msg) << o.stop->what();
  }
  EXPECT_EQ(static_cast<std::size_t>(o.inflate_calls), segment + 1) << "nessun segmento dopo quello non valido";
}

TEST(DngDeflate, T_U13_04_corrupted_streams_stop_with_zlib_code_and_message) {
  const fx::dng32_spec s = error_spec();
  const std::vector<bytes_t> good = fx::deflate_segments(s, fx::tiff_order::ii);
  ASSERT_EQ(good.size(), 6u);
  {
    SCOPED_TRACE("controllo: segmenti integri, immagine");
    expect_project_image(run(with_segments(s, good)), s);
  }
  constexpr std::size_t k = 1;
  {
    SCOPED_TRACE("bit alterato a metà del flusso");
    std::vector<bytes_t> segs = good;
    segs[k][segs[k].size() / 2] ^= 0x10;
    expect_inflate_stop(run(with_segments(s, segs)), k, std::nullopt);
  }
  {
    SCOPED_TRACE("Adler-32 alterato (ultimo byte del flusso)");
    std::vector<bytes_t> segs = good;
    segs[k].back() ^= 0x01;
    expect_inflate_stop(run(with_segments(s, segs)), k, Z_DATA_ERROR, std::string("incorrect data check"));
  }
  {
    SCOPED_TRACE("flusso corto: metà dei byte compressi");
    std::vector<bytes_t> segs = good;
    segs[k].resize(segs[k].size() / 2);
    expect_inflate_stop(run(with_segments(s, segs)), k, Z_BUF_ERROR);
  }
  {
    SCOPED_TRACE("uscita oltre il segmento: 4 byte in più");
    std::vector<bytes_t> segs = good;
    bytes_t plain = plain_tile(s, k);
    plain.insert(plain.end(), {1, 2, 3, 4});
    segs[k] = fx::zlib_stream(plain);
    expect_inflate_stop(run(with_segments(s, segs)), k, Z_BUF_ERROR);
  }
  {
    SCOPED_TRACE("uscita corta: 4 byte in meno, flusso completo");
    std::vector<bytes_t> segs = good;
    bytes_t plain = plain_tile(s, k);
    plain.resize(plain.size() - 4);
    segs[k] = fx::zlib_stream(plain);
    expect_inflate_stop(run(with_segments(s, segs)), k, Z_STREAM_END);
  }
  {
    // RFC 1950: CMF 0x78, FLG 0xBB (FDICT = 1, FLEVEL = 2, (0x78 · 256 + 0xBB) mod 31 = 0), poi DICTID; la nota tecnica
    // Adobe vieta il campo (contratto §1). inflate chiede il dizionario: Z_NEED_DICT.
    SCOPED_TRACE("bit FDICT nell'intestazione zlib");
    std::vector<bytes_t> segs = good;
    bytes_t body(good[k].begin() + 2, good[k].end());
    bytes_t fdict = {0x78, 0xBB, 0x00, 0x01, 0x00, 0x01};
    fdict.insert(fdict.end(), body.begin(), body.end());
    segs[k] = fdict;
    expect_inflate_stop(run(with_segments(s, segs)), k, Z_NEED_DICT);
  }
}

TEST(DngDeflate, T_U13_04_floating_point_predictors_with_integer_samples_are_nonconformant) {
  for (const std::uint16_t predictor : {std::uint16_t{3}, std::uint16_t{34894}, std::uint16_t{34895}}) {
    SCOPED_TRACE("Predictor " + std::to_string(predictor));
    fx::dng32_spec s = error_spec();
    s.predictor = predictor;
    const observed o = run(fx::make_deflate_dng(s));
    expect_stop(o, error_category::unsupported, "raw.dng_nonconformant");
    EXPECT_EQ(o.inflate_calls, 0);
    EXPECT_EQ(o.open_buffer_calls, 0);
  }
}

// §4: senza LinearizationTable ogni campione importato (dentro ActiveArea) deve essere ≤ 2^24 (esatto in f32); 2^24 + 1
// si ferma prima dell'immagine. Fuori da ActiveArea e nel riempimento dei tasselli i valori non si importano e non si
// controllano. Con la tabella il campione è una voce SHORT e il valore memorizzato non ha limiti.
TEST(DngDeflate, T_U13_04_sample_over_2_24_without_table_stops_with_sample_range) {
  for (const fx::tiff_order order : orders()) {
    SCOPED_TRACE(order_name(order));
    fx::dng32_spec s = error_spec();
    s.forced.push_back({10, 20, (1u << 24) + 1});
    const observed over = run(fx::make_deflate_dng(s, order));
    expect_stop(over, error_category::unsupported, "raw.sample_range");

    fx::dng32_spec ok = error_spec();
    ok.forced.push_back({10, 20, 1u << 24});
    expect_project_image(run(fx::make_deflate_dng(ok, order)), ok);

    // error_spec: ActiveArea {2, 2, 28, 36} su 37 × 29, tasselli 16 × 16 (righe 29-31 e colonne 37-47 di riempimento).
    for (const auto& [where, r, c] : std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t>>{
             {"fuori da ActiveArea (riga 0)", 0, 5},
             {"fuori da ActiveArea (ultima colonna)", 20, 36},
             {"riempimento del tassello di bordo in basso", 30, 5},
             {"riempimento del tassello di bordo a destra", 10, 40}}) {
      SCOPED_TRACE("2^32 − 1 " + where + ": non importato, nessuno stop");
      fx::dng32_spec s = error_spec();
      s.forced.push_back({r, c, 0xFFFFFFFFu});
      expect_project_image(run(fx::make_deflate_dng(s, order)), s);
    }
  }
  {
    SCOPED_TRACE("con LinearizationTable di 4096 voci: 2^32 − 1 e 2^24 + 1 danno l'ultima voce");
    fx::dng32_spec s = error_spec();
    s.forced.push_back({10, 20, 0xFFFFFFFFu});
    s.forced.push_back({11, 21, (1u << 24) + 1});
    std::vector<std::uint16_t> table(4096);
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = static_cast<std::uint16_t>(3 * i + 5);
    s.white = 12290;
    const bytes_t bytes = fx::make_deflate_dng(s, fx::tiff_order::ii, [&](fx::tiff_ifd& ifd) {
      fx::set_entry(ifd, fx::ent(50712, tt::short_, std::vector<std::uint64_t>(table.begin(), table.end())));
    });
    expect_project_image(run(bytes), s, table);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-05: equivalenza col gemello LibRaw (origini pari): stesso DNG non compresso a 16 bit con gli stessi valori.
// ---------------------------------------------------------------------------------------------------------------------

struct twin_case {
  std::string name;
  std::uint32_t x = 2, y = 2;  // origine di ActiveArea (pari)
  std::function<void(fx::tiff_ifd&)> tags;
  std::vector<std::uint16_t> table;
  std::uint32_t white = 65535;
  std::array<std::uint32_t, 4> black_written{0, 0, 0, 0};  // nero del sito (r mod 2, c mod 2) dell'area importata
  std::optional<std::string> curve_sha256;                  // digest di ADR-08.b §2.4; assente: identity
  std::function<void(fx::tiff_ifd&)> project_only;          // voci scritte solo nel file del decoder del progetto
};

std::vector<std::uint64_t> u64(const std::vector<std::uint16_t>& v) { return {v.begin(), v.end()}; }

std::vector<twin_case> twin_cases() {
  std::vector<twin_case> out;
  {
    twin_case c;
    c.name = "nero scalare 64 (SHORT), WhiteLevel 65535";
    c.tags = [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(50714, tt::short_, {64})); };
    c.black_written = {64, 64, 64, 64};
    out.push_back(c);
  }
  {
    twin_case c;
    c.name = "nero scalare RATIONAL 128/2 (progetto) e SHORT 64 (gemello)";
    c.tags = [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(50714, tt::short_, {64})); };
    c.black_written = {64, 64, 64, 64};
    c.project_only = [](fx::tiff_ifd& i) { fx::set_entry(i, fx::ent(50714, tt::rational, {128, 2})); };
    out.push_back(c);
  }
  {
    twin_case c;
    c.name = "motivo 2 × 2 {64, 72, 80, 96}, origine (0, 0)";
    c.x = 0;
    c.y = 0;
    c.tags = [](fx::tiff_ifd& i) {
      fx::set_entry(i, fx::ent(50713, tt::short_, {2, 2}));
      fx::set_entry(i, fx::ent(50714, tt::short_, {64, 72, 80, 96}));
    };
    c.black_written = {64, 72, 80, 96};
    out.push_back(c);
  }
  {
    twin_case c;
    c.name = "WhiteLevel 40000";
    c.white = 40000;
    out.push_back(c);
  }
  {
    twin_case c;
    c.name = "LinearizationTable identica di 65 536 voci";
    c.table.resize(65536);
    for (std::size_t i = 0; i < c.table.size(); ++i) c.table[i] = static_cast<std::uint16_t>(i);
    out.push_back(c);
  }
  {
    twin_case c;
    c.name = "LinearizationTable identica di 4096 voci";
    c.table.resize(4096);
    for (std::size_t i = 0; i < c.table.size(); ++i) c.table[i] = static_cast<std::uint16_t>(i);
    c.white = 4095;
    c.curve_sha256 = "2fe87d203699ad6efe1a4da3c45691a072f2d91b8fabea89bd0bfcb61fbf0264";
    out.push_back(c);
  }
  {
    twin_case c;
    c.name = "LinearizationTable 3i + 5 di 4096 voci";
    c.table.resize(4096);
    for (std::size_t i = 0; i < c.table.size(); ++i) c.table[i] = static_cast<std::uint16_t>(3 * i + 5);
    c.white = 12290;
    c.curve_sha256 = "a3089d2ad8be7b57af4ad35ff19bd2aa94fca3ac23a0f676cff7f47a826b6662";
    out.push_back(c);
  }
  return out;
}

// Nero del campione visibile (r, c) dalla formula di H3 (ADR-08.b §2.4) sulla rappresentazione dichiarata.
std::uint64_t h3_black(const image& img, std::uint32_t r, std::uint32_t c) {
  const decoder_state& s = *img.decoder_state;
  std::uint64_t b = s.black + s.black_per_color[s.cfa_color_index[(r % 2) * 2 + c % 2]];
  if (s.black_pattern_rows * s.black_pattern_cols > 0)
    b += s.black_pattern[(r % s.black_pattern_rows) * s.black_pattern_cols + (c % s.black_pattern_cols)];
  return b;
}

std::string hex(const std::array<std::uint8_t, 32>& d) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (const std::uint8_t b : d) {
    out += digits[b >> 4];
    out += digits[b & 15];
  }
  return out;
}

TEST(DngTwin, T_U13_05_project_decoder_equals_libraw_on_the_uncompressed_twin) {
  for (const twin_case& c : twin_cases()) {
    for (const std::size_t layout_index : {std::size_t{1}, std::size_t{4}}) {
      fx::dng32_spec p = spec32(2, layouts()[layout_index]);
      p.sample_max = 65535;
      p.white = c.white;
      set_area(p, c.x, c.y);
      fx::dng32_spec t = p;  // gemello: stessi valori, 16 bit, non compresso
      t.bits = 16;
      t.compression = 1;
      t.predictor = 1;
      const auto add_tags = [&](fx::tiff_ifd& ifd) {
        if (c.tags) c.tags(ifd);
        if (!c.table.empty()) fx::set_entry(ifd, fx::ent(50712, tt::short_, u64(c.table)));
      };
      SCOPED_TRACE(c.name + ", " + layouts()[layout_index].name);
      const bytes_t project = fx::make_deflate_dng(p, fx::tiff_order::ii, [&](fx::tiff_ifd& ifd) {
        add_tags(ifd);
        if (c.project_only) c.project_only(ifd);
      });
      fx::tiff_ifd twin_ifd = fx::make_dng32_ifd(t, fx::tiff_order::ii, [](const bytes_t& b) { return b; });
      add_tags(twin_ifd);
      fx::tiff_file twin_file;
      twin_file.chain.push_back(std::move(twin_ifd));
      const observed a = run(project);
      const observed b = run(fx::write_tiff(twin_file).bytes);
      ASSERT_TRUE(b.img.has_value()) << "gemello LibRaw: " << describe(b);
      ASSERT_EQ(b.routes.size(), 1u);
      EXPECT_EQ(b.routes[0].decoder, raw_decoder_kind::libraw);
      expect_project_image(a, p, c.table);
      if (!a.img) continue;
      const image& x = *a.img;
      const image& y = *b.img;
      EXPECT_EQ(x.geometry, y.geometry);
      EXPECT_EQ(x.cfa, y.cfa);
      ASSERT_TRUE(x.decoder_state && y.decoder_state);
      EXPECT_EQ(x.decoder_state->window, y.decoder_state->window);
      EXPECT_EQ(x.decoder_state->cfa_color_index, y.decoder_state->cfa_color_index);
      EXPECT_EQ(x.decoder_state->white, y.decoder_state->white);
      EXPECT_EQ(x.decoder_state->white, c.white);
      EXPECT_EQ(x.decoder_state->linearization, y.decoder_state->linearization);
      EXPECT_EQ(x.decoder_state->curve_sha256, y.decoder_state->curve_sha256);
      if (c.curve_sha256) {
        EXPECT_EQ(x.decoder_state->linearization, decoder_linearization::decoder_curve);
        ASSERT_TRUE(x.decoder_state->curve_sha256.has_value());
        EXPECT_EQ(hex(*x.decoder_state->curve_sha256), *c.curve_sha256);
      } else {
        EXPECT_EQ(x.decoder_state->linearization, decoder_linearization::identity);
        EXPECT_FALSE(x.decoder_state->curve_sha256.has_value());
      }
      ASSERT_EQ(x.samples.size(), y.samples.size());
      EXPECT_EQ(std::memcmp(x.samples.data(), y.samples.data(), x.samples.size()), 0) << "campioni diversi dal gemello";
      std::uint64_t black_diffs = 0;
      for (std::uint32_t r = 0; r < x.geometry.height; ++r)
        for (std::uint32_t col = 0; col < x.geometry.width; ++col) {
          const std::uint64_t want = c.black_written[(r % 2) * 2 + col % 2];
          if (h3_black(x, r, col) != want || h3_black(y, r, col) != want) ++black_diffs;
        }
      EXPECT_EQ(black_diffs, 0u) << "nero per sito (H3) diverso da quello scritto";
      const json rx = raw_decoder_record(x), ry = raw_decoder_record(y);
      for (std::string_view key : {rk::linearization, rk::curve_sha256, rk::crop, rk::cfa_pattern, rk::white}) {
        ASSERT_NE(member(rx, key), nullptr) << key;
        ASSERT_NE(member(ry, key), nullptr) << key;
        EXPECT_EQ(canonical_json(*member(rx, key)), canonical_json(*member(ry, key))) << key;
      }
      expect_project_record(x, p, fx::tiff_order::ii);
    }
  }
}

// Motivo di nero: rappresentazione di §4 (nero scalare 0, per colore 0, motivo R × C nell'ordine del file, ancorato
// all'origine dell'area importata) anche con origine dispari, dove il percorso LibRaw si ferma (raw.black_pattern_anchor).
TEST(DngTwin, T_U13_05_black_pattern_is_anchored_at_the_imported_origin_also_when_odd) {
  for (const auto& [x, y] : origins()) {
    fx::dng32_spec s = spec32(1, layouts()[4]);
    set_area(s, x, y);
    SCOPED_TRACE("origine (" + std::to_string(x) + ", " + std::to_string(y) + ")");
    const observed o = run(fx::make_deflate_dng(s, fx::tiff_order::mm, [](fx::tiff_ifd& ifd) {
      fx::set_entry(ifd, fx::ent(50713, tt::short_, {2, 2}));
      fx::set_entry(ifd, fx::ent(50714, tt::long_, {64, 72, 80, 96}));
    }));
    expect_project_image(o, s);
    if (!o.img) continue;
    const decoder_state& st = *o.img->decoder_state;
    EXPECT_EQ(st.black, 0u);
    EXPECT_EQ(st.black_per_color, (std::array<std::uint32_t, 4>{0, 0, 0, 0}));
    EXPECT_EQ(st.black_pattern_rows, 2u);
    EXPECT_EQ(st.black_pattern_cols, 2u);
    EXPECT_EQ(st.black_pattern, (std::vector<std::uint32_t>{64, 72, 80, 96}));
  }
  {
    SCOPED_TRACE("nero scalare: valore in black, motivo vuoto");
    fx::dng32_spec s = spec32(1, layouts()[4]);
    set_area(s, 1, 1);
    const observed o = run(fx::make_deflate_dng(s, fx::tiff_order::ii, [](fx::tiff_ifd& ifd) {
      fx::set_entry(ifd, fx::ent(50714, tt::long_, {70000}));
    }));
    expect_project_image(o, s);
    if (o.img) {
      const decoder_state& st = *o.img->decoder_state;
      EXPECT_EQ(st.black, 70000u);
      EXPECT_EQ(st.black_per_color, (std::array<std::uint32_t, 4>{0, 0, 0, 0}));
      EXPECT_EQ(st.black_pattern_rows, 0u);
      EXPECT_EQ(st.black_pattern_cols, 0u);
      EXPECT_TRUE(st.black_pattern.empty());
    }
  }
}

// T-U13-06, riga BlackLevel di §4: `R · C` > 4098 (limite del motivo del nero di decoder_state, ADR-03.b §b.2) → stop
// `unsupported`, `raw.black_level`; `R · C` = 4098 accettato, motivo nell'ordine del file. 4099 è primo: 1 × 4099 e
// 4099 × 1.
TEST(DngLevels, T_U13_06_black_pattern_of_4098_values_is_accepted_and_4099_stops) {
  const auto values = [](std::size_t n) {
    std::vector<std::uint64_t> v(n);
    for (std::size_t k = 0; k < n; ++k) v[k] = 100 + (k * 7) % 1000;
    return v;
  };
  const auto with_black = [&](std::uint64_t rows, std::uint64_t cols) {
    return fx::make_deflate_dng(spec32(1, layouts()[4]), fx::tiff_order::ii, [&](fx::tiff_ifd& ifd) {
      fx::set_entry(ifd, fx::ent(50713, tt::short_, {rows, cols}));
      fx::set_entry(ifd, fx::ent(50714, tt::short_, values(rows * cols)));
    });
  };
  for (const auto& [rows, cols] : std::vector<std::pair<std::uint32_t, std::uint32_t>>{{1, 4098}, {2, 2049}, {4098, 1}}) {
    SCOPED_TRACE("accettato: " + std::to_string(rows) + " × " + std::to_string(cols));
    const observed o = run(with_black(rows, cols));
    expect_project_image(o, spec32(1, layouts()[4]));
    if (!o.img) continue;
    const decoder_state& st = *o.img->decoder_state;
    EXPECT_EQ(st.black, 0u);
    EXPECT_EQ(st.black_pattern_rows, rows);
    EXPECT_EQ(st.black_pattern_cols, cols);
    const std::vector<std::uint64_t> written = values(std::size_t{rows} * cols);
    EXPECT_EQ(st.black_pattern, std::vector<std::uint32_t>(written.begin(), written.end()));
  }
  for (const auto& [rows, cols] : std::vector<std::pair<std::uint32_t, std::uint32_t>>{{1, 4099}, {4099, 1}}) {
    SCOPED_TRACE("stop: " + std::to_string(rows) + " × " + std::to_string(cols));
    const observed o = run(with_black(rows, cols));
    expect_stop(o, error_category::unsupported, "raw.black_level");
    if (o.stop) {
      EXPECT_EQ(ctx(*o.stop, "tag"), std::string("50714"));
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-07: metadati (§4), ora con timegm sotto TZ=UTC0 (main_utc0.cpp), nessuna chiamata a mktime nella decodifica.
// ---------------------------------------------------------------------------------------------------------------------

std::vector<std::uint64_t> ascii(std::string_view text) {
  std::vector<std::uint64_t> out(text.begin(), text.end());
  out.push_back(0);
  return out;
}

struct exif_tags {
  std::optional<fx::tiff_entry> exposure = fx::ent(33434, tt::rational, {60, 1});
  std::optional<fx::tiff_entry> iso = fx::ent(34855, tt::short_, {800});
  std::optional<fx::tiff_entry> datetime = fx::ent(36867, tt::ascii, ascii("2026:10:30 12:34:56"));
  std::optional<fx::tiff_entry> make = fx::ent(271, tt::ascii, ascii("AstroTest"));
  std::optional<fx::tiff_entry> model = fx::ent(272, tt::ascii, ascii("SYN-DNG-02"));
};

fx::dng32_spec meta_spec() {
  fx::dng32_spec s = spec32(1, layouts()[4], 32, 32);
  set_area(s, 2, 2);
  return s;
}

// IFD0 raw con Make e Model e l'IFD EXIF (34665) con ExposureTime, PhotographicSensitivity, DateTimeOriginal.
bytes_t meta_file(const exif_tags& m, const std::function<void(fx::tiff_ifd&)>& more = {}) {
  return fx::make_deflate_dng(meta_spec(), fx::tiff_order::ii, [&](fx::tiff_ifd& ifd) {
    if (m.make) fx::set_entry(ifd, *m.make);
    if (m.model) fx::set_entry(ifd, *m.model);
    fx::tiff_ifd exif;
    for (const auto* e : {&m.exposure, &m.iso, &m.datetime})
      if (*e) exif.entries.push_back(**e);
    if (!exif.entries.empty()) ifd.exif.push_back(std::move(exif));
    if (more) more(ifd);
  });
}

json record_of(const observed& o) { return o.img ? raw_decoder_record(*o.img) : json::null(); }

TEST(DngMetadata, T_U13_07_exif_make_model_and_capture_time) {
  const std::vector<std::pair<std::string, std::int64_t>> dates = {
      {"2026:10:30 12:34:56", 1793363696}, {"2026:03:29 02:30:00", 1774751400}, {"2026:10:25 02:30:00", 1792895400}};
  for (const auto& [text, expected] : dates) {
    SCOPED_TRACE(text);
    exif_tags m;
    m.datetime = fx::ent(36867, tt::ascii, ascii(text));
    const observed o = run(meta_file(m));
    expect_project_image(o, meta_spec());
    if (!o.img) continue;
    const frame_metadata& md = o.img->metadata;
    ASSERT_TRUE(md.capture_time_libraw.has_value());
    EXPECT_EQ(md.capture_time_libraw->value, expected);
    EXPECT_EQ(md.capture_time_libraw->source.origin, metadata_origin::decoder);
    ASSERT_TRUE(md.exposure_s.has_value());
    EXPECT_EQ(md.exposure_s->value, 60.0);
    EXPECT_EQ(md.exposure_s->source.origin, metadata_origin::decoder);
    ASSERT_TRUE(md.gain.has_value());
    EXPECT_EQ(md.gain->value, 800.0);
    EXPECT_EQ(md.gain->source.origin, metadata_origin::decoder);
    ASSERT_TRUE(md.camera.has_value());
    EXPECT_EQ(md.camera->value, "AstroTest SYN-DNG-02");
    EXPECT_EQ(md.camera->source.origin, metadata_origin::decoder);
    EXPECT_FALSE(md.temperature_c.has_value());
    const json rec = record_of(o);
    EXPECT_EQ(integer_of(rec, rk::capture_time_libraw), expected);
    EXPECT_EQ(string_of(rec, rk::capture_time_clock), std::string("camera_no_zone"));
    EXPECT_TRUE(strings_of(rec, rk::metadata_rejected).empty());
    const std::vector<std::string> present = strings_of(rec, rk::metadata_present);
    for (const char* name : {"exposure_s", "gain", "camera", "capture_time_libraw"})
      EXPECT_NE(std::find(present.begin(), present.end(), name), present.end()) << name;
    expect_project_record(*o.img, meta_spec(), fx::tiff_order::ii);
  }
  {
    SCOPED_TRACE("ExposureTime 1/4; Make e Model troncati al primo NUL");
    exif_tags m;
    m.exposure = fx::ent(33434, tt::rational, {1, 4});
    m.make = fx::ent(271, tt::ascii, ascii(std::string_view("Acme\0XYZ", 8)));
    m.model = fx::ent(272, tt::ascii, ascii(std::string_view("M1\0\0", 4)));
    const observed o = run(meta_file(m));
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    ASSERT_TRUE(o.img->metadata.exposure_s.has_value());
    EXPECT_EQ(o.img->metadata.exposure_s->value, 0.25);
    ASSERT_TRUE(o.img->metadata.camera.has_value());
    EXPECT_EQ(o.img->metadata.camera->value, "Acme M1");
  }
  // Un solo tag: il valore di quel tag, senza spazio (stessa regola del percorso LibRaw, raw_decode.cpp).
  for (const bool keep_make : {true, false}) {
    SCOPED_TRACE(keep_make ? "solo Make" : "solo Model");
    exif_tags m;
    if (keep_make) {
      m.model.reset();
    } else {
      m.make.reset();
    }
    const observed o = run(meta_file(m));
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    ASSERT_TRUE(o.img->metadata.camera.has_value());
    EXPECT_EQ(o.img->metadata.camera->value, keep_make ? "AstroTest" : "SYN-DNG-02");
  }
}

TEST(DngMetadata, T_U13_07_absent_tags_leave_fields_absent) {
  exif_tags m;
  m.exposure.reset();
  m.iso.reset();
  m.datetime.reset();
  m.make.reset();
  m.model.reset();
  const observed o = run(meta_file(m));
  expect_project_image(o, meta_spec());
  if (!o.img) return;
  const frame_metadata& md = o.img->metadata;
  EXPECT_FALSE(md.exposure_s.has_value());
  EXPECT_FALSE(md.gain.has_value());
  EXPECT_FALSE(md.capture_time_libraw.has_value());
  EXPECT_FALSE(md.camera.has_value());
  EXPECT_FALSE(md.temperature_c.has_value());
  const json rec = record_of(o);
  EXPECT_TRUE(is_null(rec, rk::capture_time_libraw));
  EXPECT_TRUE(strings_of(rec, rk::metadata_rejected).empty());
  for (std::string_view absent : {rk::sensor, rk::pixel_size_um, rk::pixel_scale_arcsec, rk::wcs})
    EXPECT_TRUE(is_null(rec, absent)) << absent;
  EXPECT_EQ(string_of(rec, rk::absent_reason), std::string("not_provided_by_decoder"));
}

// Tag malformato (tipo, conteggio, data non nel formato o fuori calendario): campo assente, nome TIFF in
// metadata_rejected, gli altri campi presenti e l'immagine prodotta.
TEST(DngMetadata, T_U13_07_malformed_tags_are_rejected_by_name) {
  struct bad {
    std::string name;
    std::function<void(exif_tags&)> edit;
    std::string tag;
  };
  const std::vector<bad> cases = {
      {"data con trattini", [](exif_tags& m) { m.datetime = fx::ent(36867, tt::ascii, ascii("2026-10-30 12:34:56")); },
       "DateTimeOriginal"},
      {"data fuori calendario 2026:02:30", [](exif_tags& m) { m.datetime = fx::ent(36867, tt::ascii, ascii("2026:02:30 00:00:00")); },
       "DateTimeOriginal"},
      {"ora 25:00:00", [](exif_tags& m) { m.datetime = fx::ent(36867, tt::ascii, ascii("2026:10:30 25:00:00")); },
       "DateTimeOriginal"},
      {"data troncata", [](exif_tags& m) { m.datetime = fx::ent(36867, tt::ascii, ascii("2026:10:30 12:34")); },
       "DateTimeOriginal"},
      {"DateTimeOriginal di tipo SHORT", [](exif_tags& m) { m.datetime = fx::ent(36867, tt::short_, {2026}); },
       "DateTimeOriginal"},
      {"ExposureTime di tipo SHORT", [](exif_tags& m) { m.exposure = fx::ent(33434, tt::short_, {60}); }, "ExposureTime"},
      {"ExposureTime con 2 valori", [](exif_tags& m) { m.exposure = fx::ent(33434, tt::rational, {60, 1, 30, 1}); },
       "ExposureTime"},
      {"PhotographicSensitivity di tipo ASCII", [](exif_tags& m) { m.iso = fx::ent(34855, tt::ascii, ascii("800")); },
       "PhotographicSensitivity"},
  };
  for (const bad& c : cases) {
    SCOPED_TRACE(c.name);
    exif_tags m;
    c.edit(m);
    const observed o = run(meta_file(m));
    expect_project_image(o, meta_spec());
    if (!o.img) continue;
    const frame_metadata& md = o.img->metadata;
    EXPECT_EQ(md.capture_time_libraw.has_value(), c.tag != "DateTimeOriginal");
    EXPECT_EQ(md.exposure_s.has_value(), c.tag != "ExposureTime");
    EXPECT_EQ(md.gain.has_value(), c.tag != "PhotographicSensitivity");
    EXPECT_TRUE(md.camera.has_value());
    EXPECT_EQ(strings_of(record_of(o), rk::metadata_rejected), std::vector<std::string>{c.tag});
  }
}

// Puntatore ExifIFD (34665) malformato per tipo o conteggio (§4: tag malformato → nome in `metadata_rejected`): la tabella
// EXIF non si legge, quindi i campi che ne vengono restano assenti come per tag assenti; `camera` (IFD0) resta presente.
// Il caso SHORT punta alla tabella EXIF vera (stesso offset, < 2^16), così un tipo SHORT accettato la leggerebbe.
bytes_t with_exif_pointer_type(bytes_t b, std::uint16_t type) {
  const auto u16 = [&b](std::size_t at) { return static_cast<std::uint32_t>(b.at(at) | (b.at(at + 1) << 8)); };
  const auto u32 = [&](std::size_t at) { return u16(at) | (u16(at + 2) << 16); };
  if (b.at(0) != 'I' || b.at(1) != 'I') return {};
  const std::size_t table = u32(4);
  for (std::uint32_t k = 0; k < u16(table); ++k) {
    const std::size_t e = table + 2 + 12 * std::size_t{k};
    if (u16(e) != 34665) continue;
    if (u16(e + 2) != tt::long_ || u32(e + 4) != 1 || u32(e + 8) >= 65536) return {};
    b.at(e + 2) = static_cast<std::uint8_t>(type);
    b.at(e + 3) = 0;
    return b;
  }
  return {};
}

TEST(DngMetadata, T_U13_07_malformed_exif_pointer_is_rejected_by_name) {
  const exif_tags full;
  const bytes_t short_pointer = with_exif_pointer_type(meta_file(full), tt::short_);
  ASSERT_FALSE(short_pointer.empty()) << "voce 34665 LONG con un offset < 2^16 non trovata in IFD0";
  const auto replaced = [&full](const fx::tiff_entry& pointer) {
    return meta_file(full, [&pointer](fx::tiff_ifd& ifd) {
      ifd.exif.clear();
      fx::set_entry(ifd, pointer);
    });
  };
  const std::vector<std::pair<std::string, bytes_t>> cases = {
      {"ExifIFD di tipo SHORT con l'offset della tabella EXIF", short_pointer},
      {"ExifIFD di tipo ASCII", replaced(fx::ent(34665, tt::ascii, ascii("8")))},
      {"ExifIFD con 2 valori LONG", replaced(fx::ent(34665, tt::long_, {8, 8}))},
  };
  for (const auto& [name, bytes] : cases) {
    SCOPED_TRACE(name);
    const observed o = run(bytes);
    expect_project_image(o, meta_spec());
    if (!o.img) continue;
    const frame_metadata& md = o.img->metadata;
    EXPECT_FALSE(md.exposure_s.has_value());
    EXPECT_FALSE(md.gain.has_value());
    EXPECT_FALSE(md.capture_time_libraw.has_value());
    ASSERT_TRUE(md.camera.has_value());
    EXPECT_EQ(md.camera->value, "AstroTest SYN-DNG-02");
    const json rec = record_of(o);
    EXPECT_EQ(strings_of(rec, rk::metadata_rejected), std::vector<std::string>{"ExifIFD"});
    EXPECT_TRUE(is_null(rec, rk::capture_time_libraw));
  }
  {
    SCOPED_TRACE("controllo della fixture: lo stesso file con il puntatore LONG legge la tabella EXIF");
    const observed o = run(meta_file(full));
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    EXPECT_TRUE(o.img->metadata.exposure_s.has_value());
  }
}

// PhotographicSensitivity (34855): EXIF 2.32 dichiara conteggio «Any» e il primo valore è la sensibilità; un conteggio 2
// non è malformato e `gain` è il primo valore.
TEST(DngMetadata, T_U13_07_photographic_sensitivity_with_two_values_gives_the_first) {
  for (const auto& [values, expected] : std::vector<std::pair<std::vector<std::uint64_t>, double>>{
           {{1600, 200}, 1600.0}, {{100, 6400}, 100.0}, {{400, 400, 3}, 400.0}}) {
    SCOPED_TRACE(std::to_string(values.size()) + " valori, primo " + std::to_string(values[0]));
    exif_tags m;
    m.iso = fx::ent(34855, tt::short_, values);
    const observed o = run(meta_file(m));
    expect_project_image(o, meta_spec());
    if (!o.img) continue;
    ASSERT_TRUE(o.img->metadata.gain.has_value());
    EXPECT_EQ(o.img->metadata.gain->value, expected);
    EXPECT_EQ(o.img->metadata.gain->source.origin, metadata_origin::decoder);
    EXPECT_TRUE(strings_of(record_of(o), rk::metadata_rejected).empty());
  }
}

TEST(DngMetadata, T_U13_07_project_decode_never_calls_mktime) {
  {
    SCOPED_TRACE("controllo dello strumento: una chiamata propria del test");
    const std::uint64_t before = mktime_calls();
    std::tm t{};
    t.tm_year = 126;
    t.tm_mday = 1;
    t.tm_isdst = -1;
    EXPECT_NE(std::mktime(&t), static_cast<std::time_t>(-1));
    EXPECT_EQ(mktime_calls() - before, 1u) << "l'interposizione non vede mktime";
  }
  // Intervallo: dall'avviso dell'instradamento (dopo la verifica del fuso, che chiama mktime per contratto) alla fine.
  std::optional<std::uint64_t> at_route;
  const observed o = run(meta_file(exif_tags{}), {}, [&](raw_decode_hooks& h) {
    h.after_route = [&, previous = h.after_route](const dng_route_result& r) {
      previous(r);
      at_route = mktime_calls();
    };
  });
  const std::uint64_t at_end = mktime_calls();
  ASSERT_EQ(o.routes.size(), 1u);
  expect_project_image(o, meta_spec());
  ASSERT_TRUE(at_route.has_value());
  EXPECT_EQ(at_end - *at_route, 0u) << "mktime chiamata dalla decodifica del progetto";
}

// Orientation, DefaultCropOrigin e DefaultCropSize registrati e non applicati; OpcodeList1-3 registrati e non applicati.
TEST(DngMetadata, T_U13_07_orientation_default_crop_and_opcode_lists_are_recorded_not_applied) {
  const bytes_t plain = meta_file(exif_tags{});
  const observed base = run(plain);
  expect_project_image(base, meta_spec());
  const bytes_t tagged = meta_file(exif_tags{}, [](fx::tiff_ifd& ifd) {
    fx::set_entry(ifd, fx::ent(274, tt::short_, {6}));
    fx::set_entry(ifd, fx::ent(50719, tt::long_, {2, 3}));
    fx::set_entry(ifd, fx::ent(50720, tt::long_, {20, 18}));
    for (const std::uint16_t tag : {std::uint16_t{51008}, std::uint16_t{51009}, std::uint16_t{51022}})
      fx::set_entry(ifd, fx::ent(tag, tt::undefined, {0, 0, 0, 0}));  // lista di opcode vuota (conteggio 0, big-endian)
  });
  const observed o = run(tagged);
  expect_project_image(o, meta_spec());
  if (!base.img || !o.img) return;
  ASSERT_EQ(o.img->samples.size(), base.img->samples.size());
  EXPECT_EQ(std::memcmp(o.img->samples.data(), base.img->samples.data(), o.img->samples.size()), 0);
  EXPECT_EQ(o.img->geometry, base.img->geometry);
  const json rec = record_of(o);
  EXPECT_EQ(integer_of(rec, rk::orientation), 6);
  ASSERT_NE(member(rec, rk::default_crop_origin), nullptr);
  ASSERT_NE(member(rec, rk::default_crop_size), nullptr);
  EXPECT_EQ(canonical_json(*member(rec, rk::default_crop_origin)), "[2,3]");
  EXPECT_EQ(canonical_json(*member(rec, rk::default_crop_size)), "[20,18]");
  EXPECT_EQ(strings_of(rec, rk::opcode_lists), (std::vector<std::string>{"OpcodeList1", "OpcodeList2", "OpcodeList3"}));
  const json plain_rec = record_of(base);
  EXPECT_TRUE(strings_of(plain_rec, rk::opcode_lists).empty());
  EXPECT_TRUE(is_null(plain_rec, rk::default_crop_origin));
  EXPECT_TRUE(is_null(plain_rec, rk::default_crop_size));
}

// DefaultCropOrigin e DefaultCropSize RATIONAL (DNG 1.7.1.0: SHORT, LONG o RATIONAL) registrati come scritti
// (raw_record_keys.hpp): un valore che si divide come intero, uno non intero come "num/den" dei due interi scritti, non
// ridotti; campioni e geometria invariati.
TEST(DngMetadata, T_U13_07_rational_default_crop_is_recorded_as_written) {
  const observed base = run(meta_file(exif_tags{}));
  expect_project_image(base, meta_spec());
  const observed o = run(meta_file(exif_tags{}, [](fx::tiff_ifd& ifd) {
    fx::set_entry(ifd, fx::ent(50719, tt::rational, {5, 2, 8, 2}));
    fx::set_entry(ifd, fx::ent(50720, tt::rational, {42, 4, 36, 3}));
  }));
  expect_project_image(o, meta_spec());
  if (!base.img || !o.img) return;
  EXPECT_EQ(o.img->geometry, base.img->geometry);
  ASSERT_EQ(o.img->samples.size(), base.img->samples.size());
  EXPECT_EQ(std::memcmp(o.img->samples.data(), base.img->samples.data(), o.img->samples.size()), 0);
  const json rec = record_of(o);
  ASSERT_NE(member(rec, rk::default_crop_origin), nullptr);
  ASSERT_NE(member(rec, rk::default_crop_size), nullptr);
  EXPECT_EQ(canonical_json(*member(rec, rk::default_crop_origin)), "[\"5/2\",4]");
  EXPECT_EQ(canonical_json(*member(rec, rk::default_crop_size)), "[\"42/4\",12]");
  EXPECT_TRUE(strings_of(rec, rk::metadata_rejected).empty());
}

// Make, Model ed EXIF stanno in IFD0 anche quando l'IFD raw è in `SubIFDs` (anteprima in IFD0).
TEST(DngMetadata, T_U13_07_metadata_are_read_from_ifd0_when_the_raw_ifd_is_a_subifd) {
  fx::dng32_spec s = meta_spec();
  fx::tiff_ifd raw = fx::make_deflate_ifd(s, fx::tiff_order::ii);
  fx::remove_entry(raw, 50706);
  fx::remove_entry(raw, 50707);
  fx::tiff_ifd ifd0;
  ifd0.entries = {fx::ent(254, tt::long_, {1}),      fx::ent(256, tt::long_, {4}),
                  fx::ent(257, tt::long_, {4}),      fx::ent(258, tt::short_, {8, 8, 8}),
                  fx::ent(259, tt::short_, {1}),     fx::ent(262, tt::short_, {2}),
                  fx::ent(271, tt::ascii, ascii("AstroTest")), fx::ent(272, tt::ascii, ascii("SYN-DNG-02")),
                  {273, tt::long_, {0}, {}, {}, true, false}, fx::ent(277, tt::short_, {3}),
                  fx::ent(278, tt::long_, {4}),      {279, tt::long_, {0}, {}, {}, false, true},
                  fx::ent(50706, tt::byte_, {1, 7, 1, 0}), fx::ent(50707, tt::byte_, {1, 4, 0, 0})};
  ifd0.segments.push_back(bytes_t(48, 0x40));
  fx::tiff_ifd exif;
  exif.entries = {fx::ent(33434, tt::rational, {30, 1}), fx::ent(36867, tt::ascii, ascii("2026:10:30 12:34:56"))};
  ifd0.exif.push_back(std::move(exif));
  ifd0.sub_ifds.push_back(std::move(raw));
  fx::tiff_file f;
  f.chain.push_back(std::move(ifd0));
  const observed o = run(fx::write_tiff(f).bytes);
  expect_project_image(o, s);
  if (!o.img) return;
  ASSERT_TRUE(o.img->metadata.camera.has_value());
  EXPECT_EQ(o.img->metadata.camera->value, "AstroTest SYN-DNG-02");
  ASSERT_TRUE(o.img->metadata.exposure_s.has_value());
  EXPECT_EQ(o.img->metadata.exposure_s->value, 30.0);
  ASSERT_TRUE(o.img->metadata.capture_time_libraw.has_value());
  EXPECT_EQ(o.img->metadata.capture_time_libraw->value, 1793363696);
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-08: limiti e memoria (§7).
// ---------------------------------------------------------------------------------------------------------------------

// max_file_bytes prima dell'instradamento: il buffer è una mappa PROT_NONE, qualunque lettura terminerebbe il processo.
TEST(DngLimits, T_U13_08_file_over_max_file_bytes_stops_before_the_route) {
  const std::size_t over = static_cast<std::size_t>(raw_limits::max_file_bytes) + 1;
  void* base = ::mmap(nullptr, over, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  ASSERT_NE(base, MAP_FAILED);
  const std::span<const std::uint8_t> bytes(static_cast<const std::uint8_t*>(base), over);
  int routes = 0;
  raw_decode_hooks hooks;
  hooks.after_route = [&](const dng_route_result&) { ++routes; };
  std::optional<stop_error> stop;
  try {
    (void)decode_raw_buffer(bytes, hooks, {});
  } catch (const stop_error& e) {
    stop.emplace(e);
  }
  ::munmap(base, over);
  ASSERT_TRUE(stop.has_value());
  EXPECT_EQ(stop->category(), error_category::limit_exceeded);
  EXPECT_EQ(stop->code(), "raw.max_file_bytes");
  EXPECT_EQ(routes, 0);
}

TEST(DngLimits, T_U13_08_max_pixels_on_image_and_segment_before_any_inflate) {
  {
    fx::dng32_spec s = spec32(2, layouts()[1]);
    const bytes_t bytes = fx::make_deflate_dng(s);
    raw_decode_context c;
    c.max_pixels = std::uint64_t{s.width} * s.height - 1;
    {
      SCOPED_TRACE("immagine: ImageWidth · ImageLength = max_pixels + 1");
      const observed o = run(bytes, c);
      expect_stop(o, error_category::limit_exceeded, "raw.max_pixels");
      EXPECT_EQ(o.inflate_calls, 0);
    }
    c.max_pixels += 1;
    SCOPED_TRACE("immagine: ImageWidth · ImageLength = max_pixels");
    expect_project_image(run(bytes, c), s);
  }
  {
    fx::dng32_spec s = spec32(2, layouts()[5]);  // tassello 256 × 256 su 37 × 29
    const bytes_t bytes = fx::make_deflate_dng(s);
    raw_decode_context c;
    c.max_pixels = 256 * 256 - 1;
    {
      SCOPED_TRACE("segmento: TileWidth · TileLength = max_pixels + 1, immagine sotto il limite");
      const observed o = run(bytes, c);
      expect_stop(o, error_category::limit_exceeded, "raw.max_pixels");
      EXPECT_EQ(o.inflate_calls, 0);
    }
    c.max_pixels += 1;
    SCOPED_TRACE("segmento: TileWidth · TileLength = max_pixels");
    expect_project_image(run(bytes, c), s);
  }
}

// Prenotazione di §7 calcolata dalle dimensioni scritte: 6 · w · h (area importata) + S (segmento: TW · TL · 4 oppure
// ImageWidth · min(RowsPerStrip, ImageLength) · 4) + 65 536 (tetto di zlib). Il pool tiene già i byte del file.
std::uint64_t expected_reservation(const fx::dng32_spec& s) {
  const std::uint64_t w = s.active_area.right - s.active_area.left, h = s.active_area.bottom - s.active_area.top;
  const std::uint64_t seg = s.tiles ? std::uint64_t{s.tile_width} * s.tile_length * 4
                                    : std::uint64_t{s.width} * std::min(s.rows_per_strip, s.height) * 4;
  return 6 * w * h + seg + 65536;
}

TEST(DngLimits, T_U13_08_reservation_equals_the_formula_and_one_byte_less_stops) {
  for (const std::size_t li : {std::size_t{1}, std::size_t{3}, std::size_t{4}, std::size_t{5}}) {
    fx::dng32_spec s = spec32(34892, layouts()[li]);
    set_area(s, 3, 0);
    const bytes_t bytes = fx::make_deflate_dng(s);
    const std::uint64_t file = bytes.size();
    const std::uint64_t need = expected_reservation(s);
    SCOPED_TRACE(std::string(layouts()[li].name) + ", prenotazione attesa " + std::to_string(need));
    for (const std::uint64_t budget : {file + need, file + need - 1}) {
      memory_pool pool(budget);
      std::optional<reservation> file_bytes = pool.try_reserve(file, "file");
      ASSERT_TRUE(file_bytes.has_value());
      raw_decode_context c;
      c.pool = &pool;
      c.held_bytes = file;
      const observed o = run(bytes, c);
      if (budget == file + need) {
        expect_project_image(o, s);
        EXPECT_EQ(pool.peak_reserved_bytes(), file + need) << "prenotazione diversa dalla formula di §7";
      } else {
        expect_stop(o, error_category::budget, "sched.job_exceeds_budget");
        EXPECT_EQ(o.inflate_calls, 0);
      }
      EXPECT_EQ(pool.reserved_bytes(), file) << "prenotazione del decoder non rilasciata";
      file_bytes->release();
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-12: determinismo e record (§8). Le chiavi si confrontano sullo stesso digest del file (quello della base), come
// T-M3A-RAW-12: la differenza viene solo dal record.
// ---------------------------------------------------------------------------------------------------------------------

json with_field_changed(const json& record, const std::string& key) {
  json out = json::object();
  for (const std::string& k : record.keys()) {
    const json& v = *record.find(k);
    if (k != key) {
      out.set(k, v);
      continue;
    }
    switch (v.type()) {
      case json::kind::string: out.set(k, json::string(v.as_string() + "x")); break;
      case json::kind::integer: out.set(k, json::integer(v.as_integer() + 1)); break;
      case json::kind::boolean: out.set(k, json::boolean(!v.as_bool())); break;
      default: out.set(k, json::string("cambiato")); break;
    }
  }
  return out;
}

fx::dng32_spec record_spec() {
  fx::dng32_spec s = spec32(34893, layouts()[4]);
  set_area(s, 2, 2);
  return s;
}

TEST(DngRecord, T_U13_12_two_decodes_are_identical) {
  const bytes_t bytes = meta_file(exif_tags{});
  const observed a = run(bytes), b = run(bytes);
  ASSERT_TRUE(a.img && b.img) << describe(a) << " / " << describe(b);
  ASSERT_EQ(a.img->samples.size(), b.img->samples.size());
  EXPECT_EQ(std::memcmp(a.img->samples.data(), b.img->samples.data(), a.img->samples.size()), 0);
  EXPECT_TRUE(*a.img == *b.img);
  const json ra = raw_decoder_record(*a.img), rb = raw_decoder_record(*b.img);
  EXPECT_EQ(canonical_json(ra), canonical_json(rb));
  const content_digest d = digest_of(bytes);
  EXPECT_EQ(make_raw_import_node(d, ra).key(), make_raw_import_node(d, rb).key());
}

TEST(DngRecord, T_U13_12_every_record_field_changes_the_node_key) {
  const bytes_t bytes = meta_file(exif_tags{});
  const observed o = run(bytes);
  ASSERT_TRUE(o.img.has_value()) << describe(o);
  const json rec = raw_decoder_record(*o.img);
  const content_digest d = digest_of(bytes);
  const std::string base = make_raw_import_node(d, rec).key();
  for (std::string_view key : rk::astro_dng_keys) {
    SCOPED_TRACE(std::string(key));
    ASSERT_NE(member(rec, key), nullptr) << "campo del record assente";
    EXPECT_NE(make_raw_import_node(d, with_field_changed(rec, std::string(key))).key(), base);
  }
}

// Righe di ADR-08.b §2.9 che U13 §8 riprende («Geometria e livelli: come ADR-08.b §2.9») e `process_tz` (U13 §3): valori
// del contratto, anche con nero a motivo e con la tabella di linearizzazione.
TEST(DngRecord, T_U13_12_record_declares_levels_state_and_process_tz_as_adr08) {
  const std::vector<std::pair<std::string, bytes_t>> files = {
      {"metadati completi", meta_file(exif_tags{})},
      {"motivo del nero 2 × 2 e LinearizationTable", fx::make_deflate_dng(record_spec(), fx::tiff_order::mm,
                                                                             [](fx::tiff_ifd& ifd) {
                                                                               fx::set_entry(ifd, fx::ent(50713, tt::short_, {2, 2}));
                                                                               fx::set_entry(ifd, fx::ent(50714, tt::short_, {64, 72, 80, 96}));
                                                                               fx::set_entry(ifd, fx::ent(50712, tt::short_, {0, 10, 20, 30}));
                                                                             })}};
  for (const auto& [name, bytes] : files) {
    SCOPED_TRACE(name);
    const observed o = run(bytes);
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    const json rec = raw_decoder_record(*o.img);
    EXPECT_EQ(string_of(rec, rk::black_applied), std::string(rk::expected_applied_none));
    EXPECT_EQ(string_of(rec, rk::scale_applied), std::string(rk::expected_applied_none));
    EXPECT_EQ(string_of(rec, rk::data_state), std::string(rk::expected_data_state));
    EXPECT_EQ(string_of(rec, rk::camera_processing), std::string(rk::expected_camera_processing));
    EXPECT_EQ(string_of(rec, rk::process_tz), std::string(rk::expected_process_tz));
    EXPECT_EQ(strings_of(rec, rk::metadata_not_kept),
              std::vector<std::string>(rk::expected_metadata_not_kept.begin(), rk::expected_metadata_not_kept.end()));
  }
}

// Variazioni nel file, una alla volta: decoder, Compression, Predictor, segmenti, ordine dei byte, crop, parità, nero,
// curva, ora, DNGBackwardVersion. Ogni variazione cambia il record e la chiave sullo stesso digest. zlibVersion() non si
// cambia per iniezione (zlib di sistema): vale la prova per campo sopra.
TEST(DngRecord, T_U13_12_file_variations_change_record_and_key) {
  const fx::dng32_spec base_s = record_spec();
  const bytes_t base_bytes = fx::make_deflate_dng(base_s);
  const observed base = run(base_bytes);
  ASSERT_TRUE(base.img.has_value()) << describe(base);
  const json base_rec = raw_decoder_record(*base.img);
  const content_digest d = digest_of(base_bytes);
  const std::string base_key = make_raw_import_node(d, base_rec).key();
  const auto check = [&](const std::string& name, const bytes_t& bytes, std::string_view changed_key) {
    SCOPED_TRACE(name);
    const observed o = run(bytes);
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    const json rec = raw_decoder_record(*o.img);
    const json* a = member(rec, changed_key);
    const json* b = member(base_rec, changed_key);
    ASSERT_TRUE(a != nullptr && b != nullptr) << changed_key;
    EXPECT_NE(canonical_json(*a), canonical_json(*b)) << changed_key;
    EXPECT_NE(make_raw_import_node(d, rec).key(), base_key);
  };
  {
    fx::dng32_spec s = base_s;
    s.predictor = 2;
    check("Predictor 2 al posto di 34893, stessi campioni", fx::make_deflate_dng(s), rk::predictor);
  }
  {
    fx::dng32_spec s = base_s;
    s.tiles = false;
    s.rows_per_strip = 7;
    check("strisce al posto dei tasselli", fx::make_deflate_dng(s), rk::segments);
  }
  check("ordine dei byte MM", fx::make_deflate_dng(base_s, fx::tiff_order::mm), rk::byte_order);
  {
    fx::dng32_spec s = base_s;
    set_area(s, 3, 3);
    check("ActiveArea con origine (3, 3): parità", fx::make_deflate_dng(s), rk::margin_parity);
  }
  {
    fx::dng32_spec s = base_s;
    set_area(s, 2, 4);
    check("ActiveArea con origine (2, 4): crop", fx::make_deflate_dng(s), rk::crop);
  }
  check("BlackLevel 64", fx::make_deflate_dng(base_s, fx::tiff_order::ii, [](fx::tiff_ifd& i) {
          fx::set_entry(i, fx::ent(50714, tt::short_, {64}));
        }),
        rk::black);
  {
    std::vector<std::uint64_t> table(4096);
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = 3 * i + 5;
    check("LinearizationTable 3i + 5", fx::make_deflate_dng(base_s, fx::tiff_order::ii, [&](fx::tiff_ifd& i) {
            fx::set_entry(i, fx::ent(50712, tt::short_, table));
          }),
          rk::curve_sha256);
  }
  check("DateTimeOriginal", fx::make_deflate_dng(base_s, fx::tiff_order::ii, [](fx::tiff_ifd& i) {
          fx::tiff_ifd exif;
          exif.entries = {fx::ent(36867, tt::ascii, ascii("2026:10:30 12:34:57"))};
          i.exif.push_back(std::move(exif));
        }),
        rk::capture_time_libraw);
  {
    fx::dng32_spec s = base_s;
    s.backward_version.reset();
    check("DNGBackwardVersion assente (predefinito 1.7.0.0)", fx::make_deflate_dng(s), rk::dng_backward_version);
  }
  {
    // Gemello non compresso a 16 bit, stessi valori: decoder LibRaw, Compression 1.
    fx::dng32_spec t = base_s;
    t.bits = 16;
    t.compression = 1;
    t.predictor = 1;
    t.sample_max = 65535;
    t.white = 65535;
    set_area(t, 2, 2);
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(t, fx::tiff_order::ii, [](const bytes_t& b) { return b; }));
    const bytes_t twin = fx::write_tiff(f).bytes;
    check("decoder LibRaw sul gemello non compresso", twin, rk::decoder);
    check("Compression 1 sul gemello non compresso", twin, rk::compression);
  }
}

// Il campo `decoder` entra anche nel record delle decodifiche LibRaw, con il motivo dell'instradamento e i campi
// dell'IFD raw per un DNG (§8).
TEST(DngRecord, T_U13_12_libraw_record_names_decoder_and_route) {
  fx::dng32_spec t;
  t.bits = 16;
  t.compression = 1;
  t.white = 65535;
  fx::tiff_file f;
  f.chain.push_back(fx::make_dng32_ifd(t, fx::tiff_order::ii, [](const bytes_t& b) { return b; }));
  const observed o = run(fx::write_tiff(f).bytes);
  ASSERT_TRUE(o.img.has_value()) << describe(o);
  const json rec = raw_decoder_record(*o.img);
  EXPECT_EQ(string_of(rec, rk::decoder), std::string(rk::decoder_libraw));
  EXPECT_EQ(string_of(rec, rk::route_reason), std::string("dng_libraw_compression"));
  EXPECT_EQ(integer_of(rec, rk::compression), 1);
  EXPECT_EQ(integer_of(rec, rk::sample_format), 1);
  EXPECT_EQ(integer_of(rec, rk::bits_per_sample), 16);
}

// File non DNG di PUB-RAW-01 (sola lettura da $ASTRO_RAW_PIXLS_DIR, SHA-256 verificato): Canon EOS 6D (CR2, intestazione
// TIFF senza DNGVersion) → `tiff_not_dng`; Fujifilm X-A1 (RAF, intestazione «FUJIFILM») → `not_tiff`. I campi
// dell'instradamento mancano per un file non DNG (§8): chiave assente o null.
TEST(DngRecord, T_U13_12_libraw_record_of_non_dng_files_names_the_route_reason) {
  for (const auto& [index, reason] : std::vector<std::pair<std::size_t, std::string>>{{0, "tiff_not_dng"}, {3, "not_tiff"}}) {
    std::string name, error;
    bytes_t bytes;
    ASSERT_TRUE(pub_raw::load(index, name, bytes, error)) << error;
    SCOPED_TRACE(name);
    const observed o = run(bytes);
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    ASSERT_EQ(o.routes.size(), 1u);
    EXPECT_EQ(o.routes[0].decoder, raw_decoder_kind::libraw);
    EXPECT_EQ(o.routes[0].reason, reason);
    const json rec = raw_decoder_record(*o.img);
    EXPECT_EQ(string_of(rec, rk::decoder), std::string(rk::decoder_libraw));
    EXPECT_EQ(string_of(rec, rk::route_reason), reason);
    for (std::string_view key : {rk::compression, rk::sample_format, rk::bits_per_sample})
      EXPECT_TRUE(member(rec, key) == nullptr || is_null(rec, key)) << key;
  }
}

// Andata e ritorno del record (U13 §8, ADR-08.b §2.9, F-IO-03 CL-12): lo stato del decoder del progetto si ricostruisce
// dal record, membro per membro, su strisce e tasselli, origini di ActiveArea pari e dispari, II e MM, i quattro
// predittori, e con i campi facoltativi di `dng` presenti (Orientation, DefaultCrop interi e razionali, OpcodeList,
// tag di metadato rifiutati, DNGBackwardVersion predefinito) e nero a motivo con LinearizationTable.
TEST(DngRecord, T_U13_12_record_rebuilds_the_project_decoder_state) {
  const std::uint16_t predictors[] = {1, 2, 34892, 34893};
  std::size_t i = 0;
  for (std::size_t li = 1; li < layouts().size(); ++li) {
    for (const auto& [x, y] : origins()) {
      for (const fx::tiff_order order : orders()) {
        fx::dng32_spec s = spec32(predictors[i++ % 4], layouts()[li]);
        set_area(s, x, y);
        SCOPED_TRACE(case_name(s, layouts()[li].name, order));
        const observed o = run(fx::make_deflate_dng(s, order));
        ASSERT_TRUE(o.img.has_value()) << describe(o);
        ASSERT_TRUE(o.img->decoder_state && o.img->decoder_state->dng.has_value());
        EXPECT_EQ(o.img->decoder_state->route.decoder, raw_decoder_kind::astro_dng);
        record_roundtrip::expect_state_round_trip(*o.img);
      }
    }
  }
  fx::dng32_spec no_backward = record_spec();
  no_backward.backward_version.reset();
  exif_tags bad_exposure;
  bad_exposure.exposure = fx::ent(33434, tt::short_, {60});
  const std::vector<std::pair<std::string, bytes_t>> files = {
      {"Orientation, DefaultCrop LONG, OpcodeList1-3", meta_file(exif_tags{}, [](fx::tiff_ifd& ifd) {
         fx::set_entry(ifd, fx::ent(274, tt::short_, {6}));
         fx::set_entry(ifd, fx::ent(50719, tt::long_, {2, 3}));
         fx::set_entry(ifd, fx::ent(50720, tt::long_, {20, 18}));
         for (const std::uint16_t tag : {std::uint16_t{51008}, std::uint16_t{51009}, std::uint16_t{51022}})
           fx::set_entry(ifd, fx::ent(tag, tt::undefined, {0, 0, 0, 0}));
       })},
      {"DefaultCrop RATIONAL interi e non interi", meta_file(exif_tags{}, [](fx::tiff_ifd& ifd) {
         fx::set_entry(ifd, fx::ent(50719, tt::rational, {5, 2, 8, 2}));
         fx::set_entry(ifd, fx::ent(50720, tt::rational, {42, 4, 36, 3}));
       })},
      {"ExposureTime rifiutato", meta_file(bad_exposure)},
      {"DNGBackwardVersion assente", fx::make_deflate_dng(no_backward)},
      {"motivo del nero 2 × 2 e LinearizationTable", fx::make_deflate_dng(record_spec(), fx::tiff_order::mm,
                                                                             [](fx::tiff_ifd& ifd) {
                                                                               fx::set_entry(ifd, fx::ent(50713, tt::short_, {2, 2}));
                                                                               fx::set_entry(ifd, fx::ent(50714, tt::short_, {64, 72, 80, 96}));
                                                                               fx::set_entry(ifd, fx::ent(50712, tt::short_, {0, 10, 20, 30}));
                                                                             })}};
  for (const auto& [name, bytes] : files) {
    SCOPED_TRACE(name);
    const observed o = run(bytes);
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    ASSERT_TRUE(o.img->decoder_state && o.img->decoder_state->dng.has_value());
    record_roundtrip::expect_state_round_trip(*o.img);
  }
}

// Controllo dei campi facoltativi del caso precedente: presenti nello stato decodificato, così l'andata e ritorno li
// attraversa davvero (un `dng` con i soli predefiniti non distinguerebbe un campo non ricostruito).
TEST(DngRecord, T_U13_12_round_trip_fixture_carries_the_optional_dng_fields) {
  const observed o = run(meta_file(exif_tags{}, [](fx::tiff_ifd& ifd) {
    fx::set_entry(ifd, fx::ent(274, tt::short_, {6}));
    fx::set_entry(ifd, fx::ent(50719, tt::rational, {5, 2, 8, 2}));
    fx::set_entry(ifd, fx::ent(50720, tt::long_, {20, 18}));
    fx::set_entry(ifd, fx::ent(51008, tt::undefined, {0, 0, 0, 0}));
  }));
  ASSERT_TRUE(o.img.has_value()) << describe(o);
  ASSERT_TRUE(o.img->decoder_state && o.img->decoder_state->dng.has_value());
  const dng_decoder_fields& f = *o.img->decoder_state->dng;
  EXPECT_EQ(f.orientation, std::optional<std::uint32_t>(6));
  ASSERT_TRUE(f.default_crop_origin.has_value());
  EXPECT_EQ((*f.default_crop_origin)[0], (dng_rational{5, 2}));
  EXPECT_EQ((*f.default_crop_origin)[1], (dng_rational{4, 1}));
  ASSERT_TRUE(f.default_crop_size.has_value());
  EXPECT_EQ(f.opcode_lists, std::vector<std::string>{"OpcodeList1"});
  EXPECT_TRUE(f.tiles);
  EXPECT_EQ(f.tile_width, 16u);
  record_roundtrip::expect_state_round_trip(*o.img);
}

// Record del decoder del progetto privato di una chiave alla volta (comuni, del decoder del progetto, interne di
// `segments` per strisce e tasselli): raw.record_malformed con quella chiave nel contesto.
TEST(DngRecord, T_U13_12_record_missing_one_key_stops_with_record_malformed) {
  fx::dng32_spec strips = spec32(2, layouts()[1]);
  set_area(strips, 1, 1);
  const std::vector<std::pair<std::string, bytes_t>> files = {{"tasselli", meta_file(exif_tags{})},
                                                              {"strisce", fx::make_deflate_dng(strips)}};
  for (const auto& [name, bytes] : files) {
    SCOPED_TRACE(name);
    const observed o = run(bytes);
    ASSERT_TRUE(o.img.has_value()) << describe(o);
    const json rec = raw_decoder_record(*o.img);
    for (std::string_view key : rk::state_common_keys) {
      SCOPED_TRACE(std::string(key));
      ASSERT_NE(member(rec, key), nullptr);
      record_roundtrip::expect_record_malformed(record_roundtrip::without_key(rec, key), key);
    }
    for (std::string_view key : rk::state_astro_dng_keys) {
      SCOPED_TRACE(std::string(key));
      ASSERT_NE(member(rec, key), nullptr);
      record_roundtrip::expect_record_malformed(record_roundtrip::without_key(rec, key), key);
    }
    const json* seg = member(rec, rk::segments);
    ASSERT_NE(seg, nullptr);
    for (const std::string& key : seg->keys()) {
      SCOPED_TRACE("segments." + key);
      record_roundtrip::expect_record_malformed(record_roundtrip::without_nested_key(rec, rk::segments, key), "segments." + key);
    }
    // Un valore fuori dominio per ramo: la chiave del contesto è quella del valore letto.
    struct corrupt {
      const char* name;
      json rec;
      std::string key;
    };
    const auto pair_of = [](json a, json b) {
      json v = json::array();
      v.push(std::move(a));
      v.push(std::move(b));
      return v;
    };
    using record_roundtrip::with_key;
    using record_roundtrip::with_nested_key;
    const std::vector<corrupt> rows = {
        {"decoder sconosciuto", with_key(rec, rk::decoder, json::string("dcraw")), "decoder"},
        {"decoder di LibRaw su record del progetto", with_key(rec, rk::decoder, json::string("libraw")), "route_reason"},
        {"dng_spec diversa", with_key(rec, rk::dng_spec, json::string("DNG 1.6.0.0")), "dng_spec"},
        {"versione con zero iniziale", with_key(rec, rk::dng_version, json::string("01.7.1.0")), "dng_version"},
        {"versione di tre numeri", with_key(rec, rk::dng_version, json::string("1.7.1")), "dng_version"},
        {"versione con componente > 255", with_key(rec, rk::dng_backward_version, json::string("1.256.0.0")),
         "dng_backward_version"},
        {"razionale con zero iniziale", with_key(rec, rk::default_crop_size, pair_of(json::string("10/04"), json::uinteger(4))),
         "default_crop_size"},
        {"razionale intero come testo", with_key(rec, rk::default_crop_size, pair_of(json::string("8/2"), json::uinteger(4))),
         "default_crop_size"},
        {"razionale senza '/'", with_key(rec, rk::default_crop_origin, pair_of(json::uinteger(0), json::string("5"))),
         "default_crop_origin"},
        {"razionale a denominatore nullo", with_key(rec, rk::default_crop_origin, pair_of(json::string("5/0"), json::uinteger(0))),
         "default_crop_origin"},
        {"segments.kind sconosciuto", with_nested_key(rec, rk::segments, "kind", json::string("rows")), "segments.kind"},
        {"segments.kind non testuale", with_nested_key(rec, rk::segments, "kind", json::uinteger(1)), "segments.kind"},
        {"segments.count negativo", with_nested_key(rec, rk::segments, "count", json::integer(-1)), "segments.count"},
        {"byte_order sconosciuto", with_key(rec, rk::byte_order, json::string("BE")), "byte_order"},
    };
    for (const corrupt& c : rows) {
      SCOPED_TRACE(c.name);
      record_roundtrip::expect_record_malformed(c.rec, c.key);
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-24: avviso di brevetto DNG (§10) con il testo esatto nell'intestazione del sorgente del decoder (parte M3.a).
// ---------------------------------------------------------------------------------------------------------------------

TEST(DngPatent, T_U13_24_patent_notice_in_the_decoder_source_header) {
  const std::string path = std::string(ASTRO_SOURCE_DIR) + "/src/core/dng_decode.cpp";
  std::ifstream in(path);
  ASSERT_TRUE(in.good()) << path;
  std::string header, line;
  while (std::getline(in, line) && (line.empty() || line.rfind("//", 0) == 0)) header += line + "\n";
  EXPECT_NE(header.find("This product includes DNG technology under license by Adobe."), std::string::npos)
      << "avviso assente dal commento iniziale di " << path;
}

}  // namespace
