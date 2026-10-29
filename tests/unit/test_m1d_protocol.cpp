// T-M2D-13: protocollo M1.d sul prodotto. Pool 32 MiB, tile da 2 MiB, 1/2/4 worker, 40 GiB logici (SYN-STREAM) mai
// materializzati, spill fisico da 256 MiB con quota. Lungo: ctest label `slow`.
#include <gtest/gtest.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#include "m2d_support.hpp"

namespace {

using namespace m2d;
constexpr std::uint64_t GiB = std::uint64_t{1} << 30;
constexpr std::uint64_t pool_bytes = 32 * MiB;
constexpr std::uint64_t tile_bytes = 2 * MiB;
constexpr std::uint64_t tile_elems = tile_bytes / sizeof(float);
constexpr std::uint64_t stream_tiles = 40 * GiB / tile_bytes;
constexpr std::uint64_t spill_tiles = 256 * MiB / tile_bytes;
constexpr std::uint64_t shared_reserve = 8 * MiB;
constexpr std::uint64_t task_reserve = 3 * tile_bytes;

// SYN-DYADIC per indice assoluto: k_j = ((73 j + 19) mod 1024) - 512, x_j = k_j / 1024.
std::int64_t k_of(std::uint64_t j) { return static_cast<std::int64_t>((73 * j + 19) % 1024) - 512; }

// Oracle indipendente dal prodotto: y = 3x/4 + 1/8 espresso in unita' 1/4096 vale 3k + 512 per elemento.
// 73 e' dispari, quindi su 1024 indici consecutivi k percorre una volta ogni valore -512..511 (somma -512).
std::int64_t gt_prefix(std::uint64_t n) {
  const std::uint64_t periods = n / 1024;
  const std::int64_t per_period = 3 * -512 + 512 * 1024;
  std::int64_t sum = static_cast<std::int64_t>(periods) * per_period;
  for (std::uint64_t j = 0; j < n % 1024; ++j) sum += 3 * k_of(j) + 512;
  return sum;
}

std::int64_t brute_prefix(std::uint64_t n) {
  std::int64_t sum = 0;
  for (std::uint64_t j = 0; j < n; ++j) sum += 3 * k_of(j) + 512;
  return sum;
}

std::int64_t gt_tile(std::uint64_t tile) { return gt_prefix((tile + 1) * tile_elems) - gt_prefix(tile * tile_elems); }

void generate(float* in, std::uint64_t first) {
  for (std::uint64_t t = 0; t < tile_elems; ++t) {
    const auto k = static_cast<std::int32_t>((73u * static_cast<std::uint32_t>(first + t) + 19u) & 1023u);
    in[t] = static_cast<float>(k - 512) * (1.0f / 1024.0f);
  }
}

void transform(const float* in, float* scratch, float* out) {
  for (std::uint64_t t = 0; t < tile_elems; ++t) scratch[t] = in[t] * 3.0f;
  for (std::uint64_t t = 0; t < tile_elems; ++t) out[t] = scratch[t] * 0.25f + 0.125f;
}

std::int64_t checksum(const float* out) {
  std::int64_t sum = 0;
  for (std::uint64_t t = 0; t < tile_elems; ++t) sum += static_cast<std::int32_t>(out[t] * 4096.0f);
  return sum;
}

// Una lettura di /proc mancante o non interpretabile e' un errore: mai uno zero che farebbe passare la guardia.
std::uint64_t proc_kib(const char* path, const char* key) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error(std::string("apertura di ") + path + " fallita");
  const std::string prefix = std::string(key) + ":";
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind(prefix, 0) != 0) continue;
    std::istringstream fields(line.substr(prefix.size()));
    std::uint64_t value = 0;
    std::string unit;
    if (!(fields >> value >> unit) || unit != "kB") {
      throw std::runtime_error(std::string("campo ") + key + " di " + path + " non interpretabile: " + line);
    }
    return value;
  }
  throw std::runtime_error(std::string("campo ") + key + " assente in " + path);
}

std::uint64_t kib(const char* key) { return proc_kib("/proc/self/status", key); }

std::uint64_t meminfo_kib(const char* key) { return proc_kib("/proc/meminfo", key); }

class rss_sampler {
 public:
  rss_sampler() : thread_([this] {
    try {
      while (!stop_) {
        peak_ = std::max<std::uint64_t>(peak_, kib("VmRSS"));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    } catch (const std::exception& e) {
      error_ = e.what();
      failed_ = true;
    }
  }) {}
  ~rss_sampler() {
    stop_ = true;
    thread_.join();
  }
  // Nessun campione valido: errore, non zero.
  std::uint64_t peak_kib() const {
    if (failed_) throw std::runtime_error("campionamento RSS fallito: " + error_);
    if (peak_ == 0) throw std::runtime_error("nessun campione RSS raccolto");
    return peak_;
  }

 private:
  std::atomic<bool> stop_{false};
  std::atomic<bool> failed_{false};
  std::string error_;
  std::atomic<std::uint64_t> peak_{kib("VmRSS")};
  std::thread thread_;
};

using fields = std::map<std::string, std::string>;

// Esegue il caso in un processo figlio: ogni misura di RSS parte da un processo pulito.
fields in_child(const std::function<fields()>& body) {
  int fds[2];
  EXPECT_EQ(::pipe(fds), 0);
  const pid_t pid = ::fork();
  if (pid == 0) {
    ::close(fds[0]);
    fields out;
    int code = 0;
    try {
      out = body();
    } catch (const std::exception& e) {
      out["child_error"] = e.what();
      code = 1;
    }
    std::string text;
    for (const auto& [k, v] : out) text += k + "=" + v + "\n";
    for (std::size_t done = 0; done < text.size();) {
      const ssize_t n = ::write(fds[1], text.data() + done, text.size() - done);
      if (n <= 0) ::_exit(3);
      done += static_cast<std::size_t>(n);
    }
    ::_exit(code);
  }
  ::close(fds[1]);
  std::string text;
  char buf[4096];
  while (const ssize_t n = ::read(fds[0], buf, sizeof(buf))) {
    if (n < 0) break;
    text.append(buf, static_cast<std::size_t>(n));
  }
  ::close(fds[0]);
  int status = 0;
  ::waitpid(pid, &status, 0);
  fields out;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const std::size_t eq = line.find('=');
    if (eq != std::string::npos) out[line.substr(0, eq)] = line.substr(eq + 1);
  }
  out["exit_status"] = std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status));
  return out;
}

void report(const std::string& title, const fields& f) {
  std::ostringstream os;
  os << "--- " << title << "\n";
  for (const auto& [k, v] : f) os << "  " << k << " = " << v << "\n";
  std::cout << os.str() << std::flush;
  const fs::path out = fs::path(ASTRO_PROTOCOL_OUT) / "protocol-report.txt";
  fs::create_directories(out.parent_path());
  std::ofstream(out, std::ios::app) << os.str();
}

struct stream_params {
  unsigned workers;
  std::uint64_t tiles;
  bool spill;
  bool drop_cache;
};

fields run_stream(const stream_params& p, const fs::path& scratch) {
  fields out;
  out["workers"] = std::to_string(p.workers);
  out["tiles"] = std::to_string(p.tiles);
  out["logical_bytes"] = std::to_string(p.tiles * tile_bytes);
  out["rss_baseline_kib"] = std::to_string(kib("VmRSS"));
  const std::uint64_t cached_before = meminfo_kib("Cached");
  const std::uint64_t dirty_before = meminfo_kib("Dirty");

  memory_pool pool(pool_bytes);
  job_scheduler sched(pool);
  const node_spec node = test_node("syn_stream");
  std::vector<std::int64_t> partial(p.tiles, 0);
  std::mutex order_mutex;
  std::vector<std::uint64_t> completion;
  spill_file* file = nullptr;
  std::uint64_t resident_after_write = 0, resident_after_read = 0, allocated = 0;
  std::uint64_t readback_mismatch = 0;
  std::uint64_t cached_after_write = 0, dirty_after_write = 0, cached_after_read = 0;

  job_spec spec{node, p.tiles, [&](task_context& ctx) {
                  pool_buffer in(ctx.memory, tile_bytes), scratch_buf(ctx.memory, tile_bytes), res(ctx.memory, tile_bytes);
                  float* input = reinterpret_cast<float*>(in.data());
                  float* middle = reinterpret_cast<float*>(scratch_buf.data());
                  float* result = reinterpret_cast<float*>(res.data());
                  ctx.check_cancel();
                  generate(input, ctx.index * tile_elems);
                  transform(input, middle, result);
                  partial[ctx.index] = checksum(result);
                  if (p.spill) {
                    file->write(ctx.index * tile_bytes, {res.data(), tile_bytes});
                    if (p.drop_cache) file->drop_cache(ctx.index * tile_bytes, tile_bytes);
                  }
                  {
                    std::lock_guard lock(order_mutex);
                    completion.push_back(ctx.index);
                  }
                  return std::string();
                }};
  if (p.spill) {
    spec.prepare = [&](job_context& jc) { file = &jc.spill->create_shared_file(); };
    spec.finalize = [&](job_context& jc) {
      resident_after_write = file->resident_bytes();
      allocated = file->allocated_bytes();
      cached_after_write = meminfo_kib("Cached");
      dirty_after_write = meminfo_kib("Dirty");
      pool_buffer back(jc.shared, tile_bytes);
      float* data = reinterpret_cast<float*>(back.data());
      for (std::uint64_t tile = 0; tile < p.tiles; ++tile) {
        file->read(tile * tile_bytes, {back.data(), tile_bytes});
        for (std::uint64_t t = 0; t < tile_elems; ++t) {
          const float expected = static_cast<float>(3 * k_of(tile * tile_elems + t) + 512) / 4096.0f;
          if (std::memcmp(&expected, &data[t], sizeof(float)) != 0) ++readback_mismatch;
        }
        if (p.drop_cache) file->drop_cache(tile * tile_bytes, tile_bytes);
      }
      resident_after_read = file->resident_bytes();
      cached_after_read = meminfo_kib("Cached");
    };
  } else {
    spec.prepare = [&](job_context& jc) { pool_buffer table(jc.shared, p.tiles * sizeof(std::int64_t)); };
  }

  job_plan plan;
  plan.shared_bytes = shared_reserve;
  plan.per_task_bytes = task_reserve;
  plan.worker_count = p.workers;
  plan.queue_capacity = 2 * p.workers;
  plan.tile_rows = static_cast<std::uint32_t>(tile_elems / 1024);  // righe da 1024 campioni
  if (p.spill) plan.spill = spill_config{scratch, p.tiles * tile_bytes};
  cancel_token cancel;

  job_report r;
  double wall = 0;
  std::uint64_t sampled_peak = 0;
  {
    rss_sampler sampler;
    const auto t0 = std::chrono::steady_clock::now();
    r = sched.run(plan, spec, cancel);
    wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    sampled_peak = sampler.peak_kib();
  }
  std::int64_t total = 0, expected_total = 0;
  std::uint64_t wrong_tiles = 0;
  for (std::uint64_t i = 0; i < p.tiles; ++i) {
    total += partial[i];
    expected_total += gt_tile(i);
    if (partial[i] != gt_tile(i)) ++wrong_tiles;
  }
  std::uint64_t inversions = 0;
  for (std::size_t i = 1; i < completion.size(); ++i) inversions += completion[i] < completion[i - 1] ? 1 : 0;
  std::vector<std::uint8_t> raw(partial.size() * sizeof(std::int64_t));
  std::memcpy(raw.data(), partial.data(), raw.size());

  out["wall_s"] = std::to_string(wall);
  out["throughput_gib_s"] = std::to_string(static_cast<double>(p.tiles * tile_bytes) / static_cast<double>(GiB) / wall);
  out["tasks_run"] = std::to_string(r.tasks_run);
  out["max_active_workers"] = std::to_string(r.max_active_workers);
  out["producer_waits"] = std::to_string(r.producer_waits);
  out["max_queue_depth"] = std::to_string(r.max_queue_depth);
  out["completion_inversions"] = std::to_string(inversions);
  out["pool_budget"] = std::to_string(pool.budget_bytes());
  out["pool_peak_reserved"] = std::to_string(pool.peak_reserved_bytes());
  out["pool_final_reserved"] = std::to_string(pool.reserved_bytes());
  out["job_peak_declared"] = std::to_string(r.job_peak_bytes);
  out["total_sum"] = std::to_string(total);
  out["gt_total"] = std::to_string(expected_total);
  out["wrong_tiles"] = std::to_string(wrong_tiles);
  out["partials_sha256"] = sha256_hex(raw);
  out["rss_peak_vmhwm_kib"] = std::to_string(kib("VmHWM"));
  out["rss_sampled_max_kib"] = std::to_string(sampled_peak);
  if (p.spill) {
    out["spill_drop_cache"] = p.drop_cache ? "yes" : "no";
    out["spill_quota_bytes"] = std::to_string(p.tiles * tile_bytes);
    out["spill_peak_bytes"] = std::to_string(r.spill_peak_bytes);
    out["spill_allocated_bytes"] = std::to_string(allocated);
    out["page_cache_resident_after_write_bytes"] = std::to_string(resident_after_write);
    out["page_cache_resident_after_readback_bytes"] = std::to_string(resident_after_read);
    const auto delta = [](std::uint64_t after, std::uint64_t before) {
      return std::to_string(static_cast<std::int64_t>(after) - static_cast<std::int64_t>(before));
    };
    out["meminfo_cached_delta_after_write_kib"] = delta(cached_after_write, cached_before);
    out["meminfo_cached_delta_after_readback_kib"] = delta(cached_after_read, cached_before);
    out["meminfo_dirty_delta_after_write_kib"] = delta(dirty_after_write, dirty_before);
    out["readback_mismatching_floats"] = std::to_string(readback_mismatch);
  }
  return out;
}

std::uint64_t as_u64(const fields& f, const char* key) { return std::stoull(f.at(key)); }

TEST(Protocol, T_M2D_13_oracle_closed_form_matches_brute_force_on_small_prefixes) {
  for (const std::uint64_t n : {0ULL, 1ULL, 5ULL, 511ULL, 1023ULL, 1024ULL, 1025ULL, 4097ULL, 100003ULL, 524288ULL}) {
    EXPECT_EQ(gt_prefix(n), brute_prefix(n)) << n;
  }
  EXPECT_EQ(gt_tile(0), 267649024);
  EXPECT_EQ(gt_tile(stream_tiles - 1), 267649024);
  EXPECT_EQ(stream_tiles, 20480u);
  EXPECT_EQ(shared_reserve + 4 * task_reserve, pool_bytes);
}

TEST(Protocol, T_M2D_13_stream_40GiB_logical_through_a_32MiB_pool_with_1_2_4_workers) {
  const fs::path scratch = fresh_dir("protocol_scratch_unused");
  std::set<std::string> digests;
  for (const unsigned workers : {1u, 2u, 4u}) {
    const fields f = in_child([&] { return run_stream({workers, stream_tiles, false, false}, scratch); });
    report("stream 40 GiB logici, " + std::to_string(workers) + " worker", f);
    ASSERT_EQ(f.at("exit_status"), "0") << (f.count("child_error") ? f.at("child_error") : "");
    EXPECT_EQ(f.at("total_sum"), f.at("gt_total"));
    EXPECT_EQ(f.at("wrong_tiles"), "0");
    EXPECT_EQ(as_u64(f, "tasks_run"), stream_tiles);
    EXPECT_LE(as_u64(f, "pool_peak_reserved"), pool_bytes);
    EXPECT_EQ(as_u64(f, "pool_peak_reserved"), shared_reserve + workers * task_reserve);
    EXPECT_EQ(f.at("pool_final_reserved"), "0");
    EXPECT_LE(as_u64(f, "max_active_workers"), workers);
    EXPECT_GE(as_u64(f, "max_active_workers"), 1u);
    EXPECT_LE(as_u64(f, "max_queue_depth"), 2 * workers);
    EXPECT_LT(as_u64(f, "rss_peak_vmhwm_kib"), 512u * 1024u) << "guardia best effort, non un limite del pool";
    digests.insert(f.at("partials_sha256"));
  }
  EXPECT_EQ(digests.size(), 1u) << "i risultati parziali dipendono dal numero di worker";
}

TEST(Protocol, T_M2D_13_physical_spill_of_256MiB_with_quota_and_page_cache_accounting) {
  const fs::path scratch = fresh_dir("protocol_scratch");
  for (const bool drop : {false, true}) {
    for (const unsigned workers : {1u, 2u, 4u}) {
      const fields f = in_child([&] { return run_stream({workers, spill_tiles, true, drop}, scratch); });
      report(std::string("spill 256 MiB, ") + std::to_string(workers) + " worker, drop_cache=" + (drop ? "si" : "no"), f);
      ASSERT_EQ(f.at("exit_status"), "0") << (f.count("child_error") ? f.at("child_error") : "");
      EXPECT_EQ(f.at("total_sum"), f.at("gt_total"));
      EXPECT_EQ(f.at("wrong_tiles"), "0");
      EXPECT_EQ(f.at("readback_mismatching_floats"), "0");
      EXPECT_EQ(as_u64(f, "spill_peak_bytes"), 256 * MiB);
      EXPECT_GE(as_u64(f, "spill_allocated_bytes"), 256 * MiB);
      EXPECT_LE(as_u64(f, "spill_peak_bytes"), 1 * GiB);
      EXPECT_LE(as_u64(f, "pool_peak_reserved"), pool_bytes);
      EXPECT_EQ(f.at("pool_final_reserved"), "0");
      EXPECT_LT(as_u64(f, "rss_peak_vmhwm_kib"), 512u * 1024u);
      EXPECT_EQ(entry_count(scratch), 0u);
    }
  }
}

TEST(Protocol, T_M2D_13_faults_at_stream_scale_stop_without_reducing_anything) {
  const fs::path scratch = fresh_dir("protocol_scratch_faults");
  memory_pool pool(pool_bytes);
  job_scheduler sched(pool);
  const node_spec node = test_node("syn_stream_faults");
  std::atomic<std::uint64_t> tasks{0};
  const auto body = [&](task_context& ctx) {
    pool_buffer in(ctx.memory, tile_bytes), scratch_buf(ctx.memory, tile_bytes), res(ctx.memory, tile_bytes);
    ctx.check_cancel();
    ++tasks;
    generate(reinterpret_cast<float*>(in.data()), ctx.index * tile_elems);
    transform(reinterpret_cast<float*>(in.data()), reinterpret_cast<float*>(scratch_buf.data()),
              reinterpret_cast<float*>(res.data()));
    return std::string();
  };
  job_plan plan;
  plan.shared_bytes = shared_reserve;
  plan.per_task_bytes = task_reserve;
  plan.worker_count = 4;
  plan.queue_capacity = 8;
  plan.tile_rows = 2048;
  cancel_token cancel;

  job_plan too_big = plan;
  too_big.shared_bytes = shared_reserve + MiB;
  job_spec rejected{node, stream_tiles, body};
  EXPECT_STOP(sched.run(too_big, rejected, cancel), error_category::budget, "sched.job_exceeds_budget");
  EXPECT_EQ(tasks.load(), 0u);

  job_spec failing{node, stream_tiles, [&](task_context& ctx) {
                     if (ctx.index == 3000) throw stop_error(error_category::io, "test.injected", "guasto iniettato");
                     return body(ctx);
                   }};
  EXPECT_STOP(sched.run(plan, failing, cancel), error_category::io, "test.injected");
  EXPECT_LT(tasks.load(), stream_tiles);
  EXPECT_EQ(pool.reserved_bytes(), 0u);

  tasks = 0;
  cancel_token mid;
  job_plan spilling = plan;
  spilling.spill = spill_config{scratch, 256 * MiB};
  spill_file* file = nullptr;
  job_spec cancelling{node, stream_tiles, [&](task_context& ctx) {
                        if (ctx.index == 500) mid.request();
                        const std::string r = body(ctx);
                        std::vector<std::byte> block(MiB, std::byte{1});
                        file->write((ctx.index % 200) * MiB, block);
                        return r;
                      }};
  cancelling.prepare = [&](job_context& jc) { file = &jc.spill->create_shared_file(); };
  EXPECT_STOP(sched.run(spilling, cancelling, mid), error_category::cancelled, "sched.cancelled");
  EXPECT_LT(tasks.load(), stream_tiles);
  EXPECT_EQ(entry_count(scratch), 0u);
  EXPECT_EQ(pool.reserved_bytes(), 0u);

  job_spec ok{node, 64, body};
  EXPECT_NO_THROW(sched.run(plan, ok, cancel));
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_LE(pool.peak_reserved_bytes(), pool_bytes);
}

}  // namespace
