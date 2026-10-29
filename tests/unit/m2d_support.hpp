#pragma once

#include <gtest/gtest.h>
#include <sched.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#include "astro/hash.hpp"
#include "astro/node.hpp"
#include "astro/scheduler.hpp"
#include "expect_stop.hpp"

namespace m2d {

using namespace astro;
namespace fs = std::filesystem;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;

inline fs::path fresh_dir(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "m2d" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

inline void write_text(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
}

inline std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

inline std::size_t entry_count(const fs::path& dir) {
  return static_cast<std::size_t>(std::distance(fs::directory_iterator(dir), fs::directory_iterator()));
}

// Fotografia sintetica di /proc e /sys/fs/cgroup.
struct fake_system {
  fs::path root;
  budget_sources sources() const { return budget_sources{root / "proc", root / "cgroup"}; }

  explicit fake_system(const std::string& name) : root(fresh_dir(name)) {
    meminfo(8 * 1024 * 1024, 4 * 1024 * 1024);
  }
  void meminfo(std::uint64_t total_kib, std::uint64_t available_kib) const {
    write_text(root / "proc" / "meminfo", "MemTotal:       " + std::to_string(total_kib) +
                                              " kB\nMemFree:         1000 kB\nMemAvailable:   " +
                                              std::to_string(available_kib) + " kB\nBuffers:  12 kB\n");
  }
  void psi(const std::string& text) const { write_text(root / "proc" / "pressure" / "memory", text); }
  void self_cgroup(const std::string& text) const { write_text(root / "proc" / "self" / "cgroup", text); }
  void cgroup(const std::string& rel, const std::string& max, const std::string& current) const {
    write_text(root / "cgroup" / rel / "memory.max", max);
    if (current.empty()) {
      fs::remove(root / "cgroup" / rel / "memory.current");
    } else {
      write_text(root / "cgroup" / rel / "memory.current", current);
    }
  }
};

// Insieme di CPU iniettato al posto di sched_getaffinity.
inline affinity_fn affinity_of(int cpus) {
  return [cpus](cpu_set_t* set) {
    CPU_ZERO(set);
    for (int i = 0; i < cpus; ++i) CPU_SET(i, set);
    return 0;
  };
}

inline node_spec test_node(const std::string& name, const std::string& backend = "cpu_scalar") {
  json params = json::object();
  params.set("name", json::string(name));
  return node_spec("test.op", 1, 1, std::move(params), {}, sample_type::f32, backend, std::nullopt);
}

inline std::string digest_text(const std::string& text) {
  return sha256_hex({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

// Cancello a una volta, con guardia di tempo: un test che non converge fallisce invece di bloccarsi.
class gate {
 public:
  void open() {
    {
      std::lock_guard lock(m_);
      open_ = true;
    }
    cv_.notify_all();
  }
  bool wait(std::chrono::seconds limit = std::chrono::seconds(30)) {
    std::unique_lock lock(m_);
    return cv_.wait_for(lock, limit, [&] { return open_; });
  }

 private:
  std::mutex m_;
  std::condition_variable cv_;
  bool open_ = false;
};

template <class Pred>
bool wait_until(Pred pred, std::chrono::seconds limit = std::chrono::seconds(30)) {
  const auto end = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < end) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return pred();
}

}  // namespace m2d
