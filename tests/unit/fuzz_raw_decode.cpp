// U3 (M3.a): T-M3A-RAW-10, fuzz del decoder RAW con seed fisso (docs/contracts/ADR-08-decoder-pack.md §2.5 H5, §6).
// Per ogni file seme 5 000 mutazioni (parametro --mutations solo per le prove dell'eseguibile; il contratto vale col
// valore predefinito). Ogni input: immagine valida o stop_error con categoria; qualunque altra eccezione, prenotazione non
// rilasciata o immagine non valida è un fallimento; un crash o un abort termina il processo (ctest lo registra).
// Seed: fissi per file seme (kBaseSeed + indice del seme), mutazioni da std::mt19937_64 e operazioni su interi grezzi
// (nessuna distribuzione della libreria standard: la sequenza non dipende dall'implementazione).
// Semi: `--synthetic` le fixture di T-M3A-RAW-01...04 e RAW-03; `--real <indice>` l'indice-esimo file di PUB-RAW-01
// (ordine del record, SHA-256 verificato) sotto $ASTRO_RAW_PIXLS_DIR, sola lettura; con la variabile assente, un file
// mancante o un hash diverso il processo esce con errore.
// Un processo, un solo thread: al massimo 4 processi in parallelo stanno nel 70% delle CPU (ctest -j non oltre 4).
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <tuple>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "astro/error.hpp"
#include "astro/hash.hpp"
#include "pub_raw_01.hpp"
#include "astro/image.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "syn_dng.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;
namespace fs = std::filesystem;

constexpr std::uint64_t kBaseSeed = 0xA57F0010ull;
constexpr unsigned kMutationsPerSeed = 5000;

constexpr std::uint64_t kPoolBytes = 1ull << 30;

struct seed_file {
  std::string name;
  std::vector<std::uint8_t> bytes;
};

fx::dng_spec base_spec() {
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.white = 4000;
  std::vector<std::uint16_t> v(64u * 48u);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>(100 + (i * 37) % 3500);
  s.pixels = fx::pixels_u16(v);
  return s;
}

// Corrispondenza con le fixture di ADR-08.b §6. RAW-01: cfa<p>_origine_<x><y> (quattro pattern x quattro origini).
// RAW-02: nero_scalare, nero_motivo_2x2, nero_non_invariante_<x><y> (64, 72, 80, 96 con origini (1,0), (0,1), (1,1), (2,2)).
// RAW-03: xtrans_6x6, cfa_4x4_non_periodico, linearraw_3_campioni, virgola_mobile_16/24/32, due_immagini.
// RAW-04: linearizzazione_identita_65536, linearizzazione_identita_4096, linearizzazione_4096.
std::vector<seed_file> synthetic_seeds() {
  std::vector<seed_file> out;
  const std::vector<std::vector<std::uint8_t>> patterns = {{0, 1, 1, 2}, {2, 1, 1, 0}, {1, 0, 2, 1}, {1, 2, 0, 1}};
  for (std::size_t p = 0; p < patterns.size(); ++p)
    for (std::uint32_t oy = 0; oy < 2; ++oy)
      for (std::uint32_t ox = 0; ox < 2; ++ox) {
        fx::dng_spec s = base_spec();
        s.cfa = patterns[p];
        s.active_area = fx::dng_active_area{oy, ox, oy + 32, ox + 40};
        out.push_back({"cfa" + std::to_string(p) + "_origine_" + std::to_string(ox) + std::to_string(oy), fx::make_dng(s)});
      }
  {
    fx::dng_spec s = base_spec();
    s.black = {64};
    out.push_back({"nero_scalare", fx::make_dng(s)});
  }
  {
    fx::dng_spec s = base_spec();
    s.black_dim = std::make_pair<std::uint16_t, std::uint16_t>(2, 2);
    s.black = {64, 64, 80, 80};
    s.active_area = fx::dng_active_area{0, 1, 32, 41};
    out.push_back({"nero_motivo_2x2", fx::make_dng(s)});
  }
  for (const auto& [name, x, y] : std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t>>{
           {"10", 1, 0}, {"01", 0, 1}, {"11", 1, 1}, {"22", 2, 2}}) {
    fx::dng_spec s = base_spec();
    s.black_dim = std::make_pair<std::uint16_t, std::uint16_t>(2, 2);
    s.black = {64, 72, 80, 96};
    s.active_area = fx::dng_active_area{y, x, y + 32, x + 40};
    out.push_back({"nero_non_invariante_" + name, fx::make_dng(s)});
  }
  for (const std::size_t entries : {std::size_t{65536}, std::size_t{4096}}) {
    fx::dng_spec s = base_spec();
    s.linearization.resize(entries);
    for (std::size_t i = 0; i < entries; ++i) s.linearization[i] = static_cast<std::uint16_t>(i);
    out.push_back({"linearizzazione_identita_" + std::to_string(entries), fx::make_dng(s)});
  }
  {
    fx::dng_spec s = base_spec();
    s.cfa_rows = 4;
    s.cfa_cols = 4;
    s.cfa = {0, 1, 0, 1, 1, 2, 1, 2, 0, 1, 2, 1, 1, 2, 1, 0};
    out.push_back({"cfa_4x4_non_periodico", fx::make_dng(s)});
  }
  for (const std::uint16_t bits : {std::uint16_t{16}, std::uint16_t{24}}) {
    fx::dng_spec s = base_spec();
    s.bits = bits;
    s.sample_format = 3;
    s.pixels = std::vector<std::uint8_t>(64u * 48u * (bits / 8u), 0x3F);
    out.push_back({"virgola_mobile_" + std::to_string(bits), fx::make_dng(s)});
  }
  {
    fx::dng_spec s = base_spec();
    s.linearization.resize(4096);
    for (std::size_t i = 0; i < s.linearization.size(); ++i) s.linearization[i] = static_cast<std::uint16_t>(i * 3 / 2);
    out.push_back({"linearizzazione_4096", fx::make_dng(s)});
  }
  {
    fx::dng_spec s = base_spec();
    s.cfa_rows = 6;
    s.cfa_cols = 6;
    s.cfa = std::vector<std::uint8_t>(36, 1);
    out.push_back({"xtrans_6x6", fx::make_dng(s)});
  }
  {
    fx::dng_spec s = base_spec();
    s.photometric = 34892;
    s.samples_per_pixel = 3;
    s.pixels = std::vector<std::uint8_t>(64u * 48u * 3u * 2u, 7);
    out.push_back({"linearraw_3_campioni", fx::make_dng(s)});
  }
  {
    fx::dng_spec s = base_spec();
    s.bits = 32;
    s.sample_format = 3;
    s.pixels = std::vector<std::uint8_t>(64u * 48u * 4u, 0x3F);
    out.push_back({"virgola_mobile_32", fx::make_dng(s)});
  }
  {
    out.push_back({"due_immagini", fx::make_dng(std::vector<fx::dng_spec>{base_spec(), base_spec()})});
  }
  return out;
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

struct outcome {
  std::uint64_t images = 0;
  std::map<std::string, std::uint64_t> stops;
  std::uint64_t failures = 0;
  double max_seconds = 0;
};

struct attempt_result {
  bool image = false;
  std::string stop;     // "<categoria> <codice>" se la decodifica si è fermata con stop_error
  std::string failure;  // non vuoto: eccezione diversa da stop_error, stop senza codice o immagine non valida
};

attempt_result attempt(const std::vector<std::uint8_t>& bytes, memory_pool& pool) {
  raw_decode_context ctx;
  ctx.pool = &pool;
  std::optional<image> img;
  try {
    img.emplace(decode_raw_buffer(bytes, {}, ctx));
  } catch (const stop_error& e) {
    if (e.code().empty()) return {false, {}, "stop senza codice"};
    return {false, std::string(to_string(e.category())) + " " + e.code(), {}};
  } catch (const std::exception& e) {
    return {false, {}, std::string("eccezione non di stop: ") + e.what()};
  } catch (...) {
    return {false, {}, "eccezione sconosciuta"};
  }
  try {
    validate_image(*img);
  } catch (const std::exception& e) {
    return {false, {}, std::string("immagine non valida: ") + e.what()};
  } catch (...) {
    return {false, {}, "immagine non valida: eccezione sconosciuta"};
  }
  return {true, {}, {}};
}

void run_seed(const seed_file& seed, std::uint64_t seed_value, unsigned mutations, outcome& out) {
  std::mt19937_64 rng(seed_value);
  std::vector<std::uint8_t> work;
  for (unsigned i = 0; i < mutations; ++i) {
    work = seed.bytes;
    mutate(work, rng);
    memory_pool pool(kPoolBytes);
    const auto t0 = std::chrono::steady_clock::now();
    const attempt_result r = attempt(work, pool);
    if (r.image) ++out.images;
    if (!r.stop.empty()) ++out.stops[r.stop];
    if (!r.failure.empty()) {
      std::fprintf(stderr, "FALLITO %s seed %llu mutazione %u: %s\n", seed.name.c_str(),
                   static_cast<unsigned long long>(seed_value), i, r.failure.c_str());
      ++out.failures;
    }
    if (pool.reserved_bytes() != 0) {
      std::fprintf(stderr, "FALLITO %s seed %llu mutazione %u: prenotazione non rilasciata (%llu byte)\n",
                   seed.name.c_str(), static_cast<unsigned long long>(seed_value), i,
                   static_cast<unsigned long long>(pool.reserved_bytes()));
      ++out.failures;
    }
    out.max_seconds =
        std::max(out.max_seconds, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
  }
}

template <typename T>
bool parse_number(const char* text, unsigned long maximum, T& out) {
  if (*text == '\0' || *text == '-' || *text == '+') return false;
  char* end = nullptr;
  errno = 0;
  const unsigned long value = std::strtoul(text, &end, 10);
  if (errno != 0 || *end != '\0' || value > maximum) return false;
  out = static_cast<T>(value);
  return true;
}

// Ultima riga di ogni esecuzione: CHECK-RESULT ok | defect | tool-error (protocollo di tools/mutate.sh).
int finish(int rc, bool defect) {
  std::fflush(stderr);
  std::puts(rc == 0 ? "CHECK-RESULT: ok" : defect ? "CHECK-RESULT: defect" : "CHECK-RESULT: tool-error");
  std::fflush(stdout);
  return rc;
}

int usage() {
  std::fputs("uso: fuzz_raw_decode (--synthetic | --real <indice>) [--mutations N]\n", stderr);
  return finish(2, false);
}

}  // namespace

int main(int argc, char** argv) {
  // ADR-08.b §2.6: TZ=UTC0 prima di ogni thread.
  if (setenv("TZ", "UTC0", 1) != 0) {
    std::fputs("setenv(TZ) fallita\n", stderr);
    return finish(2, false);
  }
  tzset();
  bool synthetic = false;
  long real_index = -1;
  unsigned mutations = kMutationsPerSeed;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--synthetic") {
      synthetic = true;
    } else if (a == "--real" && i + 1 < argc) {
      if (!parse_number(argv[++i], pub_raw::kSeedCount - 1, real_index)) return usage();
    } else if (a == "--mutations" && i + 1 < argc) {
      unsigned long value = 0;
      if (!parse_number(argv[++i], 1000000, value) || value == 0) return usage();
      mutations = static_cast<unsigned>(value);
    } else {
      return usage();
    }
  }
  if (synthetic == (real_index >= 0)) return usage();

  std::vector<seed_file> seeds;
  std::uint64_t first_seed_index = 0;
  if (synthetic) {
    seeds = synthetic_seeds();
  } else {
    seed_file seed;
    std::string error;
    if (!pub_raw::load(static_cast<std::size_t>(real_index), seed.name, seed.bytes, error)) {
      std::fprintf(stderr, "%s\n", error.c_str());
      return finish(1, false);
    }
    seeds.push_back(std::move(seed));
    first_seed_index = 1000 + static_cast<std::uint64_t>(real_index);
  }

  outcome total;
  for (std::size_t s = 0; s < seeds.size(); ++s) {
    outcome one;
    run_seed(seeds[s], kBaseSeed + first_seed_index + s, mutations, one);
    std::uint64_t stops = 0;
    for (const auto& kv : one.stops) stops += kv.second;
    std::printf("seme %s: %u mutazioni, %llu immagini, %llu stop, %llu fallimenti, massimo %.2f s\n", seeds[s].name.c_str(),
                mutations, static_cast<unsigned long long>(one.images), static_cast<unsigned long long>(stops),
                static_cast<unsigned long long>(one.failures), one.max_seconds);
    total.images += one.images;
    total.failures += one.failures;
    for (const auto& [k, v] : one.stops) total.stops[k] += v;
    std::fflush(stdout);
  }
  for (const auto& [k, v] : total.stops) std::printf("  %llu x %s\n", static_cast<unsigned long long>(v), k.c_str());
  std::printf("totale: %llu immagini, %llu fallimenti\n", static_cast<unsigned long long>(total.images),
              static_cast<unsigned long long>(total.failures));
  return finish(total.failures == 0 ? 0 : 1, total.failures != 0);
}
