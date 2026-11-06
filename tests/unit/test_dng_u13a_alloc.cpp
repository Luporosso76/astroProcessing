// U13a (M3.a), T-U13-06: BlackLevelRepeatDim 65535 × 65535 senza BlackLevel in un file di pochi KiB. BlackLevel assente
// vale 0 in ogni sito (docs/contracts/U13-dng-decoder.md §4): lo stesso esito del file con la dimensione predefinita, senza
// una tabella di R · C valori. Eseguibile a parte: la sonda sostituisce `operator new` per tutto il processo e non deve
// togliere i controlli (ASan) agli altri casi di U13a.
// Fixture: tests/fixtures/syn_dng.hpp, indipendente da astrocore e da LibRaw. Tolleranze: nessuna, criteri binari.
#include <gtest/gtest.h>
#include <zlib.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <new>
#include <optional>
#include <string>
#include <vector>

#include "astro/error.hpp"
#include "astro/image.hpp"
#include "astro/raw_decode.hpp"
#include "syn_dng.hpp"

// Con `armed` registra la richiesta più grande e rifiuta (bad_alloc) quelle sopra 1 GiB senza chiedere memoria al sistema.
// Sostituisce gli operatori non allineati (anche nothrow, come fa ASan); malloc e free restano quelli del processo.
namespace alloc_probe {
inline std::atomic<bool> armed{false};
inline std::atomic<std::size_t> largest{0};
constexpr std::size_t refuse_above = std::size_t{1} << 30;
inline void* take(std::size_t n) {
  if (armed.load(std::memory_order_relaxed)) {
    std::size_t seen = largest.load(std::memory_order_relaxed);
    while (n > seen && !largest.compare_exchange_weak(seen, n, std::memory_order_relaxed)) {
    }
    if (n > refuse_above) throw std::bad_alloc();
  }
  if (void* p = std::malloc(n != 0 ? n : 1)) return p;
  throw std::bad_alloc();
}
}  // namespace alloc_probe
void* operator new(std::size_t n) { return alloc_probe::take(n); }
void* operator new[](std::size_t n) { return alloc_probe::take(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  try {
    return alloc_probe::take(n);
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
  try {
    return alloc_probe::take(n);
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {

using namespace astro;
namespace fx = astro_fixture;
namespace tt = astro_fixture::tiff_type;

using bytes_t = std::vector<std::uint8_t>;

bytes_t deflate(const bytes_t& in) {
  uLongf n = compressBound(static_cast<uLong>(in.size()));
  bytes_t out(n);
  const int rc = compress2(out.data(), &n, in.data(), static_cast<uLong>(in.size()), 6);
  if (rc != Z_OK) {
    ADD_FAILURE() << "compress2 della fixture fallita: " << rc;
    return {};
  }
  out.resize(n);
  return out;
}

bytes_t one_ifd(const std::function<void(fx::tiff_ifd&)>& mutate) {
  fx::tiff_ifd ifd = fx::make_dng32_ifd(fx::dng32_spec{}, fx::tiff_order::ii, deflate);
  mutate(ifd);
  fx::tiff_file f;
  f.chain.push_back(std::move(ifd));
  return fx::write_tiff(f).bytes;
}

struct observed {
  std::optional<image> img;
  std::optional<stop_error> stop;
  int open_buffer_calls = 0;
  int inflate_calls = 0;
};

observed run(const bytes_t& bytes) {
  observed o;
  raw_decode_hooks hooks;
  hooks.libraw_return = [&](std::string_view function, int rc) {
    if (function == "open_buffer") ++o.open_buffer_calls;
    return rc;
  };
  hooks.inflate_return = [&](std::size_t, int rc) {
    ++o.inflate_calls;
    return rc;
  };
  try {
    o.img.emplace(decode_raw_buffer(bytes, hooks));
  } catch (const stop_error& e) {
    o.stop.emplace(e);
  }
  return o;
}

std::string describe(const observed& o) {
  if (o.stop) return o.stop->what();
  if (o.img) return "immagine " + std::to_string(o.img->geometry.width) + "x" + std::to_string(o.img->geometry.height);
  return "nessun esito";
}

TEST(DngTags, T_U13_06_black_level_repeat_dim_without_black_level_allocates_nothing_large) {
  const bytes_t base = one_ifd([](fx::tiff_ifd& i) { fx::remove_entry(i, 50714); });
  const bytes_t huge = one_ifd([](fx::tiff_ifd& i) {
    fx::remove_entry(i, 50714);
    fx::set_entry(i, fx::ent(50713, tt::short_, {65535, 65535}));
  });
  ASSERT_LT(huge.size(), 64u * 1024u);
  const observed expected = run(base);
  ASSERT_GE(expected.inflate_calls, 1) << describe(expected);

  alloc_probe::largest.store(0);
  alloc_probe::armed.store(true);
  std::optional<observed> got;
  std::string failure;
  try {
    got.emplace(run(huge));
  } catch (const std::exception& e) {
    failure = e.what();
  }
  alloc_probe::armed.store(false);
  ASSERT_TRUE(failure.empty()) << "eccezione non gestita (allocazione rifiutata?): " << failure;
  EXPECT_LT(alloc_probe::largest.load(), std::size_t{64} << 20) << "allocazione grande dal valore di BlackLevelRepeatDim";
  ASSERT_TRUE(got.has_value());
  EXPECT_EQ(got->open_buffer_calls, 0);
  EXPECT_EQ(got->inflate_calls, expected.inflate_calls) << "i segmenti non sono stati letti come nel file di base";
  ASSERT_EQ(got->stop.has_value(), expected.stop.has_value());
  if (got->stop) {
    EXPECT_EQ(got->stop->category(), expected.stop->category()) << got->stop->what();
    EXPECT_EQ(got->stop->code(), expected.stop->code()) << got->stop->what();
  }
  EXPECT_EQ(got->img.has_value(), expected.img.has_value());
}

}  // namespace
