// U8b (M3.a), F-IO-03 CL-11 (lacuna L5): `verify_selection` e stop del solo ramo dopo la scelta.
// Prove: T-M3A-CAL-09 parte M3.a (luce), T-M3A-06 a e b. Analisi, gruppi e candidati sono in test_batch_analysis.cpp.
//
// Il file usa simboli che U8b deve ancora introdurre; li fissa con questa forma (nomi di funzione dal contratto; campi e
// firme sono scelte dei test, da ratificare). Le strutture di U5-U7 (`chain_recipe`, `decoder_identity`, ...) sono
// quelle dei loro test.
//
//   astro/calibration_analysis.hpp
//     struct selected_decoder_input { std::string name; decoder_state state; decoder_identity identity; };
//     struct calibration_selection { chain_recipe recipe; std::vector<selected_decoder_input> raw_inputs; };
//     void verify_selection(const calibration_selection&, const std::vector<selected_decoder_input>& observed);
//         // CL-11: funzione pura; l'unica uscita è «nessun effetto» (ritorno void) oppure lo stop
//         // `integrity` `calib.decoder_state_changed` con contesto `field`, `predicate` = P-LOST e
//         // `prerequisite` = `<name>.decoder_state`. `field` è il nome del membro di `decoder_state` (la prima
//         // differenza nell'ordine di dichiarazione) oppure `decoder_identity.<membro>`; lo stato si confronta prima
//         // dell'identità. `observed` ha gli stessi `name` della selezione, nello stesso ordine; un ingresso della selezione
//         // senza osservazione è un prerequisito perso (P-LOST): stesso stop, `field` = `observation`.
//   astro/batch_classification.hpp (come in test_batch_analysis.cpp)
//     input_observation make_observation(const image&, const std::string& declarations_canonical);  // operator==
//     batch_proposal analyze_batch(const sqlite_dag_store&, const candidate_choices&, memory_pool&, const cancel_token&, ...);
//
// B14 (M3.b), candidato (c) di CL-06: nodo `master.combine` proposto dall'analisi di C-02.b. API attesa (punti aperti,
// decide il coordinatore):
//   struct selected_candidate { ...; std::optional<std::vector<content_digest>> proposed_frames; };
//       // solo per il candidato (c): i digest dei frame del nodo scelti, in ordine canonico (CL-11)
//   struct observed_proposed_node { std::string node_key; std::vector<content_digest> frames; };
//       // i frame che il nodo contiene ora (dopo l'esecuzione del master), in ordine canonico
//   void verify_selection(const calibration_selection&, const std::vector<selected_decoder_input>& observed,
//                         const std::vector<observed_proposed_node>& nodes = {});
//       // per ogni candidato con `proposed_frames`: il nodo osservato con la stessa `node_key` ha esattamente quei frame,
//       // nello stesso ordine; altrimenti integrity `calib.proposed_node_changed`, contesto `node_key` e `field`
//       // (`frames`, oppure `observation` se il nodo non è osservato). Non esclude, non sostituisce, non modifica nulla.
//       // Un candidato senza `proposed_frames` (input o slot) non richiede osservazione del nodo.
//
// T-M3A-06b: il job del test è composto da `run_branches` sullo store SQLite (`branch_files_job`), come il job del test di
// G-PACK: non è un tipo di job del prodotto. Legge l'input dal confine di OB-11 (`require_input_digest`), decodifica il
// DNG con il decoder del prodotto (`decode_raw_buffer`) e chiama `verify_selection`. Il cambio dello stato del decoder alla
// nuova decodifica è il gancio `after_unpack` del decoder (il contratto parla di «fault_injector alla nuova decodifica»:
// il gancio è il meccanismo di iniezione del decoder).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/calibration_analysis.hpp"
#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/raw_decode.hpp"
#include "astro/scheduler.hpp"
#include "astro/sqlite_dag_store.hpp"
#include "dng_state_changes.hpp"
#include "expect_stop.hpp"
#include "import_support.hpp"
#include "store_test_support.hpp"
#include "syn_dng.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;

constexpr std::uint32_t kW = 4;
constexpr std::uint32_t kH = 4;

// ---------------------------------------------------------------- stato e identità del decoder

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
  s.data_maximum = 16000;
  s.linear_max = std::array<std::int64_t, 4>{16000, 16000, 16000, 16000};
  s.linearization = decoder_linearization::identity;
  s.curve_sha256 = std::array<std::uint8_t, 32>{};
  s.process_warnings = 0;
  s.source = metadata_source{metadata_origin::decoder, "color.black"};
  return s;
}

decoder_identity base_identity() { return decoder_identity{"0.22.3-Release", 0x001603, 0xc0, std::string(64, '1')}; }

selected_decoder_input input_of(const std::string& name, const decoder_state& s = base_state(),
                                const decoder_identity& id = base_identity()) {
  return selected_decoder_input{name, s, id};
}

calibration_selection selection_of(std::vector<selected_decoder_input> raw) {
  calibration_selection s;
  s.recipe.light = light_chain::none;
  s.recipe.flat = flat_chain::none;
  s.raw_inputs = std::move(raw);
  return s;
}

// Stop atteso di `verify_selection`: categoria, codice e le tre chiavi del contesto.
::testing::AssertionResult stops_with(const calibration_selection& sel, const std::vector<selected_decoder_input>& observed,
                                      const std::string& field, const std::string& prerequisite) {
  try {
    verify_selection(sel, observed);
  } catch (const stop_error& e) {
    if (e.category() != error_category::integrity || e.code() != "calib.decoder_state_changed") {
      return ::testing::AssertionFailure() << "stop diverso: " << e.what();
    }
    std::map<std::string, std::string> ctx(e.context().begin(), e.context().end());
    if (ctx["field"] != field) return ::testing::AssertionFailure() << "campo " << ctx["field"] << " invece di " << field;
    if (ctx["predicate"] != "P-LOST") return ::testing::AssertionFailure() << "predicate " << ctx["predicate"];
    if (ctx["prerequisite"] != prerequisite) {
      return ::testing::AssertionFailure() << "prerequisite " << ctx["prerequisite"] << " invece di " << prerequisite;
    }
    return ::testing::AssertionSuccess();
  } catch (const std::exception& e) {
    return ::testing::AssertionFailure() << "eccezione diversa da stop_error: " << e.what();
  }
  return ::testing::AssertionFailure() << "nessuno stop (campo atteso " << field << ")";
}

// ---------------------------------------------------------------- T-M3A-CAL-09 e T-M3A-06 a

TEST(VerifySelection, T_M3A_CAL_09_signature_returns_nothing_and_takes_the_selection_by_const_reference) {
  static_assert(std::is_void_v<decltype(verify_selection(std::declval<const calibration_selection&>(),
                                                          std::declval<const std::vector<selected_decoder_input>&>()))>,
                "l'unica uscita è «nessun effetto» o lo stop: nessun'altra selezione, ricetta o candidato");
  SUCCEED();
}

TEST(VerifySelection, T_M3A_CAL_09_equal_state_and_identity_have_no_effect_and_leave_the_selection_untouched) {
  const calibration_selection sel = selection_of({input_of("light-1"), input_of("light-2")});
  const calibration_selection copy = sel;
  EXPECT_NO_THROW(verify_selection(sel, {input_of("light-1"), input_of("light-2")}));
  ASSERT_EQ(sel.raw_inputs.size(), copy.raw_inputs.size());
  for (std::size_t i = 0; i < sel.raw_inputs.size(); ++i) {
    EXPECT_EQ(sel.raw_inputs[i].name, copy.raw_inputs[i].name);
    EXPECT_EQ(sel.raw_inputs[i].state, copy.raw_inputs[i].state);
    EXPECT_EQ(sel.raw_inputs[i].identity.libraw_version, copy.raw_inputs[i].identity.libraw_version);
  }
  EXPECT_EQ(sel.recipe.light, copy.recipe.light);
  EXPECT_EQ(sel.recipe.flat, copy.recipe.flat);
  // Selezione senza ingressi RAW (FITS): nessun effetto.
  EXPECT_NO_THROW(verify_selection(selection_of({}), {}));
}

TEST(VerifySelection, T_M3A_CAL_09_provenance_of_the_state_is_not_content_and_does_not_stop) {
  decoder_state other = base_state();
  other.source = metadata_source{metadata_origin::declared, "altra provenienza"};
  EXPECT_NO_THROW(verify_selection(selection_of({input_of("light-1")}), {input_of("light-1", other)}));
}

struct state_change {
  const char* field;
  std::function<void(decoder_state&)> apply;
};

// Un cambio per ognuno dei membri di contenuto di `decoder_state` (ADR-03.b §b.2, U13 §8), scritti a mano; i campi di
// `dng` con lo stato del decoder del progetto sono in `dng_state_changes()`.
std::vector<state_change> state_changes() {
  return {
      {"raw_width", [](decoder_state& s) { s.raw_width += 2; }},
      {"raw_height", [](decoder_state& s) { s.raw_height += 2; }},
      {"window", [](decoder_state& s) { s.window.x0 = 0; }},
      {"black", [](decoder_state& s) { s.black += 1; }},
      {"cfa_color_index", [](decoder_state& s) { s.cfa_color_index = {1, 0, 2, 3}; }},
      {"black_per_color", [](decoder_state& s) { s.black_per_color[2] = 1; }},
      {"black_pattern_rows", [](decoder_state& s) { s.black_pattern_rows = 1; s.black_pattern = {0, 0}; }},
      {"black_pattern_cols", [](decoder_state& s) { s.black_pattern_cols = 1; s.black_pattern = {0, 0}; }},
      {"black_pattern", [](decoder_state& s) { s.black_pattern[3] = 1; }},
      {"white", [](decoder_state& s) { s.white -= 1; }},
      {"data_maximum", [](decoder_state& s) { s.data_maximum = 15999; }},
      {"linear_max", [](decoder_state& s) { (*s.linear_max)[1] = 15999; }},
      {"linearization", [](decoder_state& s) { s.linearization = decoder_linearization::decoder_curve; }},
      {"curve_sha256", [](decoder_state& s) { (*s.curve_sha256)[7] = 1; }},
      {"process_warnings", [](decoder_state& s) { s.process_warnings = 1; }},
      {"raw_inset_crops", [](decoder_state& s) { s.raw_inset_crops[1][2] = 4; }},
      {"flip", [](decoder_state& s) { s.flip = 6; }},
      {"route", [](decoder_state& s) { s.route.reason = "tiff_not_dng"; }},
      {"route", [](decoder_state& s) { s.route.compression = 7; }},
  };
}

// Stato del decoder del progetto: instradamento astro_dng, `dng` presente (U13 §8).
decoder_state base_dng_state() {
  decoder_state s = base_state();
  s.route = dng_state_fixture::project_route();
  s.dng = dng_state_fixture::project_fields(kH);
  return s;
}

// Un cambio per membro di `dng_decoder_fields`, con `route` uguale: il campo dello stop è `dng`.
std::vector<state_change> dng_state_changes() {
  std::vector<state_change> out;
  for (const dng_state_fixture::field_change& c : dng_state_fixture::dng_field_changes(kW, kH)) out.push_back({c.field, c.apply});
  return out;
}

TEST(VerifySelection, T_M3A_06a_each_decoder_state_field_changed_one_at_a_time_stops_with_that_field) {
  const calibration_selection sel = selection_of({input_of("light-1")});
  for (const state_change& c : state_changes()) {
    SCOPED_TRACE(c.field);
    decoder_state changed = base_state();
    c.apply(changed);
    ASSERT_FALSE(changed == base_state()) << "il cambio deve toccare il contenuto";
    EXPECT_TRUE(stops_with(sel, {input_of("light-1", changed)}, c.field, "light-1.decoder_state"));
  }
}

TEST(VerifySelection, T_M3A_06a_each_dng_field_changed_with_equal_route_stops_with_dng) {
  const calibration_selection sel = selection_of({input_of("light-1", base_dng_state())});
  EXPECT_NO_THROW(verify_selection(sel, {input_of("light-1", base_dng_state())})) << "stato del progetto uguale";
  for (const state_change& c : dng_state_changes()) {
    SCOPED_TRACE(c.field);
    decoder_state changed = base_dng_state();
    c.apply(changed);
    ASSERT_FALSE(changed == base_dng_state()) << "il cambio deve toccare il contenuto";
    ASSERT_TRUE(changed.route == base_dng_state().route) << "solo `dng` cambia";
    EXPECT_TRUE(stops_with(sel, {input_of("light-1", changed)}, "dng", "light-1.decoder_state"));
  }
}

TEST(VerifySelection, T_M3A_06a_each_decoder_identity_field_changed_with_equal_state_stops) {
  const calibration_selection sel = selection_of({input_of("light-1")});
  struct row {
    const char* field;
    std::function<void(decoder_identity&)> apply;
  };
  const std::vector<row> rows = {
      {"decoder_identity.libraw_version", [](decoder_identity& i) { i.libraw_version = "0.22.2-Release"; }},
      {"decoder_identity.libraw_version_number", [](decoder_identity& i) { i.libraw_version_number = 0x001602; }},
      {"decoder_identity.libraw_capabilities", [](decoder_identity& i) { i.libraw_capabilities = 0x80; }},
      {"decoder_identity.libraw_tarball_sha256", [](decoder_identity& i) { i.libraw_tarball_sha256 = std::string(64, '2'); }},
  };
  for (const row& r : rows) {
    SCOPED_TRACE(r.field);
    decoder_identity id = base_identity();
    r.apply(id);
    EXPECT_TRUE(stops_with(sel, {input_of("light-1", base_state(), id)}, r.field, "light-1.decoder_state"));
  }
}

TEST(VerifySelection, T_M3A_06a_first_difference_wins_and_names_the_input_that_changed) {
  const calibration_selection sel = selection_of({input_of("light-1"), input_of("light-2")});
  decoder_state two = base_state();
  two.black += 1;
  two.white -= 1;
  // Due differenze nello stesso stato: la prima nell'ordine di dichiarazione (`black` prima di `white`).
  EXPECT_TRUE(stops_with(sel, {input_of("light-1"), input_of("light-2", two)}, "black", "light-2.decoder_state"));
  // Stato e identità diversi insieme: prima lo stato.
  decoder_identity id = base_identity();
  id.libraw_version = "0.22.2-Release";
  EXPECT_TRUE(stops_with(sel, {input_of("light-1", two, id), input_of("light-2")}, "black", "light-1.decoder_state"));
}

TEST(VerifySelection, T_M3A_CAL_09_an_input_of_the_selection_without_observation_is_a_lost_prerequisite) {
  const calibration_selection sel = selection_of({input_of("light-1"), input_of("light-2")});
  EXPECT_TRUE(stops_with(sel, {input_of("light-1")}, "observation", "light-2.decoder_state"));
  EXPECT_TRUE(stops_with(sel, {}, "observation", "light-1.decoder_state"));
  // Un'osservazione di un ingresso che la selezione non ha non sostituisce quella mancante.
  EXPECT_TRUE(stops_with(sel, {input_of("light-1"), input_of("altro")}, "observation", "light-2.decoder_state"));
}

// ---------------------------------------------------------------- T-M3A-06 a: revisioni dopo la scelta

std::string declarations(std::uint64_t binning, const std::string& filter) {
  json fields = json::object();
  fields.set("binning_x", json::uinteger(binning));
  fields.set("binning_y", json::uinteger(binning));
  json meta = json::object();
  meta.set("filter", json::string(filter));
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  doc.set("declared_image_fields", std::move(fields));
  doc.set("frame_metadata", std::move(meta));
  return canonical_json(doc);
}

// ---------------------------------------------------------------- fixture su store: tre light RAW importati

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

// DNG sintetico FIX-SYN-DNG-01; `seed` cambia i pixel (digest) e il livello del nero (stato del decoder).
std::string write_dng(const fs::path& path, std::uint32_t seed) {
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.active_area = fx::dng_active_area{0, 0, 32, 40};
  s.white = 4000;
  s.black = {static_cast<std::uint16_t>(64 + seed)};
  std::vector<std::uint16_t> v(64 * 48);
  for (std::uint32_t r = 0; r < 48; ++r)
    for (std::uint32_t c = 0; c < 64; ++c) v[r * 64 + c] = static_cast<std::uint16_t>(100 + (r * 37 + c * 11 + seed * 5) % 3500);
  s.pixels = fx::pixels_u16(v);
  imp::write_file(path, fx::make_dng(s));
  return path.string();
}

classification_record user_revision(std::uint64_t id, const std::string& role, const std::string& declarations_doc) {
  classification_record r;
  r.input_id = id;
  r.role = role;
  r.source = "user";
  json ev = json::object();
  if (role == "unclassified") ev.set("reason", json::string("user"));
  r.evidence_canonical = canonical_json(ev);
  r.declarations_canonical = declarations_doc;
  return r;
}

json analysis_params() {
  json d = json::object();
  d.set("format", json::string("astro.batch_analysis_params.v1"));
  d.set("exposure_rel_tol", json::number(0.01));
  d.set("temperature_abs_tol_c", json::number(1.0));
  d.set("temperature_policy", json::string("compare"));
  d.set("decoder_black_abs_tol_adu", json::uinteger(10));
  d.set("normalization", json::string("global_median"));
  d.set("q_min", json::number(0.5));
  d.set("q_rel_sigma_max", json::number(10.0));
  return d;
}

struct raw_project {
  fs::path dir;
  fs::path project;
  std::unique_ptr<sqlite_dag_store> store;
  std::vector<std::string> paths;
  std::vector<input_record> records;
  std::vector<image> decoded;
};

raw_project raw_project_of(const std::string& name) {
  raw_project p;
  p.dir = fresh(name);
  p.project = p.dir / "project";
  p.store = sqlite_dag_store::create(p.project);
  memory_pool pool(64 * imp::MiB);
  for (std::uint32_t i = 0; i < 3; ++i) p.paths.push_back(write_dng(p.dir / ("l" + std::to_string(i) + ".dng"), i));
  const auto imported = import_batch(*p.store, p.paths, pool, never());
  EXPECT_EQ(imported.size(), 3u);
  for (const auto& x : imported) {
    EXPECT_FALSE(x.stop.has_value()) << (x.stop ? x.stop->what() : "");
    p.records.push_back(x.input);
  }
  p.store->append_classifications({user_revision(p.records[0].id, "light", declarations(1, "L")),
                                   user_revision(p.records[1].id, "light", declarations(1, "L")),
                                   user_revision(p.records[2].id, "light", declarations(1, "L"))});
  p.store->put_setting("batch.analysis_params", canonical_json(analysis_params()));
  for (const std::string& path : p.paths) {
    const auto bytes = imp::read_file(path);
    p.decoded.push_back(decode_raw_buffer(std::span<const std::uint8_t>(bytes.data(), bytes.size())));
  }
  return p;
}

// Selezione del gruppo (unico) con la sua ricetta di default, come la dà `select_recipe`.
calibration_selection select_default(const raw_project& p, memory_pool& pool, batch_proposal& proposal) {
  proposal = analyze_batch(*p.store, {}, pool, never());
  EXPECT_EQ(proposal.groups.size(), 1u);
  EXPECT_EQ(proposal.groups.at(0).outcome, group_outcome::ready);
  EXPECT_FALSE(proposal.groups.at(0).recipes.empty());
  return select_recipe(proposal.groups.at(0), proposal.groups.at(0).recipes.front());
}

// L'ingresso congelato dalla selezione che ha lo stato del decoder di `decoded` (i tre file hanno livelli del nero diversi).
const selected_decoder_input* frozen_for(const calibration_selection& sel, const image& decoded) {
  for (const selected_decoder_input& in : sel.raw_inputs) {
    if (in.state == *decoded.decoder_state) return &in;
  }
  return nullptr;
}

TEST(VerifySelection, T_M3A_06a_selection_freezes_the_observed_decoder_state_of_every_raw_input) {
  raw_project p = raw_project_of("t06a_freeze");
  memory_pool pool(64 * imp::MiB);
  batch_proposal proposal;
  const calibration_selection sel = select_default(p, pool, proposal);
  ASSERT_EQ(sel.raw_inputs.size(), 3u);
  for (const image& d : p.decoded) {
    ASSERT_TRUE(d.decoder_state.has_value());
    const selected_decoder_input* f = frozen_for(sel, d);
    ASSERT_NE(f, nullptr) << "lo stato del decoder osservato nell'analisi è nella selezione";
    EXPECT_FALSE(f->name.empty());
  }
  std::set<std::string> names;
  for (const selected_decoder_input& in : sel.raw_inputs) names.insert(in.name);
  EXPECT_EQ(names.size(), 3u) << "nomi distinti, uno per input";
  // Con lo stato osservato di nuovo uguale: nessun effetto.
  std::vector<selected_decoder_input> again = sel.raw_inputs;
  EXPECT_NO_THROW(verify_selection(sel, again));
}

TEST(VerifySelection, T_M3A_06a_later_classification_and_declaration_revisions_neither_change_nor_stop_the_selection) {
  raw_project p = raw_project_of("t06a_revisions");
  memory_pool pool(64 * imp::MiB);
  batch_proposal before;
  const calibration_selection sel = select_default(p, pool, before);
  const calibration_selection frozen_copy = sel;
  const std::string frozen_declarations = declarations(1, "L");
  const image& light = p.decoded[0];
  const input_observation at_selection = make_observation(light, frozen_declarations);

  // Selezione invariata: nomi, stati, identità e ricetta uguali alla copia presa alla selezione.
  const auto expect_selection_unchanged = [&] {
    ASSERT_EQ(sel.raw_inputs.size(), frozen_copy.raw_inputs.size());
    for (std::size_t i = 0; i < sel.raw_inputs.size(); ++i) {
      EXPECT_EQ(sel.raw_inputs[i].name, frozen_copy.raw_inputs[i].name);
      EXPECT_TRUE(sel.raw_inputs[i].state == frozen_copy.raw_inputs[i].state);
      EXPECT_EQ(sel.raw_inputs[i].identity.libraw_version, frozen_copy.raw_inputs[i].identity.libraw_version);
      EXPECT_EQ(sel.raw_inputs[i].identity.libraw_version_number, frozen_copy.raw_inputs[i].identity.libraw_version_number);
      EXPECT_EQ(sel.raw_inputs[i].identity.libraw_capabilities, frozen_copy.raw_inputs[i].identity.libraw_capabilities);
      EXPECT_EQ(sel.raw_inputs[i].identity.libraw_tarball_sha256, frozen_copy.raw_inputs[i].identity.libraw_tarball_sha256);
    }
    EXPECT_EQ(sel.recipe.light, frozen_copy.recipe.light);
    EXPECT_EQ(sel.recipe.flat, frozen_copy.recipe.flat);
  };
  // Osservazione dei tre input da una decodifica nuova, fatta dopo le revisioni: stato del decoder della decodifica nuova,
  // nome e identità dell'ingresso congelato dello stesso file (il decoder non espone l'identità per immagine). L'ordine è
  // quello della selezione.
  const auto observed_after_revisions = [&] {
    std::vector<selected_decoder_input> observed;
    for (const selected_decoder_input& frozen : sel.raw_inputs) {
      for (std::size_t i = 0; i < p.paths.size(); ++i) {
        if (frozen_for(sel, p.decoded[i]) != &frozen) continue;
        const auto bytes = imp::read_file(p.paths[i]);
        const image fresh_decode = decode_raw_buffer(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
        observed.push_back(selected_decoder_input{frozen.name, *fresh_decode.decoder_state, frozen.identity});
      }
    }
    EXPECT_EQ(observed.size(), sel.raw_inputs.size()) << "ogni ingresso congelato ha il suo file";
    return observed;
  };

  // Dopo la scelta, revisione 1: dichiarazioni diverse per un input (binning 2) e ruolo `unclassified` per un altro.
  const std::string revised = declarations(2, "R");
  p.store->append_classifications({user_revision(p.records[0].id, "light", revised),
                                   user_revision(p.records[2].id, "unclassified", declarations(1, "L"))});
  EXPECT_EQ(p.store->classifications(p.records[0].id).back().declarations_canonical, revised);

  // La selezione non cambia e non si ferma: l'osservazione costruita sulle dichiarazioni congelate è quella di prima.
  EXPECT_TRUE(make_observation(light, frozen_declarations) == at_selection);
  EXPECT_FALSE(make_observation(light, revised) == at_selection) << "le dichiarazioni rivedute sono un'altra osservazione";
  expect_selection_unchanged();
  EXPECT_NO_THROW(verify_selection(sel, observed_after_revisions())) << "a stato del decoder uguale nessun effetto";

  // La nuova analisi propone altro: il light con binning 2 forma un gruppo a parte, l'altro è escluso `unclassified.user`.
  const batch_proposal after = analyze_batch(*p.store, {}, pool, never());
  EXPECT_NE(after.canonical(), before.canonical());
  EXPECT_EQ(after.groups.size(), 2u);
  ASSERT_EQ(after.excluded.size(), 1u);
  EXPECT_EQ(after.excluded[0].reason, "unclassified.user");

  // Revisione 2 (§12, 06a: «valore in conflitto con l'header»): per un RAW il valore letto viene dal decoder, che dà la
  // linearità `linear` con origine `decoder` (`image.lin_origin`, ADR-03.b §b.1); il secondo input dichiara
  // `linearity = nonlinear`. ADR-03.b §b.1 e CL-05: un valore dichiarato diverso da un valore `decoder` presente esclude
  // l'input con `declared_conflict`. La selezione resta invariata e non si ferma: l'osservazione sulle
  // dichiarazioni congelate di quel file è quella della selezione; quella sulle dichiarazioni nuove è un'altra.
  json fields = json::object();
  fields.set("linearity", json::string("nonlinear"));
  json doc = json::object();
  doc.set("format", json::string("astro.input_declarations.v1"));
  doc.set("declared_image_fields", std::move(fields));
  const std::string conflicting = canonical_json(doc);
  const input_observation second_at_selection = make_observation(p.decoded[1], frozen_declarations);
  p.store->append_classifications({user_revision(p.records[1].id, "light", conflicting)});
  EXPECT_TRUE(make_observation(p.decoded[1], frozen_declarations) == second_at_selection);
  input_observation second_conflicting = second_at_selection;
  EXPECT_NO_THROW(second_conflicting = make_observation(p.decoded[1], conflicting)) << "il conflitto non lancia: lo registra";
  EXPECT_FALSE(second_conflicting == second_at_selection);
  expect_selection_unchanged();
  EXPECT_NO_THROW(verify_selection(sel, observed_after_revisions())) << "nessuno stop dopo la dichiarazione in conflitto";
  const batch_proposal conflicted = analyze_batch(*p.store, {}, pool, never());
  EXPECT_NE(conflicted.canonical(), after.canonical());
  bool found = false;
  for (const excluded_input& e : conflicted.excluded) {
    if (e.input_ids == std::vector<std::uint64_t>{p.records[1].id}) {
      found = true;
      EXPECT_EQ(e.reason, "declared_conflict");
    }
  }
  EXPECT_TRUE(found) << "l'input con la dichiarazione in conflitto è escluso";
}

// ---------------------------------------------------------------- T-M3A-06 b: tre rami con run_branches

job_files_result one_file(sqlite_dag_store& store, const std::string& content) {
  const auto bytes = store_test::bytes_of(content);
  return job_files_result{store.write_files({artifact_content{bytes, "bin", "image", std::nullopt}}), {}};
}

std::string table(const fs::path& project, const std::string& name) {
  store_test::raw_db db(project, true);
  return db.query("SELECT * FROM " + name);
}

std::string count(const fs::path& project, const std::string& name) {
  store_test::raw_db db(project, true);
  return db.query("SELECT COUNT(*) FROM " + name);
}

// Colonne degli slot che l'esecuzione non tocca: nodo, ruolo di pubblicazione, esclusione.
std::string slot_fixed_columns(const fs::path& project) {
  store_test::raw_db db(project, true);
  return db.query("SELECT slot_name, IFNULL(target_node_key, 'null'), publish_role, detached FROM slots ORDER BY slot_name");
}

std::int64_t slot_generation(const fs::path& project, const std::string& slot) {
  store_test::raw_db db(project, true);
  const std::string row = db.query("SELECT current_generation FROM slots WHERE slot_name = '" + slot + "'");
  if (row.empty()) throw std::runtime_error("slot assente: " + slot);
  return std::stoll(row);
}

// Artefatto pubblicato dallo slot, in forma di riga di `raw_db::query`: `null|` se nessuno.
std::string slot_artifact(const fs::path& project, const std::string& slot) {
  store_test::raw_db db(project, true);
  return db.query("SELECT IFNULL(artifact_id, 'null') FROM slots WHERE slot_name = '" + slot + "'");
}

std::vector<artifact_record> product_artifacts_of(const sqlite_dag_store& s, const std::string& node_key) {
  std::vector<artifact_record> v;
  for (const artifact_record& a : s.artifacts_for(node_key)) {
    if (a.output_role != provenance_output_role) v.push_back(a);
  }
  return v;
}

std::size_t product_artifacts(const sqlite_dag_store& s, const std::string& node_key) {
  return product_artifacts_of(s, node_key).size();
}

TEST(VerifySelection, T_M3A_06b_only_the_hit_branch_stops_others_complete_and_the_project_reopens_identical) {
  raw_project p = raw_project_of("t06b");
  memory_pool pool(64 * imp::MiB);
  batch_proposal proposal_before;
  const calibration_selection selection = select_default(p, pool, proposal_before);
  ASSERT_EQ(selection.raw_inputs.size(), 3u);
  // Ogni ramo verifica il solo ingresso congelato del proprio file.
  std::vector<selected_decoder_input> frozen;
  for (const image& d : p.decoded) {
    const selected_decoder_input* f = frozen_for(selection, d);
    ASSERT_NE(f, nullptr);
    frozen.push_back(*f);
  }

  // Dopo la scelta, ramo 1: un byte del light cambiato (file del test, nella directory del test).
  {
    auto bytes = imp::read_file(p.paths[0]);
    bytes[bytes.size() - 1] ^= 0x01;
    imp::write_file(p.paths[0], bytes);
  }

  recipe_graph g;
  for (int i = 1; i <= 3; ++i) {
    g.add("n" + std::to_string(i),
          store_test::entry("test.u8b_branch", i, {recipe_input{"file", store_test::fake_digest(static_cast<char>('a' + i))}}));
  }
  const auto nodes = g.resolve();
  const std::map<std::string, std::string> keys = store_test::keys_of(nodes);
  p.store->apply_recipe(nodes, {{"u1", "n1"}, {"u2", "n2"}, {"u3", "n3"}});
  p.store.reset();

  const std::string inputs_before = table(p.project, "inputs");
  const std::string revisions_before = table(p.project, "input_classifications");
  const std::string nodes_before = table(p.project, "nodes");
  const std::string edges_before = table(p.project, "node_edges");
  const std::string snapshots_before = table(p.project, "snapshots") + table(p.project, "snapshot_entries");
  const std::string jobs_before = count(p.project, "jobs");
  const std::string slot_fixed_before = slot_fixed_columns(p.project);
  std::map<std::string, std::int64_t> generation_before;
  for (const char* slot : {"u1", "u2", "u3"}) {
    generation_before[slot] = slot_generation(p.project, slot);
    EXPECT_EQ(slot_artifact(p.project, slot), "null|\n") << slot << ": nessun artefatto prima dell'esecuzione";
  }

  auto store = sqlite_dag_store::open(p.project, open_intent::read_write);
  sqlite_dag_store* raw = store.get();
  const auto branch = [&](std::size_t i, bool change_decoder) {
    return [&, i, change_decoder](const job_record&) -> job_files_result {
      const input_record& rec = p.records[i];
      std::ifstream in(rec.path, std::ios::binary);
      const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      require_input_digest(digest_of(std::span<const std::uint8_t>(bytes.data(), bytes.size())),
                           content_digest{rec.sha256, rec.size_bytes}, rec.path);
      raw_decode_hooks hooks;
      if (change_decoder) hooks.after_unpack = [](raw_unpack_fields& f) { f.cblack[0] += 1; };
      const image decoded = decode_raw_buffer(std::span<const std::uint8_t>(bytes.data(), bytes.size()), hooks);
      calibration_selection own = selection;
      own.raw_inputs = {frozen[i]};
      verify_selection(own, {selected_decoder_input{frozen[i].name, *decoded.decoder_state, frozen[i].identity}});
      return one_file(*raw, "uscita del ramo " + std::to_string(i + 1));
    };
  };
  // I tre rami sono indipendenti: nessun arco fra n1, n2 e n3.
  std::vector<branch_files_job> jobs;
  jobs.push_back(branch_files_job{"u1", branch(0, false)});
  jobs.push_back(branch_files_job{"u2", branch(1, true)});
  jobs.push_back(branch_files_job{"u3", branch(2, false)});
  testing::internal::CaptureStderr();
  const std::vector<branch_result> results = run_branches(*store, std::move(jobs));
  testing::internal::GetCapturedStderr();
  std::map<std::string, branch_result> by_slot;
  for (const branch_result& b : results) by_slot[b.slot_name] = b;
  ASSERT_EQ(by_slot.size(), 3u);

  EXPECT_EQ(by_slot.at("u1").state, branch_state::failed);
  ASSERT_TRUE(by_slot.at("u1").outcome->stop.has_value());
  EXPECT_EQ(by_slot.at("u1").outcome->stop->category(), error_category::integrity);
  EXPECT_EQ(by_slot.at("u1").outcome->stop->code(), "input.digest_mismatch");
  EXPECT_EQ(by_slot.at("u2").state, branch_state::failed);
  ASSERT_TRUE(by_slot.at("u2").outcome->stop.has_value());
  const stop_error& u2 = *by_slot.at("u2").outcome->stop;
  EXPECT_EQ(u2.category(), error_category::integrity);
  EXPECT_EQ(u2.code(), "calib.decoder_state_changed");
  std::map<std::string, std::string> ctx(u2.context().begin(), u2.context().end());
  EXPECT_EQ(ctx["field"], "black_per_color");
  EXPECT_EQ(ctx["predicate"], "P-LOST");
  EXPECT_EQ(ctx["prerequisite"], frozen[1].name + ".decoder_state");
  EXPECT_EQ(by_slot.at("u3").state, branch_state::completed) << "il ramo non colpito completa";
  EXPECT_EQ(branches_exit_code(results), 2);

  // Solo il ramo 3 pubblica un'uscita: i rami fermati non hanno artefatti di prodotto.
  EXPECT_EQ(product_artifacts(*store, keys.at("n1")), 0u);
  EXPECT_EQ(product_artifacts(*store, keys.at("n2")), 0u);
  const std::vector<artifact_record> published = product_artifacts_of(*store, keys.at("n3"));
  ASSERT_EQ(published.size(), 1u);
  const std::string u3_artifact = std::to_string(published.front().id) + "|\n";
  store.reset();

  // Nessun nodo o ramo sostitutivo: input, revisioni, nodi, archi e snapshot invariati; un record di job per ramo.
  EXPECT_EQ(table(p.project, "inputs"), inputs_before);
  EXPECT_EQ(table(p.project, "input_classifications"), revisions_before);
  EXPECT_EQ(table(p.project, "nodes"), nodes_before);
  EXPECT_EQ(table(p.project, "node_edges"), edges_before);
  EXPECT_EQ(table(p.project, "snapshots") + table(p.project, "snapshot_entries"), snapshots_before);
  EXPECT_EQ(std::stoll(count(p.project, "jobs")), std::stoll(jobs_before) + 3) << "un record di job per ramo avviato";
  // Slot (criterio di 06b: generazione degli slot dei rami avviati, slot pubblicati del ramo 3). Ogni ramo avvia un job, e
  // l'avvio porta la generazione dello slot a quella del job (+1); u1 e u2 restano senza artefatto, u3 pubblica il suo.
  // Nodo, ruolo di pubblicazione ed esclusione degli slot non cambiano.
  EXPECT_EQ(slot_fixed_columns(p.project), slot_fixed_before);
  for (const char* slot : {"u1", "u2", "u3"}) {
    EXPECT_EQ(slot_generation(p.project, slot), generation_before[slot] + 1) << slot << ": generazione dello slot del ramo avviato";
  }
  EXPECT_EQ(slot_artifact(p.project, "u1"), "null|\n") << "un ramo fermato non pubblica";
  EXPECT_EQ(slot_artifact(p.project, "u2"), "null|\n") << "un ramo fermato non pubblica";
  EXPECT_EQ(slot_artifact(p.project, "u3"), u3_artifact) << "u3 pubblica l'artefatto del suo ramo";

  // Stato logico a progetto chiuso dopo l'ultima scrittura (nessuna scrittura successiva) uguale a quello dopo la riapertura;
  // la proposta ricalcolata con le stesse scelte è identica a quella prima della selezione.
  const std::string logical_before = store_test::dump_logical(p.project);
  auto reopened = sqlite_dag_store::open(p.project, open_intent::read_write);
  const batch_proposal proposal_after = analyze_batch(*reopened, {}, pool, never());
  reopened.reset();
  EXPECT_EQ(store_test::dump_logical(p.project), logical_before);
  EXPECT_EQ(proposal_after.canonical(), proposal_before.canonical());
}

// ---------------------------------------------------------------- arbitraggio di U8b (6 novembre 2026)

// CL-11: la selezione congela i light con digest e dimensione (confine OB-11), le dichiarazioni dell'analisi e, per ogni
// ingresso RAW, stato e identità del decoder. Verità: i digest e le dimensioni sono quelli registrati all'import; la
// tolleranza del nero è quella dei parametri; l'identità è quella del decoder di questo processo.
TEST(VerifySelection, T_M3A_11_selection_freezes_lights_with_digest_and_size_declarations_and_decoder_identity) {
  raw_project p = raw_project_of("t11_freeze_fields");
  memory_pool pool(64 * imp::MiB);
  batch_proposal proposal;
  const calibration_selection sel = select_default(p, pool, proposal);
  std::vector<content_digest> expected;
  for (const input_record& r : p.records) expected.push_back(content_digest{r.sha256, r.size_bytes});
  std::sort(expected.begin(), expected.end(), [](const content_digest& a, const content_digest& b) { return a.sha256 < b.sha256; });
  ASSERT_EQ(expected.size(), 3u);
  EXPECT_EQ(sel.lights, expected);
  for (const content_digest& d : sel.lights) EXPECT_GT(d.size_bytes, 0u);
  EXPECT_TRUE(sel.candidates.empty()) << "ricetta di default senza master: nessun candidato";
  EXPECT_EQ(sel.declarations.exposure_rel_tol, std::optional<double>(0.01));
  EXPECT_EQ(sel.declarations.temperature_abs_tol_c, std::optional<double>(1.0));
  EXPECT_EQ(sel.declarations.temp_policy, std::optional<temperature_policy>(temperature_policy::compare));
  EXPECT_EQ(sel.declarations.decoder_black_abs_tol_adu, std::optional<std::uint32_t>(10));
  ASSERT_EQ(sel.raw_inputs.size(), 3u);
  for (const selected_decoder_input& in : sel.raw_inputs) EXPECT_EQ(in.identity, current_decoder_identity()) << in.name;
}

// Record del decoder incoerente nello store: `black.pattern` con `pattern_rows × pattern_cols` diverso dal numero di valori
// ferma l'analisi con `batch.report_malformed`, mai uno stato del decoder inventato.
TEST(VerifySelection, T_M3A_11_black_pattern_inconsistent_with_its_dimensions_stops_with_report_malformed) {
  raw_project p = raw_project_of("t11_black_pattern");
  memory_pool pool(64 * imp::MiB);
  EXPECT_NO_THROW((void)analyze_batch(*p.store, {}, pool, never())) << "controllo positivo: il record intatto è letto";
  const auto slot = p.store->get_slot("batch.input." + std::to_string(p.records[0].id));
  ASSERT_TRUE(slot.has_value());
  const std::string old_key = slot->target_node_key;
  const std::optional<node_spec> old_node = p.store->get_node(old_key);
  ASSERT_TRUE(old_node.has_value());
  // Nodo con lo stesso contenuto salvo `decoder.black.pattern` (un valore con `pattern_rows × pattern_cols` = 0, come nei DNG sintetici) e la sua
  // chiave vera: lo store non lo rifiuta per la chiave, e l'incoerenza è del solo record.
  const auto with = [](const json& object, const std::string& key, json value) {
    json out = json::object();
    for (std::size_t i = 0; i < object.keys().size(); ++i) {
      out.set(object.keys()[i], object.keys()[i] == key ? value : object.items()[i]);
    }
    return out;
  };
  const json& decoder_record = *old_node->params().find("decoder");
  json one_value = json::array();
  one_value.push(json::uinteger(1));
  const json params = with(old_node->params(), "decoder",
                           with(decoder_record, "black", with(*decoder_record.find("black"), "pattern", std::move(one_value))));
  const node_spec bad(old_node->operator_id(), old_node->algorithm_version(), old_node->serialization_version(), params,
                      old_node->inputs(), old_node->precision(), old_node->backend(), old_node->seed());
  ASSERT_NE(bad.key(), old_key);
  p.store.reset();
  {
    store_test::raw_db db(p.project);
    for (const char* trigger : {"nodes_immutable_update", "edges_immutable_update", "entries_immutable_update"}) {
      ASSERT_EQ(db.exec(std::string("DROP TRIGGER ") + trigger), SQLITE_OK);
    }
    const std::string from = "'" + old_key + "'";
    const std::string to = "'" + bad.key() + "'";
    for (const std::string& sql : {
             "UPDATE nodes SET node_key = " + to + ", params_canonical = '" + canonical_json(bad.params()) + "' WHERE node_key = " + from,
             "UPDATE node_edges SET child_key = " + to + " WHERE child_key = " + from,
             "UPDATE node_edges SET parent_key = " + to + " WHERE parent_key = " + from,
             "UPDATE artifacts SET node_key = " + to + " WHERE node_key = " + from,
             "UPDATE slots SET target_node_key = " + to + " WHERE target_node_key = " + from,
             "UPDATE jobs SET node_key = " + to + " WHERE node_key = " + from,
             "UPDATE snapshot_entries SET node_key = " + to + " WHERE node_key = " + from}) {
      ASSERT_EQ(db.exec(sql), SQLITE_OK) << sql;
    }
  }
  auto store = sqlite_dag_store::open(p.project, open_intent::read_only);
  EXPECT_STOP(analyze_batch(*store, {}, pool, never()), error_category::integrity, "batch.report_malformed");
}

// ---------------------------------------------------------------- B14: candidato (c) (nodo proposto)

content_digest frame_digest(char c) { return content_digest{std::string(64, c), 2880}; }

calibration_selection selection_with_proposed_dark(const std::vector<content_digest>& frames, const std::string& node_key = std::string(64, 'n')) {
  calibration_selection s = selection_of({});
  selected_candidate c;
  c.role = master_role::dark;
  c.node_key = node_key;
  c.proposed_frames = frames;
  s.candidates.push_back(c);
  return s;
}

::testing::AssertionResult proposed_stops_with(const calibration_selection& sel, const std::vector<observed_proposed_node>& nodes,
                                               const std::string& field) {
  try {
    verify_selection(sel, {}, nodes);
  } catch (const stop_error& e) {
    if (e.category() != error_category::integrity || e.code() != "calib.proposed_node_changed") {
      return ::testing::AssertionFailure() << "stop diverso: " << e.what();
    }
    std::map<std::string, std::string> ctx(e.context().begin(), e.context().end());
    if (ctx["field"] != field) return ::testing::AssertionFailure() << "campo " << ctx["field"] << " invece di " << field;
    if (ctx["node_key"] != std::string(64, 'n')) return ::testing::AssertionFailure() << "node_key " << ctx["node_key"];
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << "nessuno stop";
}

TEST(VerifySelection, T_M3B_B14_proposed_candidate_whose_node_holds_exactly_the_chosen_frames_is_accepted) {
  const std::vector<content_digest> frames = {frame_digest('1'), frame_digest('2'), frame_digest('3')};
  EXPECT_NO_THROW(verify_selection(selection_with_proposed_dark(frames), {}, {observed_proposed_node{std::string(64, 'n'), frames}}));
}

TEST(VerifySelection, T_M3B_B14_proposed_candidate_whose_node_frame_set_differs_from_the_chosen_one_stops) {
  const std::vector<content_digest> chosen = {frame_digest('1'), frame_digest('2'), frame_digest('3')};
  const std::vector<std::pair<const char*, std::vector<content_digest>>> changed = {
      {"un frame sostituito", {frame_digest('1'), frame_digest('2'), frame_digest('4')}},
      {"un frame in meno", {frame_digest('1'), frame_digest('2')}},
      {"un frame in più", {frame_digest('1'), frame_digest('2'), frame_digest('3'), frame_digest('4')}},
      {"ordine diverso", {frame_digest('2'), frame_digest('1'), frame_digest('3')}},
      {"nodo vuoto", {}},
  };
  for (const auto& [name, frames] : changed) {
    SCOPED_TRACE(name);
    EXPECT_TRUE(proposed_stops_with(selection_with_proposed_dark(chosen), {observed_proposed_node{std::string(64, 'n'), frames}}, "frames"));
  }
}

TEST(VerifySelection, T_M3B_B14_proposed_candidate_whose_node_is_not_observed_is_a_lost_prerequisite) {
  const std::vector<content_digest> chosen = {frame_digest('1'), frame_digest('2')};
  EXPECT_TRUE(proposed_stops_with(selection_with_proposed_dark(chosen), {}, "observation"));
  EXPECT_TRUE(proposed_stops_with(selection_with_proposed_dark(chosen), {observed_proposed_node{std::string(64, 'x'), chosen}}, "observation"))
      << "un nodo con un'altra chiave non è l'osservazione di questo";
}

TEST(VerifySelection, T_M3B_B14_slot_candidate_without_proposed_frames_needs_no_node_observation) {
  calibration_selection s = selection_of({});
  selected_candidate c;
  c.role = master_role::dark;
  c.node_key = std::string(64, 'n');
  s.candidates.push_back(c);
  EXPECT_NO_THROW(verify_selection(s, {}, {}));
}

TEST(VerifySelection, T_M3B_B14_input_candidate_needs_no_node_observation) {
  calibration_selection s = selection_of({});
  selected_candidate c;
  c.role = master_role::dark;
  c.input = frame_digest('d');
  s.candidates.push_back(c);
  EXPECT_NO_THROW(verify_selection(s, {}, {}));
}

TEST(VerifySelection, T_M3B_B14_decoder_state_stop_precedes_the_proposed_node_check) {
  const std::vector<content_digest> chosen = {frame_digest('1')};
  calibration_selection s = selection_with_proposed_dark(chosen);
  s.raw_inputs.push_back(input_of("light.dng"));
  decoder_state changed = base_state();
  changed.black = 600;
  EXPECT_TRUE(stops_with(s, {input_of("light.dng", changed)}, "black", "light.dng.decoder_state"));
}

TEST(VerifySelection, T_M3B_B14_two_proposed_candidates_are_each_checked_against_their_own_node) {
  const std::vector<content_digest> dark_frames = {frame_digest('1'), frame_digest('2')};
  const std::vector<content_digest> flat_frames = {frame_digest('5'), frame_digest('6')};
  calibration_selection s = selection_of({});
  selected_candidate dark;
  dark.role = master_role::dark;
  dark.node_key = std::string(64, 'n');
  dark.proposed_frames = dark_frames;
  selected_candidate flat;
  flat.role = master_role::flat;
  flat.node_key = std::string(64, 'm');
  flat.proposed_frames = flat_frames;
  s.candidates = {dark, flat};
  EXPECT_NO_THROW(verify_selection(s, {}, {observed_proposed_node{std::string(64, 'm'), flat_frames}, observed_proposed_node{std::string(64, 'n'), dark_frames}}));
  EXPECT_THROW(verify_selection(s, {}, {observed_proposed_node{std::string(64, 'n'), flat_frames}, observed_proposed_node{std::string(64, 'm'), dark_frames}}),
               stop_error);
}

// Controllo termico di `flat.bias` non calcolato con un master candidato (c): la selezione non gira senza nuova analisi.
calibration_selection flat_bias_selection_with_proposed(master_role role, const std::vector<content_digest>& frames) {
  calibration_selection s = selection_of({});
  s.recipe.flat = flat_chain::bias;
  selected_candidate c;
  c.role = role;
  c.node_key = std::string(64, 'n');
  c.proposed_frames = frames;
  s.candidates.push_back(c);
  return s;
}

TEST(VerifySelection, T_M3B_B16_flat_bias_selection_with_a_proposed_bias_or_flat_stops_with_thermal_check_pending) {
  const std::vector<content_digest> frames = {frame_digest('1'), frame_digest('2'), frame_digest('3')};
  for (const master_role role : {master_role::bias, master_role::flat}) {
    SCOPED_TRACE(role == master_role::bias ? "bias" : "flat");
    const calibration_selection sel = flat_bias_selection_with_proposed(role, frames);
    try {
      verify_selection(sel, {}, {observed_proposed_node{std::string(64, 'n'), frames}});
      ADD_FAILURE() << "nessuno stop";
    } catch (const stop_error& e) {
      EXPECT_EQ(e.category(), error_category::integrity) << e.what();
      EXPECT_EQ(e.code(), "calib.thermal_check_pending") << e.what();
      const std::map<std::string, std::string> ctx(e.context().begin(), e.context().end());
      EXPECT_EQ(ctx.size(), 2u);
      EXPECT_EQ(ctx.count("node_key") ? ctx.at("node_key") : "", std::string(64, 'n'));
      EXPECT_EQ(ctx.count("role") ? ctx.at("role") : "", role == master_role::bias ? "bias" : "flat");
    }
  }
}

TEST(VerifySelection, T_M3B_B16_flat_bias_selection_with_a_proposed_dark_or_a_slot_bias_is_accepted) {
  const std::vector<content_digest> frames = {frame_digest('1'), frame_digest('2'), frame_digest('3')};
  calibration_selection dark = flat_bias_selection_with_proposed(master_role::dark, frames);
  EXPECT_NO_THROW(verify_selection(dark, {}, {observed_proposed_node{std::string(64, 'n'), frames}}));
  calibration_selection slot = flat_bias_selection_with_proposed(master_role::bias, frames);
  slot.candidates[0].proposed_frames.reset();
  EXPECT_NO_THROW(verify_selection(slot, {}));
}

}  // namespace
