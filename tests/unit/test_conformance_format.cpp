// Casi T-M3BASE-20, 21, 22 (docs/contracts/BASE-confini.md §3.2, emendamento L1 di §2.5): lettura degli input
// indipendente dal formato. 20 e 21 girano per formato dichiarato e non hanno un job; 22 gira per ogni voce del registro.
// Il banco di prova e' quello di test_conformance.cpp, incluso senza modificarlo; il filtro di ctest seleziona solo i
// casi di questo file.
//
// Interfaccia attesa (nomi scelti dall'autore dei test, da seguire nel codice):
//   astro/input_format.hpp
//     struct input_format { const char* prefix; std::uint64_t max_file_bytes; bool header_plan; };
//     namespace raw_limits { constexpr std::uint64_t max_file_bytes = 1073741824; }
//     const input_format& fits_input_format();
//     const input_format& raw_input_format();
//     const std::vector<const input_format*>& input_formats();   // l'elenco delle dichiarazioni
//   astro/fits.hpp
//     fits_input open_input(const std::string& path, const input_format& format);   // protocollo unico;
//                                                                                    // open_fits_input = chiamata FITS
//   astro/hash.hpp
//     std::vector<std::uint8_t> read_verified_input(const fits_input&, const std::string& path,
//         const content_digest& expected, std::uint64_t budget_bytes, const fits_plan* planned);  // nullptr: nessun piano
//     void reverify_input(const std::string& path, const input_format& format, const fits_input* opened,
//         const content_digest& expected, std::span<std::byte> work, const std::function<void()>& check_cancel);
//   astro/job_types.hpp
//     job_type_entry::role_formats: std::map<std::string, const input_format*> (ruolo -> formato dichiarato).
//   astro/job_probe.hpp
//     punto `input.open_format`, notificato da ogni apertura del protocollo unico, con dettaglio
//     "<prefisso>\t<percorso>"; `input.open` resta col solo percorso (T-M3BASE-10).

#pragma GCC diagnostic ignored "-Wsubobject-linkage"
#include "test_conformance.cpp"

#pragma GCC diagnostic ignored "-Wmismatched-new-delete"

#include <dirent.h>
#include <fcntl.h>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "astro/input_format.hpp"

namespace {

// Riferimenti scritti a mano dal contratto (ADR-03: 2^28 * 18 + 16 MiB; ADR-08.b §2.1: 1 GiB), non letti dal prodotto.
constexpr std::uint64_t fits_limit_ref = 4848615424ULL;
constexpr std::uint64_t raw_limit_ref = 1073741824ULL;

std::uint64_t reference_limit(const std::string& prefix) {
  if (prefix == "fits") return fits_limit_ref;
  if (prefix == "raw") return raw_limit_ref;
  throw std::runtime_error("formato senza riferimento: " + prefix);
}

std::optional<stop_error> stop_from(const std::function<void()>& f) {
  try {
    f();
  } catch (const stop_error& e) {
    return e;
  }
  return std::nullopt;
}

std::string ctx(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "<assente>";
}

void expect_stop_of(const std::optional<stop_error>& s, error_category cat, const std::string& code) {
  ASSERT_TRUE(s.has_value()) << "nessuno stop, atteso " << code;
  EXPECT_EQ(s->category(), cat) << s->what();
  EXPECT_EQ(s->code(), code) << s->what();
}

void make_sparse(const fs::path& p, std::uint64_t size) {
  const int fd = ::open(p.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0) throw std::runtime_error("creazione non riuscita: " + p.string());
  const int rc = ::ftruncate(fd, static_cast<off_t>(size));
  ::close(fd);
  if (rc != 0) throw std::runtime_error("ftruncate non riuscita: " + p.string());
}

void write_file(const fs::path& p, const std::vector<std::uint8_t>& b) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  out.close();
  if (!out) throw std::runtime_error("scrittura non riuscita: " + p.string());
}

// Byte arbitrari e deterministici (LCG), che non sono un file valido di nessun formato.
std::vector<std::uint8_t> arbitrary_bytes(std::size_t n, std::uint32_t seed) {
  std::vector<std::uint8_t> b(n);
  std::uint32_t s = seed;
  for (std::uint8_t& x : b) {
    s = s * 1664525u + 1013904223u;
    x = static_cast<std::uint8_t>(s >> 24);
  }
  return b;
}

// Flag di apertura (campo `flags` di /proc/self/fdinfo) del descrittore aperto sul file `p`.
std::optional<unsigned long> open_flags_of(const fs::path& p) {
  const std::string want = fs::canonical(p).string();
  DIR* d = ::opendir("/proc/self/fd");
  if (d == nullptr) throw std::runtime_error("/proc/self/fd non leggibile");
  std::optional<unsigned long> flags;
  while (const dirent* e = ::readdir(d)) {
    if (e->d_name[0] == '.') continue;
    char target[4096];
    const ssize_t n = ::readlink((std::string("/proc/self/fd/") + e->d_name).c_str(), target, sizeof(target) - 1);
    if (n <= 0 || std::string(target, static_cast<std::size_t>(n)) != want) continue;
    std::ifstream info(std::string("/proc/self/fdinfo/") + e->d_name);
    std::string line;
    while (std::getline(info, line)) {
      if (line.rfind("flags:", 0) == 0) flags = std::stoul(line.substr(6), nullptr, 8);
    }
  }
  ::closedir(d);
  return flags;
}

// Letture di un input aperto, nell'ordine in cui avvengono.
struct read_log {
  std::mutex m;
  std::vector<std::pair<std::uint64_t, std::size_t>> calls;
};

std::vector<std::byte> work_buffer() { return std::vector<std::byte>(std::size_t{1} << 16); }

// Metadati del file senza leggerne i byte (i file sparsi al limite sono di gigabyte).
struct meta_state {
  std::uint64_t size = 0, inode = 0, nlink = 0;
  std::int64_t mtime_ns = 0;
  unsigned mode = 0;
  friend bool operator==(const meta_state&, const meta_state&) = default;
};

meta_state meta_of(const fs::path& p) {
  struct stat sb {};
  if (::stat(p.c_str(), &sb) != 0) throw std::runtime_error("stat non riuscita: " + p.string());
  return {static_cast<std::uint64_t>(sb.st_size), static_cast<std::uint64_t>(sb.st_ino),
          static_cast<std::uint64_t>(sb.st_nlink), std::int64_t{sb.st_mtim.tv_sec} * 1000000000 + sb.st_mtim.tv_nsec,
          static_cast<unsigned>(sb.st_mode & 07777)};
}

// Allocazione piu' grande richiesta a operator new dal processo dopo l'ultimo azzeramento (contatore globale nel
// binario di test). I buffer di un input al limite sono di almeno 1 GiB; le allocazioni dell'apertura (stringhe,
// contesto dello stop) sono di pochi kB. Soglia 1 MiB = `sha256_chunk_bytes`, ordini di grandezza sotto il minimo
// dei limiti dichiarati.
std::atomic<std::size_t> largest_allocation{0};
constexpr std::size_t allocation_ceiling = std::size_t{1} << 20;

// Occorrenze di `needle` nei sorgenti .cpp di src/core; `files` riceve i file che la contengono.
std::size_t occurrences_in_core(const std::string& needle, std::vector<std::string>* files = nullptr) {
  std::size_t n = 0;
  for (const fs::directory_entry& e : fs::directory_iterator(fs::path(ASTRO_SOURCE_DIR) / "src" / "core")) {
    if (e.path().extension() != ".cpp") continue;
    const std::string text = read_text(e.path());
    std::size_t at = 0, here = 0;
    while ((at = text.find(needle, at)) != std::string::npos) {
      ++here;
      at += needle.size();
    }
    if (here != 0 && files != nullptr) files->push_back(e.path().filename().string());
    n += here;
  }
  return n;
}

// SHA-256 di arbitrary_bytes(70001, 7), calcolato fuori dal prodotto (hashlib su LCG 1664525/1013904223).
constexpr const char* arbitrary_70001_sha256 = "d22b60811275a4cec32b574f8c09c0a43184e290e9ac59dbc8944261d7b3ce8b";

}  // namespace

// Misura delle allocazioni: sostituisce operator new nel binario di test (una sola unita' di traduzione).
void* operator new(std::size_t size) {
  std::size_t seen = largest_allocation.load(std::memory_order_relaxed);
  while (size > seen && !largest_allocation.compare_exchange_weak(seen, size, std::memory_order_relaxed)) {
  }
  void* p = std::malloc(size == 0 ? 1 : size);
  if (p == nullptr) throw std::bad_alloc();
  return p;
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
// Con ASan (preset dev) la forma nothrow viene dal runtime di ASan e non passa da quella sopra: senza questa
// sostituzione un blocco allocato da ASan arriva alla delete sostituita (alloc-dealloc-mismatch).
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  std::size_t seen = largest_allocation.load(std::memory_order_relaxed);
  while (size > seen && !largest_allocation.compare_exchange_weak(seen, size, std::memory_order_relaxed)) {
  }
  return std::malloc(size == 0 ? 1 : size);
}

// ---------------------------------------------------------------- T-M3BASE-20: protocollo di apertura per formato

TEST(ConformanceFormat, T_M3BASE_20_open_protocol_per_declared_format) {
  const std::vector<const input_format*>& formats = input_formats();
  std::set<std::string> prefixes;
  for (const input_format* f : formats) prefixes.insert(f->prefix);
  ASSERT_EQ(prefixes, (std::set<std::string>{"fits", "raw"})) << "elenco delle dichiarazioni diverso da FITS e RAW";
  ASSERT_EQ(formats.size(), 2u) << "dichiarazioni duplicate";
  EXPECT_EQ(raw_limits::max_file_bytes, raw_limit_ref);

  for (const input_format* f : formats) {
    const std::string prefix = f->prefix;
    SCOPED_TRACE(prefix);
    const std::uint64_t limit = reference_limit(prefix);
    ASSERT_EQ(f->max_file_bytes, limit) << "limite dichiarato diverso dal contratto";
    EXPECT_EQ(f->header_plan, prefix == "fits");
    const fs::path dir = fresh("t20_" + prefix);

    {  // limite piu' un byte: stop dall'apertura, nessuna lettura
      const fs::path p = dir / "over.bin";
      make_sparse(p, limit + 1);
      const meta_state before = meta_of(p);
      probe pr;
      largest_allocation = 0;
      const auto s = stop_from([&] { (void)open_input(p.string(), *f); });
      EXPECT_LT(largest_allocation.load(), allocation_ceiling) << "buffer allocato prima dello stop di limite";
      EXPECT_EQ(meta_of(p), before);
      expect_stop_of(s, error_category::limit_exceeded, prefix + ".max_file_bytes");
      if (s) {
        EXPECT_EQ(ctx(*s, "dimensione"), std::to_string(limit + 1));
        EXPECT_EQ(ctx(*s, "limite"), std::to_string(limit));
      }
      EXPECT_EQ(pr.count("input.read"), 0u) << "letture prima dello stop di limite";
      fs::remove(p);
    }
    {  // limite esatto: nessuno stop di limite, descrittore O_RDONLY
      const fs::path p = dir / "at.bin";
      make_sparse(p, limit);
      const meta_state before = meta_of(p);
      probe pr;
      std::optional<fits_input> input;
      ASSERT_NO_THROW(input.emplace(open_input(p.string(), *f)));
      EXPECT_EQ(input->size_bytes(), limit);
      const std::optional<unsigned long> flags = open_flags_of(p);
      ASSERT_TRUE(flags.has_value()) << "descrittore dell'input non trovato in /proc/self/fd";
      EXPECT_EQ(*flags & O_ACCMODE, static_cast<unsigned long>(O_RDONLY)) << "flag " << std::oct << *flags;
      EXPECT_EQ(pr.count("input.open"), 1u);
      EXPECT_EQ(pr.first_detail("input.open_format"), std::optional<std::string>(prefix + "\t" + p.string()));
      EXPECT_EQ(pr.count("input.read"), 0u);
      input.reset();
      EXPECT_EQ(meta_of(p), before);
      fs::remove(p);
    }
    {  // directory al posto del file
      const fs::path p = dir / "d.bin";
      fs::create_directory(p);
      const meta_state before = meta_of(p);
      expect_stop_of(stop_from([&] { (void)open_input(p.string(), *f); }), error_category::invalid_input,
                     prefix + ".not_regular");
      EXPECT_EQ(meta_of(p), before);
    }
    {  // file senza permesso di lettura
      ASSERT_NE(::geteuid(), 0u) << "il caso richiede un utente non root";
      const fs::path p = dir / "noperm.bin";
      write_file(p, arbitrary_bytes(16, 1));
      fs::permissions(p, fs::perms::none);
      const meta_state before = meta_of(p);
      const auto s = stop_from([&] { (void)open_input(p.string(), *f); });
      EXPECT_EQ(meta_of(p), before);
      fs::permissions(p, fs::perms::owner_read | fs::perms::owner_write);
      expect_stop_of(s, error_category::io, prefix + ".open");
    }
    {  // percorso assente
      const fs::path p = dir / "absent.bin";
      expect_stop_of(stop_from([&] { (void)open_input(p.string(), *f); }), error_category::io, "input.missing");
      EXPECT_FALSE(fs::exists(p)) << "il percorso assente e' stato creato";
    }
  }

  {  // il formato e' dichiarato, non dedotto dall'estensione
    const fs::path dir = fresh("t20_declared");
    const fs::path fits_named = dir / "x.fits";
    make_sparse(fits_named, raw_limit_ref + 1);
    expect_stop_of(stop_from([&] { (void)open_input(fits_named.string(), raw_input_format()); }),
                   error_category::limit_exceeded, "raw.max_file_bytes");
    EXPECT_NO_THROW((void)open_input(fits_named.string(), fits_input_format()));
    const fs::path raw_named = dir / "x.cr2";
    make_sparse(raw_named, raw_limit_ref + 1);
    const meta_state fits_before = meta_of(fits_named), raw_before = meta_of(raw_named);
    EXPECT_NO_THROW((void)open_input(raw_named.string(), fits_input_format()));
    EXPECT_EQ(meta_of(fits_named), fits_before);
    EXPECT_EQ(meta_of(raw_named), raw_before);
  }
}

// Un solo proprietario del protocollo di apertura: i punti del confine e la `open` stanno in un solo file e in un solo
// punto (una seconda funzione che ricopia il protocollo sarebbe un secondo proprietario, §2.5 emendamento L1).
TEST(ConformanceFormat, T_M3BASE_20_single_owner_of_the_open_protocol) {
  std::vector<std::string> files;
  EXPECT_EQ(occurrences_in_core("job_probe::notify(\"input.open\""), 1u);
  EXPECT_EQ(occurrences_in_core("job_probe::notify(\"input.open_format\"", &files), 1u);
  ASSERT_EQ(files.size(), 1u);
  const std::string owner = read_text(fs::path(ASTRO_SOURCE_DIR) / "src" / "core" / files.front());
  std::size_t opens = 0;
  for (std::size_t at = 0; (at = owner.find("::open(", at)) != std::string::npos; at += 7) ++opens;
  EXPECT_EQ(opens, 1u) << "piu' di una open nel file del protocollo di apertura";
}

// ---------------------------------------------------------------- T-M3BASE-21: lettura verificata senza piano degli header

TEST(ConformanceFormat, T_M3BASE_21_verified_read_without_header_plan) {
  std::size_t covered = 0;
  for (const input_format* f : input_formats()) {
    if (f->header_plan) continue;
    ++covered;
    const std::string prefix = f->prefix;
    SCOPED_TRACE(prefix);
    const fs::path dir = fresh("t21_" + prefix);
    const std::string path = (dir / "in.bin").string();
    const std::vector<std::uint8_t> bytes = arbitrary_bytes(70001, 7);
    ASSERT_ANY_THROW((void)plan_fits(bytes)) << "i byte della prova sono un FITS valido";
    // Identita' attesa: digest fissato fuori dal prodotto, dimensione 70001.
    const content_digest expected{arbitrary_70001_sha256, 70001};
    ASSERT_EQ(bytes.size(), 70001u);
    constexpr std::uint64_t budget = std::uint64_t{64} << 20;
    write_file(path, bytes);
    std::vector<std::byte> work = work_buffer();

    {  // lettura corretta e riverifica: stato invariato, una sola lettura (intera) dall'apertura al ritorno
      const file_state before = state_of(path);
      probe pr;
      fits_input input = open_input(path, *f);
      EXPECT_EQ(pr.count("input.read"), 0u) << "lettura durante l'apertura";
      read_log log;
      input.set_pread_observer([&log](std::uint64_t offset, std::size_t length) {
        std::lock_guard lock(log.m);
        log.calls.emplace_back(offset, length);
        return 0;
      });
      std::vector<std::uint8_t> got;
      ASSERT_NO_THROW(got = read_verified_input(input, path, expected, budget, nullptr));
      EXPECT_EQ(got, bytes);
      EXPECT_EQ(sha256_hex(got), arbitrary_70001_sha256);
      EXPECT_EQ(got.size(), 70001u);
      EXPECT_EQ(pr.count("input.read"), 1u) << "il file e' stato letto piu' di una volta o in parti";
      ASSERT_EQ(log.calls.size(), 1u);
      EXPECT_EQ(log.calls.front(), (std::pair<std::uint64_t, std::size_t>{0, 70001}))
          << "il file e' stato letto in altro modo prima della lettura intera";
      EXPECT_NO_THROW(reverify_input(path, *f, &input, expected, work, nullptr));
      EXPECT_NO_THROW(reverify_input(path, *f, nullptr, expected, work, nullptr));
      EXPECT_EQ(state_of(path), before);
    }
    {  // identita' attesa sbagliata con il file invariato: dimensione dichiarata diversa, digest dichiarato diverso
      const file_state ref = state_of(path);
      fits_input input = open_input(path, *f);
      expect_stop_of(
          stop_from([&] { (void)read_verified_input(input, path, {arbitrary_70001_sha256, 70002}, budget, nullptr); }),
          error_category::integrity, "input.digest_mismatch");
      expect_stop_of(
          stop_from([&] { (void)read_verified_input(input, path, {std::string(64, '0'), 70001}, budget, nullptr); }),
          error_category::integrity, "input.digest_mismatch");
      expect_stop_of(stop_from([&] { reverify_input(path, *f, nullptr, {std::string(64, '0'), 70001}, work, nullptr); }),
                     error_category::integrity, "input.digest_mismatch");
      expect_stop_of(stop_from([&] { reverify_input(path, *f, nullptr, {arbitrary_70001_sha256, 70002}, work, nullptr); }),
                     error_category::integrity, "input.size_changed");
      expect_stop_of(stop_from([&] { reverify_input(path, *f, &input, {std::string(64, '0'), 70001}, work, nullptr); }),
                     error_category::integrity, "input.digest_mismatch");
      EXPECT_EQ(state_of(path), ref);
    }
    {  // il formato e' quello dichiarato anche se i byte hanno la firma di un FITS valido o di un FITS corrotto
      const std::vector<std::uint8_t> valid = cf::frame_for_role("file");
      std::vector<std::uint8_t> signed_garbage = {'S', 'I', 'M', 'P', 'L', 'E', ' ', ' ', '=', ' ', 'T'};
      const std::vector<std::uint8_t> tail = arbitrary_bytes(5000, 3);
      signed_garbage.insert(signed_garbage.end(), tail.begin(), tail.end());
      const std::vector<const std::vector<std::uint8_t>*> contents{&valid, &signed_garbage};
      for (const std::vector<std::uint8_t>* content : contents) {
        const std::string name = (dir / "signed.bin").string();
        write_file(name, *content);
        const file_state ref = state_of(name);
        const content_digest id{sha256_hex(*content), content->size()};
        probe pr;
        std::mutex om;
        std::vector<std::string> opened;
        pr.action = [&](std::string_view point, std::string_view detail) {
          if (point != "input.open_format") return;
          std::lock_guard lock(om);
          opened.emplace_back(detail);
        };
        fits_input input = open_input(name, *f);
        EXPECT_EQ(pr.count("input.read"), 0u) << "lettura durante l'apertura";
        std::vector<std::uint8_t> got;
        ASSERT_NO_THROW(got = read_verified_input(input, name, id, budget, nullptr));
        EXPECT_EQ(got, *content) << "il confine ha interpretato il file prima del digest";
        EXPECT_EQ(pr.count("input.read"), 1u) << "letture oltre la lettura intera";
        EXPECT_NO_THROW(reverify_input(name, *f, &input, id, work, nullptr));
        EXPECT_EQ(state_of(name), ref);
        std::lock_guard lock(om);
        EXPECT_EQ(opened, (std::vector<std::string>{prefix + "\t" + name, prefix + "\t" + name}))
            << "aperture (lettura e riverifica) con formato diverso da quello dichiarato";
      }
    }
    {  // un byte cambiato
      std::vector<std::uint8_t> b = bytes;
      b[bytes.size() / 2] ^= 0x01;
      write_file(path, b);
      const file_state ref = state_of(path);
      fits_input input = open_input(path, *f);
      expect_stop_of(stop_from([&] { (void)read_verified_input(input, path, expected, budget, nullptr); }),
                     error_category::integrity, "input.digest_mismatch");
      EXPECT_EQ(state_of(path), ref);
      write_file(path, bytes);
    }
    {  // file cresciuto dopo l'apertura
      fits_input input = open_input(path, *f);
      {
        std::ofstream out(path, std::ios::binary | std::ios::app);
        out.put('x');
      }
      const file_state ref = state_of(path);
      expect_stop_of(stop_from([&] { (void)read_verified_input(input, path, expected, budget, nullptr); }),
                     error_category::integrity, "input.size_changed");
      EXPECT_EQ(state_of(path), ref);
      write_file(path, bytes);
    }
    {  // il percorso indica un altro file
      const fs::path other = dir / "other.bin";
      write_file(other, arbitrary_bytes(bytes.size(), 99));
      fits_input input = open_input(path, *f);
      fs::rename(other, path);
      const file_state ref = state_of(path);
      expect_stop_of(stop_from([&] { (void)read_verified_input(input, path, expected, budget, nullptr); }),
                     error_category::integrity, "input.modified");
      EXPECT_EQ(state_of(path), ref);
      write_file(path, bytes);
    }
    {  // file assente alla riverifica
      fits_input input = open_input(path, *f);
      ASSERT_NO_THROW((void)read_verified_input(input, path, expected, budget, nullptr));
      fs::remove(path);
      const file_state ref = state_of(path);
      EXPECT_FALSE(ref.exists);
      expect_stop_of(stop_from([&] { reverify_input(path, *f, &input, expected, work, nullptr); }),
                     error_category::io, "input.missing");
      EXPECT_EQ(state_of(path), ref) << "il percorso assente e' stato creato";
      write_file(path, bytes);
    }
    {  // file cambiato fra la lettura verificata e la riverifica
      fits_input input = open_input(path, *f);
      ASSERT_NO_THROW((void)read_verified_input(input, path, expected, budget, nullptr));
      std::vector<std::uint8_t> b = bytes;
      b[17] ^= 0x01;
      overwrite(path, b);
      const file_state ref = state_of(path);
      const auto s = stop_from([&] { reverify_input(path, *f, &input, expected, work, nullptr); });
      EXPECT_EQ(state_of(path), ref);
      ASSERT_TRUE(s.has_value()) << "nessuno stop alla riverifica";
      EXPECT_EQ(s->category(), error_category::integrity) << s->what();
      EXPECT_TRUE(s->code() == "input.modified" || s->code() == "input.size_changed" ||
                  s->code() == "input.digest_mismatch")
          << s->what();
      write_file(path, bytes);
    }
    {  // la riverifica apre con la dichiarazione del formato del ruolo: limite e codice di questo formato
      const fs::path big = dir / "big.bin";
      make_sparse(big, f->max_file_bytes + 1);
      const meta_state ref = meta_of(big);
      expect_stop_of(
          stop_from([&] { reverify_input(big.string(), *f, nullptr, {std::string(64, '0'), 1}, work, nullptr); }),
          error_category::limit_exceeded, prefix + ".max_file_bytes");
      EXPECT_EQ(meta_of(big), ref);
      fs::remove(big);
    }
  }
  EXPECT_GE(covered, 1u) << "nessun formato senza piano degli header: il caso non prova nulla";
}

// ---------------------------------------------------------------- T-M3BASE-22: formato dichiarato dalla voce del registro

namespace {

class ConformanceFormatRole : public testing::TestWithParam<type_param> {};

}  // namespace

TEST_P(ConformanceFormatRole, T_M3BASE_22_open_uses_the_format_declared_for_the_role) {
  const job_type_entry& entry = registered_job_types()[GetParam().entry];
  std::set<std::string> known;
  for (const input_format* f : input_formats()) known.insert(f->prefix);

  std::map<std::string, std::string> declared;  // percorso -> prefisso dichiarato
  type_case c(GetParam());
  for (const std::string& role : entry.input_roles) {
    const auto it = entry.role_formats.find(role);
    if (it == entry.role_formats.end() || it->second == nullptr) {
      ADD_FAILURE() << "ruolo " << role << " senza formato nella voce " << entry.operator_id;
      continue;
    }
    EXPECT_EQ(known.count(it->second->prefix), 1u) << "formato non presente nell'elenco delle dichiarazioni: " << role;
    // Estensione discordante dal formato dichiarato: la dichiarazione prevale, il formato non si deduce dal nome.
    const std::string discordant = std::string(it->second->prefix) == "raw" ? ".fits" : ".cr2";
    const fs::path renamed = fs::path(c.inputs.at(role).path).replace_extension(discordant);
    fs::rename(c.inputs.at(role).path, renamed);
    c.inputs.at(role).path = renamed.string();
    declared[renamed.string()] = it->second->prefix;
  }
  c.job.emplace(entry.make(c.inputs, c.variant.name, c.store->root()));
  c.store->put_node(c.job->node);
  (void)c.store->retarget_slot(slot, c.job->node.key());
  c.snapshot();
  EXPECT_EQ(entry.role_formats.size(), entry.input_roles.size()) << "formati dichiarati per ruoli inesistenti";

  probe pr;
  std::mutex m;
  std::vector<std::pair<std::string, std::string>> opens;  // (prefisso, percorso)
  pr.action = [&](std::string_view point, std::string_view detail) {
    if (point != "input.open_format") return;
    const std::size_t tab = detail.find('\t');
    std::lock_guard lock(m);
    opens.emplace_back(std::string(detail.substr(0, tab)),
                       tab == std::string_view::npos ? std::string() : std::string(detail.substr(tab + 1)));
  };
  const ran r = c.run_default();
  ASSERT_TRUE(r.outcome.has_value() && r.outcome->exit_code == 0) << describe(r);
  for (const auto& [role, in] : c.inputs) EXPECT_EQ(state_of(in.path), c.expected.at(role)) << "input alterato: " << role;

  std::lock_guard lock(m);
  EXPECT_EQ(pr.count("input.open"), opens.size()) << "aperture che non passano dal punto con il formato";
  std::map<std::string, std::size_t> seen;
  for (const auto& [prefix, path] : opens) {
    const auto it = declared.find(path);
    if (it == declared.end()) continue;
    ++seen[path];
    EXPECT_EQ(prefix, it->second) << "apertura di " << path << " con formato diverso da quello dichiarato";
  }
  for (const auto& [path, prefix] : declared) {
    EXPECT_GE(seen[path], 1u) << "input " << path << " mai aperto con il formato dichiarato";
  }
}

namespace {

INSTANTIATE_TEST_SUITE_P(Types, ConformanceFormatRole, testing::ValuesIn(all_types()), param_name);

}  // namespace
