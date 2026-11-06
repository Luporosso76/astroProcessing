// U13b (M3.a): T-U13-11, fuzz del decoder DNG del progetto con seed fisso (docs/contracts/U13-dng-decoder.md §3-§6, §12).
// Per ogni file seme 5 000 mutazioni (parametro --mutations solo per le prove dell'eseguibile; il contratto vale col
// valore predefinito), build dev con ASan e UBSan. Ogni input: immagine valida o stop_error con categoria; qualunque
// altra eccezione, prenotazione non rilasciata o immagine non valida è un fallimento; un crash o un abort termina il
// processo (ctest lo registra). Il seme non mutato deve dare il suo esito atteso (immagine per i semi deflate,
// `raw.dng_nonconformant` per il DNG 52546 con i soli tag): altrimenti le mutazioni non proverebbero il decoder del
// progetto, e l'esecuzione è un difetto anche con zero fallimenti nelle mutazioni (che si eseguono comunque).
// Semi (FIX-SYN-DNG-02): `strips` (deflate, strisce di 7 righe, Predictor 2 da libtiff, II), `tiles` (tasselli 16 × 16,
// Predictor 34893, MM), `tiles256` (un tassello 256 × 256 con bordi riempiti, Predictor 1, II), `jxl` (Compression
// 52546, CFA, soli tag). Seed delle mutazioni: kBaseSeed + indice del seme; operazioni uguali a fuzz_raw_decode.cpp
// (T-M3A-RAW-10), su interi grezzi di std::mt19937_64.
// Un processo per seme, un solo thread: al massimo 4 processi in parallelo stanno nel 70% delle CPU.
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "astro/error.hpp"
#include "astro/image.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "syn_dng.hpp"
#include "syn_dng_deflate.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;

constexpr std::uint64_t kBaseSeed = 0xD06F0011ull;
constexpr unsigned kMutationsPerSeed = 5000;
constexpr std::uint64_t kPoolBytes = 1ull << 30;

struct seed_file {
  std::string name;
  std::vector<std::uint8_t> bytes;
  const char* expected_stop;  // nullptr: il seme non mutato dà un'immagine
  std::uint64_t index;
};

std::optional<seed_file> make_seed(const std::string& name) {
  fx::dng32_spec s;
  s.width = 37;
  s.height = 29;
  s.active_area = fx::dng_active_area{1, 1, 28, 36};
  if (name == "strips") {
    s.tiles = false;
    s.rows_per_strip = 7;
    s.predictor = 2;
    return seed_file{name, fx::make_deflate_dng(s, fx::tiff_order::ii), nullptr, 0};
  }
  if (name == "tiles") {
    s.predictor = 34893;
    return seed_file{name, fx::make_deflate_dng(s, fx::tiff_order::mm), nullptr, 1};
  }
  if (name == "tiles256") {
    s.tile_width = 256;
    s.tile_length = 256;
    return seed_file{name, fx::make_deflate_dng(s, fx::tiff_order::ii), nullptr, 2};
  }
  if (name == "jxl") {
    s.bits = 16;
    s.compression = 52546;
    s.white = 65535;
    s.backward_version = std::array<std::uint8_t, 4>{1, 7, 0, 0};  // p. 125
    fx::tiff_file f;
    f.chain.push_back(fx::make_dng32_ifd(s, fx::tiff_order::ii, [](const std::vector<std::uint8_t>& b) { return b; }));
    return seed_file{name, fx::write_tiff(f).bytes, "raw.dng_nonconformant", 3};
  }
  return std::nullopt;
}

// Posizione: metà dei casi nei primi/ultimi 8 KiB (intestazioni e directory), il resto su tutto il file.
std::size_t pick_position(std::mt19937_64& rng, std::size_t size) {
  if (size == 0) return 0;
  const std::uint64_t r = rng();
  const std::size_t window = std::min<std::size_t>(size, 8192);
  switch (r & 3) {
    case 0:
      return static_cast<std::size_t>((r >> 8) % window);
    case 1:
      return size - 1 - static_cast<std::size_t>((r >> 8) % window);
    default:
      return static_cast<std::size_t>((r >> 8) % size);
  }
}

void mutate(std::vector<std::uint8_t>& b, std::mt19937_64& rng) {
  const unsigned rounds = 1 + static_cast<unsigned>(rng() % 3);
  for (unsigned round = 0; round < rounds && !b.empty(); ++round) {
    switch (rng() % 8) {
      case 0: {  // bit flip
        const unsigned n = 1 + static_cast<unsigned>(rng() % 8);
        for (unsigned i = 0; i < n; ++i) b[pick_position(rng, b.size())] ^= static_cast<std::uint8_t>(1u << (rng() % 8));
        break;
      }
      case 1: {  // byte casuali
        const unsigned n = 1 + static_cast<unsigned>(rng() % 16);
        for (unsigned i = 0; i < n; ++i) b[pick_position(rng, b.size())] = static_cast<std::uint8_t>(rng() & 0xFF);
        break;
      }
      case 2: {  // parola da 32 bit con un valore di confine
        if (b.size() < 4) break;
        static const std::uint32_t values[] = {0u, 1u, 0xFFFFu, 0x10000u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0xFFFFFFFEu};
        const std::size_t pos = std::min(pick_position(rng, b.size()), b.size() - 4);
        const std::uint32_t v = values[rng() % 8];
        std::memcpy(&b[pos], &v, 4);
        break;
      }
      case 3: {  // parola da 16 bit
        if (b.size() < 2) break;
        const std::size_t pos = std::min(pick_position(rng, b.size()), b.size() - 2);
        const std::uint16_t v = static_cast<std::uint16_t>(rng() & 0xFFFF);
        std::memcpy(&b[pos], &v, 2);
        break;
      }
      case 4:  // troncamento
        b.resize(static_cast<std::size_t>(rng() % (b.size() + 1)));
        break;
      case 5: {  // cancellazione di un tratto
        const std::size_t pos = pick_position(rng, b.size());
        const std::size_t n = std::min<std::size_t>(1 + rng() % 256, b.size() - pos);
        b.erase(b.begin() + static_cast<std::ptrdiff_t>(pos), b.begin() + static_cast<std::ptrdiff_t>(pos + n));
        break;
      }
      case 6: {  // inserzione di byte casuali
        const std::size_t pos = pick_position(rng, b.size());
        const std::size_t n = 1 + rng() % 256;
        std::vector<std::uint8_t> ins(n);
        for (auto& v : ins) v = static_cast<std::uint8_t>(rng() & 0xFF);
        b.insert(b.begin() + static_cast<std::ptrdiff_t>(pos), ins.begin(), ins.end());
        break;
      }
      default: {  // copia di un tratto su un altro
        const std::size_t from = pick_position(rng, b.size());
        const std::size_t to = pick_position(rng, b.size());
        const std::size_t n = std::min<std::size_t>({1 + rng() % 512, b.size() - from, b.size() - to});
        std::memmove(&b[to], &b[from], n);
        break;
      }
    }
  }
}

struct attempt_result {
  bool image = false;
  std::string stop;     // "<categoria> <codice>"
  std::string code;     // codice dello stop
  std::string failure;  // eccezione diversa da stop_error, stop senza codice o immagine non valida
  bool project = false;  // l'instradamento ha scelto il decoder del progetto
};

attempt_result attempt(const std::vector<std::uint8_t>& bytes, memory_pool& pool) {
  raw_decode_context ctx;
  ctx.pool = &pool;
  raw_decode_hooks hooks;
  attempt_result out;
  hooks.after_route = [&out](const dng_route_result& r) { out.project = r.decoder == raw_decoder_kind::astro_dng; };
  std::optional<image> img;
  try {
    img.emplace(decode_raw_buffer(bytes, hooks, ctx));
  } catch (const stop_error& e) {
    if (e.code().empty()) {
      out.failure = "stop senza codice";
      return out;
    }
    out.stop = std::string(to_string(e.category())) + " " + e.code();
    out.code = e.code();
    return out;
  } catch (const std::exception& e) {
    out.failure = std::string("eccezione non di stop: ") + e.what();
    return out;
  } catch (...) {
    out.failure = "eccezione sconosciuta";
    return out;
  }
  try {
    validate_image(*img);
  } catch (const std::exception& e) {
    out.failure = std::string("immagine non valida: ") + e.what();
    return out;
  }
  out.image = true;
  return out;
}

struct outcome {
  std::uint64_t images = 0;
  std::uint64_t project = 0;
  std::map<std::string, std::uint64_t> stops;
  std::uint64_t failures = 0;
  double max_seconds = 0;
};

void run_seed(const seed_file& seed, unsigned mutations, outcome& out) {
  std::mt19937_64 rng(kBaseSeed + seed.index);
  std::vector<std::uint8_t> work;
  for (unsigned i = 0; i < mutations; ++i) {
    work = seed.bytes;
    mutate(work, rng);
    memory_pool pool(kPoolBytes);
    const auto t0 = std::chrono::steady_clock::now();
    const attempt_result r = attempt(work, pool);
    if (r.image) ++out.images;
    if (r.project) ++out.project;
    if (!r.stop.empty()) ++out.stops[r.stop];
    if (!r.failure.empty()) {
      std::fprintf(stderr, "FALLITO %s mutazione %u: %s\n", seed.name.c_str(), i, r.failure.c_str());
      ++out.failures;
    }
    if (pool.reserved_bytes() != 0) {
      std::fprintf(stderr, "FALLITO %s mutazione %u: prenotazione non rilasciata (%llu byte)\n", seed.name.c_str(), i,
                   static_cast<unsigned long long>(pool.reserved_bytes()));
      ++out.failures;
    }
    out.max_seconds =
        std::max(out.max_seconds, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
  }
}

bool parse_count(const char* text, unsigned& out) {
  if (*text == '\0' || *text == '-' || *text == '+') return false;
  char* end = nullptr;
  errno = 0;
  const unsigned long value = std::strtoul(text, &end, 10);
  if (errno != 0 || *end != '\0' || value == 0 || value > 1000000) return false;
  out = static_cast<unsigned>(value);
  return true;
}

int finish(int rc, bool defect) {
  std::fflush(stderr);
  std::puts(rc == 0 ? "CHECK-RESULT: ok" : defect ? "CHECK-RESULT: defect" : "CHECK-RESULT: tool-error");
  std::fflush(stdout);
  return rc;
}

int usage() {
  std::fputs("uso: fuzz_dng_decode --seed <strips|tiles|tiles256|jxl> [--mutations N]\n", stderr);
  return finish(2, false);
}

int run_main(int argc, char** argv) {
  std::string name;
  unsigned mutations = kMutationsPerSeed;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--seed" && i + 1 < argc) {
      name = argv[++i];
    } else if (a == "--mutations" && i + 1 < argc) {
      if (!parse_count(argv[++i], mutations)) return usage();
    } else {
      return usage();
    }
  }
  std::optional<seed_file> seed;
  try {
    seed = make_seed(name);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "fixture non costruita: %s\n", e.what());
    return finish(2, false);
  }
  if (!seed) return usage();

  bool seed_as_expected = false;
  {
    memory_pool pool(kPoolBytes);
    const attempt_result r = attempt(seed->bytes, pool);
    seed_as_expected = r.failure.empty() && r.project &&
                       (seed->expected_stop == nullptr ? r.image : r.code == seed->expected_stop);
    std::printf("seme %s non mutato: %s\n", seed->name.c_str(),
                r.image ? "immagine" : !r.stop.empty() ? r.stop.c_str() : r.failure.c_str());
    if (!seed_as_expected)
      std::fprintf(stderr, "FALLITO: il seme %s non mutato non dà l'esito atteso (%s) sul decoder del progetto\n",
                   seed->name.c_str(), seed->expected_stop == nullptr ? "immagine" : seed->expected_stop);
  }

  outcome one;
  run_seed(*seed, mutations, one);
  std::uint64_t stops = 0;
  for (const auto& kv : one.stops) stops += kv.second;
  std::printf("seme %s: %u mutazioni, %llu al decoder del progetto, %llu immagini, %llu stop, %llu fallimenti, massimo %.3f s\n",
              seed->name.c_str(), mutations, static_cast<unsigned long long>(one.project),
              static_cast<unsigned long long>(one.images), static_cast<unsigned long long>(stops),
              static_cast<unsigned long long>(one.failures), one.max_seconds);
  for (const auto& [k, v] : one.stops) std::printf("  %llu x %s\n", static_cast<unsigned long long>(v), k.c_str());
  const bool defect = one.failures != 0 || !seed_as_expected;
  return finish(defect ? 1 : 0, defect);
}

}  // namespace

int main(int argc, char** argv) {
  // ADR-08.b §2.6: TZ=UTC0 prima di ogni thread.
  if (setenv("TZ", "UTC0", 1) != 0) {
    std::fputs("setenv(TZ) fallita\n", stderr);
    return finish(2, false);
  }
  tzset();
  try {
    return run_main(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "errore dello strumento: %s\n", e.what());
    return finish(2, false);
  }
}
