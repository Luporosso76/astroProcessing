// U3 (M3.a): T-M3A-RAW-08, creazioni di thread negli intervalli open_buffer e unpack (H6), ADR-08.b §2.7, §6.
// Un processo nuovo per campione e per fase, perché libgomp riusa i thread di una regione precedente:
//   --product <campione>  (a) decodifica del prodotto con omp_set_num_threads(4) preimpostato dal chiamante:
//                         esito atteso per campione (expected_for): immagine, oppure lo stop del contratto; ganci presenti
//                         e in ordine; 0 creazioni negli intervalli attraversati (un intervallo non eseguito, come unpack
//                         dopo `raw.xtrans`, è dichiarato «non eseguito» e non è una prova); omp_get_max_threads() == 1
//                         prima di open_buffer;
//   --libraw2 <campione>  (b) stessa decodifica con LibRaw diretta e omp_set_num_threads(2): solo registrata, stampa
//                         «H6 provata» (almeno una creazione) o «H6 non provata», esce 0 in entrambi i casi;
//   --control             (c) regione parallela propria a due thread, nessuna decodifica: almeno una creazione, altrimenti
//                         esce con errore. Prova solo che l'interposizione vede i thread di libgomp, non H6.
// <campione>: `syn` (DNG sintetico 2048x1040) oppure l'indice 0..7 di PUB-RAW-01 (hash verificato, mai un salto).
// Nessuna regione OpenMP a più thread precede (b): nel processo di (b) il test non apre regioni proprie.
#include <dlfcn.h>
#include <libraw/libraw.h>
#include <omp.h>
#include <pthread.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "astro/error.hpp"
#include "astro/raw_decode.hpp"
#include "pub_raw_01.hpp"
#include "syn_dng.hpp"

namespace {

std::atomic<std::uint64_t> g_creations{0};

std::uint64_t creations() { return g_creations.load(std::memory_order_relaxed); }

bool load_sample(const std::string& sample, std::vector<std::uint8_t>& bytes) {
  if (sample == "syn") {
    namespace fx = astro_fixture;
    fx::dng_spec s;
    s.width = 2048;
    s.height = 1040;
    s.white = 4000;
    std::vector<std::uint16_t> v(2048u * 1040u);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>((i * 7) % 4000);
    s.pixels = fx::pixels_u16(v);
    bytes = fx::make_dng(s);
    return true;
  }
  char* end = nullptr;
  const unsigned long index = std::strtoul(sample.c_str(), &end, 10);
  if (sample.empty() || *end != '\0') {
    std::fprintf(stderr, "campione non valido: %s\n", sample.c_str());
    return false;
  }
  std::string name, error;
  if (!pub_raw::load(index, name, bytes, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return false;
  }
  return true;
}

// Esito atteso della decodifica del prodotto per campione (ADR-08.b §2.3, §2.5, §6 RAW-09): `ok`, oppure uno stop con
// categoria e codice; `unpack_reached` dice se il flusso attraversa `unpack` (e quindi l'intervallo si misura).
struct expectation {
  const char* stop_code;  // nullptr: la decodifica riesce
  astro::error_category category;
  bool unpack_reached;
};

expectation expected_for(const std::string& sample) {
  using astro::error_category;
  if (sample == "7") return {"raw.xtrans", error_category::unsupported, false};      // X-E1: stop prima di unpack
  // X-A1 (indice 3): decodifica riuscita con PARSEFUJI_PROCESSED (0x10000) registrato, ammesso da §2.5.
  return {nullptr, error_category::invalid_input, true};
}

int run_product(const std::vector<std::uint8_t>& bytes, const std::string& sample) {
  using namespace astro;
  const expectation expect = expected_for(sample);
  omp_set_num_threads(4);
  std::vector<std::string> order;
  std::uint64_t open_start = 0, open_end = 0, unpack_start = 0, unpack_end = 0;
  int before_open = -1, after_open = -1, before_unpack = -1;
  raw_decode_hooks hooks;
  hooks.before_open = [&](raw_user_params&) {
    order.emplace_back("before_open");
    open_start = creations();
    before_open = omp_get_max_threads();
  };
  hooks.after_open = [&](raw_probe_fields&) {
    order.emplace_back("after_open");
    open_end = creations();
    after_open = omp_get_max_threads();
  };
  hooks.before_unpack = [&] {
    order.emplace_back("before_unpack");
    unpack_start = creations();
    before_unpack = omp_get_max_threads();
  };
  hooks.after_unpack = [&](raw_unpack_fields&) {
    order.emplace_back("after_unpack");
    unpack_end = creations();
  };
  std::string outcome = "immagine";
  bool stopped = false, stop_matches = false;
  int rc = 0;
  try {
    const image img = decode_raw_buffer(bytes, hooks, {});
    if (sample == "3" && !(img.decoder_state.has_value() && (img.decoder_state->process_warnings & 0x10000u) != 0)) {
      std::fputs("FALLITO: X-A1 senza PARSEFUJI_PROCESSED (0x10000) registrato\n", stderr);
      rc = 1;
    }
    if (expect.stop_code != nullptr) {
      std::fprintf(stderr, "FALLITO: atteso lo stop %s, la decodifica è riuscita\n", expect.stop_code);
      rc = 1;
    }
  } catch (const stop_error& e) {
    outcome = e.code();
    stopped = true;
    stop_matches = expect.stop_code != nullptr && e.code() == expect.stop_code && e.category() == expect.category;
  }
  if (stopped && !stop_matches) {
    std::fprintf(stderr, "FALLITO: stop non atteso: %s\n", outcome.c_str());
    rc = 1;
  }
  const std::vector<std::string> full = {"before_open", "after_open", "before_unpack", "after_unpack"};
  const std::vector<std::string> open_only = {"before_open", "after_open"};
  if (order != (expect.unpack_reached ? full : open_only)) {
    std::fputs("FALLITO: ganci mancanti o fuori ordine\n", stderr);
    rc = 1;
  }
  const bool open_measured = order.size() >= 2;
  const bool unpack_measured = order.size() == 4;
  std::printf("(a) esito %s; open_buffer %s%llu, unpack %s%llu creazioni; omp_get_max_threads prima di open %d, dopo open %d, prima di unpack %d\n",
              outcome.c_str(), open_measured ? "" : "non eseguito ", open_measured ? static_cast<unsigned long long>(open_end - open_start) : 0ull,
              unpack_measured ? "" : "non eseguito ", unpack_measured ? static_cast<unsigned long long>(unpack_end - unpack_start) : 0ull,
              before_open, after_open, before_unpack);
  if (before_open != 1 || after_open != 1 || (unpack_measured && before_unpack != 1)) {
    std::fputs("FALLITO: omp_set_num_threads(1) va chiamata nel thread del chiamante prima di open_buffer\n", stderr);
    rc = 1;
  }
  if ((open_measured && open_end != open_start) || (unpack_measured && unpack_end != unpack_start)) {
    std::fputs("FALLITO: creazioni di thread nella decodifica del prodotto\n", stderr);
    rc = 1;
  }
  return rc;
}

int run_libraw2(const std::vector<std::uint8_t>& bytes) {
  omp_set_num_threads(2);
  LibRaw lr;
  const std::uint64_t t0 = creations();
  const int rc_open = lr.open_buffer(bytes.data(), bytes.size());
  const std::uint64_t t1 = creations();
  const int rc_unpack = rc_open == LIBRAW_SUCCESS ? lr.unpack() : rc_open;
  const std::uint64_t t2 = creations();
  if (rc_open != LIBRAW_SUCCESS || rc_unpack != LIBRAW_SUCCESS) {
    std::fprintf(stderr, "LibRaw diretta: open_buffer %d, unpack %d\n", rc_open, rc_unpack);
    return 1;
  }
  const bool proven = (t1 - t0) + (t2 - t1) >= 1;
  std::printf("(b) open_buffer %llu, unpack %llu creazioni con due thread OpenMP: %s\n",
              static_cast<unsigned long long>(t1 - t0), static_cast<unsigned long long>(t2 - t1),
              proven ? "H6 provata (a condizione che (a) dia 0)" : "H6 non provata");
  return 0;
}

int run_control() {
  omp_set_num_threads(2);
  const std::uint64_t before = creations();
  int sum = 0;
#pragma omp parallel reduction(+ : sum)
  { sum += 1; }
  const std::uint64_t created = creations() - before;
  std::printf("(c) regione propria a due thread: %d thread, %llu creazioni\n", sum, static_cast<unsigned long long>(created));
  if (created < 1) {
    std::fputs("FALLITO: l'interposizione non vede la creazione dei thread di libgomp\n", stderr);
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

// Ultima riga di ogni esecuzione: CHECK-RESULT ok | defect | tool-error (protocollo di tools/mutate.sh). Un difetto è
// solo un esito sbagliato della decodifica del prodotto; ogni altro uscita non nulla è dello strumento.
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
  if (argc == 3 && (std::strcmp(argv[1], "--product") == 0 || std::strcmp(argv[1], "--libraw2") == 0)) {
    std::vector<std::uint8_t> bytes;
    if (!load_sample(argv[2], bytes)) return finish(1, false);
    return std::strcmp(argv[1], "--product") == 0 ? finish(run_product(bytes, argv[2]), true)
                                                  : finish(run_libraw2(bytes), false);
  }
  std::fputs("uso: raw_threads (--product|--libraw2) <syn|0..7> | --control\n", stderr);
  return finish(2, false);
}
