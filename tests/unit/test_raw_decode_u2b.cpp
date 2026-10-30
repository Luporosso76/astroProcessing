// U2b (M3.a): fuso del processo e avviso PARSEFUJI_PROCESSED. T-M3A-RAW-01, -05 (docs/contracts/ADR-08-decoder-pack.md §2.5,
// §2.6, §6).
// Estensioni d'interfaccia attese: raw_unpack_fields::process_warnings (unsigned, letto dopo unpack, modificabile dal gancio
// after_unpack) e decoder_state::process_warnings (std::uint32_t: bit di avviso ammessi e registrati, 0 se nessuno).
// Il main (main_utc0.cpp) imposta TZ=UTC0; i death test cambiano TZ (o falsano mktime, interposta qui) solo nel figlio.
#include <dlfcn.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "astro/image.hpp"
#include "astro/raw_decode.hpp"
#include "expect_stop.hpp"
#include "no_crash_report.hpp"
#include "syn_dng.hpp"

// mktime del processo di test, interposta: con `g_wrong_month >= 0` il risultato per la data di riferimento di §2.6 di quel
// mese (mese 0 = 15 gennaio 2026, mese 6 = 15 luglio 2026, ore 12:00) e' spostato di un'ora; altrimenti e' quello di libc.
// Prova ciascuno dei due controlli di mktime separatamente, con TZ gia' uguale a "UTC0".
int g_wrong_month = -1;

extern "C" time_t mktime(std::tm* t) noexcept {
  using real_fn = time_t (*)(std::tm*);
  static const real_fn real = reinterpret_cast<real_fn>(::dlsym(RTLD_NEXT, "mktime"));
  const bool reference = t->tm_year == 126 && t->tm_mday == 15 && t->tm_hour == 12 && t->tm_min == 0 && t->tm_sec == 0;
  const int month = t->tm_mon;
  const time_t value = real(t);
  return (reference && month == g_wrong_month) ? value + 3600 : value;
}

namespace {

using namespace astro;
namespace fx = astro_fixture;

constexpr unsigned parsefuji_processed = 1u << 16;  // LIBRAW_WARN_PARSEFUJI_PROCESSED (libraw_const.h riga 695)
constexpr unsigned no_metadata = 1u << 3;           // LIBRAW_WARN_NO_METADATA
constexpr unsigned no_jpeglib = 1u << 4;            // LIBRAW_WARN_NO_JPEGLIB

std::vector<std::uint8_t> small_dng() {
  fx::dng_spec s;
  s.width = 64;
  s.height = 48;
  s.white = 4000;
  s.datetime_original = "2026:10:30 12:34:56";
  std::vector<std::uint16_t> v(64u * 48u);
  for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint16_t>((i * 7) % 4000);
  s.pixels = fx::pixels_u16(v);
  return fx::make_dng(s);
}

image decode_with_warnings(unsigned bits) {
  raw_decode_hooks hooks;
  hooks.after_unpack = [bits](raw_unpack_fields& f) { f.process_warnings = bits; };
  return decode_raw_buffer(small_dng(), hooks);
}

// T-M3A-RAW-05 (§2.5): PARSEFUJI_PROCESSED iniettato da solo non ferma; l'immagine porta il bit registrato.
TEST(RawDecodeU2b, T_M3A_RAW_05_parsefuji_alone_decodes_with_bit_recorded) {
  const image img = decode_with_warnings(parsefuji_processed);
  validate_image(img);
  ASSERT_TRUE(img.decoder_state.has_value());
  EXPECT_EQ(img.decoder_state->process_warnings, parsefuji_processed);
}

// T-M3A-RAW-05 (§2.5): insieme a un altro bit ammesso, entrambi registrati.
TEST(RawDecodeU2b, T_M3A_RAW_05_parsefuji_with_allowed_bit_records_both) {
  const image img = decode_with_warnings(parsefuji_processed | no_metadata);
  validate_image(img);
  ASSERT_TRUE(img.decoder_state.has_value());
  EXPECT_EQ(img.decoder_state->process_warnings, parsefuji_processed | no_metadata);
}

// T-M3A-RAW-05 (§2.5): insieme a un bit non ammesso lo stop e' per quel bit, con il suo nome nel contesto.
TEST(RawDecodeU2b, T_M3A_RAW_05_parsefuji_with_disallowed_bit_stops_for_the_other) {
  try {
    (void)decode_with_warnings(parsefuji_processed | no_jpeglib);
    ADD_FAILURE() << "stop_error atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::unsupported);
    EXPECT_EQ(e.code(), "raw.warning");
    bool names_jpeglib = false, names_parsefuji = false;
    for (const auto& [k, v] : e.context()) {
      if (v.find("NO_JPEGLIB") != std::string::npos) names_jpeglib = true;
      if (v.find("PARSEFUJI_PROCESSED") != std::string::npos) names_parsefuji = true;
    }
    EXPECT_TRUE(names_jpeglib) << e.what();
    EXPECT_FALSE(names_parsefuji) << e.what();
  }
}

// Senza avvisi il campo registrato e' 0.
TEST(RawDecodeU2b, T_M3A_RAW_05_no_warning_records_zero) {
  const image img = decode_with_warnings(0);
  ASSERT_TRUE(img.decoder_state.has_value());
  EXPECT_EQ(img.decoder_state->process_warnings, 0u);
}

// Decodifica con sentinella: se il decoder arriva a open_buffer senza aver verificato TZ e mktime il gancio before_open (chiamato prima di open_buffer) termina il processo con uscita 0, che
// il death test non accetta come morte; l'asserzione deve quindi precedere open_buffer.
void decode_expecting_assert_before_open() {
  raw_decode_hooks hooks;
  hooks.before_open = [](raw_user_params&) { std::_Exit(0); };
  (void)decode_raw_buffer(small_dng(), hooks);
  std::_Exit(0);
}

// T-M3A-RAW-01 (§2.6): prima di open_buffer il decoder verifica che TZ valga esattamente "UTC0"; altrimenti ASTRO_ASSERT.
// Il cambio di TZ avviene solo nel processo figlio del death test.
TEST(RawDecodeU2bDeathTest, T_M3A_RAW_01_tz_must_be_utc0) {
  const auto run = [](const char* tz) {
    return [tz] {
      exit_instead_of_core_dump();
      if (tz != nullptr) setenv("TZ", tz, 1);
      else unsetenv("TZ");
      tzset();
      decode_expecting_assert_before_open();
    };
  };
  EXPECT_DEATH(run("Europe/Rome")(), "ASTRO_ASSERT");
  EXPECT_DEATH(run("UTC")(), "ASTRO_ASSERT");
  EXPECT_DEATH(run("")(), "ASTRO_ASSERT");
  EXPECT_DEATH(run(nullptr)(), "ASTRO_ASSERT");
}

// T-M3A-RAW-01 (§2.6): con TZ uguale a "UTC0" ma mktime sbagliata su uno solo dei due riferimenti (gennaio, luglio),
// ASTRO_ASSERT prima di open_buffer.
TEST(RawDecodeU2bDeathTest, T_M3A_RAW_01_each_mktime_reference_is_checked) {
  const auto run = [](int month) {
    return [month] {
      exit_instead_of_core_dump();
      g_wrong_month = month;
      decode_expecting_assert_before_open();
    };
  };
  EXPECT_DEATH(run(0)(), "ASTRO_ASSERT");
  EXPECT_DEATH(run(6)(), "ASTRO_ASSERT");
}

// Controllo positivo del death test: con TZ=UTC0 (main di test) la stessa decodifica riesce, e il valore e' timegm dei campi.
TEST(RawDecodeU2b, T_M3A_RAW_01_utc0_decodes) {
  const image img = decode_raw_buffer(small_dng());
  ASSERT_TRUE(img.metadata.capture_time_libraw.has_value());
  EXPECT_EQ(img.metadata.capture_time_libraw->value, 1793363696);
}

}  // namespace
