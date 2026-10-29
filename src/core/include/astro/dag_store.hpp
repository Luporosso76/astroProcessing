#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "astro/error.hpp"
#include "astro/node.hpp"

namespace astro {

enum class freshness { current, stale, missing };
// `interrupted`: job rimasto `running` alla chiusura del processo, marcato dalla recovery dello store durevole.
enum class job_state { running, completed, failed, cancelled, interrupted };

struct artifact_record {
  std::uint64_t id = 0;  // assegnato da add_artifact
  std::string node_key;
  std::uint64_t generation = 0;
  std::string sha256;
  std::uint64_t size_bytes = 0;
  std::string kind;
  bool present = true;
  // Nome del file sotto artifacts/ (<sha256>.<estensione>); ignorato dallo store in memoria, obbligatorio su SQLite.
  std::string rel_path;
  // Ruolo dell'output del job; lo slot pubblica l'artefatto del proprio ruolo (default `primary`).
  std::string output_role = "primary";
};

struct slot_record {
  std::string slot_name;
  std::uint64_t current_generation = 0;
  std::string target_node_key;
  std::optional<std::uint64_t> artifact_id;
  // Slot escluso dalla ricetta corrente (undo oltre la sua creazione): target vuoto, sempre `missing`.
  bool detached = false;
};

struct slot_view {
  std::optional<artifact_record> artifact;
  freshness state = freshness::missing;
};

struct job_record {
  std::uint64_t id = 0;
  std::string slot_name;
  std::string node_key;  // target dello slot all'avvio
  std::uint64_t generation = 0;
  job_state state = job_state::running;
  std::optional<error_category> stop_category;
  std::string stop_code;
};

// Interfaccia del DAG: ogni operazione e' una transazione. M2.c la implementa su SQLite.
class dag_store {
 public:
  virtual ~dag_store() = default;

  // Idempotente per la stessa chiave. Padre assente: graph dag.missing_parent.
  virtual void put_node(const node_spec& node) = 0;
  virtual std::optional<node_spec> get_node(const std::string& node_key) const = 0;
  // Discendenti transitivi, ordinati per chiave.
  virtual std::vector<std::string> descendants(const std::string& node_key) const = 0;

  // Nodo target assente: graph dag.missing_node. Slot esistente: invalid_input dag.duplicate_slot.
  virtual void create_slot(const std::string& slot_name, const std::string& target_node_key) = 0;
  virtual std::optional<slot_record> get_slot(const std::string& slot_name) const = 0;
  // Cambia il target e incrementa la generazione; restituisce la nuova generazione.
  virtual std::uint64_t retarget_slot(const std::string& slot_name, const std::string& target_node_key) = 0;

  // generation += 1; il job riceve quel valore.
  virtual job_record start_job(const std::string& slot_name) = 0;
  virtual void finish_job(std::uint64_t job_id, job_state state, std::optional<error_category> stop_category,
                          const std::string& stop_code) = 0;
  virtual std::optional<job_record> get_job(std::uint64_t job_id) const = 0;

  // L'artefatto resta sotto il suo node_key anche se non diventa mai corrente.
  virtual artifact_record add_artifact(artifact_record artifact) = 0;
  virtual void set_artifact_present(std::uint64_t artifact_id, bool present) = 0;
  virtual std::vector<artifact_record> artifacts_for(const std::string& node_key) const = 0;

  // Il job deve essere `running` (dag.job_not_running) e l'artefatto `present` (dag.artifact_not_present) e
  // del nodo del job (dag.artifact_node_mismatch). Se la generazione dello slot e' ancora quella del job lo slot
  // punta all'artefatto; altrimenti stale_generation dag.stale_publish, senza modificare lo slot.
  virtual void publish(std::uint64_t job_id, std::uint64_t artifact_id) = 0;
  // Registra l'artefatto, lo pubblica come `publish` e chiude il job `completed`. Gli store durevoli lo fanno in
  // un'unica transazione: dopo un crash non esiste un risultato corrente con job non completato.
  // Generazione cambiata: stale_generation dag.stale_publish; il job resta `running` (memoria) o diventa `failed`
  // con quel codice (durevole).
  virtual artifact_record complete_job(std::uint64_t job_id, artifact_record artifact) = 0;
  virtual slot_view read_slot(const std::string& slot_name) const = 0;

  // Operazione unica (transazione): inserisce i nodi risolti (padri prima dei figli) e allinea gli slot.
  // `slot_targets` associa il nome dello slot all'identificativo locale del nodo target. Cambia (e incrementa la
  // generazione di) solo gli slot il cui target differisce; restituisce i loro nomi, ordinati. Tutto viene
  // validato prima di modificare: padre o target assente (graph dag.missing_parent), nessuna modifica parziale.
  virtual std::vector<std::string> apply_recipe(const std::vector<resolved_node>& nodes,
                                                const std::map<std::string, std::string>& slot_targets) = 0;
};

class memory_dag_store : public dag_store {
 public:
  void put_node(const node_spec& node) override;
  std::optional<node_spec> get_node(const std::string& node_key) const override;
  std::vector<std::string> descendants(const std::string& node_key) const override;
  void create_slot(const std::string& slot_name, const std::string& target_node_key) override;
  std::optional<slot_record> get_slot(const std::string& slot_name) const override;
  std::uint64_t retarget_slot(const std::string& slot_name, const std::string& target_node_key) override;
  job_record start_job(const std::string& slot_name) override;
  void finish_job(std::uint64_t job_id, job_state state, std::optional<error_category> stop_category,
                  const std::string& stop_code) override;
  std::optional<job_record> get_job(std::uint64_t job_id) const override;
  artifact_record add_artifact(artifact_record artifact) override;
  void set_artifact_present(std::uint64_t artifact_id, bool present) override;
  std::vector<artifact_record> artifacts_for(const std::string& node_key) const override;
  void publish(std::uint64_t job_id, std::uint64_t artifact_id) override;
  artifact_record complete_job(std::uint64_t job_id, artifact_record artifact) override;
  slot_view read_slot(const std::string& slot_name) const override;
  std::vector<std::string> apply_recipe(const std::vector<resolved_node>& nodes,
                                        const std::map<std::string, std::string>& slot_targets) override;

 private:
  void check_parents(const node_spec& node, const std::set<std::string>& pending) const;
  void insert_node(const node_spec& node);
  slot_record& slot_ref(const std::string& slot_name);
  const slot_record& slot_ref(const std::string& slot_name) const;
  void require_node(const std::string& node_key) const;

  mutable std::mutex mutex_;
  std::map<std::string, node_spec> nodes_;
  std::map<std::string, std::set<std::string>> children_;
  std::map<std::string, slot_record> slots_;
  std::map<std::uint64_t, job_record> jobs_;
  std::map<std::uint64_t, artifact_record> artifacts_;
  std::uint64_t next_job_id_ = 1;
  std::uint64_t next_artifact_id_ = 1;
};

}  // namespace astro
