#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "astro/fault_injector.hpp"
#include "astro/job.hpp"
#include "astro/node.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

constexpr const char* workflow_format = "astro.workflow.v1";
// Record di arresto del workflow (docs/contracts/BASE-confini.md, OB-03): impostazione con chiave
// `workflow_stop.<sha256 della definizione canonica data>.<esecuzione>` (esecuzione: la prima libera da 1) e valore
// JSON canonico con format, workflow, definition_sha256, execution, operation (`run` o `resume`), category, code,
// message e context (coppie [chiave, valore]): solo stringhe e interi.
constexpr const char* workflow_stop_prefix = "workflow_stop.";
constexpr const char* workflow_stop_format = "astro.workflow_stop.v1";

// Passo di un workflow: esegue il job dello slot, il cui target deve essere `node` (ADR-04, estensione R4).
struct workflow_step {
  std::string slot_name;
  node_spec node;
  std::function<job_outcome()> run;
};

struct workflow {
  std::string name;  // [a-z0-9_-]{1,64}
  std::vector<workflow_step> steps;
  // Solo test: chiamato con "workflow.step_begin" prima e "workflow.step_end" dopo il job di ogni passo eseguito.
  fault_injector hook;
};

enum class step_action { reused, executed, not_reached };

struct workflow_step_result {
  std::string slot_name;
  step_action action = step_action::not_reached;
  std::optional<job_outcome> outcome;  // solo per i passi eseguiti
};

struct workflow_result {
  int exit_code = 0;  // 0, o il codice del primo passo fermato
  std::vector<workflow_step_result> steps;
};

// Registra la definizione (`settings`, chiave workflow.<nome>) ed esegue i passi in ordine; il primo passo fermato chiude
// il workflow. Errori prima di ogni passo: invalid_input workflow.definition / workflow.slot_target. Un passo che apre
// un job su un altro slot o nodo: invalid_input workflow.step_job.
// Un arresto prima del primo passo (anche un'eccezione non stop_error, registrata come integrity
// job.unexpected_exception) scrive il record di arresto del workflow e si propaga invariato; se il record non si scrive,
// lo stop_error si propaga col contesto `registrazione_arresto` e la diagnosi su stderr.
workflow_result run_workflow(sqlite_dag_store& store, const workflow& wf);

// Resume esplicito (D15, D16): definizione registrata e target degli slot identici a `wf`, poi riuso delle fasi sicure
// (digest riverificati) ed esecuzione degli altri passi. Nessun passo parte se: resume_mismatch resume.unknown_workflow /
// resume.recipe; integrity resume.artifact_digest; io resume.artifact_missing. Questi e gli altri arresti prima del
// primo passo lasciano il record di arresto del workflow, come in run_workflow.
workflow_result resume_workflow(sqlite_dag_store& store, const workflow& wf);

}  // namespace astro
