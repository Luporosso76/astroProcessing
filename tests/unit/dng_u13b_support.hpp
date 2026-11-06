#pragma once

// Supporto comune dei test di U13b (test_dng_u13b.cpp, test_dng_u13b_hooks.cpp): esecuzione del decoder con i contatori
// dei ganci, casi di layout e di ActiveArea di T-U13-03, immagine attesa del decoder del progetto (§4) dai valori scritti
// dal generatore della fixture. Nessun valore atteso viene dal decoder sotto prova.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "astro/error.hpp"
#include "astro/image.hpp"
#include "astro/raw_decode.hpp"
#include "syn_dng.hpp"
#include "syn_dng_deflate.hpp"

namespace u13b {

using namespace astro;
namespace fx = astro_fixture;

using bytes_t = std::vector<std::uint8_t>;

inline std::optional<std::string> ctx(const stop_error& e, std::string_view key) {
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

inline observed run(const bytes_t& bytes, const raw_decode_context& c = {},
             const std::function<void(raw_decode_hooks&)>& extra = {}) {
  observed o;
  raw_decode_hooks hooks;
  hooks.libraw_return = [&](std::string_view function, int rc) {
    if (function == "open_buffer") ++o.open_buffer_calls;
    return rc;
  };
  hooks.inflate_return = [&](std::size_t, int rc) {
    ++o.inflate_calls;
    return rc;
  };
  hooks.after_route = [&](const dng_route_result& r) { o.routes.push_back(r); };
  if (extra) extra(hooks);
  try {
    o.img.emplace(decode_raw_buffer(bytes, hooks, c));
  } catch (const stop_error& e) {
    o.stop.emplace(e);
  }
  return o;
}

inline std::string describe(const observed& o) {
  if (o.stop) return o.stop->what();
  if (o.img) return "immagine " + std::to_string(o.img->geometry.width) + "x" + std::to_string(o.img->geometry.height);
  return "nessun esito";
}

inline void expect_stop(const observed& o, error_category category, std::string_view code) {
  ASSERT_TRUE(o.stop.has_value()) << "atteso lo stop " << code << ", esito: " << describe(o);
  EXPECT_EQ(o.stop->category(), category) << o.stop->what();
  EXPECT_EQ(o.stop->code(), code) << o.stop->what();
  EXPECT_FALSE(o.img.has_value());
}

// ------------------------------------------------------------------------------------------------- fixture dei casi

struct layout {
  const char* name;
  bool tiles;
  std::uint32_t tile_width;
  std::uint32_t tile_length;
  std::uint32_t rows_per_strip;  // 0: l'altezza dell'immagine
};
inline const std::vector<layout>& layouts() {
  static const std::vector<layout> all = {
      {"strisce RowsPerStrip 1", false, 0, 0, 1},
      {"strisce RowsPerStrip 7", false, 0, 0, 7},
      {"striscia unica RowsPerStrip = ImageLength", false, 0, 0, 0},
      {"striscia unica RowsPerStrip 2^32 − 1", false, 0, 0, 0xFFFFFFFFu},
      {"tasselli 16 × 16", true, 16, 16, 0},
      {"tasselli 256 × 256 con bordi riempiti", true, 256, 256, 0},
  };
  return all;
}

using cell_t = std::array<std::uint8_t, 4>;
inline const std::vector<std::pair<cell_t, cfa_pattern>>& bayer_cells() {
  static const std::vector<std::pair<cell_t, cfa_pattern>> all = {{{0, 1, 1, 2}, cfa_pattern::rggb},
                                                                  {{2, 1, 1, 0}, cfa_pattern::bggr},
                                                                  {{1, 0, 2, 1}, cfa_pattern::grbg},
                                                                  {{1, 2, 0, 1}, cfa_pattern::gbrg}};
  return all;
}
inline const char* pattern_name(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return "rggb";
    case cfa_pattern::bggr: return "bggr";
    case cfa_pattern::grbg: return "grbg";
    case cfa_pattern::gbrg: return "gbrg";
  }
  return "?";
}

// Origini (x, y) di ActiveArea del contratto (T-U13-03).
inline const std::vector<std::pair<std::uint32_t, std::uint32_t>>& origins() {
  static const std::vector<std::pair<std::uint32_t, std::uint32_t>> all = {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {3, 0}, {2, 2}};
  return all;
}

inline std::uint32_t sample_top(const fx::dng32_spec& s) {
  return s.sample_max ? *s.sample_max : s.bits >= 32 ? (1u << 24) : ((1u << s.bits) - 1u);
}

// ActiveArea con origine (x, y): tutta l'immagine per (0, 0), altrimenti fino alla penultima riga e colonna. 0 e il
// massimo sempre presenti nell'area importata (angolo in alto a sinistra e in basso a destra).
inline void set_area(fx::dng32_spec& s, std::uint32_t x, std::uint32_t y) {
  const bool full = x == 0 && y == 0;
  s.active_area = fx::dng_active_area{y, x, full ? s.height : s.height - 1, full ? s.width : s.width - 1};
  const std::uint32_t top = sample_top(s);
  s.forced = {{y, x, 0}, {y, x + 1, top}, {s.active_area.bottom - 1, s.active_area.right - 1, top}};
}

inline fx::dng32_spec spec32(std::uint16_t predictor, const layout& l, std::uint32_t width = 37, std::uint32_t height = 29) {
  fx::dng32_spec s;
  s.width = width;
  s.height = height;
  s.predictor = predictor;
  s.tiles = l.tiles;
  if (l.tiles) {
    s.tile_width = l.tile_width;
    s.tile_length = l.tile_length;
  } else {
    s.rows_per_strip = l.rows_per_strip == 0 ? height : l.rows_per_strip;
  }
  set_area(s, 0, 0);
  return s;
}

inline std::uint64_t segment_count(const fx::dng32_spec& s) {
  if (s.tiles)
    return ((std::uint64_t{s.width} + s.tile_width - 1) / s.tile_width) *
           ((std::uint64_t{s.height} + s.tile_length - 1) / s.tile_length);
  const std::uint64_t rows = std::min<std::uint64_t>(s.rows_per_strip, s.height);
  return (s.height + rows - 1) / rows;
}

inline std::string order_name(fx::tiff_order o) { return o == fx::tiff_order::ii ? "II" : "MM"; }

inline std::string case_name(const fx::dng32_spec& s, const std::string& layout_name, fx::tiff_order order) {
  std::ostringstream n;
  n << "Predictor " << s.predictor << ", " << layout_name << ", " << order_name(order) << ", " << s.width << " × "
    << s.height << ", ActiveArea origine (" << s.active_area.left << ", " << s.active_area.top << "), cella {"
    << int(s.cfa[0]) << ", " << int(s.cfa[1]) << ", " << int(s.cfa[2]) << ", " << int(s.cfa[3]) << "}";
  return n.str();
}

// Campione importato atteso nel sito grezzo (r, c): il valore scritto, oppure la voce della tabella (§4, p. 99).
inline std::uint32_t expected_value(const fx::dng32_spec& s, std::uint32_t r, std::uint32_t c,
                             const std::vector<std::uint16_t>& table = {}) {
  const std::uint32_t v = fx::dng32_sample(s, r, c);
  if (table.empty()) return v;
  return table[std::min<std::uint64_t>(v, table.size() - 1)];
}

// Immagine attesa del decoder del progetto (§4): area ActiveArea esatta, campioni f32 uguali ai valori scritti, pattern
// della cella scritta all'origine dell'area, crop e dimensioni grezze, fase e linearità dichiarate, nessuna maschera.
inline void expect_project_image(const observed& o, const fx::dng32_spec& s, const std::vector<std::uint16_t>& table = {}) {
  ASSERT_TRUE(o.img.has_value()) << "attesa un'immagine, esito: " << describe(o);
  const image& img = *o.img;
  const fx::dng_active_area& a = s.active_area;
  const std::uint32_t w = a.right - a.left, h = a.bottom - a.top;
  EXPECT_EQ(o.open_buffer_calls, 0);
  EXPECT_EQ(static_cast<std::uint64_t>(o.inflate_calls), segment_count(s));
  ASSERT_EQ(o.routes.size(), 1u);
  EXPECT_EQ(o.routes[0].decoder, raw_decoder_kind::astro_dng);
  EXPECT_EQ(img.geometry, image_geometry(w, h, 1));
  EXPECT_EQ(img.layout, plane_layout::cfa);
  EXPECT_EQ(img.type, sample_type::f32);
  EXPECT_EQ(img.unit, pixel_unit::adu);
  EXPECT_EQ(img.lin, linearity::linear);
  EXPECT_EQ(img.lin_origin.value, metadata_origin::decoder);
  ASSERT_TRUE(img.phase.has_value());
  EXPECT_EQ(img.phase->value, process_phase::raw);
  EXPECT_EQ(img.phase->source.origin, metadata_origin::decoder);
  cfa_pattern expected_pattern = cfa_pattern::rggb;
  for (const auto& [cell, p] : bayer_cells())
    if (cell == s.cfa) expected_pattern = p;
  ASSERT_TRUE(img.cfa.has_value());
  EXPECT_EQ(*img.cfa, (cfa_layout{expected_pattern, 0, 0}));
  ASSERT_TRUE(img.decoder_state.has_value());
  const decoder_state& st = *img.decoder_state;
  EXPECT_EQ(st.raw_width, s.width);
  EXPECT_EQ(st.raw_height, s.height);
  EXPECT_EQ(st.window, (decoder_window{a.left, a.top, w, h})) << "crop {x0, y0, w, h} = ActiveArea esatta, nessun arrotondamento";
  // U13 §4: indice di LibRaw, il verde sulla riga 1 della cella ha indice 3.
  cell_t expected_index = s.cfa;
  for (std::size_t site = 2; site < 4; ++site)
    if (expected_index[site] == 1) expected_index[site] = 3;
  EXPECT_EQ(st.cfa_color_index, expected_index);
  EXPECT_EQ(st.white, s.white);
  EXPECT_FALSE(st.data_maximum.has_value()) << "§4: data_maximum assente";
  EXPECT_FALSE(st.linear_max.has_value()) << "§4: linear_max assente";
  EXPECT_FALSE(img.variance.has_value());
  for (const std::uint16_t b : img.invalid.bits) ASSERT_EQ(b, 0u) << "maschera di invalidità impostata dal decoder";
  ASSERT_EQ(img.samples.size(), std::size_t{w} * h * 4);
  const float* px = img.data<float>();
  std::uint64_t diffs = 0;
  std::string first;
  for (std::uint32_t r = 0; r < h; ++r)
    for (std::uint32_t c = 0; c < w; ++c) {
      const float want = static_cast<float>(expected_value(s, a.top + r, a.left + c, table));
      if (px[std::size_t{r} * w + c] != want) {
        if (diffs++ == 0)
          first = "(" + std::to_string(r) + ", " + std::to_string(c) + "): " + std::to_string(px[std::size_t{r} * w + c]) +
                  " invece di " + std::to_string(want);
      }
    }
  EXPECT_EQ(diffs, 0u) << "prima differenza nell'area importata " << first;
}


inline const std::vector<fx::tiff_order>& orders() {
  static const std::vector<fx::tiff_order> all = {fx::tiff_order::ii, fx::tiff_order::mm};
  return all;
}

// ---------------------------------------------------------------------------------------------------------------------

}  // namespace u13b
