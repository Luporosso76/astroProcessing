// U13b (M3.a): T-U13-10, creazioni di thread nella decodifica del decoder DNG del progetto (docs/contracts/U13-dng-decoder.md
// §7, regola sui thread di ADR-08.b §2.7), come T-M3A-RAW-08 (raw_threads.cpp): l'eseguibile definisce `pthread_create`,
// che conta le creazioni e inoltra alla funzione reale (`dlsym(RTLD_NEXT)`). Un processo nuovo per campione:
//   --product <strips|tiles>  (a) decodifica del prodotto su un DNG deflate a 32 bit (FIX-SYN-DNG-02, Predictor 2 da
//                             libtiff): l'intervallo va dall'avviso dell'instradamento (`after_route`, decoder del progetto)
//                             alla fine della decodifica e comprende ogni `inflate`. Attese: immagine, 0 creazioni.
//   --control                 (c) thread proprio, nessuna decodifica: almeno una creazione, altrimenti esce con errore.
// Ultima riga: CHECK-RESULT ok | defect | tool-error (protocollo di tools/mutate.sh).
#include <dlfcn.h>
#include <pthread.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "astro/error.hpp"
#include "astro/raw_decode.hpp"
#include "syn_dng.hpp"
#include "syn_dng_deflate.hpp"

namespace {

std::atomic<std::uint64_t> g_creations{0};

std::uint64_t creations() { return g_creations.load(std::memory_order_relaxed); }

// 2048 × 1040 come il campione sintetico di T-M3A-RAW-08; strisce di 16 righe o tasselli 256 × 256.
bool load_sample(const std::string& sample, std::vector<std::uint8_t>& bytes) {
  namespace fx = astro_fixture;
  if (sample != "strips" && sample != "tiles") {
    std::fprintf(stderr, "campione non valido: %s\n", sample.c_str());
    return false;
  }
  fx::dng32_spec s;
  s.width = 2048;
  s.height = 1040;
  s.predictor = 2;
  s.tiles = sample == "tiles";
  s.tile_width = 256;
  s.tile_length = 256;
  s.rows_per_strip = 16;
  s.active_area = fx::dng_active_area{0, 0, s.height, s.width};
  try {
    bytes = fx::make_deflate_dng(s);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "fixture non costruita: %s\n", e.what());
    return false;
  }
  return true;
}

int run_product(const std::vector<std::uint8_t>& bytes) {
  using namespace astro;
  std::optional<std::uint64_t> start;
  int inflates = 0;
  bool project = false;
  raw_decode_hooks hooks;
  hooks.after_route = [&](const dng_route_result& r) {
    project = r.decoder == raw_decoder_kind::astro_dng;
    start = creations();
  };
  hooks.inflate_return = [&](std::size_t, int rc) {
    ++inflates;
    return rc;
  };
  std::string outcome = "immagine";
  bool image_made = false;
  try {
    const image img = decode_raw_buffer(bytes, hooks, {});
    image_made = img.decoder_state.has_value();
  } catch (const stop_error& e) {
    outcome = e.code();
  } catch (const std::exception& e) {
    outcome = std::string("eccezione non di stop: ") + e.what();
  }
  const std::uint64_t end = creations();
  int rc = 0;
  if (!start || !project) {
    std::fputs("FALLITO: l'instradamento non ha scelto il decoder del progetto\n", stderr);
    rc = 1;
  }
  if (!image_made) {
    std::fprintf(stderr, "FALLITO: attesa un'immagine, esito %s\n", outcome.c_str());
    rc = 1;
  }
  const unsigned long long created = start ? static_cast<unsigned long long>(end - *start) : 0ull;
  std::printf("(a) esito %s; %d inflate; decodifica del progetto %s%llu creazioni\n", outcome.c_str(), inflates,
              start ? "" : "non eseguita ", created);
  if (start && end != *start) {
    std::fputs("FALLITO: creazioni di thread nella decodifica del progetto\n", stderr);
    rc = 1;
  }
  return rc;
}

int run_control() {
  const std::uint64_t before = creations();
  int sum = 0;
  std::thread worker([&sum] { sum += 1; });
  worker.join();
  const std::uint64_t created = creations() - before;
  std::printf("(c) thread proprio: %d thread, %llu creazioni\n", sum, static_cast<unsigned long long>(created));
  if (created < 1) {
    std::fputs("FALLITO: l'interposizione non vede la creazione dei thread con pthread_create\n", stderr);
    return 1;
  }
  return 0;
}

}  // namespace

extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg) {
  using real_fn = int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
  static const real_fn real = reinterpret_cast<real_fn>(::dlsym(RTLD_NEXT, "pthread_create"));
  g_creations.fetch_add(1, std::memory_order_relaxed);
  return real(thread, attr, start, arg);
}

int finish(int rc, bool defect_on_failure) {
  std::fflush(stderr);
  std::puts(rc == 0 ? "CHECK-RESULT: ok" : defect_on_failure && rc == 1 ? "CHECK-RESULT: defect" : "CHECK-RESULT: tool-error");
  std::fflush(stdout);
  return rc;
}

int main(int argc, char** argv) {
  // ADR-08.b §2.6: TZ=UTC0 prima di ogni thread.
  if (setenv("TZ", "UTC0", 1) != 0) {
    std::fputs("setenv(TZ) fallita\n", stderr);
    return finish(2, false);
  }
  tzset();
  if (argc == 2 && std::strcmp(argv[1], "--control") == 0) return finish(run_control(), false);
  if (argc == 3 && std::strcmp(argv[1], "--product") == 0) {
    std::vector<std::uint8_t> bytes;
    if (!load_sample(argv[2], bytes)) return finish(2, false);
    return finish(run_product(bytes), true);
  }
  std::fputs("uso: raw_threads_dng --product <strips|tiles> | --control\n", stderr);
  return finish(2, false);
}
