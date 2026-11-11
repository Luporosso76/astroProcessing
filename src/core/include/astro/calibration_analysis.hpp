#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/error.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"

namespace astro {

// Catene del light (ADR-16.b §b.5), nomi testuali dei contratti: compaiono nel ledger e nelle chiavi.
enum class light_chain { none, bias, dark, bias_dark_sub, dark_sub_partial };
// Catene del flat (ADR-16.b §b.5), nell'ordine della tabella.
enum class flat_chain { none, dark_flat, bias_dark_flat_sub, bias, normalize, given_q };

// Stato dell'offset del light dopo la catena (C-01.b §1).
enum class offset_state { removed, known_not_removed, unknown };

// Nero applicato dal decoder (ADR-08.b §2.9): il decoder non sottrae mai il nero.
enum class black_applied_by_decoder { none };

const char* to_string(light_chain chain) noexcept;
const char* to_string(flat_chain chain) noexcept;
const char* to_string(offset_state state) noexcept;
const char* to_string(black_applied_by_decoder applied) noexcept;

// Nero effettivo `black + black_per_color[FC(r, c)] + P(r, c)` (H3, ADR-08.b §2.4) in interi a 64 bit esatti, riga per
// riga, su un periodo di `lcm(2, rows) × lcm(2, cols)` siti (dimensione nulla del motivo = 1) limitato all'area visibile.
struct decoder_black_period {
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
  std::vector<std::uint64_t> values;  // rows * cols, riga per riga
  friend bool operator==(const decoder_black_period&, const decoder_black_period&) = default;
};

// Usa gli indici di `cfa_color_index` dello stato così come sono. Stato incoerente (indice colore > 3, motivo con una
// sola dimensione nulla o con dimensioni diverse dai valori, area visibile vuota): invalid_input calib.decoder_state.
decoder_black_period decoder_black_period_of(const decoder_state& state);

// Confronto di ADR-16.b §b.7 fra due stati coi periodi del nero già calcolati: primo campo strutturale diverso
// nell'ordine del contratto (`raw_width`, `raw_height`, `window`, `cfa_color_index`, `black_pattern_rows`,
// `black_pattern_cols`, `linearization`, `curve_sha256`); poi, se `additive`, `decoder_black` se un sito del periodo
// differisce di più di `tol`. nullptr se compatibili.
const char* decoder_state_difference(const decoder_state& x, const decoder_black_period& bx, const decoder_state& y,
                                     const decoder_black_period& by, bool additive, std::uint64_t tol);

// Master che ha tolto l'offset del light.
struct offset_remover {
  master_role role = master_role::bias;
  content_digest digest;
};

// Ingressi della catena del light: esattamente quelli che la catena richiede (ADR-16.b §b.5, colonna «Ingressi»), più
// l'identità del decoder per un light con `decoder_state`.
struct light_offset_inputs {
  std::optional<content_digest> bias, dark, dark_prime;
  std::optional<decoder_identity> decoder;
};

// Voce `light_offset` (C-01.b §10): il nero non si sottrae mai dai campioni.
struct light_offset_entry {
  offset_state state = offset_state::unknown;
  std::optional<offset_remover> remover;  // assente se nessun master toglie l'offset
  // Solo per un light con `decoder_state`; assenti, non nulli, per ogni altro light.
  std::optional<decoder_black_period> black;
  std::optional<black_applied_by_decoder> black_applied;
  std::optional<decoder_identity> identity;
};

// Stato e voce `light_offset` di un light per una catena (C-01.b §1, §2). `OFFSET` FITS non cambia lo stato.
// Errori: unsupported calib.pedestal (`pedestal_adu` diverso da zero); invalid_input calib.chain_input (ingresso richiesto
// dalla catena assente), calib.chain_input_not_applicable (ingresso non richiesto, o identità del decoder su un light
// senza `decoder_state`), calib.decoder_identity (light con `decoder_state` senza identità), calib.decoder_state.
light_offset_entry make_light_offset_entry(light_chain chain, const image& light, const light_offset_inputs& inputs);

// ---------------------------------------------------------------- analisi G-CAL (ADR-16.b §b.1 punto 1, §b.2, §b.3, §b.7)

// Candidato di un ruolo: immagine letta (header, descrittore, stato del decoder), descrittore e identità del file.
struct analysis_candidate {
  image img;
  master_descriptor descriptor;
  content_digest digest;
  // Motivo di esclusione che vale per l'origine del candidato e non si ricava dal descrittore (per esempio un master
  // esterno prodotto da un RAW: il suo descrittore non porta `frame_decoder_states`). Applicato dopo lo stato, il dominio,
  // l'IMAGETYP e l'ingresso condiviso.
  std::optional<std::string> origin_reason;
  // Caricamento differito dei campioni: se `img` porta solo le intestazioni, il controllo termico di `flat.bias` chiama
  // questa funzione (dopo la sonda `calib.analysis.samples`) per ottenere l'immagine completa. Assente: `img` ha già i
  // campioni. Si chiama solo per il flat e il bias di una catena `flat.bias` effettivamente valutata.
  std::function<image()> load_samples = nullptr;
  // Master di un nodo proposto e non ancora eseguito (F-IO-03 CL-06 c): l'immagine è l'osservazione del frame di
  // riferimento, senza campioni del master. Il controllo termico di `flat.bias` che lo coinvolge non si calcola.
  bool pending_master = false;
};

// Dichiarazioni del progetto lette dall'analisi. Nessun default: un valore richiesto assente o fuori dominio ferma
// l'analisi; `q_min`, `q_rel_sigma_max` e le due termiche la fermano fuori dominio anche se non richieste.
// `normalization`, `q_min`, `q_rel_sigma_max` e le due dichiarazioni termiche non sono richieste dall'analisi: le usa il
// controllo termico di `flat.bias` (§b.5) e il default le copia nella ricetta.
struct calibration_declarations {
  std::optional<double> exposure_rel_tol;
  std::optional<double> temperature_abs_tol_c;
  std::optional<temperature_policy> temp_policy;
  std::optional<std::uint32_t> decoder_black_abs_tol_adu;
  std::optional<normalization_mode> normalization;
  std::optional<double> q_min;            // > 0
  std::optional<double> q_rel_sigma_max;  // > 0
  // Ipotesi dell'utente per `flat.bias`: limite superiore di `t_F d` per pixel (≥ 0) e budget relativo in (0, 1),
  // presenti insieme o assenti insieme.
  std::optional<double> flat_thermal_bound_adu;
  std::optional<double> flat_thermal_rel_budget;
};

struct calibration_analysis_input {
  image light;
  content_digest light_digest;
  std::optional<analysis_candidate> bias, dark, dark_flat, flat;
  calibration_declarations declarations;
};

enum class candidate_status { usable, excluded };

// Esito di un candidato in una catena di §b.5 (nome testuale del contratto); `reason` solo se `excluded`, dalla lista
// chiusa di §b.3.
struct chain_verdict {
  std::string chain;
  candidate_status status = candidate_status::excluded;
  std::optional<std::string> reason;
};

// `usable` se lo è in almeno una catena (§b.1). Le catene sono quelle di §b.5 che accettano il ruolo e lo stato
// dichiarato, in ordine di tabella; una coppia con un candidato assente non si valuta. Senza flat candidato il bias non
// ha catene del flat (§b.4); il dark-flat conserva la sua, valutata dal solo stato (§b.3).
struct candidate_analysis {
  master_role role = master_role::bias;
  candidate_status status = candidate_status::excluded;
  std::vector<chain_verdict> chains;
};

// Dichiarazioni che una combinazione di ruoli presenti richiede all'analisi (ADR-16.a §2, §b.7): è la regola con cui
// `analyze_calibration` fa i suoi stop `calib.param.<nome>`, esposta a chi deve sapere quali mancano prima di chiamarla.
// `flat_pairs`: coppie con un flat presente (F–DF, F–B).
struct declaration_needs {
  bool exposure = false;     // exposure_rel_tol
  bool temperature = false;  // temperature_policy e temperature_abs_tol_c
  bool decoder = false;      // decoder_black_abs_tol_adu
};
declaration_needs declaration_needs_of(bool dark, bool bias, bool dark_flat, bool flat, bool any_raw) noexcept;

// Coppie di §b.3 (stesse regole di `analyze_calibration`, un solo proprietario): riferimento e candidato.
enum class pair_kind { light_bias, light_dark, light_flat, flat_dark_flat, flat_bias };

// Un lato di una coppia: immagine e, se ha stato del decoder, gli stati dei suoi frame (un light: il proprio; un master
// RAW: quelli di `frame_decoder_states`).
struct pair_side {
  const image* img = nullptr;
  std::optional<std::vector<decoder_state>> states;
};

// Primo motivo di incompatibilità della coppia (campi di ADR-16.a §2 nell'ordine di `.a`, poi §b.7), nullopt se
// compatibile. Non valuta lo stato né il dominio del candidato (`candidate_own_reason`). Dichiarazioni richieste dalla
// coppia assenti o fuori dominio: invalid_input calib.param.<nome>. `warnings` riceve gli avvisi di ADR-16.a §2.
std::optional<std::string> pair_compatibility(pair_kind kind, const pair_side& reference, const pair_side& candidate,
                                              const calibration_declarations& declarations,
                                              std::vector<std::string>& warnings);

// Motivo di esclusione di un candidato per sé, nell'ordine di ADR-16.b §b.2, §b.3 e §b.7: stato, dominio, IMAGETYP,
// ingresso condiviso (`shared`), `origin_reason`, elenco dei frame vuoto, stato del decoder misto col light.
std::optional<std::string> candidate_own_reason(const analysis_candidate& candidate, bool shared, bool light_raw,
                                                std::vector<std::string>& warnings);

// Dominio del light (C-01.b §2, §7): lo stop che fermerebbe `analyze_calibration`, nullopt se il light è nel dominio.
std::optional<stop_error> light_domain_stop(const image& light, std::vector<std::string>& warnings);

// P-DF di §b.4: dark-flat assente, `usable` con `bias_included`, `usable` con `bias_subtracted`, presente ed `excluded`.
enum class dark_flat_predicate { absent, inc, sub, bad };

struct calibration_predicates {
  std::string dfb;  // "P-DFBxyz", esattamente uno degli otto
  dark_flat_predicate df = dark_flat_predicate::absent;
  std::optional<flat_state> flat_available;  // stato del flat se `usable`, assente altrimenti
};

// Ricetta selezionata: coppia di catene e tutti e soli i parametri che le catene usano (§b.5). Un parametro assente è
// assente, mai un valore implicito.
struct chain_recipe {
  light_chain light = light_chain::none;
  flat_chain flat = flat_chain::none;
  std::optional<double> exposure_rel_tol;              // catene con dark o dark-flat
  std::optional<double> temperature_abs_tol_c;         // catene con dark, bias o dark-flat
  std::optional<temperature_policy> temp_policy;       // catene con dark, bias o dark-flat
  std::optional<normalization_mode> normalization;     // catene del flat che calcolano N
  std::optional<double> q_min;                         // catene del flat che dividono
  std::optional<double> q_rel_sigma_max;               // catene del flat che dividono
  std::optional<double> flat_thermal_bound_adu;        // `flat.bias`
  std::optional<double> flat_thermal_rel_budget;       // `flat.bias`
  std::optional<std::uint32_t> decoder_black_abs_tol_adu;  // light con `decoder_state` (§b.7)
  // Percorso parziale (G-OFFSET): obbligatorio, e uguale a `known_not_removed`, con offset del light
  // `known_not_removed` e catena del flat che divide; vietato negli altri casi.
  std::optional<offset_state> flat_division_offset;
};

// P-FLAT-RAW, P-FLAT-Q e P-OFFSET di §b.4: predicati della ricetta, non dell'analisi.
bool p_flat_raw(const chain_recipe& recipe) noexcept;
bool p_flat_q(const chain_recipe& recipe) noexcept;
bool p_offset(const chain_recipe& recipe) noexcept;

// Coppia dell'insieme ammissibile; `partial` = divisione con `flat_division_offset = known_not_removed`.
struct admissible_pair {
  light_chain light = light_chain::none;
  flat_chain flat = flat_chain::none;
  bool partial = false;
};

struct calibration_analysis {
  std::vector<candidate_analysis> candidates;  // bias, dark, dark_flat, flat, solo i presenti
  // Avvisi di ADR-16.a §2: `imagetyp_unrecognized:<ingresso>:<valore>`, `temperature_comparison_skipped:<coppia>`.
  std::vector<std::string> warnings;
  calibration_predicates predicates;
  // Controllo termico di `flat.bias` calcolato e superato (§b.5); falso se non calcolato (dichiarazioni assenti, flat
  // non `raw`, flat escluso per sé o nella coppia L–F, bias escluso per sé o nella coppia F–B) o se esclude la catena.
  bool flat_thermal_within_budget = false;
  // Controllo termico di `flat.bias` valutabile ma non calcolato perché flat o bias è un master proposto non eseguito
  // (esito `thermal_check_pending_master`). Esclude `flat_thermal_within_budget`; la catena resta ammissibile.
  bool flat_thermal_pending_master = false;
  bool light_has_decoder_state = false;
  calibration_declarations declarations;     // dichiarazioni lette dall'analisi
  std::vector<admissible_pair> admissible;   // ordine di tabella: catena del light, poi del flat, poi `partial`
  chain_recipe default_recipe;               // D-16, §b.6: mai il percorso parziale
};

// Funzione pura degli ingressi. Un master fuori regola è `excluded` con motivo, mai uno stop. Stop, tutti prima di ogni
// calcolo (`fault` è chiamato con "calib.analysis.pair" prima di ogni coppia e con "calib.analysis.samples" prima
// di leggere i campioni del controllo termico; un valore diverso da zero dà io calib.analysis.fault):
// - light: metadata_missing calib.metadata.phase / calib.metadata.unit; incompatible op.phase_blocked /
//   op.linearity_blocked (riga `calibrate_light`, `role` nel contesto); unsupported calib.unit, calib.plane_layout,
//   calib.coverage_input, calib.pedestal; incompatible calib.imagetyp_conflict; invalid_input calib.decoder_state;
// - descrittori: invalid_input calib.role_slot, calib.param_not_applicable.flat_normalization_declared,
//   calib.decoder_state (stato di un frame incoerente);
// - dichiarazioni: invalid_input calib.param.<nome> (tolleranza richiesta da una coppia presente, assente o fuori
//   dominio; una dichiarazione presente fuori dominio; una sola delle due termiche), calib.param.decoder_black_abs_tol_adu
//   (assente con un ingresso con stato del decoder), calib.param_not_applicable.decoder_black_abs_tol_adu (presente
//   senza);
// - controllo termico di `flat.bias` (calcolato solo con le due dichiarazioni, flat `raw` senza motivo proprio né della
//   coppia L–F, bias senza motivo proprio né della coppia F–B): invalid_input calib.param.normalization /
//   calib.param.q_min (assenti), calib.normalization_requires_cfa; invalid_input image.* (campioni incoerenti).
calibration_analysis analyze_calibration(const calibration_analysis_input& input, const fault_injector& fault = {});

// Ricetta per una coppia ammissibile, con i parametri delle sue catene copiati dalle dichiarazioni dell'analisi e, per il
// percorso parziale, `flat_division_offset = known_not_removed`. Assente se una dichiarazione che la coppia usa manca (il
// default non inventa dichiarazioni, §b.9).
std::optional<chain_recipe> recipe_of_pair(const calibration_analysis& analysis, const admissible_pair& pair);

// Vero se la ricetta usa il master del ruolo in una delle sue catene (§b.5).
bool recipe_uses_role(const chain_recipe& recipe, master_role role) noexcept;

// Selezione (§b.1 punto 2, §b.9): ritorna se la ricetta appartiene all'insieme ammissibile dell'analisi e porta tutti e
// soli i parametri delle sue catene, uguali alle dichiarazioni dell'analisi quando questa le ha. Stop, nessun nodo:
// - master `unknown` selezionato: metadata_missing calib.master_state_unknown; dark o dark-flat `scaled`: unsupported
//   calib.master_state_scaled;
// - invalid_input calib.recipe_not_admissible, con `reason` nel contesto (motivo di §b.3 del master escluso, o di §b.10:
//   master_absent, double_offset, light_offset_not_removed, dark_flat_sub_requires_bias, flat_offset_not_removable,
//   flat_already_corrected, flat_thermal_not_declared, master_excluded per una catena incoerente con lo stato);
// - invalid_input calib.param_not_applicable.<nome> (parametro di una catena non selezionata, `flat_division_offset`
//   senza percorso parziale), calib.param.<nome> (assente, fuori dominio o diverso dalla dichiarazione dell'analisi;
//   `flat_division_offset` diverso da `known_not_removed`).
void require_admissible(const calibration_analysis& analysis, const chain_recipe& recipe);

// ---------------------------------------------------------------- selezione congelata (F-IO-03 CL-11)

// Stato e identità del decoder di un ingresso RAW osservati nell'analisi; `name` identifica l'ingresso nella selezione.
struct selected_decoder_input {
  std::string name;
  decoder_state state;
  decoder_identity identity;
};

// Candidato di un ruolo scelto per l'analisi: `input` (digest e dimensione) per un input registrato, `node_key` per il
// master di uno slot; esattamente uno dei due.
struct selected_candidate {
  master_role role = master_role::dark;
  std::optional<content_digest> input;
  std::optional<std::string> node_key;
  // Solo il candidato di un nodo proposto (CL-06 c): digest dei frame del nodo, in ordine canonico. La presenza lo marca.
  std::optional<std::vector<content_digest>> proposed_frames = std::nullopt;
};

// Selezione congelata (CL-11), tutti i campi: ricetta e parametri; light del gruppo (digest e dimensione, per digest) e
// candidato dei ruoli che la ricetta usa; dichiarazioni dell'analisi; stato e identità del decoder di ogni ingresso RAW
// osservato nell'analisi.
struct calibration_selection {
  chain_recipe recipe;
  std::vector<selected_decoder_input> raw_inputs;
  std::vector<content_digest> lights;
  std::vector<selected_candidate> candidates;  // nell'ordine dei ruoli bias, dark, dark_flat, flat
  calibration_declarations declarations;
};

// Funzione pura: per ogni ingresso RAW della selezione, nell'ordine, confronta lo stato (membri di contenuto nell'ordine di
// dichiarazione, `source` esclusa) e poi l'identità del decoder con l'osservazione dello stesso `name`. Prima differenza:
// integrity calib.decoder_state_changed con `field` (membro dello stato, `decoder_identity.<membro>`, oppure
// `observation` se l'ingresso non ha osservazione), `predicate` = P-LOST e `prerequisite` = `<name>.decoder_state`. Nessuna
// altra uscita: né ricetta, né candidato, né selezione modificata.
//
// Nodo proposto osservato dopo l'esecuzione del master: i frame che contiene ora, in ordine canonico.
struct observed_proposed_node {
  std::string node_key;
  std::vector<content_digest> frames;
};

// Poi, per ogni candidato con `proposed_frames`: il nodo osservato con la stessa `node_key` deve avere esattamente quei
// frame, nello stesso ordine; altrimenti integrity calib.proposed_node_changed con `node_key` e `field` (`frames`, oppure
// `observation` se il nodo non è osservato). Con ricetta `flat.bias` e candidato bias o flat proposto: integrity
// calib.thermal_check_pending (`node_key`, `role`), perché il controllo termico non è stato calcolato (nuova analisi sul master
// eseguito). Non esclude né sostituisce nulla.
void verify_selection(const calibration_selection& selection, const std::vector<selected_decoder_input>& observed,
                      const std::vector<observed_proposed_node>& nodes = {});

}  // namespace astro
