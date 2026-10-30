#include "astro/scheduler.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <set>
#include <pthread.h>

#include <thread>

#include "astro/cpu_dispatch.hpp"
#include "astro/hash.hpp"
#include "astro/job_probe.hpp"
#include "sqlite_internal.hpp"

namespace astro {

int system_affinity(cpu_set_t* set) { return ::sched_getaffinity(0, sizeof(cpu_set_t), set) == 0 ? 0 : errno; }

std::size_t cpu_count(const affinity_fn& affinity) {
  cpu_set_t set;
  CPU_ZERO(&set);
  const int err = affinity(&set);
  if (err != 0) {
    sqlint::throw_errno(error_category::io, "sched.cpu_count", "sched_getaffinity fallita", err);
  }
  const int count = CPU_COUNT(&set);
  if (count <= 0) {
    throw stop_error(error_category::io, "sched.cpu_count", "sched_getaffinity non restituisce nessuna CPU");
  }
  return static_cast<std::size_t>(count);
}

void job_context::check_cancel() const {
  if (cancel.requested() || stopping.requested()) {
    throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto o job in arresto");
  }
}

void task_context::check_cancel() const {
  if (job.cancel.requested() || job.stopping.requested()) {
    throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto o job in arresto",
                     {{"task", std::to_string(index)}});
  }
}

spill_area& task_context::spill() const {
  if (job.spill == nullptr) {
    throw stop_error(error_category::invalid_input, "spill.not_declared", "il job non ha dichiarato uno spill",
                     {{"task", std::to_string(index)}});
  }
  return *job.spill;
}

namespace {

[[noreturn]] void invalid(const char* code, const char* message, stop_error::context_list context = {}) {
  throw stop_error(error_category::invalid_input, code, message, std::move(context));
}

void validate_plan(const job_plan& plan, const job_spec& spec, std::size_t cpus) {
  if (plan.worker_count < 1 || plan.worker_count > cpus) {
    invalid("sched.worker_count", "worker_count fuori da 1..cpu_count",
            {{"worker_count", std::to_string(plan.worker_count)}, {"cpu_count", std::to_string(cpus)}});
  }
  if (plan.queue_capacity < 1) invalid("sched.queue_capacity", "queue_capacity deve essere almeno 1");
  if (plan.tile_rows < 1) invalid("sched.tile_rows", "tile_rows deve essere almeno 1");
  if ((plan.checkpoint_every > 0 || spec.resume) && (spec.checkpoint_dir.empty() || spec.checkpoint_name.empty())) {
    invalid("sched.checkpoint_dir", "checkpoint o resume richiedono checkpoint_dir e checkpoint_name");
  }
  if (spec.resume && !spec.verify_partial) invalid("sched.resume_verifier", "il resume richiede verify_partial");
}

std::uint64_t job_peak(const job_plan& plan, std::uint64_t budget) {
  std::uint64_t tasks = 0, peak = 0;
  if (__builtin_mul_overflow(static_cast<std::uint64_t>(plan.worker_count), plan.per_task_bytes, &tasks) ||
      __builtin_add_overflow(plan.shared_bytes, tasks, &peak)) {
    throw stop_error(error_category::budget, "sched.job_exceeds_budget", "il picco del job non e' rappresentabile",
                     {{"budget", std::to_string(budget)}});
  }
  return peak;
}

struct resumed_state {
  std::vector<bool> done;
  std::vector<std::string> digests;
  std::uint64_t count = 0;
};

// Controlli che non richiedono memoria: ricetta e backend identici a quelli del checkpoint.
std::optional<checkpoint_record> load_resume(const job_plan& plan, const job_spec& spec) {
  std::optional<checkpoint_record> record = read_checkpoint(spec.checkpoint_dir, spec.checkpoint_name);
  if (!record) {
    throw stop_error(error_category::metadata_missing, "sched.checkpoint_missing", "nessun checkpoint da riprendere",
                     {{"nome", spec.checkpoint_name}});
  }
  const std::string digest = recipe_digest(spec.node, plan);
  const auto mismatch = [&](const char* what, const std::string& saved, const std::string& current) {
    return stop_error(error_category::resume_mismatch, "sched.resume_recipe",
                      "il checkpoint appartiene a una ricetta o a un backend diversi",
                      {{"campo", what}, {"checkpoint", saved}, {"corrente", current}});
  };
  if (record->node_key != spec.node.key()) throw mismatch("node_key", record->node_key, spec.node.key());
  if (record->recipe_digest != digest) throw mismatch("recipe_digest", record->recipe_digest, digest);
  if (record->backend != spec.node.backend()) throw mismatch("backend", record->backend, spec.node.backend());
  if (record->task_count != spec.task_count) {
    throw mismatch("task_count", std::to_string(record->task_count), std::to_string(spec.task_count));
  }
  return record;
}

void verify_resume_digests(const checkpoint_record& record, const job_spec& spec) {
  for (std::size_t i = 0; i < record.completed.size(); ++i) {
    const std::string actual = spec.verify_partial(record.completed[i]);
    if (actual != record.partial_digests[i]) {
      throw stop_error(error_category::integrity, "sched.checkpoint_digest",
                       "il risultato parziale non corrisponde al digest del checkpoint",
                       {{"task", std::to_string(record.completed[i])}, {"checkpoint", record.partial_digests[i]},
                        {"calcolato", actual}});
    }
  }
}

struct run_state {
  std::mutex m;
  std::condition_variable not_empty, not_full;
  std::deque<std::uint64_t> queue;
  bool closed = false;
  bool failed = false;
  std::exception_ptr error;
  std::vector<bool> done;
  std::vector<std::string> digests;
  std::uint32_t since_checkpoint = 0;
  job_report report;
  std::atomic<unsigned> active{0};
  std::atomic<unsigned> max_active{0};
  cancel_token stopping;

  void fail(std::exception_ptr e) {
    {
      std::lock_guard lock(m);
      if (!failed) error = std::move(e);
      failed = true;
    }
    stopping.request();
    not_empty.notify_all();
    not_full.notify_all();
  }
};

// Conta i task in esecuzione e ne ricorda il massimo.
class active_scope {
 public:
  explicit active_scope(run_state& st) : st_(st) {
    const unsigned now = st_.active.fetch_add(1) + 1;
    unsigned peak = st_.max_active.load();
    while (peak < now && !st_.max_active.compare_exchange_weak(peak, now)) {
    }
  }
  ~active_scope() { st_.active.fetch_sub(1); }
  active_scope(const active_scope&) = delete;
  active_scope& operator=(const active_scope&) = delete;

 private:
  run_state& st_;
};

struct worker_threads {
  explicit worker_threads(run_state& s) : state(s) {}
  ~worker_threads() {
    {
      std::lock_guard lock(state.m);
      state.closed = true;
      if (!joined) {
        state.failed = true;
        state.stopping.request();
      }
    }
    state.not_empty.notify_all();
    state.not_full.notify_all();
    join();
  }
  void join() {
    for (std::thread& t : threads) {
      if (t.joinable()) t.join();
    }
  }
  run_state& state;
  std::vector<std::thread> threads;
  bool joined = false;
};

void write_progress(run_state& st, const job_spec& spec, const std::string& recipe) {
  checkpoint_record record;
  record.node_key = spec.node.key();
  record.recipe_digest = recipe;
  record.backend = spec.node.backend();
  record.task_count = spec.task_count;
  for (std::uint64_t i = 0; i < spec.task_count; ++i) {
    if (!st.done[i]) continue;
    record.completed.push_back(i);
    record.partial_digests.push_back(st.digests[i]);
  }
  write_checkpoint(spec.checkpoint_dir, spec.checkpoint_name, record, spec.fault ? &spec.fault : nullptr);
  ++st.report.checkpoints_written;
}

void worker_loop(run_state& st, job_context& jc, reservation& job_res, const job_plan& plan, const job_spec& spec,
                 const std::string& recipe, const cpu_set_t* pin) {
  if (pin != nullptr) {
    const int err = ::pthread_setaffinity_np(::pthread_self(), sizeof(cpu_set_t), pin);
    if (err != 0) {
      try {
        sqlint::throw_errno(error_category::io, "sched.cpu_set_apply", "affinita' dei worker non applicabile", err);
      } catch (...) {
        st.fail(std::current_exception());
      }
      return;
    }
  }
  for (;;) {
    std::uint64_t index = 0;
    {
      std::unique_lock lock(st.m);
      st.not_empty.wait(lock, [&] { return !st.queue.empty() || st.closed || st.failed; });
      if (st.failed || st.queue.empty()) return;
      index = st.queue.front();
      st.queue.pop_front();
    }
    st.not_full.notify_one();
    try {
      if (jc.cancel.requested() || jc.stopping.requested()) {
        throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto o job in arresto",
                         {{"task", std::to_string(index)}});
      }
      // Nel thread del worker, prima che il task legga o decodifichi i propri dati (ADR-07).
      require_fp_environment();
      reservation memory = job_res.carve(plan.per_task_bytes, "task");
      task_context tc{jc, index, memory};
      std::string digest;
      {
        const active_scope active(st);
        const std::string task = std::to_string(index);
        job_probe::notify("sched.task", task);
        digest = spec.run_task(tc);
        job_probe::notify("sched.task_done", task);
      }
      if (plan.checkpoint_every > 0 && !is_sha256_hex(digest)) {
        invalid("sched.task_digest", "il digest del risultato parziale non e' SHA-256 esadecimale",
                {{"task", std::to_string(index)}});
      }
      std::lock_guard lock(st.m);
      st.done[index] = true;
      st.digests[index] = std::move(digest);
      ++st.report.tasks_run;
      if (plan.checkpoint_every > 0 && ++st.since_checkpoint >= plan.checkpoint_every) {
        st.since_checkpoint = 0;
        write_progress(st, spec, recipe);
      }
    } catch (...) {
      st.fail(std::current_exception());
      return;
    }
  }
}

// Ammissione sotto pressione (D11, D12): attende senza timeout finche' la PSI e' sotto soglia o arriva un annullamento.
// Conserva la prima lettura e quella dell'ammissione; le letture intermedie sopra soglia sono solo contate.
class pressure_admission {
 public:
  pressure_admission(pressure_gate& gate, const cancel_token& cancel) : gate_(gate), cancel_(cancel) {}
  ~pressure_admission() { set_waiting(false); }
  pressure_admission(const pressure_admission&) = delete;
  pressure_admission& operator=(const pressure_admission&) = delete;

  // Una lettura: true sotto soglia (o sorgente assente). Sopra soglia il job conta come in attesa.
  bool sample_below() {
    check_cancel();
    const std::optional<double> psi = gate_.sample();
    const bool below = !psi || *psi < gate_.threshold;
    if (!first_) first_ = admission_sample{psi, below};
    last_ = admission_sample{psi, below};
    if (!below) ++waits_;
    set_waiting(!below);
    return below;
  }
  void wait_below() {
    while (!sample_below()) sleep_one_poll();
  }
  // Un intervallo di campionamento, interrotto dall'annullamento: il controllo avviene almeno ogni 10 ms. Il tempo si
  // conta a tratti di al piu' 10 ms sottratti da `poll` (sleep_for dorme almeno il tratto): nessuna scadenza assoluta,
  // quindi nessun overflow anche con `poll` vicino al massimo configurabile.
  void sleep_one_poll() {
    constexpr std::chrono::milliseconds slice{10};
    for (std::chrono::milliseconds remaining = gate_.poll; remaining.count() > 0;) {
      check_cancel();
      const std::chrono::milliseconds step = std::min(remaining, slice);
      std::this_thread::sleep_for(step);
      remaining -= step;
    }
    check_cancel();
  }
  // La prima lettura sempre; l'ultima solo se diversa dalla prima.
  std::vector<admission_sample> samples() const {
    if (*last_ == *first_) return {*first_};
    return {*first_, *last_};
  }
  std::uint64_t waits() const noexcept { return waits_; }

 private:
  void check_cancel() const {
    if (cancel_.requested()) {
      throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto durante l'attesa di ammissione");
    }
  }
  void set_waiting(bool waiting) {
    if (waiting == waiting_) return;
    waiting_ = waiting;
    if (waiting) gate_.waiting_jobs.fetch_add(1);
    else gate_.waiting_jobs.fetch_sub(1);
  }

  pressure_gate& gate_;
  const cancel_token& cancel_;
  std::optional<admission_sample> first_, last_;
  std::uint64_t waits_ = 0;
  bool waiting_ = false;
};

// Produce gli indici in ordine; si blocca a coda piena senza scartare nulla.
void produce(run_state& st, const std::vector<std::uint64_t>& pending, std::size_t capacity, const cancel_token& cancel) {
  for (const std::uint64_t index : pending) {
    std::unique_lock lock(st.m);
    if (st.queue.size() >= capacity) ++st.report.producer_waits;
    st.not_full.wait(lock, [&] { return st.queue.size() < capacity || st.failed; });
    if (st.failed) return;
    if (cancel.requested()) {
      lock.unlock();
      try {
        throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto");
      } catch (...) {
        st.fail(std::current_exception());
      }
      return;
    }
    st.queue.push_back(index);
    st.report.max_queue_depth = std::max(st.report.max_queue_depth, st.queue.size());
    lock.unlock();
    st.not_empty.notify_one();
  }
}

}  // namespace

job_report job_scheduler::run(const job_plan& plan, const job_spec& spec, const cancel_token& cancel) {
  ASTRO_ASSERT(static_cast<bool>(spec.run_task));
  job_probe::notify("sched.plan", "shared_bytes=" + std::to_string(plan.shared_bytes) +
                                      ";per_task_bytes=" + std::to_string(plan.per_task_bytes) +
                                      ";worker_count=" + std::to_string(plan.worker_count) +
                                      ";queue_capacity=" + std::to_string(plan.queue_capacity) +
                                      ";tile_rows=" + std::to_string(plan.tile_rows) +
                                      ";spill=" + (plan.spill ? "1" : "0") +
                                      ";checkpoint_every=" + std::to_string(plan.checkpoint_every));
  const std::size_t cpus = cpu_count(affinity_);
  validate_plan(plan, spec, cpus);
  if (tuning_.max_queued_jobs < 1) invalid("sched.queue_capacity", "max_queued_jobs deve essere almeno 1");
  cpu_set_t worker_set;
  CPU_ZERO(&worker_set);
  if (!tuning_.cpu_set.empty()) {
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (const int err = affinity_(&allowed); err != 0) {
      sqlint::throw_errno(error_category::io, "sched.cpu_count", "sched_getaffinity fallita", err);
    }
    for (const unsigned cpu : tuning_.cpu_set) {
      if (cpu >= CPU_SETSIZE || !CPU_ISSET(cpu, &allowed)) {
        invalid("sched.cpu_set", "CPU fuori dall'affinita' del processo", {{"cpu", std::to_string(cpu)}});
      }
      CPU_SET(cpu, &worker_set);
    }
    if (plan.worker_count > static_cast<unsigned>(CPU_COUNT(&worker_set))) {
      invalid("sched.worker_count", "worker_count oltre le CPU dell'insieme",
              {{"worker_count", std::to_string(plan.worker_count)}, {"cpu_set", std::to_string(CPU_COUNT(&worker_set))}});
    }
  }
  const std::uint64_t peak = job_peak(plan, pool_.budget_bytes());
  if (peak > pool_.budget_bytes()) {
    throw stop_error(error_category::budget, "sched.job_exceeds_budget", "il picco del job supera il budget",
                     {{"picco", std::to_string(peak)}, {"budget", std::to_string(pool_.budget_bytes())},
                      {"shared_bytes", std::to_string(plan.shared_bytes)},
                      {"per_task_bytes", std::to_string(plan.per_task_bytes)},
                      {"worker_count", std::to_string(plan.worker_count)}});
  }
  std::optional<spill_area> spill;
  if (plan.spill) spill.emplace(*plan.spill);
  const std::string recipe = recipe_digest(spec.node, plan);
  std::optional<checkpoint_record> resume;
  if (spec.resume) resume = load_resume(plan, spec);

  // La pressione si campiona prima della coda del pool e di nuovo all'ammissione effettiva: se e' salita durante
  // l'attesa della memoria, la prenotazione si restituisce e l'attesa riprende.
  std::optional<pressure_admission> pressure;
  if (tuning_.pressure) pressure.emplace(*tuning_.pressure, cancel);
  reservation job_res;
  for (;;) {
    if (pressure) pressure->wait_below();
    job_res = pool_.reserve(peak, "job", &cancel, tuning_.max_queued_jobs);
    if (!pressure || pressure->sample_below()) break;
    job_res.release();
    pressure->sleep_one_poll();
  }
  reservation shared = job_res.carve(plan.shared_bytes, "shared");

  run_state st;
  st.done.assign(spec.task_count, false);
  st.digests.assign(spec.task_count, std::string());
  st.report.task_count = spec.task_count;
  st.report.job_peak_bytes = peak;
  if (pressure) {
    st.report.admission = pressure->samples();
    st.report.pressure_wait_samples = pressure->waits();
  }
  if (resume) {
    for (std::size_t i = 0; i < resume->completed.size(); ++i) {
      st.done[resume->completed[i]] = true;
      st.digests[resume->completed[i]] = resume->partial_digests[i];
    }
    st.report.tasks_resumed = resume->completed.size();
  }
  std::vector<std::uint64_t> pending;
  for (std::uint64_t i = 0; i < spec.task_count; ++i) {
    if (!st.done[i]) pending.push_back(i);
  }

  const bool pin = !tuning_.cpu_set.empty();
  job_context jc{plan, shared, spill ? &*spill : nullptr, cancel, st.stopping};
  job_probe::notify("sched.prepare");
  if (spec.prepare) spec.prepare(jc);
  if (resume) verify_resume_digests(*resume, spec);
  {
    worker_threads workers(st);
    for (unsigned w = 0; w < plan.worker_count; ++w) {
      workers.threads.emplace_back(worker_loop, std::ref(st), std::ref(jc), std::ref(job_res), std::cref(plan),
                                   std::cref(spec), std::cref(recipe), pin ? &worker_set : nullptr);
    }
    produce(st, pending, plan.queue_capacity, cancel);
    {
      std::lock_guard lock(st.m);
      st.closed = true;
    }
    st.not_empty.notify_all();
    workers.joined = true;
    workers.join();
  }
  if (st.error) std::rethrow_exception(st.error);
  if (cancel.requested()) {
    throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto prima della pubblicazione");
  }
  ASTRO_ASSERT(st.report.tasks_run == pending.size());
  job_probe::notify("sched.finalize");
  if (spec.finalize) spec.finalize(jc);
  if (cancel.requested()) {
    throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto durante finalize");
  }
  if (spill) st.report.spill_peak_bytes = spill->peak_bytes();
  st.report.max_active_workers = st.max_active.load();
  st.report.partial_digests = std::move(st.digests);
  return std::move(st.report);
}

namespace {

struct branch_slot {
  std::size_t position = 0;
  std::string target;
  std::set<std::string> descendants;
};

}  // namespace

namespace {

// Ordine, blocco dei discendenti e cancel comuni ai due percorsi; `run_one(i)` esegue il job i-esimo.
std::vector<branch_result> run_branches_with(const dag_store& store, const std::vector<std::string>& names,
                                             const std::function<job_outcome(std::size_t)>& run_one) {
  std::vector<branch_slot> slots(names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    const std::optional<slot_record> slot = store.get_slot(names[i]);
    if (!slot) {
      throw stop_error(error_category::invalid_input, "dag.unknown_slot", "slot inesistente",
                       {{"slot", names[i]}});
    }
    slots[i].position = i;
    slots[i].target = slot->target_node_key;
    const std::vector<std::string> desc = store.descendants(slot->target_node_key);
    slots[i].descendants.insert(desc.begin(), desc.end());
  }
  const auto depends_on = [&](std::size_t child, std::size_t parent) {
    return child != parent && slots[parent].descendants.count(slots[child].target) > 0;
  };

  std::vector<branch_result> results(names.size());
  for (std::size_t i = 0; i < names.size(); ++i) results[i].slot_name = names[i];
  std::vector<bool> handled(names.size(), false);
  bool cancelled = false;
  for (std::size_t round = 0; round < names.size(); ++round) {
    std::size_t next = names.size();
    for (std::size_t i = 0; i < names.size() && next == names.size(); ++i) {
      if (handled[i]) continue;
      bool ready = true;
      for (std::size_t p = 0; p < names.size() && ready; ++p) ready = handled[p] || !depends_on(i, p);
      if (ready) next = i;
    }
    ASTRO_ASSERT(next < names.size());
    handled[next] = true;
    if (results[next].state == branch_state::blocked) continue;
    if (cancelled) continue;
    job_outcome outcome = run_one(next);
    if (outcome.exit_code == 0) {
      results[next].state = branch_state::completed;
    } else if (outcome.exit_code == exit_code_cancelled) {
      results[next].state = branch_state::cancelled;
      cancelled = true;
    } else {
      results[next].state = branch_state::failed;
      for (std::size_t j = 0; j < names.size(); ++j) {
        if (!handled[j] && depends_on(j, next)) {
          results[j].state = branch_state::blocked;
          results[j].blocked_by_slot = names[next];
        }
      }
    }
    results[next].outcome = std::move(outcome);
  }
  return results;
}

}  // namespace

std::vector<branch_result> run_branches(dag_store& store, std::vector<branch_job> jobs,
                                        const std::optional<budget_snapshot>& budget, const cancel_token* cancel) {
  std::vector<std::string> names;
  for (const branch_job& j : jobs) names.push_back(j.slot_name);
  return run_branches_with(store, names, [&](std::size_t i) {
    return run_job(store, jobs[i].slot_name, jobs[i].work, budget, cancel);
  });
}

std::vector<branch_result> run_branches(sqlite_dag_store& store, std::vector<branch_files_job> jobs,
                                        const std::optional<budget_snapshot>& budget, const cancel_token* cancel) {
  std::vector<std::string> names;
  for (const branch_files_job& j : jobs) names.push_back(j.slot_name);
  return run_branches_with(store, names, [&](std::size_t i) {
    return run_job_files(store, jobs[i].slot_name, jobs[i].work, budget, cancel);
  });
}

int branches_exit_code(const std::vector<branch_result>& results) noexcept {
  bool failed = false;
  for (const branch_result& r : results) {
    if (r.state == branch_state::cancelled) return exit_code_cancelled;
    if (r.state == branch_state::failed) failed = true;
  }
  return failed ? exit_code_stop : 0;
}

}  // namespace astro
