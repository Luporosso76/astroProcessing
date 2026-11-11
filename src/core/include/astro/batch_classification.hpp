#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "astro/calibration.hpp"
#include "astro/calibration_analysis.hpp"
#include "astro/cancel.hpp"
#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "astro/json.hpp"
#include "astro/memory_pool.hpp"
#include "astro/scheduler.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

// Proposta di classificazione di un input (F-IO-03 CL-01): funzione pura del report di import o dello stop d'import.
// `reason` è vuoto se `role` non è `unclassified`; `evidence` non contiene il motivo (lo aggiunge la revisione).
struct classification_proposal {
  std::string role;
  std::string rule_id;
  std::string reason;
  json evidence;
};

// Regola `imagetyp.v1` sul report `astro.import_report.v1` di un FITS. Valore riconosciuto: il suo ruolo, evidenza
// {"imagetyp"}; non riconosciuto: `unclassified`, `imagetyp_unrecognized`; keyword assente: `imagetyp_absent`.
classification_proposal propose_classification(const json& fits_import_report);
// Regola `raw_no_type.v1`: un RAW non ha tipo di immagine, sempre `unclassified` (`raw_no_type_metadata`).
classification_proposal propose_classification_raw();
// Regola `import_stopped.v1`: evidenza {"category", "code"} dello stop.
classification_proposal propose_classification_stopped(error_category category, const std::string& code);

struct batch_import_result {
  input_record input;
  std::string slot_name;
  std::string job_type;  // io.import o io.raw_import
  branch_state state = branch_state::not_started;
  std::optional<stop_error> stop;
  classification_record classification;  // revisione 1 scritta
};

// I job si compongono con `run_branches`, un ramo per file e uno slot per file. I nodi non hanno archi fra loro, quindi i
// rami sono indipendenti.
// Registra ogni file (`role = batch`), esegue il suo job d'import (FITS se comincia con la card `SIMPLE  =`, altrimenti
// RAW: decide il contenuto, non l'estensione) e scrive le revisioni 1 delle regole in una sola chiamata di
// append_classifications. Un import fermato chiude solo il suo ramo. Percorsi non assoluti o file illeggibili: arresto
// prima di ogni scrittura. Annullamento: cancelled sched.cancelled, inputs registrati senza revisioni.
std::vector<batch_import_result> import_batch(sqlite_dag_store& store, const std::vector<std::string>& absolute_paths,
                                              memory_pool& pool, const cancel_token& cancel);

// ---------------------------------------------------------------- analisi del batch (F-IO-03 CL-05…CL-12)

// Osservazione di un input (CL-06, §6): immagine letta con le dichiarazioni applicate come ADR-03, più l'identità del
// decoder per un RAW. Un valore dichiarato diverso da quello letto non ferma la costruzione: l'osservazione lo registra
// (`declared_conflict`) e l'analisi esclude l'input. L'uguaglianza confronta i campi osservati (header, stato e identità
// del decoder, conflitto), non i campioni.
class input_observation {
 public:
  input_observation() = default;
  // Immagine osservata; per un'osservazione vuota (input non analizzato) ASTRO_ASSERT.
  const image& observed_image() const;
  bool empty() const noexcept { return !data_; }
  // Nome del campo dichiarato in conflitto con il valore letto.
  const std::optional<std::string>& declared_conflict() const;
  // Identità del decoder che ha prodotto lo stato del decoder; assente per un'immagine senza stato del decoder.
  const std::optional<decoder_identity>& identity() const;
  friend bool operator==(const input_observation& a, const input_observation& b) noexcept;

  struct data;  // opaco

 private:
  friend input_observation make_observation(const image&, const std::string&, const std::optional<decoder_identity>&);
  std::shared_ptr<const data> data_;
};

// Costruttore unico (CL-06): `declarations_canonical` è `astro.input_declarations.v1`. Formato errato: invalid_input
// classify.declaration. `identity` è l'identità del decoder di un'immagine con stato del decoder: nel preflight, l'immagine
// è decodificata da questo processo; nell'analisi dal report persistito. Immagine con stato del decoder senza identità:
// integrity batch.decoder_identity_missing.
input_observation make_observation(const image& img, const std::string& declarations_canonical,
                                   const std::optional<decoder_identity>& identity);
// Immagine decodificata da questo processo: identità di `current_decoder_identity()` se ha stato del decoder.
input_observation make_observation(const image& img, const std::string& declarations_canonical);

// Input registrato con la sua classificazione effettiva (CL-03): `role` vuoto = nessuna revisione.
struct batch_input {
  std::uint64_t input_id = 0;
  content_digest digest;
  std::string role;                      // light, bias, dark, dark_flat, flat, unclassified, oppure vuoto
  std::string declarations_canonical;    // revisione corrente: fa parte della classificazione effettiva (CL-05)
  std::string unclassified_reason;       // solo se role == "unclassified"
  std::string evidence_canonical;        // evidenza della revisione, riportata con l'esclusione
  std::optional<master_descriptor> descriptor;  // master esterno dichiarato (CL-06 a)
  input_observation observation;         // vuota per un input non analizzato
  // Immagine completa con campioni (dichiarazioni applicate), per il controllo termico di `flat.bias`: assente se
  // l'osservazione porta già i campioni o l'input non può entrarvi. Valida solo durante la chiamata di `analyze_batch`.
  std::function<image()> load_samples = nullptr;
};

// Slot `current` il cui nodo è `master.combine` (CL-06 b), con il descrittore derivato dal nodo.
struct slot_candidate {
  std::string node_key;
  master_role role = master_role::dark;
  master_descriptor descriptor;
  input_observation observation;
  std::function<image()> load_samples = nullptr;  // come `batch_input::load_samples`
  // Solo il candidato (c) di CL-06, nodo proposto non eseguito (`proposed_slot_candidates`): digest dei frame del nodo in
  // ordine canonico. La presenza lo marca; `load_samples` assente, osservazione del frame di riferimento.
  std::optional<std::vector<content_digest>> proposed_frames = std::nullopt;
};

// Ruolo -> chiave del candidato (digest esadecimale o chiave del nodo).
using candidate_choices = std::map<master_role, std::string>;

struct batch_analysis_request {
  calibration_declarations params;
  candidate_choices choices;
};

enum class group_outcome { ready, candidate_choice_required, light_blocked, params_missing };
enum class choice_kind { absent, excluded_only, only_compatible, required };

struct batch_member {
  content_digest digest;
  std::vector<std::uint64_t> input_ids;  // crescenti
};

struct candidate_report {
  master_role role = master_role::dark;
  std::string key;
  candidate_status status = candidate_status::excluded;
  std::string reason;  // vuoto se usable
};

struct role_choice {
  master_role role = master_role::dark;
  choice_kind kind = choice_kind::absent;
  std::vector<std::string> eligible;  // chiavi in ordine canonico
  std::optional<std::string> chosen;
};

// Voce `omitted` del ledger del default (ADR-16.b §b.10): `master_absent` o `master_excluded`.
struct role_omission {
  master_role role = master_role::dark;
  std::string reason;
};

struct group_proposal {
  std::string id;                       // digest minimo dei membri
  std::vector<batch_member> members;    // per digest
  group_outcome outcome = group_outcome::ready;
  error_category block_category = error_category::invalid_input;  // solo `light_blocked`
  std::string block_code;                                          // solo `light_blocked`
  std::vector<std::string> missing_params;                         // solo `params_missing`
  std::vector<candidate_report> candidates;                        // per ruolo, poi per chiave
  std::vector<role_choice> choices;                                // bias, dark, dark_flat, flat
  std::optional<calibration_analysis> analysis;                    // solo `ready`
  std::vector<chain_recipe> recipes;                               // `ready`: il default per primo, poi le coppie ammissibili
  std::vector<role_omission> omissions;
  std::vector<selected_decoder_input> raw_inputs;                  // ingressi RAW (i light del gruppo) osservati
  std::vector<selected_candidate> chosen;                          // `ready`: candidato scelto per ogni ruolo presente
};

struct excluded_input {
  content_digest digest;
  std::vector<std::uint64_t> input_ids;
  std::string reason;
  std::string evidence_canonical;
};

struct batch_proposal {
  std::vector<group_proposal> groups;     // in ordine di `id`
  std::vector<excluded_input> excluded;   // in ordine di digest
  // Frame di un ruolo di calibrazione senza descrittore (C-02.b §b.1): chiavi (digest) in ordine.
  std::map<master_role, std::vector<std::string>> frame_pending_master;
  // JSON canonico `astro.batch_proposal.v1`: gli input sono identificati dal digest, gli `input_id` non compaiono.
  std::string canonical() const;
};

// CL-05…CL-09. Funzione pura degli ingressi: nessuna scrittura, nessuna lettura di campioni salvo il controllo termico di
// `flat.bias` (dentro `analyze_calibration`, che riceve `fault`, tramite `load_samples` del solo flat e bias candidati di
// una catena `flat.bias` valutata). Scelta su un candidato inesistente per il ruolo:
// invalid_input classify.choice_unknown_candidate.
batch_proposal analyze_batch(const std::vector<batch_input>& inputs, const std::vector<slot_candidate>& slots,
                             const batch_analysis_request& request, const fault_injector& fault = {});

// Sugli ingressi persistiti: revisioni correnti, report di import, nodi RAW, slot dei master e dichiarazioni
// `batch.analysis_params` di `settings` (JSON `astro.batch_analysis_params.v1`). I campioni si leggono solo al controllo
// di `flat.bias`, e solo quelli del flat e del bias candidati di una catena `flat.bias` valutata.
batch_proposal analyze_batch(const sqlite_dag_store& store, const candidate_choices& choices, memory_pool& pool,
                             const cancel_token& cancel, const fault_injector& fault = {});

// Selezione di una ricetta per un gruppo (CL-11): gruppo con un ruolo ambiguo senza scelta: invalid_input
// classify.choice_required; gruppo `light_blocked`: lo stop del suo dominio; `params_missing`: invalid_input
// calib.param.<nome>; ricetta fuori dall'insieme ammissibile: lo stop di `require_admissible`. Congela tutti i campi di
// CL-11: ricetta, light del gruppo, candidato dei ruoli che la ricetta usa, dichiarazioni dell'analisi, stato e identità
// del decoder di ogni ingresso RAW osservato nell'analisi.
calibration_selection select_recipe(const group_proposal& group, const chain_recipe& recipe);

}  // namespace astro
