#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "astro/batch_classification.hpp"
#include "astro/calibration.hpp"
#include "astro/cancel.hpp"
#include "astro/cpu_dispatch.hpp"
#include "astro/fault_injector.hpp"
#include "astro/hash.hpp"
#include "astro/json.hpp"
#include "astro/master.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

// Analisi dei master (C-02.b §b.2): esclusioni per frame sul set candidato di un ruolo.

// Parametri d'analisi (L4). Le quattro tolleranze comuni stanno al livello superiore di `batch.analysis_params`, gli altri
// nel membro `master`. Un parametro assente resta assente: nessun default.
struct master_analysis_params {
  std::optional<double> exposure_rel_tol, temperature_abs_tol_c;
  std::optional<temperature_policy> temp_policy;
  std::optional<std::uint32_t> decoder_black_abs_tol_adu;
  std::optional<double> saturation_adu, saturated_fraction_max, flat_min_signal_adu, flat_level_rel_tol, offset_adu;
};

// `settings` `batch.analysis_params`, con lo stesso parser dei light. Documento assente: tutti i parametri assenti.
// Chiave sconosciuta (anche dentro `master`), tolleranza comune dentro `master`, `master` non oggetto o valore di tipo
// sbagliato: invalid_input batch.analysis_params.
master_analysis_params read_master_analysis_params(const sqlite_dag_store& store);

// Esito di decodifica fallita di un frame: codice dello stop di decodifica e suo dettaglio.
struct master_decode_failure {
  std::string code;
  std::string detail;
};

// Statistiche di un frame (L2), ingresso puro dell'analisi.
struct master_frame_stats {
  std::optional<master_decode_failure> decode_failure;  // se presente, gli altri campi non contano
  std::uint64_t saturated_samples = 0;  // campioni validi >= saturation_adu
  std::uint64_t pixel_count = 0;        // width * height
  // m_(f,c) per canale: 1 per un mono, 4 per un CFA nell'ordine dei siti (0,0) (0,1) (1,0) (1,1) del motivo. nullopt per un
  // canale senza pixel validi. Vuoto se non richiesti.
  std::vector<std::optional<double>> levels;
};

struct master_stats_request {
  std::optional<double> saturation_adu;  // assente: campioni saturi non contati
  bool flat_levels = false;
  // Offset sottratto ai livelli, obbligatorio con `flat_levels`: per un FITS quello dell'osservazione (header o
  // dichiarato), per un RAW `offset_adu` dei parametri d'analisi.
  std::optional<double> offset_adu;
};

// Lettore FITS verificato (OB-11): stesso percorso di apertura, piano, lettura verificata e decodifica del controllo termico.
// Un errore di piano o decodifica FITS con codice `fits.*` o `image.*` (compreso `fits.checksum`) diventa `decode_failure`,
// dopo la verifica del digest. Integrità OB-11 (`input.*`), I/O (`input.missing`), budget (`sched.*`) e annullamento si
// propagano come stop.
master_frame_stats read_master_frame_stats(const std::string& path, const content_digest& digest, const master_stats_request& request,
                                           memory_pool& pool, const cancel_token& cancel);

struct master_analysis_frame {
  std::uint64_t input_id = 0;
  content_digest digest;
  input_observation observation;
  // Letta solo se il frame arriva al motivo che la usa: `decode_failed` (se presente), `saturated_fraction` con
  // `saturation_adu` dichiarato e `flat_low_signal` per il flat (obbligatoria in questi due casi).
  std::optional<master_frame_stats> stats;
};

struct master_exclusion {
  std::uint64_t input_id = 0;
  content_digest digest;
  std::string reason;  // primo motivo della lista chiusa di C-02.b §b.2
  json values;
};

// `flat_level`: un membro di un gruppo flat viola `flat_level_rel_tol` in un canale. Non esclude frame.
enum class master_group_status { ok, flat_level };

// Gruppo di C-02.b §b.2 punto 2: un nodo per gruppo.
struct master_group {
  std::uint64_t reference = 0;          // input_id del riferimento, `members[0]`
  std::vector<std::uint64_t> members;   // input_id, ordine canonico
  master_group_status status = master_group_status::ok;
  // Solo per il flat: m_(f,c) per membro nell'ordine di `members`, canali nell'ordine dei siti. Vuoto per gli altri ruoli.
  std::vector<std::vector<double>> flat_levels;
};

struct master_screening {
  std::vector<std::string> missing_params;  // `params_missing`: nessuna esclusione, nessun frame utilizzabile
  std::vector<master_exclusion> excluded;   // ordine canonico (digest, poi input_id)
  std::vector<std::uint64_t> usable;        // input_id, ordine canonico
  std::vector<master_group> groups;         // disgiunti, coprono `usable`; ordine canonico del riferimento
};

// Funzione pura degli ingressi, indipendente dall'ordine dei frame. Parametri richiesti assenti: `missing_params`, senza
// stop. Soglia di saturazione di un frame con `saturation_adu` assente (prima di `params_missing`): invalid_input
// master.param.saturation_adu.
// Parametro fuori dominio (dopo `params_missing`): invalid_input master.param.<nome>. Nessun frame utilizzabile:
// invalid_input master.no_usable_frames. Altrimenti chiama `fault("master.analysis.groups")`
// una volta, dopo le esclusioni, e forma i gruppi sui frame utilizzabili.
// Gruppi: chiave esatta di C-02.b §b.2 punto 2, poi tolleranze col riferimento (frame restante a digest minimo) con le
// regole del preflight: esposizione |x − y| ≤ tol · max(|x|, |y|), temperatura entro `temperature_abs_tol_c` con
// `compare`, nero effettivo RAW per sito in interi a 64 bit entro `decoder_black_abs_tol_adu`. Flat: m̄_c media dei
// membri in ordine canonico, stato `flat_level` se |m_(f,c) / m̄_c − 1| > `flat_level_rel_tol`; m̄_c non finita:
// numeric master.report_nonfinite.
master_screening screen_master_frames(master_role role, const std::vector<master_analysis_frame>& frames,
                                      const master_analysis_params& params, const fault_injector& fault = {});

// ---------------------------------------------------------------- proposta a priori (C-02.b §b.4)

// Frame della richiesta: ingresso dell'analisi e specifica per il nodo (`spec.digest == analysis.digest`). Le dichiarazioni
// della specifica sono le stesse usate per l'osservazione: non si confrontano. Lo stato e l'identità del decoder di un RAW
// vengono dall'osservazione; un `spec.decoder` presente e diverso, di qualunque frame della richiesta, è invalid_input master.proposal.decoder_state.
struct master_proposal_frame {
  master_analysis_frame analysis;
  master_frame_spec spec;
};

struct master_proposal_request {
  master_role role = master_role::dark;
  std::vector<master_proposal_frame> frames;  // ordine qualunque
  master_analysis_params params;
  // Modello di rumore dichiarato (con `params.offset_adu`): condiziona la proposta `single` di un gruppo da un frame.
  std::optional<double> conversion_gain_e_per_adu, read_noise_adu;
  std::optional<simd_request> simd;  // ADR-07: nessun default; entra nella ricetta del nodo
};

struct master_proposed_method {
  combine_method combine = combine_method::mean;
  master_rejection rejection = master_rejection::none;
};

struct proposed_master {
  master_group group;
  std::optional<master_proposed_method> method;  // assente: nessuna proposta
  std::string absence;                           // "" con la proposta; "noise_model_not_declared" oppure "flat_level"
  std::vector<std::string> alternatives;         // {"median_ratio"} con "flat_level"
  std::optional<master_recipe> recipe;           // solo con la proposta
  std::optional<node_spec> node;                 // solo con la proposta: make_master_node(*recipe)
  std::string node_key;                          // vuota senza proposta
  json analysis;                                 // parametro `analysis` del nodo (L8); null senza proposta
  std::vector<content_digest> frames;            // membri, ordine canonico
  input_observation reference_observation;       // osservazione del frame di riferimento
};

struct master_proposal {
  master_role role = master_role::dark;
  master_screening screening;
  std::vector<proposed_master> groups;  // stesso ordine di `screening.groups`
  json document;                        // `astro.master_proposal.v1`
  std::string canonical() const { return canonical_json(document); }
};

// Funzione pura degli ingressi, indipendente dall'ordine dei frame e dagli `input_id`. Analisi di `screen_master_frames`
// (stessi stop); `params_missing`: nessun gruppo. Tabella di §b.4: n = 1 `single` solo con `conversion_gain_e_per_adu`,
// `read_noise_adu` e `params.offset_adu` dichiarati, altrimenti nessuna proposta (`noise_model_not_declared`); flat con
// stato `flat_level` senza proposta (`flat_level`) e `median_ratio` come alternativa; n = 2 `mean`; n = 3…1024 `median`
// (`mean` per il flat); n > 1024 `mean`; rejection sempre `none`. Il nodo di ogni proposta passa `validate_master_recipe`
// (stop di dominio: invalid_input master.param.<nome> / var.param.<nome>; il dominio di `conversion_gain_e_per_adu` e
// `read_noise_adu` si valida solo quando serve un gruppo `single`, e uno stop ferma l'intera proposta); i frame di un altro gruppo o esclusi non
// entrano mai fra i suoi ingressi. `output_type` è il default della ricetta.
master_proposal propose_masters(const master_proposal_request& request, const fault_injector& fault = {});

// Candidati (c) di F-IO-03 CL-06 per `analyze_batch`: un `slot_candidate` per gruppo con nodo proposto, senza `load_samples`.
std::vector<slot_candidate> proposed_slot_candidates(const master_proposal& proposal);

}  // namespace astro
