// U3 (M3.a): decoder RAW robusto. T-M3A-RAW-05, -06, -08 (docs/contracts/ADR-08-decoder-pack.md §2.1, §2.5, §2.7, §2.8, §6);
// T-M3A-RAW-10 è l'eseguibile fuzz_raw_decode (fuzz_raw_decode.cpp).
// Fixture FIX-SYN-DNG-01 (tests/fixtures/syn_dng.hpp), indipendente da astrocore e da LibRaw. I codici di ritorno e i bit
// di avviso iniettati sono le costanti di <libraw/libraw_const.h>; categoria, codice e contesto attesi sono quelli della
// tabella di §2.5, mai derivati dall'uscita del decoder. Estensioni d'interfaccia usate (fissate dal contratto):
// raw_limits::max_pixels, raw_decode_context, raw_decode_hooks::{libraw_return, with_handlers, before_unpack_plan},
// raw_probe_fields::{raw_width, raw_height}, raw_unpack_fields::process_warnings, decoder_state::process_warnings
// (std::uint32_t: bit di avviso ammessi e registrati, §2.5).
// raw_probe_fields::{raw_width, raw_height} sono le dimensioni su cui il prodotto decide max_pixels e M di §2.1, dopo il
// gancio after_open. raw_unpack_plan::max_raw_memory_mb è il valore letto da rawparams.max_raw_memory_mb subito prima di
// unpack, non quello calcolato; raw_unpack_plan::raw_image_bytes è la parte della prenotazione per raw_image (M * 2^20).
// T-M3A-RAW-08 è nell'eseguibile raw_threads.cpp: un processo nuovo per campione e per fase.
#include <gtest/gtest.h>
#include <libraw/libraw.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "astro/cancel.hpp"
#include "astro/error.hpp"
#include "astro/image.hpp"
#include "astro/input_format.hpp"
#include "astro/memory_pool.hpp"
#include "astro/raw_decode.hpp"
#include "expect_stop.hpp"
#include "syn_dng.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;

constexpr std::uint64_t mib = 1024 * 1024;

fx::dng_spec fixture_spec(std::uint32_t w, std::uint32_t h) {
  fx::dng_spec s;
  s.width = w;
  s.height = h;
  s.white = 4000;
  std::vector<std::uint16_t> v(static_cast<std::size_t>(w) * h);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>((i * 7) % 4000);
  s.pixels = fx::pixels_u16(v);
  return s;
}

const std::vector<std::uint8_t>& small_dng() {
  static const std::vector<std::uint8_t> bytes = fx::make_dng(fixture_spec(64, 48));
  return bytes;
}

// M di ADR-08.b §2.1: max_raw_memory_mb = max(1, ceil(2 * raw_width * (raw_height + 8) / 2^20)), calcolato dalle dimensioni
// scritte nella fixture.
constexpr std::uint64_t expected_m(std::uint64_t width, std::uint64_t height) {
  const std::uint64_t m = (2 * width * (height + 8) + mib - 1) / mib;
  return m < 1 ? 1 : m;
}
// Prenotazione del decoder (§2.8): M * 2^20 per raw_image, campioni f32 dell'area visibile (4 B per pixel) e maschera di
// invalidità (invalid_mask di ADR-03, 2 B per pixel). I byte del file sono già prenotati dal chiamante con la lettura.
constexpr std::uint64_t reservation_bytes(std::uint64_t width, std::uint64_t height) {
  return expected_m(width, height) * mib + 6 * width * height;
}
constexpr std::uint64_t small_dng_min_reservation = reservation_bytes(64, 48);

std::optional<std::string> ctx_value(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context())
    if (k == key) return v;
  return std::nullopt;
}

bool ctx_has_substring(const stop_error& e, const std::string& needle) {
  for (const auto& [k, v] : e.context())
    if (v.find(needle) != std::string::npos) return true;
  return false;
}

struct rc_row {
  int rc;
  error_category category;
  const char* code;
};

// Tabella di ADR-08.b §2.5, più righe "ogni altro valore non nullo" (negativi noti, positivo, valore sconosciuto).
const std::vector<rc_row>& rc_table() {
  static const std::vector<rc_row> rows = {
      {LIBRAW_FILE_UNSUPPORTED, error_category::unsupported, "raw.file_unsupported"},
      {LIBRAW_DATA_ERROR, error_category::integrity, "raw.data_error"},
      {LIBRAW_IO_ERROR, error_category::integrity, "raw.data_error"},
      {LIBRAW_BAD_CROP, error_category::integrity, "raw.data_error"},
      {LIBRAW_TOO_BIG, error_category::limit_exceeded, "raw.too_big"},
      {LIBRAW_UNSUFFICIENT_MEMORY, error_category::limit_exceeded, "raw.too_big"},
      {LIBRAW_MEMPOOL_OVERFLOW, error_category::limit_exceeded, "raw.too_big"},
      {LIBRAW_CANCELLED_BY_CALLBACK, error_category::cancelled, "raw.cancelled"},
      {LIBRAW_UNSPECIFIED_ERROR, error_category::invalid_input, "raw.libraw_error"},
      {LIBRAW_REQUEST_FOR_NONEXISTENT_IMAGE, error_category::invalid_input, "raw.libraw_error"},
      {LIBRAW_OUT_OF_ORDER_CALL, error_category::invalid_input, "raw.libraw_error"},
      {LIBRAW_NO_THUMBNAIL, error_category::invalid_input, "raw.libraw_error"},
      {LIBRAW_UNSUPPORTED_THUMBNAIL, error_category::invalid_input, "raw.libraw_error"},
      {LIBRAW_INPUT_CLOSED, error_category::invalid_input, "raw.libraw_error"},
      {LIBRAW_NOT_IMPLEMENTED, error_category::invalid_input, "raw.libraw_error"},
      {LIBRAW_REQUEST_FOR_NONEXISTENT_THUMBNAIL, error_category::invalid_input, "raw.libraw_error"},
      {1, error_category::invalid_input, "raw.libraw_error"},
      {-12345, error_category::invalid_input, "raw.libraw_error"},
  };
  return rows;
}

// Riga della tabella per un valore di LibRaw qualunque.
rc_row row_for(int rc) {
  for (const auto& r : rc_table())
    if (r.rc == rc) return r;
  return {rc, error_category::invalid_input, "raw.libraw_error"};
}

// Contesto di uno stop da codice di ritorno: funzione, valore numerico e libraw_strerror (ADR-08.b §2.5).
void expect_rc_context(const stop_error& e, const std::string& function, int rc) {
  EXPECT_EQ(ctx_value(e, "funzione"), function) << e.what();
  EXPECT_EQ(ctx_value(e, "valore"), std::to_string(rc)) << e.what();
  EXPECT_EQ(ctx_value(e, "libraw"), std::string(libraw_strerror(rc))) << e.what();
}

// Stop atteso da un codice di LibRaw ottenuto dal contesto stesso: la riga di tabella dipende solo dal valore numerico.
void expect_table_consistent(const stop_error& e) {
  const auto function = ctx_value(e, "funzione");
  const auto value = ctx_value(e, "valore");
  ASSERT_TRUE(function.has_value()) << e.what();
  ASSERT_TRUE(value.has_value()) << e.what();
  EXPECT_TRUE(*function == "open_buffer" || *function == "unpack") << *function;
  const int rc = std::stoi(*value);
  const rc_row row = row_for(rc);
  EXPECT_EQ(e.category(), row.category) << e.what();
  EXPECT_EQ(e.code(), row.code) << e.what();
  EXPECT_EQ(ctx_value(e, "libraw"), std::string(libraw_strerror(rc))) << e.what();
}

stop_error capture(const std::vector<std::uint8_t>& bytes, const raw_decode_hooks& hooks = {},
                   const raw_decode_context& ctx = {}) {
  try {
    const image img = decode_raw_buffer(bytes, hooks, ctx);
    ADD_FAILURE() << "stop_error atteso, immagine prodotta " << img.geometry.width << "x" << img.geometry.height;
  } catch (const stop_error& e) {
    return e;
  }
  return stop_error(error_category::invalid_input, "test.no_stop", "nessuno stop");
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-RAW-05: input malformati, limiti, gestore di errori, avvisi, tabella dei codici di ritorno.
// ---------------------------------------------------------------------------------------------------------------------

// Base di confronto: il DNG valido e la registrazione delle chiamate a LibRaw (funzione, codice reale).
TEST(RawRobust, T_M3A_RAW_05_valid_file_reports_open_then_unpack_with_zero) {
  std::vector<std::pair<std::string, int>> calls;
  raw_decode_hooks hooks;
  hooks.libraw_return = [&](std::string_view function, int rc) {
    calls.emplace_back(std::string(function), rc);
    return rc;
  };
  const image img = decode_raw_buffer(small_dng(), hooks, {});
  validate_image(img);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[0], (std::pair<std::string, int>{"open_buffer", LIBRAW_SUCCESS}));
  EXPECT_EQ(calls[1], (std::pair<std::string, int>{"unpack", LIBRAW_SUCCESS}));
}

TEST(RawRobust, T_M3A_RAW_05_empty_and_truncated_files_stop_with_table_codes) {
  const std::vector<std::uint8_t>& full = small_dng();
  // Lunghezze su tutto il file (intestazione, dati, IFD): ogni troncamento è uno stop della tabella con il contesto
  // coerente con il valore di LibRaw, mai un'immagine; il test non fissa quale dei due codici.
  for (std::size_t length : {std::size_t{0}, std::size_t{1}, std::size_t{4}, std::size_t{8}, std::size_t{16},
                             std::size_t{100}, std::size_t{1000}, std::size_t{3000}, std::size_t{6000},
                             std::size_t{6200}, std::size_t{6400}, full.size() / 2, full.size() - 100}) {
    SCOPED_TRACE("lunghezza " + std::to_string(length));
    const std::vector<std::uint8_t> cut(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(length));
    bool unpack_called = false;
    raw_decode_hooks hooks;
    hooks.before_unpack = [&] { unpack_called = true; };
    const stop_error e = capture(cut, hooks);
    EXPECT_FALSE(unpack_called);
    EXPECT_TRUE(e.code() == "raw.data_error" || e.code() == "raw.file_unsupported") << e.what();
    expect_table_consistent(e);
    EXPECT_EQ(ctx_value(e, "funzione"), std::string("open_buffer"));
  }
}

TEST(RawRobust, T_M3A_RAW_05_random_bytes_stop_with_table_codes) {
  std::mt19937_64 rng(0x5EED0005u);
  for (std::size_t length : {std::size_t{1}, std::size_t{7}, std::size_t{64}, std::size_t{513}, std::size_t{4096}}) {
    SCOPED_TRACE("lunghezza " + std::to_string(length));
    std::vector<std::uint8_t> noise(length);
    for (auto& b : noise) b = static_cast<std::uint8_t>(rng() & 0xFF);
    bool unpack_called = false;
    raw_decode_hooks hooks;
    hooks.before_unpack = [&] { unpack_called = true; };
    const stop_error e = capture(noise, hooks);
    EXPECT_FALSE(unpack_called);
    expect_table_consistent(e);
    EXPECT_NE(e.code(), "raw.libraw_error");
  }
}

// ADR-08.b §2.1: file oltre max_file_bytes, nessuna lettura. Il buffer è una mappa PROT_NONE: qualunque lettura sarebbe
// un SIGSEGV, quindi un'implementazione che guarda i byte invece della dimensione fa fallire il test.
TEST(RawRobust, T_M3A_RAW_05_file_over_max_file_bytes_stops_without_reading) {
  const std::size_t over = static_cast<std::size_t>(raw_limits::max_file_bytes) + 1;
  void* base = ::mmap(nullptr, over, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  ASSERT_NE(base, MAP_FAILED);
  bool open_called = false;
  raw_decode_hooks hooks;
  hooks.before_open = [&](raw_user_params&) { open_called = true; };
  const std::span<const std::uint8_t> bytes(static_cast<const std::uint8_t*>(base), over);
  EXPECT_STOP(decode_raw_buffer(bytes, hooks, {}), error_category::limit_exceeded, "raw.max_file_bytes");
  EXPECT_FALSE(open_called);
  EXPECT_EQ(::munmap(base, over), 0);
}

// Confine inclusivo (m02): esattamente max_file_bytes è ammesso e arriva al gancio before_open, che ferma con una
// sentinella prima di ogni lettura del buffer (mappa PROT_NONE); max_file_bytes + 1 si ferma prima del gancio.
TEST(RawRobust, T_M3A_RAW_05_file_exactly_at_max_file_bytes_is_accepted_without_reading) {
  struct sentinel {};
  const std::size_t exact = static_cast<std::size_t>(raw_limits::max_file_bytes);
  void* base = ::mmap(nullptr, exact, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  ASSERT_NE(base, MAP_FAILED);
  const std::span<const std::uint8_t> bytes(static_cast<const std::uint8_t*>(base), exact);
  bool open_reached = false;
  raw_decode_hooks hooks;
  hooks.before_open = [&](raw_user_params&) {
    open_reached = true;
    throw sentinel{};
  };
  bool sentinel_seen = false;
  try {
    (void)decode_raw_buffer(bytes, hooks, {});
    ADD_FAILURE() << "la sentinella del gancio doveva interrompere la decodifica";
  } catch (const sentinel&) {
    sentinel_seen = true;
  } catch (const stop_error& e) {
    ADD_FAILURE() << "file di max_file_bytes byte rifiutato: " << e.what();
  }
  EXPECT_TRUE(sentinel_seen);
  EXPECT_TRUE(open_reached);
  EXPECT_EQ(::munmap(base, exact), 0);
}

TEST(RawRobust, T_M3A_RAW_05_max_pixels_constant_matches_the_contract) {
  EXPECT_EQ(raw_limits::max_pixels, 268435456ull);
  EXPECT_EQ(raw_limits::max_file_bytes, 1073741824ull);
}

// Dimensioni dichiarate dal file oltre max_pixels: stop dopo open_buffer e prima di unpack. 20000 x 20000 = 4e8 > 2^28; il
// file porta 1000 byte di dati (open_buffer accetta queste dimensioni).
TEST(RawRobust, T_M3A_RAW_05_declared_dimensions_over_max_pixels_stop_before_unpack) {
  fx::dng_spec s;
  s.width = 20000;
  s.height = 20000;
  s.white = 4000;
  s.pixels = std::vector<std::uint8_t>(1000, 0);
  const std::vector<std::uint8_t> bytes = fx::make_dng(s);
  bool unpack_called = false;
  std::vector<std::string> calls;
  raw_decode_hooks hooks;
  hooks.before_unpack = [&] { unpack_called = true; };
  hooks.libraw_return = [&](std::string_view function, int rc) {
    calls.emplace_back(function);
    return rc;
  };
  EXPECT_STOP(decode_raw_buffer(bytes, hooks, {}), error_category::limit_exceeded, "raw.max_pixels");
  EXPECT_FALSE(unpack_called);
  EXPECT_EQ(calls, std::vector<std::string>{"open_buffer"});
}

// Limite esatto sul prodotto raw_width * raw_height, con il limite del contesto abbassato al file di prova (64 x 48).
TEST(RawRobust, T_M3A_RAW_05_max_pixels_boundary_is_inclusive) {
  raw_decode_context at_limit;
  at_limit.max_pixels = 64ull * 48;
  validate_image(decode_raw_buffer(small_dng(), {}, at_limit));

  raw_decode_context below;
  below.max_pixels = 64ull * 48 - 1;
  bool unpack_called = false;
  raw_decode_hooks hooks;
  hooks.before_unpack = [&] { unpack_called = true; };
  EXPECT_STOP(decode_raw_buffer(small_dng(), hooks, below), error_category::limit_exceeded, "raw.max_pixels");
  EXPECT_FALSE(unpack_called);
}

// Confine del limite di prodotto per iniezione delle dimensioni lette: 2^14 x 2^14 = 2^28 è ammesso, un pixel in più no.
TEST(RawRobust, T_M3A_RAW_05_injected_dimensions_around_the_product_limit) {
  {
    raw_decode_hooks hooks;
    bool unpack_called = false;
    hooks.after_open = [](raw_probe_fields& f) {
      f.raw_width = 16384;
      f.raw_height = 16385;
    };
    hooks.before_unpack = [&] { unpack_called = true; };
    EXPECT_STOP(decode_raw_buffer(small_dng(), hooks, {}), error_category::limit_exceeded, "raw.max_pixels");
    EXPECT_FALSE(unpack_called);
  }
  {
    // 32 bit non bastano per 2^32 pixel: il prodotto di due unsigned a 32 bit va fatto a 64 bit.
    raw_decode_hooks hooks;
    bool unpack_called = false;
    hooks.after_open = [](raw_probe_fields& f) {
      f.raw_width = 65536;
      f.raw_height = 65536;
    };
    hooks.before_unpack = [&] { unpack_called = true; };
    EXPECT_STOP(decode_raw_buffer(small_dng(), hooks, {}), error_category::limit_exceeded, "raw.max_pixels");
    EXPECT_FALSE(unpack_called);
  }
}

// Gestore degli errori di dati (§2.5): richiamato durante la fase di unpack, che poi ritorna LIBRAW_SUCCESS, la
// decodifica si ferma comunque; file e offset sono quelli riportati dal gestore.
TEST(RawRobust, T_M3A_RAW_05_data_error_handler_stops_even_when_unpack_succeeds) {
  int unpack_rc = 12345;
  bool after_unpack_called = false;
  raw_decode_hooks hooks;
  hooks.with_handlers = [](const raw_unpack_callbacks& cb) { cb.data_error("sintetico.dng", 4242); };
  hooks.libraw_return = [&](std::string_view function, int rc) {
    if (function == "unpack") unpack_rc = rc;
    return rc;
  };
  hooks.after_unpack = [&](raw_unpack_fields&) { after_unpack_called = true; };
  const stop_error e = capture(small_dng(), hooks);
  EXPECT_EQ(unpack_rc, LIBRAW_SUCCESS);
  EXPECT_EQ(e.category(), error_category::integrity);
  EXPECT_EQ(e.code(), "raw.data_error");
  EXPECT_EQ(ctx_value(e, "file"), std::string("sintetico.dng"));
  EXPECT_EQ(ctx_value(e, "offset"), std::string("4242"));
  expect_rc_context(e, "unpack", LIBRAW_SUCCESS);
  EXPECT_FALSE(after_unpack_called) << "nessun campione si legge dopo un errore di dati";
}

// Il contratto conserva la PRIMA segnalazione (§2.5, "registra file e offset dell'errore"; LibRaw prosegue dopo la
// prima): due segnalazioni distinte, uno solo stop, con file e offset della prima. Conservare l'ultima fa fallire.
TEST(RawRobust, T_M3A_RAW_05_data_error_handler_called_more_than_once_still_stops_once_with_the_first_report) {
  raw_decode_hooks hooks;
  hooks.with_handlers = [](const raw_unpack_callbacks& cb) {
    cb.data_error("prima.dng", 10);
    cb.data_error("seconda.dng", 20);
    cb.data_error(nullptr, 30);
  };
  const stop_error e = capture(small_dng(), hooks);
  EXPECT_EQ(e.category(), error_category::integrity);
  EXPECT_EQ(e.code(), "raw.data_error");
  EXPECT_EQ(ctx_value(e, "file"), std::string("prima.dng"));
  EXPECT_EQ(ctx_value(e, "offset"), std::string("10"));
  expect_rc_context(e, "unpack", LIBRAW_SUCCESS);
}

// Registrazione reale (m22): il gestore passato a LibRaw con set_dataerror_handler è invocato da LibRaw stessa, non dal
// gancio di test. DNG che dichiara 64 x 48 ma porta 1000 byte di campioni: unpack legge oltre la fine. L'oracolo è
// LibRaw diretta con un proprio gestore sullo stesso file: la sua prima segnalazione (file, offset) è quella attesa.
struct oracle_report {
  int calls = 0;
  std::optional<std::string> file;
  INT64 offset = -1;
};

void oracle_handler(void* data, const char* file, const INT64 offset) {
  auto& r = *static_cast<oracle_report*>(data);
  if (r.calls++ == 0) {
    if (file != nullptr) r.file = file;
    r.offset = offset;
  }
}

TEST(RawRobust, T_M3A_RAW_05_data_error_raised_inside_libraw_reaches_the_registered_handler) {
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.white = 4000;
  s.pixels = std::vector<std::uint8_t>(1000, 0x11);
  const std::vector<std::uint8_t> bytes = fx::make_dng(s);

  oracle_report oracle;
  int oracle_rc = 0;
  {
    LibRaw lr;
    lr.set_dataerror_handler(oracle_handler, &oracle);
    ASSERT_EQ(lr.open_buffer(bytes.data(), bytes.size()), LIBRAW_SUCCESS);
    oracle_rc = lr.unpack();
  }
  ASSERT_GE(oracle.calls, 1) << "il file di prova deve far scattare l'errore di dati dentro LibRaw (unpack " << oracle_rc << ")";

  bool with_handlers_called = false;
  raw_decode_hooks hooks;
  hooks.with_handlers = [&](const raw_unpack_callbacks&) { with_handlers_called = true; };
  const stop_error e = capture(bytes, hooks);
  EXPECT_TRUE(with_handlers_called);
  EXPECT_EQ(e.category(), error_category::integrity) << e.what();
  EXPECT_EQ(e.code(), "raw.data_error") << e.what();
  EXPECT_EQ(ctx_value(e, "offset"), std::to_string(oracle.offset)) << e.what();
  EXPECT_EQ(ctx_value(e, "file"), oracle.file) << e.what();
  EXPECT_EQ(ctx_value(e, "funzione"), std::string("unpack")) << e.what();
}

// Avvisi di §2.5: i bit ammessi non fermano, ogni altro bit sì.
TEST(RawRobust, T_M3A_RAW_05_allowed_warnings_do_not_stop) {
  const unsigned allowed[] = {LIBRAW_WARN_BAD_CAMERA_WB,     LIBRAW_WARN_NO_METADATA,        LIBRAW_WARN_NO_EMBEDDED_PROFILE,
                              LIBRAW_WARN_NO_INPUT_PROFILE,  LIBRAW_WARN_BAD_OUTPUT_PROFILE, LIBRAW_WARN_NO_BADPIXELMAP,
                              LIBRAW_WARN_BAD_DARKFRAME_FILE, LIBRAW_WARN_BAD_DARKFRAME_DIM,
                              LIBRAW_WARN_PARSEFUJI_PROCESSED};
  unsigned all = 0;
  for (unsigned bit : allowed) {
    SCOPED_TRACE("bit " + std::to_string(bit));
    all |= bit;
    raw_decode_hooks hooks;
    hooks.after_unpack = [bit](raw_unpack_fields& f) { f.process_warnings = bit; };
    const image img = decode_raw_buffer(small_dng(), hooks, {});
    validate_image(img);
    ASSERT_TRUE(img.decoder_state.has_value());
    EXPECT_EQ(img.decoder_state->process_warnings, bit);
  }
  raw_decode_hooks hooks;
  hooks.after_unpack = [all](raw_unpack_fields& f) { f.process_warnings = all; };
  const image img = decode_raw_buffer(small_dng(), hooks, {});
  validate_image(img);
  ASSERT_TRUE(img.decoder_state.has_value());
  EXPECT_EQ(img.decoder_state->process_warnings, all);
}

TEST(RawRobust, T_M3A_RAW_05_other_warning_bits_stop_with_the_name) {
  struct named {
    unsigned bit;
    const char* name;
  };
  const named disallowed[] = {
      {LIBRAW_WARN_NO_JPEGLIB, "NO_JPEGLIB"},
      {1u << 11, "NO_JASPER"}  /* LIBRAW_WARN_NO_JASPER: dichiarato solo sotto una macro di LibRaw */,
      {LIBRAW_WARN_RAWSPEED_PROBLEM, "RAWSPEED_PROBLEM"},
      {LIBRAW_WARN_RAWSPEED_UNSUPPORTED, "RAWSPEED_UNSUPPORTED"},
      {LIBRAW_WARN_RAWSPEED_PROCESSED, "RAWSPEED_PROCESSED"},
      {LIBRAW_WARN_FALLBACK_TO_AHD, "FALLBACK_TO_AHD"},
      {LIBRAW_WARN_DNGSDK_PROCESSED, "DNGSDK_PROCESSED"},
      {LIBRAW_WARN_DNG_IMAGES_REORDERED, "DNG_IMAGES_REORDERED"},
      {LIBRAW_WARN_DNG_STAGE2_APPLIED, "DNG_STAGE2_APPLIED"},
      {LIBRAW_WARN_DNG_STAGE3_APPLIED, "DNG_STAGE3_APPLIED"},
      {LIBRAW_WARN_RAWSPEED3_PROBLEM, "RAWSPEED3_PROBLEM"},
      {LIBRAW_WARN_RAWSPEED3_UNSUPPORTED, "RAWSPEED3_UNSUPPORTED"},
      {LIBRAW_WARN_RAWSPEED3_PROCESSED, "RAWSPEED3_PROCESSED"},
      {LIBRAW_WARN_RAWSPEED3_NOTLISTED, "RAWSPEED3_NOTLISTED"},
  };
  for (const auto& d : disallowed) {
    SCOPED_TRACE(d.name);
    raw_decode_hooks hooks;
    hooks.after_unpack = [bit = d.bit](raw_unpack_fields& f) { f.process_warnings = bit; };
    const stop_error e = capture(small_dng(), hooks);
    EXPECT_EQ(e.category(), error_category::unsupported) << e.what();
    EXPECT_EQ(e.code(), "raw.warning") << e.what();
    EXPECT_TRUE(ctx_has_substring(e, d.name)) << e.what();
  }
  // Un bit ammesso insieme a uno non ammesso: si ferma per quello non ammesso.
  raw_decode_hooks mixed;
  mixed.after_unpack = [](raw_unpack_fields& f) { f.process_warnings = LIBRAW_WARN_NO_METADATA | LIBRAW_WARN_NO_JPEGLIB; };
  const stop_error e = capture(small_dng(), mixed);
  EXPECT_EQ(e.code(), "raw.warning");
  EXPECT_TRUE(ctx_has_substring(e, "NO_JPEGLIB")) << e.what();
}

TEST(RawRobust, T_M3A_RAW_05_unknown_warning_bits_stop) {
  for (unsigned shift : {0u, 1u, 25u, 26u, 30u, 31u}) {
    SCOPED_TRACE("bit " + std::to_string(shift));
    raw_decode_hooks hooks;
    hooks.after_unpack = [shift](raw_unpack_fields& f) { f.process_warnings = 1u << shift; };
    const stop_error e = capture(small_dng(), hooks);
    EXPECT_EQ(e.category(), error_category::unsupported) << e.what();
    EXPECT_EQ(e.code(), "raw.warning") << e.what();
    EXPECT_FALSE(e.context().empty());
  }
}

// Tabella dei codici di ritorno: ogni riga, per open_buffer e per unpack, con funzione, valore e libraw_strerror.
TEST(RawRobust, T_M3A_RAW_05_every_return_code_row_for_open_buffer) {
  for (const auto& row : rc_table()) {
    SCOPED_TRACE("rc " + std::to_string(row.rc));
    bool unpack_called = false;
    raw_decode_hooks hooks;
    hooks.libraw_return = [&](std::string_view function, int rc) { return function == "open_buffer" ? row.rc : rc; };
    hooks.before_unpack = [&] { unpack_called = true; };
    const stop_error e = capture(small_dng(), hooks);
    EXPECT_EQ(e.category(), row.category) << e.what();
    EXPECT_EQ(e.code(), row.code) << e.what();
    expect_rc_context(e, "open_buffer", row.rc);
    EXPECT_FALSE(unpack_called);
  }
}

TEST(RawRobust, T_M3A_RAW_05_every_return_code_row_for_unpack) {
  for (const auto& row : rc_table()) {
    SCOPED_TRACE("rc " + std::to_string(row.rc));
    bool after_unpack_called = false;
    raw_decode_hooks hooks;
    hooks.libraw_return = [&](std::string_view function, int rc) { return function == "unpack" ? row.rc : rc; };
    hooks.after_unpack = [&](raw_unpack_fields&) { after_unpack_called = true; };
    const stop_error e = capture(small_dng(), hooks);
    EXPECT_EQ(e.category(), row.category) << e.what();
    EXPECT_EQ(e.code(), row.code) << e.what();
    expect_rc_context(e, "unpack", row.rc);
    EXPECT_FALSE(after_unpack_called);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Cancel (§2.5): il gestore di progresso consulta il token; un cancel ferma unpack con LIBRAW_CANCELLED_BY_CALLBACK.
// ---------------------------------------------------------------------------------------------------------------------

TEST(RawRobust, T_M3A_RAW_05_cancel_requested_before_unpack_stops_with_libraw_code) {
  cancel_token token;
  raw_decode_context ctx;
  ctx.cancel = &token;
  raw_decode_hooks hooks;
  hooks.before_unpack = [&] { token.request(); };
  const stop_error e = capture(small_dng(), hooks, ctx);
  EXPECT_EQ(e.category(), error_category::cancelled);
  EXPECT_EQ(e.code(), "raw.cancelled");
  expect_rc_context(e, "unpack", LIBRAW_CANCELLED_BY_CALLBACK);
}

TEST(RawRobust, T_M3A_RAW_05_progress_handler_returns_nonzero_only_after_cancel) {
  cancel_token token;
  raw_decode_context ctx;
  ctx.cancel = &token;
  std::optional<int> before, after;
  raw_decode_hooks hooks;
  hooks.with_handlers = [&](const raw_unpack_callbacks& cb) {
    before = cb.progress(LIBRAW_PROGRESS_LOAD_RAW, 0, 2);
    token.request();
    after = cb.progress(LIBRAW_PROGRESS_LOAD_RAW, 1, 2);
  };
  const stop_error e = capture(small_dng(), hooks, ctx);
  ASSERT_TRUE(before.has_value());
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(*before, 0);
  EXPECT_NE(*after, 0);
  EXPECT_EQ(e.category(), error_category::cancelled);
  EXPECT_EQ(e.code(), "raw.cancelled");
}

TEST(RawRobust, T_M3A_RAW_05_unrequested_token_changes_nothing) {
  cancel_token token;
  raw_decode_context ctx;
  ctx.cancel = &token;
  validate_image(decode_raw_buffer(small_dng(), {}, ctx));
}

// ---------------------------------------------------------------------------------------------------------------------
// T-M3A-RAW-06: prenotazione nel pool prima di unpack, max_raw_memory_mb, picco di RSS registrato.
// ---------------------------------------------------------------------------------------------------------------------

TEST(RawRobust, T_M3A_RAW_06_reservation_is_taken_before_unpack_and_released_after) {
  memory_pool pool(64 * mib);
  raw_decode_context ctx;
  ctx.pool = &pool;
  std::uint64_t reserved_at_plan = 0, reserved_at_unpack = 0;
  bool plan_seen = false, plan_before_unpack = false;
  raw_decode_hooks hooks;
  hooks.before_unpack_plan = [&](const raw_unpack_plan& plan) {
    plan_seen = true;
    reserved_at_plan = pool.reserved_bytes();
    EXPECT_EQ(plan.raw_image_bytes, 1 * mib);
    EXPECT_GE(plan.reserved_bytes, small_dng_min_reservation);
  };
  hooks.libraw_return = [&](std::string_view function, int rc) {
    if (function == "unpack") {
      plan_before_unpack = plan_seen;
      reserved_at_unpack = pool.reserved_bytes();
    }
    return rc;
  };
  const image img = decode_raw_buffer(small_dng(), hooks, ctx);
  validate_image(img);
  EXPECT_TRUE(plan_before_unpack);
  EXPECT_GE(reserved_at_plan, small_dng_min_reservation);
  EXPECT_GE(reserved_at_unpack, small_dng_min_reservation);
  EXPECT_GE(pool.peak_reserved_bytes(), small_dng_min_reservation);
  EXPECT_EQ(pool.reserved_bytes(), 0u) << "la prenotazione del decoder si rilascia al ritorno";
}

TEST(RawRobust, T_M3A_RAW_06_budget_below_the_minimum_stops_before_unpack) {
  memory_pool pool(small_dng_min_reservation - 1);
  raw_decode_context ctx;
  ctx.pool = &pool;
  bool unpack_called = false;
  std::vector<std::string> calls;
  raw_decode_hooks hooks;
  hooks.before_unpack = [&] { unpack_called = true; };
  hooks.libraw_return = [&](std::string_view function, int rc) {
    calls.emplace_back(function);
    return rc;
  };
  EXPECT_STOP(decode_raw_buffer(small_dng(), hooks, ctx), error_category::budget, "sched.job_exceeds_budget");
  EXPECT_FALSE(unpack_called);
  EXPECT_EQ(calls, std::vector<std::string>{"open_buffer"});
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

// Il job tiene già i byte del file nello stesso pool (§2.8): il controllo del budget somma file e prenotazione del
// decoder. Un budget di un byte sotto la somma si ferma subito; senza la somma il decoder attenderebbe per sempre, quindi
// un cancel dopo l'attesa trasforma il guasto in un fallimento del test invece che in un blocco.
TEST(RawRobust, T_M3A_RAW_06_budget_check_adds_the_file_bytes_already_reserved_in_the_pool) {
  const std::uint64_t file_bytes = small_dng().size();
  ASSERT_LT(file_bytes, 1 * mib);
  for (const std::uint64_t budget : {small_dng_min_reservation + file_bytes - 1, small_dng_min_reservation + file_bytes}) {
    SCOPED_TRACE("budget " + std::to_string(budget));
    const bool fits = budget >= small_dng_min_reservation + file_bytes;
    memory_pool pool(budget);
    reservation input = pool.reserve(file_bytes, "input");
    cancel_token token;
    raw_decode_context ctx;
    ctx.pool = &pool;
    ctx.cancel = &token;
    std::atomic<bool> unpack_called{false};
    raw_decode_hooks hooks;
    hooks.before_unpack = [&] { unpack_called = true; };
    std::exception_ptr failure;
    std::optional<image> img;
    std::atomic<bool> finished{false};
    std::thread worker([&] {
      try {
        img = decode_raw_buffer(small_dng(), hooks, ctx);
      } catch (...) {
        failure = std::current_exception();
      }
      finished = true;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (pool.waiting_count() == 0 && std::chrono::steady_clock::now() < deadline) {
      if (finished.load()) break;
      std::this_thread::yield();
    }
    const bool waited = pool.waiting_count() != 0;
    if (waited) token.request();
    worker.join();
    EXPECT_FALSE(waited) << "il decoder non deve attendere nel pool: la somma file + prenotazione va decisa subito";
    if (fits) {
      EXPECT_TRUE(failure == nullptr);
      ASSERT_TRUE(img.has_value());
      validate_image(*img);
      EXPECT_TRUE(unpack_called.load());
    } else {
      ASSERT_TRUE(failure != nullptr);
      try {
        std::rethrow_exception(failure);
      } catch (const stop_error& e) {
        EXPECT_EQ(e.category(), error_category::budget) << e.what();
        EXPECT_EQ(e.code(), "sched.job_exceeds_budget") << e.what();
        EXPECT_EQ(ctx_value(e, "file"), std::to_string(file_bytes)) << e.what();
        EXPECT_EQ(ctx_value(e, "minimo"), std::to_string(small_dng_min_reservation + file_bytes)) << e.what();
        EXPECT_EQ(ctx_value(e, "budget"), std::to_string(budget)) << e.what();
      }
      EXPECT_FALSE(unpack_called.load());
    }
    input.release();
  }
}

TEST(RawRobust, T_M3A_RAW_06_stop_after_reservation_releases_it) {
  memory_pool pool(64 * mib);
  raw_decode_context ctx;
  ctx.pool = &pool;
  raw_decode_hooks hooks;
  hooks.libraw_return = [](std::string_view function, int rc) { return function == "unpack" ? LIBRAW_DATA_ERROR : rc; };
  EXPECT_STOP(decode_raw_buffer(small_dng(), hooks, ctx), error_category::integrity, "raw.data_error");
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

// Pool occupato da altri: la prenotazione attende in coda e un cancel durante l'attesa ferma prima di unpack.
TEST(RawRobust, T_M3A_RAW_06_cancel_while_waiting_for_the_pool_stops_before_unpack) {
  memory_pool pool(8 * mib);
  reservation other = pool.reserve(8 * mib - 100, "altro");
  cancel_token token;
  raw_decode_context ctx;
  ctx.pool = &pool;
  ctx.cancel = &token;
  std::atomic<bool> unpack_called{false};
  raw_decode_hooks hooks;
  hooks.before_unpack = [&] { unpack_called = true; };
  std::exception_ptr failure;
  std::thread worker([&] {
    try {
      (void)decode_raw_buffer(small_dng(), hooks, ctx);
    } catch (...) {
      failure = std::current_exception();
    }
  });
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (pool.waiting_count() == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
  const bool waiting = pool.waiting_count() == 1;
  token.request();
  worker.join();
  other.release();
  ASSERT_TRUE(waiting) << "il decoder doveva attendere nel pool";
  ASSERT_TRUE(failure != nullptr);
  try {
    std::rethrow_exception(failure);
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::cancelled);
    EXPECT_EQ(e.code(), "sched.cancelled");
  }
  EXPECT_FALSE(unpack_called.load());
  EXPECT_EQ(pool.reserved_bytes(), 0u);
}

// max_raw_memory_mb (§2.1): valore effettivo di LibRaw letto subito prima di unpack, esattamente M; la parte della
// prenotazione per raw_image è M * 2^20 byte. Controllo della libreria con LibRaw diretta: M passa, M - 1 dà
// LIBRAW_TOO_BIG in unpack.
TEST(RawRobust, T_M3A_RAW_06_max_raw_memory_mb_is_exactly_m_and_the_library_agrees) {
  struct row {
    std::uint32_t width, height;
    std::uint64_t m;
  };
  const row rows[] = {{64, 48, 1}, {2048, 1040, 5}, {2048, 1016, 4}, {2048, 1017, 5}, {2048, 1024, 5}};
  for (const row& r : rows) {
    SCOPED_TRACE(std::to_string(r.width) + "x" + std::to_string(r.height));
    ASSERT_EQ(expected_m(r.width, r.height), r.m);
    const std::vector<std::uint8_t> bytes = fx::make_dng(fixture_spec(r.width, r.height));

    memory_pool pool(256 * mib);
    raw_decode_context ctx;
    ctx.pool = &pool;
    std::optional<raw_unpack_plan> seen;
    raw_decode_hooks hooks;
    hooks.before_unpack_plan = [&](const raw_unpack_plan& plan) { seen = plan; };
    struct rusage before {}, after {};
    ASSERT_EQ(::getrusage(RUSAGE_SELF, &before), 0);
    const image img = decode_raw_buffer(bytes, hooks, ctx);
    ASSERT_EQ(::getrusage(RUSAGE_SELF, &after), 0);
    // Su stdout: ctest lo conserva nell'output del test e nel junit (RecordProperty non arriva nel junit di ctest).
    std::printf("RSS-PEAK T-M3A-RAW-06 %ux%u: ru_maxrss_kib_before=%ld ru_maxrss_kib_after=%ld\n", r.width, r.height,
                before.ru_maxrss, after.ru_maxrss);
    std::fflush(stdout);
    validate_image(img);
    EXPECT_EQ(img.geometry.width, r.width);
    EXPECT_EQ(img.geometry.height, r.height);
    ASSERT_TRUE(seen.has_value());
    EXPECT_EQ(std::uint64_t{seen->max_raw_memory_mb}, r.m);
    EXPECT_EQ(seen->raw_image_bytes, r.m * mib);
    EXPECT_GE(seen->reserved_bytes, reservation_bytes(r.width, r.height));

    for (const std::uint64_t limit : {r.m, r.m - 1}) {
      LibRaw lr;
      lr.imgdata.rawparams.max_raw_memory_mb = static_cast<int>(limit);
      ASSERT_EQ(lr.open_buffer(bytes.data(), bytes.size()), LIBRAW_SUCCESS);
      EXPECT_EQ(lr.unpack(), limit == r.m ? LIBRAW_SUCCESS : LIBRAW_TOO_BIG) << "limite " << limit;
    }
  }
}

