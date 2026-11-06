// U8b (M3.a), F-IO-03 CL-05…CL-10, CL-12: analisi del batch, gruppi di light, candidati, proposta di sole ricette
// ammissibili. Prove: T-M3A-04 a…f; T-M3A-05 c (parte di analisi) e d. Il resto di U8b (`verify_selection`, T-M3A-06,
// T-M3A-CAL-09) è in test_verify_selection.cpp. Persistenza e proposta del ruolo sono di U8a (test_batch_classification).
//
// Il file usa simboli che U8b deve ancora introdurre e li fissa con questa forma (i nomi di funzione sono quelli del
// contratto; i campi e le firme sono scelte dei test, non scritte nel contratto: vanno ratificate dal coordinatore).
// L'API di calibrazione di U5-U7 è quella dei loro test: `analyze_calibration`, `calibration_analysis`, `chain_recipe`,
// `require_admissible`, `calibration_declarations`, `candidate_status` (astro/calibration_analysis.hpp).
//
//   astro/batch_classification.hpp
//     class input_observation;                         // opaco, confrontabile con ==, copiabile
//     input_observation make_observation(const image& img, const std::string& declarations_canonical);
//         // CL-06/§6: unico costruttore; applica le dichiarazioni `astro.input_declarations.v1` come ADR-03.
//         // Una dichiarazione diversa dal valore letto NON lancia: l'osservazione la registra e l'analisi esclude
//         // l'input con `declared_conflict`.
//     struct batch_input { std::uint64_t input_id; content_digest digest; std::string role;           // sei ruoli CL-01
//                          std::string declarations_canonical;                 // revisione corrente (CL-02): fa parte della
//                                                                              // classificazione effettiva (CL-05, duplicati)
//                          std::string unclassified_reason;                    // solo se role == "unclassified"
//                          std::optional<master_descriptor> descriptor;        // master esterno dichiarato (CL-06 a)
//                          input_observation observation; };
//     struct slot_candidate { std::string node_key; master_role role; master_descriptor descriptor;
//                             input_observation observation; };               // slot `current` (CL-06 b)
//         // Un master RAW è sempre uno slot: solo il descrittore derivato dal nodo porta `frame_decoder_states` (ADR-16.b
//         // §b.7). Il descrittore dichiarato di un master esterno non li ha: un master esterno con osservazione RAW è
//         // escluso con `decoder_state_mixed` per il suo ORIGINE, qualunque descrittore porti.
//     using candidate_choices = std::map<master_role, std::string>;           // ruolo -> chiave del candidato (digest
//                                                                             // esadecimale o chiave del nodo)
//     struct batch_analysis_request { calibration_declarations params; candidate_choices choices; };
//     enum class group_outcome { ready, candidate_choice_required, light_blocked, params_missing };
//     enum class choice_kind { absent, excluded_only, only_compatible, required };
//     struct batch_member { content_digest digest; std::vector<std::uint64_t> input_ids; };
//     struct candidate_report { master_role role; std::string key; candidate_status status; std::string reason; };
//     struct role_choice { master_role role; choice_kind kind; std::vector<std::string> eligible;
//                          std::optional<std::string> chosen; };
//     struct group_proposal { std::string id;                                 // digest minimo dei membri
//                             std::vector<batch_member> members;              // per digest, a pari digest per input_id
//                             group_outcome outcome; error_category block_category; std::string block_code;
//                             std::vector<std::string> missing_params; std::vector<candidate_report> candidates;
//                             std::vector<role_choice> choices; std::optional<calibration_analysis> analysis;
//                             std::vector<chain_recipe> recipes;              // proposta (CL-09): per `ready`, il default
//                                                                             // per primo, poi ogni coppia ammissibile con i
//                                                                             // soli parametri applicabili (§b.5)
//                             std::vector<role_omission> omissions; };        // voci `omitted` del ledger del default
//     struct role_omission { master_role role; std::string reason; };         // `master_absent` | `master_excluded`
//     struct excluded_input { content_digest digest; std::vector<std::uint64_t> input_ids; std::string reason; };
//     struct batch_proposal { std::vector<group_proposal> groups;             // in ordine di `id`
//                             std::vector<excluded_input> excluded;           // in ordine di digest
//                             std::map<master_role, std::vector<std::string>> frame_pending_master;
//                             std::string canonical() const; };               // astro.batch_proposal.v1, senza input_id
//     batch_proposal analyze_batch(const std::vector<batch_input>&, const std::vector<slot_candidate>&,
//                                  const batch_analysis_request&, const fault_injector& fault = {});
//     batch_proposal analyze_batch(const sqlite_dag_store&, const candidate_choices&, memory_pool&, const cancel_token&,
//                                  const fault_injector& fault = {});         // parametri da `settings` batch.analysis_params
//     calibration_selection select_recipe(const group_proposal&, const chain_recipe&);
//         // gruppo `candidate_choice_required`: invalid_input classify.choice_required; ricetta fuori
//         // dall'insieme ammissibile: lo stop di require_admissible (G-STOP, ADR-16.b §b.9)
//   Errori di `analyze_batch` (scelte): invalid_input `classify.choice_unknown_candidate`.
//   Contatore dei campioni: come in U6, `analyze_batch` inoltra `fault` ad `analyze_calibration`, che lo chiama con
//   "calib.analysis.samples" prima di leggere campioni.
//
// Lettura di CL-07, punto 3: due esiti di coppia hanno «lo stesso codice di motivo» se il motivo è lo stesso testo
// (`incompatible.<campo>`, `metadata_missing.<campo>`, ...); campi diversi sono codici diversi, i valori no. Scelta dei
// test, non scritta nel contratto.
//
// Oracle scritti a mano dal contratto: tabella di CL-10, esiti di CL-07/CL-08 e righe 1, 10, 13 di ADR-16.b §b.6
// (le stesse della tabella a mano di U7). Nessun valore atteso viene dal prodotto.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/calibration_analysis.hpp"
#include "astro/error.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "astro/scheduler.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "dng_state_changes.hpp"
#include "expect_stop.hpp"
#include "import_support.hpp"
#include "pub_raw_01.hpp"
#include "record_roundtrip.hpp"
#include "store_test_support.hpp"
#include "syn_fits.hpp"
#include "dng_u13b_support.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;

constexpr std::uint32_t kW = 4;
constexpr std::uint32_t kH = 4;
constexpr std::uint32_t kBlackTol = 10;

// ---------------------------------------------------------------- immagini e ingressi in memoria

metadata_value<double> hd(double v, const char* key) { return {v, {metadata_origin::header, key}}; }
metadata_value<std::string> hs(const std::string& v, const char* key) { return {v, {metadata_origin::header, key}}; }

// Frame nella regola di ogni riga di ADR-16.b §b.2: fase `raw`, `linear`, `adu`, binning 1×1, metadati uguali a quelli
// del light salvo esposizione.
image frame_image(double exposure_s, plane_layout layout = plane_layout::mono, cfa_pattern pattern = cfa_pattern::rggb,
                  std::uint32_t width = kW, std::uint32_t height = kH) {
  image_geometry g(width, height, layout == plane_layout::rgb ? 3 : 1);
  g.binning_x = 1;
  g.binning_y = 1;
  std::optional<cfa_layout> cfa;
  if (layout == plane_layout::cfa) cfa = cfa_layout{pattern, 0, 0};
  image im = make_image(g, sample_type::f64, layout, cfa);
  double* p = im.data<double>();
  for (std::uint64_t i = 0; i < g.sample_count(); ++i) p[i] = 1000.0;
  im.unit = pixel_unit::adu;
  im.lin = linearity::linear;
  im.lin_origin.value = metadata_origin::header;
  im.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::header, "ASTRPHAS"}};
  im.metadata.exposure_s = hd(exposure_s, "EXPTIME");
  im.metadata.gain = hd(100.0, "GAIN");
  im.metadata.offset_adu = hd(10.0, "OFFSET");
  im.metadata.temperature_c = hd(-10.0, "CCD-TEMP");
  im.metadata.filter = hs("L", "FILTER");
  im.metadata.camera = hs("cam-a", "INSTRUME");
  return im;
}

content_digest digest_with(char c) { return content_digest{std::string(64, c), 2880}; }

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

master_descriptor bias_desc() {
  master_descriptor d;
  d.role = master_role::bias;
  return d;
}
master_descriptor dark_desc(master_role role, additive_state add) {
  master_descriptor d;
  d.role = role;
  d.additive = add;
  d.scaling = scaling_state::unscaled;
  return d;
}
master_descriptor flat_desc() {
  master_descriptor d;
  d.role = master_role::flat;
  d.flat = flat_state::raw;
  return d;
}

batch_input make_input(std::uint64_t id, char tag, const std::string& role, const image& img,
                       std::optional<master_descriptor> descriptor = std::nullopt, const std::string& decl = declarations(),
                       const std::string& reason = "") {
  batch_input in;
  in.input_id = id;
  in.digest = digest_with(tag);
  in.role = role;
  in.declarations_canonical = decl;
  in.unclassified_reason = reason;
  in.descriptor = std::move(descriptor);
  in.observation = make_observation(img, decl);
  return in;
}

// Esposizioni: light e dark 300 s, flat e dark-flat 100 s, bias 0,001 s. Digest: light 'a', bias 'b', dark 'd',
// dark-flat 'e', flat 'f'. Tutti nella regola: D `bias_included`, dark-flat INC, flat `raw`.
constexpr double kLightExp = 300.0;
constexpr double kFlatExp = 100.0;

struct baseline {
  image light = frame_image(kLightExp);
  image bias = frame_image(0.001);
  image dark = frame_image(kLightExp);
  image dark_flat = frame_image(kFlatExp);
  image flat = frame_image(kFlatExp);
  std::string light_decl = declarations();
};

std::vector<batch_input> inputs_of(const baseline& b) {
  return {make_input(1, 'a', "light", b.light, std::nullopt, b.light_decl),
          make_input(2, 'b', "bias", b.bias, bias_desc()),
          make_input(3, 'd', "dark", b.dark, dark_desc(master_role::dark, additive_state::bias_included)),
          make_input(4, 'e', "dark_flat", b.dark_flat, dark_desc(master_role::dark_flat, additive_state::bias_included)),
          make_input(5, 'f', "flat", b.flat, flat_desc())};
}

calibration_declarations full_params() {
  calibration_declarations p;
  p.exposure_rel_tol = 0.01;
  p.temperature_abs_tol_c = 1.0;
  p.temp_policy = temperature_policy::compare;
  p.decoder_black_abs_tol_adu = kBlackTol;
  p.normalization = normalization_mode::global_median;  // stessi valori dell'oracolo di U7 (test_calibration_selection)
  p.q_min = 0.5;
  p.q_rel_sigma_max = 10.0;
  return p;
}

batch_analysis_request request(candidate_choices choices = {}) {
  batch_analysis_request r;
  r.params = full_params();
  r.choices = std::move(choices);
  return r;
}

batch_proposal analyze(const std::vector<batch_input>& in, const batch_analysis_request& req = request(),
                       const std::vector<slot_candidate>& slots = {}) {
  return analyze_batch(in, slots, req);
}

// ---------------------------------------------------------------- lettura della proposta

using L = light_chain;
using F = flat_chain;
using pair_key = std::tuple<L, F, bool>;

std::vector<pair_key> admissible_of(const calibration_analysis& a) {
  std::vector<pair_key> v;
  for (const admissible_pair& p : a.admissible) v.emplace_back(p.light, p.flat, p.partial);
  std::sort(v.begin(), v.end());
  return v;
}

std::vector<pair_key> sorted(std::vector<pair_key> v) {
  std::sort(v.begin(), v.end());
  return v;
}

const group_proposal& only_group(const batch_proposal& p) {
  EXPECT_EQ(p.groups.size(), 1u);
  if (p.groups.empty()) throw std::runtime_error("nessun gruppo");
  return p.groups.front();
}

const candidate_report* report_of(const group_proposal& g, master_role role) {
  for (const candidate_report& c : g.candidates) {
    if (c.role == role) return &c;
  }
  return nullptr;
}

const role_choice* choice_of(const group_proposal& g, master_role role) {
  for (const role_choice& c : g.choices) {
    if (c.role == role) return &c;
  }
  return nullptr;
}

// Accesso con marcatore gtest: l'assenza dell'oggetto è un'eccezione del test (fallimento), mai un puntatore nullo dereferenziato.
template <class T>
const T& must(const std::optional<T>& v) {
  if (!v) throw std::runtime_error("valore assente nella proposta");
  return *v;
}
const candidate_report& must_report(const group_proposal& g, master_role role) {
  const candidate_report* r = report_of(g, role);
  if (r == nullptr) throw std::runtime_error("candidato assente per il ruolo " + std::to_string(static_cast<int>(role)));
  return *r;
}
const role_choice& must_choice(const group_proposal& g, master_role role) {
  const role_choice* c = choice_of(g, role);
  if (c == nullptr) throw std::runtime_error("scelta assente per il ruolo " + std::to_string(static_cast<int>(role)));
  return *c;
}

const excluded_input* excluded_of(const batch_proposal& p, char tag) {
  for (const excluded_input& e : p.excluded) {
    if (e.digest == digest_with(tag)) return &e;
  }
  return nullptr;
}

const char* role_name(master_role r) {
  switch (r) {
    case master_role::bias: return "bias";
    case master_role::dark: return "dark";
    case master_role::dark_flat: return "dark_flat";
    case master_role::flat: return "flat";
  }
  return "?";
}

// Catene che un ruolo usa (ricodifica a mano di ADR-16.b §b.5): una ricetta le usa se compare una di queste.
bool uses_role(master_role role, L light, F flat) {
  switch (role) {
    case master_role::dark: return light == L::dark || light == L::bias_dark_sub || light == L::dark_sub_partial;
    case master_role::bias:
      return light == L::bias || light == L::bias_dark_sub || flat == F::bias_dark_flat_sub || flat == F::bias;
    case master_role::dark_flat: return flat == F::dark_flat || flat == F::bias_dark_flat_sub;
    case master_role::flat: return flat != F::none;
  }
  return false;
}

// Nessuna ricetta proposta (default e ammissibili) usa il ruolo escluso.
void expect_role_unused(const calibration_analysis& a, master_role role) {
  EXPECT_FALSE(uses_role(role, a.default_recipe.light, a.default_recipe.flat)) << "default usa " << role_name(role);
  for (const admissible_pair& p : a.admissible) {
    EXPECT_FALSE(uses_role(role, p.light, p.flat)) << "ammissibile usa " << role_name(role);
  }
}

// ---------------------------------------------------------------- T-M3A-04 a: una riga di CL-10 alla volta

// Regola di U6 per esposizione (src/core/calibration.cpp): |a − b| ≤ exposure_rel_tol · max(|a|, |b|). Confine esatto in
// double con exposure_rel_tol = 0,5: light 300 s e dark 600 s (300 = 0,5 · 600), flat 100 s e dark-flat 200 s
// (100 = 0,5 · 200); appena oltre, 601 s e 201 s (301 > 0,5 · 601; 101 > 0,5 · 201). Temperatura: |a − b| ≤ tol con
// tol = 1: −9 contro −10 è esatto in double (1 ≤ 1), −8,9 è oltre.
struct violation {
  const char* name;
  master_role role;
  std::function<void(baseline&)> apply;
  const char* reason;  // vuoto: ancora usable (al confine della tolleranza)
  bool cfa = false;
  double exposure_rel_tol = 0.01;
};

std::vector<violation> violations() {
  std::vector<violation> v;
  v.push_back({"dimensioni: larghezza", master_role::dark, [](baseline& b) { b.dark = frame_image(kLightExp, plane_layout::mono, cfa_pattern::rggb, kW + 1, kH); },
               "incompatible.width"});
  v.push_back({"dimensioni: altezza", master_role::bias, [](baseline& b) { b.bias = frame_image(0.001, plane_layout::mono, cfa_pattern::rggb, kW, kH + 1); },
               "incompatible.height"});
  v.push_back({"CFA: pattern", master_role::dark,
               [](baseline& b) { b.dark = frame_image(kLightExp, plane_layout::cfa, cfa_pattern::grbg); }, "incompatible.cfa", true});
  v.push_back({"esposizione L-D: al confine", master_role::dark, [](baseline& b) { b.dark.metadata.exposure_s = hd(600.0, "EXPTIME"); }, "", false, 0.5});
  v.push_back({"esposizione L-D: oltre", master_role::dark, [](baseline& b) { b.dark.metadata.exposure_s = hd(601.0, "EXPTIME"); },
               "incompatible.exposure_s", false, 0.5});
  v.push_back({"esposizione F-DF: al confine", master_role::dark_flat, [](baseline& b) { b.dark_flat.metadata.exposure_s = hd(200.0, "EXPTIME"); }, "", false, 0.5});
  v.push_back({"esposizione F-DF: oltre", master_role::dark_flat, [](baseline& b) { b.dark_flat.metadata.exposure_s = hd(201.0, "EXPTIME"); },
               "incompatible.exposure_s", false, 0.5});
  v.push_back({"gain", master_role::bias, [](baseline& b) { b.bias.metadata.gain = hd(101.0, "GAIN"); }, "incompatible.gain"});
  v.push_back({"binning_x", master_role::bias, [](baseline& b) { b.bias.geometry.binning_x = 2; }, "incompatible.binning_x"});
  v.push_back({"binning_y", master_role::dark, [](baseline& b) { b.dark.geometry.binning_y = 2; }, "incompatible.binning_y"});
  v.push_back({"filtro", master_role::flat, [](baseline& b) { b.flat.metadata.filter = hs("R", "FILTER"); }, "incompatible.filter"});
  v.push_back({"temperatura L-B: al confine", master_role::bias, [](baseline& b) { b.bias.metadata.temperature_c = hd(-9.0, "CCD-TEMP"); }, ""});
  v.push_back({"temperatura L-B: oltre", master_role::bias, [](baseline& b) { b.bias.metadata.temperature_c = hd(-8.9, "CCD-TEMP"); },
               "incompatible.temperature_c"});
  v.push_back({"temperatura F-DF: oltre", master_role::dark_flat, [](baseline& b) { b.dark_flat.metadata.temperature_c = hd(-8.9, "CCD-TEMP"); },
               "incompatible.temperature_c"});
  v.push_back({"offset", master_role::dark, [](baseline& b) { b.dark.metadata.offset_adu = hd(12.0, "OFFSET"); }, "incompatible.offset_adu"});
  v.push_back({"campo assente: gain", master_role::dark, [](baseline& b) { b.dark.metadata.gain.reset(); }, "metadata_missing.gain"});
  return v;
}

batch_analysis_request request_for(const violation& v) {
  batch_analysis_request r = request();
  r.params.exposure_rel_tol = v.exposure_rel_tol;
  return r;
}

// Oracle scritto a mano (ADR-16.b §b.6, stesse righe della tabella a mano di U7): default e insieme ammissibile quando
// UN solo master è escluso, a partire dalla riga 1 (111, D, flat raw con INC).
struct expected_proposal {
  L default_light;
  F default_flat;
  std::vector<pair_key> admissible;
};

expected_proposal expected_when_excluded(master_role excluded) {
  switch (excluded) {
    case master_role::dark:  // riga 13 (011, flat raw con INC)
      return {L::bias, F::dark_flat, sorted({{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias, F::dark_flat, false}})};
    case master_role::bias:  // riga 1 senza il bias: D e INC già includono il bias
      return {L::dark, F::dark_flat, sorted({{L::none, F::none, false}, {L::dark, F::none, false}, {L::dark, F::dark_flat, false}})};
    case master_role::dark_flat:  // riga 23: P-DF-BAD, il flat non si usa
      return {L::dark, F::none, sorted({{L::none, F::none, false}, {L::bias, F::none, false}, {L::dark, F::none, false}})};
    case master_role::flat:  // riga 10 (101): nessun flat usabile
      return {L::dark, F::none, sorted({{L::none, F::none, false}, {L::bias, F::none, false}, {L::dark, F::none, false}})};
  }
  return {L::none, F::none, {}};
}

TEST(BatchAnalysis, T_M3A_04a_baseline_is_ready_with_the_u7_hand_table_row_1) {
  const baseline b;
  const batch_proposal p = analyze(inputs_of(b));
  const group_proposal& g = only_group(p);
  EXPECT_EQ(g.outcome, group_outcome::ready);
  EXPECT_EQ(g.id, std::string(64, 'a'));
  EXPECT_TRUE(p.excluded.empty());
  for (master_role r : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    const candidate_report* c = report_of(g, r);
    ASSERT_NE(c, nullptr) << role_name(r);
    EXPECT_EQ(c->status, candidate_status::usable) << role_name(r) << ": " << c->reason;
    EXPECT_TRUE(c->reason.empty()) << role_name(r);
    const role_choice* ch = choice_of(g, r);
    ASSERT_NE(ch, nullptr) << role_name(r);
    EXPECT_EQ(ch->kind, choice_kind::only_compatible) << role_name(r);
  }
  ASSERT_TRUE(g.analysis.has_value());
  // ADR-16.b §b.6 riga 1 (111, D, flat raw con INC), identica alla riga 1 della tabella a mano di U7.
  EXPECT_EQ(must(g.analysis).default_recipe.light, L::dark);
  EXPECT_EQ(must(g.analysis).default_recipe.flat, F::dark_flat);
  EXPECT_EQ(admissible_of(must(g.analysis)),
            sorted({{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias, F::dark_flat, false},
                    {L::dark, F::none, false}, {L::dark, F::dark_flat, false}}));
}

// ADR-16.b §b.9: la ricetta di default non usa mai una dichiarazione assente (D-16). Senza `normalization`, `q_min` e
// `q_rel_sigma_max` la catena del flat con dark-flat resta nell'insieme ammissibile ma il default del flat è `flat.none`.
TEST(BatchAnalysis, T_M3A_04a_without_flat_declarations_the_default_flat_is_none_and_the_chain_stays_admissible) {
  const baseline b;
  batch_analysis_request req = request();
  req.params.normalization.reset();
  req.params.q_min.reset();
  req.params.q_rel_sigma_max.reset();
  const group_proposal g = only_group(analyze(inputs_of(b), req));
  ASSERT_TRUE(g.analysis.has_value());
  EXPECT_EQ(must(g.analysis).default_recipe.flat, F::none);
  EXPECT_EQ(admissible_of(must(g.analysis)),
            sorted({{L::none, F::none, false}, {L::bias, F::none, false}, {L::bias, F::dark_flat, false},
                    {L::dark, F::none, false}, {L::dark, F::dark_flat, false}}));
}

baseline cfa_baseline() {
  baseline b;
  b.light = frame_image(kLightExp, plane_layout::cfa);
  b.bias = frame_image(0.001, plane_layout::cfa);
  b.dark = frame_image(kLightExp, plane_layout::cfa);
  b.dark_flat = frame_image(kFlatExp, plane_layout::cfa);
  b.flat = frame_image(kFlatExp, plane_layout::cfa);
  return b;
}

TEST(BatchAnalysis, T_M3A_04a_each_cl10_row_violated_by_one_master_gives_the_table_reason_and_the_oracle_proposal) {
  for (const violation& v : violations()) {
    SCOPED_TRACE(v.name);
    baseline b = v.cfa ? cfa_baseline() : baseline{};
    v.apply(b);
    const batch_proposal p = analyze(inputs_of(b), request_for(v));
    const group_proposal& g = only_group(p);
    ASSERT_EQ(g.outcome, group_outcome::ready);
    const std::string reason = v.reason;
    for (master_role r : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
      const candidate_report* c = report_of(g, r);
      ASSERT_NE(c, nullptr) << role_name(r);
      if (r == v.role && !reason.empty()) {
        EXPECT_EQ(c->status, candidate_status::excluded);
        EXPECT_EQ(c->reason, reason);
      } else {
        EXPECT_EQ(c->status, candidate_status::usable) << role_name(r) << ": " << c->reason;
        EXPECT_TRUE(c->reason.empty()) << role_name(r);
      }
    }
    ASSERT_TRUE(g.analysis.has_value());
    if (reason.empty()) {
      EXPECT_EQ(must(g.analysis).default_recipe.light, L::dark);
      EXPECT_EQ(must(g.analysis).default_recipe.flat, F::dark_flat);
    } else {
      const expected_proposal e = expected_when_excluded(v.role);
      EXPECT_EQ(must(g.analysis).default_recipe.light, e.default_light) << to_string(must(g.analysis).default_recipe.light);
      EXPECT_EQ(must(g.analysis).default_recipe.flat, e.default_flat) << to_string(must(g.analysis).default_recipe.flat);
      EXPECT_EQ(admissible_of(must(g.analysis)), e.admissible);
      expect_role_unused(must(g.analysis), v.role);
    }
  }
}

// RAW: osservazioni in memoria di un light e di master con `frame_decoder_states` (ADR-16.b §b.7).
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

image raw_image(double exposure, const decoder_state& s) {
  image im = frame_image(exposure, plane_layout::cfa);
  im.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::decoder, "decoder"}};
  im.metadata.offset_adu.reset();
  // Il decoder non produce binning né filtro (ADR-08.b §2.6): vengono solo dalle dichiarazioni.
  im.geometry.binning_x.reset();
  im.geometry.binning_y.reset();
  im.metadata.filter.reset();
  im.decoder_state = s;
  return im;
}

// Master RAW: sempre uno slot `current` (CL-06 b) col descrittore derivato dal nodo, che porta `frame_decoder_states`.
slot_candidate raw_slot(char tag, master_role role, const image& img, master_descriptor d, const decoder_state& s,
                        const std::string& decl) {
  d.frame_decoder_states = std::vector<frame_decoder_state>{frame_decoder_state{s, identity_a()}};
  return slot_candidate{std::string(64, tag), role, std::move(d), make_observation(img, decl)};
}

// Dichiarazioni di binning e filtro di un ingresso RAW: il decoder non li produce (CL-10, estensione 2).
std::string raw_decl(std::uint64_t binning = 1, const std::string& filter = "L") {
  return declarations({{"binning_x", json::uinteger(binning)}, {"binning_y", json::uinteger(binning)}},
                      {{"filter", json::string(filter)}});
}

struct raw_batch {
  decoder_state light_state = base_state();
  decoder_state dark_state = base_state();
  std::string light_decl = raw_decl();
  std::string master_decl = raw_decl();
};

// Un light RAW come input classificato; i quattro master RAW come slot `current`.
batch_proposal analyze_raw(const raw_batch& b, const batch_analysis_request& req = request()) {
  const decoder_state s = base_state();
  const std::vector<batch_input> inputs = {make_input(1, 'a', "light", raw_image(kLightExp, b.light_state), std::nullopt, b.light_decl)};
  const std::vector<slot_candidate> slots = {
      raw_slot('b', master_role::bias, raw_image(0.001, s), bias_desc(), s, b.master_decl),
      raw_slot('d', master_role::dark, raw_image(kLightExp, b.dark_state), dark_desc(master_role::dark, additive_state::bias_included),
               b.dark_state, b.master_decl),
      raw_slot('e', master_role::dark_flat, raw_image(kFlatExp, s), dark_desc(master_role::dark_flat, additive_state::bias_included), s,
               b.master_decl),
      raw_slot('f', master_role::flat, raw_image(kFlatExp, s), flat_desc(), s, b.master_decl)};
  return analyze_batch(inputs, slots, req);
}

// L'uguaglianza di due osservazioni confronta anche l'instradamento (ogni membro di `dng_route_result`) e ciascun membro
// dei campi del decoder del progetto (U13 §8).
TEST(BatchAnalysis, T_M3A_06a_observations_differing_only_in_route_or_in_one_dng_field_are_not_equal) {
  decoder_state libraw_state = base_state();
  libraw_state.route = dng_route_result{raw_decoder_kind::libraw, "not_tiff", std::nullopt, std::nullopt, std::nullopt};
  decoder_state project_state = base_state();
  project_state.route = dng_state_fixture::project_route();
  project_state.dng = dng_state_fixture::project_fields(kH);

  const auto observe = [](const decoder_state& s) { return make_observation(raw_image(kLightExp, s), raw_decl(), identity_a()); };
  EXPECT_TRUE(observe(libraw_state) == observe(libraw_state)) << "controllo positivo: stati uguali";
  EXPECT_TRUE(observe(project_state) == observe(project_state)) << "controllo positivo: stati uguali";

  struct row {
    std::string name;
    const decoder_state* base;
    std::function<void(decoder_state&)> change;
  };
  std::vector<row> rows = {
      {"route.decoder", &libraw_state, [](decoder_state& s) { s.route.decoder = raw_decoder_kind::astro_dng; }},
      {"route.reason", &libraw_state, [](decoder_state& s) { s.route.reason = "tiff_not_dng"; }},
      {"route.compression", &libraw_state, [](decoder_state& s) { s.route.compression = 7; }},
      {"route.sample_format", &project_state, [](decoder_state& s) { s.route.sample_format = 3; }},
      {"route.bits_per_sample", &project_state, [](decoder_state& s) { s.route.bits_per_sample = 32; }},
  };
  const std::vector<dng_state_fixture::field_change> dng_changes = dng_state_fixture::dng_field_changes(kW, kH);
  ASSERT_EQ(dng_changes.size(), 15U) << "un cambio per ciascun membro di `dng_decoder_fields`";
  for (const dng_state_fixture::field_change& c : dng_changes) rows.push_back({c.field, &project_state, c.apply});

  for (const row& r : rows) {
    SCOPED_TRACE(r.name);
    decoder_state changed = *r.base;
    r.change(changed);
    ASSERT_FALSE(changed == *r.base) << "il cambio deve toccare il contenuto";
    if (r.name.rfind("dng.", 0) == 0) {
      ASSERT_TRUE(changed.route == r.base->route) << "solo `dng` cambia";
    }
    EXPECT_FALSE(observe(changed) == observe(*r.base)) << "osservazioni diverse solo in " << r.name << " risultano uguali";
  }
}

TEST(BatchAnalysis, T_M3A_04a_raw_black_at_tolerance_and_beyond_and_structural_field) {
  struct row {
    const char* name;
    std::function<void(decoder_state&)> change;
    const char* reason;
  };
  const std::vector<row> rows = {
      {"nero alla tolleranza", [](decoder_state& s) { s.black = 512 + kBlackTol; }, ""},
      {"nero tolleranza + 1", [](decoder_state& s) { s.black = 512 + kBlackTol + 1; }, "decoder_black"},
      {"nero tolleranza - 1 sotto", [](decoder_state& s) { s.black = 512 - kBlackTol; }, ""},
      {"nero sotto la tolleranza", [](decoder_state& s) { s.black = 512 - kBlackTol - 1; }, "decoder_black"},
      {"cfa_color_index", [](decoder_state& s) { s.cfa_color_index = {1, 0, 2, 3}; }, "decoder_state_mismatch.cfa_color_index"},
  };
  for (const row& r : rows) {
    SCOPED_TRACE(r.name);
    raw_batch b;
    r.change(b.dark_state);
    const group_proposal g = only_group(analyze_raw(b));
    ASSERT_EQ(g.outcome, group_outcome::ready);
    const candidate_report* c = report_of(g, master_role::dark);
    ASSERT_NE(c, nullptr);
    if (std::string(r.reason).empty()) {
      EXPECT_EQ(c->status, candidate_status::usable) << c->reason;
    } else {
      EXPECT_EQ(c->status, candidate_status::excluded);
      EXPECT_EQ(c->reason, r.reason);
      ASSERT_TRUE(g.analysis.has_value());
      expect_role_unused(must(g.analysis), master_role::dark);
    }
  }
}

// ---------------------------------------------------------------- T-M3A-04 b: dichiarazioni di binning e filtro

TEST(BatchAnalysis, T_M3A_04b_raw_binning_and_filter_come_only_from_declarations) {
  const auto reasons = [](const raw_batch& b) {
    const group_proposal g = only_group(analyze_raw(b));
    return std::make_pair(must_report(g, master_role::dark).reason, must_report(g, master_role::flat).reason);
  };
  {  // nessuna dichiarazione: il prodotto non assume 1×1 né «nessun filtro»
    raw_batch b;
    b.light_decl = declarations();
    b.master_decl = declarations();
    const auto got = reasons(b);
    // Il contratto elenca i tre motivi senza fissare quale compaia per primo: ne basta uno dei tre, per coppia.
    const std::set<std::string> any_missing = {"metadata_missing.binning_x", "metadata_missing.binning_y", "metadata_missing.filter"};
    EXPECT_TRUE(any_missing.count(got.first) == 1) << got.first;
    EXPECT_TRUE(any_missing.count(got.second) == 1) << got.second;
    const group_proposal g = only_group(analyze_raw(b));
    EXPECT_EQ(must_report(g, master_role::dark).status, candidate_status::excluded);
  }
  {  // solo binning dichiarato: il filtro manca solo per la coppia L-F
    raw_batch b;
    b.light_decl = declarations({{"binning_x", json::uinteger(1)}, {"binning_y", json::uinteger(1)}});
    b.master_decl = b.light_decl;
    const group_proposal g = only_group(analyze_raw(b));
    EXPECT_EQ(must_report(g, master_role::dark).status, candidate_status::usable);
    EXPECT_EQ(must_report(g, master_role::flat).status, candidate_status::excluded);
    EXPECT_EQ(must_report(g, master_role::flat).reason, "metadata_missing.filter");
  }
  {  // solo binning_y mancante
    raw_batch b;
    b.light_decl = declarations({{"binning_x", json::uinteger(1)}}, {{"filter", json::string("L")}});
    b.master_decl = b.light_decl;
    const group_proposal g = only_group(analyze_raw(b));
    EXPECT_EQ(must_report(g, master_role::dark).reason, "metadata_missing.binning_y");
  }
  {  // dichiarazioni uguali: usable
    const raw_batch b;
    const group_proposal g = only_group(analyze_raw(b));
    for (master_role r : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
      EXPECT_EQ(must_report(g, r).status, candidate_status::usable) << role_name(r) << ": " << must_report(g, r).reason;
    }
  }
  {  // dichiarazioni diverse
    raw_batch b;
    b.master_decl = raw_decl(2, "R");
    const group_proposal g = only_group(analyze_raw(b));
    EXPECT_EQ(must_report(g, master_role::dark).reason, "incompatible.binning_x");
    EXPECT_EQ(must_report(g, master_role::flat).reason, "incompatible.binning_x");
    raw_batch f;
    f.master_decl = raw_decl(1, "R");
    const group_proposal gf = only_group(analyze_raw(f));
    EXPECT_EQ(must_report(gf, master_role::dark).status, candidate_status::usable);
    EXPECT_EQ(must_report(gf, master_role::flat).reason, "incompatible.filter");
  }
}

TEST(BatchAnalysis, T_M3A_04b_declared_value_in_conflict_with_the_header_excludes_the_input) {
  baseline b;
  b.light_decl = declarations({{"binning_x", json::uinteger(2)}});  // l'header dice 1
  const batch_proposal p = analyze(inputs_of(b));
  const excluded_input* e = excluded_of(p, 'a');
  ASSERT_NE(e, nullptr);
  EXPECT_EQ(e->reason, "declared_conflict");
  EXPECT_TRUE(p.groups.empty());  // nessun light: nessun gruppo
  // Lo stesso su un master: escluso dall'analisi con `declared_conflict`, non candidato.
  baseline m;
  std::vector<batch_input> in = inputs_of(m);
  in[2] = make_input(3, 'd', "dark", m.dark, dark_desc(master_role::dark, additive_state::bias_included),
                     declarations({{"binning_y", json::uinteger(2)}}));
  const batch_proposal q = analyze(in);
  const excluded_input* d = excluded_of(q, 'd');
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->reason, "declared_conflict");
  EXPECT_EQ(report_of(only_group(q), master_role::dark), nullptr);
}

// ---------------------------------------------------------------- T-M3A-04 c: gruppi di light

struct light_spec {
  std::uint64_t id;
  char tag;
  std::function<void(image&)> change;
  std::string decl = declarations();
};

// Batch progettato a mano: un dark (300 s, -10 °C) e un flat; i light variano di un campo alla volta.
std::vector<batch_input> lights_batch(const std::vector<light_spec>& lights) {
  const baseline b;
  std::vector<batch_input> in;
  for (const light_spec& l : lights) {
    image im = b.light;
    if (l.change) l.change(im);
    in.push_back(make_input(l.id, l.tag, "light", im, std::nullopt, l.decl));
  }
  in.push_back(make_input(10, 'd', "dark", b.dark, dark_desc(master_role::dark, additive_state::bias_included)));
  in.push_back(make_input(11, 'f', "flat", b.flat, flat_desc()));
  return in;
}

std::vector<std::string> ids_of(const group_proposal& g) {
  std::vector<std::string> v;
  for (const batch_member& m : g.members) v.push_back(m.digest.sha256.substr(0, 1));
  return v;
}

TEST(BatchAnalysis, T_M3A_04c_partition_follows_the_exact_key_and_the_pair_outcomes) {
  const std::vector<light_spec> lights = {
      {1, '1', nullptr},                                                                                // riferimento
      {2, '2', nullptr},                                                                                // uguale
      {3, '3', [](image& i) { i.metadata.filter = hs("R", "FILTER"); }},                                // solo filtro
      {4, '4', [](image& i) { i.geometry.binning_x = 2; i.geometry.binning_y = 2; }},                   // solo binning
      {5, '5', [](image& i) { i.metadata.exposure_s = hd(303.0, "EXPTIME"); }},                         // dentro la tolleranza del dark
      {6, '6', [](image& i) { i.metadata.exposure_s = hd(330.0, "EXPTIME"); }},                         // fuori
      {7, '7', [](image& i) { i.metadata.temperature_c = hd(-9.0, "CCD-TEMP"); }},                      // dentro
      {9, '9', [](image& i) { i.phase = metadata_value<process_phase>{process_phase::calibrated, {metadata_origin::header, "ASTRPHAS"}}; }},  // dominio
  };
  const batch_proposal p = analyze(lights_batch(lights));
  // Oracle a mano. La chiave esatta separa filtro (3) e binning (4); l'esposizione e la temperatura separano solo
  // attraverso l'esito della coppia L-D: 5 e 7 (dentro la tolleranza) stanno col riferimento, 6 (fuori) ha un esito
  // diverso. Il light `calibrated` ha un dominio diverso: gruppo `light_blocked`.
  const std::vector<std::vector<std::string>> expected = {{"1", "2", "5", "7"}, {"3"}, {"4"}, {"6"}, {"9"}};
  std::vector<std::vector<std::string>> got;
  for (const group_proposal& g : p.groups) got.push_back(ids_of(g));
  std::sort(got.begin(), got.end());
  auto want = expected;
  std::sort(want.begin(), want.end());
  EXPECT_EQ(got, want);
  for (const group_proposal& g : p.groups) {
    const std::vector<std::string> m = ids_of(g);
    EXPECT_EQ(g.id, std::string(64, m.front()[0])) << "il riferimento è il digest minimo";
    EXPECT_TRUE(std::is_sorted(m.begin(), m.end()));
    if (m == std::vector<std::string>{"9"}) {
      EXPECT_EQ(g.outcome, group_outcome::light_blocked);
      EXPECT_EQ(g.block_code, "op.phase_blocked");
      EXPECT_FALSE(g.analysis.has_value());
    } else {
      EXPECT_EQ(g.outcome, group_outcome::ready) << g.id;
    }
  }
}

TEST(BatchAnalysis, T_M3A_04c_filter_and_binning_are_in_the_group_key_even_without_any_master) {
  const baseline b;
  const auto lights_only = [&](const std::function<void(image&)>& change) {
    image other = b.light;
    change(other);
    return analyze({make_input(1, '1', "light", b.light), make_input(2, '2', "light", other)});
  };
  EXPECT_EQ(lights_only([](image& i) { i.metadata.filter = hs("R", "FILTER"); }).groups.size(), 2u) << "solo il filtro";
  EXPECT_EQ(lights_only([](image& i) { i.geometry.binning_x = 2; }).groups.size(), 2u) << "solo binning_x";
  EXPECT_EQ(lights_only([](image& i) { i.geometry.binning_y = 2; }).groups.size(), 2u) << "solo binning_y";
  EXPECT_EQ(lights_only([](image&) {}).groups.size(), 1u) << "controllo: light uguali stanno insieme";
}

TEST(BatchAnalysis, T_M3A_04c_groups_with_different_pair_outcome_codes_are_split) {
  // 5 (dentro) e 6 (fuori): stessa chiave, ma L-D `usable` contro `excluded`: gruppi distinti. Il light 6 e un light
  // con temperatura fuori dalla tolleranza del bias non condividono l'esito: l'esito per candidato decide.
  const batch_proposal p = analyze(lights_batch({{5, '5', [](image& i) { i.metadata.exposure_s = hd(303.0, "EXPTIME"); }},
                                                 {6, '6', [](image& i) { i.metadata.exposure_s = hd(330.0, "EXPTIME"); }}}));
  ASSERT_EQ(p.groups.size(), 2u);
  const group_proposal* inside = p.groups[0].id == std::string(64, '5') ? &p.groups[0] : &p.groups[1];
  const group_proposal* outside = inside == &p.groups[0] ? &p.groups[1] : &p.groups[0];
  EXPECT_EQ(must_report(*inside, master_role::dark).status, candidate_status::usable);
  EXPECT_EQ(must_report(*outside, master_role::dark).status, candidate_status::excluded);
  EXPECT_EQ(must_report(*outside, master_role::dark).reason, "incompatible.exposure_s");
  ASSERT_TRUE(outside->analysis.has_value());
  expect_role_unused(*outside->analysis, master_role::dark);
}

TEST(BatchAnalysis, T_M3A_04c_member_analysis_equals_the_reference_analysis) {
  const std::vector<light_spec> lights = {
      {1, '1', nullptr},
      {2, '2', [](image& i) { i.metadata.exposure_s = hd(303.0, "EXPTIME"); }},
      {3, '3', [](image& i) { i.metadata.temperature_c = hd(-9.0, "CCD-TEMP"); }},
  };
  const batch_proposal p = analyze(lights_batch(lights));
  const group_proposal& g = only_group(p);
  ASSERT_EQ(g.members.size(), 3u);
  ASSERT_TRUE(g.analysis.has_value());
  for (const light_spec& l : lights) {
    const group_proposal alone = only_group(analyze(lights_batch({l})));
    ASSERT_TRUE(alone.analysis.has_value());
    EXPECT_EQ(admissible_of(must(alone.analysis)), admissible_of(must(g.analysis))) << "light " << l.tag;
    EXPECT_EQ(must(alone.analysis).predicates.dfb, must(g.analysis).predicates.dfb) << "light " << l.tag;
    EXPECT_EQ(must(alone.analysis).predicates.df, must(g.analysis).predicates.df) << "light " << l.tag;
    EXPECT_EQ(must(alone.analysis).predicates.flat_available, must(g.analysis).predicates.flat_available) << "light " << l.tag;
    ASSERT_EQ(must(alone.analysis).candidates.size(), must(g.analysis).candidates.size()) << "light " << l.tag;
    for (std::size_t k = 0; k < must(g.analysis).candidates.size(); ++k) {
      const candidate_analysis& x = must(alone.analysis).candidates[k];
      const candidate_analysis& y = must(g.analysis).candidates[k];
      EXPECT_EQ(x.role, y.role);
      EXPECT_EQ(x.status, y.status);
      ASSERT_EQ(x.chains.size(), y.chains.size());
      for (std::size_t c = 0; c < x.chains.size(); ++c) {
        EXPECT_EQ(x.chains[c].chain, y.chains[c].chain);
        EXPECT_EQ(x.chains[c].status, y.chains[c].status) << x.chains[c].chain;
        EXPECT_EQ(x.chains[c].reason.value_or(""), y.chains[c].reason.value_or("")) << x.chains[c].chain;
      }
    }
    EXPECT_EQ(must(alone.analysis).default_recipe.light, must(g.analysis).default_recipe.light) << "light " << l.tag;
    EXPECT_EQ(must(alone.analysis).default_recipe.flat, must(g.analysis).default_recipe.flat) << "light " << l.tag;
  }
}

TEST(BatchAnalysis, T_M3A_04c_duplicate_digests_agree_or_conflict) {
  const baseline b;
  {  // copie concordi (stesso ruolo e dichiarazioni): un solo input con l'elenco degli input_id
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(make_input(6, 'b', "bias", b.bias, bias_desc()));
    const batch_proposal p = analyze(in);
    const group_proposal& g = only_group(p);
    EXPECT_TRUE(p.excluded.empty());
    const role_choice* bias = choice_of(g, master_role::bias);
    ASSERT_NE(bias, nullptr);
    EXPECT_EQ(bias->kind, choice_kind::only_compatible);
    EXPECT_EQ(bias->eligible, std::vector<std::string>{std::string(64, 'b')});
  }
  {  // light duplicato e concorde: un solo membro con due input_id
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(make_input(7, 'a', "light", b.light));
    const group_proposal g = only_group(analyze(in));
    ASSERT_EQ(g.members.size(), 1u);
    EXPECT_EQ(g.members[0].input_ids, (std::vector<std::uint64_t>{1, 7}));
  }
  {  // ruoli diversi: tutte le copie escluse
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(make_input(6, 'b', "dark", b.bias, dark_desc(master_role::dark, additive_state::bias_included)));
    const batch_proposal p = analyze(in);
    const excluded_input* e = excluded_of(p, 'b');
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->reason, "duplicate_digest_conflict");
    EXPECT_EQ(e->input_ids, (std::vector<std::uint64_t>{2, 6}));
    EXPECT_EQ(report_of(only_group(p), master_role::bias), nullptr);
  }
  {  // stesso ruolo, dichiarazioni diverse
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(make_input(6, 'b', "bias", b.bias, bias_desc(), declarations({{"binning_x", json::uinteger(1)}})));
    const batch_proposal p = analyze(in);
    const excluded_input* e = excluded_of(p, 'b');
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->reason, "duplicate_digest_conflict");
  }
}

TEST(BatchAnalysis, T_M3A_04c_registration_order_does_not_change_the_canonical_proposal) {
  const baseline b;
  std::vector<batch_input> in = inputs_of(b);
  in.push_back(make_input(6, 'c', "light", [] { image i = frame_image(kLightExp); i.metadata.filter = hs("R", "FILTER"); return i; }()));
  in.push_back(make_input(7, 'b', "dark", b.bias, dark_desc(master_role::dark, additive_state::bias_included)));  // conflitto
  const std::string reference = analyze(in).canonical();
  EXPECT_NE(reference.find("astro.batch_proposal.v1"), std::string::npos);
  std::vector<batch_input> permuted = in;
  std::set<std::string> seen;
  for (int round = 0; round < 6; ++round) {
    std::rotate(permuted.begin(), permuted.begin() + 1, permuted.end());
    if (round % 2 == 1) std::reverse(permuted.begin(), permuted.end());
    // gli input_id seguono l'ordine di registrazione: cambiano con la permutazione
    std::uint64_t next = 100;
    std::vector<batch_input> renumbered = permuted;
    for (batch_input& x : renumbered) x.input_id = next++;
    EXPECT_EQ(analyze(renumbered).canonical(), reference) << "permutazione " << round;
  }
}

// ---------------------------------------------------------------- T-M3A-04 d: candidati e scelta

TEST(BatchAnalysis, T_M3A_04d_zero_or_one_candidate_per_role) {
  const baseline b;
  {  // nessun candidato per il dark-flat: ruolo assente (P-DF-ABS)
    std::vector<batch_input> in = inputs_of(b);
    in.erase(in.begin() + 3);
    const group_proposal g = only_group(analyze(in));
    const role_choice* c = choice_of(g, master_role::dark_flat);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::absent);
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(must(g.analysis).predicates.df, dark_flat_predicate::absent);
  }
  {  // due candidati, entrambi esclusi con motivi diversi: ruolo presente ed escluso (P-DF-BAD), col motivo del primo
     // in ordine canonico (digest minimo: 'c', gain); l'altro compare `excluded` col suo motivo
    baseline x;
    std::vector<batch_input> in = inputs_of(x);  // 'e': esposizione fuori tolleranza col flat
    in[3] = make_input(4, 'e', "dark_flat", [&] { image i = x.dark_flat; i.metadata.exposure_s = hd(101.2, "EXPTIME"); return i; }(),
                       dark_desc(master_role::dark_flat, additive_state::bias_included));
    in.push_back(make_input(6, 'c', "dark_flat", [&] { image i = x.dark_flat; i.metadata.gain = hd(101.0, "GAIN"); return i; }(),
                            dark_desc(master_role::dark_flat, additive_state::bias_included)));
    const group_proposal g = only_group(analyze(in));
    const role_choice* c = choice_of(g, master_role::dark_flat);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::excluded_only);
    EXPECT_TRUE(c->eligible.empty());
    std::map<std::string, std::string> reasons;
    for (const candidate_report& r : g.candidates) {
      if (r.role == master_role::dark_flat) {
        EXPECT_EQ(r.status, candidate_status::excluded);
        reasons[r.key] = r.reason;
      }
    }
    EXPECT_EQ(reasons.size(), 2u);
    EXPECT_EQ(reasons[std::string(64, 'c')], "incompatible.gain");
    EXPECT_EQ(reasons[std::string(64, 'e')], "incompatible.exposure_s");
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(must(g.analysis).predicates.df, dark_flat_predicate::bad);
    const candidate_analysis* first = nullptr;
    std::size_t per_role = 0;
    for (const candidate_analysis& a : must(g.analysis).candidates) {
      if (a.role == master_role::dark_flat) {
        first = &a;
        ++per_role;
      }
    }
    ASSERT_EQ(per_role, 1u) << "l'analisi ha un solo candidato per ruolo: quello canonico";
    ASSERT_NE(first, nullptr);
    ASSERT_FALSE(first->chains.empty());
    for (const chain_verdict& v : first->chains) {
      EXPECT_EQ(v.reason.value_or(""), "incompatible.gain") << "il motivo del ruolo è quello del primo candidato canonico";
    }
  }
}

// Due dark compatibili ('d' e 'c'), per il ruolo ambiguo.
std::vector<batch_input> two_darks() {
  const baseline b;
  std::vector<batch_input> in = inputs_of(b);
  image second = b.dark;
  second.metadata.temperature_c = hd(-10.5, "CCD-TEMP");  // dentro la tolleranza, immagine diversa
  in.push_back(make_input(6, 'c', "dark", second, dark_desc(master_role::dark, additive_state::bias_included)));
  return in;
}

TEST(BatchAnalysis, T_M3A_04d_two_eligible_candidates_require_the_user_choice) {
  const batch_proposal p = analyze(two_darks());
  const group_proposal& g = only_group(p);
  EXPECT_EQ(g.outcome, group_outcome::candidate_choice_required);
  const role_choice* c = choice_of(g, master_role::dark);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->kind, choice_kind::required);
  EXPECT_EQ(c->eligible, (std::vector<std::string>{std::string(64, 'c'), std::string(64, 'd')}));
  EXPECT_FALSE(c->chosen.has_value());
  EXPECT_FALSE(g.analysis.has_value()) << "nessuna ricetta né default finché l'utente non sceglie";
  // Gli altri ruoli, con un solo idoneo, non richiedono scelta.
  EXPECT_EQ(must_choice(g, master_role::bias).kind, choice_kind::only_compatible);
  // La selezione richiesta senza scelta: classify.choice_required.
  EXPECT_STOP(select_recipe(g, chain_recipe{}), error_category::invalid_input, "classify.choice_required");
}

TEST(BatchAnalysis, T_M3A_04d_valid_choice_matches_the_proposal_with_that_candidate_alone) {
  const std::vector<batch_input> in = two_darks();
  const group_proposal chosen = only_group(analyze(in, request({{master_role::dark, std::string(64, 'c')}})));
  ASSERT_EQ(chosen.outcome, group_outcome::ready);
  EXPECT_EQ(*must_choice(chosen, master_role::dark).chosen, std::string(64, 'c'));
  // L'oracle è la proposta dello stesso batch senza l'altro dark.
  std::vector<batch_input> alone = in;
  alone.erase(alone.begin() + 2);  // 'd'
  const group_proposal expected = only_group(analyze(alone));
  ASSERT_TRUE(chosen.analysis.has_value());
  ASSERT_TRUE(expected.analysis.has_value());
  EXPECT_EQ(admissible_of(must(chosen.analysis)), admissible_of(must(expected.analysis)));
  EXPECT_EQ(must(chosen.analysis).default_recipe.light, must(expected.analysis).default_recipe.light);
  EXPECT_EQ(must(chosen.analysis).default_recipe.flat, must(expected.analysis).default_recipe.flat);
}

TEST(BatchAnalysis, T_M3A_04d_choosing_an_ineligible_candidate_of_an_ambiguous_role_shows_it_excluded) {
  // Tre dark: 'c' e 'd' idonei (ruolo ambiguo), '9' escluso per il gain. L'utente sceglie '9'.
  std::vector<batch_input> in = two_darks();
  in.push_back(make_input(7, '9', "dark", [] { image i = frame_image(kLightExp); i.metadata.gain = hd(101.0, "GAIN"); return i; }(),
                          dark_desc(master_role::dark, additive_state::bias_included)));
  const group_proposal open = only_group(analyze(in));
  EXPECT_EQ(open.outcome, group_outcome::candidate_choice_required);
  EXPECT_EQ(must_choice(open, master_role::dark).eligible, (std::vector<std::string>{std::string(64, 'c'), std::string(64, 'd')}));
  const group_proposal chosen = only_group(analyze(in, request({{master_role::dark, std::string(64, '9')}})));
  ASSERT_EQ(chosen.outcome, group_outcome::ready);
  EXPECT_EQ(*must_choice(chosen, master_role::dark).chosen, std::string(64, '9'));
  bool shown = false;
  for (const candidate_report& c : chosen.candidates) {
    if (c.role == master_role::dark && c.key == std::string(64, '9')) {
      shown = true;
      EXPECT_EQ(c.status, candidate_status::excluded);
      EXPECT_EQ(c.reason, "incompatible.gain");
    }
  }
  EXPECT_TRUE(shown);
  ASSERT_TRUE(chosen.analysis.has_value());
  expect_role_unused(must(chosen.analysis), master_role::dark);
}

TEST(BatchAnalysis, T_M3A_04d_choice_is_not_persisted_a_fresh_analysis_needs_the_same_choice_again) {
  // La scelta è un ingresso, non uno stato (CL-08): ricalcolare da zero (come dopo una riapertura) senza scelta dà di nuovo
  // `candidate_choice_required`; con la stessa scelta la proposta è byte-identica. La riapertura del progetto su store è
  // in T_M3A_04d_reopening_the_project_after_a_choice_on_an_ambiguous_group_needs_the_choice_again.
  const std::vector<batch_input> in = two_darks();
  const candidate_choices choice = {{master_role::dark, std::string(64, 'c')}};
  const std::string first = analyze(in, request(choice)).canonical();
  EXPECT_EQ(analyze(in).groups.at(0).outcome, group_outcome::candidate_choice_required);
  EXPECT_EQ(analyze(in, request(choice)).canonical(), first);
}

TEST(BatchAnalysis, T_M3A_04d_unknown_candidate_is_rejected_and_none_is_not_allowed) {
  const std::vector<batch_input> in = two_darks();
  EXPECT_STOP(analyze(in, request({{master_role::dark, std::string(64, '9')}})), error_category::invalid_input,
              "classify.choice_unknown_candidate");
  // Un candidato di un altro ruolo non è un candidato del ruolo.
  EXPECT_STOP(analyze(in, request({{master_role::dark, std::string(64, 'f')}})), error_category::invalid_input,
              "classify.choice_unknown_candidate");
  EXPECT_STOP(analyze(in, request({{master_role::dark, ""}})), error_category::invalid_input,
              "classify.choice_unknown_candidate");
}

TEST(BatchAnalysis, T_M3A_04d_dark_flat_eligibility_follows_the_candidate_flat_even_when_excluded) {
  const baseline b;
  auto with_df = [&](double exposure_second) {
    std::vector<batch_input> in = inputs_of(b);
    image second = b.dark_flat;
    second.metadata.exposure_s = hd(exposure_second, "EXPTIME");
    in.push_back(make_input(6, 'c', "dark_flat", second, dark_desc(master_role::dark_flat, additive_state::bias_included)));
    return in;
  };
  {  // due dark-flat, uno solo compatibile col flat: un solo idoneo
    const group_proposal g = only_group(analyze(with_df(101.2)));
    EXPECT_EQ(must_choice(g, master_role::dark_flat).kind, choice_kind::only_compatible);
    EXPECT_EQ(must_choice(g, master_role::dark_flat).eligible, std::vector<std::string>{std::string(64, 'e')});
    EXPECT_EQ(g.outcome, group_outcome::ready);
    bool found = false;
    for (const candidate_report& c : g.candidates) {
      if (c.role == master_role::dark_flat && c.key == std::string(64, 'c')) {
        found = true;
        EXPECT_EQ(c.status, candidate_status::excluded);
        EXPECT_EQ(c.reason, "incompatible.exposure_s");
      }
    }
    EXPECT_TRUE(found);
  }
  {  // flat escluso (filtro) con un dark-flat incompatibile e uno compatibile: il confronto è col flat escluso
    std::vector<batch_input> in = with_df(101.2);
    in[4] = make_input(5, 'f', "flat", [&] { image i = b.flat; i.metadata.filter = hs("R", "FILTER"); return i; }(), flat_desc());
    const group_proposal g = only_group(analyze(in));
    EXPECT_EQ(must_report(g, master_role::flat).status, candidate_status::excluded);
    EXPECT_EQ(must_choice(g, master_role::dark_flat).eligible, std::vector<std::string>{std::string(64, 'e')});
    for (const candidate_report& c : g.candidates) {
      if (c.role == master_role::dark_flat && c.key == std::string(64, 'c')) {
        EXPECT_EQ(c.reason, "incompatible.exposure_s");
      }
    }
  }
  {  // senza alcun flat candidato basta lo stato: entrambi idonei, scelta richiesta
    std::vector<batch_input> in = with_df(101.2);
    in.erase(in.begin() + 4);
    const group_proposal g = only_group(analyze(in));
    EXPECT_EQ(must_choice(g, master_role::dark_flat).kind, choice_kind::required);
    EXPECT_EQ(g.outcome, group_outcome::candidate_choice_required);
  }
}

TEST(BatchAnalysis, T_M3A_04d_bias_is_eligible_through_the_flat_pair_alone) {
  baseline b;
  b.bias.metadata.temperature_c = hd(-9.0, "CCD-TEMP");
  b.light.metadata.temperature_c = hd(-12.0, "CCD-TEMP");  // L-B fuori tolleranza, F-B dentro
  const group_proposal g = only_group(analyze(inputs_of(b)));
  const role_choice* c = choice_of(g, master_role::bias);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->kind, choice_kind::only_compatible) << "F-B compatibile con un flat idoneo";
  EXPECT_EQ(c->eligible, std::vector<std::string>{std::string(64, 'b')});
}

TEST(BatchAnalysis, T_M3A_04d_external_masters_and_current_slots_are_both_candidates_and_raw_light_has_none) {
  const baseline b;
  {  // master esterno e slot `current` dello stesso ruolo: due candidati
    std::vector<batch_input> in = inputs_of(b);
    slot_candidate slot;
    slot.node_key = std::string(64, '7');
    slot.role = master_role::dark;
    slot.descriptor = dark_desc(master_role::dark, additive_state::bias_included);
    slot.observation = make_observation(b.dark, declarations());
    const group_proposal g = only_group(analyze(in, request(), {slot}));
    const role_choice* c = choice_of(g, master_role::dark);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::required);
    EXPECT_EQ(c->eligible, (std::vector<std::string>{std::string(64, '7'), std::string(64, 'd')}));
    const group_proposal sel = only_group(analyze(in, request({{master_role::dark, std::string(64, '7')}}), {slot}));
    EXPECT_EQ(sel.outcome, group_outcome::ready);
  }
  {  // master esterno con osservazione RAW: escluso per l'origine (`decoder_state_mixed`), anche se porta stati di frame
    const decoder_state s = base_state();
    std::vector<batch_input> in = inputs_of(b);
    in[0] = make_input(1, 'a', "light", raw_image(kLightExp, s), std::nullopt, raw_decl());
    in[2] = make_input(3, 'd', "dark", raw_image(kLightExp, s),
                       [&] { master_descriptor d = dark_desc(master_role::dark, additive_state::bias_included);
                             d.frame_decoder_states = std::vector<frame_decoder_state>{frame_decoder_state{s, identity_a()}};
                             return d; }(),
                       raw_decl());
    const group_proposal g = only_group(analyze(in));
    EXPECT_EQ(must_report(g, master_role::dark).status, candidate_status::excluded);
    EXPECT_EQ(must_report(g, master_role::dark).reason, "decoder_state_mixed");
  }
  {  // light RAW con soli master esterni da FITS: decoder_state_mixed, proposta light.none + flat.none
    const decoder_state s = base_state();
    std::vector<batch_input> in = inputs_of(b);
    in[0] = make_input(1, 'a', "light", raw_image(kLightExp, s), std::nullopt, raw_decl());
    const group_proposal g = only_group(analyze(in));
    ASSERT_EQ(g.outcome, group_outcome::ready);
    for (master_role r : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
      EXPECT_EQ(must_report(g, r).status, candidate_status::excluded) << role_name(r);
      EXPECT_EQ(must_report(g, r).reason, "decoder_state_mixed") << role_name(r);
    }
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(must(g.analysis).default_recipe.light, L::none);
    EXPECT_EQ(must(g.analysis).default_recipe.flat, F::none);
  }
}

// ---------------------------------------------------------------- T-M3A-04 e: ogni ricetta proposta si seleziona

// Ogni configurazione di 04a–04d: batch, slot e richiesta.
struct configuration {
  std::string name;
  std::vector<batch_input> inputs;
  std::vector<slot_candidate> slots;
  batch_analysis_request req;
};

std::vector<configuration> all_configurations() {
  std::vector<configuration> v;
  const baseline ok;
  v.push_back({"04a: baseline", inputs_of(ok), {}, request()});
  for (const violation& viol : violations()) {
    baseline b = viol.cfa ? cfa_baseline() : baseline{};
    viol.apply(b);
    v.push_back({std::string("04a: ") + viol.name, inputs_of(b), {}, request_for(viol)});
  }
  {  // 04a RAW: master come slot
    const decoder_state s = base_state();
    const raw_batch rb;
    v.push_back({"04a: RAW",
                 {make_input(1, 'a', "light", raw_image(kLightExp, rb.light_state), std::nullopt, rb.light_decl)},
                 {raw_slot('b', master_role::bias, raw_image(0.001, s), bias_desc(), s, rb.master_decl),
                  raw_slot('d', master_role::dark, raw_image(kLightExp, s), dark_desc(master_role::dark, additive_state::bias_included), s, rb.master_decl),
                  raw_slot('f', master_role::flat, raw_image(kFlatExp, s), flat_desc(), s, rb.master_decl)},
                 request()});
  }
  {  // 04b: declared binning/filter diversi su RAW
    const decoder_state s = base_state();
    v.push_back({"04b: RAW dichiarazioni diverse",
                 {make_input(1, 'a', "light", raw_image(kLightExp, s), std::nullopt, raw_decl())},
                 {raw_slot('d', master_role::dark, raw_image(kLightExp, s), dark_desc(master_role::dark, additive_state::bias_included), s, raw_decl(2, "R"))},
                 request()});
  }
  v.push_back({"04c: tre light", [] {
                 baseline b;
                 std::vector<batch_input> in = inputs_of(b);
                 in.push_back(make_input(6, '6', "light", [] { image i = frame_image(kLightExp); i.metadata.filter = hs("R", "FILTER"); return i; }()));
                 in.push_back(make_input(7, '7', "light", [] { image i = frame_image(kLightExp); i.metadata.exposure_s = hd(330.0, "EXPTIME"); return i; }()));
                 return in;
               }(),
               {}, request()});
  v.push_back({"04d: due dark, scelta", two_darks(), {}, request({{master_role::dark, std::string(64, 'c')}})});
  {
    baseline b;
    b.bias.metadata.temperature_c = hd(-9.0, "CCD-TEMP");
    b.light.metadata.temperature_c = hd(-12.0, "CCD-TEMP");
    v.push_back({"04d: bias solo per F-B", inputs_of(b), {}, request()});
  }
  {
    baseline b;
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(make_input(6, 'c', "dark_flat", [&] { image i = b.dark_flat; i.metadata.exposure_s = hd(101.2, "EXPTIME"); return i; }(),
                            dark_desc(master_role::dark_flat, additive_state::bias_included)));
    v.push_back({"04d: due dark-flat, uno compatibile", in, {}, request()});
  }
  {
    baseline b;
    std::vector<batch_input> in = inputs_of(b);
    in[0] = make_input(1, 'a', "light", raw_image(kLightExp, base_state()), std::nullopt, raw_decl());
    v.push_back({"04d: light RAW con master esterni", in, {}, request()});
  }
  return v;
}

bool is_partial(const chain_recipe& r) {
  return r.flat_division_offset.value_or(offset_state::removed) == offset_state::known_not_removed;
}

TEST(BatchAnalysis, T_M3A_04e_every_proposed_recipe_passes_the_selection_without_gstop) {
  for (const configuration& c : all_configurations()) {
    SCOPED_TRACE(c.name);
    const batch_proposal p = analyze(c.inputs, c.req, c.slots);
    for (const group_proposal& g : p.groups) {
      if (g.outcome != group_outcome::ready) {
        EXPECT_TRUE(g.recipes.empty()) << "nessuna ricetta per un gruppo non pronto";
        continue;
      }
      ASSERT_TRUE(g.analysis.has_value());
      ASSERT_FALSE(g.recipes.empty());
      EXPECT_EQ(g.recipes.front().light, must(g.analysis).default_recipe.light) << "il default è la prima ricetta";
      EXPECT_EQ(g.recipes.front().flat, must(g.analysis).default_recipe.flat);
      std::vector<pair_key> proposed;
      for (const chain_recipe& r : g.recipes) {
        EXPECT_NO_THROW((void)select_recipe(g, r)) << to_string(r.light) << "/" << to_string(r.flat);
        proposed.emplace_back(r.light, r.flat, is_partial(r));
      }
      std::sort(proposed.begin(), proposed.end());
      proposed.erase(std::unique(proposed.begin(), proposed.end()), proposed.end());
      EXPECT_EQ(proposed, admissible_of(must(g.analysis))) << "nessuna ricetta fuori dall'insieme ammissibile, nessuna ammissibile omessa";
    }
  }
}

TEST(BatchAnalysis, T_M3A_04e_a_recipe_outside_the_admissible_set_is_a_gstop_not_a_selection) {
  // La ricetta del batch di base (dark + dark_flat) ha parametri applicabili alla sua catena; presentata a un gruppo il cui dark
  // è escluso, l'unica ragione di stop è l'insieme ammissibile.
  const group_proposal base = only_group(analyze(inputs_of(baseline{})));
  ASSERT_FALSE(base.recipes.empty());
  const chain_recipe r = base.recipes.front();
  ASSERT_EQ(r.light, L::dark);
  ASSERT_EQ(r.flat, F::dark_flat);
  baseline b;
  b.dark.metadata.exposure_s = hd(303.1, "EXPTIME");  // dark escluso
  const group_proposal g = only_group(analyze(inputs_of(b)));
  try {
    (void)select_recipe(g, r);
    ADD_FAILURE() << "ricetta con il dark escluso selezionata";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::invalid_input) << e.what();
    EXPECT_EQ(e.code(), "calib.recipe_not_admissible") << e.what();
    bool has_reason = false;
    for (const auto& [k, v] : e.context()) has_reason |= k == "reason" && !v.empty();
    EXPECT_TRUE(has_reason) << "il motivo è nel contesto: " << e.what();
  }
}

TEST(BatchAnalysis, T_M3A_04e_omissions_of_the_default_carry_the_ledger_reason_for_every_role) {
  const auto omission = [](const group_proposal& g, master_role role) -> std::optional<std::string> {
    for (const role_omission& o : g.omissions) {
      if (o.role == role) return o.reason;
    }
    return std::nullopt;
  };
  const baseline b;
  // Oracle per ruolo, ricavato a mano da CL-10 (campo che esclude), ADR-16.b §b.6 (riga DFB e default) e §b.10 (motivo):
  // - bias: nella baseline il dark è `bias_included` e il default 111 è `light.dark` con il bias `not_applicable`
  //   (`bias_in_matched_dark`) e `flat.dark_flat` (INC, senza bias): il bias non è un'omissione. Le sue omissioni si provano
  //   senza dark, dove il default 011 è `light.bias` + `flat.dark_flat`. Senza bias la riga diventa 010 (`master_absent`); con
  //   il bias escluso dal gain (riga «gain» di CL-10: L–B e F–B, quindi escluso in tutte le catene) resta 010
  //   (`master_excluded`);
  // - dark: escluso dal gain (riga «gain»: L–D), la riga 111 diventa 011 e `light.dark` non è applicato (`master_excluded`);
  // - dark-flat: escluso dal gain (riga «gain»: F–DF), P-DF-BAD; la riga 111 con `D` e flat `raw` con BAD dà `flat.none`
  //   (nessuna dichiarazione termica) e `flat.dark_flat` non è applicato (`master_excluded`);
  // - flat: la riga «gain» di CL-10 non confronta L–F, quindi un flat a gain diverso resta `usable`. Lo esclude la riga
  //   «filtro» (L–F); la riga diventa 101 e il default del flat è `flat.none` (`master_excluded`).
  struct row {
    master_role role;
    bool without_dark;
    std::function<void(image&)> break_it;
    L light;  // default senza il ruolo e con il ruolo escluso: la stessa riga DFB
    F flat;
  };
  const std::vector<row> rows = {
      {master_role::bias, true, [](image& i) { i.metadata.gain = hd(101.0, "GAIN"); }, L::none, F::none},
      {master_role::dark, false, [](image& i) { i.metadata.gain = hd(101.0, "GAIN"); }, L::bias, F::dark_flat},
      {master_role::dark_flat, false, [](image& i) { i.metadata.gain = hd(101.0, "GAIN"); }, L::dark, F::none},
      {master_role::flat, false, [](image& i) { i.metadata.filter = hs("R", "FILTER"); }, L::dark, F::none},
  };
  const auto expect_default = [](const group_proposal& g, L light, F flat) {
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(must(g.analysis).default_recipe.light, light);
    EXPECT_EQ(must(g.analysis).default_recipe.flat, flat);
  };
  const auto index_of = [](const std::vector<batch_input>& in, master_role role) {
    const char* name = role_name(role);
    for (std::size_t k = 0; k < in.size(); ++k) {
      if (in[k].role == name) return k;
    }
    throw std::runtime_error("ruolo assente dal batch di prova");
  };
  for (const row& r : rows) {
    SCOPED_TRACE(role_name(r.role));
    std::vector<batch_input> base_in = inputs_of(b);
    if (r.without_dark) {
      base_in.erase(base_in.begin() + static_cast<std::ptrdiff_t>(index_of(base_in, master_role::dark)));
      const group_proposal g011 = only_group(analyze(base_in));
      expect_default(g011, L::bias, F::dark_flat);  // riga 011: il default usa il bias
      EXPECT_FALSE(omission(g011, master_role::bias).has_value());
    }
    {  // nessun candidato: ledger `master_absent`
      std::vector<batch_input> in = base_in;
      in.erase(in.begin() + static_cast<std::ptrdiff_t>(index_of(in, r.role)));
      const group_proposal g = only_group(analyze(in));
      expect_default(g, r.light, r.flat);
      EXPECT_EQ(omission(g, r.role), std::optional<std::string>("master_absent"));
    }
    {  // un solo candidato, escluso: ledger `master_excluded`
      std::vector<batch_input> in = base_in;
      const std::size_t at = index_of(in, r.role);
      image bad = r.role == master_role::bias ? b.bias : r.role == master_role::dark ? b.dark : r.role == master_role::dark_flat ? b.dark_flat : b.flat;
      r.break_it(bad);
      in[at] = make_input(in[at].input_id, in[at].digest.sha256[0], in[at].role, bad, in[at].descriptor);
      const group_proposal g = only_group(analyze(in));
      expect_default(g, r.light, r.flat);
      EXPECT_EQ(omission(g, r.role), std::optional<std::string>("master_excluded"));
    }
  }
  // Baseline (riga 111, `D` `bias_included`, flat `raw` con INC): default `light.dark` + `flat.dark_flat`. Dark, dark-flat e
  // flat sono applicati; il bias è `not_applicable`: nel light `bias_in_matched_dark` (riga 111), nel flat la catena INC
  // contiene già l'offset e `F − B − DF` non esiste (§b.5, nessuna doppia sottrazione). Nessuna omissione.
  const group_proposal g = only_group(analyze(inputs_of(b)));
  ASSERT_TRUE(g.analysis.has_value());
  EXPECT_EQ(must(g.analysis).default_recipe.light, L::dark);
  EXPECT_EQ(must(g.analysis).default_recipe.flat, F::dark_flat);
  for (master_role role : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    EXPECT_FALSE(omission(g, role).has_value()) << role_name(role);
  }
}

// ---------------------------------------------------------------- T-M3A-04 f: purezza e parametri

TEST(BatchAnalysis, T_M3A_04f_missing_analysis_parameters_one_at_a_time) {
  struct row {
    const char* name;
    std::function<void(calibration_declarations&)> remove;
    const char* expected;
  };
  const std::vector<row> rows = {
      {"exposure_rel_tol", [](calibration_declarations& p) { p.exposure_rel_tol.reset(); }, "exposure_rel_tol"},
      {"temperature_abs_tol_c", [](calibration_declarations& p) { p.temperature_abs_tol_c.reset(); }, "temperature_abs_tol_c"},
      {"temperature_policy", [](calibration_declarations& p) { p.temp_policy.reset(); }, "temperature_policy"},
  };
  const baseline b;
  for (const row& r : rows) {
    SCOPED_TRACE(r.name);
    batch_analysis_request req = request();
    r.remove(req.params);
    const group_proposal g = only_group(analyze(inputs_of(b), req));
    EXPECT_EQ(g.outcome, group_outcome::params_missing);
    EXPECT_EQ(g.missing_params, std::vector<std::string>{r.expected});
    EXPECT_FALSE(g.analysis.has_value()) << "nessuna ricetta e nessun default";
  }
  {  // light RAW: manca anche decoder_black_abs_tol_adu
    batch_analysis_request req = request();
    req.params.decoder_black_abs_tol_adu.reset();
    const group_proposal g = only_group(analyze_raw(raw_batch{}, req));
    EXPECT_EQ(g.outcome, group_outcome::params_missing);
    EXPECT_EQ(g.missing_params, std::vector<std::string>{"decoder_black_abs_tol_adu"});
    EXPECT_FALSE(g.analysis.has_value());
  }
}

// ADR-16 §b.9: `temperature_abs_tol_c` e' obbligatorio con entrambe le `temperature_policy`.
TEST(BatchAnalysis, T_M3A_04f_temperature_tolerance_is_required_with_not_applicable_declared) {
  const baseline b;
  {
    batch_analysis_request req = request();
    req.params.temp_policy = temperature_policy::not_applicable_declared;
    req.params.temperature_abs_tol_c.reset();
    const group_proposal g = only_group(analyze(inputs_of(b), req));
    EXPECT_EQ(g.outcome, group_outcome::params_missing);
    EXPECT_EQ(g.missing_params, std::vector<std::string>{"temperature_abs_tol_c"});
    EXPECT_FALSE(g.analysis.has_value());
  }
  {
    batch_analysis_request req = request();
    req.params.temp_policy = temperature_policy::not_applicable_declared;
    const group_proposal g = only_group(analyze(inputs_of(b), req));
    EXPECT_NE(g.outcome, group_outcome::params_missing);
    EXPECT_TRUE(g.missing_params.empty());
  }
}

TEST(BatchAnalysis, T_M3A_04f_samples_are_read_only_for_the_thermal_check) {
  int sample_reads = 0;
  int pair_checks = 0;
  const fault_injector counter = [&](std::string_view point) {
    if (point == "calib.analysis.samples") ++sample_reads;
    if (point == "calib.analysis.pair") ++pair_checks;
    return 0;
  };
  const baseline b;
  const batch_proposal p = analyze_batch(inputs_of(b), {}, request(), counter);
  EXPECT_EQ(only_group(p).outcome, group_outcome::ready);
  EXPECT_GT(pair_checks, 0) << "controllo positivo: il contatore vede le coppie";
  EXPECT_EQ(sample_reads, 0) << "senza dichiarazioni termiche del flat nessun campione si legge";
}

void fill(image& im, double value) {
  double* p = im.data<double>();
  for (std::uint64_t i = 0; i < im.geometry.sample_count(); ++i) p[i] = value;
}

TEST(BatchAnalysis, T_M3A_04f_thermal_declarations_of_the_flat_are_the_only_reason_to_read_samples) {
  int sample_reads = 0;
  const fault_injector counter = [&](std::string_view point) {
    if (point == "calib.analysis.samples") ++sample_reads;
    return 0;
  };
  // Configurazione di U7 per `flat.bias` (riga 3): flat raw senza dark-flat, F = 1124, B = 100, q_min = 0,5, bound 64 e
  // budget 0,25 (nel budget).
  baseline b;
  fill(b.flat, 1124.0);
  fill(b.bias, 100.0);
  std::vector<batch_input> in = inputs_of(b);
  in.erase(in.begin() + 3);  // niente dark-flat
  batch_analysis_request thermal = request();
  thermal.params.normalization = normalization_mode::global_median;
  thermal.params.q_min = 0.5;
  thermal.params.q_rel_sigma_max = 10.0;
  thermal.params.flat_thermal_bound_adu = 64.0;
  thermal.params.flat_thermal_rel_budget = 0.25;
  (void)analyze_batch(in, {}, thermal, counter);
  EXPECT_GT(sample_reads, 0) << "con le dichiarazioni termiche il controllo di flat.bias legge campioni";
  sample_reads = 0;
  (void)analyze_batch(in, {}, request(), counter);
  EXPECT_EQ(sample_reads, 0) << "senza dichiarazioni termiche nessuna lettura";
}

// ---------------------------------------------------------------- T-M3A-05 c: ruolo cambiato dall'utente

TEST(BatchAnalysis, T_M3A_05c_changed_role_moves_the_input_between_sets) {
  const baseline b;
  {  // dark -> flat: il dark sparisce dal ruolo dark ed entra fra i candidati flat
    std::vector<batch_input> in = inputs_of(b);
    in[2].role = "flat";
    in[2].descriptor = flat_desc();
    const group_proposal g = only_group(analyze(in));
    EXPECT_EQ(must_choice(g, master_role::dark).kind, choice_kind::absent);
    bool dark_as_flat = false;
    for (const candidate_report& c : g.candidates) dark_as_flat |= c.role == master_role::flat && c.key == std::string(64, 'd');
    EXPECT_TRUE(dark_as_flat);
    // Nuova analisi uguale all'oracle: due flat idonei (stessa geometria e filtro del light), scelta richiesta.
    EXPECT_EQ(g.outcome, group_outcome::candidate_choice_required);
    EXPECT_EQ(must_choice(g, master_role::flat).eligible, (std::vector<std::string>{std::string(64, 'd'), std::string(64, 'f')}));
    EXPECT_FALSE(g.analysis.has_value());
  }
  {  // light -> unclassified: esce dai gruppi, `unclassified.user`
    std::vector<batch_input> in = inputs_of(b);
    in[0].role = "unclassified";
    in[0].unclassified_reason = "user";
    const batch_proposal p = analyze(in);
    const excluded_input* e = excluded_of(p, 'a');
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->reason, "unclassified.user");
    EXPECT_TRUE(p.groups.empty());
  }
  {  // frame di un ruolo senza descrittore: insieme candidato di frame, `frame_pending_master`
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(make_input(6, 'c', "dark", b.dark));
    const batch_proposal p = analyze(in);
    ASSERT_EQ(p.frame_pending_master.count(master_role::dark), 1u);
    EXPECT_EQ(p.frame_pending_master.at(master_role::dark), std::vector<std::string>{std::string(64, 'c')});
    EXPECT_EQ(must_choice(only_group(p), master_role::dark).eligible, std::vector<std::string>{std::string(64, 'd')});
  }
}

TEST(BatchAnalysis, T_M3A_05c_user_role_in_conflict_with_a_recognized_imagetyp_is_excluded_not_overridden) {
  baseline b;
  {  // master: IMAGETYP riconosciuto `flat`, ruolo utente `dark`
    b.dark.metadata.imagetyp = hs("Flat Field", "IMAGETYP");
    const group_proposal g = only_group(analyze(inputs_of(b)));
    const candidate_report* c = report_of(g, master_role::dark);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->status, candidate_status::excluded);
    EXPECT_EQ(c->reason, "imagetyp_conflict");
  }
  {  // light: IMAGETYP `Dark Frame` con ruolo `light`: gruppo light_blocked
    baseline l;
    l.light.metadata.imagetyp = hs("Dark Frame", "IMAGETYP");
    const group_proposal g = only_group(analyze(inputs_of(l)));
    EXPECT_EQ(g.outcome, group_outcome::light_blocked);
    EXPECT_EQ(g.block_category, error_category::incompatible);
    EXPECT_EQ(g.block_code, "calib.imagetyp_conflict");
    EXPECT_FALSE(g.analysis.has_value());
  }
  {  // IMAGETYP di dark con ruolo dark_flat: accettato
    baseline d;
    d.dark_flat.metadata.imagetyp = hs("Dark Frame", "IMAGETYP");
    const group_proposal g = only_group(analyze(inputs_of(d)));
    EXPECT_EQ(must_report(g, master_role::dark_flat).status, candidate_status::usable);
  }
}

// ---------------------------------------------------------------- T-M3A-05 d: prestacked e RGB

TEST(BatchAnalysis, T_M3A_05d_prestacked_and_rgb_lights_are_blocked_without_recipes) {
  const baseline b;
  const auto blocked = [&](const image& light, const char* code, error_category cat) {
    std::vector<batch_input> in = inputs_of(b);
    in[0] = make_input(1, 'a', "light", light);
    const group_proposal g = only_group(analyze(in));
    EXPECT_EQ(g.outcome, group_outcome::light_blocked) << code;
    EXPECT_EQ(g.block_code, code);
    EXPECT_EQ(g.block_category, cat);
    EXPECT_FALSE(g.analysis.has_value()) << "nessuna ricetta";
  };
  image mono_pre = frame_image(kLightExp);
  mono_pre.phase = metadata_value<process_phase>{process_phase::prestacked, {metadata_origin::header, "ASTRPHAS"}};
  blocked(mono_pre, "op.phase_blocked", error_category::incompatible);
  image rgb_pre = frame_image(kLightExp, plane_layout::rgb);
  rgb_pre.phase = metadata_value<process_phase>{process_phase::prestacked, {metadata_origin::header, "ASTRPHAS"}};
  blocked(rgb_pre, "op.phase_blocked", error_category::incompatible);
  blocked(frame_image(kLightExp, plane_layout::rgb), "calib.plane_layout", error_category::unsupported);
}

TEST(BatchAnalysis, T_M3A_05d_prestacked_and_rgb_masters_are_excluded_with_phase_or_layout) {
  const auto reason_of = [](master_role role, const image& img) {
    const baseline b;
    std::vector<batch_input> in = inputs_of(b);
    const std::size_t at = role == master_role::bias ? 1 : role == master_role::dark ? 2 : role == master_role::dark_flat ? 3 : 4;
    const master_descriptor d = *in[at].descriptor;
    in[at] = make_input(in[at].input_id, in[at].digest.sha256[0], in[at].role, img, d);
    return must_report(only_group(analyze(in)), role).reason;
  };
  for (master_role r : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    image pre = frame_image(r == master_role::bias ? 0.001 : r == master_role::dark ? kLightExp : kFlatExp);
    pre.phase = metadata_value<process_phase>{process_phase::prestacked, {metadata_origin::header, "ASTRPHAS"}};
    EXPECT_EQ(reason_of(r, pre), "phase") << role_name(r);
    image rgb = frame_image(r == master_role::bias ? 0.001 : r == master_role::dark ? kLightExp : kFlatExp, plane_layout::rgb);
    EXPECT_EQ(reason_of(r, rgb), "layout") << role_name(r);
  }
}

// ---------------------------------------------------------------- store: purezza e nuova analisi dopo una correzione

fs::path fresh(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "u8b" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

const cancel_token& never() {
  static const cancel_token token;
  return token;
}

std::string write_light(const fs::path& path, double exposure, const std::string& filter = "L") {
  fx::calib_header h;
  h.exposure = exposure;
  h.filter = filter;
  std::vector<std::string> cards;
  for (const std::string& c : fx::calib_cards(h)) cards.push_back(c);
  imp::gen g;
  g.bitpix = 16;
  g.w = 8;
  g.h = 8;
  g.cards = cards;
  g.value = [exposure](std::uint32_t, std::uint32_t x, std::uint32_t y) { return 100.0 + x + 8 * y + exposure; };
  imp::write_file(path, imp::make_fits(g));
  return path.string();
}

// Master dark esterno su FITS: `IMAGETYP = 'Dark Frame'` (ruolo `dark` di ADR-16.a, nessun conflitto con la revisione
// dell'utente), header del light salvo il tipo, campioni a 32 bit in virgola mobile; `seed` cambia i campioni e il digest.
std::string write_dark(const fs::path& path, double exposure, std::uint32_t seed) {
  fx::calib_header h;
  h.imagetyp = "Dark Frame";
  h.exposure = exposure;
  std::vector<std::string> cards;
  for (const std::string& c : fx::calib_cards(h)) cards.push_back(c);
  imp::gen g;
  g.bitpix = -32;
  g.w = 8;
  g.h = 8;
  g.cards = cards;
  g.value = [seed](std::uint32_t, std::uint32_t x, std::uint32_t y) { return 20.0 + x + 8 * y + seed; };
  imp::write_file(path, imp::make_fits(g));
  return path.string();
}

json params_doc() {
  json doc = json::object();
  doc.set("format", json::string("astro.batch_analysis_params.v1"));
  doc.set("exposure_rel_tol", json::number(0.01));
  doc.set("temperature_abs_tol_c", json::number(1.0));
  doc.set("temperature_policy", json::string("compare"));
  doc.set("decoder_black_abs_tol_adu", json::uinteger(kBlackTol));
  doc.set("normalization", json::string("global_median"));
  doc.set("q_min", json::number(0.5));
  doc.set("q_rel_sigma_max", json::number(10.0));
  return doc;
}

struct fixture {
  fs::path project;
  std::unique_ptr<sqlite_dag_store> store;
  std::vector<std::uint64_t> ids;
};

fixture light_store(const std::string& name) {
  const fs::path dir = fresh(name);
  fixture f;
  f.project = dir / "project";
  f.store = sqlite_dag_store::create(f.project);
  f.store->put_setting("batch.analysis_params", canonical_json(params_doc()));
  memory_pool pool(64 * imp::MiB);
  const auto res = import_batch(*f.store, {write_light(dir / "a.fits", 60), write_light(dir / "b.fits", 61),
                                           write_light(dir / "c.fits", 60, "R")},
                                pool, never());
  for (const auto& x : res) f.ids.push_back(x.input.id);
  return f;
}

classification_record light_by_user(std::uint64_t id) {
  classification_record r;
  r.input_id = id;
  r.role = "light";
  r.source = "user";
  r.evidence_canonical = canonical_json(json::object());
  r.declarations_canonical = declarations();
  return r;
}

// Revisione `user` di un master esterno: ruolo `dark` e `master_descriptor` nelle dichiarazioni (CL-02). Forma del
// descriptor, scelta dei test da ratificare: chiave di primo livello `master_descriptor` del documento
// `astro.input_declarations.v1` (come nei test di U8a) con i campi di ADR-16.a nei nomi dei parametri del nodo di calibrazione.
classification_record dark_master_by_user(std::uint64_t id) {
  classification_record r = light_by_user(id);
  r.role = "dark";
  json desc = json::object();
  desc.set("role", json::string("dark"));
  desc.set("additive", json::string("bias_included"));
  desc.set("scaling", json::string("unscaled"));
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  doc.set("declared_image_fields", json::object());
  doc.set("master_descriptor", std::move(desc));
  r.declarations_canonical = canonical_json(doc);
  return r;
}

classification_record unclassified_by_user(std::uint64_t id) {
  classification_record r = light_by_user(id);
  r.role = "unclassified";
  json ev = json::object();
  ev.set("reason", json::string("user"));
  r.evidence_canonical = canonical_json(ev);
  return r;
}

TEST(BatchAnalysis, T_M3A_04f_analysis_on_a_read_only_store_writes_nothing_and_is_reproducible_after_reopen) {
  fixture f = light_store("t04f_ro");
  f.store->append_classifications({light_by_user(f.ids[0]), light_by_user(f.ids[1]), light_by_user(f.ids[2])});
  f.store.reset();
  const std::string before = store_test::dump_logical(f.project);

  memory_pool pool(64 * imp::MiB);
  int sample_reads = 0;
  const fault_injector counter = [&](std::string_view point) {
    if (point == "calib.analysis.samples") ++sample_reads;
    return 0;
  };
  auto ro = sqlite_dag_store::open(f.project, open_intent::read_only);
  const batch_proposal first = analyze_batch(*ro, {}, pool, never(), counter);
  ro.reset();
  EXPECT_EQ(sample_reads, 0);
  EXPECT_EQ(store_test::dump_logical(f.project), before) << "l'analisi non scrive nello store";
  // Light a (60 s, L) e b (61 s, L) stanno insieme; c (R) è un gruppo a parte.
  ASSERT_EQ(first.groups.size(), 2u);
  std::set<std::size_t> sizes = {first.groups[0].members.size(), first.groups[1].members.size()};
  EXPECT_EQ(sizes, (std::set<std::size_t>{1, 2}));
  for (const group_proposal& g : first.groups) {
    EXPECT_EQ(g.outcome, group_outcome::ready);
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(must(g.analysis).default_recipe.light, L::none);
    EXPECT_EQ(must(g.analysis).default_recipe.flat, F::none);
  }
  auto reopened = sqlite_dag_store::open(f.project, open_intent::read_write);
  EXPECT_EQ(analyze_batch(*reopened, {}, pool, never()).canonical(), first.canonical());
}

TEST(BatchAnalysis, T_M3A_04d_reopening_the_project_after_a_choice_on_an_ambiguous_group_needs_the_choice_again) {
  // CL-08: la scelta dei candidati è un ingresso dell'analisi e non si persiste in M3.a. Su store reale: un light e due master
  // dark esterni idonei (stessi metadati, digest diversi), quindi ruolo `dark` ambiguo. Dopo la scelta il progetto si chiude e
  // si riapre: senza scelta il gruppo torna `candidate_choice_required`; con la stessa scelta la proposta è byte-identica.
  // Oracle del default con la scelta: riga 100 di ADR-16.b §b.6 con `D` (nessun flat, nessun bias): `light.dark` + `flat.none`.
  const fs::path dir = fresh("t04d_reopen_choice");
  const fs::path project = dir / "project";
  auto store = sqlite_dag_store::create(project);
  store->put_setting("batch.analysis_params", canonical_json(params_doc()));
  memory_pool pool(64 * imp::MiB);
  const auto res = import_batch(*store, {write_light(dir / "l.fits", 60), write_dark(dir / "d1.fits", 60, 0), write_dark(dir / "d2.fits", 60, 7)},
                                pool, never());
  ASSERT_EQ(res.size(), 3u);
  for (const auto& x : res) ASSERT_FALSE(x.stop.has_value()) << x.stop->what();
  ASSERT_NE(res[1].input.sha256, res[2].input.sha256);
  store->append_classifications({light_by_user(res[0].input.id), dark_master_by_user(res[1].input.id), dark_master_by_user(res[2].input.id)});
  std::vector<std::string> darks = {res[1].input.sha256, res[2].input.sha256};
  std::sort(darks.begin(), darks.end());
  const candidate_choices choice = {{master_role::dark, darks.front()}};

  const auto expect_choice_required = [&](const batch_proposal& p) {
    ASSERT_EQ(p.groups.size(), 1u);
    const group_proposal& g = p.groups.front();
    EXPECT_EQ(g.outcome, group_outcome::candidate_choice_required);
    const role_choice* c = choice_of(g, master_role::dark);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::required);
    EXPECT_EQ(c->eligible, darks);
    EXPECT_FALSE(c->chosen.has_value());
    EXPECT_FALSE(g.analysis.has_value());
    EXPECT_TRUE(g.recipes.empty());
  };
  expect_choice_required(analyze_batch(*store, {}, pool, never()));
  const batch_proposal chosen = analyze_batch(*store, choice, pool, never());
  {
    const group_proposal& g = only_group(chosen);
    ASSERT_EQ(g.outcome, group_outcome::ready);
    ASSERT_NE(choice_of(g, master_role::dark), nullptr);
    EXPECT_EQ(must_choice(g, master_role::dark).chosen, std::optional<std::string>(darks.front()));
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(must(g.analysis).default_recipe.light, L::dark);
    EXPECT_EQ(must(g.analysis).default_recipe.flat, F::none);
  }
  const std::string chosen_canonical = chosen.canonical();
  store.reset();
  const std::string logical_at_close = store_test::dump_logical(project);

  auto reopened = sqlite_dag_store::open(project, open_intent::read_write);
  expect_choice_required(analyze_batch(*reopened, {}, pool, never()));
  EXPECT_EQ(analyze_batch(*reopened, choice, pool, never()).canonical(), chosen_canonical);
  reopened.reset();
  EXPECT_EQ(store_test::dump_logical(project), logical_at_close) << "la scelta non si scrive nello store";
}

TEST(BatchAnalysis, T_M3A_05c_user_revision_after_the_first_analysis_gives_a_new_analysis) {
  fixture f = light_store("t05c_revision");
  f.store->append_classifications({light_by_user(f.ids[0]), light_by_user(f.ids[1]), light_by_user(f.ids[2])});
  memory_pool pool(64 * imp::MiB);
  const batch_proposal before = analyze_batch(*f.store, {}, pool, never());
  ASSERT_EQ(before.groups.size(), 2u);
  EXPECT_TRUE(before.excluded.empty());
  f.store->append_classifications({unclassified_by_user(f.ids[1])});
  const batch_proposal after = analyze_batch(*f.store, {}, pool, never());
  EXPECT_NE(after.canonical(), before.canonical());
  EXPECT_EQ(after.groups.size(), 2u);
  ASSERT_EQ(after.excluded.size(), 1u);
  EXPECT_EQ(after.excluded[0].reason, "unclassified.user");
  EXPECT_EQ(after.excluded[0].input_ids, std::vector<std::uint64_t>{f.ids[1]});
  // Tornare al ruolo precedente è una nuova revisione `user`: stessa proposta di prima.
  f.store->append_classifications({light_by_user(f.ids[1])});
  EXPECT_EQ(analyze_batch(*f.store, {}, pool, never()).canonical(), before.canonical());
}

// ---------------------------------------------------------------- arbitraggio di U8b (6 novembre 2026)

// Percorso di un campione di PUB-RAW-01, con hash verificato contro il record; mai un salto.
std::string pub_raw_path(std::size_t index) {
  std::string name, error;
  std::vector<std::uint8_t> bytes;
  const bool ok = pub_raw::load(index, name, bytes, error);
  EXPECT_TRUE(ok) << error;
  if (!ok) throw std::runtime_error(error);
  return (fs::path(std::getenv("ASTRO_RAW_PIXLS_DIR")) / pub_raw::kSeeds[index].path).string();
}

// CL-05: un import fermato non ha report né immagine. Qualunque revisione `user` successiva (light, master,
// `unclassified`) lascia l'input escluso con `import_stopped`, categoria e codice dell'import (verità: X-Trans =
// `unsupported` / `raw.xtrans`, come in raw_threads.cpp), e l'analisi del resto del batch prosegue.
TEST(BatchAnalysis, T_M3A_05a_stopped_import_stays_excluded_with_category_and_code_after_any_user_revision) {
  const fs::path dir = fresh("t05a_import_stopped");
  auto store = sqlite_dag_store::create(dir / "project");
  store->put_setting("batch.analysis_params", canonical_json(params_doc()));
  memory_pool pool(512 * imp::MiB);
  const auto res = import_batch(*store, {write_light(dir / "l.fits", 60), pub_raw_path(7)}, pool, never());
  ASSERT_EQ(res.size(), 2u);
  ASSERT_FALSE(res[0].stop.has_value());
  ASSERT_TRUE(res[1].stop.has_value());
  ASSERT_EQ(res[1].stop->code(), "raw.xtrans");
  const std::uint64_t light_id = res[0].input.id;
  const std::uint64_t stopped_id = res[1].input.id;
  store->append_classifications({light_by_user(light_id)});

  const auto expect_import_stopped = [&](const char* step) {
    SCOPED_TRACE(step);
    const batch_proposal p = analyze_batch(*store, {}, pool, never());
    ASSERT_EQ(p.groups.size(), 1u) << "il light dell'altro ramo è analizzato";
    EXPECT_EQ(p.groups[0].outcome, group_outcome::ready);
    ASSERT_EQ(p.excluded.size(), 1u);
    EXPECT_EQ(p.excluded[0].reason, "import_stopped");
    EXPECT_EQ(p.excluded[0].input_ids, std::vector<std::uint64_t>{stopped_id});
    const json ev = parse_json(p.excluded[0].evidence_canonical);
    const json* category = ev.find("category");
    const json* code = ev.find("code");
    ASSERT_NE(category, nullptr);
    ASSERT_NE(code, nullptr);
    EXPECT_EQ(category->as_string(), "unsupported");
    EXPECT_EQ(code->as_string(), "raw.xtrans");
  };
  expect_import_stopped("solo la revisione dell'import");
  store->append_classifications({light_by_user(stopped_id)});
  expect_import_stopped("user light");
  store->append_classifications({dark_master_by_user(stopped_id)});
  expect_import_stopped("user master");
  store->append_classifications({unclassified_by_user(stopped_id)});
  expect_import_stopped("user unclassified");
}

// CL-05 emendato: due registrazioni dello stesso file con import fermato, una poi riclassificata dall'utente come master dark, non
// sono un conflitto di duplicati: il confronto usa le dichiarazioni della revisione dell'import. Verità: una sola entità,
// `import_stopped` (X-Trans = `unsupported` / `raw.xtrans`), con entrambi gli `input_id`.
TEST(BatchAnalysis, T_M3A_05a_duplicate_digest_with_stopped_import_is_import_stopped_not_a_conflict) {
  const fs::path dir = fresh("t05a_dup_stopped");
  auto store = sqlite_dag_store::create(dir / "project");
  store->put_setting("batch.analysis_params", canonical_json(params_doc()));
  memory_pool pool(512 * imp::MiB);
  const std::string xtrans = pub_raw_path(7);
  const auto res = import_batch(*store, {xtrans, xtrans}, pool, never());
  ASSERT_EQ(res.size(), 2u);
  ASSERT_TRUE(res[0].stop.has_value());
  ASSERT_TRUE(res[1].stop.has_value());
  ASSERT_EQ(res[0].stop->code(), "raw.xtrans");
  ASSERT_NE(res[0].input.id, res[1].input.id) << "controllo positivo: due input con lo stesso digest";
  ASSERT_EQ(res[0].input.sha256, res[1].input.sha256);
  // La revisione `user` ha ruolo e dichiarazioni (master_descriptor) diversi da quelli dell'import: senza la regola sarebbe un conflitto.
  store->append_classifications({dark_master_by_user(res[1].input.id)});
  const batch_proposal p = analyze_batch(*store, {}, pool, never());
  EXPECT_TRUE(p.groups.empty());
  ASSERT_EQ(p.excluded.size(), 1u);
  EXPECT_EQ(p.excluded[0].reason, "import_stopped");
  EXPECT_NE(p.excluded[0].reason, "duplicate_digest_conflict");
  EXPECT_EQ(p.excluded[0].input_ids, (std::vector<std::uint64_t>{res[0].input.id, res[1].input.id}));
  const json ev = parse_json(p.excluded[0].evidence_canonical);
  ASSERT_NE(ev.find("code"), nullptr);
  EXPECT_EQ(ev.find("code")->as_string(), "raw.xtrans");
}

// Controllo termico sullo store (CL-12): flat `raw` e bias FITS importati, `load_samples` del percorso persistito rilegge i
// campioni dagli originali. Verità a mano: F = 1124 e B = 100 uniformi, Fc = 1024, N = 1024, q_min = 0,5, budget 0,25, quindi
// soglia 0,25·0,5·1024 = 128: bound 64 nel budget, bound 200 fuori (`flat_thermal_budget`).
std::string write_uniform(const fs::path& path, const std::string& imagetyp, double exposure, double value) {
  fx::calib_header h;
  h.imagetyp = imagetyp;
  h.exposure = exposure;
  std::vector<std::string> cards;
  for (const std::string& c : fx::calib_cards(h)) cards.push_back(c);
  imp::gen g;
  g.bitpix = -32;
  g.w = 8;
  g.h = 8;
  g.cards = cards;
  g.value = [value](std::uint32_t, std::uint32_t, std::uint32_t) { return value; };
  imp::write_file(path, imp::make_fits(g));
  return path.string();
}

classification_record calib_by_user(std::uint64_t id, const std::string& role, json desc) {
  classification_record r = light_by_user(id);
  r.role = role;
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  doc.set("declared_image_fields", json::object());
  doc.set("master_descriptor", std::move(desc));
  r.declarations_canonical = canonical_json(doc);
  return r;
}

TEST(BatchAnalysis, T_M3A_04f_thermal_check_on_the_store_reads_the_flat_and_bias_samples_from_the_originals) {
  const auto run_with_bound = [](const std::string& name, double bound, int& reads) {
    const fs::path dir = fresh(name);
    auto store = sqlite_dag_store::create(dir / "project");
    json params = params_doc();
    params.set("flat_thermal_bound_adu", json::number(bound));
    params.set("flat_thermal_rel_budget", json::number(0.25));
    store->put_setting("batch.analysis_params", canonical_json(params));
    memory_pool pool(64 * imp::MiB);
    const auto res = import_batch(*store, {write_light(dir / "l.fits", 60), write_uniform(dir / "f.fits", "Flat Field", 10, 1124.0),
                                           write_uniform(dir / "b.fits", "Bias Frame", 0.001, 100.0)},
                                  pool, never());
    EXPECT_EQ(res.size(), 3u);
    for (const auto& x : res) {
      EXPECT_FALSE(x.stop.has_value());
      if (x.stop) throw std::runtime_error(x.stop->what());
    }
    json flat = json::object();
    flat.set("role", json::string("flat"));
    flat.set("flat", json::string("raw"));
    json bias = json::object();
    bias.set("role", json::string("bias"));
    store->append_classifications({light_by_user(res[0].input.id), calib_by_user(res[1].input.id, "flat", std::move(flat)),
                                   calib_by_user(res[2].input.id, "bias", std::move(bias))});
    reads = 0;
    const fault_injector counter = [&reads](std::string_view point) {
      if (point == "calib.analysis.samples") ++reads;
      return 0;
    };
    return analyze_batch(*store, {}, pool, never(), counter);
  };
  int reads = 0;
  {
    const batch_proposal p = run_with_bound("t04f_thermal_within", 64.0, reads);
    const group_proposal& g = only_group(p);
    ASSERT_EQ(g.outcome, group_outcome::ready);
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(reads, 1);
    EXPECT_TRUE(must(g.analysis).flat_thermal_within_budget) << "64 <= 128";
  }
  {
    const batch_proposal p = run_with_bound("t04f_thermal_over", 200.0, reads);
    const group_proposal& g = only_group(p);
    ASSERT_EQ(g.outcome, group_outcome::ready);
    ASSERT_TRUE(g.analysis.has_value());
    EXPECT_EQ(reads, 1);
    EXPECT_FALSE(must(g.analysis).flat_thermal_within_budget) << "200 > 128";
  }
}

// CL-07, `params_missing` per gruppo (m6): `decoder_black_abs_tol_adu` serve al gruppo la cui analisi ha un ingresso con lo
// stato del decoder.
TEST(BatchAnalysis, T_M3A_04f_decoder_black_tolerance_is_missing_only_for_the_group_with_a_decoder_state) {
  batch_analysis_request no_tol = request();
  no_tol.params.decoder_black_abs_tol_adu.reset();
  const baseline b;
  const decoder_state s = base_state();
  const auto dark_desc_bias = dark_desc(master_role::dark, additive_state::bias_included);
  {  // light FITS, unico dark candidato RAW (escluso per lo stato del decoder, ma entra nell'analisi)
    const std::vector<batch_input> in = {make_input(1, 'a', "light", b.light, std::nullopt, b.light_decl)};
    const std::vector<slot_candidate> slots = {raw_slot('d', master_role::dark, raw_image(kLightExp, s), dark_desc_bias, s, raw_decl())};
    const group_proposal g = only_group(analyze(in, no_tol, slots));
    EXPECT_EQ(g.outcome, group_outcome::params_missing);
    EXPECT_EQ(g.missing_params, std::vector<std::string>{"decoder_black_abs_tol_adu"});
    EXPECT_FALSE(g.analysis.has_value());
    EXPECT_TRUE(g.recipes.empty());
  }
  {  // light FITS con soli master FITS: la tolleranza non serve
    const group_proposal g = only_group(analyze(inputs_of(b), no_tol));
    EXPECT_NE(g.outcome, group_outcome::params_missing);
    EXPECT_EQ(g.outcome, group_outcome::ready);
    EXPECT_TRUE(g.missing_params.empty());
  }
  {  // light FITS con un dark FITS idoneo e un dark RAW candidato non scelto (escluso): l'analisi usa il FITS, nessuna tolleranza
    const std::vector<slot_candidate> slots = {raw_slot('k', master_role::dark, raw_image(kLightExp, s), dark_desc_bias, s, raw_decl())};
    const group_proposal g = only_group(analyze(inputs_of(b), no_tol, slots));
    EXPECT_EQ(g.outcome, group_outcome::ready);
    EXPECT_TRUE(g.missing_params.empty());
    EXPECT_EQ(must_choice(g, master_role::dark).chosen, std::optional<std::string>(std::string(64, 'd')));
  }
  {  // due gruppi nello stesso batch: il light FITS e il light RAW (chiave del gruppo diversa per lo stato del decoder);
     // nel batch c'è anche un dark RAW candidato, che non riguarda il gruppo del light FITS
    const std::vector<batch_input> in = {make_input(1, 'a', "light", b.light, std::nullopt, b.light_decl),
                                         make_input(2, 'r', "light", raw_image(kLightExp, s), std::nullopt, raw_decl()),
                                         make_input(3, 'd', "dark", b.dark, dark_desc_bias)};
    const std::vector<slot_candidate> slots = {raw_slot('k', master_role::dark, raw_image(kLightExp, s), dark_desc_bias, s, raw_decl())};
    const batch_proposal p = analyze(in, no_tol, slots);
    ASSERT_EQ(p.groups.size(), 2u);
    for (const group_proposal& g : p.groups) {
      ASSERT_EQ(g.members.size(), 1u);
      if (g.members[0].digest == digest_with('r')) {
        EXPECT_EQ(g.outcome, group_outcome::params_missing);
        EXPECT_EQ(g.missing_params, std::vector<std::string>{"decoder_black_abs_tol_adu"});
      } else {
        EXPECT_EQ(g.members[0].digest, digest_with('a'));
        EXPECT_EQ(g.outcome, group_outcome::ready);
        EXPECT_TRUE(g.missing_params.empty());
      }
    }
  }
}

// CL-08, idoneità tramite il flat: il confronto di bias e dark-flat è con i flat IDONEI (m7).
TEST(BatchAnalysis, T_M3A_04d_bias_and_dark_flat_follow_the_eligible_flats_not_the_excluded_ones) {
  const auto excluded_flat = [](const baseline& b, double temperature, double exposure) {
    image g = b.flat;
    g.metadata.filter = hs("R", "FILTER");  // L–F incompatibile: escluso
    g.metadata.temperature_c = hd(temperature, "CCD-TEMP");
    g.metadata.exposure_s = hd(exposure, "EXPTIME");
    return make_input(7, 'g', "flat", g, flat_desc());
  };
  {  // F–B compatibile solo con il flat escluso: il bias è escluso
    baseline b;
    b.bias.metadata.temperature_c = hd(-20.0, "CCD-TEMP");  // L–B e F(f)–B oltre 1 °C, F(g)–B uguale
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(excluded_flat(b, -20.0, kFlatExp));
    const group_proposal g = only_group(analyze(in));
    EXPECT_EQ(must_report(g, master_role::flat).status, candidate_status::usable) << "il flat 'f' resta idoneo";
    const role_choice* c = choice_of(g, master_role::bias);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::excluded_only);
    EXPECT_TRUE(c->eligible.empty());
    const candidate_report* r = report_of(g, master_role::bias);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->status, candidate_status::excluded);
  }
  {  // F–B compatibile con il flat idoneo (L–B no): il bias è idoneo, anche col flat escluso accanto
    baseline b;
    b.light.metadata.temperature_c = hd(-12.0, "CCD-TEMP");
    b.bias.metadata.temperature_c = hd(-9.0, "CCD-TEMP");
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(excluded_flat(b, -20.0, kFlatExp));
    const group_proposal g = only_group(analyze(in));
    const role_choice* c = choice_of(g, master_role::bias);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::only_compatible);
    EXPECT_EQ(c->eligible, std::vector<std::string>{std::string(64, 'b')});
  }
  {  // dark-flat compatibile solo col flat escluso, mentre 'f' è idoneo: escluso (non è il caso «nessun flat idoneo»)
    baseline b;
    b.dark_flat.metadata.exposure_s = hd(200.0, "EXPTIME");  // F(f) = 100 s: oltre la tolleranza; F(g) = 200 s: uguale
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(excluded_flat(b, -10.0, 200.0));
    const group_proposal g = only_group(analyze(in));
    const role_choice* c = choice_of(g, master_role::dark_flat);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::excluded_only);
    EXPECT_TRUE(c->eligible.empty());
    const candidate_report* r = report_of(g, master_role::dark_flat);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->status, candidate_status::excluded);
    EXPECT_EQ(r->reason, "incompatible.exposure_s");
  }
  {  // con un secondo dark-flat compatibile col flat idoneo: è l'unico idoneo; il primo resta escluso
    baseline b;
    b.dark_flat.metadata.exposure_s = hd(200.0, "EXPTIME");
    std::vector<batch_input> in = inputs_of(b);
    in.push_back(excluded_flat(b, -10.0, 200.0));
    in.push_back(make_input(8, 'h', "dark_flat", baseline().dark_flat, dark_desc(master_role::dark_flat, additive_state::bias_included)));
    const group_proposal g = only_group(analyze(in));
    const role_choice* c = choice_of(g, master_role::dark_flat);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->kind, choice_kind::only_compatible);
    EXPECT_EQ(c->eligible, std::vector<std::string>{std::string(64, 'h')});
  }
}

// CL-11: la selezione porta tutti i campi.
TEST(BatchAnalysis, T_M3A_11_selection_carries_lights_candidates_by_role_declarations_and_the_raw_state) {
  const baseline b;
  const std::string node_key(64, 'k');
  std::vector<batch_input> in = inputs_of(b);
  in.erase(in.begin() + 2);  // il dark è lo slot `current`
  in.push_back(make_input(6, '9', "light", b.light, std::nullopt, b.light_decl));
  const std::vector<slot_candidate> slots = {
      slot_candidate{node_key, master_role::dark, dark_desc(master_role::dark, additive_state::bias_included), make_observation(b.dark, declarations())}};
  const group_proposal g = only_group(analyze(in, request(), slots));
  ASSERT_EQ(g.outcome, group_outcome::ready);
  ASSERT_FALSE(g.recipes.empty());
  const std::vector<content_digest> lights = {digest_with('9'), digest_with('a')};  // per digest
  const std::vector<std::pair<master_role, char>> by_role = {{master_role::bias, 'b'}, {master_role::dark, 'k'},
                                                              {master_role::dark_flat, 'e'}, {master_role::flat, 'f'}};
  bool saw_slot = false;
  for (const chain_recipe& r : g.recipes) {
    SCOPED_TRACE(std::string(to_string(r.light)) + "/" + to_string(r.flat));
    const calibration_selection sel = select_recipe(g, r);
    EXPECT_EQ(sel.recipe.light, r.light);
    EXPECT_EQ(sel.recipe.flat, r.flat);
    EXPECT_EQ(sel.lights, lights);
    EXPECT_TRUE(sel.raw_inputs.empty()) << "nessun ingresso RAW";
    EXPECT_EQ(sel.declarations.exposure_rel_tol, std::optional<double>(0.01));
    EXPECT_EQ(sel.declarations.temperature_abs_tol_c, std::optional<double>(1.0));
    EXPECT_EQ(sel.declarations.temp_policy, std::optional<temperature_policy>(temperature_policy::compare));
    std::vector<selected_candidate> expected;
    for (const auto& [role, tag] : by_role) {
      if (!uses_role(role, r.light, r.flat)) continue;
      selected_candidate c;
      c.role = role;
      if (tag == 'k') {
        c.node_key = node_key;
        saw_slot = true;
      } else {
        c.input = digest_with(tag);
      }
      expected.push_back(c);
    }
    ASSERT_EQ(sel.candidates.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(sel.candidates[i].role, expected[i].role);
      EXPECT_EQ(sel.candidates[i].input, expected[i].input) << role_name(expected[i].role);
      EXPECT_EQ(sel.candidates[i].node_key, expected[i].node_key) << role_name(expected[i].role);
    }
  }
  EXPECT_TRUE(saw_slot) << "controllo positivo: una ricetta usa il candidato dello slot";

  // Light RAW con master RAW in slot: stato e identità del decoder del light nella selezione (i master RAW portano lo
  // stato dei loro frame nel descrittore del nodo, parte di M3.b), candidati per chiave del nodo, tolleranza del nero
  // nelle dichiarazioni.
  const group_proposal raw = only_group(analyze_raw(raw_batch{}));
  ASSERT_EQ(raw.outcome, group_outcome::ready);
  ASSERT_FALSE(raw.recipes.empty());
  const chain_recipe& rr = raw.recipes.front();
  const calibration_selection sel = select_recipe(raw, rr);
  EXPECT_EQ(sel.lights, std::vector<content_digest>{digest_with('a')});
  EXPECT_EQ(sel.declarations.decoder_black_abs_tol_adu, std::optional<std::uint32_t>(kBlackTol));
  std::size_t used = 0;
  for (const auto& [role, tag] : std::vector<std::pair<master_role, char>>{{master_role::bias, 'b'}, {master_role::dark, 'd'},
                                                                          {master_role::dark_flat, 'e'}, {master_role::flat, 'f'}}) {
    if (!uses_role(role, rr.light, rr.flat)) continue;
    ASSERT_LT(used, sel.candidates.size());
    EXPECT_EQ(sel.candidates[used].role, role);
    EXPECT_EQ(sel.candidates[used].node_key, std::optional<std::string>(std::string(64, tag))) << role_name(role);
    EXPECT_FALSE(sel.candidates[used].input.has_value()) << role_name(role);
    ++used;
  }
  EXPECT_GT(used, 0u);
  EXPECT_EQ(sel.candidates.size(), used);
  ASSERT_EQ(sel.raw_inputs.size(), 1u);
  EXPECT_TRUE(sel.raw_inputs[0].state == base_state()) << "stato del decoder del light RAW";
  EXPECT_EQ(sel.raw_inputs[0].identity, current_decoder_identity());
}

// CL-12: con le dichiarazioni termiche, le letture di campioni sono solo quelle del flat e del bias della catena
// `flat.bias` valutata. `load_samples` conta le letture per input.
TEST(BatchAnalysis, T_M3A_04f_samples_are_read_only_for_the_flat_and_bias_of_the_evaluated_flat_bias_chain) {
  struct run {
    int probe = 0;
    std::map<char, int> loads;
  };
  const auto analyze_thermal = [](bool flat_ok, bool bias_ok, bool extras, bool thermal) {
    run out;
    baseline b;
    fill(b.flat, 1124.0);
    fill(b.bias, 100.0);
    if (!flat_ok) b.flat.metadata.filter = hs("R", "FILTER");
    if (!bias_ok) b.bias.metadata.gain = hd(101.0, "GAIN");
    std::vector<batch_input> in = inputs_of(b);
    in.erase(in.begin() + 3);  // niente dark-flat
    if (extras) {  // flat e bias non candidati: esclusi per il filtro e per il gain
      image xf = b.flat;
      xf.metadata.filter = hs("R", "FILTER");
      image xb = b.bias;
      xb.metadata.gain = hd(101.0, "GAIN");
      in.push_back(make_input(7, 'g', "flat", xf, flat_desc()));
      in.push_back(make_input(8, 'c', "bias", xb, bias_desc()));
    }
    for (batch_input& i : in) {
      if (i.role != "flat" && i.role != "bias") continue;
      const char tag = i.digest.sha256[0];
      const image full = i.observation.observed_image();
      i.load_samples = [&out, tag, full]() {
        ++out.loads[tag];
        return full;
      };
    }
    batch_analysis_request req = request();
    if (thermal) {
      req.params.flat_thermal_bound_adu = 64.0;
      req.params.flat_thermal_rel_budget = 0.25;
    }
    const fault_injector counter = [&out](std::string_view point) {
      if (point == "calib.analysis.samples") ++out.probe;
      return 0;
    };
    (void)analyze_batch(in, {}, req, counter);
    return out;
  };
  {  // chain valutata, con flat e bias non candidati nel batch: leggono solo il flat 'f' e il bias 'b'
    const run r = analyze_thermal(true, true, true, true);
    EXPECT_EQ(r.probe, 1);
    EXPECT_EQ(r.loads.count('f') ? r.loads.at('f') : 0, 1);
    EXPECT_EQ(r.loads.count('b') ? r.loads.at('b') : 0, 1);
    EXPECT_EQ(r.loads.count('g') ? r.loads.at('g') : 0, 0) << "flat non candidato";
    EXPECT_EQ(r.loads.count('c') ? r.loads.at('c') : 0, 0) << "bias non candidato";
  }
  {  // flat escluso per il filtro: la catena non è valutata, nessuna lettura
    const run r = analyze_thermal(false, true, false, true);
    EXPECT_EQ(r.probe, 0);
    EXPECT_TRUE(r.loads.empty());
  }
  {  // bias escluso per il gain: nessuna lettura
    const run r = analyze_thermal(true, false, false, true);
    EXPECT_EQ(r.probe, 0);
    EXPECT_TRUE(r.loads.empty());
  }
  {  // senza dichiarazioni termiche: nessuna lettura, con tutti i candidati
    const run r = analyze_thermal(true, true, true, false);
    EXPECT_EQ(r.probe, 0);
    EXPECT_TRUE(r.loads.empty());
  }
}

// Store corrotto: stop di integrità, mai un default.
struct stored_light {
  fs::path project;
  std::string node_key;
  std::string report_path;
  std::uint64_t artifact_id = 0;
};

stored_light stored_light_of(const std::string& name) {
  const fs::path dir = fresh(name);
  stored_light out;
  out.project = dir / "project";
  auto store = sqlite_dag_store::create(out.project);
  store->put_setting("batch.analysis_params", canonical_json(params_doc()));
  memory_pool pool(64 * imp::MiB);
  const auto res = import_batch(*store, {write_light(dir / "l.fits", 60)}, pool, never());
  EXPECT_EQ(res.size(), 1u);
  store->append_classifications({light_by_user(res.at(0).input.id)});
  const auto slot = store->get_slot("batch.input." + std::to_string(res.at(0).input.id));
  EXPECT_TRUE(slot.has_value());
  out.node_key = slot->target_node_key;
  for (const artifact_record& a : store->artifacts_for(out.node_key)) {
    if (a.kind == "import_report") {
      out.artifact_id = a.id;
      out.report_path = store->artifact_path(a.rel_path).string();
    }
  }
  EXPECT_FALSE(out.report_path.empty());
  return out;
}

TEST(BatchAnalysis, T_M3A_04f_report_with_a_sample_type_outside_the_list_stops_with_report_malformed) {
  const stored_light s = stored_light_of("t04f_report_sample_type");
  const fx::bytes raw_text = imp::read_file(s.report_path);
  std::string text(raw_text.begin(), raw_text.end());
  const std::string key = "\"sample_type\":\"";
  const std::size_t at = text.find(key);
  ASSERT_NE(at, std::string::npos);
  const std::size_t end = text.find('"', at + key.size());
  ASSERT_NE(end, std::string::npos);
  text.replace(at + key.size(), end - (at + key.size()), "f16");
  fs::permissions(s.report_path, fs::perms::owner_write, fs::perm_options::add);
  {
    std::ofstream out(s.report_path, std::ios::binary | std::ios::trunc);
    out << text;
  }
  const std::string digest = sha256_hex({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
  {
    store_test::raw_db db(s.project);
    ASSERT_EQ(db.exec("UPDATE artifacts SET sha256 = '" + digest + "', size_bytes = " + std::to_string(text.size()) +
                      " WHERE id = " + std::to_string(s.artifact_id)),
              SQLITE_OK);
  }
  auto store = sqlite_dag_store::open(s.project, open_intent::read_only);
  memory_pool pool(64 * imp::MiB);
  EXPECT_STOP(analyze_batch(*store, {}, pool, never()), error_category::integrity, "batch.report_malformed");
}

TEST(BatchAnalysis, T_M3A_04f_slot_whose_node_is_missing_stops_with_slot_node_missing) {
  const stored_light s = stored_light_of("t04f_slot_node_missing");
  {
    store_test::raw_db db(s.project);
    ASSERT_EQ(db.exec("DROP TRIGGER nodes_immutable_delete"), SQLITE_OK);
    ASSERT_EQ(db.exec("DELETE FROM nodes WHERE node_key = '" + s.node_key + "'"), SQLITE_OK);
  }
  auto store = sqlite_dag_store::open(s.project, open_intent::read_only);
  memory_pool pool(64 * imp::MiB);
  EXPECT_STOP(analyze_batch(*store, {}, pool, never()), error_category::integrity, "batch.slot_node_missing");
}

// Light DNG del decoder del progetto sullo store (CL-11, CL-12; U13 §8): l'analisi legge lo stato del decoder dal record
// persistito nel nodo d'import, non ridecodifica. Verità: la decodifica indipendente degli stessi byte con
// `decode_raw_buffer` (decoder del progetto: deflate a 32 bit, tasselli 16 × 16, origine di ActiveArea dispari). Lo
// stato della selezione è quello della decodifica; `verify_selection` con la nuova decodifica dei byte riletti dal disco
// non ferma; con un campo di `dng` diverso ferma (controllo che il confronto attraversi i campi del decoder del progetto).
// Identità attesa: `current_decoder_identity()`, letta dal record persistito (ADR-08.b §2.9, riga «Libreria»: identità del
// pacchetto di decoder nel record di entrambi i decoder).
TEST(BatchAnalysis, T_M3A_11_project_dng_light_on_the_store_carries_the_decoded_state_and_verify_selection_passes) {
  const fs::path dir = fresh("t11_project_dng");
  fx::dng32_spec spec = u13b::spec32(1, u13b::layouts()[4], 32, 32);
  u13b::set_area(spec, 1, 1);
  const std::vector<std::uint8_t> bytes = fx::make_deflate_dng(spec, fx::tiff_order::ii, [](fx::tiff_ifd& ifd) {
    fx::tiff_ifd exif;
    exif.entries.push_back(fx::ent(33434, fx::tiff_type::rational, {static_cast<std::uint64_t>(kLightExp), 1}));
    ifd.exif.push_back(std::move(exif));
    fx::set_entry(ifd, fx::ent(274, fx::tiff_type::short_, {6}));
    fx::set_entry(ifd, fx::ent(50719, fx::tiff_type::rational, {5, 2, 8, 2}));
  });
  const fs::path file = dir / "light.dng";
  imp::write_file(file, bytes);
  const image decoded = decode_raw_buffer(bytes);
  ASSERT_TRUE(decoded.decoder_state.has_value());
  ASSERT_EQ(decoded.decoder_state->route.decoder, raw_decoder_kind::astro_dng);
  ASSERT_TRUE(decoded.decoder_state->dng.has_value());

  auto store = sqlite_dag_store::create(dir / "project");
  store->put_setting("batch.analysis_params", canonical_json(params_doc()));
  memory_pool pool(512 * imp::MiB);
  const auto res = import_batch(*store, {file.string()}, pool, never());
  ASSERT_EQ(res.size(), 1u);
  ASSERT_FALSE(res[0].stop.has_value()) << res[0].stop->what();
  classification_record light = light_by_user(res[0].input.id);
  light.declarations_canonical = raw_decl();
  store->append_classifications({light});

  const batch_proposal p = analyze_batch(*store, {}, pool, never());
  ASSERT_TRUE(p.excluded.empty()) << p.excluded.front().reason << " " << p.excluded.front().evidence_canonical;
  const group_proposal g = only_group(p);
  ASSERT_EQ(g.outcome, group_outcome::ready);
  ASSERT_FALSE(g.recipes.empty());
  const calibration_selection sel = select_recipe(g, g.recipes.front());
  ASSERT_EQ(sel.raw_inputs.size(), 1u);
  const selected_decoder_input& frozen = sel.raw_inputs[0];
  EXPECT_TRUE(frozen.state == *decoded.decoder_state)
      << "stato letto dal report diverso da quello della decodifica: "
      << record_roundtrip::differing_members(frozen.state, *decoded.decoder_state);
  EXPECT_EQ(frozen.state.route.decoder, raw_decoder_kind::astro_dng);
  EXPECT_EQ(frozen.identity, current_decoder_identity());

  const image again = decode_raw_buffer(imp::read_file(file));
  ASSERT_TRUE(again.decoder_state.has_value());
  EXPECT_NO_THROW(verify_selection(sel, {selected_decoder_input{frozen.name, *again.decoder_state, current_decoder_identity()}}));

  decoder_state changed = *again.decoder_state;
  changed.dng->predictor = 2;
  try {
    verify_selection(sel, {selected_decoder_input{frozen.name, changed, current_decoder_identity()}});
    ADD_FAILURE() << "un campo di `dng` diverso non ferma la selezione";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::integrity) << e.what();
    EXPECT_EQ(e.code(), std::string("calib.decoder_state_changed")) << e.what();
    const std::map<std::string, std::string> ctx(e.context().begin(), e.context().end());
    EXPECT_EQ(ctx.count("field") ? ctx.at("field") : std::string("<assente>"), "dng") << e.what();
  }
}

}  // namespace
