#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <thread>
#include <vector>

#include "alloc_fault.hpp"
#include "astro/memory_pool.hpp"
#include "m2d_support.hpp"
#include "syn_fits.hpp"

namespace {

using namespace m2d;
namespace fx = astro_fixture;

// Generatore deterministico (xorshift64*), seme dichiarato nel test.
class prng {
 public:
  explicit prng(std::uint64_t seed) : s_(seed) {}
  std::uint64_t next() {
    s_ ^= s_ >> 12;
    s_ ^= s_ << 25;
    s_ ^= s_ >> 27;
    return s_ * 0x2545F4914F6CDD1DULL;
  }
  std::uint64_t below(std::uint64_t n) { return next() % n; }

 private:
  std::uint64_t s_;
};

constexpr std::uint64_t property_seed = 0x4D32445F50524F50ULL;

#ifdef ASTRO_SANITIZED
constexpr int ops_per_thread = 1500;
#else
constexpr int ops_per_thread = 20000;
#endif

class PoolProperty : public ::testing::TestWithParam<int> {};

TEST_P(PoolProperty, T_M2D_04_concurrent_reservations_never_exceed_the_budget) {
  const int threads = GetParam();
  constexpr std::uint64_t budget = 64 * 1024;
  memory_pool pool(budget);
  std::atomic<std::uint64_t> held{0};
  std::atomic<std::uint64_t> max_held{0};
  std::atomic<std::uint64_t> violations{0};
  std::atomic<std::uint64_t> granted{0}, refused{0};

  const auto note_grant = [&](std::uint64_t bytes) {
    const std::uint64_t now = held.fetch_add(bytes) + bytes;
    if (now > budget) ++violations;
    std::uint64_t peak = max_held.load();
    while (peak < now && !max_held.compare_exchange_weak(peak, now)) {
    }
    if (pool.reserved_bytes() > budget) ++violations;
  };

  const auto body = [&](int id) {
    prng rng(property_seed + static_cast<std::uint64_t>(id) * 7919);
    std::vector<reservation> mine;
    for (int op = 0; op < ops_per_thread; ++op) {
      const std::uint64_t action = rng.below(8);
      if (action < 4) {
        const std::uint64_t bytes = rng.below(budget / 3 + 1);
        if (mine.size() < 4) {
          if (auto r = pool.try_reserve(bytes, "p")) {
            note_grant(bytes);
            mine.push_back(std::move(*r));
            ++granted;
          } else {
            ++refused;
          }
        }
      } else if (action == 4 && mine.empty()) {
        const std::uint64_t bytes = rng.below(budget / 4 + 1);
        reservation r = pool.reserve(bytes, "b");
        note_grant(bytes);
        mine.push_back(std::move(r));
        ++granted;
      } else if (!mine.empty()) {
        const std::size_t victim = rng.below(mine.size());
        held.fetch_sub(mine[victim].bytes());
        mine.erase(mine.begin() + static_cast<std::ptrdiff_t>(victim));
      }
    }
    for (reservation& r : mine) held.fetch_sub(r.bytes());
    mine.clear();
  };

  std::vector<std::thread> pool_threads;
  for (int i = 0; i < threads; ++i) pool_threads.emplace_back(body, i);
  for (std::thread& t : pool_threads) t.join();

  EXPECT_EQ(violations.load(), 0u);
  EXPECT_LE(max_held.load(), budget);
  EXPECT_LE(pool.peak_reserved_bytes(), budget);
  EXPECT_GT(pool.peak_reserved_bytes(), 0u);
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_EQ(held.load(), 0u);
  EXPECT_GT(granted.load(), 0u);
  if (threads > 1) {
    EXPECT_GT(refused.load(), 0u);
  }
}

INSTANTIATE_TEST_SUITE_P(Threads, PoolProperty, ::testing::Values(1, 2, 4));

TEST(Pool, T_M2D_04_sub_reservations_never_exceed_their_parent_under_concurrency) {
  memory_pool pool(1 << 20);
  reservation parent = pool.reserve(1000, "parent");
  std::atomic<std::uint64_t> held{0};
  std::atomic<std::uint64_t> violations{0};
  std::atomic<std::uint64_t> refused{0};
  const auto body = [&](int id) {
    prng rng(property_seed ^ static_cast<std::uint64_t>(id + 1));
    std::vector<reservation> mine;
    for (int op = 0; op < ops_per_thread; ++op) {
      if (rng.below(3) != 0 && mine.size() < 3) {
        const std::uint64_t bytes = rng.below(301);
        try {
          mine.push_back(parent.carve(bytes, "c"));
          if (held.fetch_add(bytes) + bytes > 1000) ++violations;
        } catch (const stop_error& e) {
          if (e.code() != "pool.reservation_exceeded") ++violations;
          ++refused;
        }
      } else if (!mine.empty()) {
        held.fetch_sub(mine.back().bytes());
        mine.pop_back();
      }
    }
    for (reservation& r : mine) held.fetch_sub(r.bytes());
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) threads.emplace_back(body, i);
  for (std::thread& t : threads) t.join();
  EXPECT_EQ(violations.load(), 0u);
  EXPECT_GT(refused.load(), 0u);
  EXPECT_EQ(parent.committed_bytes(), 0u);
  EXPECT_EQ(pool.reserved_bytes(), 1000u);
}

TEST(Pool, reservation_is_all_or_nothing_and_released_once) {
  memory_pool pool(100);
  {
    std::optional<reservation> a = pool.try_reserve(60, "a");
    ASSERT_TRUE(a);
    EXPECT_EQ(pool.reserved_bytes(), 60u);
    EXPECT_FALSE(pool.try_reserve(41, "b"));
    EXPECT_EQ(pool.reserved_bytes(), 60u);
    std::optional<reservation> c = pool.try_reserve(40, "c");
    ASSERT_TRUE(c);
    EXPECT_EQ(pool.reserved_bytes(), 100u);
    reservation moved = std::move(*a);
    EXPECT_EQ(pool.reserved_bytes(), 100u);
    a.reset();
    EXPECT_EQ(pool.reserved_bytes(), 100u);
    moved.release();
    moved.release();
    EXPECT_EQ(pool.reserved_bytes(), 40u);
  }
  EXPECT_EQ(pool.reserved_bytes(), 0u);
  EXPECT_EQ(pool.peak_reserved_bytes(), 100u);
  EXPECT_STOP(pool.try_reserve(101, "big"), error_category::budget, "pool.exceeds_budget");
  EXPECT_STOP(pool.reserve(101, "big"), error_category::budget, "pool.exceeds_budget");
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Pool, T_M2D_12_allocation_beyond_the_reservation_stops) {
  memory_pool pool(1 << 20);
  reservation r = pool.reserve(4096, "task");
  {
    pool_buffer a(r, 4096);
    EXPECT_EQ(a.size(), 4096u);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(a.data()) % aligned_buffer::alignment, 0u);
    EXPECT_EQ(r.committed_bytes(), 4096u);
    EXPECT_STOP(pool_buffer(r, 1), error_category::budget, "pool.reservation_exceeded");
    EXPECT_STOP(r.carve(1, "extra"), error_category::budget, "pool.reservation_exceeded");
  }
  EXPECT_EQ(r.committed_bytes(), 0u);
  {
    pool_buffer a(r, 1024);
    pool_buffer b(r, 3072);
    EXPECT_STOP(pool_buffer(r, 1), error_category::budget, "pool.reservation_exceeded");
  }
  EXPECT_STOP(pool_buffer(r, 4097), error_category::budget, "pool.reservation_exceeded");
  EXPECT_EQ(r.committed_bytes(), 0u);
  EXPECT_EQ(pool.reserved_bytes(), 4096u);
  r.release();
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Pool, T_M2D_12_the_stop_names_the_reservation_and_the_amounts) {
  memory_pool pool(1 << 20);
  reservation r = pool.reserve(64, "tile");
  try {
    pool_buffer big(r, 65);
    ADD_FAILURE() << "stop attesa";
  } catch (const stop_error& e) {
    std::string ctx;
    for (const auto& [k, v] : e.context()) ctx += k + "=" + v + ";";
    EXPECT_NE(ctx.find("prenotazione=tile"), std::string::npos) << ctx;
    EXPECT_NE(ctx.find("richiesti=65"), std::string::npos) << ctx;
  }
}

TEST(Pool, sub_reservation_returns_its_bytes_to_the_parent_not_the_pool) {
  memory_pool pool(1000);
  reservation parent = pool.reserve(600, "parent");
  {
    reservation child = parent.carve(400, "child");
    EXPECT_EQ(parent.committed_bytes(), 400u);
    EXPECT_EQ(pool.reserved_bytes(), 600u);
    pool_buffer inside(child, 100);
    EXPECT_STOP(pool_buffer(child, 301), error_category::budget, "pool.reservation_exceeded");
  }
  EXPECT_EQ(parent.committed_bytes(), 0u);
  EXPECT_EQ(pool.reserved_bytes(), 600u);
}

TEST(Pool, waiters_are_granted_in_strict_fifo_order) {
  memory_pool pool(10);
  reservation holder = pool.reserve(10, "holder");
  // Tre richieste incompatibili fra loro: ciascuna entra solo dopo il rilascio della precedente.
  constexpr int waiters = 3;
  std::atomic<int> sequence{0};
  std::array<std::atomic<int>, waiters> rank{};
  std::array<gate, waiters> release;
  std::vector<std::thread> threads;
  for (int i = 0; i < waiters; ++i) {
    threads.emplace_back([&, i] {
      reservation r = pool.reserve(8, "w" + std::to_string(i));
      rank[static_cast<std::size_t>(i)].store(++sequence);
      release[static_cast<std::size_t>(i)].wait();
    });
    ASSERT_TRUE(wait_until([&] { return pool.waiting_count() == static_cast<std::size_t>(i + 1); }));
  }
  EXPECT_EQ(sequence.load(), 0);
  holder.release();
  for (int i = 0; i < waiters; ++i) {
    ASSERT_TRUE(wait_until([&] { return rank[static_cast<std::size_t>(i)].load() != 0; }));
    EXPECT_EQ(rank[static_cast<std::size_t>(i)].load(), i + 1) << "grant fuori ordine";
    EXPECT_EQ(pool.reserved_bytes(), 8u);
    EXPECT_EQ(pool.waiting_count(), static_cast<std::size_t>(waiters - 1 - i));
    release[static_cast<std::size_t>(i)].open();
  }
  for (std::thread& t : threads) t.join();
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Pool, an_allocation_failure_leaves_no_orphan_reservation) {
  memory_pool pool(100);
  reservation parent = pool.reserve(60, "parent");
  std::string first = "x";
  EXPECT_THROW(
      {
        alloc_fault::arm();
        (void)pool.try_reserve(10, std::move(first));
      },
      std::bad_alloc);
  EXPECT_EQ(pool.reserved_bytes(), 60u);
  std::string second = "y";
  EXPECT_THROW(
      {
        alloc_fault::arm();
        (void)parent.carve(10, std::move(second));
      },
      std::bad_alloc);
  EXPECT_EQ(parent.committed_bytes(), 0u);
  EXPECT_EQ(pool.reserved_bytes(), 60u);
  EXPECT_TRUE(pool.try_reserve(40, "fits"));
}

TEST(Pool, a_waiting_reservation_honours_the_queue_cap_and_cancellation) {
  memory_pool pool(10);
  reservation holder = pool.reserve(10, "holder");
  cancel_token cancel;
  std::optional<stop_error> cancelled;
  std::thread waiter([&] {
    try {
      (void)pool.reserve(1, "waiter", &cancel, 1);
    } catch (const stop_error& e) {
      cancelled = e;
    }
  });
  ASSERT_TRUE(wait_until([&] { return pool.waiting_count() == 1; }));
  EXPECT_STOP(pool.reserve(1, "second", nullptr, 1), error_category::budget, "sched.queue_full");
  EXPECT_EQ(pool.waiting_count(), 1u);
  cancel.request();
  waiter.join();
  ASSERT_TRUE(cancelled);
  EXPECT_EQ(cancelled->category(), error_category::cancelled);
  EXPECT_EQ(cancelled->code(), "sched.cancelled");
  EXPECT_EQ(pool.waiting_count(), 0u);
  holder.release();
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

TEST(Pool, input_read_buffer_is_reserved_like_any_other) {
  const fs::path dir = fresh_dir("pool_input");
  const auto file = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  const fs::path path = dir / "frame.fits";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }
  const content_digest digest = digest_of(file);
  memory_pool pool(1 << 20);
  reservation big = pool.reserve(1 << 16, "job");
  {
    verified_fits input = read_verified_fits(path.string(), digest, big);
    EXPECT_EQ(input.bytes, file);
    EXPECT_EQ(big.committed_bytes(), file.size());
  }
  EXPECT_EQ(big.committed_bytes(), 0u);
  reservation small = pool.reserve(file.size() - 1, "small");
  EXPECT_STOP(read_verified_fits(path.string(), digest, small), error_category::budget, "pool.reservation_exceeded");
  EXPECT_STOP(read_verified_fits(path.string(), content_digest{std::string(64, '1'), file.size()}, big),
              error_category::integrity, "input.digest_mismatch");
  EXPECT_EQ(big.committed_bytes(), 0u);
}

}  // namespace
