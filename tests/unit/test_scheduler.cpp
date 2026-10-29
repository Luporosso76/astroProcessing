#include <gtest/gtest.h>

#include <atomic>
#include <map>
#include <set>
#include <thread>
#include <vector>

#include "astro/dag_store.hpp"
#include "astro/provenance.hpp"
#include "astro/scheduler.hpp"
#include "m2d_support.hpp"

namespace {

using namespace m2d;

constexpr unsigned test_cpus = 4;

job_plan plan_of(unsigned workers, std::uint64_t shared = 0, std::uint64_t per_task = 1024) {
  job_plan p;
  p.shared_bytes = shared;
  p.per_task_bytes = per_task;
  p.worker_count = workers;
  p.queue_capacity = 4;
  p.tile_rows = 16;
  return p;
}

std::string digest_for(std::uint64_t index) { return digest_text("result-" + std::to_string(index)); }

job_result result_for(const job_record& job) {
  artifact_record a;
  a.node_key = job.node_key;
  a.generation = job.generation;
  a.sha256 = std::string(64, '5');
  a.size_bytes = 8;
  a.kind = "image";
  return job_result{a, {}};
}

struct single_slot_store {
  memory_dag_store store;
  node_spec node = test_node("single");
  single_slot_store() {
    store.put_node(node);
    store.create_slot("s", node.key());
  }
};

job_outcome run_quiet(dag_store& store, const std::string& slot, const std::function<job_result(const job_record&)>& work,
                      const std::optional<budget_snapshot>& budget = std::nullopt) {
  testing::internal::CaptureStderr();
  try {
    job_outcome out = run_job(store, slot, work, budget);
    testing::internal::GetCapturedStderr();
    return out;
  } catch (...) {
    testing::internal::GetCapturedStderr();
    throw;
  }
}

stop_error injected() { return stop_error(error_category::io, "test.injected", "guasto iniettato"); }

bool no_publication(const memory_dag_store& store, const node_spec& node) {
  return store.read_slot("s").state == freshness::missing && store.artifacts_for(node.key()).empty();
}

// ---------------------------------------------------------------- esecuzione di base

TEST(Scheduler, runs_every_task_exactly_once_and_reports_digests) {
  for (const unsigned workers : {1u, 2u, 4u}) {
    memory_pool pool(1 << 20);
    job_scheduler sched(pool, affinity_of(test_cpus));
    const node_spec node = test_node("basic");
    std::vector<std::atomic<int>> runs(100);
    job_spec spec{node, 100, [&](task_context& ctx) {
                    ++runs[ctx.index];
                    pool_buffer scratch(ctx.memory, 512);
                    return digest_for(ctx.index);
                  }};
    cancel_token cancel;
    const job_report report = sched.run(plan_of(workers, 4096, 1024), spec, cancel);
    for (const auto& r : runs) EXPECT_EQ(r.load(), 1);
    EXPECT_EQ(report.tasks_run, 100u);
    EXPECT_EQ(report.partial_digests.size(), 100u);
    EXPECT_EQ(report.partial_digests[37], digest_for(37));
    EXPECT_EQ(report.job_peak_bytes, 4096u + workers * 1024u);
    EXPECT_EQ(pool.reserved_bytes(), 0u);
    EXPECT_EQ(pool.peak_reserved_bytes(), 4096u + workers * 1024u);
  }
}

TEST(Scheduler, the_shared_reservation_and_prepare_and_finalize_see_the_plan) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("hooks");
  std::vector<std::string> events;
  std::mutex m;
  const auto note = [&](std::string e) {
    std::lock_guard lock(m);
    events.push_back(std::move(e));
  };
  job_spec spec{node, 3, [&](task_context& ctx) {
                  note("task");
                  EXPECT_EQ(ctx.job.shared.bytes(), 2048u);
                  EXPECT_EQ(ctx.memory.bytes(), 256u);
                  EXPECT_EQ(ctx.job.plan.tile_rows, 16u);
                  return digest_for(ctx.index);
                }};
  spec.prepare = [&](job_context& jc) {
    note("prepare");
    pool_buffer shared_buffer(jc.shared, 2048);
  };
  spec.finalize = [&](job_context&) { note("finalize"); };
  cancel_token cancel;
  sched.run(plan_of(2, 2048, 256), spec, cancel);
  ASSERT_EQ(events.size(), 5u);
  EXPECT_EQ(events.front(), "prepare");
  EXPECT_EQ(events.back(), "finalize");
}

// ---------------------------------------------------------------- T-M2D-05 / 06

TEST(Scheduler, T_M2D_05_impossible_job_is_rejected_before_any_task) {
  memory_pool pool(32 * MiB);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("impossible");
  std::atomic<int> tasks{0}, hooks{0};
  job_spec spec{node, 10, [&](task_context&) {
                  ++tasks;
                  return digest_for(0);
                }};
  spec.prepare = [&](job_context&) { ++hooks; };
  cancel_token cancel;
  try {
    sched.run(plan_of(2, 20 * MiB, 8 * MiB), spec, cancel);
    ADD_FAILURE() << "stop attesa";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::budget);
    EXPECT_EQ(e.code(), "sched.job_exceeds_budget");
    std::string ctx;
    for (const auto& [k, v] : e.context()) ctx += k + "=" + v + ";";
    EXPECT_NE(ctx.find("picco=" + std::to_string(36 * MiB)), std::string::npos) << ctx;
    EXPECT_NE(ctx.find("budget=" + std::to_string(32 * MiB)), std::string::npos) << ctx;
  }
  EXPECT_STOP(sched.run(plan_of(4, 0, UINT64_MAX / 2), spec, cancel), error_category::budget,
              "sched.job_exceeds_budget");
  EXPECT_STOP(sched.run(plan_of(1, UINT64_MAX, 1), spec, cancel), error_category::budget, "sched.job_exceeds_budget");
  EXPECT_EQ(tasks.load(), 0);
  EXPECT_EQ(hooks.load(), 0);
  EXPECT_EQ(pool.peak_reserved_bytes(), 0u);
  EXPECT_NO_THROW(sched.run(plan_of(4, 8 * MiB, 6 * MiB), spec, cancel));  // picco = budget esatto
  EXPECT_EQ(tasks.load(), 10);
  EXPECT_EQ(pool.peak_reserved_bytes(), 32 * MiB);
}

TEST(Scheduler, T_M2D_06_tight_budget_stops_and_never_shrinks_tiles_or_workers) {
  memory_pool pool(16 * MiB);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("tight");
  std::atomic<int> tasks{0};
  std::atomic<std::uint32_t> seen_tile_rows{0};
  std::mutex m;
  std::condition_variable cv;
  unsigned arrived = 0;
  job_spec spec{node, 8, [&](task_context& ctx) {
                  ++tasks;
                  seen_tile_rows = ctx.job.plan.tile_rows;
                  std::unique_lock lock(m);
                  ++arrived;
                  cv.notify_all();
                  EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(20), [&] { return arrived >= 4; }))
                      << "i quattro worker dichiarati non sono partiti insieme";
                  return digest_for(ctx.index);
                }};
  job_plan too_big = plan_of(4, 2 * MiB, 4 * MiB);  // 18 MiB > 16 MiB
  too_big.tile_rows = 512;
  const job_plan before = too_big;
  cancel_token cancel;
  EXPECT_STOP(sched.run(too_big, spec, cancel), error_category::budget, "sched.job_exceeds_budget");
  EXPECT_EQ(tasks.load(), 0);
  EXPECT_EQ(too_big.tile_rows, before.tile_rows);
  EXPECT_EQ(too_big.worker_count, before.worker_count);

  job_plan fits = plan_of(4, 2 * MiB, 3 * MiB);  // 14 MiB
  fits.tile_rows = 512;
  const job_report report = sched.run(fits, spec, cancel);
  EXPECT_EQ(seen_tile_rows.load(), 512u);
  EXPECT_EQ(report.max_active_workers, 4u);
  EXPECT_EQ(tasks.load(), 8);
}

// ---------------------------------------------------------------- T-M2D-15

TEST(Scheduler, T_M2D_15_worker_count_is_bounded_by_the_injected_affinity) {
  memory_pool pool(1 << 20);
  const node_spec node = test_node("workers");
  std::atomic<int> tasks{0};
  job_spec spec{node, 4, [&](task_context&) {
                  ++tasks;
                  return digest_for(0);
                }};
  cancel_token cancel;
  job_scheduler two_cpus(pool, affinity_of(2));
  EXPECT_STOP(two_cpus.run(plan_of(0), spec, cancel), error_category::invalid_input, "sched.worker_count");
  EXPECT_STOP(two_cpus.run(plan_of(3), spec, cancel), error_category::invalid_input, "sched.worker_count");
  EXPECT_EQ(tasks.load(), 0);
  EXPECT_NO_THROW(two_cpus.run(plan_of(1), spec, cancel));
  EXPECT_NO_THROW(two_cpus.run(plan_of(2), spec, cancel));
  EXPECT_EQ(tasks.load(), 8);
  job_scheduler real(pool);
  const std::size_t cpus = cpu_count();
  EXPECT_GE(cpus, 1u);
  EXPECT_STOP(real.run(plan_of(static_cast<unsigned>(cpus) + 1), spec, cancel), error_category::invalid_input,
              "sched.worker_count");
}

TEST(Scheduler, T_M2D_15_a_failed_or_empty_affinity_is_a_stop) {
  memory_pool pool(1 << 20);
  const node_spec node = test_node("affinity");
  std::atomic<int> tasks{0};
  job_spec spec{node, 4, [&](task_context&) {
                  ++tasks;
                  return digest_for(0);
                }};
  cancel_token cancel;
  job_scheduler failing(pool, [](cpu_set_t*) { return EPERM; });
  EXPECT_STOP(failing.run(plan_of(1), spec, cancel), error_category::io, "sched.cpu_count");
  job_scheduler empty(pool, [](cpu_set_t* set) {
    CPU_ZERO(set);
    return 0;
  });
  EXPECT_STOP(empty.run(plan_of(1), spec, cancel), error_category::io, "sched.cpu_count");
  EXPECT_STOP(cpu_count([](cpu_set_t*) { return EINVAL; }), error_category::io, "sched.cpu_count");
  EXPECT_EQ(tasks.load(), 0);
  EXPECT_EQ(pool.peak_reserved_bytes(), 0u);
}

TEST(Scheduler, the_plan_is_validated_before_anything_runs) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("plan");
  std::atomic<int> tasks{0};
  job_spec spec{node, 4, [&](task_context&) {
                  ++tasks;
                  return digest_for(0);
                }};
  cancel_token cancel;
  job_plan p = plan_of(1);
  p.queue_capacity = 0;
  EXPECT_STOP(sched.run(p, spec, cancel), error_category::invalid_input, "sched.queue_capacity");
  p = plan_of(1);
  p.tile_rows = 0;
  EXPECT_STOP(sched.run(p, spec, cancel), error_category::invalid_input, "sched.tile_rows");
  p = plan_of(1);
  p.checkpoint_every = 1;
  EXPECT_STOP(sched.run(p, spec, cancel), error_category::invalid_input, "sched.checkpoint_dir");
  job_spec resume = spec;
  resume.resume = true;
  resume.checkpoint_dir = fresh_dir("plan_ck");
  resume.checkpoint_name = "job";
  EXPECT_STOP(sched.run(plan_of(1), resume, cancel), error_category::invalid_input, "sched.resume_verifier");
  EXPECT_EQ(tasks.load(), 0);
}

// ---------------------------------------------------------------- T-M2D-07 / T-M2B-12

TEST(Scheduler, T_M2D_07_injected_stop_in_a_worker_publishes_nothing_and_releases_everything) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  single_slot_store rig;
  std::atomic<int> executed{0};
  std::atomic<bool> finalized{false};
  const std::optional<budget_snapshot> snapshot = [] {
    budget_snapshot s;
    s.budget_bytes = 1 << 20;
    return s;
  }();
  job_spec spec{rig.node, 1000, [&](task_context& ctx) {
                  ++executed;
                  if (ctx.index == 3) throw injected();
                  return digest_for(ctx.index);
                }};
  spec.finalize = [&](job_context&) { finalized = true; };
  cancel_token cancel;
  const job_outcome out = run_quiet(
      rig.store, "s",
      [&](const job_record& job) {
        sched.run(plan_of(4, 4096, 1024), spec, cancel);
        return result_for(job);
      },
      snapshot);
  EXPECT_EQ(out.exit_code, 2);
  ASSERT_TRUE(out.stop);
  EXPECT_EQ(out.stop->code(), "test.injected");
  EXPECT_EQ(out.job.state, job_state::failed);
  EXPECT_TRUE(no_publication(rig.store, rig.node));
  EXPECT_FALSE(finalized.load());
  EXPECT_LT(executed.load(), 1000) << "gli altri worker devono fermarsi al primo errore";
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_GT(pool.peak_reserved_bytes(), 0u);
  ASSERT_TRUE(out.provenance.budget);
  EXPECT_NE(provenance_json(out.provenance).find("budget"), nullptr);
}

TEST(Scheduler, T_M2D_07_a_foreign_exception_marks_the_job_failed_and_releases_everything) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  single_slot_store rig;
  job_spec spec{rig.node, 50, [&](task_context& ctx) -> std::string {
                  if (ctx.index == 7) throw std::runtime_error("eccezione iniettata");
                  return digest_for(ctx.index);
                }};
  cancel_token cancel;
  EXPECT_THROW(run_quiet(rig.store, "s",
                         [&](const job_record& job) {
                           sched.run(plan_of(2), spec, cancel);
                           return result_for(job);
                         }),
               std::runtime_error);
  EXPECT_EQ(rig.store.get_job(1)->state, job_state::failed);
  EXPECT_EQ(rig.store.get_job(1)->stop_code, "job.unexpected_exception");
  EXPECT_TRUE(no_publication(rig.store, rig.node));
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Scheduler, T_M2B_12_reservations_are_released_on_success_stop_and_cancel) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const auto run_case = [&](int kind) {
    single_slot_store rig;
    cancel_token cancel;
    job_spec spec{rig.node, 20, [&](task_context& ctx) {
                    pool_buffer extra(ctx.memory, 100);
                    if (ctx.index == 5 && kind == 1) throw injected();
                    if (ctx.index == 5 && kind == 2) {
                      cancel.request();
                      ctx.check_cancel();
                    }
                    return digest_for(ctx.index);
                  }};
    return run_quiet(rig.store, "s", [&](const job_record& job) {
      reservation held_by_work = pool.reserve(4096, "work");  // prenotazione propria del lavoro
      sched.run(plan_of(3, 2048), spec, cancel);
      return result_for(job);
    });
  };
  const job_outcome ok = run_case(0);
  EXPECT_EQ(ok.exit_code, 0);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  const job_outcome stopped = run_case(1);
  EXPECT_EQ(stopped.exit_code, 2);
  EXPECT_EQ(stopped.job.state, job_state::failed);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  const job_outcome cancelled = run_case(2);
  EXPECT_EQ(cancelled.exit_code, 3);
  EXPECT_EQ(cancelled.job.state, job_state::cancelled);
  ASSERT_TRUE(cancelled.stop);
  EXPECT_EQ(cancelled.stop->category(), error_category::cancelled);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_GE(pool.peak_reserved_bytes(), 4096u + 2048u + 3 * 1024u);
}

// ---------------------------------------------------------------- T-M2D-08

TEST(Scheduler, T_M2D_08_cancel_midway_leaves_no_output_and_no_temporaries) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  single_slot_store rig;
  const fs::path scratch = fresh_dir("cancel_scratch");
  const fs::path ckpt = fresh_dir("cancel_ckpt");
  cancel_token cancel;
  spill_file* shared_file = nullptr;
  std::atomic<int> started{0};
  std::atomic<bool> finalized{false};
  job_spec spec{rig.node, 200, [&](task_context& ctx) {
                  const std::vector<std::byte> block(16 * 1024, std::byte{0x5a});
                  ++started;
                  for (std::uint64_t part = 0; part < 4; ++part) {
                    ctx.check_cancel();
                    (void)ctx.spill();
                    shared_file->write((ctx.index * 4 + part) * block.size(), block);
                    if (ctx.index == 20 && part == 1) cancel.request();
                  }
                  return digest_for(ctx.index);
                }};
  spec.prepare = [&](job_context& jc) { shared_file = &jc.spill->create_shared_file(); };
  spec.finalize = [&](job_context&) { finalized = true; };
  spec.checkpoint_dir = ckpt;
  spec.checkpoint_name = "cancel";
  job_plan plan = plan_of(3, 0, 1024);
  plan.spill = spill_config{scratch, 64 * MiB};
  plan.checkpoint_every = 5;
  const job_outcome out = run_quiet(rig.store, "s", [&](const job_record& job) {
    sched.run(plan, spec, cancel);
    return result_for(job);
  });
  EXPECT_EQ(out.exit_code, 3);
  EXPECT_EQ(exit_code_for(out.stop->category()), 3);
  EXPECT_EQ(out.job.state, job_state::cancelled);
  EXPECT_EQ(out.stop->category(), error_category::cancelled);
  EXPECT_EQ(out.stop->code(), "sched.cancelled");
  EXPECT_TRUE(no_publication(rig.store, rig.node));
  EXPECT_FALSE(finalized.load());
  EXPECT_EQ(entry_count(scratch), 0u);
  for (const auto& e : fs::directory_iterator(ckpt)) {
    EXPECT_EQ(e.path().filename().string(), "cancel.ckpt.json") << "temporaneo rimasto";
  }
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_LT(started.load(), 200) << "dopo l'annullamento non partono tutti i task";
}

TEST(Scheduler, T_M2D_08_cancel_requested_before_the_run_starts_no_task) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("precancel");
  std::atomic<int> tasks{0};
  job_spec spec{node, 10, [&](task_context&) {
                  ++tasks;
                  return digest_for(0);
                }};
  cancel_token cancel;
  cancel.request();
  EXPECT_STOP(sched.run(plan_of(2), spec, cancel), error_category::cancelled, "sched.cancelled");
  EXPECT_EQ(tasks.load(), 0);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  job_spec empty{node, 0, spec.run_task};
  EXPECT_STOP(sched.run(plan_of(2), empty, cancel), error_category::cancelled, "sched.cancelled");
}

TEST(Scheduler, T_M2D_08_cancel_while_waiting_for_the_pool) {
  memory_pool pool(8 * MiB);
  job_scheduler sched(pool, affinity_of(test_cpus));
  reservation other_job = pool.reserve(8 * MiB, "other");
  const node_spec node = test_node("waiting");
  std::atomic<int> tasks{0};
  job_spec spec{node, 3, [&](task_context&) {
                  ++tasks;
                  return digest_for(0);
                }};
  cancel_token cancel;
  std::optional<stop_error> result;
  std::thread waiter([&] {
    try {
      sched.run(plan_of(1, 0, 4 * MiB), spec, cancel);
    } catch (const stop_error& e) {
      result = e;
    }
  });
  ASSERT_TRUE(wait_until([&] { return pool.waiting_count() == 1; }));
  cancel.request();
  waiter.join();
  ASSERT_TRUE(result);
  EXPECT_EQ(result->category(), error_category::cancelled);
  EXPECT_EQ(tasks.load(), 0);
  EXPECT_EQ(pool.waiting_count(), 0u);
  other_job.release();
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Scheduler, a_failure_stops_cooperative_tasks_that_are_still_running) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("cooperative");
  gate first_running;
  std::atomic<bool> timed_out{false};
  std::atomic<bool> saw_stop{false};
  job_spec spec{node, 2, [&](task_context& ctx) -> std::string {
                  if (ctx.index == 1) {
                    first_running.wait();
                    throw injected();
                  }
                  first_running.open();
                  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                  while (std::chrono::steady_clock::now() < deadline) {
                    try {
                      ctx.check_cancel();
                    } catch (const stop_error&) {
                      saw_stop = true;
                      throw;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  }
                  timed_out = true;
                  return digest_for(0);
                }};
  cancel_token cancel;
  EXPECT_STOP(sched.run(plan_of(2), spec, cancel), error_category::io, "test.injected");
  EXPECT_FALSE(timed_out.load()) << "il task attivo non ha visto lo stop interno";
  EXPECT_TRUE(saw_stop.load());
  EXPECT_FALSE(cancel.requested()) << "lo stop interno non e' il cancel dell'utente";
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Scheduler, cancel_during_finalize_is_not_a_success_and_keeps_the_checkpoint) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("finalize_cancel");
  const fs::path dir = fresh_dir("finalize_cancel");
  cancel_token cancel;
  job_spec spec{node, 4, [&](task_context& ctx) { return digest_for(ctx.index); }};
  spec.finalize = [&](job_context&) { cancel.request(); };
  spec.checkpoint_dir = dir;
  spec.checkpoint_name = "job";
  job_plan plan = plan_of(1);
  plan.checkpoint_every = 1;
  EXPECT_STOP(sched.run(plan, spec, cancel), error_category::cancelled, "sched.cancelled");
  EXPECT_TRUE(fs::exists(dir / "job.ckpt.json"));
}

TEST(Scheduler, cancel_at_the_publication_boundary_publishes_nothing) {
  single_slot_store rig;
  cancel_token cancel;
  testing::internal::CaptureStderr();
  const job_outcome out = run_job(
      rig.store, "s",
      [&](const job_record& job) {
        cancel.request();
        return result_for(job);
      },
      std::nullopt, &cancel);
  testing::internal::GetCapturedStderr();
  EXPECT_EQ(out.exit_code, 3);
  EXPECT_EQ(out.job.state, job_state::cancelled);
  EXPECT_TRUE(no_publication(rig.store, rig.node));
}

// ---------------------------------------------------------------- T-M2D-10 attraverso lo scheduler

TEST(Scheduler, T_M2D_10_unusable_scratch_stops_before_any_task_and_undeclared_spill_is_refused) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("spill");
  std::atomic<int> tasks{0};
  job_spec spec{node, 3, [&](task_context& ctx) {
                  ++tasks;
                  ctx.spill();
                  return digest_for(0);
                }};
  cancel_token cancel;
  job_plan plan = plan_of(1);
  plan.spill = spill_config{fresh_dir("sched_spill") / "absent", 1 << 20};
  EXPECT_STOP(sched.run(plan, spec, cancel), error_category::io, "spill.scratch_unusable");
  EXPECT_EQ(tasks.load(), 0);
  EXPECT_EQ(pool.peak_reserved_bytes(), 0u);
  EXPECT_STOP(sched.run(plan_of(1), spec, cancel), error_category::invalid_input, "spill.not_declared");
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Scheduler, T_M2D_10_spill_quota_exceeded_in_a_task_fails_the_job_with_exit_2) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  single_slot_store rig;
  const fs::path scratch = fresh_dir("sched_quota");
  spill_file* file = nullptr;
  job_spec spec{rig.node, 16, [&](task_context& ctx) {
                  const std::vector<std::byte> block(MiB, std::byte{1});
                  file->write(ctx.index * MiB, block);
                  return digest_for(ctx.index);
                }};
  spec.prepare = [&](job_context& jc) { file = &jc.spill->create_shared_file(); };
  job_plan plan = plan_of(2);
  plan.spill = spill_config{scratch, 4 * MiB};
  cancel_token cancel;
  const job_outcome out = run_quiet(rig.store, "s", [&](const job_record& job) {
    sched.run(plan, spec, cancel);
    return result_for(job);
  });
  EXPECT_EQ(out.exit_code, 2);
  EXPECT_EQ(out.stop->category(), error_category::budget);
  EXPECT_EQ(out.stop->code(), "spill.quota_exceeded");
  EXPECT_TRUE(no_publication(rig.store, rig.node));
  EXPECT_EQ(entry_count(scratch), 0u);
}

// ---------------------------------------------------------------- T-M2D-11

TEST(Scheduler, T_M2D_11_backpressure_blocks_the_producer_and_loses_no_task) {
  for (const unsigned workers : {1u, 4u}) {
    memory_pool pool(1 << 20);
    job_scheduler sched(pool, affinity_of(test_cpus));
    const node_spec node = test_node("backpressure");
    constexpr std::uint64_t tasks = 200;
    std::vector<std::atomic<int>> runs(tasks);
    gate release;
    job_spec spec{node, tasks, [&](task_context& ctx) {
                    if (ctx.index < workers) release.wait();  // i primi task trattengono tutti i worker
                    ++runs[ctx.index];
                    return digest_for(ctx.index);
                  }};
    job_plan plan = plan_of(workers);
    plan.queue_capacity = 2;
    std::thread opener([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      release.open();
    });
    cancel_token cancel;
    const job_report report = sched.run(plan, spec, cancel);
    opener.join();
    for (std::uint64_t i = 0; i < tasks; ++i) EXPECT_EQ(runs[i].load(), 1) << "task " << i;
    EXPECT_GT(report.producer_waits, 0u);
    EXPECT_LE(report.max_queue_depth, 2u);
    EXPECT_EQ(report.max_queue_depth, 2u);
    EXPECT_EQ(report.tasks_run, tasks);
  }
}

// ---------------------------------------------------------------- T-M2D-14

TEST(Scheduler, T_M2D_14_full_job_queue_is_a_stop_and_jobs_run_in_fifo_order) {
  memory_pool pool(8 * MiB);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("queue");
  gate hold;
  std::atomic<int> started_holder{0};
  std::mutex order_mutex;
  std::vector<int> order;
  std::atomic<int> rejected_tasks{0};

  const auto make_job = [&](int id) {
    return job_spec{node, 1, [&, id](task_context&) {
                      if (id == 0) {
                        ++started_holder;
                        hold.wait();
                      }
                      std::lock_guard lock(order_mutex);
                      order.push_back(id);
                      return digest_for(0);
                    }};
  };
  const job_plan whole_pool = plan_of(1, 0, 8 * MiB);
  cancel_token cancel;
  std::vector<std::thread> threads;
  threads.emplace_back([&] { sched.run(whole_pool, make_job(0), cancel); });
  ASSERT_TRUE(wait_until([&] { return started_holder.load() == 1; }));
  for (int id = 1; id <= static_cast<int>(job_queue_capacity); ++id) {
    threads.emplace_back([&, id] { sched.run(whole_pool, make_job(id), cancel); });
    ASSERT_TRUE(wait_until([&] { return pool.waiting_count() == static_cast<std::size_t>(id); }));
  }
  EXPECT_EQ(pool.waiting_count(), job_queue_capacity);
  job_spec rejected{node, 1, [&](task_context&) {
                      ++rejected_tasks;
                      return digest_for(0);
                    }};
  EXPECT_STOP(sched.run(whole_pool, rejected, cancel), error_category::budget, "sched.queue_full");
  EXPECT_EQ(rejected_tasks.load(), 0);
  EXPECT_EQ(pool.waiting_count(), job_queue_capacity);
  hold.open();
  for (std::thread& t : threads) t.join();
  ASSERT_EQ(order.size(), job_queue_capacity + 1);
  for (std::size_t i = 0; i < order.size(); ++i) EXPECT_EQ(order[i], static_cast<int>(i));
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_EQ(pool.waiting_count(), 0u);
}

TEST(Scheduler, two_jobs_each_admitted_alone_but_not_together_wait_instead_of_shrinking) {
  memory_pool pool(8 * MiB);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("together");
  gate hold;
  std::atomic<int> first_started{0};
  std::atomic<int> concurrent{0}, max_concurrent{0};
  const auto body = [&](task_context&) {
    const int now = ++concurrent;
    int peak = max_concurrent.load();
    while (peak < now && !max_concurrent.compare_exchange_weak(peak, now)) {
    }
    ++first_started;
    hold.wait();
    --concurrent;
    return digest_for(0);
  };
  job_spec a{node, 1, body};
  job_spec b{node, 1, body};
  cancel_token cancel;
  std::thread ta([&] { sched.run(plan_of(1, 0, 5 * MiB), a, cancel); });
  ASSERT_TRUE(wait_until([&] { return first_started.load() == 1; }));
  std::thread tb([&] { sched.run(plan_of(1, 0, 5 * MiB), b, cancel); });
  ASSERT_TRUE(wait_until([&] { return pool.waiting_count() == 1; }));
  EXPECT_EQ(first_started.load(), 1);
  hold.open();
  ta.join();
  tb.join();
  EXPECT_EQ(max_concurrent.load(), 1);
  EXPECT_EQ(first_started.load(), 2);
}

// ---------------------------------------------------------------- T-M2D-09

class Resume : public ::testing::Test {
 protected:
  Resume() : dir(fresh_dir(std::string("resume_") + ::testing::UnitTest::GetInstance()->current_test_info()->name())) {}

  job_spec make_spec(const node_spec& node, std::uint64_t tasks, std::int64_t fail_at, bool resume) {
    job_spec spec{node, tasks, [this, fail_at](task_context& ctx) -> std::string {
                    {
                      std::lock_guard lock(m);
                      executed.push_back(ctx.index);
                    }
                    if (static_cast<std::int64_t>(ctx.index) == fail_at) throw injected();
                    const std::string content = "partial-" + std::to_string(ctx.index);
                    std::lock_guard lock(m);
                    partial[ctx.index] = content;
                    return digest_text(content);
                  }};
    spec.verify_partial = [this](std::uint64_t index) {
      std::lock_guard lock(m);
      return digest_text(partial.at(index));
    };
    spec.checkpoint_dir = dir;
    spec.checkpoint_name = "job";
    spec.resume = resume;
    return spec;
  }

  job_plan checkpointed(unsigned workers) {
    job_plan p = plan_of(workers);
    p.checkpoint_every = 2;
    return p;
  }

  std::string checkpoint_bytes() const { return read_text(dir / "job.ckpt.json"); }

  fs::path dir;
  std::mutex m;
  std::vector<std::uint64_t> executed;
  std::map<std::uint64_t, std::string> partial;
};

TEST_F(Resume, T_M2D_09_identical_resume_continues_from_the_checkpoint) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("resume");
  cancel_token cancel;
  EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, 5, false), cancel), error_category::io, "test.injected");
  const std::optional<checkpoint_record> saved = read_checkpoint(dir, "job");
  ASSERT_TRUE(saved);
  EXPECT_EQ(saved->completed, (std::vector<std::uint64_t>{0, 1, 2, 3}));
  EXPECT_EQ(saved->node_key, node.key());
  EXPECT_EQ(saved->backend, "cpu_scalar");
  EXPECT_EQ(saved->task_count, 10u);
  EXPECT_EQ(saved->recipe_digest, recipe_digest(node, checkpointed(1)));
  EXPECT_EQ(saved->partial_digests[2], digest_text("partial-2"));

  executed.clear();
  const job_report report = sched.run(checkpointed(1), make_spec(node, 10, -1, true), cancel);
  EXPECT_EQ(executed, (std::vector<std::uint64_t>{4, 5, 6, 7, 8, 9}));
  EXPECT_EQ(report.tasks_resumed, 4u);
  EXPECT_EQ(report.tasks_run, 6u);
  for (std::uint64_t i = 0; i < 10; ++i) EXPECT_EQ(report.partial_digests[i], digest_text("partial-" + std::to_string(i)));
  EXPECT_TRUE(fs::exists(dir / "job.ckpt.json")) << "lo scheduler non rimuove il checkpoint: lo fa il chiamante";
  remove_checkpoint(dir, "job");
  EXPECT_EQ(entry_count(dir), 0u);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST_F(Resume, T_M2D_09_resume_with_parallel_workers_never_repeats_checkpointed_tasks) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("resume_par");
  cancel_token cancel;
  EXPECT_STOP(sched.run(checkpointed(4), make_spec(node, 60, 40, false), cancel), error_category::io, "test.injected");
  const std::optional<checkpoint_record> saved = read_checkpoint(dir, "job");
  ASSERT_TRUE(saved);
  ASSERT_FALSE(saved->completed.empty());
  executed.clear();
  const job_report report = sched.run(checkpointed(4), make_spec(node, 60, -1, true), cancel);
  std::set<std::uint64_t> first(saved->completed.begin(), saved->completed.end());
  std::set<std::uint64_t> second(executed.begin(), executed.end());
  EXPECT_EQ(executed.size(), second.size()) << "un task eseguito due volte nel resume";
  for (const std::uint64_t i : first) EXPECT_EQ(second.count(i), 0u) << i;
  EXPECT_EQ(first.size() + second.size(), 60u);
  EXPECT_EQ(report.tasks_resumed + report.tasks_run, 60u);
}

TEST_F(Resume, T_M2D_09_every_recipe_or_backend_difference_is_resume_mismatch_and_changes_nothing) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("recipe");
  cancel_token cancel;
  EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, 5, false), cancel), error_category::io, "test.injected");
  const std::string before = checkpoint_bytes();
  executed.clear();

  const auto expect_mismatch = [&](const node_spec& n, const job_plan& p, std::uint64_t tasks, const char* what) {
    SCOPED_TRACE(what);
    EXPECT_STOP(sched.run(p, make_spec(n, tasks, -1, true), cancel), error_category::resume_mismatch,
                "sched.resume_recipe");
    EXPECT_TRUE(executed.empty());
    EXPECT_EQ(checkpoint_bytes(), before);
  };
  job_plan p = checkpointed(1);
  p.tile_rows = 17;
  expect_mismatch(node, p, 10, "tile_rows");
  p = checkpointed(2);
  expect_mismatch(node, p, 10, "worker_count");
  p = checkpointed(1);
  p.checkpoint_every = 3;
  expect_mismatch(node, p, 10, "checkpoint_every");
  p = checkpointed(1);
  p.spill = spill_config{fresh_dir("recipe_spill"), 1 << 20};
  expect_mismatch(node, p, 10, "spill");
  p = checkpointed(1);
  p.spill = spill_config{fresh_dir("recipe_spill2"), 1 << 20};
  EXPECT_NE(recipe_digest(node, p), recipe_digest(node, checkpointed(1)));
  expect_mismatch(test_node("other_params"), checkpointed(1), 10, "parametri del nodo");
  expect_mismatch(test_node("recipe", "cpu_simd"), checkpointed(1), 10, "backend nel nodo");
  expect_mismatch(node, checkpointed(1), 11, "numero di task");

  checkpoint_record tampered = *read_checkpoint(dir, "job");
  tampered.backend = "cpu_simd";
  write_checkpoint(dir, "job", tampered);
  const std::string tampered_bytes = checkpoint_bytes();
  EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, -1, true), cancel), error_category::resume_mismatch,
              "sched.resume_recipe");
  EXPECT_EQ(checkpoint_bytes(), tampered_bytes);
  EXPECT_TRUE(executed.empty());
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST_F(Resume, T_M2D_09_a_partial_with_a_different_digest_is_an_integrity_stop) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("digest");
  cancel_token cancel;
  EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, 5, false), cancel), error_category::io, "test.injected");
  const std::string before = checkpoint_bytes();
  executed.clear();
  partial[1] = "corrotto";
  EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, -1, true), cancel), error_category::integrity,
              "sched.checkpoint_digest");
  EXPECT_TRUE(executed.empty());
  EXPECT_EQ(checkpoint_bytes(), before);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST_F(Resume, T_M2D_09_a_rejected_resume_does_not_restart_from_zero_and_a_new_run_is_a_new_job) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("new_job");
  cancel_token cancel;
  EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, -1, true), cancel), error_category::metadata_missing,
              "sched.checkpoint_missing");
  EXPECT_TRUE(executed.empty());

  EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, 5, false), cancel), error_category::io, "test.injected");
  executed.clear();
  const job_report fresh = sched.run(checkpointed(1), make_spec(node, 10, -1, false), cancel);
  EXPECT_EQ(executed.size(), 10u);
  EXPECT_EQ(fresh.tasks_resumed, 0u);
  EXPECT_TRUE(fs::exists(dir / "job.ckpt.json"));
}

TEST_F(Resume, T_M2D_09_malformed_checkpoints_are_integrity_stops) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  const node_spec node = test_node("malformed");
  cancel_token cancel;
  const std::string good_digest(64, 'a');
  for (const std::string& bad :
       {std::string("not json"), std::string("[]"), std::string("{}"),
        std::string("{\"format\":\"other\"}"),
        "{\"backend\":\"b\",\"completed\":[3,1],\"format\":\"astro.checkpoint.v1\",\"node_key\":\"" + good_digest +
            "\",\"partial_digests\":[\"" + good_digest + "\",\"" + good_digest + "\"],\"recipe_digest\":\"" +
            good_digest + "\",\"task_count\":10}",
        "{\"backend\":\"b\",\"completed\":[99],\"format\":\"astro.checkpoint.v1\",\"node_key\":\"" + good_digest +
            "\",\"partial_digests\":[\"" + good_digest + "\"],\"recipe_digest\":\"" + good_digest +
            "\",\"task_count\":10}"}) {
    SCOPED_TRACE(bad);
    write_text(dir / "job.ckpt.json", bad);
    EXPECT_STOP(sched.run(checkpointed(1), make_spec(node, 10, -1, true), cancel), error_category::integrity,
                "sched.checkpoint_malformed");
  }
  EXPECT_TRUE(executed.empty());
}

TEST_F(Resume, checkpoint_directory_and_names_are_validated) {
  const checkpoint_record record{std::string(64, 'a'), std::string(64, 'b'), "cpu", 1, {}, {}};
  EXPECT_STOP(write_checkpoint(dir / "absent", "job", record), error_category::io, "sched.checkpoint_dir");
  fs::create_directory_symlink(dir, dir.parent_path() / "resume_link");
  EXPECT_STOP(write_checkpoint(dir.parent_path() / "resume_link", "job", record), error_category::io,
              "sched.checkpoint_dir");
  fs::remove(dir.parent_path() / "resume_link");
  for (const char* name : {"", "../x", "a/b", "a.b", ".hidden", "has space"}) {
    SCOPED_TRACE(name);
    EXPECT_STOP(write_checkpoint(dir, name, record), error_category::invalid_input, "sched.checkpoint_name");
  }
  write_checkpoint(dir, "job", record);
  EXPECT_EQ(read_checkpoint(dir, "job")->backend, "cpu");
  EXPECT_FALSE(read_checkpoint(dir, "other"));
  checkpoint_record changed = record;
  changed.backend = "gpu";
  write_checkpoint(dir, "job", changed);
  EXPECT_EQ(read_checkpoint(dir, "job")->backend, "gpu");
  for (const auto& e : fs::directory_iterator(dir)) EXPECT_EQ(e.path().filename().string(), "job.ckpt.json");
  remove_checkpoint(dir, "job");
  remove_checkpoint(dir, "job");
  EXPECT_EQ(entry_count(dir), 0u);
}

TEST_F(Resume, the_checkpoint_survives_a_failed_publication_and_a_later_resume_needs_no_task) {
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  single_slot_store rig;
  const node_spec other = test_node("retarget");
  rig.store.put_node(other);
  cancel_token cancel;
  job_spec spec = make_spec(rig.node, 10, -1, false);
  // La pubblicazione vera (complete_job) fallisce: lo slot cambia generazione mentre il lavoro gira.
  const job_outcome out = run_quiet(rig.store, "s", [&](const job_record& job) {
    sched.run(checkpointed(1), spec, cancel);
    rig.store.retarget_slot("s", other.key());
    return result_for(job);
  });
  EXPECT_EQ(out.exit_code, 2);
  ASSERT_TRUE(out.stop);
  EXPECT_EQ(out.stop->code(), "dag.stale_publish");
  ASSERT_TRUE(fs::exists(dir / "job.ckpt.json"));
  executed.clear();
  const job_report report = sched.run(checkpointed(1), make_spec(rig.node, 10, -1, true), cancel);
  EXPECT_TRUE(executed.empty());
  EXPECT_EQ(report.tasks_resumed, 10u);
}

TEST_F(Resume, a_checkpoint_over_the_limit_is_refused_by_the_reader_with_the_size_code) {
  write_text(dir / "job.ckpt.json", std::string((std::size_t{64} << 20) + 1, 'x'));
  EXPECT_STOP(read_checkpoint(dir, "job"), error_category::limit_exceeded, "sched.checkpoint_size");
}

TEST_F(Resume, a_checkpoint_the_reader_would_refuse_is_not_written_and_the_previous_one_stays) {
  checkpoint_record small{std::string(64, 'a'), std::string(64, 'b'), "cpu", 1, {}, {}};
  write_checkpoint(dir, "job", small);
  const std::string before = checkpoint_bytes();
  checkpoint_record huge = small;
  huge.backend = std::string(std::size_t{65} << 20, 'x');
  EXPECT_STOP(write_checkpoint(dir, "job", huge), error_category::limit_exceeded, "sched.checkpoint_size");
  EXPECT_EQ(checkpoint_bytes(), before);
  EXPECT_EQ(read_checkpoint(dir, "job")->backend, "cpu");
  EXPECT_EQ(entry_count(dir), 1u);
}

// ---------------------------------------------------------------- T-M2B-09

struct branch_rig {
  memory_dag_store store;
  std::map<std::string, int> calls;

  branch_rig() {
    const auto make = [](const char* op, recipe_input input) {
      return recipe_entry(op, 1, 1, json::object(), {std::move(input)}, sample_type::f32, "cpu_scalar", std::nullopt);
    };
    const auto child = [&](const char* parent) { return make("calib.child", recipe_input{"in", std::string(parent)}); };
    recipe_graph g;
    g.add("r1", make("fits.decode", recipe_input{"file", content_digest{std::string(64, '1'), 10}}));
    g.add("r2", make("fits.decode", recipe_input{"file", content_digest{std::string(64, '2'), 10}}));
    g.add("c1", child("r1"));
    g.add("g1", child("c1"));
    g.add("c2", child("r2"));
    store.apply_recipe(g.resolve(), {{"r1", "r1"}, {"r2", "r2"}, {"c1", "c1"}, {"g1", "g1"}, {"c2", "c2"}});
  }

  branch_job job(const std::string& slot, std::function<void()> action = {}) {
    return branch_job{slot, [this, slot, action](const job_record& j) {
                        ++calls[slot];
                        if (action) action();
                        return result_for(j);
                      }};
  }
};

TEST(Branches, T_M2B_09_a_failure_blocks_only_its_dependents_and_independent_branches_proceed) {
  branch_rig rig;
  testing::internal::CaptureStderr();
  const std::vector<branch_result> results = run_branches(
      rig.store, {rig.job("g1"), rig.job("c2"), rig.job("c1"), rig.job("r2"), rig.job("r1", [] { throw injected(); })});
  testing::internal::GetCapturedStderr();

  std::map<std::string, branch_result> by_slot;
  for (const branch_result& r : results) by_slot[r.slot_name] = r;
  EXPECT_EQ(by_slot["r1"].state, branch_state::failed);
  ASSERT_TRUE(by_slot["r1"].outcome);
  EXPECT_EQ(by_slot["r1"].outcome->exit_code, 2);
  EXPECT_EQ(by_slot["c1"].state, branch_state::blocked);
  EXPECT_EQ(by_slot["c1"].blocked_by_slot, "r1");
  EXPECT_EQ(by_slot["g1"].state, branch_state::blocked);
  EXPECT_EQ(by_slot["g1"].blocked_by_slot, "r1");
  EXPECT_FALSE(by_slot["g1"].outcome);
  EXPECT_EQ(by_slot["r2"].state, branch_state::completed);
  EXPECT_EQ(by_slot["c2"].state, branch_state::completed);
  EXPECT_EQ(rig.calls["c1"], 0);
  EXPECT_EQ(rig.calls["g1"], 0);
  EXPECT_EQ(rig.calls["r1"], 1);
  EXPECT_EQ(rig.calls["r2"], 1);
  EXPECT_EQ(rig.calls["c2"], 1);
  EXPECT_EQ(rig.store.get_slot("c1")->current_generation, 0u) << "un job bloccato non parte";
  EXPECT_EQ(rig.store.read_slot("r2").state, freshness::current);
  EXPECT_EQ(rig.store.read_slot("c1").state, freshness::missing);
  EXPECT_EQ(branches_exit_code(results), 2);
}

TEST(Branches, parents_run_before_their_descendants_whatever_the_order_given) {
  branch_rig rig;
  std::vector<std::string> order;
  const auto record = [&](const std::string& slot) { return rig.job(slot, [&order, slot] { order.push_back(slot); }); };
  const std::vector<branch_result> results =
      run_branches(rig.store, {record("g1"), record("c2"), record("c1"), record("r2"), record("r1")});
  const auto pos = [&](const std::string& s) { return std::find(order.begin(), order.end(), s) - order.begin(); };
  EXPECT_LT(pos("r1"), pos("c1"));
  EXPECT_LT(pos("c1"), pos("g1"));
  EXPECT_LT(pos("r2"), pos("c2"));
  for (const branch_result& r : results) EXPECT_EQ(r.state, branch_state::completed);
  EXPECT_EQ(branches_exit_code(results), 0);
}

TEST(Branches, a_cancel_stops_every_branch_not_yet_started) {
  branch_rig rig;
  testing::internal::CaptureStderr();
  const std::vector<branch_result> results = run_branches(
      rig.store, {rig.job("r1", [] {
                    throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento richiesto");
                  }),
                  rig.job("r2"), rig.job("c1")});
  testing::internal::GetCapturedStderr();
  std::map<std::string, branch_state> state;
  for (const branch_result& r : results) state[r.slot_name] = r.state;
  EXPECT_EQ(state["r1"], branch_state::cancelled);
  EXPECT_EQ(state["r2"], branch_state::not_started);
  EXPECT_EQ(state["c1"], branch_state::not_started);
  EXPECT_EQ(rig.calls["r2"], 0);
  EXPECT_EQ(branches_exit_code(results), 3);
}

TEST(Branches, a_cancel_after_the_work_publishes_nothing_and_later_branches_do_not_start) {
  branch_rig rig;
  cancel_token cancel;
  testing::internal::CaptureStderr();
  const std::vector<branch_result> results =
      run_branches(rig.store, {rig.job("r1", [&] { cancel.request(); }), rig.job("r2"), rig.job("c1")}, std::nullopt,
                   &cancel);
  testing::internal::GetCapturedStderr();
  std::map<std::string, branch_state> state;
  for (const branch_result& r : results) state[r.slot_name] = r.state;
  EXPECT_EQ(state["r1"], branch_state::cancelled);
  EXPECT_EQ(state["r2"], branch_state::not_started);
  EXPECT_EQ(state["c1"], branch_state::not_started);
  EXPECT_EQ(rig.store.read_slot("r1").state, freshness::missing);
  EXPECT_EQ(rig.calls["r2"], 0);
  EXPECT_EQ(branches_exit_code(results), 3);
}

TEST(Branches, foreign_exceptions_propagate_and_unknown_slots_stop) {
  branch_rig rig;
  testing::internal::CaptureStderr();
  EXPECT_THROW(run_branches(rig.store, {rig.job("r1", [] { throw std::runtime_error("x"); })}), std::runtime_error);
  testing::internal::GetCapturedStderr();
  EXPECT_STOP(run_branches(rig.store, {rig.job("nope")}), error_category::invalid_input, "dag.unknown_slot");
}

TEST(Branches, scheduler_failures_inside_branches_block_dependents) {
  branch_rig rig;
  memory_pool pool(1 << 20);
  job_scheduler sched(pool, affinity_of(test_cpus));
  cancel_token cancel;
  const auto scheduled = [&](const std::string& slot, bool fail) {
    return branch_job{slot, [&, slot, fail](const job_record& j) {
                        ++rig.calls[slot];
                        const node_spec node = *rig.store.get_node(j.node_key);
                        job_spec spec{node, 12, [&](task_context& ctx) -> std::string {
                                        if (fail && ctx.index == 4) throw injected();
                                        return digest_for(ctx.index);
                                      }};
                        sched.run(plan_of(2), spec, cancel);
                        return result_for(j);
                      }};
  };
  testing::internal::CaptureStderr();
  const std::vector<branch_result> results =
      run_branches(rig.store, {scheduled("r1", false), scheduled("c1", true), scheduled("g1", false),
                               scheduled("r2", false), scheduled("c2", false)});
  testing::internal::GetCapturedStderr();
  std::map<std::string, branch_state> state;
  for (const branch_result& r : results) state[r.slot_name] = r.state;
  EXPECT_EQ(state["r1"], branch_state::completed);
  EXPECT_EQ(state["c1"], branch_state::failed);
  EXPECT_EQ(state["g1"], branch_state::blocked);
  EXPECT_EQ(state["r2"], branch_state::completed);
  EXPECT_EQ(state["c2"], branch_state::completed);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

}  // namespace
