// T-U12-05 e T-U12-06: punto di verifica del core (docs/contracts/U12-third-party.md, obblighi 5 e 6).
// Header atteso: src/core/include/astro/third_party.hpp, con
//   struct astro::third_party::observed { int libraw_version_number; std::string libraw_version;
//     unsigned libraw_capabilities; std::string cfitsio_version /* "%.3f" */; int cfitsio_reentrant;
//     std::string wcslib_version; std::array<int, 3> wcslib_vers; };
//   void check(const observed&);   // pura, ASTRO_ASSERT sul primo valore diverso
//   void verify_once();            // legge le librerie, chiama check una volta per processo
//   bool verified();
//   void set_fault_injector(fault_injector);  // astro/fault_injector.hpp; usata da verify_once, nullptr la toglie
// I valori attesi sono scritti qui dal contratto (§4, 5 e 6), non letti dal prodotto.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

#include "astro/fits.hpp"
#include "astro/image.hpp"
#include "astro/raw_decode.hpp"
#include "astro/third_party.hpp"
#include "no_crash_report.hpp"
#include "syn_dng.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;
using observed = third_party::observed;

observed good() {
  observed o;
  o.libraw_version_number = 0x001603;
  o.libraw_version = "0.22.3-Release";
  o.libraw_capabilities = 0xC0;
  o.cfitsio_version = "4.070";
  o.cfitsio_reentrant = 1;
  o.wcslib_version = "8.10";
  o.wcslib_vers = {8, 10, 0};
  return o;
}

template <class Alter>
void expect_check_dies(Alter alter) {
  observed o = good();
  alter(o);
  EXPECT_DEATH({ exit_instead_of_core_dump(); third_party::check(o); }, "ASTRO_ASSERT");
}

TEST(U12Check, GoodValuesPass) { third_party::check(good()); }

TEST(U12Check, VersionLibRawNumberAltered) {
  for (int v : {0x001602, 0x001604, 0x011603, 0x000000}) expect_check_dies([v](observed& o) { o.libraw_version_number = v; });
}
TEST(U12Check, VersionLibRawStringAltered) {
  for (const char* v : {"0.21.5b-Release", "0.22.3", "0.22.3-Snapshot", ""})
    expect_check_dies([v](observed& o) { o.libraw_version = v; });
}
TEST(U12Check, VersionCfitsioAltered) {
  for (const char* v : {"4.060", "4.071", "4.07", ""}) expect_check_dies([v](observed& o) { o.cfitsio_version = v; });
}
TEST(U12Check, VersionWcslibStringAltered) {
  for (const char* v : {"8.4", "8.11", "8.1", ""}) expect_check_dies([v](observed& o) { o.wcslib_version = v; });
}
TEST(U12Check, VersionWcslibTripleAltered) {
  for (int i = 0; i < 3; ++i) expect_check_dies([i](observed& o) { o.wcslib_vers[static_cast<std::size_t>(i)] += 1; });
}

TEST(U12Check, CapabilitiesLibRawAltered) {
  for (unsigned v : {0x80u, 0x40u, 0x00u, 0xC1u, 0xE0u, 0x1C0u}) expect_check_dies([v](observed& o) { o.libraw_capabilities = v; });
}
TEST(U12Check, CapabilitiesCfitsioNotReentrant) {
  expect_check_dies([](observed& o) { o.cfitsio_reentrant = 0; });
}

// Un processo figlio per prova: `verified()` e' per processo e il genitore non deve mai avere chiamato verify_once.
// Esiti del figlio: 0 ok, 2 gia' vero all'inizio, 4 ancora falso dopo l'uso, 5 la libreria e' stata raggiunta con valori
// alterati; un'eccezione termina il figlio (134).
[[noreturn]] void finish(int code, const char* what) {
  if (code != 0) std::fprintf(stderr, "U12Verified: %s\n", what);
  std::_Exit(code);
}

// Punti d'iniezione del punto di verifica (nome scelto dai test, header atteso: `third_party::set_fault_injector`):
// per ciascun valore osservato un valore non nullo restituito dall'iniettore sostituisce quello letto dalla libreria
// (numero di versione, capacita', versione CFITSIO in millesimi); zero lascia il valore letto.
constexpr const char* kPointLibRawVersionNumber = "third_party.libraw_version_number";
constexpr const char* kPointCfitsioVersionMilli = "third_party.cfitsio_version_milli";

fx::dng_spec tiny_dng() {
  fx::dng_spec s;
  // 64 x 64: LibRaw 0.22.3 rifiuta in open_buffer il DNG 8 x 8 (LIBRAW_FILE_UNSUPPORTED); 64 x 64 si decodifica (sonda).
  s.width = 64;
  s.height = 64;
  s.pixels = fx::pixels_u16(std::vector<std::uint16_t>(64 * 64, 100));
  return s;
}

fx::raw_hdu tiny_hdu() {
  fx::raw_hdu h;
  h.bitpix = 16;
  h.axes = {2, 2};
  h.data = fx::bytes(8, 0);
  return h;
}

void child_decode_ok() {
  exit_instead_of_core_dump();
  if (third_party::verified()) finish(2, "verified() vero prima della decodifica");
  decode_raw_buffer(fx::make_dng(tiny_dng()));
  if (!third_party::verified()) finish(4, "verified() falso dopo la decodifica");
  finish(0, "");
}

void child_fits_ok() {
  exit_instead_of_core_dump();
  if (third_party::verified()) finish(2, "verified() vero prima dell'apertura FITS");
  decode_fits(fx::build_fits({tiny_hdu()}), std::uint64_t{1} << 30);
  if (!third_party::verified()) finish(4, "verified() falso dopo l'apertura FITS");
  finish(0, "");
}

// Valore osservato alterato: la prima decodifica si ferma con ASTRO_ASSERT prima di `open_buffer` (nessun gancio
// `after_open`/`before_unpack` raggiunto, quindi la libreria non ha letto il file).
void child_decode_bad_value() {
  exit_instead_of_core_dump();
  third_party::set_fault_injector([](std::string_view point) { return point == kPointLibRawVersionNumber ? 0x001604 : 0; });
  raw_decode_hooks hooks;
  hooks.after_open = [](raw_probe_fields&) { finish(5, "open_buffer raggiunto con valori alterati"); };
  hooks.before_unpack = [] { finish(5, "unpack raggiunto con valori alterati"); };
  decode_raw_buffer(fx::make_dng(tiny_dng()), hooks);
  finish(5, "decodifica completata con valori alterati");
}

// Valore osservato di CFITSIO alterato: la prima apertura FITS si ferma con ASTRO_ASSERT e non restituisce l'immagine.
void child_fits_bad_value() {
  exit_instead_of_core_dump();
  third_party::set_fault_injector([](std::string_view point) { return point == kPointCfitsioVersionMilli ? 4071 : 0; });
  decode_fits(fx::build_fits({tiny_hdu()}), std::uint64_t{1} << 30);
  finish(5, "apertura FITS completata con valori alterati");
}

// encode_fits come primo uso di CFITSIO: verifica prima di fits_create_memfile.
image tiny_image() { return make_image(image_geometry(4, 4, 1), sample_type::f32, plane_layout::mono, std::nullopt); }

void child_encode_ok() {
  exit_instead_of_core_dump();
  if (third_party::verified()) finish(2, "verified() vero prima della scrittura FITS");
  const std::vector<std::uint8_t> out = encode_fits(tiny_image());
  if (out.size() < 2880 || std::string_view(reinterpret_cast<const char*>(out.data()), 6) != "SIMPLE")
    finish(6, "FITS scritto non valido");
  if (!third_party::verified()) finish(4, "verified() falso dopo la scrittura FITS");
  finish(0, "");
}

// Valore CFITSIO alterato: encode_fits si ferma con ASTRO_ASSERT e non restituisce alcun file.
void child_encode_bad_value() {
  exit_instead_of_core_dump();
  third_party::set_fault_injector([](std::string_view point) { return point == kPointCfitsioVersionMilli ? 4071 : 0; });
  static_cast<void>(encode_fits(tiny_image()));
  finish(5, "scrittura FITS completata con valori alterati");
}

// Ordine LibRaw: con il guasto iniettato nessun punto raggiungibile solo dopo l'ingresso in open_buffer deve scattare:
// ne' il gancio prima di open_buffer, ne' libraw_return("open_buffer"), ne' i ganci successivi. Il buffer e' un DNG
// valido: se open_buffer fosse chiamato, libraw_return scatterebbe comunque.
void child_decode_order_before_open_buffer() {
  exit_instead_of_core_dump();
  third_party::set_fault_injector([](std::string_view point) { return point == kPointLibRawVersionNumber ? 0x001604 : 0; });
  raw_decode_hooks hooks;
  hooks.before_open = [](raw_user_params&) { finish(5, "gancio before_open raggiunto prima della verifica"); };
  hooks.libraw_return = [](std::string_view, int rc) -> int { finish(5, "open_buffer chiamato prima della verifica"); return rc; };
  hooks.after_open = [](raw_probe_fields&) { finish(5, "after_open raggiunto prima della verifica"); };
  decode_raw_buffer(fx::make_dng(tiny_dng()), hooks);
  finish(5, "decodifica completata con valori alterati");
}

TEST(U12Verified, FalseBeforeAndTrueAfterEncodeFits) { EXPECT_EXIT(child_encode_ok(), ::testing::ExitedWithCode(0), ""); }
TEST(U12VerifiedInjected, EncodeFitsStopsBeforeCfitsioWithAlteredValue) { EXPECT_DEATH(child_encode_bad_value(), "ASTRO_ASSERT"); }
TEST(U12VerifiedInjected, DecodeVerifiesBeforeEnteringOpenBuffer) { EXPECT_DEATH(child_decode_order_before_open_buffer(), "ASTRO_ASSERT"); }

TEST(U12Verified, FalseBeforeAndTrueAfterDecode) { EXPECT_EXIT(child_decode_ok(), ::testing::ExitedWithCode(0), ""); }
TEST(U12Verified, FalseBeforeAndTrueAfterFitsOpen) { EXPECT_EXIT(child_fits_ok(), ::testing::ExitedWithCode(0), ""); }

TEST(U12VerifiedInjected, DecodeStopsBeforeLibRawWithAlteredValue) { EXPECT_DEATH(child_decode_bad_value(), "ASTRO_ASSERT"); }
TEST(U12VerifiedInjected, FitsOpenStopsBeforeCfitsioWithAlteredValue) { EXPECT_DEATH(child_fits_bad_value(), "ASTRO_ASSERT"); }

}  // namespace
