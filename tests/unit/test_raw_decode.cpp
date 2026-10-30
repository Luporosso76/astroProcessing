// U2 (M3.a): decoder RAW puro. T-M3A-RAW-01, -02, -03, -04 (docs/contracts/ADR-08-decoder-pack.md §2.2, §2.3, §2.4, §2.6, §6).
// Fixture FIX-SYN-DNG-01: DNG non compressi scritti da tests/fixtures/syn_dng.hpp, indipendente da astrocore e da LibRaw.
// Tutti i valori attesi derivano dalla fixture (campioni scritti, pattern e nero scritti, area attiva), mai dall'uscita del
// decoder. Tolleranze: esatte (0), tranne l'esposizione (derivazione nel test T_M3A_RAW_01_metadata_present).
//
// Convenzioni della verita' (Adobe DNG Specification 1.6.0.0, letta il 30 ottobre 2026):
//  - CFAPattern e BlackLevel con BlackLevelRepeatDim hanno l'origine nell'angolo in alto a sinistra di ActiveArea (p. 21,
//    p. 26; ActiveArea p. 42; LinearizationTable p. 84: un valore oltre la tabella vale l'ultima voce);
//  - finestra visibile per ActiveArea con origine dispari (ADR-08.b §2.3): il margine passa al pari successivo, la
//    dimensione scende di 1 su quell'asse e il bordo opposto resta fermo.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include "astro/image.hpp"
#include "astro/raw_decode.hpp"
#include "expect_stop.hpp"
#include "no_crash_report.hpp"
#include "syn_dng.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;

constexpr std::uint32_t raw_w = 64;
constexpr std::uint32_t raw_h = 48;
constexpr std::uint32_t area_w = 40;
constexpr std::uint32_t area_h = 32;

std::uint16_t written_sample(std::uint32_t r, std::uint32_t c) {
  return static_cast<std::uint16_t>(100 + (r * 37 + c * 11) % 3500);
}

std::vector<std::uint8_t> written_pixels() {
  std::vector<std::uint16_t> v(raw_w * raw_h);
  for (std::uint32_t r = 0; r < raw_h; ++r)
    for (std::uint32_t c = 0; c < raw_w; ++c) v[r * raw_w + c] = written_sample(r, c);
  return fx::pixels_u16(v);
}

struct cfa_case {
  const char* name;
  cfa_pattern expected_at_area_origin;
  std::vector<std::uint8_t> cfa;  // 0 R, 1 G, 2 B, riga per riga, ancorato all'origine dell'area attiva
};

const std::vector<cfa_case>& cfa_cases() {
  static const std::vector<cfa_case> cases = {
      {"rggb", cfa_pattern::rggb, {0, 1, 1, 2}},
      {"bggr", cfa_pattern::bggr, {2, 1, 1, 0}},
      {"grbg", cfa_pattern::grbg, {1, 0, 2, 1}},
      {"gbrg", cfa_pattern::gbrg, {1, 2, 0, 1}},
  };
  return cases;
}

fx::dng_spec base_spec(const std::vector<std::uint8_t>& cfa, std::uint32_t origin_x, std::uint32_t origin_y) {
  fx::dng_spec s;
  s.width = raw_w;
  s.height = raw_h;
  s.cfa = cfa;
  s.active_area = fx::dng_active_area{origin_y, origin_x, origin_y + area_h, origin_x + area_w};
  s.white = 4000;
  s.pixels = written_pixels();
  return s;
}

// Finestra visibile attesa dall'area attiva scritta (ADR-08.b §2.3, vedi la convenzione in testa al file).
decoder_window expected_window(const fx::dng_spec& s) {
  if (!s.active_area) return {0, 0, s.width, s.height};
  const auto& a = *s.active_area;
  const std::uint32_t x0 = (a.left + 1) & ~1u;
  const std::uint32_t y0 = (a.top + 1) & ~1u;
  return {x0, y0, a.right - x0, a.bottom - y0};
}

// Colore (0 R, 1 G, 2 B) scritto del campione grezzo (row, col).
int written_color(const fx::dng_spec& s, std::uint32_t row, std::uint32_t col) {
  const std::uint32_t top = s.active_area ? s.active_area->top : 0;
  const std::uint32_t left = s.active_area ? s.active_area->left : 0;
  return s.cfa[((row - top) % s.cfa_rows) * s.cfa_cols + (col - left) % s.cfa_cols];
}

// Nero scritto per il campione grezzo (row, col).
std::int64_t written_black(const fx::dng_spec& s, std::uint32_t row, std::uint32_t col) {
  if (s.black.empty()) return 0;
  if (!s.black_dim) return s.black[0];
  const std::uint32_t top = s.active_area ? s.active_area->top : 0;
  const std::uint32_t left = s.active_area ? s.active_area->left : 0;
  return s.black[((row - top) % s.black_dim->first) * s.black_dim->second + (col - left) % s.black_dim->second];
}

// Ipotesi H3 (ADR-08.b §2.4), nelle coordinate dell'area visibile.
std::int64_t h3_black(const decoder_state& st, std::uint32_t r, std::uint32_t c) {
  const std::uint8_t color = st.cfa_color_index[(r & 1) * 2 + (c & 1)];
  std::int64_t b = static_cast<std::int64_t>(st.black) + st.black_per_color[color];
  if (st.black_pattern_rows * st.black_pattern_cols > 0)
    b += st.black_pattern[(r % st.black_pattern_rows) * st.black_pattern_cols + (c % st.black_pattern_cols)];
  return b;
}

image decode(const fx::dng_spec& s, const raw_decode_hooks& hooks = {}) {
  const std::vector<std::uint8_t> bytes = fx::make_dng(s);
  return decode_raw_buffer(bytes, hooks);
}

int color_of_index(std::uint8_t index) { return index == 0 ? 0 : index == 2 ? 2 : 1; }

void expect_samples_are_written(const image& img, const fx::dng_spec& s, const decoder_window& w) {
  ASSERT_EQ(img.geometry.width, w.width);
  ASSERT_EQ(img.geometry.height, w.height);
  ASSERT_EQ(img.type, sample_type::f32);
  const float* p = img.data<float>();
  std::uint64_t mismatches = 0;
  std::string first;
  for (std::uint32_t r = 0; r < w.height; ++r)
    for (std::uint32_t c = 0; c < w.width; ++c) {
      const float expected = static_cast<float>(written_sample(w.y0 + r, w.x0 + c));
      if (p[static_cast<std::uint64_t>(r) * w.width + c] != expected) {
        if (mismatches++ == 0)
          first = "(" + std::to_string(r) + "," + std::to_string(c) + ") letto " +
                  std::to_string(p[static_cast<std::uint64_t>(r) * w.width + c]) + " scritto " + std::to_string(expected);
      }
    }
  EXPECT_EQ(mismatches, 0u) << "primo: " << first;
  (void)s;
}

}  // namespace

// T-M3A-RAW-01: quattro pattern x ActiveArea con origine (0,0), (1,0), (0,1), (1,1): campioni, crop dichiarato, pattern.
TEST(RawDecode, T_M3A_RAW_01_pattern_origin_samples) {
  for (const auto& pc : cfa_cases())
    for (std::uint32_t oy = 0; oy < 2; ++oy)
      for (std::uint32_t ox = 0; ox < 2; ++ox) {
        SCOPED_TRACE(std::string(pc.name) + " origine (" + std::to_string(ox) + "," + std::to_string(oy) + ")");
        const fx::dng_spec s = base_spec(pc.cfa, ox, oy);
        const decoder_window w = expected_window(s);
        const image img = decode(s);
        validate_image(img);

        expect_samples_are_written(img, s, w);
        EXPECT_EQ(img.geometry.planes, 1u);
        EXPECT_EQ(img.layout, plane_layout::cfa);

        ASSERT_TRUE(img.decoder_state.has_value());
        const decoder_state& st = *img.decoder_state;
        EXPECT_EQ(st.raw_width, raw_w);
        EXPECT_EQ(st.raw_height, raw_h);
        EXPECT_EQ(st.window, w);

        // Cella 2x2 all'origine della finestra visibile, dai colori scritti del campione grezzo corrispondente.
        const int c00 = written_color(s, w.y0, w.x0), c01 = written_color(s, w.y0, w.x0 + 1);
        const int c10 = written_color(s, w.y0 + 1, w.x0), c11 = written_color(s, w.y0 + 1, w.x0 + 1);
        cfa_pattern expected_pattern;
        if (c00 == 0) expected_pattern = cfa_pattern::rggb;
        else if (c00 == 2) expected_pattern = cfa_pattern::bggr;
        else if (c01 == 0) expected_pattern = cfa_pattern::grbg;
        else expected_pattern = cfa_pattern::gbrg;
        ASSERT_TRUE(img.cfa.has_value());
        EXPECT_EQ(img.cfa->pattern, expected_pattern);
        EXPECT_EQ(img.cfa->x_offset, 0);
        EXPECT_EQ(img.cfa->y_offset, 0);

        EXPECT_EQ(color_of_index(st.cfa_color_index[0]), c00);
        EXPECT_EQ(color_of_index(st.cfa_color_index[1]), c01);
        EXPECT_EQ(color_of_index(st.cfa_color_index[2]), c10);
        EXPECT_EQ(color_of_index(st.cfa_color_index[3]), c11);
        for (std::uint8_t index : st.cfa_color_index) EXPECT_LE(index, 3u);
        // I due siti verdi della cella portano i due indici distinti di LibRaw, 1 e 3, uno ciascuno.
        std::vector<int> green_indices;
        for (std::uint8_t index : st.cfa_color_index)
          if (index == 1 || index == 3) green_indices.push_back(index);
        std::sort(green_indices.begin(), green_indices.end());
        EXPECT_EQ(green_indices, (std::vector<int>{1, 3}));
        // H2 su tutto il campo visibile: il colore per indice di LibRaw coincide col colore scritto in ogni sito.
        for (std::uint32_t r = 0; r < w.height; ++r)
          for (std::uint32_t c = 0; c < w.width; ++c)
            ASSERT_EQ(color_of_index(st.cfa_color_index[(r & 1) * 2 + (c & 1)]), written_color(s, w.y0 + r, w.x0 + c))
                << "(" << r << "," << c << ")";
      }
}

// T-M3A-RAW-01 (stato e metadati, ADR-08.b §2.6): fase, linearita', origine decoder, nessun valore di comodo.
TEST(RawDecode, T_M3A_RAW_01_state_and_metadata) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  const image img = decode(s);
  validate_image(img);
  ASSERT_TRUE(img.phase.has_value());
  EXPECT_EQ(img.phase->value, process_phase::raw);
  EXPECT_EQ(img.phase->source.origin, metadata_origin::decoder);
  ASSERT_TRUE(img.lin.has_value());
  EXPECT_EQ(*img.lin, linearity::linear);
  ASSERT_TRUE(img.lin_origin.value.has_value());
  EXPECT_EQ(*img.lin_origin.value, metadata_origin::decoder);
  ASSERT_TRUE(img.unit.has_value());
  EXPECT_EQ(*img.unit, pixel_unit::adu);

  EXPECT_TRUE(std::all_of(img.invalid.bits.begin(), img.invalid.bits.end(), [](std::uint16_t b) { return b == 0; }));
  EXPECT_FALSE(img.variance.has_value());

  ASSERT_TRUE(img.metadata.camera.has_value());
  EXPECT_EQ(img.metadata.camera->source.origin, metadata_origin::decoder);
  EXPECT_NE(img.metadata.camera->value.find("AstroTest"), std::string::npos) << img.metadata.camera->value;
  EXPECT_NE(img.metadata.camera->value.find("SYN-DNG-01"), std::string::npos) << img.metadata.camera->value;
  ASSERT_TRUE(img.metadata.saturation_adu.has_value());
  EXPECT_EQ(img.metadata.saturation_adu->value, 4000.0);
  EXPECT_EQ(img.metadata.saturation_adu->source.origin, metadata_origin::decoder);

  // La fixture non scrive esposizione, ISO, ora, temperatura: restano assenti, nessun valore di comodo.
  EXPECT_FALSE(img.metadata.exposure_s.has_value());
  EXPECT_FALSE(img.metadata.gain.has_value());
  EXPECT_FALSE(img.metadata.capture_time_libraw.has_value());
  EXPECT_FALSE(img.metadata.temperature_c.has_value());
  EXPECT_FALSE(img.metadata.filter.has_value());
  // DNG non porta data_maximum né linear_max (LibRaw li lascia a zero): restano assenti. Valori non nulli di questi due
  // campi e la temperatura (solo da makernote) non sono producibili con la fixture: limite dichiarato.
  ASSERT_TRUE(img.decoder_state.has_value());
  EXPECT_FALSE(img.decoder_state->data_maximum.has_value());
  EXPECT_FALSE(img.decoder_state->linear_max.has_value());
}

// T-M3A-RAW-01 (metadati valorizzati, §2.6): esposizione, ISO e ora scritte nell'IFD EXIF arrivano con origine decoder.
TEST(RawDecode, T_M3A_RAW_01_metadata_present) {
  fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  s.exposure_time = std::make_pair<std::uint32_t, std::uint32_t>(1, 250);
  s.iso = 800;
  s.datetime_original = "2026:10:30 12:34:56";
  const image img = decode(s);
  validate_image(img);
  ASSERT_TRUE(img.metadata.exposure_s.has_value());
  // `other.shutter` e' un float: il valore scritto 1/250 = 0.004 e' arrotondato al float piu' vicino. 0.004 sta in
  // [2^-8, 2^-7), dove il passo dei float e' 2^-8 * 2^-23 = 2^-31; l'errore di arrotondamento e' al piu' mezzo passo e la
  // tolleranza e' un passo intero (2^-31, circa 4.7e-10), senza margine ulteriore.
  EXPECT_NEAR(img.metadata.exposure_s->value, 0.004, std::ldexp(1.0, -31));
  EXPECT_EQ(img.metadata.exposure_s->source.origin, metadata_origin::decoder);
  ASSERT_TRUE(img.metadata.gain.has_value());
  EXPECT_EQ(img.metadata.gain->value, 800.0);
  EXPECT_EQ(img.metadata.gain->source.origin, metadata_origin::decoder);
  ASSERT_TRUE(img.metadata.capture_time_libraw.has_value());
  EXPECT_EQ(img.metadata.capture_time_libraw->source.origin, metadata_origin::decoder);
  // Fuso orario non dichiarato (§b.1): l'istante scritto e' 2026-10-30 12:34:56 in un fuso ignoto, entro +-14 h da UTC.
  constexpr std::int64_t as_utc = 1793363696;  // timegm(2026-10-30 12:34:56), calcolato con calendar.timegm di Python
  EXPECT_NEAR(static_cast<double>(img.metadata.capture_time_libraw->value), static_cast<double>(as_utc), 14.0 * 3600.0);
}

// T-M3A-RAW-01 (orientamento, §2.3): l'orientamento del file si registra e non si applica.
TEST(RawDecode, T_M3A_RAW_01_flip_not_applied) {
  fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  s.active_area.reset();
  s.orientation = 6;
  const image img = decode(s);
  validate_image(img);
  expect_samples_are_written(img, s, {0, 0, raw_w, raw_h});
  ASSERT_TRUE(img.cfa.has_value());
  EXPECT_EQ(img.cfa->pattern, cfa_pattern::rggb);
}

// T-M3A-RAW-01 (crop): DefaultCrop scritto, ma il decoder importa l'area visibile intera (§2.3, il crop del formato si
// registra e non si applica).
TEST(RawDecode, T_M3A_RAW_01_default_crop_not_applied) {
  fx::dng_spec s = base_spec({2, 1, 1, 0}, 0, 0);
  s.active_area.reset();
  s.crop = std::array<std::uint32_t, 4>{4, 6, 40, 30};
  const image img = decode(s);
  expect_samples_are_written(img, s, {0, 0, raw_w, raw_h});
  ASSERT_TRUE(img.decoder_state.has_value());
  EXPECT_EQ(img.decoder_state->window, (decoder_window{0, 0, raw_w, raw_h}));
  ASSERT_TRUE(img.cfa.has_value());
  EXPECT_EQ(img.cfa->pattern, cfa_pattern::bggr);
}

namespace {

using black_pattern = std::vector<std::uint16_t>;

// Un caso di nero: campioni non sottratti (H1), bianco, nero per sito dalla formula H3 uguale a quello scritto.
void check_black_case(const cfa_case& pc, const black_pattern& pattern, std::uint32_t ox, std::uint32_t oy) {
  SCOPED_TRACE(std::string(pc.name) + " nero " + (pattern.size() == 1 ? "scalare" : "2x2") + " origine (" +
               std::to_string(ox) + "," + std::to_string(oy) + ")");
  fx::dng_spec s = base_spec(pc.cfa, ox, oy);
  if (pattern.size() > 1) s.black_dim = std::make_pair<std::uint16_t, std::uint16_t>(2, 2);
  s.black = pattern;
  s.white = 3900;
  const decoder_window w = expected_window(s);
  const image img = decode(s);
  validate_image(img);
  expect_samples_are_written(img, s, w);

  ASSERT_TRUE(img.decoder_state.has_value());
  const decoder_state& st = *img.decoder_state;
  EXPECT_EQ(st.source.origin, metadata_origin::decoder);
  EXPECT_EQ(st.white, 3900u);
  std::uint64_t mismatches = 0;
  std::string first;
  for (std::uint32_t r = 0; r < w.height; ++r)
    for (std::uint32_t c = 0; c < w.width; ++c) {
      const std::int64_t got = h3_black(st, r, c);
      const std::int64_t want = written_black(s, w.y0 + r, w.x0 + c);
      if (got != want && mismatches++ == 0)
        first = "(" + std::to_string(r) + "," + std::to_string(c) + ") formula " + std::to_string(got) + " scritto " +
                std::to_string(want);
    }
  EXPECT_EQ(mismatches, 0u) << "primo: " << first;
}

}  // namespace

// T-M3A-RAW-02: nero scalare a ogni origine (H1, H3, bianco) e motivo 2x2 non uniforme con origine (0,0).
TEST(RawDecode, T_M3A_RAW_02_black_white_not_subtracted) {
  for (const auto& pc : cfa_cases()) {
    for (std::uint32_t oy = 0; oy < 2; ++oy)
      for (std::uint32_t ox = 0; ox < 2; ++ox) check_black_case(pc, {64}, ox, oy);
    check_black_case(pc, {64, 72, 80, 96}, 0, 0);
  }
}

// T-M3A-RAW-02: H3 esatta con origine dispari quando il motivo e' invariante sull'asse spostato (§2.4, misura).
TEST(RawDecode, T_M3A_RAW_02_black_pattern_invariant_odd_origin) {
  for (const auto& pc : cfa_cases()) {
    check_black_case(pc, {64, 64, 80, 80}, 1, 0);
    check_black_case(pc, {64, 72, 64, 72}, 0, 1);
    check_black_case(pc, {64, 64, 64, 64}, 1, 1);
  }
}

// T-M3A-RAW-02: motivo non invariante sull'asse con margine maggiore di zero: stop prima dell'immagine (§2.4), anche
// con origine pari (2,2), dove la formula sarebbe esatta (limite dichiarato).
TEST(RawDecode, T_M3A_RAW_02_black_pattern_odd_origin) {
  for (const auto& pc : cfa_cases())
    for (const auto& origin : std::vector<std::pair<std::uint32_t, std::uint32_t>>{{1, 0}, {0, 1}, {1, 1}, {2, 2}}) {
      SCOPED_TRACE(std::string(pc.name) + " origine (" + std::to_string(origin.first) + "," +
                   std::to_string(origin.second) + ")");
      fx::dng_spec s = base_spec(pc.cfa, origin.first, origin.second);
      s.black_dim = std::make_pair<std::uint16_t, std::uint16_t>(2, 2);
      s.black = {64, 72, 80, 96};
      s.white = 3900;
      EXPECT_STOP(decode(s), error_category::unsupported, "raw.black_pattern_anchor");
    }
}

// T-M3A-RAW-02 (iniezione dopo unpack, §2.4, §2.6): cblack[0..3] non nulli entrano in H3; data_maximum e linear_max
// non nulli si registrano, nulli restano assenti; temperatura da SensorTemperature (-1000 = assente).
TEST(RawDecode, T_M3A_RAW_02_injected_levels_after_unpack) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  {
    raw_decode_hooks hooks;
    hooks.after_unpack = [](raw_unpack_fields& f) {
      f.cblack = {5, 6, 7, 8};
      f.data_maximum = 3900;
      f.linear_max = {3800, 3801, 3802, 3803};
    };
    const image img = decode(s, hooks);
    validate_image(img);
    expect_samples_are_written(img, s, expected_window(s));  // H1: livelli iniettati non cambiano i campioni
    ASSERT_TRUE(img.decoder_state.has_value());
    const decoder_state& st = *img.decoder_state;
    EXPECT_EQ(st.black_per_color, (std::array<std::uint32_t, 4>{5, 6, 7, 8}));
    const std::array<std::uint32_t, 4> injected = {5, 6, 7, 8};
    for (std::uint32_t r = 0; r < 2; ++r)
      for (std::uint32_t c = 0; c < 2; ++c)
        EXPECT_EQ(h3_black(st, r, c), static_cast<std::int64_t>(injected[st.cfa_color_index[r * 2 + c]]));
    ASSERT_TRUE(st.data_maximum.has_value());
    EXPECT_EQ(*st.data_maximum, 3900u);
    ASSERT_TRUE(st.linear_max.has_value());
    EXPECT_EQ(*st.linear_max, (std::array<std::int64_t, 4>{3800, 3801, 3802, 3803}));
  }
  {
    raw_decode_hooks hooks;
    hooks.after_unpack = [](raw_unpack_fields& f) {
      f.data_maximum = 0;
      f.linear_max = {0, 0, 0, 0};
    };
    const image img = decode(s, hooks);
    ASSERT_TRUE(img.decoder_state.has_value());
    EXPECT_FALSE(img.decoder_state->data_maximum.has_value());
    EXPECT_FALSE(img.decoder_state->linear_max.has_value());
  }
}

// T-M3A-RAW-01 (temperatura, §2.6): SensorTemperature iniettato dopo unpack.
TEST(RawDecode, T_M3A_RAW_01_temperature_injected) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  raw_decode_hooks hooks;
  hooks.after_unpack = [](raw_unpack_fields& f) { f.sensor_temperature = 25.0f; };
  const image warm = decode(s, hooks);
  ASSERT_TRUE(warm.metadata.temperature_c.has_value());
  EXPECT_EQ(warm.metadata.temperature_c->value, 25.0);
  EXPECT_EQ(warm.metadata.temperature_c->source.origin, metadata_origin::decoder);
  hooks.after_unpack = [](raw_unpack_fields& f) { f.sensor_temperature = -1000.0f; };
  EXPECT_FALSE(decode(s, hooks).metadata.temperature_c.has_value());
}

// T-M3A-RAW-02 (H3, nero per canale distinto per ogni sito): un nero diverso per ognuno dei quattro siti, cosi' uno
// scambio di indici tra canali cambia il valore.
TEST(RawDecode, T_M3A_RAW_02_black_distinct_per_site) {
  fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  s.black_dim = std::make_pair<std::uint16_t, std::uint16_t>(2, 2);
  s.black = {10, 20, 30, 40};
  const image img = decode(s);
  ASSERT_TRUE(img.decoder_state.has_value());
  EXPECT_EQ(h3_black(*img.decoder_state, 0, 0), 10);
  EXPECT_EQ(h3_black(*img.decoder_state, 0, 1), 20);
  EXPECT_EQ(h3_black(*img.decoder_state, 1, 0), 30);
  EXPECT_EQ(h3_black(*img.decoder_state, 1, 1), 40);
}

// T-M3A-RAW-02: parametri utente iniziali (H1, §2.2): un valore diverso da quello iniziale di LibRaw 0.21.5 (-1 per
// user_black e user_sat, -1000001 per user_cblack) e' un difetto (ASTRO_ASSERT) rilevato prima dell'apertura del buffer.
TEST(RawDecodeDeathTest, T_M3A_RAW_02_user_params_must_be_initial) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  const auto run = [&](auto mutate) {
    return [&, mutate] {
      exit_instead_of_core_dump();
      raw_decode_hooks hooks;
      hooks.before_open = mutate;
      (void)decode(s, hooks);
    };
  };
  EXPECT_DEATH(run([](raw_user_params& p) { p.user_black = 0; })(), "ASTRO_ASSERT");
  EXPECT_DEATH(run([](raw_user_params& p) { p.user_cblack[0] = 5; })(), "ASTRO_ASSERT");
  EXPECT_DEATH(run([](raw_user_params& p) { p.user_cblack[1] = 5; })(), "ASTRO_ASSERT");
  EXPECT_DEATH(run([](raw_user_params& p) { p.user_cblack[2] = 5; })(), "ASTRO_ASSERT");
  EXPECT_DEATH(run([](raw_user_params& p) { p.user_cblack[3] = 5; })(), "ASTRO_ASSERT");
  EXPECT_DEATH(run([](raw_user_params& p) { p.user_sat = 4000; })(), "ASTRO_ASSERT");
}

// T-M3A-RAW-02: i valori iniziali dei parametri utente sono -1 (user_black, user_sat) e -1000001 (user_cblack); nessuna
// iniezione: il decoder li verifica e prosegue.
TEST(RawDecode, T_M3A_RAW_02_user_params_initial_values) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  raw_decode_hooks hooks;
  bool called = false;
  hooks.before_open = [&](raw_user_params& p) {
    called = true;
    EXPECT_EQ(p.user_black, -1);
    for (int v : p.user_cblack) EXPECT_EQ(v, -1000001);  // valore iniziale di LibRaw 0.21.5 (ADR-08.b §2.2)
    EXPECT_EQ(p.user_sat, -1);
  };
  (void)decode(s, hooks);
  EXPECT_TRUE(called);
}

// T-M3A-RAW-03 da fixture: X-Trans, LinearRaw a 3 campioni, pattern non 2x2, virgola mobile, file con piu' immagini.
TEST(RawDecode, T_M3A_RAW_03_stops_from_fixtures) {
  {
    SCOPED_TRACE("X-Trans 6x6");
    fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
    s.active_area.reset();
    s.cfa_rows = 6;
    s.cfa_cols = 6;
    s.cfa = {1, 1, 0, 1, 1, 2, 2, 1, 1, 0, 1, 1, 1, 2, 1, 1, 0, 1, 1, 1, 2, 1, 1, 0, 0, 1, 1, 1, 2, 1, 1, 0, 1, 2, 1, 1};
    EXPECT_STOP(decode(s), error_category::unsupported, "raw.xtrans");
  }
  {
    SCOPED_TRACE("LinearRaw a 3 campioni");
    fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
    s.active_area.reset();
    s.samples_per_pixel = 3;
    s.photometric = 34892;
    s.pixels.assign(static_cast<std::size_t>(raw_w) * raw_h * 6, 7);
    EXPECT_STOP(decode(s), error_category::unsupported, "raw.not_cfa");
  }
  {
    SCOPED_TRACE("pattern 4x4 non periodico 2x2");
    fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
    s.active_area.reset();
    s.cfa_rows = 4;
    s.cfa_cols = 4;
    s.cfa = {0, 1, 0, 1, 1, 2, 1, 2, 0, 1, 1, 1, 1, 2, 1, 2};
    EXPECT_STOP(decode(s), error_category::unsupported, "raw.cfa_pattern");
  }
  for (std::uint16_t bits : {16, 24, 32}) {
    SCOPED_TRACE("DNG in virgola mobile a " + std::to_string(bits) + " bit");
    fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
    s.active_area.reset();
    s.bits = bits;
    s.sample_format = 3;
    s.pixels.assign(static_cast<std::size_t>(raw_w) * raw_h * (bits / 8), 0x3C);
    int unpack_calls = 0;
    raw_decode_hooks hooks;
    hooks.before_unpack = [&] { ++unpack_calls; };
    EXPECT_STOP(decode(s, hooks), error_category::unsupported, "raw.float_data");
    EXPECT_EQ(unpack_calls, 0) << "lo stop deve precedere unpack";
  }
  {
    SCOPED_TRACE("file con due immagini raw");
    fx::dng_spec a = base_spec({0, 1, 1, 2}, 0, 0);
    a.active_area.reset();
    const std::vector<std::uint8_t> bytes = fx::make_dng(std::vector<fx::dng_spec>{a, a});
    EXPECT_STOP(decode_raw_buffer(bytes), error_category::unsupported, "raw.multi_image");
  }
}

namespace {

// Costruisce `filters` di LibRaw da una cella 2x2 (colori 0 R, 1 G, 2 B, 3 G2) ripetuta sulle otto righe codificate;
// `odd_cell`, se presente, sostituisce la cella sulle righe 2-3, 6-7 (filters non periodico 2x2 in verticale).
std::uint32_t make_filters(const std::array<std::uint8_t, 4>& cell, const std::array<std::uint8_t, 4>* odd_cell = nullptr) {
  std::uint32_t f = 0;
  for (unsigned r = 0; r < 8; ++r)
    for (unsigned c = 0; c < 2; ++c) {
      const auto& use = (odd_cell && ((r >> 1) & 1)) ? *odd_cell : cell;
      const std::uint32_t shift = (((r << 1) & 14) | (c & 1)) << 1;
      f |= static_cast<std::uint32_t>(use[(r & 1) * 2 + c] & 3) << shift;
    }
  return f;
}

}  // namespace

// T-M3A-RAW-03 da iniezione dopo open_buffer: campi letti dalla libreria sostituiti prima delle decisioni di stop.
TEST(RawDecode, T_M3A_RAW_03_stops_from_injected_fields) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  const auto expect_inject = [&](const char* label, const char* code, auto mutate) {
    SCOPED_TRACE(label);
    raw_decode_hooks hooks;
    hooks.after_open = mutate;
    EXPECT_STOP(decode(s, hooks), error_category::unsupported, code);
  };
  expect_inject("filters = 9", "raw.xtrans", [](raw_probe_fields& f) { f.filters = 9; });
  expect_inject("is_foveon", "raw.cfa_pattern", [](raw_probe_fields& f) { f.is_foveon = 1; });
  expect_inject("colors = 4", "raw.cfa_pattern", [](raw_probe_fields& f) { f.colors = 4; });
  expect_inject("colors = 1", "raw.cfa_pattern", [](raw_probe_fields& f) { f.colors = 1; });
  expect_inject("raw_count = 2", "raw.multi_image", [](raw_probe_fields& f) { f.raw_count = 2; });
  expect_inject("pixel_aspect = 2", "raw.geometry", [](raw_probe_fields& f) { f.pixel_aspect = 2.0; });
  expect_inject("pixel_aspect = 0.5", "raw.geometry", [](raw_probe_fields& f) { f.pixel_aspect = 0.5; });
  // `fuji_width` e' il valore di is_fuji_rotated() dopo open_buffer.
  expect_inject("is_fuji_rotated = 100", "raw.geometry", [](raw_probe_fields& f) { f.fuji_width = 100; });
  expect_inject("is_floating_point = 1 su DNG intero", "raw.float_data",
                [](raw_probe_fields& f) { f.is_floating_point = 1; });
  expect_inject("cella senza rosso", "raw.cfa_pattern",
                [](raw_probe_fields& f) { f.filters = make_filters({1, 2, 3, 2}); });
  expect_inject("cella senza blu", "raw.cfa_pattern",
                [](raw_probe_fields& f) { f.filters = make_filters({0, 1, 3, 1}); });
  expect_inject("cella con due rossi", "raw.cfa_pattern",
                [](raw_probe_fields& f) { f.filters = make_filters({0, 1, 0, 2}); });
  expect_inject("cella con un solo verde", "raw.cfa_pattern",
                [](raw_probe_fields& f) { f.filters = make_filters({0, 1, 2, 2}); });
  expect_inject("filters non periodico 2x2", "raw.cfa_pattern", [](raw_probe_fields& f) {
    const std::array<std::uint8_t, 4> odd = {2, 1, 3, 0};
    f.filters = make_filters({0, 1, 3, 2}, &odd);
  });
}

// T-M3A-RAW-03: controllo positivo dell'iniezione. Le stesse iniezioni di campi con valori conformi non fermano la
// decodifica: gli stop sopra dipendono dal valore iniettato, non dall'esistenza del gancio.
TEST(RawDecode, T_M3A_RAW_03_conforming_injection_decodes) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  raw_decode_hooks hooks;
  hooks.after_open = [](raw_probe_fields& f) {
    f.is_foveon = 0;
    f.colors = 3;
    f.raw_count = 1;
    f.pixel_aspect = 1.0;
    f.fuji_width = 0;
    f.is_floating_point = 0;
    f.filters = make_filters({0, 1, 3, 2});
  };
  const image img = decode(s, hooks);
  validate_image(img);
  expect_samples_are_written(img, s, expected_window(s));
}

// T-M3A-RAW-03 (§2.2): il bit CONVERTFLOAT_TO_INT e' gia' tolto quando il gancio prima di open_buffer legge le opzioni.
TEST(RawDecode, T_M3A_RAW_03_convert_float_bit_removed_before_open) {
  constexpr unsigned convert_float_to_int = 0x2;  // LIBRAW_RAWOPTIONS_CONVERTFLOAT_TO_INT (ADR-08.b §2.2)
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  raw_decode_hooks hooks;
  bool called = false;
  hooks.before_open = [&](raw_user_params& p) {
    called = true;
    EXPECT_EQ(p.rawparams_options & convert_float_to_int, 0u);
  };
  (void)decode(s, hooks);
  EXPECT_TRUE(called);
}

// T-M3A-RAW-03 (§2.3): DNG a 32 bit in virgola mobile con is_floating_point() iniettato a 0: lo stop viene dal controllo
// di float_image dopo unpack (con il bit di conversione attivo float_image resterebbe nullo e l'immagine passerebbe).
TEST(RawDecode, T_M3A_RAW_03_float_image_check_after_unpack) {
  fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  s.active_area.reset();
  s.bits = 32;
  s.sample_format = 3;
  s.pixels.assign(static_cast<std::size_t>(raw_w) * raw_h * 4, 0x3C);
  int unpack_calls = 0;
  raw_decode_hooks hooks;
  hooks.after_open = [](raw_probe_fields& f) { f.is_floating_point = 0; };
  hooks.before_unpack = [&] { ++unpack_calls; };
  EXPECT_STOP(decode(s, hooks), error_category::unsupported, "raw.float_data");
  EXPECT_EQ(unpack_calls, 1) << "il controllo di float_image segue unpack";
}

// T-M3A-RAW-03 (§2.3): is_floating_point() iniettato a 1 su DNG intero ferma prima di unpack.
TEST(RawDecode, T_M3A_RAW_03_injected_floating_point_stops_before_unpack) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  int unpack_calls = 0;
  raw_decode_hooks hooks;
  hooks.after_open = [](raw_probe_fields& f) { f.is_floating_point = 1; };
  hooks.before_unpack = [&] { ++unpack_calls; };
  EXPECT_STOP(decode(s, hooks), error_category::unsupported, "raw.float_data");
  EXPECT_EQ(unpack_calls, 0);
}

// T-M3A-RAW-01 (indici per sito, §2.3): con `filters` iniettati con assegnazioni note, l'indice di LibRaw di ogni sito e'
// quello iniettato; scambiare i due verdi (1 e 3) cambia il risultato.
TEST(RawDecode, T_M3A_RAW_01_color_index_exact_per_site) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  const std::vector<std::array<std::uint8_t, 4>> cells = {{0, 1, 3, 2}, {0, 3, 1, 2}, {2, 1, 3, 0}, {2, 3, 1, 0},
                                                           {1, 0, 2, 3}, {3, 0, 2, 1}, {1, 2, 0, 3}, {3, 2, 0, 1}};
  for (const auto& cell : cells) {
    SCOPED_TRACE("cella " + std::to_string(cell[0]) + std::to_string(cell[1]) + std::to_string(cell[2]) +
                 std::to_string(cell[3]));
    raw_decode_hooks hooks;
    hooks.after_open = [&](raw_probe_fields& f) { f.filters = make_filters(cell); };
    const image img = decode(s, hooks);
    ASSERT_TRUE(img.decoder_state.has_value());
    EXPECT_EQ(img.decoder_state->cfa_color_index, cell);
  }
}


namespace {

// SHA-256 (FIPS 180-4) scritto nel test: indipendente dal prodotto e da OpenSSL.
std::array<std::uint8_t, 32> sha256(const std::vector<std::uint8_t>& data) {
  static const std::uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
      0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
      0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
      0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::vector<std::uint8_t> m = data;
  const std::uint64_t bit_len = static_cast<std::uint64_t>(data.size()) * 8;
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  for (int i = 7; i >= 0; --i) m.push_back(static_cast<std::uint8_t>(bit_len >> (8 * i)));
  const auto rotr = [](std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
  for (std::size_t off = 0; off < m.size(); off += 64) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (std::uint32_t{m[off + 4 * i]} << 24) | (std::uint32_t{m[off + 4 * i + 1]} << 16) |
             (std::uint32_t{m[off + 4 * i + 2]} << 8) | std::uint32_t{m[off + 4 * i + 3]};
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
      const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  std::array<std::uint8_t, 32> out{};
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 4; ++j) out[4 * i + j] = static_cast<std::uint8_t>(h[i] >> (24 - 8 * j));
  return out;
}

std::string hex(const std::array<std::uint8_t, 32>& d) {
  static const char* digits = "0123456789abcdef";
  std::string s;
  for (std::uint8_t b : d) {
    s += digits[b >> 4];
    s += digits[b & 15];
  }
  return s;
}

// Digest atteso (ADR-08.b §2.4): tabella scritta estesa con l'ultima voce a 65536 voci, uint16 little-endian.
std::array<std::uint8_t, 32> expected_curve_digest(const std::vector<std::uint16_t>& table) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(131072);
  for (std::size_t i = 0; i < 65536; ++i) {
    const std::uint16_t v = i < table.size() ? table[i] : table.back();
    bytes.push_back(static_cast<std::uint8_t>(v & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(v >> 8));
  }
  return sha256(bytes);
}

}  // namespace

// Controllo dell'implementazione SHA-256 del test su vettori noti (FIPS 180-4) e sui digest del contratto (§2.4).
TEST(RawDecode, T_M3A_RAW_04_sha256_reference) {
  EXPECT_EQ(hex(sha256({})), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(hex(sha256({'a', 'b', 'c'})), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  std::vector<std::uint16_t> identity4096(4096), triple4096(4096);
  for (std::uint32_t i = 0; i < 4096; ++i) {
    identity4096[i] = static_cast<std::uint16_t>(i);
    triple4096[i] = static_cast<std::uint16_t>(i * 3 + 5);
  }
  EXPECT_EQ(hex(expected_curve_digest(identity4096)), "2fe87d203699ad6efe1a4da3c45691a072f2d91b8fabea89bd0bfcb61fbf0264");
  EXPECT_EQ(hex(expected_curve_digest(triple4096)), "a3089d2ad8be7b57af4ad35ff19bd2aa94fca3ac23a0f676cff7f47a826b6662");
}

// T-M3A-RAW-04: LinearizationTable identica e non identica (H4). Atteso: tabella[min(indice, N-1)] (DNG 1.6.0.0 p. 84) e
// digest SHA-256 della tabella estesa, calcolato qui.
TEST(RawDecode, T_M3A_RAW_04_linearization_table) {
  // Indici scritti nel file, modulo `range`: per le tabelle corte alcuni superano la tabella (5000 compreso).
  const auto indices_for = [](std::uint32_t range) {
    std::vector<std::uint16_t> v(raw_w * raw_h);
    for (std::uint32_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>((i * 7) % range);
    v[5] = 5000;
    return v;
  };
  const auto make = [&](const std::vector<std::uint16_t>& table, std::uint32_t white, std::uint32_t range) {
    fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
    s.active_area.reset();
    s.pixels = fx::pixels_u16(indices_for(range));
    s.linearization = table;
    s.white = white;
    return s;
  };
  const auto check_values = [&](const image& img, const std::vector<std::uint16_t>& table, std::uint32_t range) {
    ASSERT_EQ(img.geometry.width, raw_w);
    ASSERT_EQ(img.geometry.height, raw_h);
    const std::vector<std::uint16_t> indices = indices_for(range);
    const float* p = img.data<float>();
    std::uint64_t mismatches = 0;
    for (std::uint32_t i = 0; i < raw_w * raw_h; ++i)
      if (p[i] != static_cast<float>(table[std::min<std::size_t>(indices[i], table.size() - 1)])) ++mismatches;
    EXPECT_EQ(mismatches, 0u);
  };
  const auto check_digest = [&](const image& img, const std::vector<std::uint16_t>& table) {
    ASSERT_TRUE(img.decoder_state.has_value());
    EXPECT_EQ(img.decoder_state->linearization, decoder_linearization::decoder_curve);
    ASSERT_TRUE(img.decoder_state->curve_sha256.has_value());
    EXPECT_EQ(hex(*img.decoder_state->curve_sha256), hex(expected_curve_digest(table)));
  };

  std::vector<std::uint16_t> identity(65536), identity4096(4096), triple(4096), shifted(4096);
  for (std::uint32_t i = 0; i < 65536; ++i) identity[i] = static_cast<std::uint16_t>(i);
  for (std::uint32_t i = 0; i < 4096; ++i) {
    identity4096[i] = static_cast<std::uint16_t>(i);
    triple[i] = static_cast<std::uint16_t>(i * 3 + 5);
    shifted[i] = static_cast<std::uint16_t>(i * 3 + 6);
  }

  // Tabella identica su tutto il dominio a 16 bit (65536 voci): identita'.
  const image id_img = decode(make(identity, 65535, 65536));
  validate_image(id_img);
  check_values(id_img, identity, 65536);
  ASSERT_TRUE(id_img.decoder_state.has_value());
  EXPECT_EQ(id_img.decoder_state->linearization, decoder_linearization::identity);
  EXPECT_FALSE(id_img.decoder_state->curve_sha256.has_value());

  // Tabella identica di 4096 voci: oltre la tabella vale l'ultima voce, quindi `decoder_curve`; scritto 5000, letto 4095.
  const image short_id = decode(make(identity4096, 4095, 6000));
  validate_image(short_id);
  check_values(short_id, identity4096, 6000);
  EXPECT_EQ(short_id.data<float>()[5], 4095.0f);
  check_digest(short_id, identity4096);

  const image tr_img = decode(make(triple, 12290, 4096));
  validate_image(tr_img);
  check_values(tr_img, triple, 4096);
  check_digest(tr_img, triple);
  ASSERT_TRUE(tr_img.decoder_state.has_value());
  EXPECT_EQ(tr_img.decoder_state->white, 12290u);

  // Il digest dipende solo dalla curva: stessa tabella con bianco e campioni diversi, stesso digest esatto.
  fx::dng_spec other_white = make(triple, 12290, 4096);
  other_white.white = 12000;
  const image white_img = decode(other_white);
  ASSERT_TRUE(white_img.decoder_state.has_value());
  EXPECT_EQ(white_img.decoder_state->white, 12000u);
  check_digest(white_img, triple);
  fx::dng_spec other_samples = make(triple, 12290, 4096);
  other_samples.pixels = fx::pixels_u16(std::vector<std::uint16_t>(raw_w * raw_h, 17));
  check_digest(decode(other_samples), triple);
  const image sh_img = decode(make(shifted, 12290, 4096));
  check_values(sh_img, shifted, 4096);
  check_digest(sh_img, shifted);
  EXPECT_NE(hex(expected_curve_digest(triple)), hex(expected_curve_digest(shifted)));

  // Dominio 0..65535 (§2.4): una tabella identica salvo una voce alta e' una curva non identita'.
  std::vector<std::uint16_t> high_only = identity;
  high_only[60000] = 60001;
  const image high_img = decode(make(high_only, 65535, 65536));
  check_digest(high_img, high_only);

  // Senza LinearizationTable: identita'.
  fx::dng_spec none = base_spec({0, 1, 1, 2}, 0, 0);
  none.active_area.reset();
  const image none_img = decode(none);
  ASSERT_TRUE(none_img.decoder_state.has_value());
  EXPECT_EQ(none_img.decoder_state->linearization, decoder_linearization::identity);
  EXPECT_FALSE(none_img.decoder_state->curve_sha256.has_value());
}

// T-M3A-RAW-03 (§2.3, aggiunto dall'autore del codice): `filters = 0` iniettato dopo open_buffer su un DNG Bayer. Dopo
// unpack `raw_image` non e' nullo, ma nessuna cella CFA e' codificata: stop `raw.cfa_pattern`, nessuna immagine.
TEST(RawDecode, T_M3A_RAW_03_filters_zero_with_raw_image_stops) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  int unpack_calls = 0;
  raw_decode_hooks hooks;
  hooks.after_open = [](raw_probe_fields& f) { f.filters = 0; };
  hooks.before_unpack = [&] { ++unpack_calls; };
  EXPECT_STOP(decode(s, hooks), error_category::unsupported, "raw.cfa_pattern");
  EXPECT_EQ(unpack_calls, 1) << "con filters = 0 la decisione segue unpack";
}

// T-M3A-RAW-03 (§2.3, aggiunto dall'autore del codice): cella periodica con un rosso, un blu e due verdi adiacenti (RB/GG)
// non e' una delle quattro celle Bayer: stop `raw.cfa_pattern`, anche se i conteggi dei colori sono quelli di una Bayer.
TEST(RawDecode, T_M3A_RAW_03_cell_with_adjacent_greens_stops) {
  const fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  raw_decode_hooks hooks;
  hooks.after_open = [](raw_probe_fields& f) { f.filters = make_filters({0, 2, 1, 3}); };
  EXPECT_STOP(decode(s, hooks), error_category::unsupported, "raw.cfa_pattern");
}

// T-M3A-RAW-01 (§2.6, aggiunto dall'autore del codice): `capture_time_libraw` e' `other.timestamp` copiato senza
// alterazioni. LibRaw converte `DateTimeOriginal` nell'ora locale del processo: con TZ=UTC il valore atteso e' esatto,
// 2026-10-30 12:34:56 UTC = 1793363696 (calendar.timegm di Python), e un troncamento dei secondi o dei minuti cambia il valore.
TEST(RawDecode, T_M3A_RAW_01_timestamp_copied_exactly) {
  fx::dng_spec s = base_spec({0, 1, 1, 2}, 0, 0);
  s.datetime_original = "2026:10:30 12:34:56";
  const char* old_tz = std::getenv("TZ");
  const std::optional<std::string> saved = old_tz ? std::optional<std::string>(old_tz) : std::nullopt;
  ASSERT_EQ(setenv("TZ", "UTC", 1), 0);
  tzset();
  const image img = decode(s);
  if (saved) setenv("TZ", saved->c_str(), 1);
  else unsetenv("TZ");
  tzset();
  ASSERT_TRUE(img.metadata.capture_time_libraw.has_value());
  EXPECT_EQ(img.metadata.capture_time_libraw->value, 1793363696);
}
