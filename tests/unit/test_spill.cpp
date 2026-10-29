#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
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

std::size_t mapping_count() {
  std::ifstream in("/proc/self/maps");
  std::size_t n = 0;
  std::string line;
  while (std::getline(in, line)) ++n;
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
  const std::size_t before = mapping_count();
  EXPECT_THROW(
      {
        alloc_fault::arm();
        (void)f.resident_bytes();
      },
      std::bad_alloc);
  EXPECT_EQ(mapping_count(), before);
}

}  // namespace
