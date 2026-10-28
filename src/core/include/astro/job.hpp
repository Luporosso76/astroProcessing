#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "astro/dag_store.hpp"
#include "astro/error.hpp"
#include "astro/provenance.hpp"

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
// rilancia. Nessun percorso alternativo.
job_outcome run_job(dag_store& store, const std::string& slot_name,
                    const std::function<job_result(const job_record&)>& work);

}  // namespace astro
