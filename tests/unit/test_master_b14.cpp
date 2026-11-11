// M3.b, unità B14: proposta a priori, nodi proposti, aggancio a `analyze_batch`. C-02.b §b.2 («Selezione ed esecuzione»),
// §b.4 (tabella della proposta), §b.7 (sezione «Analisi» del report), §b.10 (`proposal_not_followed`); F-IO-03 CL-05,
// CL-06 (c), CL-11; decisioni L3 (funzione propria `propose_masters`, `astro.master_proposal.v1`) e L8 (esito
// dell'analisi nel parametro `analysis` del nodo).
// Test: T-M3B-C02-09 (proposta: tabella di §b.4 caso per caso), T-M3B-C02-08 resto (preflight di ogni nodo proposto,
// anche RAW; frame incompatibile mai negli ingressi di un altro gruppo; `master.no_usable_frames`), T-M3B-C02-11
// (sorgente della sezione Analisi: parametro `analysis`; la sezione del report è di B11), T-M3B-FLAT-07 parte analisi, T-M3B-C02-10 e T-M3B-FLAT-08 parte candidati permutati, T-M3B-ANA-01, residuo di
// B12 (`saturated_fraction_max` senza `saturation_adu`). Il candidato (c) in `analyze_batch` e `verify_selection` sta in
// test_batch_analysis.cpp e test_verify_selection.cpp.
//
// API SOTTO PROVA (firme attese, tutte in `astro/master_analysis.hpp`; scelte dei test, non scritte nel contratto: PUNTI
// APERTI, decide il coordinatore):
//
//   struct master_proposal_frame {
//     master_analysis_frame analysis;  // input_id, digest, osservazione, statistiche (come per `screen_master_frames`)
//     master_frame_spec spec;          // digest (uguale a analysis.digest), declared, declared_fields, read_options;
//                                      // `spec.decoder` NON è letto: per un RAW stato e identità vengono dall'osservazione
//   };
//   struct master_proposal_request {
//     master_role role;
//     std::vector<master_proposal_frame> frames;                // ordine qualunque
//     master_analysis_params params;
//     std::optional<double> conversion_gain_e_per_adu, read_noise_adu;  // modello di rumore dichiarato (con params.offset_adu)
//     std::optional<simd_request> simd;                         // ADR-07: nessun default; entra nella ricetta del nodo
//   };
//   struct master_proposed_method { combine_method combine; master_rejection rejection; };
//   struct proposed_master {
//     master_group group;                          // come in `master_screening::groups`
//     std::optional<master_proposed_method> method;  // la proposta; assente: nessuna proposta
//     std::string absence;                         // "" se c'è la proposta; "noise_model_not_declared" oppure "flat_level"
//     std::vector<std::string> alternatives;       // {"median_ratio"} con "flat_level", altrimenti vuoto
//     std::optional<master_recipe> recipe;         // solo con la proposta
//     std::optional<node_spec> node;               // solo con la proposta: make_master_node(*recipe)
//     std::string node_key;                        // node->key(); vuota senza proposta
//     json analysis;                               // il parametro `analysis` del nodo (null senza proposta)
//     std::vector<content_digest> frames;          // digest dei membri, ordine canonico
//     input_observation reference_observation;     // osservazione del frame di riferimento
//   };
//   struct master_proposal {
//     master_role role;
//     master_screening screening;                  // missing_params, excluded, usable, groups
//     std::vector<proposed_master> groups;         // stesso ordine di screening.groups
//     std::string canonical() const;               // JCS di `astro.master_proposal.v1`
//   };
//   master_proposal propose_masters(const master_proposal_request&, const fault_injector& = {});
//   std::vector<slot_candidate> proposed_slot_candidates(const master_proposal&);   // candidati (c), solo gruppi con nodo
//
// Forma attesa di `astro.master_proposal.v1` (chiavi dell'oggetto, punto aperto):
//   {format, role, params_missing:[nomi], candidates:[C...], groups:[G...]}
//   C = {digest, status:"used", group:<digest del riferimento>} | {digest, status:"excluded", reason, values}
//       in ordine canonico (digest); gli `input_id` non compaiono.
//   G = {reference:<digest>, members:[digest...], status:"ok"|"flat_level",
//        proposal:{combine, rejection}|null, proposal_absent_reason:"noise_model_not_declared"|"flat_level"|null,
//        alternatives:["median_ratio"]|[], node_key:<chiave>|null}
// Il parametro `analysis` del nodo (L8) è {role, candidates:[C...], group:G senza `node_key`}: sottoinsieme del documento.
//
// Convenzioni: nessun valore atteso viene dal prodotto, tranne il parametro `analysis` del nodo, che la chiave del nodo
// atteso riceve dal prodotto (la sua forma si prova a parte). Le ricette attese sono scritte a mano dalla tabella di §b.4.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/error.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/master.hpp"
#include "astro/master_analysis.hpp"
#include "astro/node.hpp"
#include "expect_stop.hpp"
#include "simd_support.hpp"

namespace {

using namespace astro;

constexpr std::uint32_t kW = 4;
constexpr std::uint32_t kH = 4;

// ---------------------------------------------------------------- immagini e frame

metadata_value<double> hd(double v, const char* key) { return {v, {metadata_origin::header, key}}; }
metadata_value<std::string> hs(const std::string& v, const char* key) { return {v, {metadata_origin::header, key}}; }

content_digest dig(unsigned k) {
  char buf[65];
  std::snprintf(buf, sizeof buf, "%064x", k);
  return content_digest{std::string(buf), 2880};
}

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
  s.white = 4000;
  s.data_maximum = 3900;
  s.linearization = decoder_linearization::identity;
  s.route.decoder = raw_decoder_kind::libraw;
  s.route.reason = "dng_libraw_compression";
  s.route.compression = 1;
  s.route.sample_format = 1;
  s.route.bits_per_sample = 16;
  s.source = metadata_source{metadata_origin::decoder, "decoder_record"};
  return s;
}

decoder_identity identity_a() { return decoder_identity{"0.22.3-Release", 0x001603, 0xc0, std::string(64, '1')}; }

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

master_frame_stats stats_levels(std::vector<std::optional<double>> levels) {
  master_frame_stats s;
  s.pixel_count = kW * kH;
  s.levels = std::move(levels);
  return s;
}

master_proposal_frame pframe(std::uint64_t id, unsigned k, const image& img, std::optional<master_frame_stats> stats = std::nullopt,
                             bool raw = false) {
  master_proposal_frame f;
  f.analysis.input_id = id;
  f.analysis.digest = dig(k);
  f.analysis.observation = raw ? make_observation(img, raw_decl(), identity_a()) : make_observation(img, declarations());
  f.analysis.stats = std::move(stats);
  f.spec.path = "inesistente/frame" + std::to_string(k) + (raw ? ".dng" : ".fits");
  f.spec.digest = dig(k);
  return f;
}

// Frame del ruolo con livello `level` per il flat (un canale per il mono, quattro per un CFA); le statistiche servono solo
// al flat.
master_proposal_frame role_frame(master_role role, std::uint64_t id, unsigned k, double level = 500.0) {
  const image img = frame_image(role);
  return pframe(id, k, img, role == master_role::flat ? std::optional<master_frame_stats>(stats_levels({level})) : std::nullopt);
}

std::vector<master_proposal_frame> n_frames(master_role role, unsigned n) {
  std::vector<master_proposal_frame> v;
  for (unsigned i = 1; i <= n; ++i) v.push_back(role_frame(role, i, i));
  return v;
}

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

constexpr double kGain = 2.0;
constexpr double kReadNoise = 3.0;

master_proposal_request request(master_role role, std::vector<master_proposal_frame> frames, bool noise = false,
                                master_analysis_params p = full_params()) {
  master_proposal_request r;
  r.role = role;
  r.frames = std::move(frames);
  r.params = std::move(p);
  if (noise) {
    r.conversion_gain_e_per_adu = kGain;
    r.read_noise_adu = kReadNoise;
  }
  r.simd = simd_test::requested();
  return r;
}

const std::array<master_role, 4> kRoles = {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat};

const char* role_text(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  return "";
}

// ---------------------------------------------------------------- oracle

// Ricetta attesa del nodo proposto, scritta dalla tabella di §b.4 e dai parametri della richiesta: frame nell'ordine
// canonico (digest), `rejection = none`, `flat_normalization = none` per il flat, modello di rumore solo con `single`,
// `offset_adu` solo con `single` o con un flat RAW.
master_recipe expected_recipe(const master_proposal_request& rq, const proposed_master& pm, combine_method method, bool raw) {
  master_recipe r;
  r.role = rq.role;
  std::vector<master_proposal_frame> members;
  for (const master_proposal_frame& f : rq.frames) {
    if (std::find(pm.group.members.begin(), pm.group.members.end(), f.analysis.input_id) != pm.group.members.end()) members.push_back(f);
  }
  std::sort(members.begin(), members.end(),
            [](const master_proposal_frame& a, const master_proposal_frame& b) { return a.analysis.digest.sha256 < b.analysis.digest.sha256; });
  for (const master_proposal_frame& f : members) {
    master_frame_spec s = f.spec;
    if (raw) s.decoder = frame_decoder_state{base_state(), identity_a()};
    r.frames.push_back(s);
  }
  r.combine = method;
  r.rejection = master_rejection::none;
  if (rq.role == master_role::flat) {
    r.flat_norm = flat_normalization::none;
    r.flat_level_rel_tol = rq.params.flat_level_rel_tol;
    r.flat_min_signal_adu = rq.params.flat_min_signal_adu;
  }
  r.saturation_adu = rq.params.saturation_adu;
  r.saturated_fraction_max = rq.params.saturated_fraction_max;
  r.exposure_rel_tol = rq.params.exposure_rel_tol;
  r.temperature_abs_tol_c = rq.params.temperature_abs_tol_c;
  r.temp_policy = rq.params.temp_policy;
  r.simd = rq.simd;
  if (method == combine_method::single) {
    r.offset_adu = rq.params.offset_adu;
    r.conversion_gain_e_per_adu = rq.conversion_gain_e_per_adu;
    r.read_noise_adu = rq.read_noise_adu;
  } else if (raw && rq.role == master_role::flat) {
    r.offset_adu = rq.params.offset_adu;
  }
  if (raw) r.decoder_black_abs_tol_adu = rq.params.decoder_black_abs_tol_adu;
  r.analysis = pm.analysis;
  return r;
}

variance_estimator estimator_of(combine_method m) {
  switch (m) {
    case combine_method::single: return variance_estimator::noise_model;
    case combine_method::mean: return variance_estimator::empirical;
    default: return variance_estimator::gaussian_reference;
  }
}

// Gruppo con proposta `method`: nodo, ricetta, chiave e descrittore uguali all'oracle.
void expect_proposed(const master_proposal_request& rq, const proposed_master& pm, combine_method method, std::size_t n, bool raw = false) {
  ASSERT_TRUE(pm.method.has_value());
  EXPECT_EQ(pm.method->combine, method);
  EXPECT_EQ(pm.method->rejection, master_rejection::none);
  EXPECT_EQ(pm.absence, "");
  EXPECT_TRUE(pm.alternatives.empty());
  ASSERT_TRUE(pm.node.has_value());
  ASSERT_TRUE(pm.recipe.has_value());
  EXPECT_EQ(pm.node_key, pm.node->key());
  EXPECT_EQ(pm.group.members.size(), n);
  EXPECT_EQ(pm.frames.size(), n);
  const master_recipe want = expected_recipe(rq, pm, method, raw);
  EXPECT_EQ(pm.node_key, make_master_node(want).key()) << "la chiave del nodo proposto è quella della ricetta della tabella";
  const master_descriptor d = master_descriptor_from_node(*pm.node);
  EXPECT_EQ(d.role, rq.role);
  EXPECT_EQ(d.combine, std::optional<combine_method>(method));
  EXPECT_EQ(d.rejection, std::optional<master_rejection>(master_rejection::none));
  EXPECT_EQ(d.combine_count, std::optional<std::uint32_t>(static_cast<std::uint32_t>(n)));
  EXPECT_EQ(d.estimator, std::optional<variance_estimator>(estimator_of(method)));
}

void expect_no_proposal(const proposed_master& pm, const std::string& absence) {
  EXPECT_FALSE(pm.method.has_value());
  EXPECT_EQ(pm.absence, absence);
  EXPECT_FALSE(pm.node.has_value());
  EXPECT_FALSE(pm.recipe.has_value());
  EXPECT_EQ(pm.node_key, "");
  EXPECT_EQ(pm.analysis.type(), json::kind::null);
}

const proposed_master& only_group(const master_proposal& p) {
  EXPECT_EQ(p.groups.size(), 1u);
  if (p.groups.empty()) throw std::runtime_error("nessun gruppo proposto");
  return p.groups.front();
}

std::string text_of(const json* j) { return (j != nullptr && j->type() == json::kind::string) ? j->as_string() : std::string("<assente>"); }

const json& member(const json& o, const char* key) {
  const json* j = o.find(key);
  if (j == nullptr) throw std::runtime_error(std::string("chiave assente: ") + key);
  return *j;
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

// ---------------------------------------------------------------- T-M3B-C02-09: n = 1

TEST(MasterB14, T_M3B_C02_09_one_frame_is_proposed_as_single_when_the_three_noise_parameters_are_declared) {
  for (const master_role role : kRoles) {
    SCOPED_TRACE(role_text(role));
    const master_proposal_request rq = request(role, n_frames(role, 1), true);
    const master_proposal p = propose_masters(rq);
    expect_proposed(rq, only_group(p), combine_method::single, 1);
  }
}

TEST(MasterB14, T_M3B_C02_09_one_frame_has_no_proposal_when_a_noise_parameter_is_not_declared) {
  const std::vector<std::pair<const char*, std::function<void(master_proposal_request&)>>> omissions = {
      {"conversion_gain_e_per_adu", [](master_proposal_request& r) { r.conversion_gain_e_per_adu.reset(); }},
      {"read_noise_adu", [](master_proposal_request& r) { r.read_noise_adu.reset(); }},
      {"offset_adu", [](master_proposal_request& r) { r.params.offset_adu.reset(); }},
  };
  for (const master_role role : kRoles) {
    for (const auto& [name, omit] : omissions) {
      SCOPED_TRACE(std::string(role_text(role)) + " senza " + name);
      master_proposal_request rq = request(role, n_frames(role, 1), true);
      omit(rq);
      const master_proposal p = propose_masters(rq);
      expect_no_proposal(only_group(p), "noise_model_not_declared");
      EXPECT_TRUE(p.screening.missing_params.empty()) << "non è params_missing: il gruppo esiste, manca la proposta";
    }
  }
}

TEST(MasterB14, T_M3B_C02_09_one_frame_with_no_noise_parameters_declared_has_no_proposal) {
  master_proposal_request rq = request(master_role::dark, n_frames(master_role::dark, 1), false);
  rq.params.offset_adu.reset();
  expect_no_proposal(only_group(propose_masters(rq)), "noise_model_not_declared");
}

// ---------------------------------------------------------------- T-M3B-C02-09: n = 2, 3, 1024, 1025

TEST(MasterB14, T_M3B_C02_09_two_frames_are_proposed_as_mean_for_every_role) {
  for (const master_role role : kRoles) {
    SCOPED_TRACE(role_text(role));
    const master_proposal_request rq = request(role, n_frames(role, 2));
    expect_proposed(rq, only_group(propose_masters(rq)), combine_method::mean, 2);
  }
}

TEST(MasterB14, T_M3B_C02_09_three_frames_are_proposed_as_median_for_bias_dark_and_dark_flat) {
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    SCOPED_TRACE(role_text(role));
    const master_proposal_request rq = request(role, n_frames(role, 3));
    expect_proposed(rq, only_group(propose_masters(rq)), combine_method::median, 3);
  }
}

TEST(MasterB14, T_M3B_C02_09_three_frames_of_a_flat_are_proposed_as_mean) {
  const master_proposal_request rq = request(master_role::flat, n_frames(master_role::flat, 3));
  expect_proposed(rq, only_group(propose_masters(rq)), combine_method::mean, 3);
}

TEST(MasterB14, T_M3B_C02_09_1024_frames_are_proposed_as_median_for_a_dark) {
  const master_proposal_request rq = request(master_role::dark, n_frames(master_role::dark, 1024));
  expect_proposed(rq, only_group(propose_masters(rq)), combine_method::median, 1024);
}

TEST(MasterB14, T_M3B_C02_09_1025_frames_are_proposed_as_mean_for_a_dark) {
  const master_proposal_request rq = request(master_role::dark, n_frames(master_role::dark, 1025));
  expect_proposed(rq, only_group(propose_masters(rq)), combine_method::mean, 1025);
}

TEST(MasterB14, T_M3B_C02_09_1024_and_1025_frames_of_a_flat_are_proposed_as_mean) {
  for (const unsigned n : {1024u, 1025u}) {
    SCOPED_TRACE(n);
    const master_proposal_request rq = request(master_role::flat, n_frames(master_role::flat, n));
    expect_proposed(rq, only_group(propose_masters(rq)), combine_method::mean, n);
  }
}

// ---------------------------------------------------------------- T-M3B-C02-09 e FLAT-07 (analisi): flat con `flat_level`

// Livelli 500 e 1500: media 1000, scarto relativo 0,5 > 0,25.
std::vector<master_proposal_frame> flat_with_levels(const std::vector<double>& levels) {
  std::vector<master_proposal_frame> v;
  for (std::size_t i = 0; i < levels.size(); ++i) v.push_back(role_frame(master_role::flat, i + 1, static_cast<unsigned>(i + 1), levels[i]));
  return v;
}

TEST(MasterB14, T_M3B_FLAT_07_flat_level_group_of_two_has_no_proposal_and_names_median_ratio_as_the_alternative) {
  const master_proposal p = propose_masters(request(master_role::flat, flat_with_levels({500.0, 1500.0})));
  const proposed_master& g = only_group(p);
  EXPECT_EQ(g.group.status, master_group_status::flat_level);
  expect_no_proposal(g, "flat_level");
  EXPECT_EQ(g.alternatives, std::vector<std::string>{"median_ratio"});
}

TEST(MasterB14, T_M3B_FLAT_07_flat_level_group_of_three_has_no_proposal_and_names_median_ratio_as_the_alternative) {
  const master_proposal p = propose_masters(request(master_role::flat, flat_with_levels({500.0, 500.0, 1500.0})));
  const proposed_master& g = only_group(p);
  EXPECT_EQ(g.group.status, master_group_status::flat_level);
  expect_no_proposal(g, "flat_level");
  EXPECT_EQ(g.alternatives, std::vector<std::string>{"median_ratio"});
}

TEST(MasterB14, T_M3B_FLAT_07_flat_level_group_above_1024_frames_has_no_proposal) {
  std::vector<double> levels(1025, 500.0);
  levels.back() = 1.0e6;
  const master_proposal proposal = propose_masters(request(master_role::flat, flat_with_levels(levels)));
  const proposed_master& g = only_group(proposal);
  EXPECT_EQ(g.group.status, master_group_status::flat_level);
  expect_no_proposal(g, "flat_level");
}

TEST(MasterB14, T_M3B_FLAT_07_document_of_a_flat_level_group_has_a_null_proposal_the_reason_and_the_alternative) {
  const master_proposal p = propose_masters(request(master_role::flat, flat_with_levels({500.0, 1500.0})));
  const json doc = parse_json(p.canonical());
  EXPECT_EQ(text_of(doc.find("format")), "astro.master_proposal.v1");
  ASSERT_EQ(member(doc, "groups").items().size(), 1u);
  const json& g = member(doc, "groups").items()[0];
  EXPECT_EQ(text_of(g.find("status")), "flat_level");
  EXPECT_EQ(member(g, "proposal").type(), json::kind::null);
  EXPECT_EQ(text_of(g.find("proposal_absent_reason")), "flat_level");
  ASSERT_EQ(member(g, "alternatives").items().size(), 1u);
  EXPECT_EQ(member(g, "alternatives").items()[0].as_string(), "median_ratio");
  EXPECT_EQ(member(g, "node_key").type(), json::kind::null);
}

TEST(MasterB14, T_M3B_FLAT_07_flat_level_group_does_not_stop_the_other_groups_of_the_same_flat_set) {
  // Gruppo A: digest 1 e 2 con livelli 500 e 1500 (flat_level). Gruppo B: filtro R, digest 3 e 4, livelli uguali (ok).
  std::vector<master_proposal_frame> v = flat_with_levels({500.0, 1500.0});
  for (unsigned k : {3u, 4u}) {
    image im = frame_image(master_role::flat);
    im.metadata.filter = hs("R", "FILTER");
    v.push_back(pframe(k, k, im, stats_levels({500.0})));
  }
  const master_proposal_request rq = request(master_role::flat, v);
  const master_proposal p = propose_masters(rq);
  ASSERT_EQ(p.groups.size(), 2u);
  expect_no_proposal(p.groups[0], "flat_level");
  expect_proposed(rq, p.groups[1], combine_method::mean, 2);
}

TEST(MasterB14, T_M3B_FLAT_07_flat_without_flat_level_has_no_alternative_listed) {
  const master_proposal p = propose_masters(request(master_role::flat, flat_with_levels({500.0, 500.0})));
  EXPECT_TRUE(only_group(p).alternatives.empty());
}

// ---------------------------------------------------------------- T-M3B-C02-09: gruppi, parametri e stop

TEST(MasterB14, T_M3B_C02_09_each_group_gets_its_own_proposal_by_its_own_size) {
  // Gruppo A: 3 dark da 60 s (median). Gruppo B: 1 dark da 120 s (single, rumore dichiarato). Digest crescenti.
  std::vector<master_proposal_frame> v = n_frames(master_role::dark, 3);
  image longer = frame_image(master_role::dark);
  longer.metadata.exposure_s = hd(120.0, "EXPTIME");
  v.push_back(pframe(4, 4, longer));
  const master_proposal_request rq = request(master_role::dark, v, true);
  const master_proposal p = propose_masters(rq);
  ASSERT_EQ(p.groups.size(), 2u);
  EXPECT_EQ(p.groups[0].group.members, (std::vector<std::uint64_t>{1, 2, 3}));
  expect_proposed(rq, p.groups[0], combine_method::median, 3);
  EXPECT_EQ(p.groups[1].group.members, (std::vector<std::uint64_t>{4}));
  expect_proposed(rq, p.groups[1], combine_method::single, 1);
  EXPECT_NE(p.groups[0].node_key, p.groups[1].node_key);
}

TEST(MasterB14, T_M3B_C02_09_missing_analysis_parameters_give_no_group_and_no_node) {
  master_proposal_request rq = request(master_role::dark, n_frames(master_role::dark, 3));
  rq.params.exposure_rel_tol.reset();
  const master_proposal p = propose_masters(rq);
  EXPECT_EQ(p.screening.missing_params, std::vector<std::string>{"exposure_rel_tol"});
  EXPECT_TRUE(p.groups.empty());
  const json doc = parse_json(p.canonical());
  ASSERT_EQ(member(doc, "params_missing").items().size(), 1u);
  EXPECT_EQ(member(doc, "params_missing").items()[0].as_string(), "exposure_rel_tol");
  EXPECT_TRUE(member(doc, "groups").items().empty());
}

TEST(MasterB14, T_M3B_C02_09_parameter_out_of_domain_stops_without_a_node) {
  master_proposal_request rq = request(master_role::dark, n_frames(master_role::dark, 3));
  rq.params.exposure_rel_tol = -0.5;
  try {
    (void)propose_masters(rq);
    ADD_FAILURE() << "parametro fuori dominio: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.param.exposure_rel_tol") << e.what();
  }
}

TEST(MasterB14, T_M3B_C02_09_noise_model_parameter_out_of_domain_in_the_request_stops_with_the_recipe_code) {
  master_proposal_request rq = request(master_role::dark, n_frames(master_role::dark, 1), true);
  rq.conversion_gain_e_per_adu = -1.0;
  EXPECT_STOP(propose_masters(rq), error_category::invalid_input, "var.param.conversion_gain_e_per_adu");
}

// ---------------------------------------------------------------- residuo di B12: saturated_fraction_max senza saturation_adu

void expect_fraction_without_saturation_stop(const std::function<void()>& call) {
  try {
    call();
    ADD_FAILURE() << "saturated_fraction_max senza saturation_adu: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.param.saturated_fraction_max") << e.what();
    EXPECT_EQ(key_set(e), "motivo;parametro;") << e.what();
    EXPECT_EQ(context_of(e, "parametro"), std::optional<std::string>("saturated_fraction_max"));
    EXPECT_EQ(context_of(e, "motivo"), std::optional<std::string>("dichiarato senza saturation_adu"));
  }
}

TEST(MasterB14, T_M3B_C02_08_B12_saturated_fraction_max_without_saturation_adu_stops_in_the_analysis_like_in_the_recipe) {
  for (const master_role role : kRoles) {
    SCOPED_TRACE(role_text(role));
    master_analysis_params p = full_params();
    p.saturated_fraction_max = 0.25;
    expect_fraction_without_saturation_stop([&] {
      std::vector<master_analysis_frame> frames;
      for (const master_proposal_frame& f : n_frames(role, 2)) frames.push_back(f.analysis);
      (void)screen_master_frames(role, frames, p);
    });
  }
}

TEST(MasterB14, T_M3B_C02_08_B12_saturated_fraction_max_without_saturation_adu_stops_the_proposal_without_a_node) {
  master_analysis_params p = full_params();
  p.saturated_fraction_max = 0.25;
  expect_fraction_without_saturation_stop([&] { (void)propose_masters(request(master_role::dark, n_frames(master_role::dark, 3), false, p)); });
}

TEST(MasterB14, T_M3B_C02_08_B12_saturated_fraction_max_with_saturation_adu_is_accepted_and_reaches_the_recipe) {
  master_analysis_params p = full_params();
  p.saturation_adu = 60000.0;
  p.saturated_fraction_max = 0.25;
  std::vector<master_proposal_frame> v;
  for (const master_proposal_frame& f : n_frames(master_role::dark, 3)) {
    master_proposal_frame g = f;
    master_frame_stats s;
    s.pixel_count = kW * kH;
    g.analysis.stats = s;
    v.push_back(g);
  }
  const master_proposal_request rq = request(master_role::dark, v, false, p);
  const master_proposal pr = propose_masters(rq);
  expect_proposed(rq, only_group(pr), combine_method::median, 3);
  EXPECT_EQ(only_group(pr).recipe->saturation_adu, std::optional<double>(60000.0));
  EXPECT_EQ(only_group(pr).recipe->saturated_fraction_max, std::optional<double>(0.25));
}

// ---------------------------------------------------------------- T-M3B-C02-08 resto: preflight dei nodi proposti

// Immagini dei frame del nodo nell'ordine della ricetta (canonico): il preflight di `combine_master` le confronta.
std::vector<image> images_of(const master_proposal_request& rq, const proposed_master& pm) {
  std::vector<image> out;
  for (const master_frame_spec& s : pm.recipe->frames) {
    const auto at = std::find_if(rq.frames.begin(), rq.frames.end(),
                                 [&s](const master_proposal_frame& f) { return f.analysis.digest == s.digest; });
    if (at == rq.frames.end()) throw std::runtime_error("frame del nodo non presente nella richiesta");
    out.push_back(at->analysis.observation.observed_image());
  }
  return out;
}

// Preflight del nodo proposto: ricetta valida, nodo costruibile con la chiave dichiarata e, per i FITS, `combine_master`
// sui loro frame non ferma il gruppo (nessuna esclusione, nessun mismatch).
void expect_preflight_passes(const master_proposal_request& rq, const proposed_master& pm) {
  ASSERT_TRUE(pm.recipe.has_value());
  EXPECT_NO_THROW(validate_master_recipe(*pm.recipe));
  EXPECT_EQ(make_master_node(*pm.recipe).key(), pm.node_key);
  EXPECT_NO_THROW((void)master_descriptor_from_node(*pm.node));
  EXPECT_NO_THROW((void)combine_master(*pm.recipe, images_of(rq, pm)));
}

TEST(MasterB14, T_M3B_C02_08_preflight_of_every_proposed_fits_node_passes_for_every_role) {
  for (const master_role role : kRoles) {
    for (const unsigned n : {2u, 3u}) {
      SCOPED_TRACE(std::string(role_text(role)) + " n=" + std::to_string(n));
      const master_proposal_request rq = request(role, n_frames(role, n));
      expect_preflight_passes(rq, only_group(propose_masters(rq)));
    }
  }
}

TEST(MasterB14, T_M3B_C02_08_preflight_of_a_single_node_passes) {
  const master_proposal_request rq = request(master_role::dark, n_frames(master_role::dark, 1), true);
  expect_preflight_passes(rq, only_group(propose_masters(rq)));
}

// Confine vero, valori esatti in binario: tolleranza 0,5, |64 - 128| = 64 = 0,5 * 128 (uguale ammesso, un gruppo); con 128,0625
// la differenza 64,0625 supera 0,5 * 128,0625 = 64,03125 (due gruppi).
master_proposal_request exposure_pair(double other_exposure) {
  std::vector<master_proposal_frame> v;
  image a = frame_image(master_role::dark);
  a.metadata.exposure_s = hd(64.0, "EXPTIME");
  v.push_back(pframe(1, 1, a));
  image b = frame_image(master_role::dark);
  b.metadata.exposure_s = hd(other_exposure, "EXPTIME");
  v.push_back(pframe(2, 2, b));
  master_analysis_params p = full_params();
  p.exposure_rel_tol = 0.5;
  return request(master_role::dark, v, false, p);
}

TEST(MasterB14, T_M3B_C02_08_group_formed_exactly_at_the_exposure_tolerance_passes_the_preflight_of_its_node) {
  const master_proposal_request rq = exposure_pair(128.0);
  const master_proposal p = propose_masters(rq);
  const proposed_master& g = only_group(p);
  EXPECT_EQ(g.group.members.size(), 2u);
  expect_preflight_passes(rq, g);
}

TEST(MasterB14, T_M3B_C02_08_frames_just_beyond_the_exposure_tolerance_form_two_groups) {
  const master_proposal p = propose_masters(exposure_pair(128.0625));
  ASSERT_EQ(p.groups.size(), 2u);
  EXPECT_EQ(p.groups[0].group.members, std::vector<std::uint64_t>{1});
  EXPECT_EQ(p.groups[1].group.members, std::vector<std::uint64_t>{2});
}

TEST(MasterB14, T_M3B_C02_08_group_formed_at_the_temperature_tolerance_edge_passes_the_preflight_of_its_node) {
  std::vector<master_proposal_frame> v = n_frames(master_role::dark, 1);
  image other = frame_image(master_role::dark);
  other.metadata.temperature_c = hd(-9.0, "CCD-TEMP");  // differenza 1,0 = temperature_abs_tol_c
  v.push_back(pframe(2, 2, other));
  const master_proposal_request rq = request(master_role::dark, v);
  const master_proposal proposal = propose_masters(rq);
  const proposed_master& g = only_group(proposal);
  EXPECT_EQ(g.group.members.size(), 2u);
  expect_preflight_passes(rq, g);
}

// Livelli 500, 600, 700: media 600, scarti 1/6 < 0,25; il gruppo è `ok` e il preflight dei livelli passa.
TEST(MasterB14, T_M3B_C02_08_flat_group_with_levels_inside_the_tolerance_passes_the_preflight_of_its_node) {
  const master_proposal_request rq = request(master_role::flat, flat_with_levels({500.0, 600.0, 700.0}));
  const master_proposal proposal = propose_masters(rq);
  const proposed_master& g = only_group(proposal);
  EXPECT_EQ(g.group.status, master_group_status::ok);
  expect_preflight_passes(rq, g);
}

// RAW: il preflight del nodo è puro (B15a): ricetta valida, nodo con `frame_decoder_states`, descrittore. Nessun file.
TEST(MasterB14, T_M3B_C02_08_preflight_of_a_proposed_raw_node_passes_and_the_node_carries_the_decoder_states) {
  for (const master_role role : kRoles) {
    SCOPED_TRACE(role_text(role));
    std::vector<master_proposal_frame> v;
    for (unsigned k = 1; k <= 3; ++k) {
      v.push_back(pframe(k, k, raw_frame_image(role), role == master_role::flat ? std::optional<master_frame_stats>(stats_levels({500.0, 500.0, 500.0, 500.0})) : std::nullopt, true));
    }
    const master_proposal_request rq = request(role, v);
    const master_proposal proposal = propose_masters(rq);
    const proposed_master& g = only_group(proposal);
    expect_proposed(rq, g, role == master_role::flat ? combine_method::mean : combine_method::median, 3, true);
    ASSERT_TRUE(g.recipe.has_value());
    EXPECT_NO_THROW(validate_master_recipe(*g.recipe));
    const master_descriptor d = master_descriptor_from_node(*g.node);
    ASSERT_TRUE(d.frame_decoder_states.has_value());
    ASSERT_EQ(d.frame_decoder_states->size(), 3u);
    for (const frame_decoder_state& s : *d.frame_decoder_states) EXPECT_EQ(s, (frame_decoder_state{base_state(), identity_a()}));
    EXPECT_EQ(g.recipe->decoder_black_abs_tol_adu, std::optional<std::uint32_t>(10));
    EXPECT_EQ(g.recipe->offset_adu.has_value(), role == master_role::flat) << "offset_adu dichiarato solo per il flat RAW";
  }
}

TEST(MasterB14, T_M3B_C02_08_raw_single_node_carries_the_declared_offset_and_the_noise_model) {
  const master_proposal_request rq = request(master_role::dark, {pframe(1, 1, raw_frame_image(master_role::dark), std::nullopt, true)}, true);
  const master_proposal proposal = propose_masters(rq);
  const proposed_master& g = only_group(proposal);
  expect_proposed(rq, g, combine_method::single, 1, true);
  EXPECT_EQ(g.recipe->offset_adu, std::optional<double>(10.0));
  EXPECT_EQ(g.recipe->conversion_gain_e_per_adu, std::optional<double>(kGain));
  EXPECT_EQ(g.recipe->read_noise_adu, std::optional<double>(kReadNoise));
}

// Frame RAW con nero oltre la tolleranza: due gruppi, ognuno col suo nodo e il suo preflight.
TEST(MasterB14, T_M3B_C02_08_raw_frames_beyond_the_black_tolerance_form_two_nodes_that_do_not_share_frames) {
  image far = raw_frame_image(master_role::dark);
  decoder_state s = base_state();
  s.black = 512 + 11;  // tolleranza 10
  far.decoder_state = s;
  const master_proposal_request rq =
      request(master_role::dark, {pframe(1, 1, raw_frame_image(master_role::dark), std::nullopt, true),
                                  pframe(2, 2, raw_frame_image(master_role::dark), std::nullopt, true),
                                  pframe(3, 3, raw_frame_image(master_role::dark), std::nullopt, true), pframe(4, 4, far, std::nullopt, true)});
  const master_proposal p = propose_masters(rq);
  ASSERT_EQ(p.groups.size(), 2u);
  EXPECT_EQ(p.groups[0].group.members, (std::vector<std::uint64_t>{1, 2, 3}));
  EXPECT_EQ(p.groups[1].group.members, (std::vector<std::uint64_t>{4}));
  for (const proposed_master& g : p.groups) {
    if (g.recipe) {
      EXPECT_NO_THROW(validate_master_recipe(*g.recipe));
    }
  }
}

TEST(MasterB14, T_M3B_C02_08_raw_frames_at_exactly_the_black_tolerance_form_one_node_that_passes_the_recipe_validation) {
  image edge = raw_frame_image(master_role::dark);
  decoder_state s = base_state();
  s.black = 512 + 10;  // tolleranza 10: uguale ammesso
  edge.decoder_state = s;
  const master_proposal_request rq =
      request(master_role::dark, {pframe(1, 1, raw_frame_image(master_role::dark), std::nullopt, true), pframe(2, 2, edge, std::nullopt, true)});
  const master_proposal proposal = propose_masters(rq);
  const proposed_master& g = only_group(proposal);
  EXPECT_EQ(g.group.members.size(), 2u);
  ASSERT_TRUE(g.recipe.has_value());
  EXPECT_NO_THROW(validate_master_recipe(*g.recipe));
  EXPECT_EQ(make_master_node(*g.recipe).key(), g.node_key);
}

// ---------------------------------------------------------------- T-M3B-C02-08 resto: frame incompatibile e nessun frame utilizzabile

std::set<std::string> input_digests_of(const proposed_master& g) {
  std::set<std::string> out;
  for (const node_input& in : g.node->inputs()) out.insert(in.file_digest().sha256);
  return out;
}

TEST(MasterB14, T_M3B_C02_08_an_incompatible_or_excluded_frame_is_never_an_input_of_the_node_of_another_group) {
  // 1,2 gain 100; 3,4 gain 200 (altro gruppo); 5 layout rgb (escluso); 6 gain 100 ma esposizione 10 volte (terzo gruppo).
  std::vector<master_proposal_frame> v = n_frames(master_role::dark, 2);
  for (unsigned k : {3u, 4u}) {
    image im = frame_image(master_role::dark);
    im.metadata.gain = hd(200.0, "GAIN");
    v.push_back(pframe(k, k, im));
  }
  image rgb = frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  v.push_back(pframe(5, 5, rgb));
  image longer = frame_image(master_role::dark);
  longer.metadata.exposure_s = hd(600.0, "EXPTIME");
  v.push_back(pframe(6, 6, longer));
  const master_proposal_request rq = request(master_role::dark, v, true);
  const master_proposal p = propose_masters(rq);
  ASSERT_EQ(p.groups.size(), 3u);
  const std::vector<std::set<std::string>> want = {{dig(1).sha256, dig(2).sha256}, {dig(3).sha256, dig(4).sha256}, {dig(6).sha256}};
  std::set<std::string> seen;
  for (std::size_t i = 0; i < p.groups.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(input_digests_of(p.groups[i]), want[i]);
    for (const std::string& d : input_digests_of(p.groups[i])) EXPECT_TRUE(seen.insert(d).second) << "frame in due nodi: " << d;
    expect_preflight_passes(rq, p.groups[i]);
  }
  EXPECT_EQ(seen.count(dig(5).sha256), 0u) << "il frame escluso non entra in nessun nodo";
  ASSERT_EQ(p.screening.excluded.size(), 1u);
  EXPECT_EQ(p.screening.excluded[0].digest, dig(5));
}

TEST(MasterB14, T_M3B_C02_08_no_usable_frame_stops_the_proposal_with_master_no_usable_frames_and_builds_no_node) {
  image rgb = frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  try {
    (void)propose_masters(request(master_role::dark, {pframe(1, 1, rgb), pframe(2, 2, rgb)}));
    ADD_FAILURE() << "nessun frame utilizzabile: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.no_usable_frames") << e.what();
  }
}

TEST(MasterB14, T_M3B_C02_08_proposal_calls_the_group_fault_point_once_and_never_builds_a_node_when_it_fails) {
  int calls = 0;
  const fault_injector fault = [&calls](std::string_view point) {
    if (point == "master.analysis.groups") ++calls;
    return 0;
  };
  (void)propose_masters(request(master_role::dark, n_frames(master_role::dark, 3)), fault);
  EXPECT_EQ(calls, 1);
  const fault_injector failing = [](std::string_view point) { return point == "master.analysis.groups" ? 5 : 0; };
  EXPECT_THROW((void)propose_masters(request(master_role::dark, n_frames(master_role::dark, 3)), failing), stop_error);
}

// ---------------------------------------------------------------- T-M3B-C02-11, sorgente della sezione Analisi: parametro `analysis` e documento

// Insieme misto: 1,2,3 dark da 60 s (gruppo A, median); 4 dark da 120 s (gruppo B, single); 5 escluso (layout rgb).
std::vector<master_proposal_frame> mixed_dark_set() {
  std::vector<master_proposal_frame> v = n_frames(master_role::dark, 3);
  image longer = frame_image(master_role::dark);
  longer.metadata.exposure_s = hd(120.0, "EXPTIME");
  v.push_back(pframe(4, 4, longer));
  image rgb = frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  v.push_back(pframe(5, 5, rgb));
  return v;
}

TEST(MasterB14, T_M3B_C02_11_node_parameter_analysis_is_the_analysis_object_of_the_proposal) {
  const master_proposal p = propose_masters(request(master_role::dark, mixed_dark_set(), true));
  ASSERT_EQ(p.groups.size(), 2u);
  for (const proposed_master& g : p.groups) {
    ASSERT_TRUE(g.node.has_value());
    const json* a = g.node->params().find("analysis");
    ASSERT_NE(a, nullptr) << "il parametro `analysis` è nel nodo";
    ASSERT_EQ(a->type(), json::kind::object);
    EXPECT_EQ(canonical_json(*a), canonical_json(g.analysis));
  }
}

TEST(MasterB14, T_M3B_C02_11_analysis_lists_every_candidate_used_with_its_group_or_excluded_with_reason_and_values) {
  const master_proposal p = propose_masters(request(master_role::dark, mixed_dark_set(), true));
  const json& a = p.groups[0].analysis;
  EXPECT_EQ(text_of(a.find("role")), "dark");
  const std::vector<json>& cands = member(a, "candidates").items();
  ASSERT_EQ(cands.size(), 5u) << "tutti i candidati del ruolo, anche quelli di un altro gruppo";
  for (std::size_t i = 0; i < 5; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(text_of(cands[i].find("digest")), dig(static_cast<unsigned>(i + 1)).sha256) << "ordine canonico per digest";
    EXPECT_EQ(cands[i].find("input_id"), nullptr) << "gli input_id non compaiono";
  }
  for (std::size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(text_of(cands[i].find("status")), "used");
    EXPECT_EQ(text_of(cands[i].find("group")), dig(1).sha256) << "gruppo identificato dal digest del riferimento";
  }
  EXPECT_EQ(text_of(cands[3].find("status")), "used");
  EXPECT_EQ(text_of(cands[3].find("group")), dig(4).sha256);
  EXPECT_EQ(text_of(cands[4].find("status")), "excluded");
  EXPECT_EQ(text_of(cands[4].find("reason")), "layout");
  ASSERT_EQ(p.screening.excluded.size(), 1u);
  json want_values = json::object();
  want_values.set("layout", json::string("rgb"));
  EXPECT_EQ(canonical_json(member(cands[4], "values")), canonical_json(want_values)) << "valori dell'esclusione `layout` (C-02.b §b.2)";
  EXPECT_EQ(canonical_json(p.screening.excluded[0].values), canonical_json(want_values));
}

TEST(MasterB14, T_M3B_C02_11_analysis_describes_the_group_of_the_node_and_its_proposal) {
  const master_proposal p = propose_masters(request(master_role::dark, mixed_dark_set(), true));
  const json& g = member(p.groups[0].analysis, "group");
  EXPECT_EQ(text_of(g.find("reference")), dig(1).sha256);
  ASSERT_EQ(member(g, "members").items().size(), 3u);
  for (std::size_t i = 0; i < 3; ++i) EXPECT_EQ(member(g, "members").items()[i].as_string(), dig(static_cast<unsigned>(i + 1)).sha256);
  EXPECT_EQ(text_of(g.find("status")), "ok");
  EXPECT_EQ(text_of(member(g, "proposal").find("combine")), "median");
  EXPECT_EQ(text_of(member(g, "proposal").find("rejection")), "none");
  const json& g2 = member(p.groups[1].analysis, "group");
  EXPECT_EQ(text_of(g2.find("reference")), dig(4).sha256);
  EXPECT_EQ(text_of(member(g2, "proposal").find("combine")), "single");
}

TEST(MasterB14, T_M3B_C02_11_two_nodes_of_the_same_analysis_have_different_keys_because_their_groups_differ) {
  const master_proposal p = propose_masters(request(master_role::dark, mixed_dark_set(), true));
  EXPECT_NE(p.groups[0].node_key, p.groups[1].node_key);
  EXPECT_NE(canonical_json(p.groups[0].analysis), canonical_json(p.groups[1].analysis));
}

TEST(MasterB14, T_M3B_C02_11_document_has_format_role_candidates_groups_and_node_keys) {
  const master_proposal p = propose_masters(request(master_role::dark, mixed_dark_set(), true));
  const json doc = parse_json(p.canonical());
  EXPECT_EQ(text_of(doc.find("format")), "astro.master_proposal.v1");
  EXPECT_EQ(text_of(doc.find("role")), "dark");
  EXPECT_TRUE(member(doc, "params_missing").items().empty());
  EXPECT_EQ(member(doc, "candidates").items().size(), 5u);
  ASSERT_EQ(member(doc, "groups").items().size(), 2u);
  for (std::size_t i = 0; i < 2; ++i) {
    const json& g = member(doc, "groups").items()[i];
    EXPECT_EQ(text_of(g.find("node_key")), p.groups[i].node_key);
    EXPECT_EQ(member(g, "proposal_absent_reason").type(), json::kind::null);
    EXPECT_TRUE(member(g, "alternatives").items().empty());
    EXPECT_EQ(member(g, "status").as_string(), "ok");
  }
}

TEST(MasterB14, T_M3B_C02_11_document_of_a_group_without_noise_model_names_the_absence_reason) {
  const master_proposal p = propose_masters(request(master_role::dark, n_frames(master_role::dark, 1), false));
  const json doc = parse_json(p.canonical());
  const json& g = member(doc, "groups").items()[0];
  EXPECT_EQ(member(g, "proposal").type(), json::kind::null);
  EXPECT_EQ(text_of(g.find("proposal_absent_reason")), "noise_model_not_declared");
  EXPECT_TRUE(member(g, "alternatives").items().empty());
}

// ---------------------------------------------------------------- T-M3B-C02-10 e T-M3B-FLAT-08: candidati permutati

struct proposal_fingerprint {
  std::string document;
  std::vector<std::string> node_keys;
  std::vector<std::string> analyses;
  friend bool operator==(const proposal_fingerprint&, const proposal_fingerprint&) = default;
};

proposal_fingerprint fingerprint_of(const master_proposal& p) {
  proposal_fingerprint f;
  f.document = p.canonical();
  for (const proposed_master& g : p.groups) {
    f.node_keys.push_back(g.node_key);
    f.analyses.push_back(canonical_json(g.analysis));
  }
  return f;
}

void expect_all_permutations_equal(master_role role, const std::vector<master_proposal_frame>& base, bool noise) {
  std::vector<std::size_t> order(base.size());
  std::iota(order.begin(), order.end(), 0u);
  const proposal_fingerprint want = fingerprint_of(propose_masters(request(role, base, noise)));
  ASSERT_FALSE(want.node_keys.empty());
  std::size_t count = 0;
  do {
    std::vector<master_proposal_frame> v;
    for (const std::size_t i : order) v.push_back(base[i]);
    EXPECT_EQ(fingerprint_of(propose_masters(request(role, v, noise))), want);
    ++count;
  } while (std::next_permutation(order.begin(), order.end()));
  EXPECT_GE(count, 24u) << "tutte le permutazioni di almeno quattro frame";
}

TEST(MasterB14, T_M3B_C02_10_permuted_candidates_give_the_same_document_node_keys_and_analyses_for_a_dark_set) {
  std::vector<master_proposal_frame> base = mixed_dark_set();
  base.erase(base.begin() + 2);  // 1, 2 (gruppo A, mean), 4 (gruppo B, single), 5 (escluso)
  expect_all_permutations_equal(master_role::dark, base, true);
}

TEST(MasterB14, T_M3B_FLAT_08_permuted_candidates_give_the_same_document_and_node_keys_for_a_flat_set) {
  std::vector<master_proposal_frame> base = flat_with_levels({500.0, 600.0, 700.0});
  image other = frame_image(master_role::flat);
  other.metadata.filter = hs("R", "FILTER");
  base.push_back(pframe(4, 4, other, stats_levels({500.0})));
  expect_all_permutations_equal(master_role::flat, base, false);
}

TEST(MasterB14, T_M3B_FLAT_08_permuted_candidates_of_a_flat_level_group_give_the_same_document) {
  std::vector<master_proposal_frame> base = flat_with_levels({500.0, 1500.0, 1000.0, 1000.0});
  std::vector<std::size_t> order(base.size());
  std::iota(order.begin(), order.end(), 0u);
  const std::string want = propose_masters(request(master_role::flat, base)).canonical();
  do {
    std::vector<master_proposal_frame> v;
    for (const std::size_t i : order) v.push_back(base[i]);
    EXPECT_EQ(propose_masters(request(master_role::flat, v)).canonical(), want);
  } while (std::next_permutation(order.begin(), order.end()));
}

TEST(MasterB14, T_M3B_C02_10_input_ids_do_not_enter_the_document_the_node_key_or_the_analysis) {
  std::vector<master_proposal_frame> a = n_frames(master_role::dark, 3);
  std::vector<master_proposal_frame> b = a;
  for (std::size_t i = 0; i < b.size(); ++i) b[i].analysis.input_id = 100 + 7 * i;
  const master_proposal pa = propose_masters(request(master_role::dark, a));
  const master_proposal pb = propose_masters(request(master_role::dark, b));
  EXPECT_EQ(pa.canonical(), pb.canonical());
  EXPECT_EQ(only_group(pa).node_key, only_group(pb).node_key);
  EXPECT_EQ(canonical_json(only_group(pa).analysis), canonical_json(only_group(pb).analysis));
}

// ---------------------------------------------------------------- ingressi incoerenti della richiesta

TEST(MasterB14, T_M3B_C02_09_spec_digest_different_from_the_analysis_digest_stops_naming_both) {
  std::vector<master_proposal_frame> v = n_frames(master_role::dark, 3);
  v[1].spec.digest = dig(99);
  try {
    (void)propose_masters(request(master_role::dark, v));
    ADD_FAILURE() << "digest diversi: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.proposal.frame_digest") << e.what();
    EXPECT_EQ(key_set(e), "frame;specifica;") << e.what();
    EXPECT_EQ(context_of(e, "frame"), std::optional<std::string>(dig(2).sha256));
    EXPECT_EQ(context_of(e, "specifica"), std::optional<std::string>(dig(99).sha256));
  }
}

TEST(MasterB14, T_M3B_C02_09_repeated_input_id_stops_naming_the_id) {
  std::vector<master_proposal_frame> v = n_frames(master_role::dark, 3);
  v[2].analysis.input_id = v[0].analysis.input_id;
  try {
    (void)propose_masters(request(master_role::dark, v));
    ADD_FAILURE() << "input_id ripetuto: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.proposal.input_id") << e.what();
    EXPECT_EQ(key_set(e), "input_id;") << e.what();
    EXPECT_EQ(context_of(e, "input_id"), std::optional<std::string>("1"));
  }
}

// ---------------------------------------------------------------- B14, giro 1

TEST(MasterB14, T_M3B_C02_08_not_applicable_declared_policy_with_the_tolerance_present_gives_a_node_that_passes_the_preflight) {
  master_analysis_params p = full_params();
  p.temp_policy = temperature_policy::not_applicable_declared;
  std::vector<master_proposal_frame> v = n_frames(master_role::dark, 2);
  image warm = frame_image(master_role::dark);
  warm.metadata.temperature_c = hd(25.0, "CCD-TEMP");
  v.push_back(pframe(3, 3, warm));
  const master_proposal_request rq = request(master_role::dark, v, false, p);
  const master_proposal proposal = propose_masters(rq);
  const proposed_master& g = only_group(proposal);
  EXPECT_EQ(g.group.members.size(), 3u) << "la temperatura non separa i gruppi con not_applicable_declared";
  expect_proposed(rq, g, combine_method::median, 3);
  EXPECT_EQ(g.recipe->temperature_abs_tol_c, std::optional<double>(1.0));
  expect_preflight_passes(rq, g);
}

TEST(MasterB14, T_M3B_C02_09_not_applicable_declared_policy_without_the_tolerance_is_params_missing) {
  master_analysis_params p = full_params();
  p.temp_policy = temperature_policy::not_applicable_declared;
  p.temperature_abs_tol_c.reset();
  const master_proposal proposal = propose_masters(request(master_role::dark, n_frames(master_role::dark, 3), false, p));
  EXPECT_EQ(proposal.screening.missing_params, std::vector<std::string>{"temperature_abs_tol_c"});
  EXPECT_TRUE(proposal.groups.empty());
}

// Due copie dello stesso digest con gli input_id scambiati: stessa proposta, stessa analisi, stessa chiave; la copia è
// esclusa come `duplicate_digest` senza `values` (la voce non porta input_id).
TEST(MasterB14, T_M3B_C02_10_copies_of_the_same_digest_with_swapped_input_ids_give_the_same_document_analysis_and_key) {
  const auto build = [](std::uint64_t id_first, std::uint64_t id_copy) {
    std::vector<master_proposal_frame> v;
    v.push_back(role_frame(master_role::dark, id_first, 1));
    v.push_back(role_frame(master_role::dark, id_copy, 1));
    v.push_back(role_frame(master_role::dark, 3, 2));
    v.push_back(role_frame(master_role::dark, 4, 3));
    return propose_masters(request(master_role::dark, v));
  };
  const master_proposal a = build(1, 2);
  const master_proposal b = build(2, 1);
  EXPECT_EQ(a.canonical(), b.canonical());
  EXPECT_EQ(only_group(a).node_key, only_group(b).node_key);
  EXPECT_EQ(canonical_json(only_group(a).analysis), canonical_json(only_group(b).analysis));
  const json doc = parse_json(a.canonical());
  std::size_t duplicates = 0;
  for (const json& c : member(doc, "candidates").items()) {
    if (c.find("reason") != nullptr && c.find("reason")->as_string() == "duplicate_digest") {
      ++duplicates;
      EXPECT_EQ(text_of(c.find("status")), "excluded");
      EXPECT_EQ(text_of(c.find("digest")), dig(1).sha256);
      EXPECT_TRUE(member(c, "values").keys().empty()) << "nessun input_id nei valori";
    }
  }
  EXPECT_EQ(duplicates, 1u);
}

// input_id in ordine inverso ai digest (id = 100 - k): l'ordine canonico segue il digest.
TEST(MasterB14, T_M3B_C02_10_frames_digests_and_documents_follow_the_digest_order_when_input_ids_run_the_other_way) {
  std::vector<master_proposal_frame> v;
  for (unsigned k = 1; k <= 3; ++k) v.push_back(role_frame(master_role::dark, 100 - k, k));
  const master_proposal proposal = propose_masters(request(master_role::dark, v));
  const proposed_master& g = only_group(proposal);
  EXPECT_EQ(g.frames, (std::vector<content_digest>{dig(1), dig(2), dig(3)}));
  EXPECT_EQ(g.group.members, (std::vector<std::uint64_t>{99, 98, 97}));
  EXPECT_EQ(g.group.reference, 99u);
  const json doc = parse_json(proposal.canonical());
  const std::vector<json>& members = member(member(doc, "groups").items()[0], "members").items();
  ASSERT_EQ(members.size(), 3u);
  for (unsigned k = 1; k <= 3; ++k) EXPECT_EQ(members[k - 1].as_string(), dig(k).sha256);
  const std::vector<slot_candidate> slots = proposed_slot_candidates(proposal);
  ASSERT_EQ(slots.size(), 1u);
  ASSERT_TRUE(slots[0].proposed_frames.has_value());
  EXPECT_EQ(*slots[0].proposed_frames, (std::vector<content_digest>{dig(1), dig(2), dig(3)}));
  std::vector<std::string> recipe_order;
  for (const master_frame_spec& f : g.recipe->frames) recipe_order.push_back(f.digest.sha256);
  EXPECT_EQ(recipe_order, (std::vector<std::string>{dig(1).sha256, dig(2).sha256, dig(3).sha256}));
}

// `spec.decoder` del chiamante, se presente, deve coincidere con lo stato dell'osservazione. Contesto scelto: {frame} con il
// digest del frame (punto aperto).
TEST(MasterB14, T_M3B_C02_09_spec_decoder_different_from_the_observation_stops_naming_the_frame) {
  std::vector<master_proposal_frame> v;
  for (unsigned k = 1; k <= 3; ++k) v.push_back(pframe(k, k, raw_frame_image(master_role::dark), std::nullopt, true));
  decoder_state other = base_state();
  other.black = 513;
  v[1].spec.decoder = frame_decoder_state{other, identity_a()};
  try {
    (void)propose_masters(request(master_role::dark, v));
    ADD_FAILURE() << "stato del decoder diverso: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.proposal.decoder_state") << e.what();
    EXPECT_EQ(key_set(e), "frame;") << e.what();
    EXPECT_EQ(context_of(e, "frame"), std::optional<std::string>(dig(2).sha256));
  }
}

void expect_decoder_state_stop(const std::vector<master_proposal_frame>& v, master_role role, unsigned bad_digest) {
  try {
    (void)propose_masters(request(role, v));
    ADD_FAILURE() << "spec.decoder diverso: stop atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "master.proposal.decoder_state") << e.what();
    EXPECT_EQ(context_of(e, "frame"), std::optional<std::string>(dig(bad_digest).sha256));
  }
}

TEST(MasterB14, T_M3B_C02_09_spec_decoder_with_equal_state_and_different_identity_stops) {
  std::vector<master_proposal_frame> v;
  for (unsigned k = 1; k <= 3; ++k) v.push_back(pframe(k, k, raw_frame_image(master_role::dark), std::nullopt, true));
  decoder_identity other = identity_a();
  other.libraw_version = "0.22.4-Release";
  v[2].spec.decoder = frame_decoder_state{base_state(), other};
  expect_decoder_state_stop(v, master_role::dark, 3);
}

TEST(MasterB14, T_M3B_C02_09_spec_decoder_different_on_an_excluded_frame_still_stops) {
  std::vector<master_proposal_frame> v;
  for (unsigned k = 1; k <= 3; ++k) v.push_back(pframe(k, k, raw_frame_image(master_role::dark), std::nullopt, true));
  image rgb = raw_frame_image(master_role::dark);
  rgb.layout = plane_layout::rgb;
  v.push_back(pframe(4, 4, rgb, std::nullopt, true));
  decoder_state other = base_state();
  other.black = 513;
  v[3].spec.decoder = frame_decoder_state{other, identity_a()};
  expect_decoder_state_stop(v, master_role::dark, 4);
}

TEST(MasterB14, T_M3B_C02_09_spec_decoder_different_in_a_group_without_proposal_still_stops) {
  std::vector<master_proposal_frame> v;
  for (unsigned k = 1; k <= 2; ++k) {
    v.push_back(pframe(k, k, raw_frame_image(master_role::flat), stats_levels({500.0, 500.0, 500.0, 500.0}), true));
  }
  v[0].analysis.stats = stats_levels({500.0, 500.0, 500.0, 500.0});
  v[1].analysis.stats = stats_levels({1500.0, 1500.0, 1500.0, 1500.0});  // flat_level: nessuna proposta
  decoder_state other = base_state();
  other.black = 513;
  v[1].spec.decoder = frame_decoder_state{other, identity_a()};
  expect_decoder_state_stop(v, master_role::flat, 2);
}

TEST(MasterB14, T_M3B_C02_09_spec_decoder_equal_to_the_observation_is_accepted) {
  std::vector<master_proposal_frame> v;
  for (unsigned k = 1; k <= 3; ++k) {
    v.push_back(pframe(k, k, raw_frame_image(master_role::dark), std::nullopt, true));
    v.back().spec.decoder = frame_decoder_state{base_state(), identity_a()};
  }
  const master_proposal_request rq = request(master_role::dark, v);
  expect_proposed(rq, only_group(propose_masters(rq)), combine_method::median, 3, true);
}

// ---------------------------------------------------------------- candidati (c)

TEST(MasterB14, T_M3B_C02_08_proposed_slot_candidates_carry_the_node_key_the_derived_descriptor_and_the_reference_observation) {
  const master_proposal_request rq = request(master_role::dark, mixed_dark_set(), true);
  const master_proposal p = propose_masters(rq);
  const std::vector<slot_candidate> slots = proposed_slot_candidates(p);
  ASSERT_EQ(slots.size(), 2u);
  for (std::size_t i = 0; i < 2; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(slots[i].node_key, p.groups[i].node_key);
    EXPECT_EQ(slots[i].role, master_role::dark);
    EXPECT_EQ(slots[i].descriptor, master_descriptor_from_node(*p.groups[i].node));
    EXPECT_FALSE(static_cast<bool>(slots[i].load_samples)) << "il candidato (c) non ha load_samples";
    ASSERT_TRUE(slots[i].proposed_frames.has_value()) << "la presenza di `proposed_frames` marca il candidato di tipo (c)";
    EXPECT_EQ(*slots[i].proposed_frames, p.groups[i].frames);
  }
  const auto ref_obs = [&](unsigned k) {
    for (const master_proposal_frame& f : rq.frames)
      if (f.analysis.digest == dig(k)) return f.analysis.observation;
    throw std::runtime_error("frame assente");
  };
  EXPECT_TRUE(slots[0].observation == ref_obs(1));
  EXPECT_TRUE(slots[1].observation == ref_obs(4));
}

TEST(MasterB14, T_M3B_C02_08_groups_without_a_proposal_give_no_slot_candidate) {
  EXPECT_TRUE(proposed_slot_candidates(propose_masters(request(master_role::flat, flat_with_levels({500.0, 1500.0})))).empty());
  EXPECT_TRUE(proposed_slot_candidates(propose_masters(request(master_role::dark, n_frames(master_role::dark, 1), false))).empty());
}

// ---------------------------------------------------------------- T-M3B-ANA-01: frame_pending_master di analyze_batch -> proposta

batch_input make_batch_input(std::uint64_t id, unsigned k, const std::string& role, const image& img) {
  batch_input in;
  in.input_id = id;
  in.digest = dig(k);
  in.role = role;
  in.declarations_canonical = declarations();
  in.observation = make_observation(img, in.declarations_canonical);
  return in;
}

TEST(MasterB14, T_M3B_ANA_01_proposal_on_the_pending_frames_of_analyze_batch_equals_the_oracle_in_any_candidate_order) {
  // Oracle a mano: dark 1,2,3 (60 s) -> gruppo {1,2,3} median; dark 4 (120 s) -> gruppo {4} single; dark 5 (rgb) escluso.
  struct entry {
    unsigned k;
    double exposure;
    bool rgb;
  };
  const std::vector<entry> entries = {{1, 60.0, false}, {2, 60.0, false}, {3, 60.0, false}, {4, 120.0, false}, {5, 60.0, true}};
  const auto image_of = [](const entry& e) {
    image im = frame_image(master_role::dark);
    im.metadata.exposure_s = hd(e.exposure, "EXPTIME");
    if (e.rgb) im.layout = plane_layout::rgb;
    return im;
  };
  std::vector<std::size_t> order(entries.size());
  std::iota(order.begin(), order.end(), 0u);
  std::set<std::string> fingerprints;
  std::size_t rotation = 0;
  do {
    std::vector<batch_input> inputs;
    std::vector<master_proposal_frame> frames;
    for (const std::size_t i : order) {
      const entry& e = entries[i];
      inputs.push_back(make_batch_input(e.k, e.k, "dark", image_of(e)));
    }
    batch_analysis_request req;
    const batch_proposal bp = analyze_batch(inputs, {}, req);
    ASSERT_EQ(bp.frame_pending_master.count(master_role::dark), 1u);
    const std::vector<std::string>& pending = bp.frame_pending_master.at(master_role::dark);
    ASSERT_EQ(pending.size(), 5u);
    for (const std::string& key : pending) {
      const auto at = std::find_if(entries.begin(), entries.end(), [&key](const entry& e) { return dig(e.k).sha256 == key; });
      ASSERT_NE(at, entries.end());
      frames.push_back(pframe(at->k, at->k, image_of(*at)));
    }
    std::rotate(frames.begin(), frames.begin() + static_cast<std::ptrdiff_t>(rotation++ % frames.size()), frames.end());
    if (rotation % 2 == 0) std::reverse(frames.begin(), frames.end());
    const master_proposal_request rq = request(master_role::dark, frames, true);
    const master_proposal p = propose_masters(rq);
    ASSERT_EQ(p.groups.size(), 2u);
    EXPECT_EQ(p.groups[0].group.members, (std::vector<std::uint64_t>{1, 2, 3}));
    expect_proposed(rq, p.groups[0], combine_method::median, 3);
    EXPECT_EQ(p.groups[1].group.members, (std::vector<std::uint64_t>{4}));
    expect_proposed(rq, p.groups[1], combine_method::single, 1);
    ASSERT_EQ(p.screening.excluded.size(), 1u);
    EXPECT_EQ(p.screening.excluded[0].digest, dig(5));
    fingerprints.insert(p.canonical());
  } while (std::next_permutation(order.begin(), order.end()));
  EXPECT_EQ(fingerprints.size(), 1u) << "stessa proposta per ogni ordine dei candidati";
}

TEST(MasterB14, T_M3B_ANA_01_batch_proposal_v1_still_lists_the_pending_frames_when_proposed_candidates_are_passed) {
  std::vector<batch_input> inputs;
  std::vector<master_proposal_frame> frames = n_frames(master_role::dark, 3);
  for (unsigned k = 1; k <= 3; ++k) inputs.push_back(make_batch_input(k, k, "dark", frame_image(master_role::dark)));
  batch_analysis_request req;
  const batch_proposal without = analyze_batch(inputs, {}, req);
  const master_proposal p = propose_masters(request(master_role::dark, frames));
  const batch_proposal with = analyze_batch(inputs, proposed_slot_candidates(p), req);
  EXPECT_EQ(with.frame_pending_master, without.frame_pending_master);
  EXPECT_EQ(with.canonical().find("astro.batch_proposal.v1") != std::string::npos, true);
  EXPECT_EQ(without.canonical().find("astro.batch_proposal.v1") != std::string::npos, true);
}

}  // namespace
