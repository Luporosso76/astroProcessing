// M3.b, unità B15: master da RAW, parte 2, CFA per canale. C-02.b §b.2 («RAW», «Canali CFA», «Analisi del set
// candidato»: lettore delle statistiche), §b.6 «Flat»; ADR-16.b §b.7; decisioni L2, L9, B7 (livelli per canale, G1 e
// G2 separati) e B12 (esito di decodifica per codice; `check_unchanged` sul ramo d'errore del piano).
// Test: T-M3B-CFA-01 (master CFA per canale da DNG sintetici tramite `combine_master`, con campioni decodificati dal
// chiamante: G1 e G2 separati, livelli `m_(f,c)` per canale con `offset_adu` della ricetta, nero non sottratto) e le prove
// aggiunte senza ID proprio (L2, prefisso `T_M3B_C02_08_` come in B12): parte RAW del lettore di statistiche (lettura
// verificata OB-11, cancel, esito di decodifica `raw.*`, saturi e livelli per canale) e residuo di B12 sul lettore FITS.
// Fuori da B15: lettura dei RAW nel job (`prepare`, spill, riverifica in `finalize`) e aggancio del lettore RAW
// all'analisi, di B16.
//
// Nessun valore atteso viene dal prodotto. I DNG sono scritti da FIX-SYN-DNG-01 (`syn_dng.hpp`, indipendente da
// astrocore e da LibRaw): i valori dei campioni, i livelli e il master atteso sono calcolati qui dalla formula del
// generatore. La decodifica del DNG (provata da T-M3A-RAW-*) si usa solo per costruire gli ingressi; le sue proprietà
// che i test assumono (campioni uguali al file, nero non sottratto, soglia di saturazione = WhiteLevel) sono verificate
// come precondizioni della fixture, non come esito del prodotto sotto prova.
//
// Vincoli delle decisioni di B15 (M3b-lacune.md, «Decisioni successive (B15)» e «Correzioni della review di B15»):
// - lettore RAW: sovraccarico `read_master_frame_stats(path, digest, format, request, pool, cancel)`; esito di decodifica
//   solo per i limiti propri del file e i difetti del contenuto (`raw.max_pixels`, `raw.not_cfa`, `raw.data_error`);
//   restano stop `input.*`, il budget (`sched.job_exceeds_budget`, con `prenotazione_decoder`, `file`, `budget`),
//   l'annullamento (prima della lettura e fra lettura e decodifica), la memoria negata dal sistema e `raw.libraw_error`;
// - `combine_master` confronta lo stato del decoder di ogni immagine con quello della ricetta prima di ogni altro
//   controllo del frame: `master.decoder_state_changed` con `frame` (nome canonico dell'ingresso) e `campo` (membro di
//   `decoder_state` diverso; `decoder_state` per uno stato assente con un frame RAW o presente con un frame FITS);
// - con frame RAW `offset_adu` è solo quello della ricetta: un valore nei metadati del frame, letto o dichiarato, è
//   invalid_input `master.param.offset_adu` con `frame` e `valore`; nel master ha origine `declared`;
// - `single` da RAW in memoria: piano con `offset_adu` della ricetta.
// Limiti delle prove: il lettore non espone i ganci del decoder (`raw_decode_hooks`) e non ha un punto osservabile fra il
// secondo controllo di cancel e la decodifica, quindi memoria negata dal sistema (`LIBRAW_UNSUFFICIENT_MEMORY`,
// `LIBRAW_MEMPOOL_OVERFLOW`, `Z_MEM_ERROR` di sistema), `raw.libraw_error` e l'annullamento durante la decodifica
// (`raw.cancelled`) non si producono in modo deterministico attraverso il lettore; `LIBRAW_TOO_BIG` del file non si
// produce perché il decoder fissa `max_raw_memory_mb` esattamente dal file (T-M3A-RAW-06). Si coprono con argomento e
// mutazione documentati nella decisione.
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/cancel.hpp"
#include "astro/error.hpp"
#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/input_format.hpp"
#include "astro/json.hpp"
#include "astro/job_probe.hpp"
#include "astro/master.hpp"
#include "astro/master_analysis.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "expect_stop.hpp"
#include "master_support.hpp"
#include "master_analysis_internal.hpp"
#include "simd_support.hpp"
#include "syn_dng.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;

// ---------------------------------------------------------------- DNG sintetici

// Buffer grezzo 64 x 48 (LibRaw rifiuta lati sotto 22), RGGB senza ActiveArea: sito (y % 2, x % 2) del motivo, canali
// nell'ordine (0,0) R, (0,1) G1, (1,0) G2, (1,1) B. BlackLevel 64 e WhiteLevel 4000.
constexpr std::uint32_t kW = 64;
constexpr std::uint32_t kH = 48;
constexpr std::uint16_t kBlack = 64;
constexpr double kWhite = 4000.0;
constexpr double kOffset = 32.0;  // `offset_adu` della ricetta e della richiesta (L9), diverso dal nero del decoder

// Valore di base di ogni sito del motivo.
struct site_values {
  std::uint16_t r, g1, g2, b;
};

// Campione del pixel (y, x): base del sito più 100 sul terzo blocco 2 x 2 di ogni tre. Indice del blocco
// j = (y / 2) * 32 + x / 2, j in [0, 768): 512 blocchi a base e 256 a base + 100 per ogni sito. Mediana del sito (768
// valori, elementi 384 e 385 in ordine) = base; media = base + 100/3: un livello calcolato come media sarebbe diverso.
std::uint16_t sample_of(site_values v, std::uint32_t y, std::uint32_t x) {
  const std::uint16_t site[4] = {v.r, v.g1, v.g2, v.b};
  const std::uint32_t j = (y / 2) * (kW / 2) + x / 2;
  return static_cast<std::uint16_t>(site[(y % 2) * 2 + x % 2] + (j % 3 == 2 ? 100 : 0));
}

std::vector<std::uint8_t> dng_of(site_values v, std::uint16_t black = kBlack) {
  fx::dng_spec s;
  s.width = kW;
  s.height = kH;
  s.white = static_cast<std::uint32_t>(kWhite);
  s.black = {black};
  s.exposure_time = std::pair<std::uint32_t, std::uint32_t>{30, 1};
  s.iso = 800;
  std::vector<std::uint16_t> px(static_cast<std::size_t>(kW) * kH);
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x) px[static_cast<std::size_t>(y) * kW + x] = sample_of(v, y, x);
  s.pixels = fx::pixels_u16(px);
  return fx::make_dng(s);
}

// Precondizioni della fixture sulla decodifica: motivo, campioni uguali al file (nero non sottratto dal decoder),
// soglia di saturazione = WhiteLevel, stato del decoder presente.
image decoded(const std::vector<std::uint8_t>& bytes, site_values v) {
  image im = decode_raw_buffer(bytes);
  EXPECT_TRUE(im.decoder_state.has_value());
  EXPECT_TRUE(im.cfa.has_value() && im.cfa->pattern == cfa_pattern::rggb && im.cfa->x_offset == 0 && im.cfa->y_offset == 0);
  EXPECT_EQ(im.geometry.width, kW);
  EXPECT_EQ(im.geometry.height, kH);
  EXPECT_TRUE(im.metadata.saturation_adu.has_value() && im.metadata.saturation_adu->value == kWhite);
  EXPECT_FALSE(im.metadata.offset_adu.has_value()) << "il decoder non dichiara offset_adu (ADR-08.b §2.6)";
  const float* p = im.data<float>();
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * kW + x;
      if (p[i] != static_cast<float>(sample_of(v, y, x))) {
        ADD_FAILURE() << "fixture: campione decodificato diverso dal file in (" << y << ", " << x << ")";
        return im;
      }
    }
  return im;
}

struct raw_frame {
  std::vector<std::uint8_t> bytes;
  image im;
};

raw_frame raw_frame_of(site_values v) {
  std::vector<std::uint8_t> bytes = dng_of(v);
  image im = decoded(bytes, v);
  return raw_frame{std::move(bytes), std::move(im)};
}

// Ricetta su frame RAW decodificati dal chiamante: stato del decoder dalla decodifica, identità di questo processo,
// filtro e binning dichiarati (il DNG non li porta), soglia di saturazione = WhiteLevel, tolleranza del nero 0 (stati
// uguali). I percorsi non esistono: `combine_master` non legge file.
master_recipe raw_recipe(master_role role, const std::vector<raw_frame>& frames) {
  master_recipe r;
  r.role = role;
  for (std::size_t k = 0; k < frames.size(); ++k) {
    master_frame_spec s;
    s.path = "non-letto/frame" + std::to_string(k) + ".dng";
    s.digest = digest_of(frames[k].bytes);
    if (role == master_role::flat) s.declared.filter = metadata_value<std::string>{"L", {metadata_origin::declared, {}}};
    s.declared_fields.binning_x = 1;
    s.declared_fields.binning_y = 1;
    s.decoder = frame_decoder_state{*frames[k].im.decoder_state, current_decoder_identity()};
    r.frames.push_back(std::move(s));
  }
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  if (role == master_role::flat) {
    r.flat_norm = flat_normalization::none;
    r.flat_level_rel_tol = 0.01;
    r.flat_min_signal_adu = 900.0;
    r.offset_adu = kOffset;
  }
  r.saturation_adu = kWhite;
  r.saturated_fraction_max = 0.5;
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::not_applicable_declared;
  r.decoder_black_abs_tol_adu = 0;
  r.simd = simd_test::requested();
  r.output_type = sample_type::f64;
  return r;
}

std::vector<image> images_of(const std::vector<raw_frame>& frames) {
  std::vector<image> out;
  for (const raw_frame& f : frames) out.push_back(f.im);
  return out;
}

std::optional<stop_error> stop_of(const std::function<void()>& f) {
  try {
    f();
  } catch (const stop_error& e) {
    return e;
  }
  return std::nullopt;
}

std::optional<std::string> context_of(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

std::string key_set(const stop_error& e) {
  std::vector<std::string> keys;
  for (const auto& [k, v] : e.context()) keys.push_back(k);
  std::sort(keys.begin(), keys.end());
  std::string out;
  for (const std::string& k : keys) out += k + ";";
  return out;
}

// Coppia del flat: livelli f0 = base − 32 = (1968, 1468, 968, 1168), f1 = (1970, 1470, 972, 1172). m̄ = (1969, 1469,
// 970, 1170); scarto relativo massimo |m − m̄| / m̄ = 2/970 < 0.01 (G2). Col nero sottratto (64) i livelli sarebbero
// 32 più bassi; con G1 e G2 in un solo canale «G» la mediana di f0 sarebbe (1100 + 1500)/2 − 32 = 1268.
constexpr site_values kF0{2000, 1500, 1000, 1200};
constexpr site_values kF1{2002, 1502, 1004, 1204};

// ---------------------------------------------------------------- T-M3B-CFA-01: master CFA per canale

TEST(MasterB15, T_M3B_CFA_01_raw_flat_master_is_the_per_pixel_mean_of_the_decoded_samples_without_black_subtraction) {
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  const master_recipe r = raw_recipe(master_role::flat, frames);
  master_result res;
  ASSERT_NO_THROW(res = combine_master(r, images_of(frames)));
  const image& m = res.output;
  ASSERT_EQ(m.geometry.width, kW);
  ASSERT_EQ(m.geometry.height, kH);
  ASSERT_TRUE(m.variance.has_value());
  // C-02.a §6 con n = 2: M = (x0 + x1)/2, V = s²/2 = (x0 − x1)²/4, esatti in f64 (interi piccoli, divisori 2 e 4).
  const double* mv = m.data<double>();
  const double* vv = m.variance_data<double>();
  std::size_t wrong = 0;
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x) {
      const std::size_t i = static_cast<std::size_t>(y) * kW + x;
      const double x0 = sample_of(kF0, y, x), x1 = sample_of(kF1, y, x);
      if (mv[i] != (x0 + x1) / 2 || vv[i] != (x0 - x1) * (x0 - x1) / 4 || m.invalid.bits[i] != 0) ++wrong;
    }
  EXPECT_EQ(wrong, 0u) << "M e V per pixel sui campioni decodificati, senza nero né offset sottratti";
  EXPECT_EQ(mv[0], 2001.0) << "sito R (0,0): (2000 + 2002)/2";
  EXPECT_EQ(mv[1], 1501.0) << "sito G1 (0,1)";
  EXPECT_EQ(mv[kW], 1002.0) << "sito G2 (1,0)";
  EXPECT_EQ(vv[kW], 4.0) << "G2: (1000 − 1004)²/4";
}

TEST(MasterB15, T_M3B_CFA_01_raw_flat_master_keeps_the_cfa_layout_and_carries_no_decoder_state) {
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  master_result res;
  ASSERT_NO_THROW(res = combine_master(raw_recipe(master_role::flat, frames), images_of(frames)));
  ASSERT_TRUE(res.output.cfa.has_value());
  EXPECT_EQ(res.output.cfa->pattern, cfa_pattern::rggb);
  EXPECT_EQ(res.output.layout, plane_layout::cfa);
  EXPECT_FALSE(res.output.decoder_state.has_value()) << "ADR-16.b §b.7: l'immagine del master non porta decoder_state";
}

TEST(MasterB15, T_M3B_CFA_01_raw_flat_minimum_signal_equal_to_the_g2_level_with_the_recipe_offset_is_admitted) {
  // m_(f0,G2) = 1000 − offset_adu 32 = 968: uguale al minimo, ammesso (C-02.a §2). Col nero del decoder sottratto
  // (1000 − 64 − 32 = 904, oppure 1000 − 64 = 936) il frame si fermerebbe.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  master_recipe r = raw_recipe(master_role::flat, frames);
  r.flat_min_signal_adu = 968.0;
  EXPECT_NO_THROW((void)combine_master(r, images_of(frames)));
}

TEST(MasterB15, T_M3B_CFA_01_raw_flat_minimum_signal_just_above_the_g2_level_stops_with_flat_low_signal_on_g2) {
  // 968.5: solo G2 di f0 (968) sta sotto; G2 di f1 vale 972, G1 1468. Un canale «G» unico (mediana 1268) non si
  // fermerebbe; un'etichetta diversa da G2 indica un sito sbagliato.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  master_recipe r = raw_recipe(master_role::flat, frames);
  r.flat_min_signal_adu = 968.5;
  const std::optional<stop_error> e = stop_of([&] { (void)combine_master(r, images_of(frames)); });
  ASSERT_TRUE(e.has_value()) << "livello G2 sotto il minimo: stop atteso";
  EXPECT_EQ(e->category(), error_category::invalid_input) << e->what();
  EXPECT_EQ(e->code(), "master.flat_low_signal") << e->what();
  EXPECT_EQ(context_of(*e, "canale"), std::optional<std::string>("G2")) << e->what();
  EXPECT_EQ(context_of(*e, "m_f"), std::optional<std::string>("968")) << e->what();
}

TEST(MasterB15, T_M3B_CFA_01_raw_flat_level_tolerance_violated_only_in_g1_stops_with_variance_domain_on_g1) {
  // f1 con G1 = 1532: livelli G1 1468 e 1500, m̄ = 1484, |1468/1484 − 1| = 16/1484 > 0.01. G2 resta (968, 972):
  // con G1 e G2 in un solo canale lo scarto sarebbe diverso, e l'etichetta del canale non sarebbe G1.
  const site_values f1{2002, 1532, 1004, 1204};
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(f1)};
  const std::optional<stop_error> e = stop_of([&] { (void)combine_master(raw_recipe(master_role::flat, frames), images_of(frames)); });
  ASSERT_TRUE(e.has_value()) << "tolleranza dei livelli superata in G1: stop atteso";
  EXPECT_EQ(e->category(), error_category::unsupported) << e->what();
  EXPECT_EQ(e->code(), "master.variance_domain") << e->what();
  EXPECT_EQ(context_of(*e, "canale"), std::optional<std::string>("G1")) << e->what();
}

TEST(MasterB15, T_M3B_CFA_01_raw_dark_master_without_offset_adu_is_the_per_pixel_mean_of_the_decoded_samples) {
  // Dark da RAW: `offset_adu` vietato nella ricetta (L9) e assente nei metadati del decoder; il master non lo usa.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  const master_recipe r = raw_recipe(master_role::dark, frames);
  ASSERT_FALSE(r.offset_adu.has_value());
  master_result res;
  ASSERT_NO_THROW(res = combine_master(r, images_of(frames)));
  const double* mv = res.output.data<double>();
  std::size_t wrong = 0;
  for (std::uint32_t y = 0; y < kH; ++y)
    for (std::uint32_t x = 0; x < kW; ++x) {
      const double x0 = sample_of(kF0, y, x), x1 = sample_of(kF1, y, x);
      if (mv[static_cast<std::size_t>(y) * kW + x] != (x0 + x1) / 2) ++wrong;
    }
  EXPECT_EQ(wrong, 0u);
}

TEST(MasterB15, T_M3B_CFA_01_raw_single_dark_variance_uses_the_recipe_offset_not_the_decoder_black) {
  // `single` (C-02.b §b.6): V = read_noise² + max(x − offset_adu, 0) / gain. Pixel (0,0) = 2000, offset 32, gain 0.5,
  // read noise 2: V = 4 + 1968 / 0.5 = 3940. Col nero 64 al posto dell'offset sarebbe 4 + 1936 / 0.5 = 3876.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0)};
  master_recipe r = raw_recipe(master_role::dark, frames);
  r.combine = combine_method::single;
  r.rejection = master_rejection::none;
  r.offset_adu = kOffset;
  r.conversion_gain_e_per_adu = 0.5;
  r.read_noise_adu = 2.0;
  master_result res;
  ASSERT_NO_THROW(res = combine_master(r, images_of(frames)));
  ASSERT_TRUE(res.output.variance.has_value());
  EXPECT_EQ(res.output.data<double>()[0], 2000.0);
  EXPECT_EQ(res.output.variance_data<double>()[0], 3940.0);
  EXPECT_EQ(res.output.variance_data<double>()[kW], 4.0 + (1000.0 - kOffset) / 0.5) << "sito G2 (1,0)";
}

TEST(MasterB15, T_M3B_CFA_01_decoded_frame_with_a_decoder_state_different_from_the_recipe_stops_with_decoder_state_changed) {
  // ADR-16.b §b.7: uno stato del decoder dei campioni diverso da quello del nodo è integrity master.decoder_state_changed.
  // Gli stati della ricetta sono uguali fra loro (nessun mismatch del preflight); cambia solo lo stato dell'immagine.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  const master_recipe r = raw_recipe(master_role::flat, frames);
  std::vector<image> ims = images_of(frames);
  ims[1].decoder_state->black += 1;
  const std::optional<stop_error> e = stop_of([&] { (void)combine_master(r, std::move(ims)); });
  ASSERT_TRUE(e.has_value()) << "stato del decoder cambiato fra ricetta e campioni: stop atteso";
  EXPECT_EQ(e->category(), error_category::integrity) << e->what();
  EXPECT_EQ(e->code(), "master.decoder_state_changed") << e->what();
  // Contesto (decisione B15), chiavi come gli stop del preflight del master: `frame` = nome dell'ingresso in ordine
  // canonico (frame_000 = digest minimo), `campo` = membro di `decoder_state` diverso, nome come in `master.mismatch.<campo>`.
  const std::string changed = r.frames[1].digest.sha256 < r.frames[0].digest.sha256 ? "frame_000" : "frame_001";
  EXPECT_EQ(context_of(*e, "frame"), std::optional<std::string>(changed)) << e->what();
  EXPECT_EQ(context_of(*e, "campo"), std::optional<std::string>("black")) << e->what();
}

TEST(MasterB15, T_M3B_CFA_01_raw_image_without_decoder_state_stops_with_decoder_state_changed_on_the_decoder_state_field) {
  // Decisione B15: immagine senza `decoder_state` per un frame RAW della ricetta → `master.decoder_state_changed` con
  // `campo` = `decoder_state` e `frame` = nome canonico dell'ingresso.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  const master_recipe r = raw_recipe(master_role::flat, frames);
  std::vector<image> ims = images_of(frames);
  ims[1].decoder_state.reset();
  const std::optional<stop_error> e = stop_of([&] { (void)combine_master(r, std::move(ims)); });
  ASSERT_TRUE(e.has_value()) << "immagine senza stato per un frame RAW: stop atteso";
  EXPECT_EQ(e->category(), error_category::integrity) << e->what();
  EXPECT_EQ(e->code(), "master.decoder_state_changed") << e->what();
  const std::string changed = r.frames[1].digest.sha256 < r.frames[0].digest.sha256 ? "frame_000" : "frame_001";
  EXPECT_EQ(context_of(*e, "frame"), std::optional<std::string>(changed)) << e->what();
  EXPECT_EQ(context_of(*e, "campo"), std::optional<std::string>("decoder_state")) << e->what();
}

// Stato del decoder valido (ADR-03.b §b.2) con margine per ogni membro: buffer 66 × 50 e finestra 64 × 48 in (1, 1),
// motivo del nero 1 × 2, curva di linearizzazione, `data_maximum` e `linear_max` presenti. Instradamento LibRaw, oppure
// decoder del progetto (campi DNG presenti, campi propri di LibRaw a zero) per il membro `dng`.
decoder_state roomy_state(bool astro_dng) {
  decoder_state s;
  s.raw_width = 66;
  s.raw_height = 50;
  s.window = decoder_window{1, 1, kW, kH};
  s.black = kBlack;
  s.cfa_color_index = {0, 1, 3, 2};
  s.black_per_color = {0, 0, 0, 0};
  s.black_pattern_rows = 1;
  s.black_pattern_cols = 2;
  s.black_pattern = {0, 0};
  s.white = static_cast<std::uint32_t>(kWhite);
  s.data_maximum = 3900;
  s.linear_max = std::array<std::int64_t, 4>{4000, 4000, 4000, 4000};
  s.linearization = decoder_linearization::decoder_curve;
  std::array<std::uint8_t, 32> c{};
  c.fill(0x11);
  s.curve_sha256 = c;
  s.route.decoder = raw_decoder_kind::libraw;
  s.route.reason = "dng_libraw_compression";
  s.route.compression = 1;
  s.route.sample_format = 1;
  s.route.bits_per_sample = 16;
  s.source = metadata_source{metadata_origin::decoder, "decoder_record"};
  if (astro_dng) {
    s.route.decoder = raw_decoder_kind::astro_dng;
    s.route.reason.clear();
    s.route.compression = 8;
    s.route.bits_per_sample = 32;
    dng_decoder_fields f;
    f.dng_version = {1, 7, 1, 0};
    f.dng_backward_version = {1, 4, 0, 0};
    f.tiles = true;
    f.tile_width = 16;
    f.tile_length = 16;
    f.segment_count = 1;
    f.predictor = 1;
    f.little_endian = true;
    f.zlib_runtime_version = "1.3.1";
    s.dng = f;
  }
  return s;
}

struct member_change {
  const char* campo;
  bool astro_dng;
  std::function<void(decoder_state&)> change;
};

// Un membro di contenuto di `decoder_state` alla volta (ADR-03.b §b.2; `source` è provenienza e non si confronta).
// Ogni modifica lascia lo stato valido, così l'unica differenza col frame della ricetta è il membro nominato.
std::vector<member_change> every_member() {
  return {
      {"raw_width", false, [](decoder_state& s) { s.raw_width += 1; }},
      {"raw_height", false, [](decoder_state& s) { s.raw_height += 1; }},
      {"window", false, [](decoder_state& s) { s.window.x0 = 0; }},
      {"black", false, [](decoder_state& s) { s.black += 1; }},
      {"cfa_color_index", false, [](decoder_state& s) { s.cfa_color_index = {1, 0, 3, 2}; }},
      {"black_per_color", false, [](decoder_state& s) { s.black_per_color[0] += 1; }},
      {"black_pattern_rows", false, [](decoder_state& s) {
         s.black_pattern_rows = 2;
         s.black_pattern = {0, 0, 0, 0};
       }},
      {"black_pattern_cols", false, [](decoder_state& s) {
         s.black_pattern_cols = 1;
         s.black_pattern = {0};
       }},
      {"black_pattern", false, [](decoder_state& s) { s.black_pattern = {0, 1}; }},
      {"white", false, [](decoder_state& s) { s.white += 1; }},
      {"data_maximum", false, [](decoder_state& s) { s.data_maximum = 3901; }},
      {"linear_max", false, [](decoder_state& s) { (*s.linear_max)[0] += 1; }},
      {"linearization", false, [](decoder_state& s) {
         s.linearization = decoder_linearization::identity;
         s.curve_sha256.reset();
       }},
      {"curve_sha256", false, [](decoder_state& s) { s.curve_sha256->fill(0x22); }},
      {"process_warnings", false, [](decoder_state& s) { s.process_warnings = 1; }},
      {"raw_inset_crops", false, [](decoder_state& s) { s.raw_inset_crops[0][0] += 1; }},
      {"flip", false, [](decoder_state& s) { s.flip = 3; }},
      {"route", false, [](decoder_state& s) { s.route.bits_per_sample = 12; }},
      {"dng", true, [](decoder_state& s) { s.dng->predictor = 2; }},
  };
}

// Ricetta dark RAW e immagini con lo stesso stato `base` in ogni frame.
std::pair<master_recipe, std::vector<image>> with_state(const std::vector<raw_frame>& frames, const decoder_state& base) {
  master_recipe r = raw_recipe(master_role::dark, frames);
  for (master_frame_spec& s : r.frames) s.decoder->state = base;
  std::vector<image> ims = images_of(frames);
  for (image& im : ims) im.decoder_state = base;
  return {std::move(r), std::move(ims)};
}

TEST(MasterB15, T_M3B_CFA_01_each_decoder_state_member_changed_in_the_image_stops_with_decoder_state_changed_naming_it) {
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  for (const bool astro_dng : {false, true}) {
    SCOPED_TRACE(astro_dng ? "controllo astro_dng" : "controllo libraw");
    auto [r, ims] = with_state(frames, roomy_state(astro_dng));
    EXPECT_NO_THROW((void)combine_master(r, std::move(ims))) << "controllo: stato uguale in ricetta e immagini";
  }
  for (const member_change& m : every_member()) {
    SCOPED_TRACE(m.campo);
    auto [r, ims] = with_state(frames, roomy_state(m.astro_dng));
    m.change(*ims[1].decoder_state);
    ASSERT_EQ(decoder_state_violation(*ims[1].decoder_state), nullptr) << "fixture: stato modificato non valido";
    const std::optional<stop_error> e = stop_of([&] { (void)combine_master(r, std::move(ims)); });
    ASSERT_TRUE(e.has_value()) << "membro diverso: stop atteso";
    EXPECT_EQ(e->category(), error_category::integrity) << e->what();
    EXPECT_EQ(e->code(), "master.decoder_state_changed") << e->what();
    const std::string changed = r.frames[1].digest.sha256 < r.frames[0].digest.sha256 ? "frame_000" : "frame_001";
    EXPECT_EQ(context_of(*e, "frame"), std::optional<std::string>(changed)) << e->what();
    EXPECT_EQ(context_of(*e, "campo"), std::optional<std::string>(m.campo)) << e->what();
  }
}

TEST(MasterB15, T_M3B_CFA_01_fits_image_with_a_decoder_state_stops_with_decoder_state_changed_on_the_decoder_state_field) {
  // Decisione B15: un'immagine con stato del decoder per un frame FITS della ricetta ha lo stato cambiato.
  const mst::group g = mst::make_group(master_role::dark, 8, 4, 2);
  const master_recipe r = mst::recipe_of(g);
  std::vector<image> ims = mst::images_of(g);
  EXPECT_NO_THROW((void)combine_master(r, ims)) << "controllo: frame FITS senza stato";
  ims[1].decoder_state = roomy_state(false);
  const std::optional<stop_error> e = stop_of([&] { (void)combine_master(r, std::move(ims)); });
  ASSERT_TRUE(e.has_value()) << "stato del decoder in un frame FITS: stop atteso";
  EXPECT_EQ(e->category(), error_category::integrity) << e->what();
  EXPECT_EQ(e->code(), "master.decoder_state_changed") << e->what();
  const std::string changed = r.frames[1].digest.sha256 < r.frames[0].digest.sha256 ? "frame_000" : "frame_001";
  EXPECT_EQ(context_of(*e, "frame"), std::optional<std::string>(changed)) << e->what();
  EXPECT_EQ(context_of(*e, "campo"), std::optional<std::string>("decoder_state")) << e->what();
}

// L9 e decisione B15: con frame RAW `offset_adu` è solo quello della ricetta; un valore nei metadati del frame, letto o
// dichiarato, è invalid_input `master.param.offset_adu`.
TEST(MasterB15, T_M3B_CFA_01_raw_image_with_offset_adu_in_its_metadata_stops_with_param_offset_adu) {
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  const master_recipe r = raw_recipe(master_role::flat, frames);
  std::vector<image> ims = images_of(frames);
  ims[1].metadata.offset_adu = metadata_value<double>{kOffset, {metadata_origin::header, "OFFSET"}};
  const std::optional<stop_error> e = stop_of([&] { (void)combine_master(r, std::move(ims)); });
  ASSERT_TRUE(e.has_value()) << "offset_adu nei metadati di un frame RAW: stop atteso";
  EXPECT_EQ(e->category(), error_category::invalid_input) << e->what();
  EXPECT_EQ(e->code(), "master.param.offset_adu") << e->what();
  // Stop del frame, non della ricetta: `frame` = nome canonico dell'ingresso, `valore` = valore del metadato.
  const std::string changed = r.frames[1].digest.sha256 < r.frames[0].digest.sha256 ? "frame_000" : "frame_001";
  EXPECT_EQ(context_of(*e, "frame"), std::optional<std::string>(changed)) << e->what();
  EXPECT_EQ(context_of(*e, "valore"), std::optional<std::string>("32")) << e->what();
}

TEST(MasterB15, T_M3B_CFA_01_raw_frame_with_a_declared_offset_adu_stops_with_param_offset_adu) {
  // Lo stesso valore della ricetta, dichiarato sul frame: lo stop non dipende dal valore.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  master_recipe r = raw_recipe(master_role::flat, frames);
  r.frames[0].declared.offset_adu = metadata_value<double>{kOffset, {metadata_origin::declared, {}}};
  const std::optional<stop_error> e = stop_of([&] { (void)combine_master(r, images_of(frames)); });
  ASSERT_TRUE(e.has_value()) << "offset_adu dichiarato su un frame RAW: stop atteso";
  EXPECT_EQ(e->category(), error_category::invalid_input) << e->what();
  EXPECT_EQ(e->code(), "master.param.offset_adu") << e->what();
  const std::string declared = r.frames[0].digest.sha256 < r.frames[1].digest.sha256 ? "frame_000" : "frame_001";
  EXPECT_EQ(context_of(*e, "frame"), std::optional<std::string>(declared)) << e->what();
  EXPECT_EQ(context_of(*e, "valore"), std::optional<std::string>("32")) << e->what();
}

const json* report_member(const json& report, std::initializer_list<const char*> path) {
  const json* cur = &report;
  for (const char* k : path) {
    if (cur == nullptr || cur->type() != json::kind::object) return nullptr;
    cur = cur->find(k);
  }
  return cur;
}

TEST(MasterB15, T_M3B_CFA_01_raw_flat_master_carries_the_recipe_offset_adu_with_declared_origin_in_metadata_and_report) {
  // Decisione B15 (C-02.b §b.7): nel master da RAW `offset_adu` = quello della ricetta, origine `declared`; nel report
  // `master_metadata.offset_adu` = {value 32, source "declared"}.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  master_result res;
  ASSERT_NO_THROW(res = combine_master(raw_recipe(master_role::flat, frames), images_of(frames)));
  ASSERT_TRUE(res.output.metadata.offset_adu.has_value());
  EXPECT_EQ(res.output.metadata.offset_adu->value, kOffset);
  EXPECT_EQ(res.output.metadata.offset_adu->source.origin, metadata_origin::declared);
  const json* value = report_member(res.report, {"master_metadata", "offset_adu", "value"});
  const json* source = report_member(res.report, {"master_metadata", "offset_adu", "source"});
  ASSERT_NE(value, nullptr) << "master_metadata.offset_adu.value assente";
  ASSERT_NE(source, nullptr) << "master_metadata.offset_adu.source assente";
  ASSERT_TRUE(value->type() == json::kind::number || value->type() == json::kind::integer);
  EXPECT_EQ(value->type() == json::kind::number ? value->as_number() : static_cast<double>(value->as_integer()), kOffset);
  ASSERT_EQ(source->type(), json::kind::string);
  EXPECT_EQ(source->as_string(), "declared");
}

TEST(MasterB15, T_M3B_CFA_01_raw_dark_master_without_recipe_offset_has_no_offset_adu_in_metadata_nor_report) {
  // Dark `mean` da RAW: la ricetta non porta `offset_adu` (L9), quindi il master non lo ha e il report non lo elenca.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  master_result res;
  ASSERT_NO_THROW(res = combine_master(raw_recipe(master_role::dark, frames), images_of(frames)));
  EXPECT_FALSE(res.output.metadata.offset_adu.has_value());
  ASSERT_NE(report_member(res.report, {"master_metadata"}), nullptr);
  EXPECT_EQ(report_member(res.report, {"master_metadata", "offset_adu"}), nullptr) << "offset_adu non dichiarato: assente";
}

TEST(MasterB15, T_M3B_CFA_01_master_node_from_decoded_dng_carries_the_decoded_states_in_canonical_order) {
  // Già coperto da B15a con stati scritti a mano: qui gli stati vengono da una decodifica reale.
  const std::vector<raw_frame> frames = {raw_frame_of(kF0), raw_frame_of(kF1)};
  const master_recipe r = raw_recipe(master_role::flat, frames);
  const node_spec node = make_master_node(r);
  const master_descriptor d = master_descriptor_from_node(node);
  ASSERT_TRUE(d.frame_decoder_states.has_value());
  ASSERT_EQ(d.frame_decoder_states->size(), 2u);
  const bool f0_first = r.frames[0].digest.sha256 < r.frames[1].digest.sha256;
  const std::size_t first = f0_first ? 0 : 1;
  EXPECT_EQ((*d.frame_decoder_states)[0].state, *frames[first].im.decoder_state);
  EXPECT_EQ((*d.frame_decoder_states)[1].state, *frames[1 - first].im.decoder_state);
  EXPECT_EQ((*d.frame_decoder_states)[0].identity, current_decoder_identity());
}

// ---------------------------------------------------------------- lettore RAW delle statistiche (L2)

fs::path fresh(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "b15" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

struct written {
  std::string path;
  content_digest digest;
};

written write_bytes(const std::string& name, const std::string& file, const std::vector<std::uint8_t>& bytes) {
  const fs::path path = fresh(name) / file;
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  out.close();
  return {path.string(), digest_of(bytes)};
}

const cancel_token& never() {
  static const cancel_token token;
  return token;
}

// Osservatore dei punti del confine (solo test): conta i punti e, se dato, chiama `on_point` a ogni punto.
class probe_points {
 public:
  explicit probe_points(std::function<void(std::string_view)> on_point = {}) : on_point_(std::move(on_point)) {
    job_probe::set_observer([this](std::string_view point, std::string_view) {
      points_.emplace_back(point);
      if (on_point_) on_point_(point);
    });
  }
  ~probe_points() { job_probe::set_observer({}); }
  std::size_t count(std::string_view point) const {
    return static_cast<std::size_t>(std::count(points_.begin(), points_.end(), point));
  }

 private:
  std::function<void(std::string_view)> on_point_;
  std::vector<std::string> points_;
};

master_stats_request levels_request() {
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = kOffset;
  return req;
}

master_frame_stats read_raw(const written& f, const master_stats_request& req, memory_pool& pool, const cancel_token& cancel = never()) {
  return read_master_frame_stats(f.path, f.digest, raw_input_format(), req, pool, cancel);
}

TEST(MasterB15, T_M3B_CFA_01_raw_reader_flat_levels_are_per_site_with_g1_and_g2_separate_and_the_request_offset) {
  // Siti (0,0) (0,1) (1,0) (1,1) = R, G1, G2, B: mediana del sito (= base) − offset_adu 32, nero 64 non sottratto.
  const written f = write_bytes("raw_levels", "frame.dng", dng_of(kF0));
  memory_pool pool(64 * 1024 * 1024);
  const master_frame_stats s = read_raw(f, levels_request(), pool);
  EXPECT_FALSE(s.decode_failure.has_value());
  ASSERT_EQ(s.levels.size(), 4u);
  const double expected[4] = {1968.0, 1468.0, 968.0, 1168.0};
  for (std::size_t c = 0; c < 4; ++c) {
    ASSERT_TRUE(s.levels[c].has_value()) << c;
    EXPECT_EQ(*s.levels[c], expected[c]) << "sito " << c;
  }
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_counts_valid_samples_at_or_above_the_saturation_threshold_over_all_pixels) {
  // 64 x 48 con tre pixel a 4000 (= soglia, saturi), uno a 3999 (non saturo) e gli altri a 1000.
  fx::dng_spec s;
  s.width = kW;
  s.height = kH;
  s.white = static_cast<std::uint32_t>(kWhite);
  s.black = {kBlack};
  std::vector<std::uint16_t> px(static_cast<std::size_t>(kW) * kH, 1000);
  px[0] = 4000;
  px[kW + 1] = 4000;
  px[px.size() - 1] = 4000;
  px[5] = 3999;
  s.pixels = fx::pixels_u16(px);
  const written f = write_bytes("raw_saturated", "frame.dng", fx::make_dng(s));
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.saturation_adu = kWhite;
  const master_frame_stats st = read_raw(f, req, pool);
  EXPECT_FALSE(st.decode_failure.has_value());
  EXPECT_EQ(st.saturated_samples, 3u);
  EXPECT_EQ(st.pixel_count, std::uint64_t{kW} * kH) << "denominatore di saturated_fraction: width·height dell'area visibile";
  EXPECT_TRUE(st.levels.empty()) << "livelli non richiesti";
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_altered_file_stops_with_input_digest_mismatch_and_returns_no_statistics) {
  written f = write_bytes("raw_mismatch", "frame.dng", dng_of(kF0));
  {  // ultimo byte del file alterato dopo il calcolo del digest
    std::fstream io(f.path, std::ios::in | std::ios::out | std::ios::binary);
    io.seekg(-1, std::ios::end);
    char b = 0;
    io.read(&b, 1);
    b = static_cast<char>(b ^ 0x01);
    io.seekp(-1, std::ios::end);
    io.write(&b, 1);
  }
  memory_pool pool(64 * 1024 * 1024);
  const std::optional<stop_error> e = stop_of([&] { (void)read_raw(f, levels_request(), pool); });
  ASSERT_TRUE(e.has_value()) << "file alterato: stop atteso, non statistiche né decode_failed";
  EXPECT_EQ(e->category(), error_category::integrity) << e->what();
  EXPECT_EQ(e->code(), "input.digest_mismatch") << e->what();
  EXPECT_EQ(key_set(*e), "atteso;letto;percorso;") << e->what();
  EXPECT_EQ(context_of(*e, "percorso"), std::optional<std::string>(f.path));
  EXPECT_EQ(context_of(*e, "atteso"), std::optional<std::string>(f.digest.sha256 + ":" + std::to_string(f.digest.size_bytes)));
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_truncated_file_with_the_digest_of_the_original_stops_with_digest_mismatch) {
  // Il troncamento farebbe fallire anche la decodifica: la verifica del digest precede la decodifica (OB-11).
  std::vector<std::uint8_t> bytes = dng_of(kF0);
  const content_digest original = digest_of(bytes);
  bytes.resize(bytes.size() / 2);
  written f = write_bytes("raw_truncated", "frame.dng", bytes);
  f.digest = original;
  memory_pool pool(64 * 1024 * 1024);
  const std::optional<stop_error> e = stop_of([&] { (void)read_raw(f, levels_request(), pool); });
  ASSERT_TRUE(e.has_value()) << "file troncato rispetto al record: stop atteso, non decode_failed";
  EXPECT_EQ(e->category(), error_category::integrity) << e->what();
  EXPECT_EQ(e->code(), "input.digest_mismatch") << e->what();
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_missing_file_stops_with_input_missing) {
  const fs::path dir = fresh("raw_missing");
  const std::string path = (dir / "assente.dng").string();
  memory_pool pool(64 * 1024 * 1024);
  const content_digest d{std::string(64, 'a'), 1000};
  const std::optional<stop_error> e =
      stop_of([&] { (void)read_master_frame_stats(path, d, raw_input_format(), master_stats_request{}, pool, never()); });
  ASSERT_TRUE(e.has_value()) << "file assente: stop atteso";
  EXPECT_EQ(e->category(), error_category::io) << e->what();
  EXPECT_EQ(e->code(), "input.missing") << e->what();
  EXPECT_EQ(key_set(*e), "errno;percorso;") << e->what();
  EXPECT_EQ(context_of(*e, "percorso"), std::optional<std::string>(path));
  EXPECT_EQ(context_of(*e, "errno"), std::optional<std::string>("2"));
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_cancel_requested_before_the_read_stops_with_sched_cancelled) {
  const written f = write_bytes("raw_cancel", "frame.dng", dng_of(kF0));
  memory_pool pool(64 * 1024 * 1024);
  cancel_token cancel;
  cancel.request();
  probe_points probe;
  const std::optional<stop_error> e = stop_of([&] { (void)read_raw(f, levels_request(), pool, cancel); });
  ASSERT_TRUE(e.has_value()) << "annullamento richiesto: stop atteso";
  EXPECT_EQ(e->category(), error_category::cancelled) << e->what();
  EXPECT_EQ(e->code(), "sched.cancelled") << e->what();
  EXPECT_TRUE(e->context().empty()) << e->what();
  EXPECT_EQ(probe.count("input.open"), 0u) << "primo controllo: prima dell'apertura del file";
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_cancel_requested_during_the_verified_read_stops_before_the_decoding) {
  // Secondo controllo, fra lettura verificata e decodifica: cancel richiesto alla prima lettura del file. Atteso lo stop
  // del lettore (`sched.cancelled`, contesto vuoto), non `raw.cancelled` del decoder né un esito di decodifica.
  const written f = write_bytes("raw_cancel_read", "frame.dng", dng_of(kF0));
  memory_pool pool(64 * 1024 * 1024);
  cancel_token cancel;
  probe_points probe([&cancel](std::string_view point) {
    if (point == "input.read") cancel.request();
  });
  const std::optional<stop_error> e = stop_of([&] { (void)read_raw(f, levels_request(), pool, cancel); });
  ASSERT_GE(probe.count("input.read"), 1u) << "la lettura deve essere cominciata";
  ASSERT_TRUE(e.has_value()) << "annullamento durante la lettura: stop atteso";
  EXPECT_EQ(e->category(), error_category::cancelled) << e->what();
  EXPECT_EQ(e->code(), "sched.cancelled") << e->what();
  EXPECT_TRUE(e->context().empty()) << e->what();
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_unsupported_raw_with_a_matching_digest_is_a_decode_failure_with_its_raw_code) {
  // LinearRaw a 3 campioni per pixel: unsupported raw.not_cfa (T-M3A-RAW-03). Digest coincidente: esito, non stop.
  fx::dng_spec s;
  s.width = kW;
  s.height = kH;
  s.white = static_cast<std::uint32_t>(kWhite);
  s.samples_per_pixel = 3;
  s.photometric = 34892;
  s.pixels.assign(static_cast<std::size_t>(kW) * kH * 6, 7);
  const written f = write_bytes("raw_not_cfa", "frame.dng", fx::make_dng(s));
  memory_pool pool(64 * 1024 * 1024);
  master_frame_stats st;
  ASSERT_NO_THROW(st = read_raw(f, levels_request(), pool));
  ASSERT_TRUE(st.decode_failure.has_value());
  EXPECT_EQ(st.decode_failure->code, "raw.not_cfa");
  EXPECT_FALSE(st.decode_failure->detail.empty());
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_raw_data_error_of_a_verified_file_is_a_decode_failure_not_an_integrity_stop) {
  // DNG che dichiara 64 x 48 a 16 bit e porta 1000 byte di campioni: LibRaw segnala un errore di dati durante unpack,
  // integrity raw.data_error (T-M3A-RAW-05). Decisione B12: per codice, `raw.*` compreso raw.data_error è un esito.
  fx::dng_spec s;
  s.width = kW;
  s.height = kH;
  s.white = static_cast<std::uint32_t>(kWhite);
  s.pixels = std::vector<std::uint8_t>(1000, 0x11);
  const written f = write_bytes("raw_data_error", "frame.dng", fx::make_dng(s));
  memory_pool pool(64 * 1024 * 1024);
  master_frame_stats st;
  ASSERT_NO_THROW(st = read_raw(f, levels_request(), pool));
  ASSERT_TRUE(st.decode_failure.has_value());
  EXPECT_EQ(st.decode_failure->code, "raw.data_error");
  EXPECT_FALSE(st.decode_failure->detail.empty());
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_pool_budget_below_the_decoder_reservation_stops_instead_of_a_decode_failure) {
  // Budget = file + 1 KiB: la lettura entra, la prenotazione del decoder (≥ 1 MiB per raw_image, ADR-08.b §2.8) no.
  // Un budget è uno stop (`sched.*`), non un esito di decodifica.
  const std::vector<std::uint8_t> bytes = dng_of(kF0);
  const written f = write_bytes("raw_budget", "frame.dng", bytes);
  memory_pool pool(bytes.size() + 1024);
  const std::optional<stop_error> e = stop_of([&] { (void)read_raw(f, levels_request(), pool); });
  ASSERT_TRUE(e.has_value()) << "budget insufficiente per la decodifica: stop atteso";
  EXPECT_EQ(e->category(), error_category::budget) << e->what();
  EXPECT_EQ(e->code(), "sched.job_exceeds_budget") << e->what();
  // Prenotazione di ADR-08.b §2.8 per il buffer 64 × 48: M · 2^20 + 6 · 64 · 48, M = max(1, ⌈2 · 64 · (48 + 8) / 2^20⌉) = 1.
  EXPECT_EQ(key_set(*e), "budget;file;prenotazione_decoder;") << e->what();
  EXPECT_EQ(context_of(*e, "prenotazione_decoder"), std::optional<std::string>(std::to_string((1u << 20) + 6u * kW * kH)));
  EXPECT_EQ(context_of(*e, "file"), std::optional<std::string>(std::to_string(bytes.size())));
  EXPECT_EQ(context_of(*e, "budget"), std::optional<std::string>(std::to_string(bytes.size() + 1024)));
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_file_larger_than_the_pool_budget_stops_in_the_verified_read) {
  // Budget sotto la dimensione del file: si ferma la lettura verificata (OB-11), prima di ogni decodifica.
  const std::vector<std::uint8_t> bytes = dng_of(kF0);
  const written f = write_bytes("raw_budget_file", "frame.dng", bytes);
  memory_pool pool(1024);
  const std::optional<stop_error> e = stop_of([&] { (void)read_raw(f, levels_request(), pool); });
  ASSERT_TRUE(e.has_value()) << "file oltre il budget: stop atteso";
  EXPECT_EQ(e->category(), error_category::budget) << e->what();
  EXPECT_EQ(e->code(), "sched.job_exceeds_budget") << e->what();
  EXPECT_EQ(context_of(*e, "dimensione"), std::optional<std::string>(std::to_string(bytes.size()))) << e->what();
  EXPECT_EQ(context_of(*e, "budget"), std::optional<std::string>("1024")) << e->what();
}

TEST(MasterB15, T_M3B_C02_08_L2_raw_reader_pixel_count_over_the_decoder_limit_is_a_decode_failure_of_the_file) {
  // Limite proprio del file (decisione B15, giro 1): il DNG dichiara 20000 × 20000 > raw_limits::max_pixels = 2^28
  // (ADR-08.b §2.1) e porta 1000 byte; digest coincidente, quindi esito `raw.max_pixels`, non uno stop.
  fx::dng_spec s;
  s.width = 20000;
  s.height = 20000;
  s.white = static_cast<std::uint32_t>(kWhite);
  s.pixels = std::vector<std::uint8_t>(1000, 0);
  const written f = write_bytes("raw_max_pixels", "frame.dng", fx::make_dng(s));
  memory_pool pool(64 * 1024 * 1024);
  master_frame_stats st;
  ASSERT_NO_THROW(st = read_raw(f, levels_request(), pool));
  ASSERT_TRUE(st.decode_failure.has_value());
  EXPECT_EQ(st.decode_failure->code, "raw.max_pixels");
}

TEST(MasterB15, T_M3B_C02_08_L2_reader_with_the_fits_format_reads_a_fits_frame_like_the_fits_reader) {
  // Il sovraccarico col formato, per un FITS, è il lettore FITS di B12: stessi livelli e conteggi.
  fx::calib_header hdr;
  hdr.imagetyp = "Flat Field";
  hdr.exposure = 5;
  const fx::bytes file = fx::float_frame(4, 2, -64, {20, 30, 40, 50, 60, 70, 100, 200}, fx::calib_cards(hdr));
  const written f = write_bytes("fits_overload", "frame.fits", file);
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  req.saturation_adu = 100.0;
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, fits_input_format(), req, pool, never());
  EXPECT_FALSE(s.decode_failure.has_value());
  EXPECT_EQ(s.pixel_count, 8u);
  EXPECT_EQ(s.saturated_samples, 2u) << "100 e 200";
  ASSERT_EQ(s.levels.size(), 1u);
  ASSERT_TRUE(s.levels[0].has_value());
  EXPECT_EQ(*s.levels[0], 45.0) << "mediana di {10,20,30,40,50,60,90,190}";
}

// Decisione B15 («Classificazione degli errori del decoder»): esito del file solo per i `raw.*` attribuiti in positivo al
// file; `raw.too_big` esito solo con `rifiuto` = tetto (zlib) o `valore` = rc di LIBRAW_TOO_BIG. Gli stop sintetici hanno
// la forma di quelli del decoder: `libraw_stop` (categoria e codice dalla tabella degli rc, contesto `funzione`,
// `valore`, `libraw`), `zlib_stop` (`funzione`, `codice`, `msg`, `segmento`, `tetto`, `rifiuto`), stop dei tag e del
// modo. Valori degli rc da libraw_const.h (il test non collega LibRaw): TOO_BIG -100012, UNSUFFICIENT_MEMORY -100007,
// MEMPOOL_OVERFLOW -100013, DATA_ERROR -100008, CANCELLED_BY_CALLBACK -100010, UNSPECIFIED_ERROR -1.
TEST(MasterB15, T_M3B_C02_08_L2_raw_decoder_errors_are_file_outcomes_only_when_attributed_to_the_file) {
  using ctx = stop_error::context_list;
  const auto libraw = [](error_category cat, const char* code, int rc) {
    return stop_error(cat, code, "LibRaw ha restituito un errore",
                      ctx{{"funzione", "unpack"}, {"valore", std::to_string(rc)}, {"libraw", "testo di LibRaw"}});
  };
  const auto zlib = [](const char* refused) {
    return stop_error(error_category::limit_exceeded, "raw.too_big", "memoria di zlib",
                      ctx{{"funzione", "inflateInit"}, {"codice", "-4"}, {"msg", ""}, {"segmento", "0"}, {"tetto", "65536"},
                          {"rifiuto", refused}});
  };
  struct row {
    const char* name;
    stop_error e;
    bool outcome;
  };
  const std::vector<row> rows = {
      {"raw.max_pixels", stop_error(error_category::limit_exceeded, "raw.max_pixels", "dimensioni dichiarate oltre il limite",
                                    ctx{{"pixel", "400000000"}, {"limite", "268435456"}}),
       true},
      {"raw.not_cfa", stop_error(error_category::unsupported, "raw.not_cfa", "dati raw a più canali per pixel"), true},
      {"raw.data_error", libraw(error_category::integrity, "raw.data_error", -100008), true},
      {"raw.too_big LIBRAW_TOO_BIG", libraw(error_category::limit_exceeded, "raw.too_big", -100012), true},
      {"raw.too_big tetto di zlib", zlib("tetto"), true},
      {"raw.cancelled", libraw(error_category::cancelled, "raw.cancelled", -100010), false},
      {"raw.decode_context", stop_error(error_category::invalid_input, "raw.decode_context", "modo del decoder non valido"),
       false},
      {"raw.libraw_error", libraw(error_category::invalid_input, "raw.libraw_error", -1), false},
      {"raw.too_big LIBRAW_UNSUFFICIENT_MEMORY", libraw(error_category::limit_exceeded, "raw.too_big", -100007), false},
      {"raw.too_big LIBRAW_MEMPOOL_OVERFLOW", libraw(error_category::limit_exceeded, "raw.too_big", -100013), false},
      {"raw.too_big memoria negata dal sistema", zlib("sistema"), false},
      {"raw.too_big senza contesto", stop_error(error_category::limit_exceeded, "raw.too_big", "oltre il limite"), false},
      {"codice fuori da raw.*", stop_error(error_category::budget, "sched.job_exceeds_budget", "budget"), false},
      {"raw.* di categoria io", stop_error(error_category::io, "raw.read", "pread non riuscita", ctx{{"errno", "5"}}), false},
  };
  for (const row& r : rows) {
    SCOPED_TRACE(r.name);
    EXPECT_EQ(master_detail::raw_decode_error_is_file_outcome(r.e), r.outcome) << (r.outcome ? "atteso esito" : "atteso stop");
  }
}

// ---------------------------------------------------------------- residuo di B12: check_unchanged sul ramo d'errore del piano

// Osservatore dei punti del confine (solo test): al primo `input.read` (dentro `plan_fits`, dopo l'apertura e il suo
// fstat) cambia un byte del file sul posto e lo ripristina, poi porta mtime a un istante fisso del passato: il contenuto
// e il digest restano quelli attesi, l'identità del file aperto no. L'mtime esplicito rende il caso deterministico anche
// con timestamp a grana grossa.
struct rewrite_on_first_read {
  explicit rewrite_on_first_read(std::string path) : path_(std::move(path)) {
    job_probe::set_observer([this](std::string_view point, std::string_view) {
      if (point != "input.read" || done_) return;
      done_ = true;
      const int fd = ::open(path_.c_str(), O_RDWR | O_CLOEXEC);
      if (fd < 0) return failed("open");
      unsigned char original = 0;
      const unsigned char* restored = &original;
      bool ok = ::pread(fd, &original, 1, 0) == 1;
      const unsigned char changed = original ^ 0x01;
      ok = ok && ::pwrite(fd, &changed, 1, 0) == 1 && ::pwrite(fd, restored, 1, 0) == 1;
      ::close(fd);
      if (!ok) return failed("lettura o scrittura del byte");
      const struct timespec times[2] = {{0, UTIME_OMIT}, {1000, 0}};
      if (::utimensat(AT_FDCWD, path_.c_str(), times, 0) != 0) return failed("utimensat");
    });
  }
  ~rewrite_on_first_read() { job_probe::set_observer({}); }
  bool done() const { return done_; }
  const std::string& error() const { return error_; }

 private:
  void failed(const char* what) { error_ = what; }
  std::string path_;
  bool done_ = false;
  std::string error_;
};

TEST(MasterB15, T_M3B_C02_08_L2_reader_fits_changed_in_place_and_restored_before_the_digest_on_the_plan_error_branch_stops_with_integrity) {
  // FITS troncato di un byte: `plan_fits` si ferma con un codice `fits.*` (precondizione verificata sotto). Con il file
  // cambiato e ripristinato dopo l'apertura il digest coincide; `check_unchanged` dopo il digest deve fermare con
  // integrity input.modified (OB-11), non restituire decode_failure.
  fx::calib_header hdr;
  hdr.imagetyp = "Flat Field";
  fx::bytes file = fx::float_frame(4, 2, -64, {1, 2, 3, 4, 5, 6, 7, 8}, fx::calib_cards(hdr));
  file.pop_back();
  const written f = write_bytes("fits_restored", "frame.fits", file);
  {
    const fits_input in = open_fits_input(f.path);
    const std::optional<stop_error> pre = stop_of([&] { (void)plan_fits(in); });
    ASSERT_TRUE(pre.has_value() && pre->code().rfind("fits.", 0) == 0) << "fixture: il piano deve fallire con fits.*";
  }
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  std::optional<stop_error> e;
  master_frame_stats s;
  {
    rewrite_on_first_read probe(f.path);
    e = stop_of([&] { s = read_master_frame_stats(f.path, f.digest, req, pool, never()); });
    ASSERT_TRUE(probe.done()) << "nessuna lettura osservata";
    ASSERT_TRUE(probe.error().empty()) << "riscrittura del file non riuscita: " << probe.error();
  }
  ASSERT_TRUE(e.has_value()) << "file cambiato dopo l'apertura: stop atteso, ottenuto "
                             << (s.decode_failure ? "decode_failure " + s.decode_failure->code : std::string("statistiche"));
  EXPECT_EQ(e->category(), error_category::integrity) << e->what();
  EXPECT_EQ(e->code(), "input.modified") << e->what();
}

}  // namespace
