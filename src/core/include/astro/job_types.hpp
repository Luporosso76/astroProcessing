#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "astro/budget.hpp"
#include "astro/cancel.hpp"
#include "astro/hash.hpp"
#include "astro/job.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/resources.hpp"
#include "astro/sqlite_dag_store.hpp"

namespace astro {

// Registro dei tipi di job del prodotto (docs/contracts/BASE-confini.md §3.1): l'unico elenco dei tipi che passano da
// run_job_files. Ogni voce dichiara operatore, ruoli degli input, varianti fissate prima dell'esecuzione, passaggio
// dallo scheduler e costruttore del job minimo valido. Nessun comportamento: il lavoro resta nelle funzioni dei job.

// Input di un ruolo: percorso e identita' attesa del file.
struct job_type_input {
  std::string path;
  content_digest digest;
};

using job_type_inputs = std::map<std::string, job_type_input>;

// Ambiente di una esecuzione del job minimo. `resources` deve dichiarare `scratch` per le varianti con checkpoint;
// `checkpoint_name` e `resume` valgono solo per quelle varianti.
struct job_type_env {
  sqlite_dag_store& store;
  std::string slot_name;
  memory_pool& pool;
  const cancel_token& cancel;
  std::optional<budget_snapshot> budget;
  resource_config resources;
  std::string checkpoint_name;
  bool resume = false;
};

struct minimal_job {
  node_spec node;
  std::function<job_outcome(const job_type_env&)> run;
};

struct job_type_variant {
  std::string name;
  bool checkpoint = false;  // la variante scrive un checkpoint dello scheduler
  bool scratch = false;     // la variante passa al job lo scratch di `resources`; altrimenti lo toglie
};

struct job_type_entry {
  const char* operator_id;
  std::vector<std::string> input_roles;
  std::vector<job_type_variant> variants;
  bool scheduler = false;
  // Costruisce nodo e chiamata del job minimo dagli input per ruolo, per la variante indicata. `project_root` e' la
  // radice dello store su cui il job girera': un tipo con precondizioni nel progetto (voce di cache esistente per
  // cache.rebuild) le crea qui, prima di ogni esecuzione.
  std::function<minimal_job(const job_type_inputs& inputs, const std::string& variant,
                            const std::filesystem::path& project_root)>
      make;
};

const std::vector<job_type_entry>& registered_job_types();

}  // namespace astro
