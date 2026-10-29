#include "astro/job.hpp"

#include <cstdio>

namespace astro {

int exit_code_for(error_category category) noexcept {
  return category == error_category::cancelled ? exit_code_cancelled : exit_code_stop;
}

namespace {

job_outcome stopped(dag_store& store, const job_record& job, const stop_error& e, const node_spec& spec,
                    std::vector<std::string> warnings, const std::optional<budget_snapshot>& budget) {
  const job_state state = e.category() == error_category::cancelled ? job_state::cancelled : job_state::failed;
  if (store.get_job(job.id)->state == job_state::running) store.finish_job(job.id, state, e.category(), e.code());
  std::fprintf(stderr, "job %llu su %s fermato: %s\n", static_cast<unsigned long long>(job.id),
               job.slot_name.c_str(), e.what());
  return job_outcome{exit_code_for(e.category()), *store.get_job(job.id), e,
                     provenance_record{spec, current_library_versions(), job.generation, state,
                                       std::move(warnings), {}, e, budget}};
}

}  // namespace

job_outcome run_job(dag_store& store, const std::string& slot_name,
                    const std::function<job_result(const job_record&)>& work,
                    const std::optional<budget_snapshot>& budget, const cancel_token* cancel) {
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
    return stopped(store, job, e, spec, std::move(warnings), budget);
  } catch (...) {
    if (store.get_job(job.id)->state == job_state::running) {
      store.finish_job(job.id, job_state::failed, std::nullopt, "job.unexpected_exception");
    }
    throw;
  }
  provenance_record provenance{spec, current_library_versions(), job.generation, job_state::completed,
                               std::move(warnings), {content_digest{artifact.sha256, artifact.size_bytes}},
                               std::nullopt, budget};
  return job_outcome{0, *store.get_job(job.id), std::nullopt, std::move(provenance)};
}

job_outcome run_job_files(sqlite_dag_store& store, const std::string& slot_name,
                          const std::function<job_files_result(const job_record&)>& work,
                          const std::optional<budget_snapshot>& budget, const cancel_token* cancel) {
  const job_record job = store.start_job(slot_name);
  const node_spec spec = *store.get_node(job.node_key);
  std::vector<std::string> warnings;
  std::vector<artifact_record> artifacts;
  try {
    job_files_result result = work(job);
    warnings = std::move(result.warnings);
    if (cancel != nullptr && cancel->requested()) {
      throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto prima della pubblicazione");
    }
    artifacts = store.commit_job(job.id, result.files);
  } catch (const stop_error& e) {
    return stopped(store, job, e, spec, std::move(warnings), budget);
  } catch (...) {
    if (store.get_job(job.id)->state == job_state::running) {
      store.finish_job(job.id, job_state::failed, std::nullopt, "job.unexpected_exception");
    }
    throw;
  }
  std::vector<content_digest> digests;
  for (const artifact_record& a : artifacts) digests.push_back(content_digest{a.sha256, a.size_bytes});
  provenance_record provenance{spec, current_library_versions(), job.generation, job_state::completed,
                               std::move(warnings), std::move(digests), std::nullopt, budget};
  return job_outcome{0, *store.get_job(job.id), std::nullopt, std::move(provenance)};
}

}  // namespace astro
