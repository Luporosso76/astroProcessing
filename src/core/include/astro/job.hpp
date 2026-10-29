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
};

// Confine del job: avvia il job sullo slot (se non parte, l'errore si propaga: nessun job esiste), esegue `work`,
// registra e pubblica l'artefatto. Uno stop_error marca il job `failed` (`cancelled` per la categoria cancelled),
// scrive la diagnosi su stderr e restituisce 2 (3 per cancelled). Ogni altra eccezione marca il job `failed` e si
// rilancia. Nessun percorso alternativo. `budget`, se presente, entra nella provenance (riuscito o fermato).
// Con `cancel` richiesto quando `work` ritorna, il job e' cancelled e nulla si pubblica.
// Le prenotazioni del pool appartengono a `work`: si rilasciano per RAII anche quando lancia.
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
// transazione (commit_job) con tutti i file; la provenance riporta i digest di tutti gli output.
job_outcome run_job_files(sqlite_dag_store& store, const std::string& slot_name,
                          const std::function<job_files_result(const job_record&)>& work,
                          const std::optional<budget_snapshot>& budget = std::nullopt,
                          const cancel_token* cancel = nullptr);

}  // namespace astro
