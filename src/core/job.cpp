#include "astro/job.hpp"

#include <cstdio>
#include <exception>
#include <new>
#include <optional>
#include <span>

#include "astro/cpu_dispatch.hpp"

namespace astro {

int exit_code_for(error_category category) noexcept {
  return category == error_category::cancelled ? exit_code_cancelled : exit_code_stop;
}

namespace {

constexpr const char* provenance_role = provenance_output_role;
// Contesto aggiunto allo stop originale quando la sua registrazione prima del lavoro fallisce.
constexpr const char* record_failure_key = "registrazione_arresto";

stored_file write_provenance_text(sqlite_dag_store& store, const std::string& text) {
  const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
  return store.write_files({artifact_content{bytes, "json", "provenance", std::nullopt, provenance_role}}).front();
}

stored_file write_provenance(sqlite_dag_store& store, const provenance_record& provenance) {
  return write_provenance_text(store, provenance_canonical(provenance));
}

// Senza allocazioni: il puntatore resta valido finche' `ep` e' vivo.
const char* what_of(const std::exception_ptr& ep) noexcept {
  try {
    std::rethrow_exception(ep);
  } catch (const std::exception& ex) {
    return ex.what();
  } catch (...) {
    return "eccezione sconosciuta";
  }
}

void persist_provenance_text(sqlite_dag_store& durable, const job_record& job, const std::string& text) {
  const stored_file file = write_provenance_text(durable, text);
  artifact_record a;
  a.node_key = job.node_key;
  a.generation = job.generation;
  a.sha256 = file.digest().sha256;
  a.size_bytes = file.digest().size_bytes;
  a.kind = file.kind();
  a.rel_path = file.rel_path();
  a.output_role = provenance_role;
  durable.add_artifact(std::move(a));
}

void persist_provenance(sqlite_dag_store& durable, const job_record& job, const provenance_record& provenance) {
  persist_provenance_text(durable, job, provenance_canonical(provenance));
}

// Chiude il job `running` con `cause`; restituisce lo stato durevole finale, che e' la fonte della provenance.
job_record settle(dag_store& store, const job_record& job, error_category category, const std::string& code) {
  job_record after = *store.get_job(job.id);
  if (after.state == job_state::running) {
    store.finish_job(job.id, category == error_category::cancelled ? job_state::cancelled : job_state::failed, category, code);
    after = *store.get_job(job.id);
  }
  return after;
}

// Causa registrata nel job; `candidate` la rappresenta se ha stessa categoria e codice.
stop_error persisted_cause(const job_record& after, const stop_error* candidate) {
  if (!after.stop_category.has_value()) {
    throw stop_error(error_category::integrity, "job.stop_cause_missing", "job fermato senza causa registrata",
                     {{"job", std::to_string(after.id)}});
  }
  if (candidate != nullptr && candidate->category() == *after.stop_category && candidate->code() == after.stop_code) {
    return *candidate;
  }
  return stop_error(*after.stop_category, after.stop_code, "causa registrata dallo store prima dell'errore successivo");
}

job_outcome stopped(dag_store& store, sqlite_dag_store* durable, const job_record& job, const stop_error& e,
                    const node_spec& spec, std::vector<std::string> warnings,
                    const std::optional<budget_snapshot>& budget) {
  const std::exception_ptr original = std::current_exception();  // chiamata solo dentro il catch dell'arresto
  job_record after;
  try {
    after = settle(store, job, e.category(), e.code());
  } catch (...) {
    const std::exception_ptr secondary = std::current_exception();
    std::fprintf(stderr, "chiusura del job %llu fallita (%s); si propaga l'arresto originale: %s\n",
                 static_cast<unsigned long long>(job.id), what_of(secondary), e.what());
    std::rethrow_exception(original);
  }
  if (after.state == job_state::completed) std::rethrow_exception(original);
  // Vincolo: costruire l'esito (diagnosi, provenance, not_persisted) puo' allocare. Se fallisce, qualunque eccezione
  // secondaria (anche bad_alloc) e' diagnosticata su stderr senza allocazioni e si propaga l'arresto originale al
  // posto dell'esito: mai un'eccezione secondaria al suo posto.
  try {
    std::fprintf(stderr, "job %llu su %s fermato: %s\n", static_cast<unsigned long long>(job.id),
                 job.slot_name.c_str(), e.what());
    const stop_error cause = persisted_cause(after, &e);
    std::optional<stop_error> subsequent;
    if (cause.category() != e.category() || cause.code() != e.code()) subsequent = e;
    provenance_record provenance{spec, current_library_versions(), job.generation, after.state,
                                 std::move(warnings), {}, cause, budget};
    if (durable != nullptr) provenance.resume = durable->resume_context();
    std::optional<stop_error> not_persisted;
    if (durable != nullptr) {
      try {
        persist_provenance(*durable, job, provenance);
      } catch (const stop_error& pe) {
        std::fprintf(stderr, "provenance dell'arresto non scritta: %s\n", pe.what());
        not_persisted = pe;
      } catch (...) {
        const std::exception_ptr secondary = std::current_exception();
        std::fprintf(stderr, "provenance dell'arresto non scritta: %s\n", what_of(secondary));
        not_persisted = stop_error(error_category::integrity, "job.provenance_write_failed", what_of(secondary));
      }
    }
    return job_outcome{exit_code_for(cause.category()), after, cause, std::move(provenance), std::move(not_persisted),
                       std::move(subsequent)};
  } catch (...) {
    const std::exception_ptr secondary = std::current_exception();
    std::fprintf(stderr, "esito del job %llu non costruibile (%s); si propaga l'arresto originale: %s\n",
                 static_cast<unsigned long long>(job.id), what_of(secondary), e.what());
    std::rethrow_exception(original);
  }
}

// Eccezione estranea: il job si chiude `failed` job.unexpected_exception (categoria integrity), la provenance durevole
// rispecchia lo stato registrato, poi l'eccezione originale si rilancia invariata. Una provenance non scrivibile non
// la maschera: qualunque eccezione secondaria (chiusura del job, scrittura) e' diagnosticata su stderr (ADR-05) e non maschera mai l'originale.
[[noreturn]] void unexpected(dag_store& store, sqlite_dag_store* durable, const job_record& job, const node_spec& spec,
                             std::vector<std::string> warnings, const std::optional<budget_snapshot>& budget) {
  const std::exception_ptr original = std::current_exception();
  try {
    const job_record after = settle(store, job, error_category::integrity, "job.unexpected_exception");
    if (durable != nullptr && after.state != job_state::completed) {
      provenance_record provenance{spec, current_library_versions(), job.generation, after.state,
                                   std::move(warnings), {}, persisted_cause(after, nullptr), budget};
      provenance.resume = durable->resume_context();
      persist_provenance(*durable, job, provenance);
    }
  } catch (...) {
    const std::exception_ptr secondary = std::current_exception();
    std::fprintf(stderr, "gestione dell'eccezione del job %llu fallita: %s\n", static_cast<unsigned long long>(job.id),
                 what_of(secondary));
  }
  std::rethrow_exception(original);
}

// Causa registrata per un arresto prima del lavoro: lo stop_error stesso, oppure integrity job.unexpected_exception.
stop_error cause_of(const std::exception_ptr& ep) {
  try {
    std::rethrow_exception(ep);
  } catch (const stop_error& e) {
    return e;
  } catch (...) {
    return stop_error(error_category::integrity, "job.unexpected_exception", what_of(ep));
  }
}

// Rilancia `original` dopo una registrazione fallita: uno stop_error con stessa categoria, codice, messaggio e contesto
// piu' `registrazione_arresto` con l'errore secondario; ogni altra eccezione invariata. Se il contesto non si puo'
// aggiungere (memoria esaurita) si rilancia lo stop originale invariato: la diagnosi secondaria e' gia' su stderr.
[[noreturn]] void rethrow_with_record_failure(const std::exception_ptr& original, const char* secondary) {
  try {
    std::rethrow_exception(original);
  } catch (const stop_error& e) {
    std::optional<stop_error> annotated;
    try {
      annotated.emplace(e);
      annotated->add_context(record_failure_key, secondary);
    } catch (const std::bad_alloc&) {
      annotated.reset();
    }
    if (annotated) throw std::move(*annotated);
    throw;
  }
}

// Arresto al confine prima del lavoro del job (NFR-FBK-01.a, ADR-05): registra `original` e lo rilancia invariato. Lo
// stop_error e' la causa; ogni altra eccezione si registra come integrity job.unexpected_exception. Senza `job` il job si
// avvia qui, solo per registrare l'arresto. Il job si chiude con la causa e, su store durevole, si scrive la provenance
// dell'arresto senza il nodo (stop_before_work_record: stringhe e interi, nessun numero in virgola mobile analizzato o
// serializzato). Registrazione fallita: diagnosi su stderr e rethrow_with_record_failure, mai l'errore secondario al
// posto dell'originale.
[[noreturn]] void stop_before_work(dag_store& store, sqlite_dag_store* durable, const std::string& slot_name,
                                   std::optional<job_record> job, const std::exception_ptr& original) {
  try {
    const stop_error cause = cause_of(original);
    if (!job) job = store.start_job(slot_name);
    const job_record after = settle(store, *job, cause.category(), cause.code());
    std::fprintf(stderr, "job %llu su %s fermato prima del lavoro: %s\n", static_cast<unsigned long long>(after.id),
                 slot_name.c_str(), cause.what());
    if (durable != nullptr) {
      const stop_before_work_record record{after.node_key, current_library_versions(), after.generation, after.state,
                                           persisted_cause(after, &cause), durable->resume_context()};
      persist_provenance_text(*durable, after, stop_before_work_canonical(record));
    }
  } catch (...) {
    const std::exception_ptr secondary = std::current_exception();
    std::fprintf(stderr, "registrazione dell'arresto prima del lavoro su %s fallita (%s); si propaga l'arresto: %s\n",
                 slot_name.c_str(), what_of(secondary), what_of(original));
    rethrow_with_record_failure(original, what_of(secondary));
  }
  std::rethrow_exception(original);
}

// Nodo del job avviato; nodo assente: graph dag.missing_node. Ogni errore della lettura e' un arresto prima del lavoro.
node_spec node_of(dag_store& store, sqlite_dag_store* durable, const job_record& job) {
  try {
    std::optional<node_spec> node = store.get_node(job.node_key);
    if (!node) {
      throw stop_error(error_category::graph, "dag.missing_node", "nodo del job assente dallo store",
                       {{"nodo", job.node_key}});
    }
    return std::move(*node);
  } catch (...) {
    stop_before_work(store, durable, job.slot_name, job, std::current_exception());
  }
}

}  // namespace

void prepare_job_start(sqlite_dag_store& store, const std::string& slot_name, const std::function<void()>& prepare) {
  try {
    prepare();
  } catch (...) {
    stop_before_work(store, &store, slot_name, std::nullopt, std::current_exception());
  }
}

job_outcome run_job(dag_store& store, const std::string& slot_name,
                    const std::function<job_result(const job_record&)>& work,
                    const std::optional<budget_snapshot>& budget, const cancel_token* cancel) {
  sqlite_dag_store* const durable = dynamic_cast<sqlite_dag_store*>(&store);
  try {
    if (durable != nullptr) {
      throw stop_error(error_category::invalid_input, "job.durable_store_requires_run_job_files",
                       "uno store durevole richiede run_job_files: run_job non persiste la provenance");
    }
    require_fp_environment();  // prima di leggere il nodo (ADR-07)
  } catch (...) {
    stop_before_work(store, durable, slot_name, std::nullopt, std::current_exception());
  }
  const job_record job = store.start_job(slot_name);
  const node_spec spec = node_of(store, nullptr, job);
  std::vector<std::string> warnings;
  artifact_record artifact;
  try {
    job_result result = work(job);
    warnings = std::move(result.warnings);
    if (cancel != nullptr && cancel->requested()) {
      throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto prima della pubblicazione");
    }
    artifact = store.complete_job(job.id, std::move(result.artifact));
  } catch (const stop_error& e) {
    return stopped(store, nullptr, job, e, spec, std::move(warnings), budget);
  } catch (...) {
    unexpected(store, nullptr, job, spec, std::move(warnings), budget);
  }
  provenance_record provenance{spec, current_library_versions(), job.generation, job_state::completed,
                               std::move(warnings), {content_digest{artifact.sha256, artifact.size_bytes}},
                               std::nullopt, budget};
  return job_outcome{0, *store.get_job(job.id), std::nullopt, std::move(provenance), std::nullopt, std::nullopt};
}

job_outcome run_job_files(sqlite_dag_store& store, const std::string& slot_name,
                          const std::function<job_files_result(const job_record&)>& work,
                          const std::optional<budget_snapshot>& budget, const cancel_token* cancel) {
  // Prima di leggere il nodo, che analizza i parametri numerici, anche subnormali (ADR-07).
  prepare_job_start(store, slot_name, require_fp_environment);
  const job_record job = store.start_job(slot_name);
  const node_spec spec = node_of(store, &store, job);
  std::vector<std::string> warnings;
  std::optional<provenance_record> provenance;
  try {
    job_files_result result = work(job);
    warnings = std::move(result.warnings);
    if (cancel != nullptr && cancel->requested()) {
      throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto prima della pubblicazione");
    }
    std::vector<content_digest> digests;
    for (const stored_file& f : result.files) digests.push_back(f.digest());
    provenance.emplace(provenance_record{spec, current_library_versions(), job.generation, job_state::completed, warnings,
                                         std::move(digests), std::nullopt, budget, std::move(result.details)});
    provenance->resume = store.resume_context();
    std::vector<stored_file> files = std::move(result.files);
    files.push_back(write_provenance(store, *provenance));
    store.commit_job(job.id, files);
  } catch (const stop_error& e) {
    return stopped(store, &store, job, e, spec, std::move(warnings), budget);
  } catch (...) {
    unexpected(store, &store, job, spec, std::move(warnings), budget);
  }
  return job_outcome{0, *store.get_job(job.id), std::nullopt, std::move(*provenance), std::nullopt, std::nullopt};
}

}  // namespace astro
