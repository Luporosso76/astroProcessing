// U13b (M3.a): T-U13-04 (tetto del gestore di memoria di zlib iniettato, picco delle allocazioni) e T-U13-09 (cancel
// dopo il segmento k di n), docs/contracts/U13-dng-decoder.md §3 («Ganci dei test»), §5, §7, §12.
//
// Ganci (nomi scelti dall'autore dei test, dichiarati in astro/raw_decode.hpp):
//   raw_decode_hooks::zlib_memory_limit  std::function<std::uint64_t(std::uint64_t limit)>: riceve il tetto per flusso
//                                        su cui il prodotto conta i byte di zalloc (65 536, §5) e restituisce quello su cui
//                                        decide; chiamato prima di ogni flusso.
//   raw_decode_hooks::after_segment      std::function<void(std::size_t segment, std::uint64_t zlib_peak_bytes)>: dopo
//                                        ogni segmento decodificato (inflate, predittore, copia nell'area importata), con
//                                        il massimo dei byte che zlib ha tenuto allocati per quel flusso.
// Valori attesi: dal contratto (65 536, stop e codici); il picco misurato si verifica in modo indipendente dal valore
// riportato con il tetto stesso: con tetto = picco la decodifica riesce, con tetto = picco − 1 si ferma.
// Limite: il tetto si osserva solo con il gancio montato; con la zlib reale il picco (7 160 byte, §14) resta sotto 65 536,
// quindi senza gancio nessun input fa scattare il tetto e un prodotto che lo applicasse solo col gancio non si vedrebbe.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <zlib.h>
#include <string>
#include <vector>

#include "astro/cancel.hpp"
#include "astro/error.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "dng_u13b_support.hpp"

namespace {

using namespace u13b;

constexpr std::uint64_t zlib_cap = 65536;

struct zlib_run {
  observed o;
  std::vector<std::uint64_t> limits_seen;
  std::vector<std::pair<std::size_t, std::uint64_t>> segments;  // (indice, picco) dagli avvisi dopo ogni segmento
};

zlib_run run_with_cap(const bytes_t& bytes, std::optional<std::uint64_t> cap) {
  zlib_run z;
  z.o = run(bytes, {}, [&](raw_decode_hooks& h) {
    h.zlib_memory_limit = [&z, cap](std::uint64_t limit) {
      z.limits_seen.push_back(limit);
      return cap.value_or(limit);
    };
    h.after_segment = [&z](std::size_t segment, std::uint64_t peak) { z.segments.emplace_back(segment, peak); };
  });
  return z;
}

std::vector<std::pair<std::string, fx::dng32_spec>> zlib_cases() {
  std::vector<std::pair<std::string, fx::dng32_spec>> out;
  for (const std::size_t li : {std::size_t{1}, std::size_t{4}, std::size_t{5}}) {
    fx::dng32_spec s = spec32(2, layouts()[li]);
    set_area(s, 1, 1);
    out.emplace_back(layouts()[li].name, s);
  }
  return out;
}

TEST(DngZlibMemory, T_U13_04_zlib_cap_injected_at_1024_stops_with_too_big) {
  for (const auto& [name, s] : zlib_cases()) {
    SCOPED_TRACE(name);
    const zlib_run z = run_with_cap(fx::make_deflate_dng(s), 1024);
    expect_stop(z.o, error_category::limit_exceeded, "raw.too_big");
    if (z.o.stop) {
      // §5: `funzione` è la chiamata che ha restituito Z_MEM_ERROR; col tetto a 1024 il rifiuto arriva sullo stato
      // iniziale, cioè in inflateInit (zlib 1.3.1 alloca una sola volta lo stato, §14).
      EXPECT_EQ(ctx(*z.o.stop, "funzione"), std::string("inflateInit")) << z.o.stop->what();
      EXPECT_EQ(ctx(*z.o.stop, "codice"), std::to_string(Z_MEM_ERROR)) << z.o.stop->what();
      EXPECT_EQ(ctx(*z.o.stop, "segmento"), std::string("0")) << z.o.stop->what();
      EXPECT_EQ(ctx(*z.o.stop, "rifiuto"), std::string("tetto")) << z.o.stop->what();
    }
    ASSERT_FALSE(z.limits_seen.empty()) << "gancio del tetto mai chiamato";
    EXPECT_EQ(z.limits_seen.front(), zlib_cap) << "tetto del prodotto diverso da 65 536 byte per flusso";
    EXPECT_TRUE(z.segments.empty()) << "nessun segmento completato col tetto a 1024";
  }
}

TEST(DngZlibMemory, T_U13_04_zlib_peak_is_measured_within_the_cap_and_exact) {
  for (const auto& [name, s] : zlib_cases()) {
    SCOPED_TRACE(name);
    const bytes_t bytes = fx::make_deflate_dng(s);
    const zlib_run z = run_with_cap(bytes, std::nullopt);
    expect_project_image(z.o, s);
    ASSERT_EQ(z.segments.size(), segment_count(s));
    EXPECT_EQ(z.limits_seen.size(), segment_count(s)) << "un tetto per flusso";
    for (const std::uint64_t limit : z.limits_seen) EXPECT_EQ(limit, zlib_cap);
    std::uint64_t peak = 0;
    for (std::size_t k = 0; k < z.segments.size(); ++k) {
      EXPECT_EQ(z.segments[k].first, k) << "avvisi fuori ordine";
      EXPECT_GT(z.segments[k].second, 0u);
      EXPECT_LE(z.segments[k].second, zlib_cap);
      peak = std::max(peak, z.segments[k].second);
    }
    ::testing::Test::RecordProperty("zlib_peak_bytes_" + std::to_string(segment_count(s)) + "_segmenti",
                                    std::to_string(peak));
    std::printf("%s: picco delle allocazioni di zlib %llu byte (tetto %llu)\n", name.c_str(),
                static_cast<unsigned long long>(peak), static_cast<unsigned long long>(zlib_cap));
    if (peak == 0) continue;
    {
      SCOPED_TRACE("tetto uguale al picco riportato: immagine");
      expect_project_image(run_with_cap(bytes, peak).o, s);
    }
    {
      SCOPED_TRACE("tetto di un byte sotto il picco riportato: raw.too_big");
      expect_stop(run_with_cap(bytes, peak - 1).o, error_category::limit_exceeded, "raw.too_big");
    }
  }
}

// §5, «memoria negata dal sistema»: il gancio fa negare dal sistema la prima allocazione di zlib del flusso del segmento
// `k` (dopo k segmenti completati). Atteso: lo stesso stop del tetto, limit_exceeded raw.too_big, `funzione` =
// inflateInit (la prima allocazione del flusso è lo stato iniziale), `codice` = Z_MEM_ERROR, `segmento` = k, `rifiuto` =
// sistema; nessuna eccezione diversa da stop_error (mai std::bad_alloc). Il tetto non scatta: il gancio vede solo
// richieste entro 65 536 byte.
TEST(DngZlibMemory, T_U13_04_zlib_allocation_refused_by_system_stops_with_too_big) {
  for (const auto& [name, s] : zlib_cases()) {
    const bytes_t bytes = fx::make_deflate_dng(s);
    const std::size_t n = static_cast<std::size_t>(segment_count(s));
    {
      SCOPED_TRACE(name + ": gancio montato che non nega mai: immagine");
      std::size_t calls = 0;
      observed o;
      EXPECT_NO_THROW(o = run(bytes, {}, [&](raw_decode_hooks& h) {
        h.zlib_system_refuses = [&calls](std::size_t) {
          ++calls;
          return false;
        };
      }));
      expect_project_image(o, s);
      EXPECT_GE(calls, n) << "gancio non chiamato a ogni flusso";
    }
    for (const std::size_t k : {std::size_t{0}, n - 1}) {
      SCOPED_TRACE(name + ": sistema che nega dal segmento " + std::to_string(k) + " di " + std::to_string(n));
      std::size_t done = 0;
      std::vector<std::size_t> requested;
      observed o;
      EXPECT_NO_THROW(o = run(bytes, {}, [&](raw_decode_hooks& h) {
        h.after_segment = [&done](std::size_t, std::uint64_t) { ++done; };
        h.zlib_system_refuses = [&done, &requested, k](std::size_t bytes_requested) {
          requested.push_back(bytes_requested);
          return done == k;
        };
      })) << "eccezione diversa da stop_error";
      expect_stop(o, error_category::limit_exceeded, "raw.too_big");
      if (o.stop) {
        EXPECT_EQ(ctx(*o.stop, "funzione"), std::string("inflateInit")) << o.stop->what();
        EXPECT_EQ(ctx(*o.stop, "codice"), std::to_string(Z_MEM_ERROR)) << o.stop->what();
        EXPECT_EQ(ctx(*o.stop, "segmento"), std::to_string(k)) << o.stop->what();
        EXPECT_EQ(ctx(*o.stop, "rifiuto"), std::string("sistema")) << o.stop->what();
        EXPECT_EQ(ctx(*o.stop, "tetto"), std::to_string(zlib_cap)) << o.stop->what();
      }
      EXPECT_EQ(done, k) << "segmenti completati prima del rifiuto";
      ASSERT_FALSE(requested.empty()) << "gancio del sistema mai chiamato";
      for (const std::size_t b : requested) {
        EXPECT_GT(b, 0u);
        EXPECT_LE(b, zlib_cap) << "il gancio del sistema ha visto una richiesta oltre il tetto";
      }
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// T-U13-09: cancel richiesto dal gancio dopo il segmento k di n: cancelled, raw.cancelled, nessun segmento dopo k.
// ---------------------------------------------------------------------------------------------------------------------

TEST(DngCancel, T_U13_09_cancel_after_segment_k_stops_before_segment_k_plus_1) {
  for (const std::size_t li : {std::size_t{0}, std::size_t{4}}) {
    fx::dng32_spec s = spec32(34893, layouts()[li]);
    set_area(s, 1, 0);
    const bytes_t bytes = fx::make_deflate_dng(s, fx::tiff_order::mm);
    const std::size_t n = static_cast<std::size_t>(segment_count(s));
    ASSERT_GE(n, 3u);
    {
      SCOPED_TRACE(std::string(layouts()[li].name) + ": controllo senza cancel");
      std::vector<std::size_t> seen;
      const observed o = run(bytes, {}, [&](raw_decode_hooks& h) {
        h.after_segment = [&](std::size_t k, std::uint64_t) { seen.push_back(k); };
      });
      expect_project_image(o, s);
      ASSERT_EQ(seen.size(), n);
      for (std::size_t k = 0; k < n; ++k) EXPECT_EQ(seen[k], k);
    }
    for (const std::size_t k : {std::size_t{0}, std::size_t{1}, n - 2}) {
      SCOPED_TRACE(std::string(layouts()[li].name) + ": cancel dopo il segmento " + std::to_string(k) + " di " +
                   std::to_string(n));
      memory_pool pool(std::uint64_t{1} << 30);
      cancel_token token;
      raw_decode_context c;
      c.pool = &pool;
      c.cancel = &token;
      std::vector<std::size_t> seen;
      const observed o = run(bytes, c, [&](raw_decode_hooks& h) {
        h.after_segment = [&](std::size_t segment, std::uint64_t) {
          seen.push_back(segment);
          if (segment == k) token.request();
        };
      });
      expect_stop(o, error_category::cancelled, "raw.cancelled");
      EXPECT_EQ(seen.size(), k + 1) << "segmenti decodificati dopo il cancel";
      EXPECT_EQ(static_cast<std::size_t>(o.inflate_calls), k + 1) << "inflate dopo il cancel";
      EXPECT_EQ(pool.reserved_bytes(), 0u) << "prenotazione non rilasciata dopo il cancel";
    }
  }
}

}  // namespace
