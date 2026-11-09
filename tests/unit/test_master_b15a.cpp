// M3.b, unità B15a: master da RAW, parte 1 (stato del decoder, nodo e preflight). ADR-16.b §b.7 (master), §b.11;
// C-02.b §b.2 («Selezione ed esecuzione», «RAW»), §b.7, §b.10 (righe RAW); decisioni L9 e L10.
// Test: T-M3A-CAL-07 (stato del decoder nei parametri del nodo e nel descrittore, chiave, invarianti, RAW con FITS),
// T-M3B-C02-08 parte RAW a livello di preflight del nodo (un campo strutturale alla volta, nero effettivo alla tolleranza
// e alla tolleranza + 1, `offset_adu`).
//
// Nessun valore atteso viene dal prodotto: codici, categorie, campi strutturali e nero effettivo
// `b(r, c) = black + black_per_color[FC(r, c)] + P(r, c)` sono scritti qui dai paragrafi citati. Gli stati del decoder
// sono costruiti a mano (nessuna decodifica, nessun file: i percorsi dei frame non esistono).
//
// Convenzioni assunte, dichiarate nel rapporto dell'unità:
// - il preflight RAW del nodo è puro e sta nella ricetta: ogni stop si controlla su `validate_master_recipe` e su
//   `make_master_node` (nessun nodo);
// - gli invarianti del descrittore (`master.decoder_state_param`) si controllano sulla catena ricetta → nodo →
//   descrittore: lo stop può venire da `make_master_node` o da `master_descriptor_from_node`; uno stato che viola gli
//   invarianti è uguale in tutti i frame, così nessun confronto strutturale o di nero può precedere il controllo;
// - lunghezza e presenza parziale di `frame_decoder_states` si provano su un nodo modificato a mano: elemento tolto,
//   aggiunto o sostituito da `null`;
// - `decoder_black_abs_tol_adu` assente con frame RAW o presente con soli FITS: invalid_input
//   `master.param.decoder_black_abs_tol_adu` (C-02.b §b.10 non elenca il codice; scelta per analogia con
//   `master.param.<nome>` e con ADR-16.b §b.7 «obbligatorio [...] vietato altrimenti»);
// - «senza letture di file» (T-M3A-CAL-07) si prova con percorsi dei frame inesistenti al posto di un contatore delle
//   letture: una lettura fallirebbe con uno stop `io`, quindi un nodo e un descrittore ottenuti provano che nessun file
//   è stato letto;
// - contesto di `master.mismatch.<campo>`: `sinistra` è il valore del riferimento canonico, `destra` quello del frame
//   diverso, entrambi nella forma canonica JSON (RFC 8785) della chiave del record del decoder (ADR-08.b §2.9: `crop`,
//   `cfa_color_index`, `black.pattern_rows`, `black.pattern_cols`, `linearization`, `linearization_curve_sha256`),
//   scritta a mano nel test; `tolleranza` = `esatta`;
// - contesto di `master.mismatch.decoder_black`: `differenza_massima_adu`, `riga_periodo`, `colonna_periodo`,
//   `tolleranza_adu`; con più celle alla stessa differenza massima la posizione è la prima in ordine di riga.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/master.hpp"
#include "astro/node.hpp"
#include "expect_stop.hpp"
#include "simd_support.hpp"

namespace {

using namespace astro;

constexpr std::uint32_t kTol = 5;

// ---------------------------------------------------------------- stati del decoder scritti a mano

content_digest fake_digest(std::size_t k) {
  const std::vector<std::uint8_t> b = {'B', '1', '5', 'a', static_cast<std::uint8_t>(k & 0xff)};
  return digest_of(b);
}

std::array<std::uint8_t, 32> curve(std::uint8_t fill) {
  std::array<std::uint8_t, 32> c{};
  c.fill(fill);
  return c;
}

// Stato valido (ADR-03.b §b.2): buffer 12 × 10, finestra 8 × 6 in (2, 2), RGGB con verdi di indice 1 e 3 e nero per
// colore uguale sui due verdi (scambiare gli indici dei verdi non cambia il nero effettivo), motivo 1 × 1 nullo.
// Nero effettivo per sito: R 104, G 102, B 106.
decoder_state base_state() {
  decoder_state s;
  s.raw_width = 12;
  s.raw_height = 10;
  s.window = decoder_window{2, 2, 8, 6};
  s.black = 100;
  s.cfa_color_index = {0, 1, 3, 2};
  s.black_per_color = {4, 2, 2, 6};
  s.black_pattern_rows = 1;
  s.black_pattern_cols = 1;
  s.black_pattern = {0};
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

decoder_identity base_identity() {
  return decoder_identity("0.22.3-Release", 5635, 0x1234, std::string(64, 'a'));
}

frame_decoder_state base_frame_state() { return frame_decoder_state{base_state(), base_identity()}; }

// Ricetta RAW valida senza file: `n` frame con lo stesso stato del decoder, tolleranza del nero dichiarata.
master_recipe raw_recipe(master_role role, std::size_t n) {
  master_recipe r;
  r.role = role;
  for (std::size_t k = 0; k < n; ++k) {
    master_frame_spec s;
    s.path = "inesistente/frame" + std::to_string(k) + ".dng";
    s.digest = fake_digest(k);
    s.decoder = base_frame_state();
    r.frames.push_back(s);
  }
  r.combine = combine_method::mean;
  r.rejection = master_rejection::none;
  if (role == master_role::flat) {
    r.flat_norm = flat_normalization::none;
    r.flat_level_rel_tol = 0.25;
    r.flat_min_signal_adu = 1.0;
    r.offset_adu = 7;  // L9: dichiarato nella ricetta, richiesto per il flat RAW
  }
  r.exposure_rel_tol = 1e-9;
  r.temperature_abs_tol_c = 0.5;
  r.temp_policy = temperature_policy::compare;
  r.decoder_black_abs_tol_adu = kTol;
  r.simd = simd_test::requested();
  r.output_type = sample_type::f64;
  return r;
}

// Indice del frame di riferimento canonico (digest minimo) e di uno degli altri.
std::size_t reference_index(const master_recipe& r) {
  std::size_t best = 0;
  for (std::size_t k = 1; k < r.frames.size(); ++k)
    if (r.frames[k].digest.sha256 < r.frames[best].digest.sha256) best = k;
  return best;
}
std::size_t other_index(const master_recipe& r) { return reference_index(r) == 0 ? 1 : 0; }

void expect_recipe_stop(const master_recipe& r, error_category cat, const std::string& code) {
  EXPECT_STOP(validate_master_recipe(r), cat, code);
  EXPECT_STOP(make_master_node(r), cat, code);
}

void node_then_descriptor(const master_recipe& r) { (void)master_descriptor_from_node(make_master_node(r)); }

// Stop di `validate_master_recipe` e di `make_master_node` con lo stesso codice; restituisce quello di `make_master_node`.
std::optional<stop_error> recipe_stop(const master_recipe& r, error_category cat, const std::string& code) {
  EXPECT_STOP(validate_master_recipe(r), cat, code);
  try {
    (void)make_master_node(r);
    ADD_FAILURE() << "stop_error atteso da make_master_node: " << code;
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), cat) << e.what();
    EXPECT_EQ(e.code(), code) << e.what();
    return e;
  }
  return std::nullopt;
}

std::optional<std::string> context_of(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

// Campi del decoder del progetto (U13 §8) coerenti fra loro: tasselli 16 × 16 (le dimensioni delle strisce restano 0).
dng_decoder_fields valid_dng_fields() {
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
  return f;
}

// Stato del decoder del progetto: instradamento senza motivo, campi DNG presenti, campi propri di LibRaw a zero.
void make_astro_dng(decoder_state& s) {
  s.route.decoder = raw_decoder_kind::astro_dng;
  s.route.reason.clear();
  s.route.compression = 8;
  s.route.sample_format = 1;
  s.route.bits_per_sample = 32;
  s.process_warnings = 0;
  s.raw_inset_crops = {};
  s.flip = 0;
  s.dng = valid_dng_fields();
}

json params_replacing(const node_spec& node, const std::string& key, json value) {
  json out = json::object();
  const std::vector<std::string>& keys = node.params().keys();
  const std::vector<json>& items = node.params().items();
  for (std::size_t i = 0; i < keys.size(); ++i) out.set(keys[i], keys[i] == key ? value : items[i]);
  return out;
}

node_spec with_params(const node_spec& node, json params) {
  return node_spec(node.operator_id(), node.algorithm_version(), node.serialization_version(), std::move(params),
                   node.inputs(), node.precision(), node.backend(), node.seed());
}

// ================================================================ T-M3A-CAL-07

// ADR-16.b §b.7 «Dove sta» e «Descrittore»: un elemento per ingresso frame, nell'ordine canonico degli ingressi (digest
// crescente), letto dai parametri del nodo senza leggere file (i percorsi non esistono). La ricetta elenca i frame in
// ordine non canonico; ogni stato si distingue per un campo non strutturale (bianco) e per l'identità.
TEST(MasterB15a, T_M3A_CAL_07_frame_decoder_states_in_node_params_and_descriptor_in_canonical_order) {
  master_recipe r = raw_recipe(master_role::dark, 3);
  std::vector<master_frame_spec> frames = {r.frames[0], r.frames[1], r.frames[2]};
  for (std::size_t k = 0; k < frames.size(); ++k) {
    frames[k].decoder->state.white = 4000 + static_cast<std::uint32_t>(k);
    frames[k].decoder->identity.libraw_capabilities = 0x1234 + static_cast<std::uint32_t>(k);
  }
  // Ordine canonico calcolato qui: indici per digest crescente.
  std::vector<std::size_t> canonical = {0, 1, 2};
  std::sort(canonical.begin(), canonical.end(),
            [&](std::size_t a, std::size_t b) { return frames[a].digest.sha256 < frames[b].digest.sha256; });
  // Ricetta in ordine inverso a quello canonico.
  r.frames = {frames[canonical[2]], frames[canonical[0]], frames[canonical[1]]};

  const node_spec node = make_master_node(r);
  const json* states = node.params().find("frame_decoder_states");
  ASSERT_NE(states, nullptr) << "frame_decoder_states assente dai parametri del nodo";
  ASSERT_EQ(states->type(), json::kind::array);
  EXPECT_EQ(states->items().size(), node.inputs().size());

  const master_descriptor d = master_descriptor_from_node(node);
  ASSERT_TRUE(d.frame_decoder_states.has_value()) << "frame_decoder_states assente dal descrittore";
  ASSERT_EQ(d.frame_decoder_states->size(), 3u);
  for (std::size_t k = 0; k < 3; ++k) {
    SCOPED_TRACE("posizione canonica " + std::to_string(k));
    EXPECT_EQ((*d.frame_decoder_states)[k], *frames[canonical[k]].decoder);
  }
  EXPECT_EQ(d.combine_count, 3u);

  // Permutazioni dell'elenco: stessa chiave del nodo.
  master_recipe permuted = r;
  permuted.frames = {frames[0], frames[1], frames[2]};
  EXPECT_EQ(make_master_node(permuted).key(), node.key());
  permuted.frames = {frames[1], frames[2], frames[0]};
  EXPECT_EQ(make_master_node(permuted).key(), node.key());
}

struct state_change {
  const char* name;
  std::function<void(frame_decoder_state&)> apply;
  bool structural;  // campo confrontato per uguaglianza fra frame (§b.7)
};

// Un campo di contenuto alla volta (ADR-03.b §b.2, U13 §8 per l'instradamento, identità di §b.7), ogni stato modificato
// valido. I campi strutturali non cambiano il nero effettivo.
std::vector<state_change> every_field() {
  return {
      {"raw_width", [](frame_decoder_state& f) { f.state.raw_width += 2; }, true},
      {"raw_height", [](frame_decoder_state& f) { f.state.raw_height += 2; }, true},
      {"window", [](frame_decoder_state& f) { f.state.window.x0 = 0; }, true},
      {"black", [](frame_decoder_state& f) { f.state.black += 1; }, false},
      {"cfa_color_index", [](frame_decoder_state& f) { f.state.cfa_color_index = {0, 3, 1, 2}; }, true},
      {"black_per_color", [](frame_decoder_state& f) { f.state.black_per_color[0] += 1; }, false},
      {"black_pattern_rows",
       [](frame_decoder_state& f) {
         f.state.black_pattern_rows = 2;
         f.state.black_pattern = {0, 0};
       },
       true},
      {"black_pattern_cols",
       [](frame_decoder_state& f) {
         f.state.black_pattern_cols = 2;
         f.state.black_pattern = {0, 0};
       },
       true},
      {"black_pattern", [](frame_decoder_state& f) { f.state.black_pattern = {1}; }, false},
      {"white", [](frame_decoder_state& f) { f.state.white += 1; }, false},
      {"data_maximum", [](frame_decoder_state& f) { f.state.data_maximum.reset(); }, false},
      {"linear_max", [](frame_decoder_state& f) { f.state.linear_max = std::array<std::int64_t, 4>{1, 2, 3, 4}; }, false},
      {"linearization",
       [](frame_decoder_state& f) {
         f.state.linearization = decoder_linearization::decoder_curve;
         f.state.curve_sha256 = curve(0xA1);
       },
       true},
      {"process_warnings", [](frame_decoder_state& f) { f.state.process_warnings = 4; }, false},  // LIBRAW_WARN_BAD_CAMERA_WB
      {"raw_inset_crops", [](frame_decoder_state& f) { f.state.raw_inset_crops[0] = {1, 1, 4, 4}; }, false},
      {"flip", [](frame_decoder_state& f) { f.state.flip = 3; }, false},
      {"route.reason", [](frame_decoder_state& f) { f.state.route.reason = "tiff_not_dng"; }, false},
      {"route.decoder", [](frame_decoder_state& f) { make_astro_dng(f.state); }, false},
      {"route.compression", [](frame_decoder_state& f) { f.state.route.compression = 7; }, false},
      {"route.sample_format", [](frame_decoder_state& f) { f.state.route.sample_format = 3; }, false},
      {"route.bits_per_sample", [](frame_decoder_state& f) { f.state.route.bits_per_sample = 12; }, false},
      {"decoder_identity.libraw_version", [](frame_decoder_state& f) { f.identity.libraw_version = "0.22.4-Release"; }, false},
      {"decoder_identity.libraw_version_number", [](frame_decoder_state& f) { f.identity.libraw_version_number += 1; }, false},
      {"decoder_identity.libraw_capabilities", [](frame_decoder_state& f) { f.identity.libraw_capabilities ^= 1; }, false},
      {"decoder_identity.libraw_tarball_sha256",
       [](frame_decoder_state& f) { f.identity.libraw_tarball_sha256 = std::string(64, 'b'); }, false},
  };
}

// §b.7: lo stato del decoder entra nella chiave del nodo del master. La parte «chiave della calibrazione» (ingresso
// `from_node`) è di M3.c, con la ricetta `.b` della calibrazione: la ricetta `.a` rifiuta `frame_decoder_states`.
// Un campo cambiato in tutti i frame (preflight valido anche per i campi strutturali), e per i campi non strutturali
// anche in un solo frame: la chiave cambia. `curve_sha256` con uno stato base a curva.
TEST(MasterB15a, T_M3A_CAL_07_each_decoder_state_field_enters_the_master_key) {
  const master_recipe base = raw_recipe(master_role::dark, 3);
  const std::string base_key = make_master_node(base).key();
  for (const state_change& c : every_field()) {
    SCOPED_TRACE(c.name);
    master_recipe all = base;
    for (master_frame_spec& f : all.frames) c.apply(*f.decoder);
    EXPECT_NE(make_master_node(all).key(), base_key) << "campo cambiato in tutti i frame";
    if (!c.structural) {
      master_recipe one = base;
      c.apply(*one.frames[other_index(one)].decoder);
      EXPECT_NE(make_master_node(one).key(), base_key) << "campo cambiato in un solo frame";
    }
  }
  {
    SCOPED_TRACE("curve_sha256");
    master_recipe with_curve = base;
    for (master_frame_spec& f : with_curve.frames) {
      f.decoder->state.linearization = decoder_linearization::decoder_curve;
      f.decoder->state.curve_sha256 = curve(0xA1);
    }
    master_recipe other_curve = with_curve;
    for (master_frame_spec& f : other_curve.frames) f.decoder->state.curve_sha256 = curve(0xB2);
    EXPECT_NE(make_master_node(with_curve).key(), make_master_node(other_curve).key());
  }
  // Un nodo da FITS (stessi digest, nessuno stato) ha una chiave diversa da quella del nodo da RAW.
  master_recipe fits = base;
  for (master_frame_spec& f : fits.frames) f.decoder.reset();
  fits.decoder_black_abs_tol_adu.reset();
  EXPECT_NE(make_master_node(fits).key(), base_key);
}

// §b.7 «Descrittore», invarianti di ADR-03.b §b.2 che non dipendono dall'immagine. Lo stato invalido è in tutti i frame.
TEST(MasterB15a, T_M3A_CAL_07_state_invariants_violated_stop_with_decoder_state_param) {
  struct bad_case {
    const char* name;
    std::function<void(decoder_state&)> apply;
  };
  const std::vector<bad_case> cases = {
      {"finestra oltre raw_width", [](decoder_state& s) { s.window.x0 = 5; }},  // 5 + 8 > 12
      {"finestra oltre raw_height", [](decoder_state& s) { s.window.y0 = 5; }},  // 5 + 6 > 10
      {"x0 + width oltre 2^32 (senza overflow)",
       [](decoder_state& s) {
         s.raw_width = std::numeric_limits<std::uint32_t>::max();
         s.window.x0 = std::numeric_limits<std::uint32_t>::max() - 3;
       }},
      {"y0 + height oltre 2^32 (senza overflow)",
       [](decoder_state& s) {
         s.raw_height = std::numeric_limits<std::uint32_t>::max();
         s.window.y0 = std::numeric_limits<std::uint32_t>::max() - 3;
       }},
      {"motivo con valori diversi da rows · cols", [](decoder_state& s) { s.black_pattern = {0, 0}; }},
      {"motivo con una sola dimensione nulla",
       [](decoder_state& s) {
         s.black_pattern_rows = 0;
         s.black_pattern_cols = 1;
         s.black_pattern.clear();
       }},
      {"motivo oltre 4098 valori",
       [](decoder_state& s) {
         s.black_pattern_rows = 65;
         s.black_pattern_cols = 64;  // 4160
         s.black_pattern.assign(4160, 0);
       }},
      {"curva presente con linearizzazione identity", [](decoder_state& s) { s.curve_sha256 = curve(0xA1); }},
      {"decoder_curve senza curva", [](decoder_state& s) { s.linearization = decoder_linearization::decoder_curve; }},
      {"indice di colore fuori da 0…3", [](decoder_state& s) { s.cfa_color_index = {0, 1, 4, 2}; }},
      {"indici di colore con due rossi e nessun blu", [](decoder_state& s) { s.cfa_color_index = {0, 0, 1, 2}; }},
      {"finestra di larghezza nulla", [](decoder_state& s) { s.window.width = 0; }},
      {"astro_dng senza campi DNG",
       [](decoder_state& s) {
         make_astro_dng(s);
         s.dng.reset();
       }},
      {"libraw con campi DNG", [](decoder_state& s) { s.dng = valid_dng_fields(); }},
  };
  for (const bad_case& c : cases) {
    SCOPED_TRACE(c.name);
    master_recipe r = raw_recipe(master_role::dark, 3);
    for (master_frame_spec& f : r.frames) c.apply(f.decoder->state);
    EXPECT_STOP(node_then_descriptor(r), error_category::invalid_input, "master.decoder_state_param");
  }
  // Controllo positivo: il motivo a 4098 valori (limite incluso) è valido.
  master_recipe at_limit = raw_recipe(master_role::dark, 3);
  for (master_frame_spec& f : at_limit.frames) {
    f.decoder->state.black_pattern_rows = 2;
    f.decoder->state.black_pattern_cols = 2049;
    f.decoder->state.black_pattern.assign(4098, 0);
  }
  EXPECT_NO_THROW(node_then_descriptor(at_limit));
}

// §b.7 «Descrittore», U13 §3: uno stato che il descrittore non rileggerebbe (motivo dell'instradamento LibRaw fuori da
// {`not_tiff`, `tiff_not_dng`, `dng_libraw_compression`}) si ferma già nel preflight della ricetta, non solo in
// `master_descriptor_from_node`: preflight e descrittore accettano gli stessi stati. Stato uguale in tutti i frame.
TEST(MasterB15a, T_M3A_CAL_07_route_reason_outside_the_domain_stops_in_the_recipe_with_decoder_state_param) {
  master_recipe r = raw_recipe(master_role::dark, 3);
  for (master_frame_spec& f : r.frames) f.decoder->state.route.reason = "xyz";
  expect_recipe_stop(r, error_category::invalid_input, "master.decoder_state_param");
}

// §b.7 «Descrittore»: presente per tutti i frame o assente, lunghezza uguale al numero di ingressi frame. Nodo modificato a
// mano a partire da un nodo valido.
TEST(MasterB15a, T_M3A_CAL_07_frame_decoder_states_length_or_partial_presence_stop_with_decoder_state_param) {
  const node_spec node = make_master_node(raw_recipe(master_role::dark, 3));
  const json* states = node.params().find("frame_decoder_states");
  ASSERT_NE(states, nullptr) << "frame_decoder_states assente dai parametri del nodo";
  ASSERT_EQ(states->type(), json::kind::array);
  ASSERT_EQ(states->items().size(), 3u);
  const std::vector<json>& items = states->items();
  const auto descriptor_of = [&](json replaced) {
    return master_descriptor_from_node(with_params(node, params_replacing(node, "frame_decoder_states", std::move(replaced))));
  };
  {
    SCOPED_TRACE("un elemento in meno");
    json a = json::array();
    a.push(items[0]);
    a.push(items[1]);
    EXPECT_STOP(descriptor_of(a), error_category::invalid_input, "master.decoder_state_param");
  }
  {
    SCOPED_TRACE("un elemento in più");
    json a = json::array();
    for (const json& i : items) a.push(i);
    a.push(items[2]);
    EXPECT_STOP(descriptor_of(a), error_category::invalid_input, "master.decoder_state_param");
  }
  {
    SCOPED_TRACE("vuoto con tre ingressi");
    EXPECT_STOP(descriptor_of(json::array()), error_category::invalid_input, "master.decoder_state_param");
  }
  {
    SCOPED_TRACE("presente per due frame su tre");
    json a = json::array();
    a.push(items[0]);
    a.push(json::null());
    a.push(items[2]);
    EXPECT_STOP(descriptor_of(a), error_category::invalid_input, "master.decoder_state_param");
  }
  {
    SCOPED_TRACE("non è un array");
    EXPECT_STOP(descriptor_of(json::object()), error_category::invalid_input, "master.decoder_state_param");
  }
  // Controllo positivo: lo stesso array ricostruito dà il descrittore.
  json same = json::array();
  for (const json& i : items) same.push(i);
  EXPECT_NO_THROW((void)descriptor_of(same));
}

// §b.7, U13 §8: stato del decoder del progetto (`astro_dng`, campi DNG presenti). Nodo e descrittore: stati uguali
// all'andata e ritorno, nell'ordine canonico (tutti i frame hanno lo stesso stato). Ogni campo DNG cambiato in tutti i
// frame cambia la chiave del nodo.
TEST(MasterB15a, T_M3A_CAL_07_astro_dng_state_round_trips_and_each_dng_field_enters_the_key) {
  master_recipe base = raw_recipe(master_role::dark, 3);
  for (master_frame_spec& f : base.frames) make_astro_dng(f.decoder->state);
  const node_spec node = make_master_node(base);
  const master_descriptor d = master_descriptor_from_node(node);
  ASSERT_TRUE(d.frame_decoder_states.has_value());
  ASSERT_EQ(d.frame_decoder_states->size(), 3u);
  for (std::size_t k = 0; k < 3; ++k) {
    SCOPED_TRACE("posizione canonica " + std::to_string(k));
    EXPECT_EQ((*d.frame_decoder_states)[k], *base.frames[0].decoder);
    EXPECT_TRUE((*d.frame_decoder_states)[k].state.dng.has_value());
  }
  struct dng_change {
    const char* name;
    std::function<void(dng_decoder_fields&)> apply;
  };
  const std::vector<dng_change> changes = {
      {"dng_version", [](dng_decoder_fields& f) { f.dng_version = {1, 6, 0, 0}; }},
      {"dng_backward_version", [](dng_decoder_fields& f) { f.dng_backward_version = {1, 3, 0, 0}; }},
      {"tasselli → strisce",
       [](dng_decoder_fields& f) {
         f.tiles = false;
         f.tile_width = 0;
         f.tile_length = 0;
         f.rows_per_strip = 7;
       }},
      {"tile_width", [](dng_decoder_fields& f) { f.tile_width = 32; }},
      {"segment_count", [](dng_decoder_fields& f) { f.segment_count = 2; }},
      {"predictor", [](dng_decoder_fields& f) { f.predictor = 2; }},
      {"little_endian", [](dng_decoder_fields& f) { f.little_endian = false; }},
      {"zlib_runtime_version", [](dng_decoder_fields& f) { f.zlib_runtime_version = "1.3.2"; }},
      {"orientation", [](dng_decoder_fields& f) { f.orientation = 1; }},
      {"opcode_lists", [](dng_decoder_fields& f) { f.opcode_lists = {"OpcodeList1"}; }},
      {"tile_length", [](dng_decoder_fields& f) { f.tile_length = 32; }},
      {"default_crop_origin",
       [](dng_decoder_fields& f) { f.default_crop_origin = std::array<dng_rational, 2>{dng_rational{1, 1}, dng_rational{2, 1}}; }},
      {"default_crop_size",
       [](dng_decoder_fields& f) { f.default_crop_size = std::array<dng_rational, 2>{dng_rational{8, 1}, dng_rational{6, 1}}; }},
      {"metadata_rejected", [](dng_decoder_fields& f) { f.metadata_rejected = {"Orientation"}; }},
  };
  for (const dng_change& c : changes) {
    SCOPED_TRACE(c.name);
    master_recipe changed = base;
    for (master_frame_spec& f : changed.frames) c.apply(*f.decoder->state.dng);
    std::optional<node_spec> n;
    EXPECT_NO_THROW(n.emplace(make_master_node(changed)));
    if (!n) continue;
    EXPECT_NE(n->key(), node.key());
    const master_descriptor dc = master_descriptor_from_node(*n);
    ASSERT_TRUE(dc.frame_decoder_states.has_value());
    EXPECT_EQ(dc.frame_decoder_states->front(), *changed.frames[0].decoder) << "andata e ritorno";
  }
  {
    // `rows_per_strip` da solo: vale solo con le strisce, quindi base a strisce (7 righe) contro 8 righe.
    SCOPED_TRACE("rows_per_strip");
    const auto strips = [&](std::uint32_t rows) {
      master_recipe r = base;
      for (master_frame_spec& f : r.frames) {
        dng_decoder_fields& d = *f.decoder->state.dng;
        d.tiles = false;
        d.tile_width = 0;
        d.tile_length = 0;
        d.rows_per_strip = rows;
      }
      return r;
    };
    const master_recipe seven = strips(7), eight = strips(8);
    std::optional<node_spec> n7, n8;
    EXPECT_NO_THROW(n7.emplace(make_master_node(seven)));
    EXPECT_NO_THROW(n8.emplace(make_master_node(eight)));
    if (n7 && n8) {
      EXPECT_NE(n7->key(), n8->key());
      const master_descriptor d8 = master_descriptor_from_node(*n8);
      ASSERT_TRUE(d8.frame_decoder_states.has_value());
      EXPECT_EQ(d8.frame_decoder_states->front(), *eight.frames[0].decoder) << "andata e ritorno";
    }
  }
}

// §b.7 «Descrittore»: un elemento di un nodo modificato a mano, ben formato (chiavi e tipi del record del decoder di
// ADR-08.b §2.9, `decoder_state_of_record` lo legge) ma con uno stato che viola un invariante di ADR-03.b §b.2:
// `master_descriptor_from_node` si ferma, senza ricetta e senza `make_master_node`.
TEST(MasterB15a, T_M3A_CAL_07_hand_edited_node_element_violating_an_invariant_stops_with_decoder_state_param) {
  const node_spec node = make_master_node(raw_recipe(master_role::dark, 3));
  const json* states = node.params().find("frame_decoder_states");
  ASSERT_NE(states, nullptr);
  ASSERT_EQ(states->items().size(), 3u);
  const auto replacing = [](const json& object, const std::string& key, json value) {
    json out = json::object();
    bool found = false;
    for (std::size_t i = 0; i < object.keys().size(); ++i) {
      found = found || object.keys()[i] == key;
      out.set(object.keys()[i], object.keys()[i] == key ? value : object.items()[i]);
    }
    EXPECT_TRUE(found) << "chiave " << key << " assente dall'elemento";
    return out;
  };
  struct bad_element {
    const char* name;
    const char* key;
    std::int64_t value;
  };
  // Finestra (2, 2, 8, 6) dello stato base: raw_width 9 < 2 + 8, raw_height 7 < 2 + 6.
  for (const bad_element& c : {bad_element{"finestra oltre raw_width", "raw_width", 9},
                               bad_element{"finestra oltre raw_height", "raw_height", 7}}) {
    for (std::size_t k = 0; k < 3; ++k) {
      SCOPED_TRACE(std::string(c.name) + ", elemento " + std::to_string(k));
      json a = json::array();
      for (std::size_t i = 0; i < 3; ++i)
        a.push(i == k ? replacing(states->items()[i], c.key, json::integer(c.value)) : states->items()[i]);
      EXPECT_STOP(master_descriptor_from_node(with_params(node, params_replacing(node, "frame_decoder_states", a))),
                  error_category::invalid_input, "master.decoder_state_param");
    }
  }
  // Controllo positivo: lo stesso valore entro il buffer (raw_width 10 ≥ 2 + 8) dà il descrittore.
  json ok = json::array();
  ok.push(replacing(states->items()[0], "raw_width", json::integer(10)));
  ok.push(states->items()[1]);
  ok.push(states->items()[2]);
  EXPECT_NO_THROW((void)master_descriptor_from_node(with_params(node, params_replacing(node, "frame_decoder_states", ok))));
}

// ================================================================ T-M3A-CAL-07 e T-M3B-C02-08: RAW con FITS

// ADR-16.b §b.7 «Job del master», C-02.b §b.2: frame RAW e frame FITS nello stesso master, prima del calcolo.
TEST(MasterB15a, T_M3B_C02_08_and_T_M3A_CAL_07_raw_and_fits_frames_together_stop_with_decoder_state_mixed) {
  for (const bool reference_is_raw : {true, false}) {
    SCOPED_TRACE(reference_is_raw ? "riferimento RAW, un frame FITS" : "riferimento FITS, gli altri RAW");
    master_recipe r = raw_recipe(master_role::dark, 3);
    if (reference_is_raw) {
      r.frames[other_index(r)].decoder.reset();
    } else {
      r.frames[reference_index(r)].decoder.reset();
    }
    expect_recipe_stop(r, error_category::incompatible, "master.decoder_state_mixed");
  }
}

// ================================================================ T-M3B-C02-08 (RAW, preflight del nodo)

// C-02.b §b.2 «Selezione ed esecuzione»: campi strutturali di ADR-16.b §b.7 per uguaglianza col riferimento canonico.
// Il campo diverso è in un frame che non è il riferimento, oppure nel riferimento (tutti gli altri diversi da lui).
TEST(MasterB15a, T_M3B_C02_08_raw_structural_field_differs_from_reference_stops_with_master_mismatch_field) {
  struct structural {
    const char* field;
    std::function<void(decoder_state&)> apply;
    bool curve_base;                  // tutti i frame con linearizzazione a curva
    std::string base_text;     // forma canonica del campo nello stato base
    std::string changed_text;  // forma canonica del campo nello stato modificato
  };
  const std::vector<structural> cases = {
      {"raw_width", [](decoder_state& s) { s.raw_width += 2; }, false, "12", "14"},
      {"raw_height", [](decoder_state& s) { s.raw_height += 2; }, false, "10", "12"},
      {"window", [](decoder_state& s) { s.window.x0 = 0; }, false, R"({"h":6,"w":8,"x0":2,"y0":2})",
       R"({"h":6,"w":8,"x0":0,"y0":2})"},
      {"cfa_color_index", [](decoder_state& s) { s.cfa_color_index = {0, 3, 1, 2}; }, false, "[0,1,3,2]", "[0,3,1,2]"},
      {"black_pattern_rows",
       [](decoder_state& s) {
         s.black_pattern_rows = 2;
         s.black_pattern = {0, 0};
       },
       false, "1", "2"},
      {"black_pattern_cols",
       [](decoder_state& s) {
         s.black_pattern_cols = 2;
         s.black_pattern = {0, 0};
       },
       false, "1", "2"},
      {"linearization",
       [](decoder_state& s) {
         s.linearization = decoder_linearization::decoder_curve;
         s.curve_sha256 = curve(0xA1);
       },
       false, R"("identity")", R"("decoder_curve")"},
      {"curve_sha256", [](decoder_state& s) { s.curve_sha256 = curve(0xB2); }, true,
       R"("a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1a1")",
       R"("b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2")"},
  };
  for (const structural& c : cases) {
    for (const bool on_reference : {false, true}) {
      SCOPED_TRACE(std::string(c.field) + (on_reference ? ", nel riferimento" : ", in un frame non di riferimento"));
      master_recipe r = raw_recipe(master_role::dark, 3);
      if (c.curve_base) {
        for (master_frame_spec& f : r.frames) {
          f.decoder->state.linearization = decoder_linearization::decoder_curve;
          f.decoder->state.curve_sha256 = curve(0xA1);
        }
        ASSERT_NO_THROW((void)make_master_node(r)) << "base a curva valida";
      }
      c.apply(r.frames[on_reference ? reference_index(r) : other_index(r)].decoder->state);
      const std::optional<stop_error> e = recipe_stop(r, error_category::incompatible, std::string("master.mismatch.") + c.field);
      if (!e) continue;
      const std::optional<std::string> left = context_of(*e, "sinistra"), right = context_of(*e, "destra");
      EXPECT_TRUE(left.has_value()) << e->what();
      EXPECT_TRUE(right.has_value()) << e->what();
      EXPECT_EQ(context_of(*e, "tolleranza"), std::optional<std::string>("esatta")) << e->what();
      // Il frame modificato è il riferimento oppure l'altro: `sinistra` è sempre il riferimento canonico.
      EXPECT_EQ(left, std::optional<std::string>(on_reference ? c.changed_text : c.base_text)) << e->what();
      EXPECT_EQ(right, std::optional<std::string>(on_reference ? c.base_text : c.changed_text)) << e->what();
    }
  }
  // Controllo positivo: stati uguali passano.
  EXPECT_NO_THROW((void)make_master_node(raw_recipe(master_role::dark, 3)));
}

// C-02.b §b.2, ADR-16.b §b.7: nero effettivo `black + black_per_color[FC(r, c)] + P(r, c)` entro
// `decoder_black_abs_tol_adu` col riferimento canonico. Differenza massima = tolleranza: ammesso; tolleranza + 1:
// `master.mismatch.decoder_black`. Differenza su tutti i siti (nero scalare), su un solo colore, su una sola cella del
// motivo, in entrambi i versi; e componenti diverse con nero effettivo uguale (si confronta la somma, non i campi).
TEST(MasterB15a, T_M3B_C02_08_raw_effective_black_at_tolerance_passes_and_tolerance_plus_one_stops) {
  // Periodo del nero: lcm(2, 1) × lcm(2, 1) = 2 × 2 con il motivo 1 × 1, lcm(2, 2) × lcm(2, 2) = 2 × 2 con il motivo 2 × 2,
  // entro la finestra 8 × 6. Sito (riga, colonna) → indice di colore {0, 1, 3, 2}: il blu (indice 2) è in (1, 1); la cella
  // 3 del motivo 2 × 2 è (1, 1). Il nero scalare cambia ogni cella: posizione (0, 0), prima in ordine di riga.
  struct black_case {
    const char* name;
    std::function<void(decoder_state&, std::uint32_t)> apply;  // aggiunge `delta` al nero effettivo di almeno un sito
    bool pattern_base;
    std::uint32_t row, col;  // cella del periodo con la differenza massima
  };
  const std::vector<black_case> cases = {
      {"nero scalare, più", [](decoder_state& s, std::uint32_t d) { s.black += d; }, false, 0, 0},
      {"nero scalare, meno", [](decoder_state& s, std::uint32_t d) { s.black -= d; }, false, 0, 0},
      {"solo il blu (indice 2)", [](decoder_state& s, std::uint32_t d) { s.black_per_color[2] += d; }, false, 1, 1},
      {"solo una cella del motivo 2 × 2", [](decoder_state& s, std::uint32_t d) { s.black_pattern[3] += d; }, true, 1, 1},
  };
  for (const black_case& c : cases) {
    for (const bool on_reference : {false, true}) {
      for (const std::uint32_t delta : {kTol, kTol + 1}) {
        SCOPED_TRACE(std::string(c.name) + (on_reference ? ", nel riferimento" : ", in un altro frame") +
                     ", differenza " + std::to_string(delta));
        master_recipe r = raw_recipe(master_role::dark, 3);
        if (c.pattern_base) {
          for (master_frame_spec& f : r.frames) {
            f.decoder->state.black_pattern_rows = 2;
            f.decoder->state.black_pattern_cols = 2;
            f.decoder->state.black_pattern = {10, 10, 10, 10};
          }
        }
        c.apply(r.frames[on_reference ? reference_index(r) : other_index(r)].decoder->state, delta);
        if (delta == kTol) {
          EXPECT_NO_THROW((void)validate_master_recipe(r));
          EXPECT_NO_THROW((void)make_master_node(r));
        } else {
          const std::optional<stop_error> e = recipe_stop(r, error_category::incompatible, "master.mismatch.decoder_black");
          if (!e) continue;
          EXPECT_EQ(context_of(*e, "differenza_massima_adu"), std::optional<std::string>(std::to_string(kTol + 1))) << e->what();
          EXPECT_EQ(context_of(*e, "riga_periodo"), std::optional<std::string>(std::to_string(c.row))) << e->what();
          EXPECT_EQ(context_of(*e, "colonna_periodo"), std::optional<std::string>(std::to_string(c.col))) << e->what();
          EXPECT_EQ(context_of(*e, "tolleranza_adu"), std::optional<std::string>(std::to_string(kTol))) << e->what();
        }
      }
    }
  }
  {
    SCOPED_TRACE("tolleranza 0: uguale passa, 1 ADU si ferma");
    master_recipe r = raw_recipe(master_role::dark, 3);
    r.decoder_black_abs_tol_adu = 0;
    EXPECT_NO_THROW((void)make_master_node(r));
    r.frames[other_index(r)].decoder->state.black += 1;
    expect_recipe_stop(r, error_category::incompatible, "master.mismatch.decoder_black");
  }
  {
    SCOPED_TRACE("componenti diverse, nero effettivo uguale su ogni sito");
    master_recipe r = raw_recipe(master_role::dark, 3);
    r.decoder_black_abs_tol_adu = 0;
    decoder_state& s = r.frames[other_index(r)].decoder->state;
    s.black += 2;
    for (std::uint32_t& b : s.black_per_color) b -= 2;
    EXPECT_NO_THROW((void)make_master_node(r));
  }
}

// L9, C-02.b §b.2 «RAW», §b.10: `offset_adu` è dichiarato nella ricetta, unico per il nodo; serve solo al flat e a
// `single`. Assente con frame RAW nel flat o in `single`: metadata_missing `master.metadata.offset_adu` (riga più
// specifica di §b.10; `single` FITS resta `var.source.offset_adu`). Ammesso se e solo se il metodo è `single` o il ruolo
// è flat con frame RAW; altrove invalid_input `master.param.offset_adu` (decisione del coordinatore su B15a). Presente:
// il nodo porta il valore dichiarato, non il nero del decoder. Il controllo sui campioni (nero non sottratto) è di B15.
TEST(MasterB15a, T_M3B_C02_08_raw_offset_adu_absent_in_flat_or_single_stops_with_metadata_offset_adu) {
  {
    SCOPED_TRACE("flat RAW, mean, offset_adu assente");
    master_recipe r = raw_recipe(master_role::flat, 3);
    r.offset_adu.reset();
    expect_recipe_stop(r, error_category::metadata_missing, "master.metadata.offset_adu");
  }
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat, master_role::flat}) {
    SCOPED_TRACE("single RAW, offset_adu assente, ruolo " + std::to_string(static_cast<int>(role)));
    master_recipe r = raw_recipe(role, 1);
    r.combine = combine_method::single;
    r.rejection = master_rejection::none;
    r.offset_adu.reset();
    r.conversion_gain_e_per_adu = 1.5;
    r.read_noise_adu = 2;
    expect_recipe_stop(r, error_category::metadata_missing, "master.metadata.offset_adu");
  }
  for (const double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()}) {
    SCOPED_TRACE("flat RAW, mean, offset_adu non finito: " + std::to_string(bad));
    master_recipe r = raw_recipe(master_role::flat, 3);
    r.offset_adu = bad;
    expect_recipe_stop(r, error_category::invalid_input, "master.param.offset_adu");
  }
  {
    SCOPED_TRACE("flat RAW, mean, offset_adu dichiarato: nodo con il valore dichiarato");
    const master_recipe r = raw_recipe(master_role::flat, 3);
    ASSERT_EQ(r.offset_adu, std::optional<double>(7));
    std::optional<node_spec> node;
    EXPECT_NO_THROW(node.emplace(make_master_node(r)));
    const json* offset = node ? node->params().find("offset_adu") : nullptr;
    EXPECT_NE(offset, nullptr);
    if (offset != nullptr) {
      EXPECT_TRUE(offset->type() == json::kind::number || offset->type() == json::kind::integer);
      const double value =
          offset->type() == json::kind::integer ? static_cast<double>(offset->as_integer()) : offset->as_number();
      EXPECT_EQ(value, 7.0) << "offset_adu del nodo diverso dal valore dichiarato";
    }
  }
  {
    SCOPED_TRACE("dark RAW, mean, senza offset_adu: nessuno stop (serve solo al flat e a single)");
    EXPECT_NO_THROW((void)make_master_node(raw_recipe(master_role::dark, 3)));
  }
  for (const master_role role : {master_role::bias, master_role::dark, master_role::dark_flat}) {
    SCOPED_TRACE("RAW, mean, offset_adu presente fuori dal flat, ruolo " + std::to_string(static_cast<int>(role)));
    master_recipe r = raw_recipe(role, 3);
    r.offset_adu = 7;
    expect_recipe_stop(r, error_category::invalid_input, "master.param.offset_adu");
  }
  {
    SCOPED_TRACE("single FITS, offset_adu assente: var.source.offset_adu");
    master_recipe r = raw_recipe(master_role::dark, 1);
    for (master_frame_spec& f : r.frames) f.decoder.reset();
    r.decoder_black_abs_tol_adu.reset();
    r.combine = combine_method::single;
    r.rejection = master_rejection::none;
    r.conversion_gain_e_per_adu = 1.5;
    r.read_noise_adu = 2;
    expect_recipe_stop(r, error_category::metadata_missing, "var.source.offset_adu");
  }
}

// ADR-16.b §b.7 «Parametro» applicato al master (C-02.b §b.2, parametri per i frame RAW): tolleranza obbligatoria con
// frame RAW, vietata con soli FITS. Codice per analogia (vedi intestazione).
TEST(MasterB15a, T_M3B_C02_08_decoder_black_tolerance_required_with_raw_frames_and_forbidden_without) {
  {
    SCOPED_TRACE("RAW senza tolleranza");
    master_recipe r = raw_recipe(master_role::dark, 3);
    r.decoder_black_abs_tol_adu.reset();
    expect_recipe_stop(r, error_category::invalid_input, "master.param.decoder_black_abs_tol_adu");
  }
  {
    SCOPED_TRACE("FITS con tolleranza");
    master_recipe r = raw_recipe(master_role::dark, 3);
    for (master_frame_spec& f : r.frames) f.decoder.reset();
    expect_recipe_stop(r, error_category::invalid_input, "master.param.decoder_black_abs_tol_adu");
  }
}

}  // namespace
