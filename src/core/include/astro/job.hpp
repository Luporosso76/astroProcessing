#pragma once

#include <filesystem>
#include <functional>
#include <memory>
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

// Arresto prima del lavoro (NFR-FBK-01.a, ADR-05). Gli arresti del confine che precedono `work` si registrano e poi si
// propagano al chiamante come eccezione: lo stop_error con la stessa categoria, codice, messaggio e contesto; ogni altra
// eccezione invariata, registrata come integrity job.unexpected_exception. Il chiamante ne ricava il codice di uscita
// con exit_code_for. Sono arresti prima del lavoro:
// - l'ambiente FP vietato (require_fp_environment, ADR-07: numeric cpu.fp_mode), controllato prima di leggere il nodo;
// - la preparazione di prepare_job_start;
// - per run_job, uno sqlite_dag_store (invalid_input job.durable_store_requires_run_job_files): la provenance
//   durevole la scrive solo run_job_files;
// - la lettura del nodo del job avviato: nodo assente (graph dag.missing_node con `nodo`) o lettura fallita.
// Registrare significa chiudere il job `failed` (`cancelled` per la categoria cancelled) con categoria e codice, avviando
// il job se non e' ancora avviato, e su store durevole scrivere come artefatto `provenance` del nodo la provenance
// stop_before_work_record: chiave del nodo, versioni, generazione, stato, arresto ed eventuale resume. Il record non
// porta parametri, node_spec, digest degli input, avvisi, budget ne' details: nulla analizza o serializza numeri in
// virgola mobile, anche con l'ambiente FP vietato. Se la registrazione fallisce, l'arresto originale si propaga con
// l'errore secondario nel contesto `registrazione_arresto` (diagnosi anche su stderr); un'eccezione non stop_error si
// propaga invariata con la diagnosi su stderr.
// L'avvio del job (start_job: slot inesistente o escluso, store in sola lettura, errore SQL) non si registra: e' la
// scrittura del record stesso che non riesce. L'errore si propaga invariato, nessun job esiste e non si ritenta.
//
// Confine del job: arresti prima del lavoro come sopra, avvio del job sullo slot, lettura del nodo, esecuzione di
// `work`, registrazione e pubblicazione dell'artefatto. Uno stop_error di `work` o della pubblicazione marca il job
// `failed` (`cancelled` per la categoria cancelled), scrive la diagnosi su stderr e restituisce 2 (3 per cancelled).
// Ogni altra eccezione marca il job `failed` e si rilancia. Nessun percorso alternativo. `budget`, se presente, entra
// nella provenance (riuscito o fermato dentro `work`). Con `cancel` richiesto quando `work` ritorna, il job e' cancelled
// e nulla si pubblica. Le prenotazioni del pool appartengono a `work`: si rilasciano per RAII anche quando lancia.
job_outcome run_job(dag_store& store, const std::string& slot_name,
                    const std::function<job_result(const job_record&)>& work,
                    const std::optional<budget_snapshot>& budget = std::nullopt,
                    const cancel_token* cancel = nullptr);

struct resource_config;
struct resource_plan;

// Output multipli (ruoli distinti) gia' scritti con write_files.
struct job_files_result {
  std::vector<stored_file> files;
  std::vector<std::string> warnings;
  // Osservazioni dell'operatore per la provenance di un job riuscito (`details`, ADR-05). Le chiavi `cpu` e
  // `resource_plan` le compone run_job_files.
  std::optional<json> details = std::nullopt;
  // Piano scelto da plan_resources ed eseguito: run_job_files lo registra in details.resource_plan.
  std::shared_ptr<const resource_plan> plan = nullptr;
  // Checkpoint e staging nominato del job nella directory di scratch (OB-19): restano dopo stop e annullamento, come
  // stato del resume; run_job_files li rimuove solo dopo un commit_job riuscito.
  struct scratch_state {
    std::filesystem::path dir;
    std::string checkpoint_name;  // remove_checkpoint
    std::string staging_name;     // remove_named_spill_file
  };
  std::optional<scratch_state> scratch = std::nullopt;
};

// Richiesta del job al runner (docs/contracts/BASE-confini.md, OB-09 e OB-10).
struct job_request {
  // Nodo ricostruito dalla richiesta. run_job_files lo costruisce dopo il controllo dell'ambiente FP e dopo aver letto
  // il nodo dello slot, prima di `work`: chiave diversa da quella dello slot = arresto invalid_input `mismatch_code`
  // con le due chiavi (`nodo_slot`, `nodo_richiesta`). Vuoto: nessun confronto (composizioni come run_branches).
  std::function<node_spec()> node;
  std::string mismatch_code = "job.node_mismatch";
  // Configurazione delle risorse ricevuta dal job: con `budget`, run_job_files la serializza in budget.resources dopo
  // il controllo dell'ambiente FP. Valida per tutta la chiamata.
  const resource_config* resources = nullptr;
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
// Osservazioni comuni (OB-09): con `budget` e `request.resources` il budget registrato porta budget.resources; la
// provenance di un job riuscito porta details.cpu se il backend del nodo e' `cpu_*` (backend e `simd` dal nodo,
// capacita' lette dalla CPU) e details.resource_plan se `work` dichiara il piano. Una chiave gia' presente nei
// `details` dell'operatore e' un arresto invalid_input dag.param_duplicate_key.
// Con `scratch` dichiarato da `work`, dopo commit_job riuscito si rimuovono checkpoint e staging; un errore della
// rimozione (io sched.checkpoint_remove, spill.staging_remove) si propaga col job gia' `completed` e pubblicato.
job_outcome run_job_files(sqlite_dag_store& store, const std::string& slot_name,
                          const std::function<job_files_result(const job_record&)>& work,
                          const std::optional<budget_snapshot>& budget = std::nullopt,
                          const cancel_token* cancel = nullptr, const job_request& request = {});

// Solo test (suite di conformita', docs/contracts/BASE-confini.md §3.2): involucro del `work` che run_job_files invoca.
// Si imposta prima del job e non cambia durante l'esecuzione. Vuoto: run_job_files invoca `work` direttamente.
namespace job_probe {
using work_fn = std::function<job_files_result(const job_record&)>;
using work_wrapper = std::function<job_files_result(const job_record&, const work_fn&)>;
void set_work_wrapper(work_wrapper w);
job_files_result run_work(const job_record& job, const work_fn& work);
}  // namespace job_probe

// Preparazione di un job durevole sullo slot prima del lavoro (in run_job_files: controllo dell'ambiente FP e
// serializzazione delle risorse nel budget): un'eccezione di `prepare` e' un arresto prima del lavoro, registrato e
// propagato come sopra. `prepare` non costruisce nodi ne' serializza numeri in virgola mobile se e' chiamata prima di
// run_job_files: il controllo dell'ambiente FP del runner non l'ha ancora preceduta.
void prepare_job_start(sqlite_dag_store& store, const std::string& slot_name, const std::function<void()>& prepare);

}  // namespace astro
