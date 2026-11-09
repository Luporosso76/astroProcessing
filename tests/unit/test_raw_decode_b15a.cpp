// M3.b, unità B15a: modo del decoder con prenotazione del chiamante e `raw_decode_needed_bytes` (ADR-08.b §2.8,
// decisione L10). Test T-M3B-DEC-01 (modo con prenotazione del chiamante, `raw_decode_needed_bytes`).
//
// `needed` atteso è calcolato qui dalle dimensioni scritte nelle fixture, mai dal prodotto:
// - LibRaw (FIX-SYN-DNG-01, DNG non compresso): `M · 2^20 + 6 · w · h`, con `M = max(1, ⌈2 · raw_width · (raw_height + 8)
//   / 2^20⌉)` (§2.1) e `w · h` l'area visibile (§2.8: campioni f32 più maschera uint16);
// - decoder del progetto (FIX-SYN-DNG-02, deflate): `6 · w · h + S + 65 536` (U13 §7), con `S` il segmento più grande
//   (`TileWidth · TileLength · 4` oppure `ImageWidth · min(RowsPerStrip, ImageLength) · 4`).
// Byte del file esclusi in entrambi i casi (§2.8, modo con prenotazione del chiamante).
//
// Convenzioni assunte, dichiarate nel rapporto dell'unità:
// - modo con prenotazione del chiamante = `raw_decode_context::caller_reserved_bytes` valorizzato e `pool` nullo; i due
//   modi si escludono: entrambi valorizzati è invalid_input `raw.decode_context`, prima di ogni lettura del file;
// - «nessuna allocazione» si verifica coi ganci del decoder: nessun piano di `unpack`, nessun `unpack` (LibRaw); nessun
//   tetto di zlib chiesto, nessun `inflate`, nessun segmento (decoder del progetto); nessuna immagine prodotta.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "astro/error.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "expect_stop.hpp"
#include "dng_u13b_support.hpp"
#include "syn_dng.hpp"
#include "syn_dng_deflate.hpp"

namespace {

using namespace astro;
using namespace u13b;

constexpr std::uint64_t mib = std::uint64_t{1} << 20;

struct decode_case {
  std::string name;
  bytes_t bytes;
  std::uint64_t needed = 0;  // atteso, dalle dimensioni scritte
  raw_decoder_kind decoder = raw_decoder_kind::libraw;
  std::uint64_t stored_pixels = 0;  // pixel del buffer scritto, base di `max_pixels` (§2.1; U13 §3)
};

// FIX-SYN-DNG-01: buffer `w × h`, area attiva opzionale con origine pari (finestra = area attiva), CFA RGGB, nero 64.
decode_case libraw_case(std::uint32_t w, std::uint32_t h, std::optional<fx::dng_active_area> area) {
  fx::dng_spec s;
  s.width = w;
  s.height = h;
  s.cfa = {0, 1, 1, 2};
  s.black = {64};
  s.white = 4000;
  s.active_area = area;
  std::vector<std::uint16_t> v(std::size_t{w} * h);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>((i * 37u) % 4001u);
  s.pixels = fx::pixels_u16(v);
  const std::uint64_t vw = area ? area->right - area->left : w;
  const std::uint64_t vh = area ? area->bottom - area->top : h;
  const std::uint64_t m_raw = (2 * std::uint64_t{w} * (std::uint64_t{h} + 8) + mib - 1) / mib;
  const std::uint64_t m = m_raw < 1 ? 1 : m_raw;
  decode_case c;
  c.name = "LibRaw " + std::to_string(w) + "×" + std::to_string(h) + (area ? " con area attiva" : "");
  c.bytes = fx::make_dng(s);
  c.needed = m * mib + 6 * vw * vh;
  c.decoder = raw_decoder_kind::libraw;
  c.stored_pixels = std::uint64_t{w} * h;
  return c;
}

// FIX-SYN-DNG-02 deflate, decoder del progetto; area attiva con origine (3, 0) come T-U13-08.
decode_case deflate_case(std::size_t layout_index) {
  fx::dng32_spec s = spec32(34892, layouts()[layout_index]);
  set_area(s, 3, 0);
  const std::uint64_t w = s.active_area.right - s.active_area.left, h = s.active_area.bottom - s.active_area.top;
  const std::uint64_t seg = s.tiles ? std::uint64_t{s.tile_width} * s.tile_length * 4
                                    : std::uint64_t{s.width} * std::min(s.rows_per_strip, s.height) * 4;
  decode_case c;
  c.name = std::string("deflate, ") + layouts()[layout_index].name;
  c.bytes = fx::make_deflate_dng(s);
  c.needed = 6 * w * h + seg + 65536;
  c.decoder = raw_decoder_kind::astro_dng;
  c.stored_pixels = std::uint64_t{s.width} * s.height;
  return c;
}

std::vector<decode_case> all_cases() {
  return {
      libraw_case(64, 48, std::nullopt),                                // M = 1
      libraw_case(100, 38, fx::dng_active_area{2, 2, 38, 98}),         // finestra 96 × 36 in (2, 2), M = 1
      libraw_case(1024, 512, std::nullopt),                             // M = 2 (§2.1)
      deflate_case(1),                                                  // strisce RowsPerStrip 7
      deflate_case(4),                                                  // tasselli 16 × 16
  };
}

// Ganci che contano ogni passo dopo la prenotazione: piano e `unpack` di LibRaw; tetto di zlib, `inflate` e segmenti del
// decoder del progetto.
struct step_counter {
  int plan = 0, unpack = 0, after_unpack = 0, zlib_limit = 0, inflate = 0, segments = 0, routes = 0, opens = 0;
  std::optional<std::uint64_t> plan_reserved;
  raw_decode_hooks hooks() {
    raw_decode_hooks h;
    h.before_unpack_plan = [this](const raw_unpack_plan& p) {
      ++plan;
      plan_reserved = p.reserved_bytes;
    };
    h.before_unpack = [this] { ++unpack; };
    h.after_unpack = [this](raw_unpack_fields&) { ++after_unpack; };
    h.zlib_memory_limit = [this](std::uint64_t limit) {
      ++zlib_limit;
      return limit;
    };
    h.inflate_return = [this](std::size_t, int rc) {
      ++inflate;
      return rc;
    };
    h.after_segment = [this](std::size_t, std::uint64_t) { ++segments; };
    h.after_route = [this](const dng_route_result&) { ++routes; };
    h.after_open = [this](raw_probe_fields&) { ++opens; };
    return h;
  }
  int after_reservation() const { return plan + unpack + after_unpack + zlib_limit + inflate + segments; }
  // Instradamento (decoder del progetto) e `open_buffer` (LibRaw): i primi passi che leggono il file.
  int reading() const { return routes + opens; }
};

std::optional<std::string> context_of(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

// Modo pool di riferimento: il pool tiene già i byte del file (come la lettura verificata), budget abbondante.
image decode_in_pool_mode(const decode_case& c, std::uint64_t& peak_of_decoder) {
  memory_pool pool(c.bytes.size() + c.needed + 64 * mib);
  std::optional<reservation> file = pool.try_reserve(c.bytes.size(), "file");
  if (!file) ADD_FAILURE() << "prenotazione del file non riuscita";
  raw_decode_context ctx;
  ctx.pool = &pool;
  ctx.held_bytes = c.bytes.size();
  image img = decode_raw_buffer(c.bytes, {}, ctx);
  peak_of_decoder = pool.peak_reserved_bytes() - c.bytes.size();
  EXPECT_EQ(pool.reserved_bytes(), c.bytes.size()) << "prenotazione del decoder non rilasciata";
  return img;
}

void expect_same_decode(const image& a, const image& b) {
  EXPECT_EQ(a.type, b.type);
  EXPECT_EQ(a.geometry.width, b.geometry.width);
  EXPECT_EQ(a.geometry.height, b.geometry.height);
  ASSERT_EQ(a.samples.size(), b.samples.size());
  EXPECT_EQ(std::memcmp(a.samples.data(), b.samples.data(), a.samples.size()), 0) << "campioni diversi";
  EXPECT_EQ(a.invalid, b.invalid) << "maschera diversa";
  EXPECT_EQ(a.decoder_state, b.decoder_state) << "stato del decoder diverso";
  EXPECT_EQ(a.cfa, b.cfa);
  EXPECT_EQ(canonical_json(raw_decoder_record(a)), canonical_json(raw_decoder_record(b))) << "record del decoder diverso";
}

// §2.8: la funzione pura dal solo header dà la prenotazione che il modo pool chiede per lo stesso file (byte del file
// esclusi), uguale al valore calcolato dalle dimensioni scritte.
TEST(RawDecodeB15a, T_M3B_DEC_01_needed_bytes_from_header_equals_pool_mode_reservation) {
  const std::vector<decode_case> cases = all_cases();
  for (const decode_case& c : cases) {
    SCOPED_TRACE(c.name + ", modo pool, needed atteso " + std::to_string(c.needed));
    std::uint64_t pool_peak = 0;
    const image img = decode_in_pool_mode(c, pool_peak);
    ASSERT_TRUE(img.decoder_state.has_value());
    EXPECT_EQ(img.decoder_state->route.decoder, c.decoder) << "la fixture non va al decoder previsto";
    EXPECT_EQ(pool_peak, c.needed) << "prenotazione del modo pool diversa dalla formula";
  }
  for (const decode_case& c : cases) {
    SCOPED_TRACE(c.name + ", funzione pura, needed atteso " + std::to_string(c.needed));
    EXPECT_EQ(raw_decode_needed_bytes(c.bytes), c.needed);
  }
}

// §2.8: con forniti = needed (dalla funzione pura, come farà il pianificatore del master) il modo con prenotazione del
// chiamante decodifica bit a bit come il modo pool; anche con forniti > needed.
TEST(RawDecodeB15a, T_M3B_DEC_01_caller_reserved_equal_to_needed_decodes_bit_identical_to_pool_mode) {
  for (const decode_case& c : all_cases()) {
    SCOPED_TRACE(c.name);
    std::uint64_t pool_peak = 0;
    const image reference = decode_in_pool_mode(c, pool_peak);
    const std::uint64_t needed = raw_decode_needed_bytes(c.bytes);
    ASSERT_EQ(needed, c.needed);
    for (const std::uint64_t provided : {needed, needed + 1}) {
      SCOPED_TRACE("forniti " + std::to_string(provided));
      raw_decode_context ctx;
      ctx.caller_reserved_bytes = provided;
      expect_same_decode(decode_raw_buffer(c.bytes, {}, ctx), reference);
    }
  }
}

// §2.8: forniti = needed − 1 → budget sched.job_exceeds_budget con `minimo` = needed e `forniti` nel contesto, prima di
// ogni allocazione della decodifica: nessun passo dopo il punto della prenotazione, nessuna immagine, e il contatore dei
// byte di campioni e maschere (`allocated_sample_bytes`, che vede `make_image`, invisibile ai ganci) invariato. Il
// decoder è a un solo thread e nessun altro thread del test alloca immagini durante la chiamata.
// Limite: il contatore conta solo `make_image`, `aligned_buffer` e `invalid_mask`; il buffer del segmento del decoder
// del progetto (`std::vector`) e le allocazioni interne di LibRaw non li vede. Un'allocazione di quel buffer prima del
// controllo la rivelano solo i ganci del segmento, se la decodifica arriva fin lì.
TEST(RawDecodeB15a, T_M3B_DEC_01_caller_reserved_one_byte_short_stops_before_any_allocation) {
  for (const decode_case& c : all_cases()) {
    SCOPED_TRACE(c.name + ", needed atteso " + std::to_string(c.needed));
    step_counter steps;
    raw_decode_context ctx;
    ctx.caller_reserved_bytes = c.needed - 1;
    const std::uint64_t allocated_before = allocated_sample_bytes();
    try {
      (void)decode_raw_buffer(c.bytes, steps.hooks(), ctx);
      ADD_FAILURE() << "stop_error atteso: la decodifica è stata eseguita con forniti = needed − 1";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::budget) << e.what();
      EXPECT_EQ(e.code(), "sched.job_exceeds_budget") << e.what();
      EXPECT_EQ(context_of(e, "minimo"), std::optional<std::string>(std::to_string(c.needed))) << e.what();
      EXPECT_EQ(context_of(e, "forniti"), std::optional<std::string>(std::to_string(c.needed - 1))) << e.what();
    }
    EXPECT_EQ(allocated_sample_bytes(), allocated_before) << "campioni o maschere allocati prima dello stop";
    EXPECT_EQ(steps.after_reservation(), 0)
        << "piano " << steps.plan << ", unpack " << steps.unpack << ", tetto zlib " << steps.zlib_limit << ", inflate "
        << steps.inflate << ", segmenti " << steps.segments;
  }
}

// §2.8: un solo modo per chiamata, scelto a priori dal chiamante. Pool e byte forniti insieme: invalid_input
// `raw.decode_context` (decisione del coordinatore su B15a), qualunque sia il budget, prima di ogni passo della
// decodifica e senza prenotazioni nel pool.
TEST(RawDecodeB15a, T_M3B_DEC_01_pool_and_caller_reserved_together_stop_with_invalid_input) {
  for (const decode_case& c : all_cases()) {
    SCOPED_TRACE(c.name);
    for (const std::uint64_t provided : {c.needed, c.needed - 1}) {
      SCOPED_TRACE("forniti " + std::to_string(provided));
      memory_pool pool(c.bytes.size() + c.needed + 64 * mib);
      step_counter steps;
      raw_decode_context ctx;
      ctx.pool = &pool;
      ctx.caller_reserved_bytes = provided;
      EXPECT_STOP(decode_raw_buffer(c.bytes, steps.hooks(), ctx), error_category::invalid_input, "raw.decode_context");
      EXPECT_EQ(steps.after_reservation(), 0);
      EXPECT_EQ(steps.reading(), 0) << "instradamento " << steps.routes << ", open_buffer " << steps.opens;
      EXPECT_EQ(pool.peak_reserved_bytes(), 0u) << "prenotazione nel pool con i due modi insieme";
    }
  }
}

// §2.8: `held_bytes` vale solo nel modo che prenota dal pool. Senza pool, da solo o con i byte forniti: invalid_input
// `raw.decode_context` (scelta del codice di B15a), prima di ogni lettura del file.
TEST(RawDecodeB15a, T_M3B_DEC_01_held_bytes_without_pool_stops_with_invalid_input) {
  for (const decode_case& c : all_cases()) {
    for (const bool with_caller : {false, true}) {
      SCOPED_TRACE(c.name + (with_caller ? ", con caller_reserved_bytes" : ", da solo"));
      step_counter steps;
      raw_decode_context ctx;
      ctx.held_bytes = c.bytes.size();
      if (with_caller) ctx.caller_reserved_bytes = c.needed;
      EXPECT_STOP(decode_raw_buffer(c.bytes, steps.hooks(), ctx), error_category::invalid_input, "raw.decode_context");
      EXPECT_EQ(steps.after_reservation(), 0);
      EXPECT_EQ(steps.reading(), 0) << "instradamento " << steps.routes << ", open_buffer " << steps.opens;
    }
  }
}

// §2.8, §2.1: la funzione pura ha gli stop del decoder prima della prenotazione. `max_pixels` un pixel sotto il buffer
// scritto (LibRaw: raw_width · raw_height; decoder del progetto: ImageWidth · ImageLength): limit_exceeded
// `raw.max_pixels`; al valore esatto restituisce `needed`.
TEST(RawDecodeB15a, T_M3B_DEC_01_needed_bytes_stops_on_max_pixels_below_the_stored_image) {
  for (const decode_case& c : all_cases()) {
    SCOPED_TRACE(c.name + ", pixel scritti " + std::to_string(c.stored_pixels));
    EXPECT_STOP(raw_decode_needed_bytes(c.bytes, c.stored_pixels - 1), error_category::limit_exceeded, "raw.max_pixels");
    EXPECT_EQ(raw_decode_needed_bytes(c.bytes, c.stored_pixels), c.needed);
  }
}

}  // namespace
