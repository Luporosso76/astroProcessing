#pragma once

#include <optional>
#include <string>
#include <vector>

#include "astro/budget.hpp"
#include "astro/dag_store.hpp"
#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "astro/json.hpp"
#include "astro/node.hpp"

namespace astro {

struct library_versions {
  std::string product;
  std::string source_revision;
  std::optional<bool> source_dirty;
  std::string cfitsio;
  std::string sqlite;
  std::string openssl;
};

library_versions current_library_versions();

// Provenance di un job terminato. `completed`: nessun arresto e output dichiarati; `failed`/`cancelled`: arresto
// presente e nessun output (altrimenti invalid_input dag.bad_provenance).
struct provenance_record {
  node_spec spec;
  library_versions versions;
  std::uint64_t generation = 0;
  job_state state = job_state::completed;
  std::vector<std::string> warnings;
  std::vector<content_digest> output_digests;
  std::optional<stop_error> stop;
  std::optional<budget_snapshot> budget = std::nullopt;  // fotografato nel preflight e fisso per tutto il job
  // Osservazioni del job (file letti, esito della cache): serializzate come `details` solo se presenti (ADR-05, R1).
  std::optional<json> details = std::nullopt;
  // Solo per un job eseguito da resume_workflow (ADR-05, estensione R4); assente, il campo non compare nel JSON.
  std::optional<json> resume = std::nullopt;
};

json provenance_json(const provenance_record& record);
std::string provenance_canonical(const provenance_record& record);

// Provenance di un job fermato al confine prima del lavoro (NFR-FBK-01.a): il nodo non e' letto, quindi il record ha solo
// stringhe e interi (chiave del nodo, versioni, generazione, stato, arresto, resume) e nessun numero in virgola mobile da
// analizzare o serializzare. Non porta parametri, node_spec, digest degli input, avvisi, budget ne' details; il JSON ha
// `stop_before_work: true`. `state` failed o cancelled coerente con la categoria dell'arresto (altrimenti invalid_input
// dag.bad_provenance).
struct stop_before_work_record {
  std::string node_key;
  library_versions versions;
  std::uint64_t generation = 0;
  job_state state = job_state::failed;
  stop_error stop;
  // Contesto di resume_workflow (stringhe e interi, ADR-05 estensione R4); assente, il campo non compare.
  std::optional<json> resume = std::nullopt;
};

json stop_before_work_json(const stop_before_work_record& record);
std::string stop_before_work_canonical(const stop_before_work_record& record);

}  // namespace astro
