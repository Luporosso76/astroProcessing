#pragma once

#include <sched.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "astro/budget.hpp"
#include "astro/cancel.hpp"
#include "astro/checkpoint.hpp"
#include "astro/fault_injector.hpp"
#include "astro/job.hpp"
#include "astro/job_plan.hpp"
#include "astro/memory_pool.hpp"
#include "astro/node.hpp"
#include "astro/spill.hpp"

namespace astro {

// Capacita' fissa della coda FIFO dei job in attesa del pool (ADR-06).
constexpr std::size_t job_queue_capacity = 16;

// Come sched_getaffinity(0, ...): 0 se riuscita, altrimenti l'errno.
using affinity_fn = std::function<int(cpu_set_t*)>;
int system_affinity(cpu_set_t* set);

// CPU in sched_getaffinity. Chiamata fallita o zero CPU: io sched.cpu_count. hardware_concurrency non si usa.
std::size_t cpu_count(const affinity_fn& affinity = system_affinity);

// Ammissione sotto pressione (ADR-06, estensione R3.2; D11, D12): campionata a ogni ammissione di un job, prima della
// prenotazione del pool. Con PSI >= threshold l'ammissione attende senza timeout e ricampiona ogni `poll`.
struct pressure_gate {
  // PSI `some avg10`; nullopt = sorgente assente (nessuna pressione). Errore della sorgente: si propaga.
  std::function<std::optional<double>()> sample;
  double threshold = budget_psi_threshold;
  std::chrono::milliseconds poll{1000};
  // Job in attesa di ammissione per pressione (aggiornato dallo scheduler; letto dal monitor).
  std::atomic<std::size_t> waiting_jobs{0};
};

// Lettura della pressione fatta durante l'ammissione di un job.
struct admission_sample {
  std::optional<double> psi;
  bool admitted = false;
  friend bool operator==(const admission_sample&, const admission_sample&) = default;
};

// Parametri configurabili dello scheduler (resource_config). `cpu_set` vuoto = nessuna affinita' applicata.
// `pressure` nullo = ammissione senza campionamento della pressione.
struct scheduler_tuning {
  std::size_t max_queued_jobs = job_queue_capacity;
  std::vector<unsigned> cpu_set;
  std::shared_ptr<pressure_gate> pressure;
};

struct job_context {
  const job_plan& plan;
  reservation& shared;  // shared_bytes del piano
  spill_area* spill;    // nullptr senza spill dichiarato
  const cancel_token& cancel;
  const cancel_token& stopping;  // impostato al primo errore di un worker: i task attivi devono fermarsi
};

struct task_context {
  job_context& job;
  std::uint64_t index;
  reservation& memory;  // per_task_bytes, presa all'avvio del task

  // cancelled sched.cancelled se l'annullamento e' richiesto o un altro task ha gia' fallito (il job resta `failed`
  // con il primo errore): da chiamare a ogni blocco di righe.
  void check_cancel() const;
  // Senza spill dichiarato: invalid_input spill.not_declared.
  spill_area& spill() const;
};

struct job_spec {
  job_spec(const node_spec& node_, std::uint64_t task_count_, std::function<std::string(task_context&)> run_task_)
      : node(node_), task_count(task_count_), run_task(std::move(run_task_)) {}

  const node_spec& node;
  std::uint64_t task_count = 0;
  // Esegue il task e restituisce il digest SHA-256 esadecimale del risultato parziale (richiesto se checkpoint_every > 0).
  std::function<std::string(task_context&)> run_task;
  // Solo per il resume: ricalcola il digest del risultato parziale gia' salvato del task.
  // Chiamato dopo `prepare` e prima del primo task.
  std::function<std::string(std::uint64_t)> verify_partial;
  std::function<void(job_context&)> prepare;   // una volta, prima del primo task
  std::function<void(job_context&)> finalize;  // una volta, dopo l'ultimo task e solo se nessuno e' fallito
  std::filesystem::path checkpoint_dir;        // con checkpoint_name: obbligatori se checkpoint_every > 0 o resume
  std::string checkpoint_name;                 // identifica il job nella directory (vedi write_checkpoint)
  bool resume = false;
  // Solo test: iniettore dei guasti della scrittura dei checkpoint (punti `checkpoint.<passo>`).
  fault_injector fault;
};

struct job_report {
  std::uint64_t task_count = 0;
  std::uint64_t tasks_run = 0;
  std::uint64_t tasks_resumed = 0;
  std::uint64_t checkpoints_written = 0;
  std::size_t max_queue_depth = 0;
  std::uint64_t producer_waits = 0;  // volte in cui il produttore ha trovato la coda piena e ha atteso
  unsigned max_active_workers = 0;
  std::uint64_t job_peak_bytes = 0;
  std::uint64_t spill_peak_bytes = 0;
  std::vector<std::string> partial_digests;  // per indice di task
  // Ammissione sotto pressione (vuoto senza pressure_gate): la prima lettura e, se diversa, quella dell'ammissione.
  // Le letture intermedie sopra soglia sono solo contate: la memoria non cresce con la durata dell'attesa.
  std::vector<admission_sample> admission;
  std::uint64_t pressure_wait_samples = 0;  // letture sopra soglia (attesa)
};

// Esecutore dei job sul pool. Piu' scheduler (o piu' thread) possono condividere lo stesso pool.
class job_scheduler {
 public:
  explicit job_scheduler(memory_pool& pool, affinity_fn affinity = system_affinity, scheduler_tuning tuning = {})
      : pool_(pool), affinity_(std::move(affinity)), tuning_(std::move(tuning)) {}

  // Preflight: piano non valido (invalid_input sched.worker_count / sched.queue_capacity / sched.tile_rows /
  // sched.checkpoint_dir / sched.resume_verifier), picco > budget (budget sched.job_exceeds_budget), spill
  // inutilizzabile, resume non ammesso (resume_mismatch sched.resume_recipe, metadata_missing
  // sched.checkpoint_missing). Nessun task parte se il preflight fallisce. Poi attesa FIFO del pool
  // (budget sched.queue_full con job_queue_capacity job gia' in coda).
  // Primo errore di un worker: gli altri si fermano, join, nessun finalize, errore rilanciato; annullamento:
  // cancelled sched.cancelled. Prenotazioni e file di spill sono rilasciati in ogni caso. Il checkpoint resta su
  // disco anche a job riuscito: lo rimuove il chiamante con remove_checkpoint dopo la pubblicazione riuscita. Non cambia mai tile_rows, worker_count, spill.
  job_report run(const job_plan& plan, const job_spec& spec, const cancel_token& cancel);

 private:
  memory_pool& pool_;
  affinity_fn affinity_;
  scheduler_tuning tuning_;
};

enum class branch_state { completed, failed, cancelled, blocked, not_started };

struct branch_job {
  std::string slot_name;
  std::function<job_result(const job_record&)> work;
};

// Lavoro a file per uno store durevole (come run_job_files).
struct branch_files_job {
  std::string slot_name;
  std::function<job_files_result(const job_record&)> work;
};

struct branch_result {
  std::string slot_name;
  branch_state state = branch_state::not_started;
  std::optional<job_outcome> outcome;  // assente per blocked / not_started
  std::string blocked_by_slot;         // solo per blocked
};

// Esegue un job per slot, nell'ordine delle dipendenze (un padre prima dei suoi discendenti). Un arresto ferma
// solo i job il cui nodo e' discendente di quello fallito (blocked): i rami indipendenti proseguono (T-M2B-09).
// `cancel` e' passato a ogni run_job (cancel richiesto al ritorno del lavoro: nulla si pubblica). Un annullamento ferma tutto: i job non ancora partiti sono not_started. Altre eccezioni si propagano.
std::vector<branch_result> run_branches(dag_store& store, std::vector<branch_job> jobs,
                                        const std::optional<budget_snapshot>& budget = std::nullopt,
                                        const cancel_token* cancel = nullptr);

// Stessa semantica di run_branches per uno store SQLite: ogni ramo e' un run_job_files (un record di provenance
// durevole per job). Il tipo dello store non cambia il percorso a runtime: l'overload e' scelto da chi chiama, e
// run_branches(dag_store&) con uno sqlite_dag_store si ferma con job.durable_store_requires_run_job_files.
std::vector<branch_result> run_branches(sqlite_dag_store& store, std::vector<branch_files_job> jobs,
                                        const std::optional<budget_snapshot>& budget = std::nullopt,
                                        const cancel_token* cancel = nullptr);

// 3 se qualche ramo e' cancelled, altrimenti 2 se qualche ramo e' failed, altrimenti 0.
int branches_exit_code(const std::vector<branch_result>& results) noexcept;

}  // namespace astro
