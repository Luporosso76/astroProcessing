#pragma once

#include <optional>
#include <string>
#include <vector>

#include "astro/cancel.hpp"
#include "astro/error.hpp"
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

}  // namespace astro
