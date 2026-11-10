// M3.b, unità B13: analisi dei master, gruppi e livelli del flat. C-02.b §b.2 («Analisi del set candidato», punti 2-3:
// chiave esatta, gruppi per tolleranza col riferimento a digest minimo fra i frame restanti, stato `flat_level` per
// gruppo, un nodo per gruppo); ADR-16.b §b.7 (campi strutturali RAW, nero effettivo `b(r, c)` in interi a 64 bit).
// Test: T-M3B-C02-08, parte gruppi (un campo della chiave alla volta, tolleranze entro e oltre, RAW e FITS insieme,
// permutazioni dei candidati, gruppi disgiunti, riferimento, stato `flat_level` con `m_(f,c)`), e lo stato `flat_level`
// nell'analisi di T-M3B-C02-14.
// Fuori da B13: proposta di §b.4 e preflight dei nodi proposti (B14), lettore RAW (B15).
//
// API sotto prova: `astro/master_analysis.hpp`. I gruppi li forma `screen_master_frames` sui frame utilizzabili, dopo
// le esclusioni e dopo il punto `master.analysis.groups` del fault_injector, e li restituisce in
// `master_screening::groups`, nell'ordine canonico del riferimento. Ogni gruppo porta `reference` (input_id), `members`
// (input_id in ordine canonico, il primo è il riferimento), `status` (`master_group_status::ok` o `flat_level`) e, solo
// per il flat, `flat_levels` (m_(f,c) per membro nell'ordine di `members`, canali nell'ordine dei siti).
// Livelli del flat: m̄_c è la media dei livelli dei membri in ordine canonico; un membro viola la tolleranza se
// |m_(f,c) / m̄_c − 1| > `flat_level_rel_tol` (la regola del preflight di C-02.a §2, così che lo stato coincida con
// l'esito del preflight sull'intero gruppo). Esposizione: |x − y| ≤ `exposure_rel_tol` · max(|x|, |y|), la regola a
// coppie del preflight (C-02.a §2, «stessa regola di ADR-16»).
//
// Tutti gli esiti attesi sono scritti a mano dai paragrafi citati, con valori dyadic: nessun arrotondamento nelle
// tolleranze al confine. Nessun valore atteso viene dal prodotto.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/error.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/master_analysis.hpp"

namespace {

using namespace astro;

constexpr std::uint32_t kW = 4;
constexpr std::uint32_t kH = 4;
constexpr std::uint32_t kU32Max = std::numeric_limits<std::uint32_t>::max();

// ---------------------------------------------------------------- immagini, osservazioni, ingressi

metadata_value<double> hd(double v, const char* key) { return {v, {metadata_origin::header, key}}; }
metadata_value<std::string> hs(const std::string& v, const char* key) { return {v, {metadata_origin::header, key}}; }
metadata_value<std::int64_t> hi(std::int64_t v, const char* key) { return {v, {metadata_origin::header, key}}; }

content_digest digest_with(char c) { return content_digest{std::string(64, c), 2880}; }

// Frame FITS nella regola di C-02.a §2: fase `raw`, `linear`, `adu`, binning 1 x 1, tutti i campi di compatibilità;
// nessun campo opzionale (`roi_x`, `roi_y`, `readout_mode`, `row_order`).
image frame_image(master_role role, plane_layout layout = plane_layout::mono, std::uint32_t w = kW, std::uint32_t h = kH,
                  cfa_layout cfa_value = cfa_layout{cfa_pattern::rggb, 0, 0}) {
  image_geometry g(w, h, 1);
  g.binning_x = 1;
  g.binning_y = 1;
  std::optional<cfa_layout> cfa;
  if (layout == plane_layout::cfa) cfa = cfa_value;
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

// Stato del decoder valido per un frame rggb 4 x 4: nero effettivo 512 su ogni sito del periodo 2 x 2.
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

std::array<std::uint8_t, 32> curve(std::uint8_t fill) {
  std::array<std::uint8_t, 32> c{};
  c.fill(fill);
  return c;
}

// Frame RAW: il decoder non produce binning, filtro né `offset_adu` per frame (ADR-08.b §2.6, L9): binning e filtro
// vengono dalle dichiarazioni.
image raw_frame_image(master_role role, const decoder_state& state = base_state()) {
  image im = frame_image(role, plane_layout::cfa);
  im.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::decoder, "decoder"}};
  im.metadata.offset_adu.reset();
  im.geometry.binning_x.reset();
  im.geometry.binning_y.reset();
  im.metadata.filter.reset();
  im.decoder_state = state;
  return im;
}

std::string raw_decl() {
  return declarations({{"binning_x", json::uinteger(1)}, {"binning_y", json::uinteger(1)}}, {{"filter", json::string("L")}});
}

master_frame_stats stats_with(std::vector<std::optional<double>> levels) {
  master_frame_stats s;
  s.pixel_count = kW * kH;
  s.levels = std::move(levels);
  return s;
}

master_analysis_frame make_frame(std::uint64_t id, char tag, const image& img, std::optional<master_frame_stats> stats = std::nullopt) {
  master_analysis_frame f;
  f.input_id = id;
  f.digest = digest_with(tag);
  f.observation = make_observation(img, declarations());
  f.stats = std::move(stats);
  return f;
}

master_analysis_frame make_raw_frame(std::uint64_t id, char tag, const image& img, std::optional<master_frame_stats> stats = std::nullopt) {
  master_analysis_frame f;
  f.input_id = id;
  f.digest = digest_with(tag);
  f.observation = make_observation(img, raw_decl(), identity_a());
  f.stats = std::move(stats);
  return f;
}

// Statistiche di un flat: un livello per un mono, quattro per un CFA, tutti 500 (sopra `flat_min_signal_adu`).
std::optional<master_frame_stats> flat_stats_for(master_role role, const image& img) {
  if (role != master_role::flat) return std::nullopt;
  return stats_with(img.cfa ? std::vector<std::optional<double>>{500.0, 500.0, 500.0, 500.0}
                            : std::vector<std::optional<double>>{500.0});
}

// Parametri completi per ogni ruolo e per i RAW; saturazione non dichiarata.
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

// ---------------------------------------------------------------- oracle dei gruppi

struct group_oracle {
  std::uint64_t reference = 0;
  std::vector<std::uint64_t> members;
  master_group_status status = master_group_status::ok;
  std::vector<std::vector<double>> flat_levels = {};
};

const char* status_name(master_group_status s) { return s == master_group_status::ok ? "ok" : "flat_level"; }

// Confronto esatto, gruppo per gruppo e nell'ordine dei gruppi: membri, riferimento, stato e livelli.
void expect_groups(const master_screening& r, const std::vector<group_oracle>& want) {
  ASSERT_EQ(r.groups.size(), want.size());
  for (std::size_t i = 0; i < want.size(); ++i) {
    SCOPED_TRACE("gruppo " + std::to_string(i));
    EXPECT_EQ(r.groups[i].reference, want[i].reference);
    EXPECT_EQ(r.groups[i].members, want[i].members);
    EXPECT_STREQ(status_name(r.groups[i].status), status_name(want[i].status));
    EXPECT_EQ(r.groups[i].flat_levels, want[i].flat_levels);
  }
}

// Livelli attesi di `flat_stats_for`, per i membri dati.
std::vector<std::vector<double>> flat_levels_for(master_role role, const image& img, std::size_t members) {
  if (role != master_role::flat) return {};
  return std::vector<std::vector<double>>(members, img.cfa ? std::vector<double>{500.0, 500.0, 500.0, 500.0}
                                                           : std::vector<double>{500.0});
}

// Due frame FITS (id 1 digest 'a', id 2 digest 'b'), entrambi utilizzabili.
master_screening screen_pair(master_role role, const image& a, const image& b, const master_analysis_params& p = full_params()) {
  const master_screening r =
      screen_master_frames(role, {make_frame(1, 'a', a, flat_stats_for(role, a)), make_frame(2, 'b', b, flat_stats_for(role, b))}, p);
  EXPECT_TRUE(r.missing_params.empty());
  EXPECT_TRUE(r.excluded.empty());
  EXPECT_EQ(r.usable, (std::vector<std::uint64_t>{1, 2}));
  return r;
}

void expect_one_group(master_role role, const image& a, const image& b, const master_analysis_params& p = full_params()) {
  expect_groups(screen_pair(role, a, b, p), {{1, {1, 2}, master_group_status::ok, flat_levels_for(role, a, 2)}});
}

void expect_two_groups(master_role role, const image& a, const image& b, const master_analysis_params& p = full_params()) {
  expect_groups(screen_pair(role, a, b, p), {{1, {1}, master_group_status::ok, flat_levels_for(role, a, 1)},
                                             {2, {2}, master_group_status::ok, flat_levels_for(role, b, 1)}});
}

master_screening screen_raw_pair(const decoder_state& a, const decoder_state& b, const master_analysis_params& p = full_params()) {
  const master_screening r = screen_master_frames(
      master_role::dark,
      {make_raw_frame(1, 'a', raw_frame_image(master_role::dark, a)), make_raw_frame(2, 'b', raw_frame_image(master_role::dark, b))}, p);
  EXPECT_TRUE(r.missing_params.empty());
  EXPECT_TRUE(r.excluded.empty());
  EXPECT_EQ(r.usable, (std::vector<std::uint64_t>{1, 2}));
  return r;
}

const std::vector<group_oracle> kOneRawGroup = {{1, {1, 2}}};
const std::vector<group_oracle> kTwoRawGroups = {{1, {1}}, {2, {2}}};

// ---------------------------------------------------------------- controllo positivo

TEST(MasterB13, T_M3B_C02_08_identical_frames_of_every_role_form_one_group_with_the_minimum_digest_as_reference) {
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    SCOPED_TRACE(static_cast<int>(role));
    expect_one_group(role, frame_image(role), frame_image(role));
  }
}

TEST(MasterB13, T_M3B_C02_08_identical_raw_frames_form_one_group) {
  expect_groups(screen_raw_pair(base_state(), base_state()), kOneRawGroup);
}

// ---------------------------------------------------------------- T-M3B-C02-08: chiave esatta, un campo alla volta (FITS)

struct key_case {
  const char* name;
  master_role role;
  std::function<image()> make_a;
  std::function<image()> make_b;
};

std::vector<key_case> fits_key_cases() {
  const auto dark = [] { return frame_image(master_role::dark); };
  const auto cfa_dark = [] { return frame_image(master_role::dark, plane_layout::cfa); };
  const auto changed = [](std::function<void(image&)> change) {
    return [change] {
      image im = frame_image(master_role::dark);
      change(im);
      return im;
    };
  };
  return {
      {"width", master_role::dark, dark, [] { return frame_image(master_role::dark, plane_layout::mono, kW + 2, kH); }},
      {"height", master_role::dark, dark, [] { return frame_image(master_role::dark, plane_layout::mono, kW, kH + 2); }},
      {"binning_x", master_role::dark, dark, changed([](image& i) { i.geometry.binning_x = 2; })},
      {"binning_y", master_role::dark, dark, changed([](image& i) { i.geometry.binning_y = 2; })},
      {"cfa_presence", master_role::dark, dark, cfa_dark},
      {"cfa_pattern", master_role::dark, cfa_dark,
       [] { return frame_image(master_role::dark, plane_layout::cfa, kW, kH, cfa_layout{cfa_pattern::bggr, 0, 0}); }},
      {"cfa_x_offset", master_role::dark, cfa_dark,
       [] { return frame_image(master_role::dark, plane_layout::cfa, kW, kH, cfa_layout{cfa_pattern::rggb, 1, 0}); }},
      {"cfa_y_offset", master_role::dark, cfa_dark,
       [] { return frame_image(master_role::dark, plane_layout::cfa, kW, kH, cfa_layout{cfa_pattern::rggb, 0, 1}); }},
      {"camera", master_role::dark, dark, changed([](image& i) { i.metadata.camera = hs("cam-b", "INSTRUME"); })},
      {"gain", master_role::dark, dark, changed([](image& i) { i.metadata.gain = hd(200.0, "GAIN"); })},
      {"offset_adu", master_role::dark, dark, changed([](image& i) { i.metadata.offset_adu = hd(12.0, "OFFSET"); })},
      {"filter_flat", master_role::flat, [] { return frame_image(master_role::flat); },
       [] {
         image im = frame_image(master_role::flat);
         im.metadata.filter = hs("R", "FILTER");
         return im;
       }},
      {"roi_x_values", master_role::dark, changed([](image& i) { i.metadata.roi_x = hi(0, "ROI_X"); }),
       changed([](image& i) { i.metadata.roi_x = hi(16, "ROI_X"); })},
      {"roi_y_values", master_role::dark, changed([](image& i) { i.metadata.roi_y = hi(0, "ROI_Y"); }),
       changed([](image& i) { i.metadata.roi_y = hi(16, "ROI_Y"); })},
      {"readout_mode_values", master_role::dark, changed([](image& i) { i.metadata.readout_mode = hs("fast", "READOUTM"); }),
       changed([](image& i) { i.metadata.readout_mode = hs("slow", "READOUTM"); })},
      {"row_order_values", master_role::dark, changed([](image& i) { i.metadata.row_order = hs("TOP-DOWN", "ROWORDER"); }),
       changed([](image& i) { i.metadata.row_order = hs("BOTTOM-UP", "ROWORDER"); })},
  };
}

TEST(MasterB13, T_M3B_C02_08_frames_differing_in_one_key_field_form_two_groups) {
  for (const key_case& c : fits_key_cases()) {
    SCOPED_TRACE(c.name);
    expect_two_groups(c.role, c.make_a(), c.make_b());
  }
}

// «Assente» è un valore della chiave: un campo opzionale presente in un solo frame separa i gruppi (la chiave
// dell'analisi è più stretta del preflight).
TEST(MasterB13, T_M3B_C02_08_optional_field_absent_in_one_frame_and_present_in_the_other_forms_two_groups) {
  const std::vector<std::pair<const char*, std::function<void(image&)>>> fields = {
      {"roi_x", [](image& i) { i.metadata.roi_x = hi(0, "ROI_X"); }},
      {"roi_y", [](image& i) { i.metadata.roi_y = hi(0, "ROI_Y"); }},
      {"readout_mode", [](image& i) { i.metadata.readout_mode = hs("fast", "READOUTM"); }},
      {"row_order", [](image& i) { i.metadata.row_order = hs("TOP-DOWN", "ROWORDER"); }},
  };
  for (const auto& [name, set] : fields) {
    SCOPED_TRACE(name);
    image present = frame_image(master_role::dark);
    set(present);
    expect_two_groups(master_role::dark, frame_image(master_role::dark), present);
  }
}

TEST(MasterB13, T_M3B_C02_08_optional_fields_present_with_equal_values_keep_one_group) {
  image a = frame_image(master_role::dark);
  a.metadata.roi_x = hi(16, "ROI_X");
  a.metadata.roi_y = hi(8, "ROI_Y");
  a.metadata.readout_mode = hs("fast", "READOUTM");
  a.metadata.row_order = hs("TOP-DOWN", "ROWORDER");
  expect_one_group(master_role::dark, a, a);
}

TEST(MasterB13, T_M3B_C02_08_filter_is_not_in_the_key_of_the_roles_other_than_flat) {
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    SCOPED_TRACE(static_cast<int>(role));
    image b = frame_image(role);
    b.metadata.filter = hs("R", "FILTER");
    expect_one_group(role, frame_image(role), b);
  }
}

// ---------------------------------------------------------------- T-M3B-C02-08: tolleranze (esposizione, temperatura)

TEST(MasterB13, T_M3B_C02_08_exposure_within_exposure_rel_tol_keeps_one_group) {
  image b = frame_image(master_role::dark);
  b.metadata.exposure_s = hd(60.5, "EXPTIME");  // |0.5| <= 0.01 · 60.5
  expect_one_group(master_role::dark, frame_image(master_role::dark), b);
}

TEST(MasterB13, T_M3B_C02_08_exposure_beyond_exposure_rel_tol_forms_two_groups) {
  image b = frame_image(master_role::dark);
  b.metadata.exposure_s = hd(61.0, "EXPTIME");  // |1| > 0.01 · 61
  expect_two_groups(master_role::dark, frame_image(master_role::dark), b);
}

// Confine: tolleranza 1/4, |60 − 80| = 20 = 1/4 · max(60, 80), esatto in f64. Uguale ammesso.
TEST(MasterB13, T_M3B_C02_08_exposure_at_the_tolerance_of_the_larger_value_keeps_one_group) {
  master_analysis_params p = full_params();
  p.exposure_rel_tol = 0.25;
  image b = frame_image(master_role::dark);
  b.metadata.exposure_s = hd(80.0, "EXPTIME");
  expect_one_group(master_role::dark, frame_image(master_role::dark), b, p);
}

TEST(MasterB13, T_M3B_C02_08_exposure_just_beyond_the_tolerance_of_the_larger_value_forms_two_groups) {
  master_analysis_params p = full_params();
  p.exposure_rel_tol = 0.25;
  image b = frame_image(master_role::dark);
  b.metadata.exposure_s = hd(80.5, "EXPTIME");  // 20.5 > 0.25 · 80.5 = 20.125
  expect_two_groups(master_role::dark, frame_image(master_role::dark), b, p);
}

TEST(MasterB13, T_M3B_C02_08_temperature_at_temperature_abs_tol_keeps_one_group_with_policy_compare) {
  image b = frame_image(master_role::dark);
  b.metadata.temperature_c = hd(-9.0, "CCD-TEMP");  // |1| = 1, uguale ammesso
  expect_one_group(master_role::dark, frame_image(master_role::dark), b);
}

TEST(MasterB13, T_M3B_C02_08_temperature_beyond_temperature_abs_tol_forms_two_groups_with_policy_compare) {
  image b = frame_image(master_role::dark);
  b.metadata.temperature_c = hd(-8.75, "CCD-TEMP");  // |1.25| > 1
  expect_two_groups(master_role::dark, frame_image(master_role::dark), b);
}

TEST(MasterB13, T_M3B_C02_08_temperature_is_not_compared_with_policy_not_applicable_declared) {
  master_analysis_params p = full_params();
  p.temp_policy = temperature_policy::not_applicable_declared;
  image b = frame_image(master_role::dark);
  b.metadata.temperature_c = hd(20.0, "CCD-TEMP");
  expect_one_group(master_role::dark, frame_image(master_role::dark), b, p);
}

// ---------------------------------------------------------------- T-M3B-C02-08: RAW, campi strutturali e nero effettivo

TEST(MasterB13, T_M3B_C02_08_raw_frames_differing_in_one_structural_field_form_two_groups) {
  const std::vector<std::pair<const char*, std::function<void(decoder_state&)>>> fields = {
      {"raw_width", [](decoder_state& s) { s.raw_width = 9; }},
      {"raw_height", [](decoder_state& s) { s.raw_height = 9; }},
      {"window", [](decoder_state& s) { s.window = decoder_window{0, 0, kW, kH}; }},
      {"cfa_color_index", [](decoder_state& s) { s.cfa_color_index = {0, 3, 1, 2}; }},
      {"black_pattern_rows",
       [](decoder_state& s) {
         s.black_pattern_rows = 1;
         s.black_pattern = {0, 0};
       }},
      {"black_pattern_cols",
       [](decoder_state& s) {
         s.black_pattern_cols = 1;
         s.black_pattern = {0, 0};
       }},
      // `linearization = decoder_curve` richiede `curve_sha256` (invariante di ADR-03.b §b.2): i due campi cambiano insieme.
      {"linearization",
       [](decoder_state& s) {
         s.linearization = decoder_linearization::decoder_curve;
         s.curve_sha256 = curve(0x11);
       }},
  };
  for (const auto& [name, change] : fields) {
    SCOPED_TRACE(name);
    decoder_state b = base_state();
    change(b);
    expect_groups(screen_raw_pair(base_state(), b), kTwoRawGroups);
  }
}

TEST(MasterB13, T_M3B_C02_08_raw_frames_differing_only_in_curve_sha256_form_two_groups) {
  decoder_state a = base_state();
  a.linearization = decoder_linearization::decoder_curve;
  a.curve_sha256 = curve(0x11);
  decoder_state b = a;
  b.curve_sha256 = curve(0x22);
  expect_groups(screen_raw_pair(a, b), kTwoRawGroups);
}

TEST(MasterB13, T_M3B_C02_08_raw_effective_black_at_the_tolerance_keeps_one_group) {
  decoder_state b = base_state();
  b.black = 522;  // |512 − 522| = 10 = decoder_black_abs_tol_adu
  expect_groups(screen_raw_pair(base_state(), b), kOneRawGroup);
}

TEST(MasterB13, T_M3B_C02_08_raw_effective_black_at_the_tolerance_plus_one_forms_two_groups) {
  decoder_state b = base_state();
  b.black = 523;
  expect_groups(screen_raw_pair(base_state(), b), kTwoRawGroups);
}

// Il nero effettivo è per sito: un solo sito del periodo oltre la tolleranza separa (indice colore 2 = sito (1, 1)).
TEST(MasterB13, T_M3B_C02_08_raw_black_per_color_beyond_the_tolerance_on_one_site_forms_two_groups) {
  decoder_state b = base_state();
  b.black_per_color = {0, 0, 11, 0};
  expect_groups(screen_raw_pair(base_state(), b), kTwoRawGroups);
}

TEST(MasterB13, T_M3B_C02_08_raw_black_per_color_at_the_tolerance_on_one_site_keeps_one_group) {
  decoder_state b = base_state();
  b.black_per_color = {0, 0, 10, 0};
  expect_groups(screen_raw_pair(base_state(), b), kOneRawGroup);
}

TEST(MasterB13, T_M3B_C02_08_raw_black_pattern_beyond_the_tolerance_on_one_site_forms_two_groups) {
  decoder_state b = base_state();
  b.black_pattern = {0, 11, 0, 0};
  expect_groups(screen_raw_pair(base_state(), b), kTwoRawGroups);
}

// b = black + black_per_color + P in interi a 64 bit: 2^32 contro 0. In 32 bit la somma si annulla e i frame
// sembrerebbero uguali.
TEST(MasterB13, T_M3B_C02_08_raw_effective_black_is_summed_in_64_bit_integers_without_wraparound) {
  decoder_state a = base_state();
  a.black = kU32Max;
  a.black_per_color = {1, 1, 1, 1};
  decoder_state b = base_state();
  b.black = 0;
  expect_groups(screen_raw_pair(a, b), kTwoRawGroups);
}

// Somme oltre 2^32 (3 · (2^32 − 1) contro la stessa meno 10): differenza uguale alla tolleranza, un gruppo.
TEST(MasterB13, T_M3B_C02_08_raw_effective_black_above_two_to_the_32_at_the_tolerance_keeps_one_group) {
  decoder_state a = base_state();
  a.black = kU32Max;
  a.black_per_color = {kU32Max, kU32Max, kU32Max, kU32Max};
  a.black_pattern = {kU32Max, kU32Max, kU32Max, kU32Max};
  decoder_state b = a;
  b.black = kU32Max - 10;
  expect_groups(screen_raw_pair(a, b), kOneRawGroup);
}

// ---------------------------------------------------------------- T-M3B-C02-08: RAW e FITS insieme

// Un FITS CFA e un RAW con la stessa geometria, CFA, camera, gain, esposizione e temperatura: la presenza dello stato
// del decoder li separa sempre.
TEST(MasterB13, T_M3B_C02_08_raw_and_fits_frames_never_share_a_group) {
  const master_screening r = screen_master_frames(
      master_role::dark,
      {make_frame(1, 'a', frame_image(master_role::dark, plane_layout::cfa)), make_raw_frame(2, 'b', raw_frame_image(master_role::dark))},
      full_params());
  EXPECT_TRUE(r.excluded.empty());
  EXPECT_EQ(r.usable, (std::vector<std::uint64_t>{1, 2}));
  expect_groups(r, {{1, {1}}, {2, {2}}});
}

TEST(MasterB13, T_M3B_C02_08_raw_and_fits_frames_mixed_group_each_kind_separately) {
  const master_screening r = screen_master_frames(
      master_role::dark,
      {make_raw_frame(4, 'd', raw_frame_image(master_role::dark)), make_frame(1, 'a', frame_image(master_role::dark, plane_layout::cfa)),
       make_raw_frame(2, 'b', raw_frame_image(master_role::dark)), make_frame(3, 'c', frame_image(master_role::dark, plane_layout::cfa))},
      full_params());
  EXPECT_TRUE(r.excluded.empty());
  expect_groups(r, {{1, {1, 3}}, {2, {2, 4}}});
}

// ---------------------------------------------------------------- T-M3B-C02-08: riferimento, esclusioni, disgiunzione

// Riferimento = frame restante a digest minimo, non il primo input_id né l'esposizione centrale. Digest 'a' (61 s, id 3),
// 'b' (60.5 s, id 2), 'c' (60 s, id 1), tolleranza 0.01: 'b' è entro 'a' (0.5 <= 0.61), 'c' no (1 > 0.61).
TEST(MasterB13, T_M3B_C02_08_reference_is_the_remaining_frame_with_the_minimum_digest) {
  image ea = frame_image(master_role::dark);
  ea.metadata.exposure_s = hd(61.0, "EXPTIME");
  image eb = frame_image(master_role::dark);
  eb.metadata.exposure_s = hd(60.5, "EXPTIME");
  image ec = frame_image(master_role::dark);
  const master_screening r =
      screen_master_frames(master_role::dark, {make_frame(1, 'c', ec), make_frame(2, 'b', eb), make_frame(3, 'a', ea)}, full_params());
  expect_groups(r, {{3, {3, 2}}, {1, {1}}});
}

// I gruppi si formano sui soli frame utilizzabili: il frame escluso a digest minimo (61 s) non fa da riferimento, e
// 'b' (60.5 s) e 'c' (60 s) stanno insieme (0.5 <= 0.605).
TEST(MasterB13, T_M3B_C02_08_excluded_frames_are_in_no_group_and_never_act_as_reference) {
  image excluded = frame_image(master_role::dark);
  excluded.layout = plane_layout::rgb;
  excluded.metadata.exposure_s = hd(61.0, "EXPTIME");
  image eb = frame_image(master_role::dark);
  eb.metadata.exposure_s = hd(60.5, "EXPTIME");
  const master_screening r = screen_master_frames(
      master_role::dark, {make_frame(1, 'a', excluded), make_frame(2, 'b', eb), make_frame(3, 'c', frame_image(master_role::dark))},
      full_params());
  ASSERT_EQ(r.excluded.size(), 1u);
  EXPECT_EQ(r.excluded[0].input_id, 1u);
  expect_groups(r, {{2, {2, 3}}});
}

TEST(MasterB13, T_M3B_C02_08_params_missing_gives_no_groups) {
  master_analysis_params p = full_params();
  p.exposure_rel_tol.reset();
  const master_screening r = screen_master_frames(
      master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark)), make_frame(2, 'b', frame_image(master_role::dark))}, p);
  EXPECT_EQ(r.missing_params, std::vector<std::string>{"exposure_rel_tol"});
  EXPECT_TRUE(r.groups.empty());
}

// Candidati della prova di permutazione: sei frame, input_id non nell'ordine dei digest.
//   'a' id 6, 60 s           -> riferimento del gruppo 1
//   'b' id 2, 60.5 s         -> con 'a' (0.5 <= 0.605)
//   'c' id 5, 61 s           -> fuori da 'a' (1 > 0.61): riferimento del gruppo 2, anche se entro 'b' (0.5 <= 0.61)
//   'd' id 1, gain 200       -> chiave diversa: gruppo 3
//   'e' id 4, 61.5 s         -> con 'c' (0.5 <= 0.615), fuori da 'a'
//   'f' id 3, layout rgb     -> escluso
std::vector<master_analysis_frame> permutation_candidates() {
  const auto exposure = [](double s) {
    image im = frame_image(master_role::dark);
    im.metadata.exposure_s = hd(s, "EXPTIME");
    return im;
  };
  image gain = frame_image(master_role::dark);
  gain.metadata.gain = hd(200.0, "GAIN");
  image rgb = frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  return {make_frame(6, 'a', exposure(60.0)), make_frame(2, 'b', exposure(60.5)), make_frame(5, 'c', exposure(61.0)),
          make_frame(1, 'd', gain),           make_frame(4, 'e', exposure(61.5)), make_frame(3, 'f', rgb)};
}

const std::vector<group_oracle> kPermutationGroups = {{6, {6, 2}}, {5, {5, 4}}, {1, {1}}};

TEST(MasterB13, T_M3B_C02_08_groups_of_a_mixed_candidate_match_the_hand_written_oracle) {
  const master_screening r = screen_master_frames(master_role::dark, permutation_candidates(), full_params());
  ASSERT_EQ(r.excluded.size(), 1u);
  EXPECT_EQ(r.excluded[0].input_id, 3u);
  expect_groups(r, kPermutationGroups);
}

TEST(MasterB13, T_M3B_C02_08_every_permutation_of_the_candidates_gives_the_same_groups) {
  const std::vector<master_analysis_frame> base = permutation_candidates();
  std::vector<std::size_t> idx(base.size());
  std::iota(idx.begin(), idx.end(), 0);
  int permutations = 0;
  do {
    std::vector<master_analysis_frame> frames;
    for (const std::size_t i : idx) frames.push_back(base[i]);
    SCOPED_TRACE("permutazione " + std::to_string(permutations));
    expect_groups(screen_master_frames(master_role::dark, frames, full_params()), kPermutationGroups);
    ++permutations;
  } while (std::next_permutation(idx.begin(), idx.end()));
  EXPECT_EQ(permutations, 720);
}

// Un nodo per gruppo (C-02.b §b.2): i gruppi sono disgiunti e coprono esattamente i frame utilizzabili.
TEST(MasterB13, T_M3B_C02_08_groups_are_disjoint_and_cover_exactly_the_usable_frames) {
  const master_screening r = screen_master_frames(master_role::dark, permutation_candidates(), full_params());
  std::multiset<std::uint64_t> in_groups;
  for (const master_group& g : r.groups) in_groups.insert(g.members.begin(), g.members.end());
  const std::multiset<std::uint64_t> usable(r.usable.begin(), r.usable.end());
  EXPECT_EQ(in_groups, usable);
  EXPECT_EQ(in_groups.count(3), 0u);
}

// ---------------------------------------------------------------- T-M3B-C02-08 e C02-14: stato `flat_level` per gruppo

master_screening screen_flat_pair(std::vector<std::optional<double>> la, std::vector<std::optional<double>> lb,
                                  plane_layout layout = plane_layout::mono) {
  const master_screening r = screen_master_frames(master_role::flat,
                                                  {make_frame(1, 'a', frame_image(master_role::flat, layout), stats_with(std::move(la))),
                                                   make_frame(2, 'b', frame_image(master_role::flat, layout), stats_with(std::move(lb)))},
                                                  full_params());
  EXPECT_TRUE(r.excluded.empty());
  EXPECT_EQ(r.usable, (std::vector<std::uint64_t>{1, 2}));
  return r;
}

// m̄ = 500; 400/500 − 1 = −0.2, 600/500 − 1 = 0.2, entro 0.25.
TEST(MasterB13, T_M3B_C02_08_flat_group_within_flat_level_rel_tol_is_ok_and_reports_the_levels) {
  expect_groups(screen_flat_pair({400.0}, {600.0}), {{1, {1, 2}, master_group_status::ok, {{400.0}, {600.0}}}});
}

// m̄ = 600; 400/600 − 1 = −1/3, oltre 0.25.
TEST(MasterB13, T_M3B_C02_08_flat_group_beyond_flat_level_rel_tol_has_status_flat_level_with_the_levels) {
  expect_groups(screen_flat_pair({400.0}, {800.0}), {{1, {1, 2}, master_group_status::flat_level, {{400.0}, {800.0}}}});
}

// Confine: m̄ = 400; 300/400 − 1 = −0.25 e 500/400 − 1 = 0.25, esatti in f64. Uguale ammesso, come nel preflight.
TEST(MasterB13, T_M3B_C02_08_flat_levels_at_flat_level_rel_tol_keep_status_ok) {
  expect_groups(screen_flat_pair({300.0}, {500.0}), {{1, {1, 2}, master_group_status::ok, {{300.0}, {500.0}}}});
}

// Nessun frame è escluso per livello: il gruppo `flat_level` tiene entrambi i membri e i frame restano utilizzabili.
TEST(MasterB13, T_M3B_C02_08_flat_level_status_excludes_no_frame) {
  const master_screening r = screen_flat_pair({400.0}, {800.0});
  EXPECT_TRUE(r.excluded.empty());
  ASSERT_EQ(r.groups.size(), 1u);
  EXPECT_EQ(r.groups[0].members, (std::vector<std::uint64_t>{1, 2}));
}

// T-M3B-C02-14, parte analisi: CFA con il solo canale B fuori tolleranza (m̄_B = 750, 500/750 − 1 = −1/3), gli altri
// canali uguali; il livello medio su tutti i canali (500 contro 625) resterebbe entro 0.25.
TEST(MasterB13, T_M3B_C02_14_flat_cfa_group_with_one_channel_beyond_the_tolerance_has_status_flat_level) {
  expect_groups(screen_flat_pair({500.0, 500.0, 500.0, 500.0}, {500.0, 500.0, 500.0, 1000.0}, plane_layout::cfa),
                {{1, {1, 2}, master_group_status::flat_level, {{500.0, 500.0, 500.0, 500.0}, {500.0, 500.0, 500.0, 1000.0}}}});
}

// Lo stato è per gruppo: due filtri, il gruppo L fuori tolleranza, il gruppo R entro.
TEST(MasterB13, T_M3B_C02_08_flat_level_status_is_per_group) {
  image red = frame_image(master_role::flat);
  red.metadata.filter = hs("R", "FILTER");
  const master_screening r = screen_master_frames(
      master_role::flat,
      {make_frame(1, 'a', frame_image(master_role::flat), stats_with({400.0})), make_frame(2, 'b', red, stats_with({500.0})),
       make_frame(3, 'c', frame_image(master_role::flat), stats_with({800.0})), make_frame(4, 'd', red, stats_with({520.0}))},
      full_params());
  EXPECT_TRUE(r.excluded.empty());
  expect_groups(r, {{1, {1, 3}, master_group_status::flat_level, {{400.0}, {800.0}}},
                    {2, {2, 4}, master_group_status::ok, {{500.0}, {520.0}}}});
}

TEST(MasterB13, T_M3B_C02_08_flat_group_of_one_frame_is_ok) {
  const master_screening r =
      screen_master_frames(master_role::flat, {make_frame(1, 'a', frame_image(master_role::flat), stats_with({400.0}))}, full_params());
  expect_groups(r, {{1, {1}, master_group_status::ok, {{400.0}}}});
}

// Gli altri ruoli non hanno livelli del flat: stato ok e nessun livello, anche con statistiche che li portano.
TEST(MasterB13, T_M3B_C02_08_roles_other_than_flat_have_status_ok_and_no_levels) {
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    SCOPED_TRACE(static_cast<int>(role));
    const master_screening r = screen_master_frames(
        role, {make_frame(1, 'a', frame_image(role), stats_with({400.0})), make_frame(2, 'b', frame_image(role), stats_with({800.0}))},
        full_params());
    expect_groups(r, {{1, {1, 2}, master_group_status::ok, {}}});
  }
}


// ---------------------------------------------------------------- stop della formazione dei gruppi

// Contesto dello stop come mappa ordinata: insieme esatto delle chiavi e valori.
std::map<std::string, std::string> context_map(const stop_error& e) {
  std::map<std::string, std::string> m;
  for (const auto& [k, v] : e.context()) m[k] = v;
  return m;
}

// Due livelli uguali a DBL_MAX, finiti e sopra `flat_min_signal_adu`: la somma per m̄_c va a +inf. Stop, nessun gruppo.
TEST(MasterB13, T_M3B_C02_08_flat_group_with_a_non_finite_level_mean_stops_with_report_nonfinite) {
  const double big = std::numeric_limits<double>::max();
  try {
    (void)screen_master_frames(master_role::flat,
                               {make_frame(1, 'a', frame_image(master_role::flat), stats_with({big})),
                                make_frame(2, 'b', frame_image(master_role::flat), stats_with({big}))},
                               full_params());
    FAIL() << "atteso master.report_nonfinite";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::numeric);
    EXPECT_EQ(e.code(), "master.report_nonfinite");
    EXPECT_EQ(context_map(e), (std::map<std::string, std::string>{{"riferimento", "1"}}));  // mono: nessun `canale`
  }
}

// CFA: `canale` è l'etichetta del canale, come `flat_context` del preflight (B = sito (1, 1) di rggb).
TEST(MasterB13, T_M3B_C02_08_flat_cfa_group_with_a_non_finite_level_mean_names_the_channel_label) {
  const double big = std::numeric_limits<double>::max();
  try {
    (void)screen_master_frames(master_role::flat,
                               {make_frame(1, 'a', frame_image(master_role::flat, plane_layout::cfa), stats_with({500.0, 500.0, 500.0, big})),
                                make_frame(2, 'b', frame_image(master_role::flat, plane_layout::cfa), stats_with({500.0, 500.0, 500.0, big}))},
                               full_params());
    FAIL() << "atteso master.report_nonfinite";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::numeric);
    EXPECT_EQ(e.code(), "master.report_nonfinite");
    EXPECT_EQ(context_map(e), (std::map<std::string, std::string>{{"canale", "B"}, {"riferimento", "1"}}));
  }
}

// Motivo del nero 2 x 2 con tre valori: il nero effettivo `b(r, c)` non è calcolabile (ADR-16.b §b.7). Il frame
// supera le esclusioni e lo stop viene dalla formazione dei gruppi.
TEST(MasterB13, T_M3B_C02_08_raw_frame_with_an_incoherent_black_pattern_stops_with_calib_decoder_state) {
  decoder_state bad = base_state();
  bad.black_pattern = {0, 0, 0};
  try {
    (void)screen_master_frames(master_role::dark, {make_raw_frame(1, 'a', raw_frame_image(master_role::dark, bad))}, full_params());
    FAIL() << "atteso calib.decoder_state";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input);
    EXPECT_EQ(e.code(), "calib.decoder_state");
    EXPECT_EQ(context_map(e), (std::map<std::string, std::string>{{"check", "black_pattern"},
                                                                  {"pattern_cols", "2"},
                                                                  {"pattern_rows", "2"},
                                                                  {"pattern_size", "3"},
                                                                  {"window", "4x4"}}));
  }
}

// ---------------------------------------------------------------- correzioni della review di B13 (giro 1)

// RAW con `offset_adu` dichiarato (L9: il valore non è nella chiave dei RAW).
master_analysis_frame make_raw_frame_with_offset(std::uint64_t id, char tag, double offset, const image& img = raw_frame_image(master_role::dark)) {
  master_analysis_frame f;
  f.input_id = id;
  f.digest = digest_with(tag);
  f.observation = make_observation(
      img, declarations({{"binning_x", json::uinteger(1)}, {"binning_y", json::uinteger(1)}}, {{"filter", json::string("L")}, {"offset_adu", json::number(offset)}}),
      identity_a());
  return f;
}

// Riferimento RAW (digest minimo) e candidato FITS: la presenza dello stato del decoder separa anche in questo verso.
TEST(MasterB13, T_M3B_C02_08_raw_reference_with_a_fits_candidate_forms_two_groups) {
  const master_screening r = screen_master_frames(
      master_role::dark,
      {make_frame(2, 'b', frame_image(master_role::dark, plane_layout::cfa)), make_raw_frame(1, 'a', raw_frame_image(master_role::dark))},
      full_params());
  EXPECT_TRUE(r.excluded.empty());
  expect_groups(r, {{1, {1}}, {2, {2}}});
}

// RAW con `offset_adu` dichiarato uguale a quello del FITS (10): ogni altro campo della chiave coincide, li separa solo
// la presenza dello stato del decoder. Entrambi i versi del riferimento; la prova certa del controllo di presenza è il
// verso col riferimento FITS: senza quel controllo il verso RAW leggerebbe stati del decoder assenti (comportamento
// indefinito), non un esito deterministico.
TEST(MasterB13, T_M3B_C02_08_raw_with_the_same_declared_offset_as_the_fits_is_separated_only_by_the_decoder_state) {
  {
    SCOPED_TRACE("riferimento FITS");
    const master_screening r = screen_master_frames(
        master_role::dark, {make_frame(1, 'a', frame_image(master_role::dark, plane_layout::cfa)), make_raw_frame_with_offset(2, 'b', 10.0)},
        full_params());
    EXPECT_TRUE(r.excluded.empty());
    expect_groups(r, {{1, {1}}, {2, {2}}});
  }
  {
    SCOPED_TRACE("riferimento RAW");
    const master_screening r = screen_master_frames(
        master_role::dark, {make_raw_frame_with_offset(1, 'a', 10.0), make_frame(2, 'b', frame_image(master_role::dark, plane_layout::cfa))},
        full_params());
    EXPECT_TRUE(r.excluded.empty());
    expect_groups(r, {{1, {1}}, {2, {2}}});
  }
}

TEST(MasterB13, T_M3B_C02_08_raw_frames_with_different_declared_offsets_share_one_group) {
  const master_screening r =
      screen_master_frames(master_role::dark, {make_raw_frame_with_offset(1, 'a', 10.0), make_raw_frame_with_offset(2, 'b', 20.0)}, full_params());
  EXPECT_TRUE(r.excluded.empty());
  expect_groups(r, {{1, {1, 2}}});
}

// Il punto `master.analysis.groups` precede la formazione dei gruppi: il guasto iniettato vince sullo stop del nero.
TEST(MasterB13, T_M3B_C02_08_injected_fault_at_the_groups_point_precedes_the_decoder_state_stop) {
  decoder_state bad = base_state();
  bad.black_pattern = {0, 0, 0};
  const fault_injector fault = [](std::string_view point) { return point == "master.analysis.groups" ? EIO : 0; };
  try {
    (void)screen_master_frames(master_role::dark, {make_raw_frame(1, 'a', raw_frame_image(master_role::dark, bad))}, full_params(), fault);
    FAIL() << "atteso master.analysis.fault";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io);
    EXPECT_EQ(e.code(), "master.analysis.fault");
    EXPECT_EQ(context_map(e), (std::map<std::string, std::string>{{"errno", std::to_string(EIO)}, {"point", "master.analysis.groups"}}));
  }
}

// Flat: gruppo L con livelli diversi per membro e `flat_level` (m̄ = 2000/3, 400/m̄ − 1 = −0.4), gruppo R entro (m̄ = 510).
// input_id non nell'ordine dei digest; `flat_levels` nell'ordine canonico dei membri.
std::vector<master_analysis_frame> flat_permutation_candidates() {
  image red = frame_image(master_role::flat);
  red.metadata.filter = hs("R", "FILTER");
  const image lum = frame_image(master_role::flat);
  return {make_frame(4, 'a', lum, stats_with({400.0})), make_frame(2, 'b', lum, stats_with({600.0})),
          make_frame(3, 'c', red, stats_with({500.0})), make_frame(1, 'd', lum, stats_with({1000.0})),
          make_frame(5, 'e', red, stats_with({520.0}))};
}

TEST(MasterB13, T_M3B_C02_08_every_permutation_of_a_flat_candidate_gives_the_same_levels_and_status) {
  const std::vector<group_oracle> want = {{4, {4, 2, 1}, master_group_status::flat_level, {{400.0}, {600.0}, {1000.0}}},
                                          {3, {3, 5}, master_group_status::ok, {{500.0}, {520.0}}}};
  const std::vector<master_analysis_frame> base = flat_permutation_candidates();
  std::vector<std::size_t> idx(base.size());
  std::iota(idx.begin(), idx.end(), 0);
  int permutations = 0;
  do {
    std::vector<master_analysis_frame> frames;
    for (const std::size_t i : idx) frames.push_back(base[i]);
    SCOPED_TRACE("permutazione " + std::to_string(permutations));
    expect_groups(screen_master_frames(master_role::flat, frames, full_params()), want);
    ++permutations;
  } while (std::next_permutation(idx.begin(), idx.end()));
  EXPECT_EQ(permutations, 120);
}

// Flat RAW: livelli per canale (R, G1, G2, B) scritti a mano; solo B fuori tolleranza (m̄_B = 750, 500/750 − 1 = −1/3).
TEST(MasterB13, T_M3B_C02_08_raw_flat_group_reports_the_levels_per_channel_and_the_flat_level_status) {
  const master_screening r = screen_master_frames(
      master_role::flat,
      {make_raw_frame(1, 'a', raw_frame_image(master_role::flat), stats_with({400.0, 500.0, 600.0, 500.0})),
       make_raw_frame(2, 'b', raw_frame_image(master_role::flat), stats_with({440.0, 520.0, 580.0, 1000.0}))},
      full_params());
  EXPECT_TRUE(r.excluded.empty());
  expect_groups(r, {{1, {1, 2}, master_group_status::flat_level, {{400.0, 500.0, 600.0, 500.0}, {440.0, 520.0, 580.0, 1000.0}}}});
}

}  // namespace
