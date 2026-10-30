// U1 (M3.a): tipi del decoder. T-M3A-TYP-01, -02, -03 e le coppie `decoder_state` di T-M3A-TYP-12
// (docs/contracts/ADR-03-pixel-maschere.md, parte .b: §b.1, §b.2, §b.9 righe 1-2, §b.10).
// Valori attesi scritti a mano dal contratto; la verita' di riferimento per i byte FITS e' il writer di syn_fits.hpp.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "astro/fits.hpp"
#include "astro/fits_blocks.hpp"
#include "astro/image.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 40;
constexpr std::uint32_t u32max = std::numeric_limits<std::uint32_t>::max();

using lin_kind = decltype(decoder_state::linearization);

image decode(const fx::bytes& b, const fits_read_options& o = {}) { return decode_fits(b, big_budget, o); }

metadata_source decoder_source(const std::string& keyword) { return {metadata_origin::decoder, keyword}; }

// Cella 2x2 all'origine, ordine (0,0) (0,1) (1,0) (1,1): R=0, B=2, G=1 o 3. Scritta a mano.
//   rggb: R G / G B    bggr: B G / G R    grbg: G R / B G    gbrg: G B / R G
std::array<std::uint8_t, 4> cell(cfa_pattern p, std::uint8_t g_first, std::uint8_t g_second) {
  switch (p) {
    case cfa_pattern::rggb: return {0, g_first, g_second, 2};
    case cfa_pattern::bggr: return {2, g_first, g_second, 0};
    case cfa_pattern::grbg: return {g_first, 0, 2, g_second};
    case cfa_pattern::gbrg: return {g_first, 2, 0, g_second};
  }
  return {};
}

decoder_state valid_state() {
  decoder_state s;
  s.raw_width = 12;
  s.raw_height = 10;
  s.window.x0 = 2;
  s.window.y0 = 2;
  s.window.width = 8;
  s.window.height = 6;
  s.black = 512;
  s.cfa_color_index = {0, 1, 3, 2};
  s.black_per_color = {1, 2, 3, 4};
  s.black_pattern_rows = 2;
  s.black_pattern_cols = 2;
  s.black_pattern = {10, 20, 30, 40};
  s.white = 16383;
  s.data_maximum = 16000u;
  s.linear_max = std::array<std::int64_t, 4>{16383, 16383, 16383, 16383};
  s.linearization = lin_kind::identity;
  s.curve_sha256 = std::nullopt;
  s.source = decoder_source("color.black");
  return s;
}

// Immagine CFA 8x6 prodotta dal decoder: layout cfa, fase raw, stato valido.
image decoder_image(cfa_layout cfa = {cfa_pattern::rggb, 0, 0}) {
  image img = make_image({8, 6, 1}, sample_type::f32, plane_layout::cfa, cfa);
  img.phase = metadata_value<process_phase>{process_phase::raw, decoder_source("other.timestamp")};
  img.decoder_state = valid_state();
  return img;
}

image with_state(const decoder_state& s) {
  image img = decoder_image();
  img.decoder_state = s;
  return img;
}

#define EXPECT_STATE_STOP(img) EXPECT_STOP(validate_image(img), error_category::invalid_input, "image.decoder_state")

// ---------------------------------------------------------------- T-M3A-TYP-01

TEST(DecoderTypes, T_M3A_TYP_01_decoder_origin_is_accepted_on_every_frame_metadata_field) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  auto& m = img.metadata;
  m.exposure_s = metadata_value<double>{30.0, decoder_source("other.shutter")};
  m.gain = metadata_value<double>{1.0, decoder_source("other.iso_speed")};
  m.conversion_gain_e_per_adu = metadata_value<double>{0.5, decoder_source("x.cg")};
  m.offset_adu = metadata_value<double>{2.0, decoder_source("x.offset")};
  m.temperature_c = metadata_value<double>{-5.0, decoder_source("x.temp")};
  m.filter = metadata_value<std::string>{"L", decoder_source("x.filter")};
  m.camera = metadata_value<std::string>{"Marca Modello", decoder_source("idata.make")};
  m.imagetyp = metadata_value<std::string>{"Light", decoder_source("x.type")};
  m.saturation_adu = metadata_value<double>{16383.0, decoder_source("color.maximum")};
  m.pedestal_adu = metadata_value<double>{3.0, decoder_source("x.pedestal")};
  m.roi_x = metadata_value<std::int64_t>{1, decoder_source("x.roi_x")};
  m.roi_y = metadata_value<std::int64_t>{2, decoder_source("x.roi_y")};
  m.readout_mode = metadata_value<std::string>{"m", decoder_source("x.readout")};
  m.row_order = metadata_value<std::string>{"TOP-DOWN", decoder_source("x.row")};
  m.capture_time_libraw = metadata_value<std::int64_t>{1730286000, decoder_source("other.timestamp")};
  img.phase = metadata_value<process_phase>{process_phase::raw, decoder_source("x.phase")};
  img.lin = linearity::linear;
  img.lin_origin.value = metadata_origin::decoder;
  EXPECT_NO_THROW(validate_image(img));
  EXPECT_EQ(m.exposure_s->source.origin, metadata_origin::decoder);
  EXPECT_EQ(m.exposure_s->source.keyword, "other.shutter");
  EXPECT_EQ(m.capture_time_libraw->source.origin, metadata_origin::decoder);
  EXPECT_EQ(m.camera->source.keyword, "idata.make");
  EXPECT_EQ(img.phase->source.origin, metadata_origin::decoder);
  EXPECT_EQ(img.lin_origin.value, metadata_origin::decoder);
  EXPECT_NE(metadata_origin::decoder, metadata_origin::header);
  EXPECT_NE(metadata_origin::decoder, metadata_origin::declared);
}

TEST(DecoderTypes, T_M3A_TYP_01_declared_equal_to_decoder_value_has_no_effect) {
  std::optional<metadata_value<double>> d = metadata_value<double>{30.0, decoder_source("other.shutter")};
  EXPECT_NO_THROW(declare_metadata(d, 30.0, "exposure_s"));
  EXPECT_EQ(d->value, 30.0);
  EXPECT_EQ(d->source.origin, metadata_origin::decoder);
  EXPECT_EQ(d->source.keyword, "other.shutter");

  std::optional<metadata_value<std::string>> s = metadata_value<std::string>{"L", decoder_source("x.filter")};
  EXPECT_NO_THROW(declare_metadata(s, std::string("L"), "filter"));
  EXPECT_EQ(s->source.origin, metadata_origin::decoder);

  std::optional<metadata_value<std::int64_t>> t =
      metadata_value<std::int64_t>{1730286000, decoder_source("other.timestamp")};
  EXPECT_NO_THROW(declare_metadata(t, std::int64_t{1730286000}, "capture_time_libraw"));
  EXPECT_EQ(t->source.origin, metadata_origin::decoder);
}

TEST(DecoderTypes, T_M3A_TYP_01_declared_different_from_decoder_value_conflicts) {
  std::optional<metadata_value<double>> d = metadata_value<double>{30.0, decoder_source("other.shutter")};
  EXPECT_STOP(declare_metadata(d, 31.0, "exposure_s"), error_category::incompatible, "metadata.declared_conflict");
  EXPECT_EQ(d->value, 30.0);
  std::optional<metadata_value<std::string>> s = metadata_value<std::string>{"L", decoder_source("x.filter")};
  EXPECT_STOP(declare_metadata(s, std::string("R"), "filter"), error_category::incompatible,
              "metadata.declared_conflict");
  std::optional<metadata_value<std::int64_t>> t =
      metadata_value<std::int64_t>{1730286000, decoder_source("other.timestamp")};
  EXPECT_STOP(declare_metadata(t, std::int64_t{1730286001}, "capture_time_libraw"), error_category::incompatible,
              "metadata.declared_conflict");
  EXPECT_EQ(t->value, 1730286000);
}

TEST(DecoderTypes, T_M3A_TYP_01_equality_and_equivalent_content_ignore_the_origin) {
  image a = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  a.metadata.exposure_s = metadata_value<double>{30.0, {metadata_origin::header, "EXPTIME"}};
  a.metadata.capture_time_libraw = metadata_value<std::int64_t>{1730286000, {metadata_origin::header, "ASTRTSTP"}};
  a.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::header, "ASTRPHAS"}};
  a.lin = linearity::linear;
  a.lin_origin.value = metadata_origin::header;
  image b = a;
  b.metadata.exposure_s->source = {metadata_origin::decoder, "other.shutter"};
  b.metadata.capture_time_libraw->source = {metadata_origin::decoder, "other.timestamp"};
  b.phase->source = {metadata_origin::decoder, "x.phase"};
  b.lin_origin.value = metadata_origin::decoder;
  EXPECT_TRUE(a.metadata == b.metadata);
  EXPECT_TRUE(a == b);
  EXPECT_TRUE(equivalent_content(a, b));
  // controllo negativo: un valore diverso resta diverso
  b.metadata.capture_time_libraw->value = 1730286001;
  EXPECT_FALSE(equivalent_content(a, b));
}

TEST(DecoderTypes, T_M3A_TYP_01_fits_roundtrip_keeps_values_and_reports_header_origin) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.metadata.exposure_s = metadata_value<double>{30.0, decoder_source("other.shutter")};
  img.metadata.gain = metadata_value<double>{800.0, decoder_source("other.iso_speed")};
  img.metadata.saturation_adu = metadata_value<double>{16383.0, decoder_source("color.maximum")};
  img.metadata.camera = metadata_value<std::string>{"Marca Modello", decoder_source("idata.make")};
  img.metadata.capture_time_libraw = metadata_value<std::int64_t>{1730286000, decoder_source("other.timestamp")};
  img.phase = metadata_value<process_phase>{process_phase::raw, decoder_source("x.phase")};
  img.lin = linearity::linear;
  img.lin_origin.value = metadata_origin::decoder;

  const image back = decode(encode_fits(img));
  ASSERT_TRUE(back.metadata.exposure_s && back.metadata.gain && back.metadata.saturation_adu && back.metadata.camera &&
              back.metadata.capture_time_libraw && back.phase);
  EXPECT_EQ(back.metadata.exposure_s->value, 30.0);
  EXPECT_EQ(back.metadata.gain->value, 800.0);
  EXPECT_EQ(back.metadata.saturation_adu->value, 16383.0);
  EXPECT_EQ(back.metadata.camera->value, "Marca Modello");
  EXPECT_EQ(back.metadata.capture_time_libraw->value, 1730286000);
  EXPECT_EQ(back.phase->value, process_phase::raw);
  EXPECT_EQ(back.metadata.exposure_s->source.origin, metadata_origin::header);
  EXPECT_EQ(back.metadata.exposure_s->source.keyword, "EXPTIME");
  EXPECT_EQ(back.metadata.gain->source.origin, metadata_origin::header);
  EXPECT_EQ(back.metadata.saturation_adu->source.keyword, "SATURATE");
  EXPECT_EQ(back.metadata.camera->source.keyword, "INSTRUME");
  EXPECT_EQ(back.metadata.capture_time_libraw->source.origin, metadata_origin::header);
  EXPECT_EQ(back.metadata.capture_time_libraw->source.keyword, "ASTRTSTP");
  EXPECT_EQ(back.phase->source.origin, metadata_origin::header);
  EXPECT_EQ(back.phase->source.keyword, "ASTRPHAS");
  EXPECT_EQ(back.lin_origin.value, metadata_origin::header);
  EXPECT_TRUE(equivalent_content(back, img));
}

TEST(DecoderTypes, T_M3A_TYP_01_reader_never_produces_the_decoder_origin) {
  const fx::bytes file = fx::build_fits({[] {
    fx::raw_hdu h;
    h.bitpix = 16;
    h.axes = {2, 2};
    h.cards = {fx::card_real("EXPTIME", 30.0),  fx::card_real("GAIN", 100.0),    fx::card_real("EGAIN", 0.5),
               fx::card_real("OFFSET", 10.0),   fx::card_real("CCD-TEMP", -5.0), fx::card_real("SATURATE", 65535.0),
               fx::card_real("PEDESTAL", 1.0),  fx::card_str("FILTER", "L"),     fx::card_str("INSTRUME", "cam"),
               fx::card_str("IMAGETYP", "Light"), fx::card_int("XORGSUBF", 1),   fx::card_int("YORGSUBF", 2),
               fx::card_str("READOUTM", "m"),   fx::card_str("ROWORDER", "TOP-DOWN"), fx::card_int("ASTRTSTP", 1730286000),
               fx::card_str("ASTRPHAS", "raw"), fx::card_bool("LINEAR", true)};
    h.data.assign(2 * 2 * 2, 0);
    return h;
  }()});
  fits_read_options declared;
  declared.declared_phase = process_phase::raw;
  declared.declared_linearity = linearity_class::linear;
  for (const fits_read_options& o : {fits_read_options{}, declared}) {
    const image img = decode(file, o);
    const auto& m = img.metadata;
    std::size_t seen = 0;
    auto check = [&](const auto& field) {
      ASSERT_TRUE(field.has_value());
      EXPECT_NE(field->source.origin, metadata_origin::decoder);
      ++seen;
    };
    check(m.exposure_s);
    check(m.gain);
    check(m.conversion_gain_e_per_adu);
    check(m.offset_adu);
    check(m.temperature_c);
    check(m.filter);
    check(m.camera);
    check(m.imagetyp);
    check(m.saturation_adu);
    check(m.pedestal_adu);
    check(m.roi_x);
    check(m.roi_y);
    check(m.readout_mode);
    check(m.row_order);
    check(m.capture_time_libraw);
    check(img.phase);
    EXPECT_EQ(seen, 16u);
    ASSERT_TRUE(img.lin_origin.value.has_value());
    EXPECT_NE(*img.lin_origin.value, metadata_origin::decoder);
    EXPECT_FALSE(img.decoder_state.has_value());
  }
}

// Il nome dell'origine nei report dei nodi distingue `decoder` da `declared`.
TEST(DecoderTypes, T_M3A_TYP_01_origin_names_are_distinct) {
  EXPECT_STREQ(to_string(metadata_origin::header), "header");
  EXPECT_STREQ(to_string(metadata_origin::declared), "declared");
  EXPECT_STREQ(to_string(metadata_origin::decoder), "decoder");
}

// ---------------------------------------------------------------- T-M3A-TYP-02

TEST(DecoderTypes, T_M3A_TYP_02_valid_state_passes) {
  EXPECT_NO_THROW(validate_image(decoder_image()));
  image full = decoder_image();
  full.decoder_state->data_maximum.reset();
  full.decoder_state->linear_max.reset();
  EXPECT_NO_THROW(validate_image(full));
}

TEST(DecoderTypes, T_M3A_TYP_02_layout_must_be_cfa) {
  image mono = make_image({8, 6, 1}, sample_type::f32, plane_layout::mono);
  mono.phase = metadata_value<process_phase>{process_phase::raw, decoder_source("x")};
  mono.decoder_state = valid_state();
  EXPECT_STATE_STOP(mono);
  image rgb = make_image({8, 6, 3}, sample_type::f32, plane_layout::rgb);
  rgb.phase = metadata_value<process_phase>{process_phase::raw, decoder_source("x")};
  rgb.decoder_state = valid_state();
  EXPECT_STATE_STOP(rgb);
}

TEST(DecoderTypes, T_M3A_TYP_02_phase_must_be_present_and_raw) {
  image none = decoder_image();
  none.phase.reset();
  EXPECT_STATE_STOP(none);
  image calibrated = decoder_image();
  calibrated.phase->value = process_phase::calibrated;
  EXPECT_STATE_STOP(calibrated);
  image prestacked = decoder_image();
  prestacked.phase->value = process_phase::prestacked;
  EXPECT_STATE_STOP(prestacked);
  for (metadata_origin o : {metadata_origin::header, metadata_origin::declared, metadata_origin::decoder}) {
    image raw = decoder_image();
    raw.phase->source.origin = o;
    EXPECT_NO_THROW(validate_image(raw));
  }
}

TEST(DecoderTypes, T_M3A_TYP_02_source_origin_must_be_decoder) {
  for (metadata_origin o : {metadata_origin::header, metadata_origin::declared}) {
    image img = decoder_image();
    img.decoder_state->source.origin = o;
    EXPECT_STATE_STOP(img);
  }
  image ok = decoder_image();
  ok.decoder_state->source = decoder_source("color.maximum");
  EXPECT_NO_THROW(validate_image(ok));
}

TEST(DecoderTypes, T_M3A_TYP_02_window_must_match_geometry) {
  decoder_state s = valid_state();
  s.window.width = 7;
  EXPECT_STATE_STOP(with_state(s));
  s = valid_state();
  s.window.width = 9;
  EXPECT_STATE_STOP(with_state(s));
  s = valid_state();
  s.window.height = 5;
  EXPECT_STATE_STOP(with_state(s));
  s = valid_state();
  s.window.height = 7;
  EXPECT_STATE_STOP(with_state(s));
}

TEST(DecoderTypes, T_M3A_TYP_02_window_must_fit_the_raw_buffer) {
  // limite esatto: x0 + width == raw_width e y0 + height == raw_height passano
  decoder_state s = valid_state();
  s.window.x0 = 4;
  EXPECT_NO_THROW(validate_image(with_state(s)));
  s.window.x0 = 5;
  EXPECT_STATE_STOP(with_state(s));

  s = valid_state();
  s.window.y0 = 4;
  EXPECT_NO_THROW(validate_image(with_state(s)));
  s.window.y0 = 5;
  EXPECT_STATE_STOP(with_state(s));

  s = valid_state();
  s.raw_width = 9;
  EXPECT_STATE_STOP(with_state(s));  // x0 + width = 10 > 9
  s = valid_state();
  s.raw_height = 7;
  EXPECT_STATE_STOP(with_state(s));  // y0 + height = 8 > 7
}

TEST(DecoderTypes, T_M3A_TYP_02_window_sum_overflow_is_a_violation) {
  // x0 + width = 2^32 + 4 in aritmetica a 64 bit; in 32 bit darebbe 4 <= raw_width: deve fermarsi lo stesso
  decoder_state s = valid_state();
  s.raw_width = u32max;
  s.window.x0 = u32max - 3;
  EXPECT_STATE_STOP(with_state(s));
  s = valid_state();
  s.raw_height = u32max;
  s.window.y0 = u32max - 1;
  EXPECT_STATE_STOP(with_state(s));
  // controllo positivo: stessa raw_width massima, x0 che entra
  s = valid_state();
  s.raw_width = u32max;
  s.window.x0 = u32max - 8;
  EXPECT_NO_THROW(validate_image(with_state(s)));
}

TEST(DecoderTypes, T_M3A_TYP_02_black_pattern_size_matches_dimensions) {
  decoder_state s = valid_state();
  s.black_pattern = {10, 20, 30};
  EXPECT_STATE_STOP(with_state(s));
  s = valid_state();
  s.black_pattern = {10, 20, 30, 40, 50};
  EXPECT_STATE_STOP(with_state(s));
  s = valid_state();
  s.black_pattern_rows = 1;
  s.black_pattern_cols = 4;
  EXPECT_NO_THROW(validate_image(with_state(s)));
  s = valid_state();
  s.black_pattern_rows = 1;
  EXPECT_STATE_STOP(with_state(s));  // 1 * 2 = 2 elementi attesi, 4 presenti
}

TEST(DecoderTypes, T_M3A_TYP_02_black_pattern_limit_is_4098_values) {
  decoder_state s = valid_state();
  s.black_pattern_rows = 2;
  s.black_pattern_cols = 2049;
  s.black_pattern.assign(4098, 7);
  EXPECT_NO_THROW(validate_image(with_state(s)));
  s.black_pattern_rows = 1;
  s.black_pattern_cols = 4098;
  EXPECT_NO_THROW(validate_image(with_state(s)));
  s.black_pattern_rows = 1;
  s.black_pattern_cols = 4099;
  s.black_pattern.assign(4099, 7);
  EXPECT_STATE_STOP(with_state(s));
  s.black_pattern_rows = 4099;
  s.black_pattern_cols = 1;
  EXPECT_STATE_STOP(with_state(s));
}

TEST(DecoderTypes, T_M3A_TYP_02_black_pattern_one_dimension_zero_is_a_violation) {
  decoder_state s = valid_state();
  s.black_pattern_rows = 0;
  s.black_pattern_cols = 3;
  s.black_pattern.clear();
  EXPECT_STATE_STOP(with_state(s));
  s.black_pattern_rows = 3;
  s.black_pattern_cols = 0;
  EXPECT_STATE_STOP(with_state(s));
  // entrambe nulle e vettore vuoto: nessun pattern, valido
  s.black_pattern_rows = 0;
  s.black_pattern_cols = 0;
  EXPECT_NO_THROW(validate_image(with_state(s)));
}

TEST(DecoderTypes, T_M3A_TYP_02_black_pattern_product_overflow_is_a_violation) {
  // 65536 * 65536 = 2^32: in 32 bit varrebbe 0 e il vettore vuoto sembrerebbe coerente
  decoder_state s = valid_state();
  s.black_pattern_rows = 65536;
  s.black_pattern_cols = 65536;
  s.black_pattern.clear();
  EXPECT_STATE_STOP(with_state(s));
  s.black_pattern_rows = u32max;
  s.black_pattern_cols = u32max;
  EXPECT_STATE_STOP(with_state(s));
}

TEST(DecoderTypes, T_M3A_TYP_02_curve_digest_present_iff_decoder_curve) {
  decoder_state s = valid_state();
  s.linearization = lin_kind::decoder_curve;
  s.curve_sha256 = std::nullopt;
  EXPECT_STATE_STOP(with_state(s));
  s.curve_sha256 = std::array<std::uint8_t, 32>{};
  EXPECT_NO_THROW(validate_image(with_state(s)));
  s.linearization = lin_kind::identity;
  EXPECT_STATE_STOP(with_state(s));
  s.curve_sha256 = std::nullopt;
  EXPECT_NO_THROW(validate_image(with_state(s)));
}

TEST(DecoderTypes, T_M3A_TYP_02_cfa_color_index_must_agree_with_the_pattern) {
  const cfa_pattern patterns[] = {cfa_pattern::rggb, cfa_pattern::bggr, cfa_pattern::grbg, cfa_pattern::gbrg};
  for (cfa_pattern p : patterns) {
    SCOPED_TRACE(static_cast<int>(p));
    auto make = [&](std::array<std::uint8_t, 4> idx) {
      image img = decoder_image({p, 0, 0});
      img.decoder_state->cfa_color_index = idx;
      return img;
    };
    EXPECT_NO_THROW(validate_image(make(cell(p, 1, 3))));
    EXPECT_NO_THROW(validate_image(make(cell(p, 3, 1))));
    EXPECT_NO_THROW(validate_image(make(cell(p, 1, 1))));
    EXPECT_NO_THROW(validate_image(make(cell(p, 3, 3))));

    // rosso e blu scambiati
    std::array<std::uint8_t, 4> swapped = cell(p, 1, 3);
    for (auto& v : swapped) v = v == 0 ? 2 : (v == 2 ? 0 : v);
    EXPECT_STATE_STOP(make(swapped));
    // un sito verde con indice di rosso, di blu, o fuori intervallo
    for (std::size_t site = 0; site < 4; ++site) {
      const std::array<std::uint8_t, 4> good = cell(p, 1, 3);
      if (good[site] != 1 && good[site] != 3) continue;
      for (std::uint8_t bad : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{255}}) {
        std::array<std::uint8_t, 4> idx = good;
        idx[site] = bad;
        EXPECT_STATE_STOP(make(idx));
      }
    }
    // il sito rosso e il sito blu con indice di verde o fuori intervallo
    for (std::size_t site = 0; site < 4; ++site) {
      const std::array<std::uint8_t, 4> good = cell(p, 1, 3);
      if (good[site] != 0 && good[site] != 2) continue;
      for (std::uint8_t bad : {std::uint8_t{1}, std::uint8_t{3}, std::uint8_t{4}, std::uint8_t{255}}) {
        std::array<std::uint8_t, 4> idx = good;
        idx[site] = bad;
        EXPECT_STATE_STOP(make(idx));
      }
    }
  }
}

TEST(DecoderTypes, T_M3A_TYP_02_cfa_color_index_follows_the_cfa_offset) {
  // rggb con x_offset = 1: la cella all'origine e' G R / B G; con y_offset = 1: G B / R G (periodo 2)
  image x = decoder_image({cfa_pattern::rggb, 1, 0});
  x.decoder_state->cfa_color_index = {1, 0, 2, 3};
  EXPECT_NO_THROW(validate_image(x));
  x.decoder_state->cfa_color_index = {0, 1, 3, 2};
  EXPECT_STATE_STOP(x);
  image y = decoder_image({cfa_pattern::rggb, 0, 1});
  y.decoder_state->cfa_color_index = {3, 2, 0, 1};
  EXPECT_NO_THROW(validate_image(y));
  y.decoder_state->cfa_color_index = {0, 1, 3, 2};
  EXPECT_STATE_STOP(y);
  image xy = decoder_image({cfa_pattern::rggb, 1, 1});
  xy.decoder_state->cfa_color_index = {2, 1, 3, 0};
  EXPECT_NO_THROW(validate_image(xy));
}

TEST(DecoderTypes, T_M3A_TYP_02_green_index_assignments_are_accepted_and_distinct_for_every_pattern) {
  const cfa_pattern patterns[] = {cfa_pattern::rggb, cfa_pattern::bggr, cfa_pattern::grbg, cfa_pattern::gbrg};
  for (cfa_pattern p : patterns) {
    SCOPED_TRACE(static_cast<int>(p));
    image a = decoder_image({p, 0, 0});
    a.decoder_state->cfa_color_index = cell(p, 1, 3);
    image b = decoder_image({p, 0, 0});
    b.decoder_state->cfa_color_index = cell(p, 3, 1);
    image c = decoder_image({p, 0, 0});
    c.decoder_state->cfa_color_index = cell(p, 1, 1);
    for (const image* img : {&a, &b, &c}) EXPECT_NO_THROW(validate_image(*img));
    EXPECT_FALSE(equivalent_content(a, b));
    EXPECT_FALSE(equivalent_content(a, c));
    EXPECT_FALSE(equivalent_content(b, c));
    EXPECT_TRUE(equivalent_content(a, a));
  }
}

TEST(DecoderTypes, T_M3A_TYP_02_fits_writer_refuses_an_image_with_decoder_state) {
  const image with = decoder_image();
  ASSERT_NO_THROW(validate_image(with));
  EXPECT_STOP(encode_fits(with), error_category::unsupported, "fits.decoder_state");
  // controllo negativo: la stessa immagine senza stato si scrive
  image without = with;
  without.decoder_state.reset();
  EXPECT_NO_THROW(encode_fits(without));
}

// Lo scrittore FITS in streaming e' l'altro ingresso dello scrittore.
TEST(DecoderTypes, T_M3A_TYP_02_fits_stream_writer_refuses_an_image_with_decoder_state) {
  const image with = decoder_image();
  EXPECT_STOP(fits_stream_writer(with, false), error_category::unsupported, "fits.decoder_state");
  image without = with;
  without.decoder_state.reset();
  EXPECT_NO_THROW(fits_stream_writer(without, false));
}

// ---------------------------------------------------------------- T-M3A-TYP-03

fx::bytes file_with(std::vector<std::string> cards) {
  fx::raw_hdu h;
  h.bitpix = 16;
  h.axes = {2, 2};
  h.cards = std::move(cards);
  h.data.assign(2 * 2 * 2, 0);
  return fx::build_fits({h});
}

std::optional<std::string> find_card(const fx::bytes& file, const std::string& key) {
  const fx::raw_primary_view v = fx::parse_hdu(file);
  std::string padded = key;
  padded.resize(8, ' ');
  for (const std::string& c : v.cards) {
    if (c.size() >= 10 && c.compare(0, 8, padded) == 0 && c.compare(8, 2, "= ") == 0) return c;
  }
  return std::nullopt;
}

TEST(DecoderTypes, T_M3A_TYP_03_astrtstp_is_written_as_integer_and_read_back) {
  for (std::int64_t t : {std::int64_t{1730286000}, std::int64_t{0}, std::int64_t{-86400}, std::int64_t{4102444800}}) {
    SCOPED_TRACE(t);
    image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
    img.metadata.capture_time_libraw = metadata_value<std::int64_t>{t, decoder_source("other.timestamp")};
    const fx::bytes file = encode_fits(img);
    const auto card = find_card(file, "ASTRTSTP");
    ASSERT_TRUE(card.has_value());
    // oracle: la card intera e' esattamente quella che il writer indipendente produce per un intero
    EXPECT_EQ(card->substr(0, 30), fx::card_int("ASTRTSTP", t).substr(0, 30));
    EXPECT_FALSE(find_card(file, "DATE-OBS").has_value());
    const image back = decode(file);
    ASSERT_TRUE(back.metadata.capture_time_libraw.has_value());
    EXPECT_EQ(back.metadata.capture_time_libraw->value, t);
    EXPECT_EQ(back.metadata.capture_time_libraw->source.origin, metadata_origin::header);
    EXPECT_EQ(back.metadata.capture_time_libraw->source.keyword, "ASTRTSTP");
    EXPECT_FALSE(back.metadata.exposure_s.has_value());
  }
}

TEST(DecoderTypes, T_M3A_TYP_03_astrtstp_is_absent_when_the_field_is_absent) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  const fx::bytes file = encode_fits(img);
  EXPECT_FALSE(find_card(file, "ASTRTSTP").has_value());
  EXPECT_FALSE(decode(file).metadata.capture_time_libraw.has_value());
}

// Una card ASTRTSTP nell'header diversa dal campo ferma la scrittura.
TEST(DecoderTypes, T_M3A_TYP_03_astrtstp_card_in_conflict_with_the_field_stops_the_writer) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.metadata.capture_time_libraw = metadata_value<std::int64_t>{1730286000, decoder_source("other.timestamp")};
  img.header_cards = {fx::card_int("ASTRTSTP", 1730286000)};
  EXPECT_NO_THROW(encode_fits(img));
  img.header_cards = {fx::card_int("ASTRTSTP", 1730286001)};
  EXPECT_STOP(encode_fits(img), error_category::invalid_input, "fits.header_inconsistent");
}

TEST(DecoderTypes, T_M3A_TYP_03_astrtstp_is_an_interpreted_keyword) {
  const image img = decode(file_with({fx::card_int("ASTRTSTP", 1730286000)}));
  ASSERT_TRUE(img.metadata.capture_time_libraw.has_value());
  EXPECT_EQ(img.metadata.capture_time_libraw->value, 1730286000);
  for (const std::string& k : img.uninterpreted_keywords) EXPECT_NE(k, "ASTRTSTP");
}

TEST(DecoderTypes, T_M3A_TYP_03_astrtstp_string_or_real_is_a_keyword_value_error) {
  EXPECT_STOP(decode(file_with({fx::card_str("ASTRTSTP", "1730286000")})), error_category::invalid_input,
              "fits.keyword_value");
  EXPECT_STOP(decode(file_with({fx::card_text("ASTRTSTP", "=        1730286000.0")})), error_category::invalid_input,
              "fits.keyword_value");
  EXPECT_STOP(decode(file_with({fx::card_text("ASTRTSTP", "=         1.730286E+09")})), error_category::invalid_input,
              "fits.keyword_value");
  EXPECT_STOP(decode(file_with({fx::card_real("ASTRTSTP", 1730286000.5)})), error_category::invalid_input,
              "fits.keyword_value");
}

// ---------------------------------------------------------------- T-M3A-TYP-12 (coppie decoder_state)

TEST(DecoderTypes, T_M3A_TYP_12_presence_of_decoder_state_distinguishes) {
  const image with = decoder_image();
  image without = with;
  without.decoder_state.reset();
  ASSERT_NO_THROW(validate_image(with));
  ASSERT_NO_THROW(validate_image(without));
  EXPECT_FALSE(equivalent_content(with, without));
  EXPECT_FALSE(equivalent_content(without, with));
  EXPECT_TRUE(equivalent_content(with, decoder_image()));
}

TEST(DecoderTypes, T_M3A_TYP_12_each_decoder_state_field_distinguishes) {
  const image base = decoder_image();
  struct variant {
    const char* name;
    void (*edit)(decoder_state&);
  };
  const variant variants[] = {
      {"raw_width", [](decoder_state& s) { s.raw_width = 13; }},
      {"raw_height", [](decoder_state& s) { s.raw_height = 11; }},
      {"window.x0", [](decoder_state& s) { s.window.x0 = 3; }},
      {"window.y0", [](decoder_state& s) { s.window.y0 = 3; }},
      {"black", [](decoder_state& s) { s.black = 513; }},
      {"cfa_color_index", [](decoder_state& s) { s.cfa_color_index = {0, 3, 1, 2}; }},
      {"black_per_color", [](decoder_state& s) { s.black_per_color = {1, 2, 3, 5}; }},
      {"black_pattern value", [](decoder_state& s) { s.black_pattern = {10, 20, 30, 41}; }},
      {"black_pattern dims", [](decoder_state& s) { s.black_pattern_rows = 1; s.black_pattern_cols = 4; }},
      {"white", [](decoder_state& s) { s.white = 16382; }},
      {"data_maximum absent", [](decoder_state& s) { s.data_maximum.reset(); }},
      {"data_maximum value", [](decoder_state& s) { s.data_maximum = 16001u; }},
      {"linear_max absent", [](decoder_state& s) { s.linear_max.reset(); }},
      {"linear_max value", [](decoder_state& s) { s.linear_max = std::array<std::int64_t, 4>{16383, 16383, 16383, 16382}; }},
      {"linearization", [](decoder_state& s) {
         s.linearization = lin_kind::decoder_curve;
         s.curve_sha256 = std::array<std::uint8_t, 32>{};
       }},
  };
  for (const variant& v : variants) {
    SCOPED_TRACE(v.name);
    image other = base;
    v.edit(*other.decoder_state);
    ASSERT_NO_THROW(validate_image(other));
    EXPECT_FALSE(equivalent_content(base, other));
    EXPECT_FALSE(equivalent_content(other, base));
  }
}

TEST(DecoderTypes, T_M3A_TYP_12_curve_digest_value_distinguishes) {
  image a = decoder_image();
  a.decoder_state->linearization = lin_kind::decoder_curve;
  a.decoder_state->curve_sha256 = std::array<std::uint8_t, 32>{};
  image b = a;
  (*b.decoder_state->curve_sha256)[31] = 1;
  ASSERT_NO_THROW(validate_image(a));
  ASSERT_NO_THROW(validate_image(b));
  EXPECT_FALSE(equivalent_content(a, b));
  EXPECT_TRUE(equivalent_content(a, a));
}

TEST(DecoderTypes, T_M3A_TYP_12_source_keyword_alone_does_not_distinguish) {
  image a = decoder_image();
  a.decoder_state->source = decoder_source("color.black");
  image b = decoder_image();
  b.decoder_state->source = decoder_source("color.maximum");
  ASSERT_NO_THROW(validate_image(a));
  ASSERT_NO_THROW(validate_image(b));
  EXPECT_TRUE(equivalent_content(a, b));
  EXPECT_TRUE(equivalent_content(b, a));
  // controllo negativo: con un campo diverso la coppia resta diversa
  b.decoder_state->black = 1;
  EXPECT_FALSE(equivalent_content(a, b));
}

}  // namespace
