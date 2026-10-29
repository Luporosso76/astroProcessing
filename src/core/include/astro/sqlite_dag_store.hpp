#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "astro/dag_store.hpp"
#include "astro/hash.hpp"
#include "astro/node.hpp"

struct sqlite3;

namespace astro {

constexpr std::uint32_t project_schema_version = 1;
constexpr std::uint32_t project_application_id = 0x41535450;
constexpr std::uint64_t export_manifest_max_bytes = std::uint64_t{64} << 20;
constexpr const char* export_format = "astro.export.v1";

enum class open_intent { read_write, read_only };
enum class open_mode { read_write, read_only_requested, read_only_unknown_schema };

// Migrazione da `from_version` a `from_version + 1`, eseguita come istruzioni SQL in un'unica transazione.
struct schema_migration {
  std::uint32_t from_version = 0;
  std::vector<std::string> statements;
};

// Punto di iniezione dei guasti: chiamato prima di ogni passo del protocollo con il nome del punto; un valore diverso
// da zero e' l'errno che il passo produce (io), senza eseguire la chiamata di sistema.
using fault_injector = std::function<int(std::string_view point)>;

struct open_options {
  std::uint32_t schema_version = project_schema_version;
  std::vector<schema_migration> migrations;
  // Attivo gia' durante l'apertura (migrazione); resta installato nello store.
  fault_injector fault;
};

struct recovery_report {
  std::vector<std::uint64_t> interrupted_jobs;
  std::vector<std::string> removed_temporaries;
  std::vector<std::string> removed_orphans;
  std::vector<std::uint64_t> artifacts_marked_absent;
};

struct input_record {
  std::uint64_t id = 0;
  std::string path;
  std::uint64_t size_bytes = 0;
  std::string sha256;
  std::string role;
};

// Contenuto di un file derivato da scrivere con il protocollo atomico.
struct artifact_content {
  std::span<const std::uint8_t> bytes;
  std::string extension;  // [a-z0-9]{1,16}
  std::string kind;
  std::optional<content_digest> expected;
  std::string role = "primary";  // [a-z0-9_]{1,32}, distinto fra gli output dello stesso job
};

class sqlite_dag_store;

// File scritto dal protocollo atomico (passi 1-5). Costruibile solo da sqlite_dag_store::write_files.
class stored_file {
 public:
  const std::string& rel_path() const noexcept { return rel_path_; }  // <sha256>.<estensione>, sotto artifacts/
  const content_digest& digest() const noexcept { return digest_; }
  const std::string& kind() const noexcept { return kind_; }
  const std::string& role() const noexcept { return role_; }

  // Identita' del file scritto e dello store che lo ha scritto: il commit rifiuta token altrui o file sostituiti.
  struct identity {
    std::uint64_t dev = 0, ino = 0, size = 0;
    std::int64_t mtime_ns = 0;
    std::int64_t ctime_ns = 0;
    bool operator==(const identity&) const = default;
  };

 private:
  friend class sqlite_dag_store;
  stored_file(std::string rel_path, content_digest digest, std::string kind, std::string role, std::string store_id,
              identity id, std::uint64_t token)
      : rel_path_(std::move(rel_path)), digest_(std::move(digest)), kind_(std::move(kind)), role_(std::move(role)),
        store_id_(std::move(store_id)), id_(id), token_(token) {}
  std::string rel_path_;
  content_digest digest_;
  std::string kind_;
  std::string role_;
  std::string store_id_;
  identity id_;
  std::uint64_t token_ = 0;
};

struct snapshot_entry {
  std::string slot_name;
  std::string node_key;
  std::string publish_role;
};

struct history_state {
  std::optional<std::uint64_t> current_snapshot_id;
  std::optional<std::uint64_t> redo_snapshot_id;
};

struct export_file_entry {
  std::string rel_path;
  std::uint64_t size_bytes = 0;
  std::string sha256;
  std::string slot_name;
  std::uint64_t generation = 0;
  std::string node_key;
};

struct export_manifest {
  std::vector<export_file_entry> files;
  std::vector<node_spec> nodes;  // chiave ricalcolata dai campi e confrontata con quella dichiarata
  std::string manifest_sha256;
};

// Verifica un export trattando manifest e contenuto come input non fidato. Non scrive nulla.
// Errori: limit_exceeded export.manifest_size; invalid_input export.manifest_malformed; security
// export.manifest_path; integrity export.incomplete / export.node_key_mismatch; io export.open.
// `hook` (solo test) e' chiamato con "verify.after_read" dopo la lettura e prima del controllo finale d'identita';
// un file sostituito o modificato nel frattempo: integrity export.changed_during_verify.
export_manifest verify_export(const std::filesystem::path& dir, const fault_injector& hook = {});

// DAG durevole su SQLite (ADR-04.a): stessa semantica di memory_dag_store, piu' artefatti su disco, snapshot e export.
// Ogni metodo che scrive su uno store aperto in sola lettura: schema store.read_only.
class sqlite_dag_store : public dag_store {
 public:
  // Directory nuova (esistente: security store.target_exists). Restituisce lo store aperto in lettura-scrittura.
  static std::unique_ptr<sqlite_dag_store> create(const std::filesystem::path& root);
  static std::unique_ptr<sqlite_dag_store> open(const std::filesystem::path& root, open_intent intent,
                                                const open_options& options = {});
  ~sqlite_dag_store() override;
  sqlite_dag_store(const sqlite_dag_store&) = delete;
  sqlite_dag_store& operator=(const sqlite_dag_store&) = delete;

  open_mode mode() const noexcept { return mode_; }
  const recovery_report& recovery() const noexcept { return recovery_; }
  const std::filesystem::path& root() const noexcept { return root_; }
  std::filesystem::path artifact_path(const std::string& rel_path) const;

  // Solo per i test di guasto: vale per tutte le operazioni successive.
  void set_fault_injector(fault_injector injector);

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
  // Il file `rel_path` deve esistere sotto artifacts/ con la dimensione dichiarata (io store.artifact_missing).
  artifact_record add_artifact(artifact_record artifact) override;
  void set_artifact_present(std::uint64_t artifact_id, bool present) override;
  std::vector<artifact_record> artifacts_for(const std::string& node_key) const override;
  void publish(std::uint64_t job_id, std::uint64_t artifact_id) override;
  // Un'unica transazione: riga dell'artefatto, pubblicazione e job `completed`. `artifact` deve descrivere un file
  // prodotto da write_files di QUESTO store (altrimenti invalid_input store.file_not_written_by_store).
  artifact_record complete_job(std::uint64_t job_id, artifact_record artifact) override;
  slot_view read_slot(const std::string& slot_name) const override;
  // Come memory_dag_store; se almeno uno slot cambia crea nella stessa transazione uno snapshot.
  std::vector<std::string> apply_recipe(const std::vector<resolved_node>& nodes,
                                        const std::map<std::string, std::string>& slot_targets) override;

  // Ruolo dell'output che lo slot pubblica (default `primary`). Mutazione della ricetta: ricollega lo slot all'artefatto
  // del nuovo ruolo (o missing), incrementa la generazione, crea uno snapshot (il ruolo vi e' registrato).
  void set_publish_role(const std::string& slot_name, const std::string& role);

  input_record add_input(input_record input);
  std::vector<input_record> inputs() const;

  // Passi 1-5 della scrittura atomica per ogni file; nessuna riga viene scritta.
  std::vector<stored_file> write_files(const std::vector<artifact_content>& files);
  // Passo 6: un'unica transazione inserisce le righe di artifacts (nodo del job) e chiude il job `completed`;
  // se la generazione dello slot e' quella del job, lo slot punta al file con il proprio ruolo di pubblicazione
  // (assente fra i file: invalid_input store.role_not_produced). Generazione cambiata: le righe
  // restano come cache, il job diventa `failed` e si lancia stale_generation dag.stale_publish.
  std::vector<artifact_record> commit_job(std::uint64_t job_id, const std::vector<stored_file>& files);

  history_state history() const;
  std::optional<std::uint64_t> snapshot_parent(std::uint64_t snapshot_id) const;
  std::vector<snapshot_entry> snapshot_entries(std::uint64_t snapshot_id) const;
  // Restituiscono i nomi degli slot del snapshot raggiunto. Errori: invalid_input history.no_undo / history.no_redo,
  // integrity history.dangling_node.
  std::vector<std::string> undo();
  std::vector<std::string> redo();

  // Esporta gli slot `current` indicati in una directory nuova (ADR-04.a, Export con manifest).
  void export_slots(const std::filesystem::path& dir, const std::vector<std::string>& slot_names);

 private:
  sqlite_dag_store(std::filesystem::path root, open_mode mode);

  // Inizializzazione e recovery (sqlite_project.cpp).
  void open_database(open_intent intent, const open_options& options);
  void migrate(std::uint32_t from_version, const open_options& options);
  void recover();
  bool wal_pending() const;
  // File scritto da questa istanza e ancora identico (dev/ino/size/mtime); altrimenti invalid_input o integrity.
  void check_identity(const stored_file& file) const;
  stored_file require_written_locked(const artifact_record& artifact) const;
  // `expected_node`: se non nullo deve coincidere con il nodo del job (invalid_input dag.artifact_node_mismatch).
  std::vector<artifact_record> commit_locked(std::uint64_t job_id, const std::vector<stored_file>& files,
                                             const std::string* expected_node);

  // Chiamare con mutex_ preso.
  void check_fault(std::string_view point) const;
  void require_writable() const;
  bool node_exists_locked(const std::string& node_key) const;
  void require_node_locked(const std::string& node_key) const;
  void insert_node_locked(const node_spec& node);
  void create_snapshot_locked();
  std::vector<std::string> move_history(bool undo_direction);
  slot_record require_slot_locked(const std::string& slot_name) const;

  std::filesystem::path root_;
  open_mode mode_;
  std::string store_id_;
  std::map<std::uint64_t, stored_file> written_;  // token -> file scritto da questa istanza, non ancora confermato
  std::uint64_t next_token_ = 1;
  int root_fd_ = -1;
  int lock_fd_ = -1;
  sqlite3* db_ = nullptr;
  recovery_report recovery_;
  mutable std::mutex mutex_;
  fault_injector fault_;
};

}  // namespace astro
