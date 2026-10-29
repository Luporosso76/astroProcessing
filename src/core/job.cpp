#include "astro/job.hpp"

#include <cstdio>
#include <exception>
#include <span>

namespace astro {

int exit_code_for(error_category category) noexcept {
  return category == error_category::cancelled ? exit_code_cancelled : exit_code_stop;
}

namespace {

constexpr const char* provenance_role = provenance_output_role;

stored_file write_provenance(sqlite_dag_store& store, const provenance_record& provenance) {
  const std::string text = provenance_canonical(provenance);
  const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
  return store.write_files({artifact_content{bytes, "json", "provenance", std::nullopt, provenance_role}}).front();
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

void persist_provenance(sqlite_dag_store& durable, const job_record& job, const provenance_record& provenance) {
  const stored_file file = write_provenance(durable, provenance);
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
      persist_provenance(*durable, job, provenance);
    }
  } catch (...) {
    const std::exception_ptr secondary = std::current_exception();
    std::fprintf(stderr, "gestione dell'eccezione del job %llu fallita: %s\n", static_cast<unsigned long long>(job.id),
                 what_of(secondary));
  }
  std::rethrow_exception(original);
}

}  // namespace

job_outcome run_job(dag_store& store, const std::string& slot_name,
                    const std::function<job_result(const job_record&)>& work,
                    const std::optional<budget_snapshot>& budget, const cancel_token* cancel) {
  if (dynamic_cast<sqlite_dag_store*>(&store) != nullptr) {
    throw stop_error(error_category::invalid_input, "job.durable_store_requires_run_job_files",
                     "uno store durevole richiede run_job_files: run_job non persiste la provenance");
  }
  const job_record job = store.start_job(slot_name);
  const node_spec spec = *store.get_node(job.node_key);
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
  const job_record job = store.start_job(slot_name);
  const node_spec spec = *store.get_node(job.node_key);
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
                                         std::move(digests), std::nullopt, budget});
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
