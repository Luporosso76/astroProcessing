// M3.b, unità B12: analisi dei master, esclusioni per frame. C-02.b §b.2 («Analisi del set candidato», punto 1: lista
// chiusa dei motivi), §b.10 (`master.no_usable_frames`, `master.param.saturation_adu`); decisioni L2 (lettore di
// statistiche per frame), L4 (parametri d'analisi, `params_missing`), L9 (`offset_adu`).
// Test: T-M3B-C02-08 (ogni motivo da solo, FITS e RAW come ingressi puri), T-M3B-C02-12 (parte di analisi),
// T-M3B-C02-15 (solo `master.no_usable_frames`, contatore del fault_injector), più le prove aggiunte di B12: lettore FITS
// (`input.digest_mismatch`, `input.missing`, cancel), parametri assenti uno alla volta (L4).
// Fuori da B12: gruppi e livelli del flat (B13), preflight dei nodi proposti (B14), lettore RAW (B15).
//
// API sotto prova: `astro/master_analysis.hpp`.
//
// Convenzione del contatore: `screen_master_frames` chiama `fault("master.analysis.groups")` una volta, dopo le
// esclusioni e prima di ogni formazione di gruppi (B13) e di proposte (B14); un'analisi che si ferma prima non la chiama.
//
// Tutti gli esiti attesi sono scritti a mano dai paragrafi citati. Nessun valore atteso viene dal prodotto.
#include <gtest/gtest.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/cancel.hpp"
#include "astro/error.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/master_analysis.hpp"
#include "astro/memory_pool.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;

constexpr std::uint32_t kW = 4;
constexpr std::uint32_t kH = 4;

// ---------------------------------------------------------------- immagini, osservazioni, ingressi

metadata_value<double> hd(double v, const char* key) { return {v, {metadata_origin::header, key}}; }
metadata_value<std::string> hs(const std::string& v, const char* key) { return {v, {metadata_origin::header, key}}; }

content_digest digest_with(char c) { return content_digest{std::string(64, c), 2880}; }

// Frame FITS nella regola di C-02.a §2: fase `raw`, `linear`, `adu`, binning 1 x 1, tutti i campi di compatibilità.
image frame_image(master_role role, plane_layout layout = plane_layout::mono) {
  image_geometry g(kW, kH, 1);
  g.binning_x = 1;
  g.binning_y = 1;
  std::optional<cfa_layout> cfa;
  if (layout == plane_layout::cfa) cfa = cfa_layout{cfa_pattern::rggb, 0, 0};
  image im = make_image(g, sample_type::f64, layout, cfa);
  double* p = im.data<double>();
  for (std::uint64_t i = 0; i < g.sample_count(); ++i) p[i] = 1000.0;
  im.unit = pixel_unit::adu;
  im.lin = linearity::linear;
  im.lin_origin.value = metadata_origin::header;
  im.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::header, "ASTRPHAS"}};
  im.metadata.exposure_s = hd(role == master_role::bias ? 0.001 : 60.0, "EXPTIME");
  im.metadata.gain = hd(100.0, "GAIN");
  im.metadata.offset_adu = hd(10.0, "OFFSET");
  im.metadata.temperature_c = hd(-10.0, "CCD-TEMP");
  im.metadata.filter = hs("L", "FILTER");
  im.metadata.camera = hs("cam-a", "INSTRUME");
  return im;
}

std::string declarations(const std::map<std::string, json>& fields = {}, const std::map<std::string, json>& meta = {}) {
  json f = json::object();
  for (const auto& [k, v] : fields) f.set(k, v);
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  doc.set("declared_image_fields", std::move(f));
  if (!meta.empty()) {
    json m = json::object();
    for (const auto& [k, v] : meta) m.set(k, v);
    doc.set("frame_metadata", std::move(m));
  }
  return canonical_json(doc);
}

decoder_state base_state() {
  decoder_state s;
  s.raw_width = 8;
  s.raw_height = 8;
  s.window = decoder_window{2, 2, kW, kH};
  s.black = 512;
  s.cfa_color_index = {0, 1, 3, 2};
  s.black_per_color = {0, 0, 0, 0};
  s.black_pattern_rows = 2;
  s.black_pattern_cols = 2;
  s.black_pattern = {0, 0, 0, 0};
  s.white = 16383;
  s.linearization = decoder_linearization::identity;
  s.source = metadata_source{metadata_origin::decoder, "color.black"};
  return s;
}

decoder_identity identity_a() { return decoder_identity{"0.22.3-Release", 0x001603, 0xc0, std::string(64, '1')}; }

// Frame RAW: il decoder non produce binning, filtro né `offset_adu` per frame (ADR-08.b §2.6, L9): binning e filtro
// vengono dalle dichiarazioni.
image raw_frame_image(master_role role) {
  image im = frame_image(role, plane_layout::cfa);
  im.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::decoder, "decoder"}};
  im.metadata.offset_adu.reset();
  im.geometry.binning_x.reset();
  im.geometry.binning_y.reset();
  im.metadata.filter.reset();
  im.decoder_state = base_state();
  return im;
}

std::string raw_decl() {
  return declarations({{"binning_x", json::uinteger(1)}, {"binning_y", json::uinteger(1)}}, {{"filter", json::string("L")}});
}

master_frame_stats stats_ok(std::uint64_t saturated = 0, std::vector<std::optional<double>> levels = {500.0}) {
  master_frame_stats s;
  s.saturated_samples = saturated;
  s.pixel_count = kW * kH;
  s.levels = std::move(levels);
  return s;
}

master_analysis_frame make_frame(std::uint64_t id, char tag, const image& img, const std::string& decl = declarations(),
                                 std::optional<master_frame_stats> stats = std::nullopt) {
  master_analysis_frame f;
  f.input_id = id;
  f.digest = digest_with(tag);
  f.observation = make_observation(img, decl);
  f.stats = std::move(stats);
  return f;
}

master_analysis_frame make_raw_frame(std::uint64_t id, char tag, const image& img,
                                     std::optional<master_frame_stats> stats = std::nullopt) {
  master_analysis_frame f;
  f.input_id = id;
  f.digest = digest_with(tag);
  f.observation = make_observation(img, raw_decl(), identity_a());
  f.stats = std::move(stats);
  return f;
}

// Parametri completi per ogni ruolo e per i RAW; saturazione non dichiarata (si aggiunge dove serve).
master_analysis_params full_params() {
  master_analysis_params p;
  p.exposure_rel_tol = 0.01;
  p.temperature_abs_tol_c = 1.0;
  p.temp_policy = temperature_policy::compare;
  p.decoder_black_abs_tol_adu = 10;
  p.flat_min_signal_adu = 100.0;
  p.flat_level_rel_tol = 0.25;
  p.offset_adu = 10.0;
  return p;
}

master_analysis_params with_saturation(master_analysis_params p, double adu = 4095.0, double fraction_max = 0.25) {
  p.saturation_adu = adu;
  p.saturated_fraction_max = fraction_max;
  return p;
}

std::string key_set(const stop_error& e) {
  std::vector<std::string> keys;
  for (const auto& kv : e.context()) keys.push_back(kv.first);
  std::sort(keys.begin(), keys.end());
  std::string out;
  for (const std::string& k : keys) out += k + ";";
  return out;
}

std::optional<std::string> context_of(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

json obj(std::initializer_list<std::pair<std::string, json>> fields) {
  json o = json::object();
  for (const auto& [k, v] : fields) o.set(k, v);
  return o;
}

// Un frame buono (id 1, digest 'a') accanto a quello in prova (id 2, digest 'b'): il buono resta utilizzabile, quello in
// prova è escluso per esattamente un motivo, con i valori attesi. Il motivo è l'unico: l'altro frame non è escluso.
void expect_only_second_excluded(master_role role, const master_analysis_frame& good, const master_analysis_frame& tested,
                                 const master_analysis_params& params, const std::string& reason, const json& values) {
  const master_screening r = screen_master_frames(role, {good, tested}, params);
  EXPECT_TRUE(r.missing_params.empty());
  EXPECT_EQ(r.usable, std::vector<std::uint64_t>{good.input_id});
  ASSERT_EQ(r.excluded.size(), 1u);
  EXPECT_EQ(r.excluded[0].input_id, tested.input_id);
  EXPECT_EQ(r.excluded[0].digest, tested.digest);
  EXPECT_EQ(r.excluded[0].reason, reason);
  EXPECT_EQ(canonical_json(r.excluded[0].values), canonical_json(values));
}

// ---------------------------------------------------------------- controllo positivo

TEST(MasterB12, T_M3B_C02_08_valid_frames_of_every_role_are_all_usable_and_nothing_is_excluded) {
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    const master_screening r = screen_master_frames(
        role, {make_frame(1, 'a', frame_image(role), declarations(), stats_ok()), make_frame(2, 'b', frame_image(role), declarations(), stats_ok())},
        full_params());
    EXPECT_TRUE(r.excluded.empty());
    EXPECT_TRUE(r.missing_params.empty());
    EXPECT_EQ(r.usable, (std::vector<std::uint64_t>{1, 2}));
  }
}

// ---------------------------------------------------------------- T-M3B-C02-08: ogni motivo da solo (FITS)

TEST(MasterB12, T_M3B_C02_08_layout_rgb_is_excluded_with_the_layout_name) {
  image rgb = frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', rgb),
                              full_params(), "layout", obj({{"layout", json::string("rgb")}}));
}

TEST(MasterB12, T_M3B_C02_08_layout_multi_plane_is_excluded_with_the_layout_name) {
  image multi = frame_image(master_role::dark);
  multi.layout = plane_layout::multi_plane;
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', multi),
                              full_params(), "layout", obj({{"layout", json::string("multi_plane")}}));
}

TEST(MasterB12, T_M3B_C02_08_pedestal_different_from_zero_is_excluded_with_the_value) {
  image p = frame_image(master_role::dark);
  p.metadata.pedestal_adu = hd(5.0, "PEDESTAL");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', p),
                              full_params(), "pedestal", obj({{"pedestal_adu", json::number(5.0)}}));
}

TEST(MasterB12, T_M3B_C02_08_pedestal_equal_to_zero_is_not_an_exclusion) {
  image p = frame_image(master_role::dark);
  p.metadata.pedestal_adu = hd(0.0, "PEDESTAL");
  const master_screening r = screen_master_frames(master_role::dark, {make_frame(1, 'a', p)}, full_params());
  EXPECT_TRUE(r.excluded.empty());
  EXPECT_EQ(r.usable, std::vector<std::uint64_t>{1});
}

TEST(MasterB12, T_M3B_C02_08_input_variance_plane_is_excluded) {
  image v = frame_image(master_role::dark);
  v.variance = variance_plane{aligned_buffer(kW * kH * sizeof(double))};
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', v),
                              full_params(), "input_variance", json::object());
}

TEST(MasterB12, T_M3B_C02_08_imagetyp_of_another_role_is_excluded_with_imagetyp_and_expected_role) {
  image i = frame_image(master_role::dark);
  i.metadata.imagetyp = hs("Light Frame", "IMAGETYP");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "imagetyp_conflict",
                              obj({{"imagetyp", json::string("Light Frame")}, {"ruolo_atteso", json::string("dark")}}));
}

TEST(MasterB12, T_M3B_C02_08_dark_imagetyp_is_admitted_for_the_dark_flat_role) {
  image i = frame_image(master_role::dark_flat);
  i.metadata.imagetyp = hs("Dark Frame", "IMAGETYP");
  const master_screening r = screen_master_frames(master_role::dark_flat, {make_frame(1, 'a', i)}, full_params());
  EXPECT_TRUE(r.excluded.empty());
}

TEST(MasterB12, T_M3B_C02_08_each_missing_compatibility_field_is_excluded_alone_with_its_name) {
  struct field {
    const char* name;
    master_role role;
    std::function<void(image&)> remove;
  };
  const std::vector<field> fields = {
      {"camera", master_role::dark, [](image& i) { i.metadata.camera.reset(); }},
      {"gain", master_role::dark, [](image& i) { i.metadata.gain.reset(); }},
      {"offset_adu", master_role::dark, [](image& i) { i.metadata.offset_adu.reset(); }},
      {"exposure_s", master_role::dark, [](image& i) { i.metadata.exposure_s.reset(); }},
      {"temperature_c", master_role::dark, [](image& i) { i.metadata.temperature_c.reset(); }},
      {"filter", master_role::flat, [](image& i) { i.metadata.filter.reset(); }},
      {"binning_x", master_role::dark, [](image& i) { i.geometry.binning_x.reset(); }},
      {"binning_y", master_role::dark, [](image& i) { i.geometry.binning_y.reset(); }},
  };
  for (const field& f : fields) {
    SCOPED_TRACE(f.name);
    image bad = frame_image(f.role);
    f.remove(bad);
    expect_only_second_excluded(f.role, make_frame(1, 'a', frame_image(f.role), declarations(), stats_ok()),
                                make_frame(2, 'b', bad, declarations(), stats_ok()), full_params(),
                                std::string("metadata_missing.") + f.name, obj({{"campo", json::string(f.name)}}));
  }
}

TEST(MasterB12, T_M3B_C02_08_filter_is_required_only_for_the_flat) {
  image no_filter = frame_image(master_role::dark);
  no_filter.metadata.filter.reset();
  const master_screening r = screen_master_frames(master_role::dark, {make_frame(1, 'a', no_filter)}, full_params());
  EXPECT_TRUE(r.excluded.empty());
}

TEST(MasterB12, T_M3B_C02_08_temperature_is_not_required_when_the_policy_is_not_applicable_declared) {
  master_analysis_params p = full_params();
  p.temp_policy = temperature_policy::not_applicable_declared;
  image no_temp = frame_image(master_role::dark);
  no_temp.metadata.temperature_c.reset();
  const master_screening r = screen_master_frames(master_role::dark, {make_frame(1, 'a', no_temp)}, p);
  EXPECT_TRUE(r.excluded.empty());
}

TEST(MasterB12, T_M3B_C02_08_copies_with_the_same_digest_are_excluded_except_the_first_in_canonical_order) {
  const master_analysis_frame first = make_frame(3, 'a', frame_image(master_role::dark));
  const master_analysis_frame copy_1 = make_frame(5, 'a', frame_image(master_role::dark));
  const master_analysis_frame copy_2 = make_frame(9, 'a', frame_image(master_role::dark));
  const master_analysis_frame other = make_frame(1, 'b', frame_image(master_role::dark));
  const master_screening r = screen_master_frames(master_role::dark, {copy_2, other, copy_1, first}, full_params());
  EXPECT_EQ(r.usable, (std::vector<std::uint64_t>{3, 1}));  // digest 'a' (input 3), poi digest 'b' (input 1)
  ASSERT_EQ(r.excluded.size(), 2u);
  EXPECT_EQ(r.excluded[0].input_id, 5u);
  EXPECT_EQ(r.excluded[1].input_id, 9u);
  for (const master_exclusion& e : r.excluded) {
    EXPECT_EQ(e.reason, "duplicate_digest");
    EXPECT_EQ(e.digest, digest_with('a'));
    EXPECT_EQ(canonical_json(e.values), canonical_json(obj({{"primo", json::uinteger(3)}})));
  }
}

TEST(MasterB12, T_M3B_C02_08_decode_failed_is_excluded_with_the_code_and_the_detail) {
  master_frame_stats failed;
  failed.decode_failure = master_decode_failure{"raw.data_error", "dati corrotti nel tassello 3"};
  expect_only_second_excluded(
      master_role::dark, make_frame(1, 'a', frame_image(master_role::dark), declarations(), stats_ok()),
      make_frame(2, 'b', frame_image(master_role::dark), declarations(), failed), with_saturation(full_params()), "decode_failed",
      obj({{"codice", json::string("raw.data_error")}, {"dettaglio", json::string("dati corrotti nel tassello 3")}}));
}

TEST(MasterB12, T_M3B_C02_08_decode_failed_of_a_raw_frame_is_excluded_as_a_pure_input) {
  master_frame_stats failed;
  failed.decode_failure = master_decode_failure{"raw.libraw_error", "unpack fallito"};
  expect_only_second_excluded(
      master_role::dark, make_raw_frame(1, 'a', raw_frame_image(master_role::dark), stats_ok()),
      make_raw_frame(2, 'b', raw_frame_image(master_role::dark), failed), with_saturation(full_params()), "decode_failed",
      obj({{"codice", json::string("raw.libraw_error")}, {"dettaglio", json::string("unpack fallito")}}));
}

TEST(MasterB12, T_M3B_C02_08_fits_saturation_threshold_different_from_the_analysis_one_is_a_saturation_conflict) {
  image conflict = frame_image(master_role::dark);
  conflict.metadata.saturation_adu = hd(4000.0, "SATURATE");
  image same = frame_image(master_role::dark);
  same.metadata.saturation_adu = hd(4095.0, "SATURATE");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', same, declarations(), stats_ok()),
                              make_frame(2, 'b', conflict, declarations(), stats_ok()), with_saturation(full_params()),
                              "saturation_conflict",
                              obj({{"analisi", json::number(4095.0)}, {"soglia_frame", json::number(4000.0)}}));
}

TEST(MasterB12, T_M3B_C02_08_raw_saturation_threshold_from_the_decoder_different_from_the_analysis_one_is_a_saturation_conflict) {
  image conflict = raw_frame_image(master_role::dark);
  conflict.metadata.saturation_adu = metadata_value<double>{16383.0, {metadata_origin::decoder, "color.maximum"}};
  image same = raw_frame_image(master_role::dark);
  same.metadata.saturation_adu = metadata_value<double>{15000.0, {metadata_origin::decoder, "color.maximum"}};
  expect_only_second_excluded(master_role::dark, make_raw_frame(1, 'a', same, stats_ok()),
                              make_raw_frame(2, 'b', conflict, stats_ok()), with_saturation(full_params(), 15000.0),
                              "saturation_conflict",
                              obj({{"analisi", json::number(15000.0)}, {"soglia_frame", json::number(16383.0)}}));
}

TEST(MasterB12, T_M3B_C02_08_saturated_fraction_above_the_maximum_is_excluded_with_counts_and_fraction) {
  // 5 di 16 campioni saturi = 0,3125 > 0,25
  expect_only_second_excluded(
      master_role::dark, make_frame(1, 'a', frame_image(master_role::dark), declarations(), stats_ok(0)),
      make_frame(2, 'b', frame_image(master_role::dark), declarations(), stats_ok(5)), with_saturation(full_params()),
      "saturated_fraction",
      obj({{"frazione", json::number(0.3125)}, {"massimo", json::number(0.25)}, {"saturi", json::uinteger(5)}, {"totale", json::uinteger(16)}}));
}

TEST(MasterB12, T_M3B_C02_08_saturated_fraction_equal_to_the_maximum_is_admitted) {
  const master_screening r = screen_master_frames(
      master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark), declarations(), stats_ok(4))}, with_saturation(full_params()));
  EXPECT_TRUE(r.excluded.empty()) << "4 di 16 = 0,25 non supera 0,25";
  EXPECT_EQ(r.usable, std::vector<std::uint64_t>{1});
}

TEST(MasterB12, T_M3B_C02_08_saturated_fraction_is_not_evaluated_without_saturation_adu) {
  const master_screening r = screen_master_frames(
      master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark), declarations(), stats_ok(16))}, full_params());
  EXPECT_TRUE(r.excluded.empty());
  EXPECT_EQ(r.usable, std::vector<std::uint64_t>{1});
}

TEST(MasterB12, T_M3B_C02_08_flat_level_below_the_minimum_signal_is_excluded_with_level_and_minimum) {
  expect_only_second_excluded(master_role::flat,
                              make_frame(1, 'a', frame_image(master_role::flat), declarations(), stats_ok(0, {500.0})),
                              make_frame(2, 'b', frame_image(master_role::flat), declarations(), stats_ok(0, {99.5})), full_params(),
                              "flat_low_signal", obj({{"livello", json::number(99.5)}, {"minimo", json::number(100.0)}, {"motivo", json::string("sotto_minimo")}}));
}

TEST(MasterB12, T_M3B_C02_08_flat_level_equal_to_the_minimum_signal_is_admitted) {
  const master_screening r = screen_master_frames(
      master_role::flat, {make_frame(1, 'a', frame_image(master_role::flat), declarations(), stats_ok(0, {100.0}))}, full_params());
  EXPECT_TRUE(r.excluded.empty());
}

TEST(MasterB12, T_M3B_C02_08_flat_channel_without_valid_pixels_is_excluded_as_low_signal_with_the_channel_label) {
  image cfa_flat = frame_image(master_role::flat, plane_layout::cfa);
  const std::vector<std::optional<double>> levels = {500.0, 500.0, std::nullopt, 500.0};  // canale G2 senza pixel validi
  expect_only_second_excluded(
      master_role::flat,
      make_frame(1, 'a', cfa_flat, declarations(), stats_ok(0, {500.0, 500.0, 500.0, 500.0})),
      make_frame(2, 'b', cfa_flat, declarations(), stats_ok(0, levels)), full_params(), "flat_low_signal",
      obj({{"canale", json::string("G2")}, {"livello", json::null()}, {"minimo", json::number(100.0)}, {"motivo", json::string("assente")}}));
}

TEST(MasterB12, T_M3B_C02_08_flat_cfa_channel_below_the_minimum_names_the_channel) {
  image cfa_flat = frame_image(master_role::flat, plane_layout::cfa);
  expect_only_second_excluded(
      master_role::flat,
      make_frame(1, 'a', cfa_flat, declarations(), stats_ok(0, {500.0, 500.0, 500.0, 500.0})),
      make_frame(2, 'b', cfa_flat, declarations(), stats_ok(0, {500.0, 80.0, 500.0, 500.0})), full_params(), "flat_low_signal",
      obj({{"canale", json::string("G1")}, {"livello", json::number(80.0)}, {"minimo", json::number(100.0)}, {"motivo", json::string("sotto_minimo")}}));
}

TEST(MasterB12, T_M3B_C02_08_flat_levels_are_not_a_reason_for_other_roles) {
  const master_screening r = screen_master_frames(
      master_role::dark_flat, {make_frame(1, 'a', frame_image(master_role::dark_flat), declarations(), stats_ok(0, {1.0}))}, full_params());
  EXPECT_TRUE(r.excluded.empty());
}

TEST(MasterB12, T_M3B_C02_08_raw_frame_without_offset_adu_is_not_excluded_because_the_offset_is_declared_once) {
  const master_screening r =
      screen_master_frames(master_role::dark, {make_raw_frame(1, 'a', raw_frame_image(master_role::dark))}, full_params());
  EXPECT_TRUE(r.excluded.empty());
  EXPECT_EQ(r.usable, std::vector<std::uint64_t>{1});
}

TEST(MasterB12, T_M3B_C02_08_excluded_frame_has_only_the_first_reason_of_the_closed_list) {
  // layout viene prima di pedestal e di imagetyp_conflict nella lista chiusa di C-02.b §b.2
  image bad = frame_image(master_role::dark);
  bad.layout = plane_layout::rgb;
  bad.metadata.pedestal_adu = hd(5.0, "PEDESTAL");
  bad.metadata.imagetyp = hs("Light Frame", "IMAGETYP");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', bad),
                              full_params(), "layout", obj({{"layout", json::string("rgb")}}));
}

TEST(MasterB12, T_M3B_C02_08_screening_does_not_depend_on_the_order_of_the_frames) {
  image bad = frame_image(master_role::dark);
  bad.metadata.pedestal_adu = hd(5.0, "PEDESTAL");
  const std::vector<master_analysis_frame> frames = {make_frame(4, 'd', frame_image(master_role::dark)), make_frame(2, 'b', bad),
                                                     make_frame(3, 'c', frame_image(master_role::dark)),
                                                     make_frame(1, 'a', frame_image(master_role::dark))};
  const master_screening reference = screen_master_frames(master_role::dark, frames, full_params());
  std::vector<master_analysis_frame> permuted = frames;
  std::reverse(permuted.begin(), permuted.end());
  const master_screening r = screen_master_frames(master_role::dark, permuted, full_params());
  EXPECT_EQ(r.usable, (std::vector<std::uint64_t>{1, 3, 4}));
  EXPECT_EQ(r.usable, reference.usable);
  ASSERT_EQ(r.excluded.size(), 1u);
  ASSERT_EQ(reference.excluded.size(), 1u);
  EXPECT_EQ(r.excluded[0].input_id, reference.excluded[0].input_id);
  EXPECT_EQ(r.excluded[0].reason, reference.excluded[0].reason);
  EXPECT_EQ(canonical_json(r.excluded[0].values), canonical_json(reference.excluded[0].values));
}

// ---------------------------------------------------------------- T-M3B-C02-12: codici di fase e linearità, parte di analisi

TEST(MasterB12, T_M3B_C02_12_calibrated_phase_is_excluded_as_phase) {
  image i = frame_image(master_role::dark);
  i.phase = metadata_value<process_phase>{process_phase::calibrated, {metadata_origin::header, "ASTRPHAS"}};
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "phase", obj({{"fase", json::string("calibrated")}}));
}

TEST(MasterB12, T_M3B_C02_12_prestacked_phase_is_excluded_as_phase) {
  image i = frame_image(master_role::dark);
  i.phase = metadata_value<process_phase>{process_phase::prestacked, {metadata_origin::header, "ASTRPHAS"}};
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "phase", obj({{"fase", json::string("prestacked")}}));
}

TEST(MasterB12, T_M3B_C02_12_nonlinear_frame_is_excluded_as_linearity) {
  image i = frame_image(master_role::dark);
  i.lin = linearity::nonlinear;
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "linearity", obj({{"linearita", json::string("nonlinear")}}));
}

TEST(MasterB12, T_M3B_C02_12_phase_and_linearity_both_not_admitted_report_the_phase_first) {
  image i = frame_image(master_role::dark);
  i.phase = metadata_value<process_phase>{process_phase::calibrated, {metadata_origin::header, "ASTRPHAS"}};
  i.lin = linearity::nonlinear;
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "phase", obj({{"fase", json::string("calibrated")}}));
}

TEST(MasterB12, T_M3B_C02_12_absent_phase_is_excluded_as_metadata_missing_phase) {
  image i = frame_image(master_role::dark);
  i.phase.reset();
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "metadata_missing.phase", obj({{"campo", json::string("phase")}}));
}

TEST(MasterB12, T_M3B_C02_12_absent_linearity_is_excluded_as_metadata_missing_linearity) {
  image i = frame_image(master_role::dark);
  i.lin.reset();
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "metadata_missing.linearity", obj({{"campo", json::string("linearity")}}));
}

TEST(MasterB12, T_M3B_C02_12_absent_unit_is_excluded_as_metadata_missing_unit) {
  image i = frame_image(master_role::dark);
  i.unit.reset();
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "metadata_missing.unit", obj({{"campo", json::string("unit")}}));
}

TEST(MasterB12, T_M3B_C02_12_unit_other_than_adu_is_excluded_as_unit) {
  image i = frame_image(master_role::dark);
  i.unit = pixel_unit::electron;
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "unit", obj({{"unita", json::string("electron")}}));
}

TEST(MasterB12, T_M3B_C02_12_dimensionless_unit_is_excluded_as_unit) {
  image i = frame_image(master_role::dark);
  i.unit = pixel_unit::dimensionless;
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', i),
                              full_params(), "unit", obj({{"unita", json::string("dimensionless")}}));
}

// ---------------------------------------------------------------- T-M3B-C02-15: master.no_usable_frames

std::map<std::string, int> counted_calls;

fault_injector counter() {
  counted_calls.clear();
  return [](std::string_view point) {
    ++counted_calls[std::string(point)];
    return 0;
  };
}

TEST(MasterB12, T_M3B_C02_15_no_usable_frame_stops_with_the_exclusions_in_the_context_and_no_further_work) {
  image rgb = frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  image calibrated = frame_image(master_role::dark);
  calibrated.phase = metadata_value<process_phase>{process_phase::calibrated, {metadata_origin::header, "ASTRPHAS"}};
  const std::vector<master_analysis_frame> frames = {make_frame(2, 'b', calibrated), make_frame(1, 'a', rgb)};
  const fault_injector fault = counter();
  try {
    (void)screen_master_frames(master_role::dark, frames, full_params(), fault);
    ADD_FAILURE() << "nessun frame utilizzabile: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.no_usable_frames") << e.what();
    EXPECT_EQ(key_set(e), "esclusioni;ruolo;") << e.what();
    EXPECT_EQ(context_of(e, "ruolo"), std::optional<std::string>("dark"));
    json list = json::array();
    list.push(obj({{"digest", json::string(std::string(64, 'a'))}, {"input_id", json::uinteger(1)}, {"motivo", json::string("layout")},
                   {"valori", obj({{"layout", json::string("rgb")}})}}));
    list.push(obj({{"digest", json::string(std::string(64, 'b'))}, {"input_id", json::uinteger(2)}, {"motivo", json::string("phase")},
                   {"valori", obj({{"fase", json::string("calibrated")}})}}));
    EXPECT_EQ(context_of(e, "esclusioni"), std::optional<std::string>(canonical_json(list)));
  }
  EXPECT_EQ(counted_calls["master.analysis.groups"], 0) << "dopo lo stop nessuna formazione di gruppi né proposta";
}

TEST(MasterB12, T_M3B_C02_15_empty_candidate_stops_with_an_empty_exclusion_list) {
  try {
    (void)screen_master_frames(master_role::flat, {}, full_params());
    ADD_FAILURE() << "nessun frame: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.no_usable_frames") << e.what();
    EXPECT_EQ(key_set(e), "esclusioni;ruolo;") << e.what();
    EXPECT_EQ(context_of(e, "ruolo"), std::optional<std::string>("flat"));
    EXPECT_EQ(context_of(e, "esclusioni"), std::optional<std::string>("[]"));
  }
}

TEST(MasterB12, T_M3B_C02_15_counter_control_a_screening_with_a_usable_frame_reaches_the_next_stage_once) {
  const fault_injector fault = counter();
  const master_screening r = screen_master_frames(master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark))}, full_params(), fault);
  EXPECT_EQ(r.usable, std::vector<std::uint64_t>{1});
  EXPECT_EQ(counted_calls["master.analysis.groups"], 1) << "controllo positivo: il contatore è collegato";
}

// ---------------------------------------------------------------- parametri assenti uno alla volta (L4, L9)

const std::set<std::string> kAllNames = {"exposure_rel_tol",    "temperature_abs_tol_c", "temperature_policy", "saturated_fraction_max",
                                         "flat_min_signal_adu", "flat_level_rel_tol",    "decoder_black_abs_tol_adu", "offset_adu"};

std::set<std::string> as_set(const std::vector<std::string>& v) { return {v.begin(), v.end()}; }

master_screening screen_with(master_role role, bool raw, const master_analysis_params& p) {
  std::vector<master_analysis_frame> frames;
  if (raw) {
    frames = {make_raw_frame(1, 'a', raw_frame_image(role), stats_ok(0, {500.0, 500.0, 500.0, 500.0}))};
  } else {
    frames = {make_frame(1, 'a', frame_image(role), declarations(), stats_ok())};
  }
  return screen_master_frames(role, frames, p);
}

void expect_params_missing(const master_screening& r, const std::set<std::string>& names) {
  EXPECT_EQ(as_set(r.missing_params), names);
  EXPECT_EQ(r.missing_params.size(), names.size()) << "ogni nome una volta sola";
  EXPECT_TRUE(r.excluded.empty()) << "params_missing: nessuna esclusione calcolata";
  EXPECT_TRUE(r.usable.empty()) << "params_missing: nessuna proposta";
}

TEST(MasterB12, T_M3B_C02_08_L4_each_required_parameter_absent_alone_gives_params_missing_with_its_name) {
  struct row {
    master_role role;
    bool raw;
    std::set<std::string> required;
  };
  const std::vector<row> rows = {
      {master_role::bias, false, {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy"}},
      {master_role::dark, false, {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy"}},
      {master_role::dark_flat, false, {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy"}},
      {master_role::flat, false,
       {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy", "flat_min_signal_adu", "flat_level_rel_tol"}},
      {master_role::dark, true, {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy", "decoder_black_abs_tol_adu"}},
      {master_role::flat, true,
       {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy", "flat_min_signal_adu", "flat_level_rel_tol",
        "decoder_black_abs_tol_adu", "offset_adu"}},
  };
  for (const row& r : rows) {
    for (const std::string& name : r.required) {
      SCOPED_TRACE(std::string(r.raw ? "raw " : "fits ") + std::to_string(static_cast<int>(r.role)) + " senza " + name);
      master_analysis_params p = full_params();
      if (name == "exposure_rel_tol") p.exposure_rel_tol.reset();
      else if (name == "temperature_abs_tol_c") p.temperature_abs_tol_c.reset();
      else if (name == "temperature_policy") p.temp_policy.reset();
      else if (name == "flat_min_signal_adu") p.flat_min_signal_adu.reset();
      else if (name == "flat_level_rel_tol") p.flat_level_rel_tol.reset();
      else if (name == "decoder_black_abs_tol_adu") p.decoder_black_abs_tol_adu.reset();
      else if (name == "offset_adu") p.offset_adu.reset();
      expect_params_missing(screen_with(r.role, r.raw, p), {name});
    }
  }
}

TEST(MasterB12, T_M3B_C02_08_L4_parameters_not_required_by_the_role_do_not_give_params_missing) {
  master_analysis_params p;  // nessun parametro oltre alle tolleranze comuni
  p.exposure_rel_tol = 0.01;
  p.temperature_abs_tol_c = 1.0;
  p.temp_policy = temperature_policy::compare;
  const master_screening dark = screen_with(master_role::dark, false, p);
  EXPECT_TRUE(dark.missing_params.empty()) << "un dark FITS non vuole flat_*, decoder_black_*, offset_adu";
  EXPECT_EQ(dark.usable, std::vector<std::uint64_t>{1});
  master_analysis_params q = full_params();
  q.offset_adu.reset();
  EXPECT_TRUE(screen_with(master_role::dark, true, q).missing_params.empty()) << "offset_adu serve al flat RAW, non al dark RAW";
  EXPECT_TRUE(screen_with(master_role::flat, false, q).missing_params.empty()) << "per i FITS l'offset è un metadato di frame";
}

TEST(MasterB12, T_M3B_C02_08_L4_temperature_abs_tol_is_not_required_when_the_policy_is_not_applicable_declared) {
  master_analysis_params p = full_params();
  p.temp_policy = temperature_policy::not_applicable_declared;
  p.temperature_abs_tol_c.reset();
  EXPECT_TRUE(screen_with(master_role::dark, false, p).missing_params.empty());
}

TEST(MasterB12, T_M3B_C02_08_L4_saturated_fraction_max_is_required_only_with_saturation_adu) {
  master_analysis_params with = with_saturation(full_params());
  with.saturated_fraction_max.reset();
  expect_params_missing(screen_with(master_role::dark, false, with), {"saturated_fraction_max"});
  master_analysis_params without = full_params();
  without.saturated_fraction_max.reset();
  EXPECT_TRUE(screen_with(master_role::dark, false, without).missing_params.empty());
}

TEST(MasterB12, T_M3B_C02_08_L4_all_parameters_absent_list_every_name_the_role_requires) {
  const master_analysis_params none;
  expect_params_missing(screen_with(master_role::flat, true, none),
                        {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy", "flat_min_signal_adu", "flat_level_rel_tol",
                         "decoder_black_abs_tol_adu", "offset_adu"});
  expect_params_missing(screen_with(master_role::bias, false, none), {"exposure_rel_tol", "temperature_abs_tol_c", "temperature_policy"});
}

TEST(MasterB12, T_M3B_C02_08_L4_params_missing_wins_over_no_usable_frames_and_raises_nothing) {
  master_analysis_params p = full_params();
  p.exposure_rel_tol.reset();
  image rgb = frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  master_screening r;
  ASSERT_NO_THROW(r = screen_master_frames(master_role::dark, {make_frame(1, 'a', rgb)}, p));
  expect_params_missing(r, {"exposure_rel_tol"});
}

TEST(MasterB12, T_M3B_C02_08_L4_frame_saturation_threshold_with_saturation_adu_absent_stops_the_analysis) {
  image with_threshold = frame_image(master_role::dark);
  with_threshold.metadata.saturation_adu = hd(4095.0, "SATURATE");
  try {
    (void)screen_master_frames(master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark)), make_frame(7, 'c', with_threshold)},
                               full_params());
    ADD_FAILURE() << "soglia del frame con saturation_adu assente: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.param.saturation_adu") << e.what();
    EXPECT_EQ(key_set(e), "frame;valore;") << e.what();
    EXPECT_EQ(context_of(e, "frame"), std::optional<std::string>(std::string(64, 'c')));
    EXPECT_EQ(context_of(e, "valore"), std::optional<std::string>("4095"));
  }
}

TEST(MasterB12, T_M3B_C02_08_L4_raw_decoder_saturation_threshold_with_saturation_adu_absent_stops_the_analysis) {
  image raw = raw_frame_image(master_role::dark);
  raw.metadata.saturation_adu = metadata_value<double>{16383.0, {metadata_origin::decoder, "color.maximum"}};
  try {
    (void)screen_master_frames(master_role::dark, {make_raw_frame(4, 'd', raw)}, full_params());
    ADD_FAILURE() << "soglia del decoder con saturation_adu assente: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.param.saturation_adu") << e.what();
    EXPECT_EQ(key_set(e), "frame;valore;") << e.what();
    EXPECT_EQ(context_of(e, "frame"), std::optional<std::string>(std::string(64, 'd')));
    EXPECT_EQ(context_of(e, "valore"), std::optional<std::string>("16383"));
  }
}

TEST(MasterB12, T_M3B_C02_08_L4_saturation_adu_absent_without_frame_thresholds_is_not_a_stop_and_not_params_missing) {
  const master_screening r = screen_with(master_role::dark, false, full_params());
  EXPECT_TRUE(r.missing_params.empty());
  EXPECT_EQ(r.usable, std::vector<std::uint64_t>{1});
}

// ---------------------------------------------------------------- L4: lettura dei parametri da `batch.analysis_params`

fs::path fresh(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "b12" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

json params_doc(const json& master) {
  json doc = json::object();
  doc.set("format", json::string("astro.batch_analysis_params.v1"));
  doc.set("exposure_rel_tol", json::number(0.01));
  doc.set("temperature_abs_tol_c", json::number(1.0));
  doc.set("temperature_policy", json::string("compare"));
  doc.set("decoder_black_abs_tol_adu", json::uinteger(10));
  doc.set("master", master);
  return doc;
}

master_analysis_params read_params(const std::string& name, const json& doc) {
  auto store = sqlite_dag_store::create(fresh(name) / "project");
  store->put_setting("batch.analysis_params", canonical_json(doc));
  return read_master_analysis_params(*store);
}

TEST(MasterB12, T_M3B_C02_08_L4_master_member_of_batch_analysis_params_carries_the_master_parameters_and_the_common_tolerances) {
  const json master = obj({{"saturation_adu", json::number(4095.0)},
                           {"saturated_fraction_max", json::number(0.25)},
                           {"flat_min_signal_adu", json::number(100.0)},
                           {"flat_level_rel_tol", json::number(0.125)},
                           {"offset_adu", json::number(10.0)}});
  const master_analysis_params p = read_params("params_full", params_doc(master));
  EXPECT_EQ(p.exposure_rel_tol, std::optional<double>(0.01));
  EXPECT_EQ(p.temperature_abs_tol_c, std::optional<double>(1.0));
  EXPECT_EQ(p.temp_policy, std::optional<temperature_policy>(temperature_policy::compare));
  EXPECT_EQ(p.decoder_black_abs_tol_adu, std::optional<std::uint32_t>(10));
  EXPECT_EQ(p.saturation_adu, std::optional<double>(4095.0));
  EXPECT_EQ(p.saturated_fraction_max, std::optional<double>(0.25));
  EXPECT_EQ(p.flat_min_signal_adu, std::optional<double>(100.0));
  EXPECT_EQ(p.flat_level_rel_tol, std::optional<double>(0.125));
  EXPECT_EQ(p.offset_adu, std::optional<double>(10.0));
}

TEST(MasterB12, T_M3B_C02_08_L4_missing_master_member_leaves_the_master_parameters_absent_without_defaults) {
  json doc = json::object();
  doc.set("format", json::string("astro.batch_analysis_params.v1"));
  doc.set("exposure_rel_tol", json::number(0.01));
  const master_analysis_params p = read_params("params_no_master", doc);
  EXPECT_EQ(p.exposure_rel_tol, std::optional<double>(0.01));
  EXPECT_FALSE(p.temperature_abs_tol_c.has_value());
  EXPECT_FALSE(p.saturation_adu.has_value());
  EXPECT_FALSE(p.saturated_fraction_max.has_value());
  EXPECT_FALSE(p.flat_min_signal_adu.has_value());
  EXPECT_FALSE(p.flat_level_rel_tol.has_value());
  EXPECT_FALSE(p.offset_adu.has_value());
}

TEST(MasterB12, T_M3B_C02_08_L4_unknown_key_inside_the_master_member_stops_with_batch_analysis_params) {
  auto store = sqlite_dag_store::create(fresh("params_unknown") / "project");
  store->put_setting("batch.analysis_params", canonical_json(params_doc(obj({{"sigma_lungo", json::number(3.0)}}))));
  try {
    (void)read_master_analysis_params(*store);
    ADD_FAILURE() << "chiave sconosciuta in `master`: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "batch.analysis_params") << e.what();
  }
}

TEST(MasterB12, T_M3B_C02_08_L4_master_only_key_at_the_top_level_stops_with_batch_analysis_params) {
  json doc = params_doc(json::object());
  doc.set("saturation_adu", json::number(4095.0));
  auto store = sqlite_dag_store::create(fresh("params_toplevel") / "project");
  store->put_setting("batch.analysis_params", canonical_json(doc));
  EXPECT_STOP((void)read_master_analysis_params(*store), error_category::invalid_input, "batch.analysis_params");
}

TEST(MasterB12, T_M3B_C02_08_L4_common_tolerance_inside_the_master_member_stops_with_batch_analysis_params) {
  auto store = sqlite_dag_store::create(fresh("params_duplicate") / "project");
  store->put_setting("batch.analysis_params", canonical_json(params_doc(obj({{"exposure_rel_tol", json::number(0.02)}}))));
  EXPECT_STOP((void)read_master_analysis_params(*store), error_category::invalid_input, "batch.analysis_params");
}

TEST(MasterB12, T_M3B_C02_08_L4_master_member_that_is_not_an_object_stops_with_batch_analysis_params) {
  auto store = sqlite_dag_store::create(fresh("params_not_object") / "project");
  store->put_setting("batch.analysis_params", canonical_json(params_doc(json::string("x"))));
  EXPECT_STOP((void)read_master_analysis_params(*store), error_category::invalid_input, "batch.analysis_params");
}

TEST(MasterB12, T_M3B_C02_08_L4_master_parameter_with_a_non_numeric_value_stops_with_batch_analysis_params) {
  auto store = sqlite_dag_store::create(fresh("params_text") / "project");
  store->put_setting("batch.analysis_params", canonical_json(params_doc(obj({{"offset_adu", json::string("10")}}))));
  EXPECT_STOP((void)read_master_analysis_params(*store), error_category::invalid_input, "batch.analysis_params");
}

// ---------------------------------------------------------------- L2: lettore FITS di statistiche per frame

cancel_token& never() {
  static cancel_token token;
  return token;
}

struct written {
  std::string path;
  content_digest digest;
};

// Frame FITS f64 scritto su disco: valori, `OFFSET` 10, `SATURATE` assente (la soglia viene dall'analisi).
written write_frame(const std::string& name, std::uint32_t w, std::uint32_t h, const std::vector<double>& values,
                    const std::string& bayer = "", bool with_offset_card = true) {
  fx::calib_header hdr;
  hdr.imagetyp = "Flat Field";
  hdr.exposure = 5;
  hdr.bayer = bayer;
  std::vector<std::string> cards;
  for (const std::string& c : fx::calib_cards(hdr)) {
    if (with_offset_card || c.rfind("OFFSET", 0) != 0) cards.push_back(c);
  }
  const fx::bytes file = fx::float_frame(w, h, -64, values, cards);
  const fs::path path = fresh(name) / "frame.fits";
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  out.close();
  return {path.string(), digest_of(file)};
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_counts_saturated_samples_and_pixels_of_a_verified_fits) {
  const written f = write_frame("reader_sat", 4, 2, {20, 30, 40, 50, 60, 70, 100, 200});
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.saturation_adu = 100.0;
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, req, pool, never());
  EXPECT_FALSE(s.decode_failure.has_value());
  EXPECT_EQ(s.saturated_samples, 2u) << "100 e 200 sono >= 100";
  EXPECT_EQ(s.pixel_count, 8u);
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_flat_level_of_a_mono_frame_is_the_median_of_the_samples_minus_the_header_offset) {
  const written f = write_frame("reader_level", 4, 2, {20, 30, 40, 50, 60, 70, 100, 200});
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, req, pool, never());
  ASSERT_EQ(s.levels.size(), 1u);
  ASSERT_TRUE(s.levels[0].has_value());
  EXPECT_EQ(*s.levels[0], 45.0) << "mediana di {10,20,30,40,50,60,90,190} = (40 + 50) / 2";
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_flat_levels_of_a_cfa_frame_are_per_site_of_the_motif) {
  // 4 x 4 RGGB: siti (0,0)=100 (0,1)=200 (1,0)=300 (1,1)=400 ripetuti; con OFFSET 10 i livelli sono 90, 190, 290, 390.
  std::vector<double> v;
  for (std::uint32_t y = 0; y < 4; ++y)
    for (std::uint32_t x = 0; x < 4; ++x) v.push_back(100.0 * (1 + (x % 2) + 2 * (y % 2)));
  const written f = write_frame("reader_cfa", 4, 4, v, "RGGB");
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, req, pool, never());
  ASSERT_EQ(s.levels.size(), 4u);
  const double expected[4] = {90.0, 190.0, 290.0, 390.0};
  for (std::size_t c = 0; c < 4; ++c) {
    ASSERT_TRUE(s.levels[c].has_value()) << c;
    EXPECT_EQ(*s.levels[c], expected[c]) << c;
  }
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_channel_without_valid_pixels_has_an_absent_level) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::vector<double> v;
  for (std::uint32_t y = 0; y < 4; ++y)
    for (std::uint32_t x = 0; x < 4; ++x) v.push_back(x % 2 == 1 && y % 2 == 1 ? nan : 100.0);
  const written f = write_frame("reader_nan", 4, 4, v, "RGGB");
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, req, pool, never());
  ASSERT_EQ(s.levels.size(), 4u);
  EXPECT_TRUE(s.levels[0].has_value());
  EXPECT_TRUE(s.levels[1].has_value());
  EXPECT_TRUE(s.levels[2].has_value());
  EXPECT_FALSE(s.levels[3].has_value()) << "tutti i campioni del sito (1,1) non sono finiti";
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_without_a_request_for_levels_returns_no_levels) {
  const written f = write_frame("reader_nolevels", 4, 2, {1, 2, 3, 4, 5, 6, 7, 8});
  memory_pool pool(64 * 1024 * 1024);
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, master_stats_request{}, pool, never());
  EXPECT_TRUE(s.levels.empty());
  EXPECT_EQ(s.pixel_count, 8u);
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_truncated_fits_with_matching_digest_is_a_decode_failure_not_a_stop) {
  fx::calib_header hdr;
  hdr.imagetyp = "Flat Field";
  fx::bytes file = fx::float_frame(4, 2, -64, {1, 2, 3, 4, 5, 6, 7, 8}, fx::calib_cards(hdr));
  file.pop_back();  // il digest atteso è quello del file così com'è: la verifica passa, la decodifica no
  const fs::path path = fresh("reader_truncated") / "frame.fits";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.saturation_adu = 5.0;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  const master_frame_stats s = read_master_frame_stats(path.string(), digest_of(file), req, pool, never());
  ASSERT_TRUE(s.decode_failure.has_value());
  EXPECT_EQ(s.decode_failure->code, "fits.malformed");
  EXPECT_FALSE(s.decode_failure->detail.empty());
}

// Frame a 16 bit (BZERO 32768, valori interi) con piano INVALID: `bits[i]` è la maschera del pixel i (ADR-03).
written write_masked_frame(const std::string& name, std::uint32_t w, std::uint32_t h, const std::vector<std::uint16_t>& values,
                           const std::vector<std::uint16_t>& bits) {
  fx::calib_header hdr;
  hdr.imagetyp = "Flat Field";
  hdr.exposure = 5;
  fx::raw_hdu p;
  p.bitpix = 16;
  p.axes = {w, h};
  p.cards = {fx::card_real("BSCALE", 1.0), fx::card_real("BZERO", 32768.0)};
  for (const std::string& c : fx::calib_cards(hdr)) p.cards.push_back(c);
  for (const std::uint16_t v : values) fx::put_be(p.data, static_cast<std::uint16_t>(static_cast<std::int32_t>(v) - 32768), 2);
  fx::raw_hdu e;
  e.primary = false;
  e.bitpix = 16;
  e.axes = {w, h};
  e.cards = {fx::card_str("EXTNAME", "INVALID"), fx::card_real("BSCALE", 1.0), fx::card_real("BZERO", 32768.0)};
  for (const std::uint16_t b : bits) fx::put_be(e.data, static_cast<std::uint16_t>(b ^ 0x8000u), 2);
  const fx::bytes file = fx::build_fits({p, e});
  const fs::path path = fresh(name) / "frame.fits";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }
  return {path.string(), digest_of(file)};
}

const std::vector<std::uint16_t> kMaskedValues = {20, 30, 40, 50, 60, 70, 100, 200};
const std::vector<std::uint16_t> kLastTwoFlagged = {0, 0, 0, 0, 0, 0, invalid_reason::source_flagged, invalid_reason::source_flagged};

TEST(MasterB12, T_M3B_C02_08_L2_reader_decode_failure_of_a_fits_that_passes_plan_and_digest_carries_the_decoder_code) {
  std::vector<std::uint16_t> bits(8, 0);
  bits[3] = 1u << 10;  // bit riservato: la scansione non legge la maschera, la decodifica sì
  const written f = write_masked_frame("reader_reserved_bit", 4, 2, kMaskedValues, bits);
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.saturation_adu = 100.0;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  master_frame_stats s;
  ASSERT_NO_THROW(s = read_master_frame_stats(f.path, f.digest, req, pool, never()));
  ASSERT_TRUE(s.decode_failure.has_value());
  EXPECT_EQ(s.decode_failure->code, "fits.invalid_reserved_bits");
  EXPECT_FALSE(s.decode_failure->detail.empty());
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_invalid_pixels_with_saturated_values_do_not_count_as_saturated) {
  const written f = write_masked_frame("reader_mask_sat", 4, 2, kMaskedValues, kLastTwoFlagged);
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.saturation_adu = 100.0;
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, req, pool, never());
  EXPECT_FALSE(s.decode_failure.has_value());
  EXPECT_EQ(s.saturated_samples, 0u) << "100 e 200 sono invalidi: fuori dal conteggio";
  EXPECT_EQ(s.pixel_count, 8u) << "il totale comprende i pixel invalidi";
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_invalid_pixels_are_excluded_from_the_flat_level_median) {
  const written f = write_masked_frame("reader_mask_level", 4, 2, kMaskedValues, kLastTwoFlagged);
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, req, pool, never());
  ASSERT_EQ(s.levels.size(), 1u);
  ASSERT_TRUE(s.levels[0].has_value());
  EXPECT_EQ(*s.levels[0], 35.0) << "mediana di {10,20,30,40,50,60} = (30 + 40) / 2; con i due invalidi sarebbe 45";
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_altered_file_stops_with_input_digest_mismatch_and_returns_no_statistics) {
  const written f = write_frame("reader_mismatch", 4, 2, {1, 2, 3, 4, 5, 6, 7, 8});
  {  // un byte di dati (primo campione, dopo l'header di 2880 byte) alterato dopo il calcolo del digest
    std::fstream io(f.path, std::ios::in | std::ios::out | std::ios::binary);
    char b = 0;
    io.seekg(2880, std::ios::beg);
    io.read(&b, 1);
    b = static_cast<char>(b ^ 0x01);
    io.seekp(2880, std::ios::beg);
    io.write(&b, 1);
  }
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.saturation_adu = 5.0;
  req.flat_levels = true;
  req.offset_adu = 10.0;
  try {
    (void)read_master_frame_stats(f.path, f.digest, req, pool, never());
    ADD_FAILURE() << "file alterato: stop atteso, non un decode_failed";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::integrity) << e.what();
    EXPECT_EQ(e.code(), "input.digest_mismatch") << e.what();
    EXPECT_EQ(key_set(e), "atteso;letto;percorso;") << e.what();
    EXPECT_EQ(context_of(e, "percorso"), std::optional<std::string>(f.path));
    EXPECT_EQ(context_of(e, "atteso"), std::optional<std::string>(f.digest.sha256 + ":" + std::to_string(f.digest.size_bytes)));
  }
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_missing_file_stops_with_input_missing) {
  const fs::path dir = fresh("reader_missing");
  const std::string path = (dir / "assente.fits").string();
  memory_pool pool(64 * 1024 * 1024);
  try {
    (void)read_master_frame_stats(path, digest_with('a'), master_stats_request{}, pool, never());
    ADD_FAILURE() << "file assente: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io) << e.what();
    EXPECT_EQ(e.code(), "input.missing") << e.what();
    EXPECT_EQ(key_set(e), "errno;percorso;") << e.what();
    EXPECT_EQ(context_of(e, "percorso"), std::optional<std::string>(path));
    EXPECT_EQ(context_of(e, "errno"), std::optional<std::string>("2"));
  }
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_cancel_requested_before_the_read_stops_with_sched_cancelled) {
  const written f = write_frame("reader_cancel", 4, 2, {1, 2, 3, 4, 5, 6, 7, 8});
  memory_pool pool(64 * 1024 * 1024);
  cancel_token cancel;
  cancel.request();
  try {
    (void)read_master_frame_stats(f.path, f.digest, master_stats_request{}, pool, cancel);
    ADD_FAILURE() << "annullamento richiesto: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::cancelled) << e.what();
    EXPECT_EQ(e.code(), "sched.cancelled") << e.what();
    EXPECT_TRUE(e.context().empty()) << e.what();
  }
}

// ---------------------------------------------------------------- offset della richiesta, livelli non finiti, domini, declared_conflict, lettore

TEST(MasterB12, T_M3B_C02_08_L2_reader_flat_level_uses_the_offset_of_the_request_for_a_file_without_offset_card) {
  const written f = write_frame("reader_declared_offset", 4, 2, {20, 30, 40, 50, 60, 70, 100, 200}, "", false);
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.flat_levels = true;
  req.offset_adu = 20.0;  // dichiarato: l'header non ha OFFSET
  const master_frame_stats s = read_master_frame_stats(f.path, f.digest, req, pool, never());
  ASSERT_EQ(s.levels.size(), 1u);
  ASSERT_TRUE(s.levels[0].has_value());
  EXPECT_EQ(*s.levels[0], 35.0) << "mediana di {0,10,20,30,40,50,80,180} = (30 + 40) / 2";
}

TEST(MasterB12, T_M3B_C02_08_flat_non_finite_level_is_excluded_as_low_signal_with_a_null_level) {
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  for (const double bad : {inf, -inf, nan}) {
    SCOPED_TRACE(bad);
    expect_only_second_excluded(
        master_role::flat, make_frame(1, 'a', frame_image(master_role::flat), declarations(), stats_ok(0, {500.0})),
        make_frame(2, 'b', frame_image(master_role::flat), declarations(), stats_ok(0, {bad})), full_params(), "flat_low_signal",
        obj({{"livello", json::null()}, {"minimo", json::number(100.0)}, {"motivo", json::string("non_finito")}}));
  }
}

TEST(MasterB12, T_M3B_C02_08_flat_cfa_non_finite_level_names_the_channel) {
  image cfa_flat = frame_image(master_role::flat, plane_layout::cfa);
  expect_only_second_excluded(
      master_role::flat, make_frame(1, 'a', cfa_flat, declarations(), stats_ok(0, {500.0, 500.0, 500.0, 500.0})),
      make_frame(2, 'b', cfa_flat, declarations(), stats_ok(0, {500.0, 500.0, std::numeric_limits<double>::infinity(), 500.0})),
      full_params(), "flat_low_signal",
      obj({{"canale", json::string("G2")}, {"livello", json::null()}, {"minimo", json::number(100.0)}, {"motivo", json::string("non_finito")}}));
}

// Domini dei parametri d'analisi: gli stessi della ricetta del master (master.cpp), contesto {parametro, motivo}.
void expect_param_stop(master_role role, bool raw, const master_analysis_params& p, const std::string& name, const std::string& why) {
  try {
    (void)screen_with(role, raw, p);
    ADD_FAILURE() << name << " fuori dominio: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.param." + name) << e.what();
    EXPECT_EQ(key_set(e), "motivo;parametro;") << e.what();
    EXPECT_EQ(context_of(e, "parametro"), std::optional<std::string>(name));
    EXPECT_EQ(context_of(e, "motivo"), std::optional<std::string>(why));
  }
}

TEST(MasterB12, T_M3B_C02_08_L4_saturated_fraction_max_below_zero_stops_instead_of_excluding_every_frame) {
  expect_param_stop(master_role::dark, false, with_saturation(full_params(), 4095.0, -0.1), "saturated_fraction_max", "deve stare in [0, 1]");
}

TEST(MasterB12, T_M3B_C02_08_L4_saturated_fraction_max_above_one_stops) {
  expect_param_stop(master_role::dark, false, with_saturation(full_params(), 4095.0, 1.5), "saturated_fraction_max", "deve stare in [0, 1]");
}

TEST(MasterB12, T_M3B_C02_08_L4_non_finite_saturation_adu_stops) {
  expect_param_stop(master_role::dark, false, with_saturation(full_params(), std::numeric_limits<double>::quiet_NaN()), "saturation_adu",
                    "non finito");
}

TEST(MasterB12, T_M3B_C02_08_L4_flat_min_signal_adu_not_above_zero_stops) {
  master_analysis_params p = full_params();
  p.flat_min_signal_adu = 0.0;
  expect_param_stop(master_role::flat, false, p, "flat_min_signal_adu", "deve essere > 0");
}

TEST(MasterB12, T_M3B_C02_08_L4_flat_level_rel_tol_outside_the_open_unit_interval_stops) {
  for (const double bad : {0.0, 1.0}) {
    SCOPED_TRACE(bad);
    master_analysis_params p = full_params();
    p.flat_level_rel_tol = bad;
    expect_param_stop(master_role::flat, false, p, "flat_level_rel_tol", "deve stare in (0, 1)");
  }
}

TEST(MasterB12, T_M3B_C02_08_L4_negative_exposure_rel_tol_stops) {
  master_analysis_params p = full_params();
  p.exposure_rel_tol = -0.1;
  expect_param_stop(master_role::dark, false, p, "exposure_rel_tol", "deve essere >= 0");
}

TEST(MasterB12, T_M3B_C02_08_L4_negative_temperature_abs_tol_stops) {
  master_analysis_params p = full_params();
  p.temperature_abs_tol_c = -1.0;
  expect_param_stop(master_role::dark, false, p, "temperature_abs_tol_c", "deve essere >= 0");
}

TEST(MasterB12, T_M3B_C02_08_L4_non_finite_offset_adu_stops_for_a_raw_flat) {
  master_analysis_params p = full_params();
  p.offset_adu = std::numeric_limits<double>::infinity();
  expect_param_stop(master_role::flat, true, p, "offset_adu", "non finito");
}

// declared_conflict: un valore dichiarato diverso da quello letto, primo motivo della lista chiusa.
TEST(MasterB12, T_M3B_C02_08_fits_declared_offset_different_from_the_header_is_a_declared_conflict) {
  const std::string conflict = declarations({}, {{"offset_adu", json::number(20.0)}});  // header OFFSET 10
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)),
                              make_frame(2, 'b', frame_image(master_role::dark), conflict), full_params(), "declared_conflict",
                              obj({{"campo", json::string("offset_adu")}}));
}

TEST(MasterB12, T_M3B_C02_08_raw_declared_field_in_conflict_is_a_declared_conflict) {
  master_analysis_frame bad = make_raw_frame(2, 'b', raw_frame_image(master_role::dark));
  bad.observation = make_observation(raw_frame_image(master_role::dark),
                                     declarations({{"binning_x", json::uinteger(1)}, {"binning_y", json::uinteger(1)}},
                                                  {{"filter", json::string("L")}, {"gain", json::number(200.0)}}),
                                     identity_a());  // header/decoder: gain 100
  expect_only_second_excluded(master_role::dark, make_raw_frame(1, 'a', raw_frame_image(master_role::dark)), bad, full_params(),
                              "declared_conflict", obj({{"campo", json::string("gain")}}));
}

TEST(MasterB12, T_M3B_C02_08_declared_conflict_precedes_every_other_reason) {
  image bad = frame_image(master_role::dark);
  bad.layout = plane_layout::rgb;
  bad.metadata.pedestal_adu = hd(5.0, "PEDESTAL");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)),
                              make_frame(2, 'b', bad, declarations({}, {{"offset_adu", json::number(20.0)}})), full_params(),
                              "declared_conflict", obj({{"campo", json::string("offset_adu")}}));
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_truncated_file_with_the_digest_of_the_original_stops_with_digest_mismatch) {
  fx::calib_header hdr;
  hdr.imagetyp = "Flat Field";
  fx::bytes file = fx::float_frame(4, 2, -64, {1, 2, 3, 4, 5, 6, 7, 8}, fx::calib_cards(hdr));
  const content_digest original = digest_of(file);
  file.pop_back();
  const fs::path path = fresh("reader_truncated_original") / "frame.fits";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }
  memory_pool pool(64 * 1024 * 1024);
  master_stats_request req;
  req.saturation_adu = 5.0;
  EXPECT_STOP((void)read_master_frame_stats(path.string(), original, req, pool, never()), error_category::integrity,
              "input.digest_mismatch");
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_pool_budget_between_file_size_and_decode_peak_stops_instead_of_a_decode_failure) {
  std::vector<double> v(64 * 64, 1.0);
  const written f = write_frame("reader_budget", 64, 64, v);
  memory_pool pool(f.digest.size_bytes);  // la lettura entra, il picco della decodifica no
  EXPECT_STOP((void)read_master_frame_stats(f.path, f.digest, master_stats_request{}, pool, never()), error_category::budget,
              "sched.job_exceeds_budget");
}

TEST(MasterB12, T_M3B_C02_08_L2_reader_wrong_datasum_with_a_matching_digest_is_a_decode_failure_fits_checksum) {
  fx::raw_hdu h;
  h.bitpix = 16;
  h.axes = {4, 2};
  h.cards = {fx::card_real("BSCALE", 1.0), fx::card_real("BZERO", 32768.0)};
  for (int i = 0; i < 8; ++i) fx::put_be(h.data, static_cast<std::uint16_t>(i * 1000 + 1), 2);
  h.cards.push_back(fx::card_str("DATASUM", std::to_string(fx::datasum(h.data) + 1)));
  const fx::bytes file = fx::build_fits({h});
  const fs::path path = fresh("reader_checksum") / "frame.fits";
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }
  memory_pool pool(64 * 1024 * 1024);
  master_frame_stats s;
  ASSERT_NO_THROW(s = read_master_frame_stats(path.string(), digest_of(file), master_stats_request{}, pool, never()));
  ASSERT_TRUE(s.decode_failure.has_value());
  EXPECT_EQ(s.decode_failure->code, "fits.checksum");
}

TEST(MasterB12, T_M3B_C02_15_injected_fault_with_an_errno_stops_with_io_master_analysis_fault) {
  const fault_injector eio = [](std::string_view) { return EIO; };
  try {
    (void)screen_master_frames(master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark))}, full_params(), eio);
    ADD_FAILURE() << "guasto iniettato: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io) << e.what();
    EXPECT_EQ(e.code(), "master.analysis.fault") << e.what();
    EXPECT_EQ(key_set(e), "errno;point;") << e.what();
    EXPECT_EQ(context_of(e, "point"), std::optional<std::string>("master.analysis.groups"));
    EXPECT_EQ(context_of(e, "errno"), std::optional<std::string>(std::to_string(EIO)));
  }
}

TEST(MasterB12, T_M3B_C02_08_L4_saturation_threshold_stop_applies_to_a_frame_already_excluded_for_another_reason) {
  image bad = frame_image(master_role::dark);
  bad.layout = plane_layout::rgb;
  bad.metadata.saturation_adu = hd(4095.0, "SATURATE");
  EXPECT_STOP((void)screen_master_frames(master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', bad)},
                                         full_params()),
              error_category::invalid_input, "master.param.saturation_adu");
}

TEST(MasterB12, T_M3B_C02_08_L4_saturation_threshold_stop_precedes_params_missing) {
  master_analysis_params p = full_params();
  p.exposure_rel_tol.reset();
  image with_threshold = frame_image(master_role::dark);
  with_threshold.metadata.saturation_adu = hd(4095.0, "SATURATE");
  EXPECT_STOP((void)screen_master_frames(master_role::dark, {make_frame(1, 'a', with_threshold)}, p), error_category::invalid_input,
              "master.param.saturation_adu");
}

TEST(MasterB12, T_M3B_C02_08_order_metadata_missing_field_precedes_imagetyp_conflict) {
  image bad = frame_image(master_role::dark);
  bad.metadata.camera.reset();
  bad.metadata.imagetyp = hs("Light Frame", "IMAGETYP");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', bad),
                              full_params(), "metadata_missing.camera", obj({{"campo", json::string("camera")}}));
}

TEST(MasterB12, T_M3B_C02_08_order_duplicate_digest_precedes_decode_failed) {
  master_frame_stats failed;
  failed.decode_failure = master_decode_failure{"fits.malformed", "x"};
  const master_screening r =
      screen_master_frames(master_role::dark,
                           {make_frame(1, 'a', frame_image(master_role::dark), declarations(), stats_ok()),
                            make_frame(2, 'a', frame_image(master_role::dark), declarations(), failed)},
                           with_saturation(full_params()));
  ASSERT_EQ(r.excluded.size(), 1u);
  EXPECT_EQ(r.excluded[0].input_id, 2u);
  EXPECT_EQ(r.excluded[0].reason, "duplicate_digest");
}

TEST(MasterB12, T_M3B_C02_08_order_decode_failed_precedes_saturation_conflict) {
  master_frame_stats failed;
  failed.decode_failure = master_decode_failure{"fits.malformed", "x"};
  image bad = frame_image(master_role::dark);
  bad.metadata.saturation_adu = hd(4000.0, "SATURATE");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark), declarations(), stats_ok()),
                              make_frame(2, 'b', bad, declarations(), failed), with_saturation(full_params()), "decode_failed",
                              obj({{"codice", json::string("fits.malformed")}, {"dettaglio", json::string("x")}}));
}

TEST(MasterB12, T_M3B_C02_08_order_saturation_conflict_precedes_saturated_fraction) {
  image bad = frame_image(master_role::dark);
  bad.metadata.saturation_adu = hd(4000.0, "SATURATE");
  expect_only_second_excluded(master_role::dark, make_frame(1, 'a', frame_image(master_role::dark), declarations(), stats_ok()),
                              make_frame(2, 'b', bad, declarations(), stats_ok(16)), with_saturation(full_params()),
                              "saturation_conflict", obj({{"analisi", json::number(4095.0)}, {"soglia_frame", json::number(4000.0)}}));
}

TEST(MasterB12, T_M3B_C02_08_order_saturated_fraction_precedes_flat_low_signal) {
  expect_only_second_excluded(
      master_role::flat, make_frame(1, 'a', frame_image(master_role::flat), declarations(), stats_ok(0, {500.0})),
      make_frame(2, 'b', frame_image(master_role::flat), declarations(), stats_ok(16, {1.0})), with_saturation(full_params()),
      "saturated_fraction",
      obj({{"frazione", json::number(1.0)}, {"massimo", json::number(0.25)}, {"saturi", json::uinteger(16)}, {"totale", json::uinteger(16)}}));
}

}  // namespace
