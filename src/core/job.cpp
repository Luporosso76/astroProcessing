#include "astro/job.hpp"

#include <cstdio>

namespace astro {

int exit_code_for(error_category category) noexcept {
  return category == error_category::cancelled ? exit_code_cancelled : exit_code_stop;
}

namespace {

job_outcome stopped(dag_store& store, const job_record& job, const stop_error& e, const node_spec& spec,
                    std::vector<std::string> warnings) {
  const job_state state = e.category() == error_category::cancelled ? job_state::cancelled : job_state::failed;
  store.finish_job(job.id, state, e.category(), e.code());
  std::fprintf(stderr, "job %llu su %s fermato: %s\n", static_cast<unsigned long long>(job.id),
               job.slot_name.c_str(), e.what());
  return job_outcome{exit_code_for(e.category()), *store.get_job(job.id), e,
                     provenance_record{spec, current_library_versions(), job.generation, state,
                                       std::move(warnings), {}, e}};
}

}  // namespace

job_outcome run_job(dag_store& store, const std::string& slot_name,
                    const std::function<job_result(const job_record&)>& work) {
  const job_record job = store.start_job(slot_name);
  const node_spec spec = *store.get_node(job.node_key);
  std::vector<std::string> warnings;
  artifact_record artifact;
  try {
    job_result result = work(job);
    warnings = std::move(result.warnings);
    artifact = store.add_artifact(std::move(result.artifact));
    store.publish(job.id, artifact.id);
  } catch (const stop_error& e) {
    return stopped(store, job, e, spec, std::move(warnings));
  } catch (...) {
    store.finish_job(job.id, job_state::failed, std::nullopt, "job.unexpected_exception");
    throw;
  }
  provenance_record provenance{spec, current_library_versions(), job.generation, job_state::completed,
                               std::move(warnings), {content_digest{artifact.sha256, artifact.size_bytes}},
                               std::nullopt};
  store.finish_job(job.id, job_state::completed, std::nullopt, "");
  return job_outcome{0, *store.get_job(job.id), std::nullopt, std::move(provenance)};
}

}  // namespace astro
