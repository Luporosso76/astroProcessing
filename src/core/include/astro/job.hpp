#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "astro/budget.hpp"
#include "astro/cancel.hpp"
#include "astro/dag_store.hpp"
#include "astro/error.hpp"
#include "astro/provenance.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

constexpr int exit_code_stop = 2;
constexpr int exit_code_cancelled = 3;

// 3 per `cancelled`, 2 per ogni altra categoria.
int exit_code_for(error_category category) noexcept;

struct job_result {
  artifact_record artifact;
  std::vector<std::string> warnings;
};

struct job_outcome {
  int exit_code = 0;
  job_record job;
  std::optional<stop_error> stop;
  // Provenance del job, riuscito o fermato, generata dal confine.
  provenance_record provenance;
  // Solo su store durevole: la provenance dell'arresto non e' stata persistita (causa, categoria e codice). La causa
  // primaria resta `stop`; il codice di uscita non cambia. Vuoto se la provenance e' persistita o il job e' riuscito.
  std::optional<stop_error> provenance_not_persisted;
  // Errore sopraggiunto dopo che lo store aveva gia' registrato la causa del job (es. guasto dopo un commit con
  // generazione superata): `stop` e la provenance riportano la causa registrata, questo l'errore successivo.
  std::optional<stop_error> subsequent_error;
};

// Confine del job: avvia il job sullo slot (se non parte, l'errore si propaga: nessun job esiste), esegue `work`,
// registra e pubblica l'artefatto. Uno stop_error marca il job `failed` (`cancelled` per la categoria cancelled),
// scrive la diagnosi su stderr e restituisce 2 (3 per cancelled). Ogni altra eccezione marca il job `failed` e si
// rilancia. Nessun percorso alternativo. `budget`, se presente, entra nella provenance (riuscito o fermato).
// Con `cancel` richiesto quando `work` ritorna, il job e' cancelled e nulla si pubblica.
// Le prenotazioni del pool appartengono a `work`: si rilasciano per RAII anche quando lancia.
// Con uno sqlite_dag_store si ferma prima di avviare il job (invalid_input job.durable_store_requires_run_job_files):
// la provenance durevole la scrive solo run_job_files.
job_outcome run_job(dag_store& store, const std::string& slot_name,
                    const std::function<job_result(const job_record&)>& work,
                    const std::optional<budget_snapshot>& budget = std::nullopt,
                    const cancel_token* cancel = nullptr);

// Output multipli (ruoli distinti) gia' scritti con write_files.
struct job_files_result {
  std::vector<stored_file> files;
  std::vector<std::string> warnings;
};

// Come run_job per uno store durevole: `work` scrive i file con write_files e la pubblicazione e' un'unica
// transazione (commit_job) con tutti i file piu' la provenance, scritta col protocollo atomico come file di ruolo
// `provenance` (riservato) e con i digest di tutti gli output. Per un job fermato o annullato la provenance e' scritta
// come artefatto `provenance` del nodo dopo la chiusura del job; se non si scrive, l'esito dichiara la causa in
// `provenance_not_persisted` (oltre a stderr) e il codice di uscita non cambia. Se commit_job lancia dopo aver chiuso il
// job `completed`, l'errore si propaga: il record durevole e' gia' coerente. Generazione superata: nessun record
// `completed`, un solo record `failed` dag.stale_publish. La provenance rispecchia sempre stato e causa gia' registrati
// nel job (riletti dallo store); un errore successivo va in `subsequent_error`. Eccezione non stop_error: job `failed`
// `job.unexpected_exception` (categoria integrity), provenance persistita, eccezione originale rilanciata invariata.
job_outcome run_job_files(sqlite_dag_store& store, const std::string& slot_name,
                          const std::function<job_files_result(const job_record&)>& work,
                          const std::optional<budget_snapshot>& budget = std::nullopt,
                          const cancel_token* cancel = nullptr);

}  // namespace astro
