#include <gtest/gtest.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <string_view>
#include <thread>
#include <vector>

#include "alloc_fault.hpp"
#include "astro/spill.hpp"
#include "m2d_support.hpp"

namespace {

using namespace m2d;

std::vector<std::byte> pattern(std::size_t n, unsigned seed) {
  std::vector<std::byte> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::byte>((i * 31 + seed) & 0xff);
  return v;
}

TEST(Spill, T_M2D_10_scratch_missing_is_a_stop) {
  const fs::path dir = fresh_dir("spill_missing");
  EXPECT_STOP(spill_area(spill_config{dir / "absent", 1 << 20}), error_category::io, "spill.scratch_unusable");
}

TEST(Spill, T_M2D_10_scratch_that_is_a_regular_file_is_a_stop) {
  const fs::path dir = fresh_dir("spill_file");
  write_text(dir / "plain", "x");
  EXPECT_STOP(spill_area(spill_config{dir / "plain", 1 << 20}), error_category::io, "spill.scratch_unusable");
}

TEST(Spill, T_M2D_10_scratch_symlink_is_a_stop_even_to_a_good_directory) {
  const fs::path dir = fresh_dir("spill_symlink");
  fs::create_directories(dir / "real");
  fs::create_directory_symlink(dir / "real", dir / "link");
  EXPECT_NO_THROW(spill_area(spill_config{dir / "real", 1 << 20}));
  EXPECT_STOP(spill_area(spill_config{dir / "link", 1 << 20}), error_category::io, "spill.scratch_unusable");
}

TEST(Spill, T_M2D_10_unwritable_scratch_is_a_stop) {
  const fs::path dir = fresh_dir("spill_ro");
  fs::create_directories(dir / "ro");
  fs::permissions(dir / "ro", fs::perms::owner_read | fs::perms::owner_exec);
  ASSERT_NE(::geteuid(), 0u) << "il test richiede un utente non privilegiato";
  EXPECT_STOP(spill_area(spill_config{dir / "ro", 1 << 20}), error_category::io, "spill.scratch_unusable");
  fs::permissions(dir / "ro", fs::perms::owner_all);
}

TEST(Spill, T_M2D_10_directory_that_refuses_o_tmpfile_is_a_stop) {
  EXPECT_STOP(spill_area(spill_config{"/proc", 1 << 20}), error_category::io, "spill.scratch_unusable");
}

TEST(Spill, T_M2D_10_quota_exceeded_is_a_stop_and_writes_nothing) {
  const fs::path dir = fresh_dir("spill_quota");
  spill_area area(spill_config{dir, 8192});
  spill_file f = area.create_file();
  const auto block = pattern(4096, 1);
  f.write(0, block);
  f.write(4096, block);
  EXPECT_EQ(f.size_bytes(), 8192u);
  EXPECT_EQ(area.used_bytes(), 8192u);
  const std::byte one{0x7f};
  EXPECT_STOP(f.write(8192, std::span<const std::byte>(&one, 1)), error_category::budget, "spill.quota_exceeded");
  EXPECT_EQ(f.size_bytes(), 8192u);
  EXPECT_EQ(area.used_bytes(), 8192u);
  f.write(100, std::span<const std::byte>(&one, 1));  // dentro l'estensione: nessuna crescita
  EXPECT_EQ(area.used_bytes(), 8192u);
  std::vector<std::byte> back(8192);
  f.read(0, back);
  EXPECT_EQ(back[100], one);
  EXPECT_EQ(back[101], block[101]);
  EXPECT_EQ(back[4096 + 5], block[5]);
}

TEST(Spill, T_M2D_10_quota_is_shared_by_files_and_returned_when_a_file_is_destroyed) {
  const fs::path dir = fresh_dir("spill_quota_shared");
  spill_area area(spill_config{dir, 8192});
  const auto block = pattern(4096, 2);
  {
    spill_file a = area.create_file();
    spill_file b = area.create_file();
    a.write(0, block);
    b.write(0, block);
    EXPECT_EQ(area.used_bytes(), 8192u);
    EXPECT_STOP(a.write(4096, block), error_category::budget, "spill.quota_exceeded");
    spill_file moved = std::move(b);
    EXPECT_EQ(moved.size_bytes(), 4096u);
  }
  EXPECT_EQ(area.used_bytes(), 0u);
  EXPECT_EQ(area.peak_bytes(), 8192u);
  spill_file c = area.create_file();
  c.write(0, pattern(8192, 3));
}

TEST(Spill, files_are_anonymous_and_leave_the_scratch_directory_empty) {
  const fs::path dir = fresh_dir("spill_anon");
  spill_area area(spill_config{dir, 1 << 20});
  spill_file f = area.create_file();
  f.write(0, pattern(4096, 4));
  EXPECT_EQ(entry_count(dir), 0u);
}

TEST(Spill, round_trip_and_short_reads) {
  const fs::path dir = fresh_dir("spill_rw");
  spill_area area(spill_config{dir, 1 << 20});
  spill_file f = area.create_file();
  const auto data = pattern(10000, 5);
  f.write(123, data);
  EXPECT_EQ(f.size_bytes(), 123u + 10000u);
  std::vector<std::byte> back(10000);
  f.read(123, back);
  EXPECT_EQ(back, data);
  std::vector<std::byte> too_long(10001);
  EXPECT_STOP(f.read(123, too_long), error_category::integrity, "spill.short_read");
  EXPECT_STOP(f.read(1 << 30, back), error_category::integrity, "spill.short_read");
}

TEST(Spill, T_M2D_10_concurrent_writes_respect_the_quota_exactly) {
  const fs::path dir = fresh_dir("spill_concurrent");
  spill_area area(spill_config{dir, 4 * MiB});
  spill_file f = area.create_file();
  const auto block = pattern(MiB, 6);
  std::atomic<int> ok{0}, refused{0}, unexpected{0};
  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&, i] {
      try {
        f.write(static_cast<std::uint64_t>(i) * MiB, block);
        ++ok;
      } catch (const stop_error& e) {
        (e.code() == "spill.quota_exceeded" ? refused : unexpected)++;
      }
    });
  }
  for (std::thread& t : threads) t.join();
  EXPECT_EQ(unexpected.load(), 0);
  EXPECT_EQ(ok.load(), 4);
  EXPECT_EQ(refused.load(), 4);
  EXPECT_EQ(area.used_bytes(), 4 * MiB);
  EXPECT_LE(area.peak_bytes(), 4 * MiB);
}

TEST(Spill, resident_bytes_counts_cached_pages_and_drop_cache_keeps_the_content) {
  const fs::path dir = fresh_dir("spill_cache");
  spill_area area(spill_config{dir, 16 * MiB});
  spill_file f = area.create_file();
  EXPECT_EQ(f.resident_bytes(), 0u);
  const auto block = pattern(MiB, 7);
  for (std::uint64_t i = 0; i < 4; ++i) f.write(i * MiB, block);
  const std::uint64_t before = f.resident_bytes();
  EXPECT_GT(before, 0u);
  EXPECT_LE(before, 4 * MiB);
  f.drop_cache(0, 4 * MiB);
  EXPECT_LE(f.resident_bytes(), before);
  std::vector<std::byte> back(MiB);
  f.read(2 * MiB, back);
  EXPECT_EQ(back, block);
}

// Solo le mappature dei file di scratch in `dir`: l'allocatore (ASan compreso) aggiunge mappature proprie.
std::size_t mapping_count(const fs::path& dir) {
  const std::string prefix = fs::canonical(dir).string() + "/";
  std::ifstream in("/proc/self/maps");
  std::size_t n = 0;
  std::string line;
  while (std::getline(in, line)) {
    if (line.find(prefix) != std::string::npos) ++n;
  }
  return n;
}

TEST(Spill, concurrent_extensions_are_charged_once_and_never_spuriously_refused) {
  const fs::path dir = fresh_dir("spill_extend");
  const std::byte one{1};
  for (int round = 0; round < 300; ++round) {
    spill_area area(spill_config{dir, 4 * MiB});
    spill_file f = area.create_file();
    std::atomic<int> ready{0};
    std::atomic<int> refused{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
      threads.emplace_back([&, i] {
        ++ready;
        while (ready.load() < 8) {
        }
        try {
          f.write(static_cast<std::uint64_t>(i + 1) * 512 * 1024 - 1, std::span<const std::byte>(&one, 1));
        } catch (const stop_error&) {
          ++refused;
        }
      });
    }
    for (std::thread& t : threads) t.join();
    ASSERT_EQ(refused.load(), 0) << "round " << round;
    ASSERT_EQ(area.used_bytes(), 4 * MiB);
    ASSERT_EQ(f.size_bytes(), 4 * MiB);
  }
}

TEST(Spill, a_failed_allocation_in_resident_bytes_does_not_leak_the_mapping) {
  const fs::path dir = fresh_dir("spill_resident_leak");
  spill_area area(spill_config{dir, 16 * MiB});
  spill_file f = area.create_file();
  f.write(0, pattern(4 * MiB, 9));
  const std::size_t before = mapping_count(dir);
  EXPECT_THROW(
      {
        alloc_fault::arm();
        (void)f.resident_bytes();
      },
      std::bad_alloc);
  EXPECT_EQ(mapping_count(dir), before);
}

// ---------------------------------------------------------------- T-M2R3-05: ENOSPC e quota

TEST(Spill, T_M2R3_05_enospc_on_a_write_is_io_with_errno_and_releases_the_extension) {
  const fs::path dir = fresh_dir("spill_enospc");
  spill_area area(spill_config{dir, 1 << 20});
  int calls = 0;
  area.set_fault_injector([&](std::string_view point) {
    EXPECT_EQ(point, "spill.write");
    return ++calls == 2 ? ENOSPC : 0;
  });
  spill_file f = area.create_file();
  f.write(0, pattern(4096, 1));
  EXPECT_EQ(area.used_bytes(), 4096u);
  try {
    f.write(4096, pattern(8192, 2));
    ADD_FAILURE() << "ENOSPC atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io);
    EXPECT_EQ(e.code(), "spill.write");
    bool found = false;
    for (const auto& [k, v] : e.context()) found = found || (k == "errno" && v == std::to_string(ENOSPC));
    EXPECT_TRUE(found) << e.what();
  }
  EXPECT_EQ(area.used_bytes(), 4096u) << "l'estensione della scrittura fallita torna alla quota";
  EXPECT_EQ(f.size_bytes(), 4096u);
  f.write(4096, pattern(8192, 3));
  EXPECT_EQ(area.used_bytes(), 12288u);
  std::vector<std::byte> back(8192);
  f.read(4096, back);
  EXPECT_EQ(back, pattern(8192, 3));
}

TEST(Spill, T_M2R3_05_a_failed_write_inside_the_extension_keeps_the_charge) {
  const fs::path dir = fresh_dir("spill_enospc_inner");
  spill_area area(spill_config{dir, 1 << 20});
  bool fail = false;
  area.set_fault_injector([&](std::string_view) { return fail ? ENOSPC : 0; });
  spill_file f = area.create_file();
  f.write(0, pattern(8192, 1));
  fail = true;
  EXPECT_STOP(f.write(100, pattern(10, 2)), error_category::io, "spill.write");
  EXPECT_EQ(area.used_bytes(), 8192u);
}

// ---------------------------------------------------------------- T-M2R3-06: staging nominato

TEST(Spill, T_M2R3_06_named_file_survives_the_area_and_is_reopened_with_its_extension_charged) {
  const fs::path dir = fresh_dir("spill_named");
  {
    spill_area area(spill_config{dir, 1 << 20});
    spill_file& f = area.open_named_file("job.stage", false);
    f.write(0, pattern(5000, 4));
    EXPECT_EQ(area.used_bytes(), 5000u);
  }
  ASSERT_TRUE(fs::exists(dir / "job.stage"));
  EXPECT_EQ(fs::file_size(dir / "job.stage"), 5000u);
  {
    spill_area area(spill_config{dir, 1 << 20});
    spill_file& f = area.open_named_file("job.stage", true);
    EXPECT_EQ(area.used_bytes(), 5000u);
    std::vector<std::byte> back(5000);
    f.read(0, back);
    EXPECT_EQ(back, pattern(5000, 4));
  }
  {
    spill_area area(spill_config{dir, 4000});
    EXPECT_STOP(area.open_named_file("job.stage", true), error_category::budget, "spill.quota_exceeded");
  }
  {
    spill_area area(spill_config{dir, 1 << 20});
    spill_file& f = area.open_named_file("job.stage", false);  // esecuzione nuova: troncato
    EXPECT_EQ(f.size_bytes(), 0u);
    EXPECT_EQ(area.used_bytes(), 0u);
  }
  EXPECT_EQ(fs::file_size(dir / "job.stage"), 0u);
  remove_named_spill_file(dir, "job.stage");
  EXPECT_FALSE(fs::exists(dir / "job.stage"));
  remove_named_spill_file(dir, "job.stage");
}

TEST(Spill, T_M2R3_06_named_file_rules) {
  const fs::path dir = fresh_dir("spill_named_rules");
  spill_area area(spill_config{dir, 1 << 20});
  EXPECT_STOP(area.open_named_file("absent.stage", true), error_category::metadata_missing, "sched.checkpoint_missing");
  for (const char* bad : {"", ".", "..", "a/b", "x y"}) {
    EXPECT_STOP(area.open_named_file(bad, false), error_category::invalid_input, "spill.staging_name");
  }
  write_text(dir / "target", "x");
  fs::create_symlink(dir / "target", dir / "link.stage");
  EXPECT_STOP(area.open_named_file("link.stage", false), error_category::security, "spill.staging_unsafe");
  EXPECT_STOP(area.open_named_file("link.stage", true), error_category::security, "spill.staging_unsafe");
  ASSERT_EQ(::link((dir / "target").c_str(), (dir / "hard.stage").c_str()), 0);
  EXPECT_STOP(area.open_named_file("hard.stage", true), error_category::security, "spill.staging_unsafe");
  fs::create_directory(dir / "sub.stage");
  EXPECT_STOP(area.open_named_file("sub.stage", true), error_category::security, "spill.staging_unsafe");
  EXPECT_EQ(read_text(dir / "target"), "x");
}

// Scrittura corta reale: RLIMIT_FSIZE nel processo figlio tronca il pwrite e il successivo da' EFBIG.
TEST(Spill, T_M2R3_05_a_short_write_keeps_extension_and_quota_of_the_bytes_written) {
  const fs::path dir = fresh_dir("spill_short_write");
  const pid_t pid = ::fork();
  ASSERT_GE(pid, 0);
  if (pid == 0) {
    ::signal(SIGXFSZ, SIG_IGN);
    struct rlimit limit {6000, 6000};
    if (::setrlimit(RLIMIT_FSIZE, &limit) != 0) ::_exit(10);
    try {
      spill_area area(spill_config{dir, 1 << 20});
      spill_file f = area.create_file();
      f.write(0, pattern(4096, 1));
      bool stopped = false;
      try {
        f.write(4096, pattern(4096, 2));
      } catch (const stop_error& e) {
        stopped = e.code() == "spill.write";
      }
      if (!stopped) ::_exit(11);
      if (f.size_bytes() != 6000) ::_exit(12);
      if (area.used_bytes() != 6000) ::_exit(13);
      std::vector<std::byte> back(6000 - 4096);
      f.read(4096, back);
      const std::vector<std::byte> expected = pattern(4096, 2);
      if (!std::equal(back.begin(), back.end(), expected.begin())) ::_exit(14);
    } catch (...) {
      ::_exit(15);
    }
    ::_exit(0);
  }
  int status = 0;
  ASSERT_EQ(::waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0) << "10 setrlimit, 11 nessuno stop, 12 estensione, 13 quota, 14 contenuto, 15 eccezione";
}

TEST(Spill, T_M2R3_05_a_write_inside_a_failing_extension_waits_and_keeps_its_bytes) {
  const fs::path dir = fresh_dir("spill_concurrent_fail");
  spill_area area(spill_config{dir, 1 << 20});
  spill_file f = area.create_file();
  f.write(0, pattern(1000, 1));
  m2d::gate release;
  std::atomic<bool> first_inside{false}, second_done{false};
  area.set_fault_injector([&](std::string_view) {
    if (!first_inside.exchange(true)) {
      EXPECT_TRUE(release.wait());
      return ENOSPC;
    }
    return 0;
  });
  std::thread failing([&] { EXPECT_STOP(f.write(1000, pattern(9000, 2)), error_category::io, "spill.write"); });
  EXPECT_TRUE(wait_until([&] { return first_inside.load(); }));
  std::thread inside([&] {
    f.write(2000, pattern(500, 3));  // dentro l'estensione della scrittura che fallira'
    second_done = true;
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  EXPECT_FALSE(second_done.load()) << "la scrittura nel tratto incerto attende l'esito";
  release.open();
  failing.join();
  inside.join();
  EXPECT_EQ(f.size_bytes(), 2500u);
  EXPECT_EQ(area.used_bytes(), 2500u);
  std::vector<std::byte> back(500);
  f.read(2000, back);
  EXPECT_EQ(back, pattern(500, 3));
}

}  // namespace
