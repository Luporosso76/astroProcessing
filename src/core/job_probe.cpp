#include "astro/job_probe.hpp"

#include "astro/job.hpp"

namespace astro {

namespace {

job_probe::observer& current_observer() {
  static job_probe::observer o;
  return o;
}

job_probe::work_wrapper& current_wrapper() {
  static job_probe::work_wrapper w;
  return w;
}

}  // namespace

namespace job_probe {

void set_observer(observer o) { current_observer() = std::move(o); }

bool active() { return static_cast<bool>(current_observer()); }

void notify(std::string_view point, std::string_view detail) {
  const observer& o = current_observer();
  if (o) o(point, detail);
}

void set_work_wrapper(work_wrapper w) { current_wrapper() = std::move(w); }

job_files_result run_work(const job_record& job, const work_fn& work) {
  const work_wrapper& w = current_wrapper();
  return w ? w(job, work) : work(job);
}

}  // namespace job_probe

}  // namespace astro
