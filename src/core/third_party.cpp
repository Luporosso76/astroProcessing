#include "astro/third_party.hpp"

#include <fitsio.h>
#include <libraw/libraw.h>
#include <wcslib/wcslib.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <utility>

#include "astro/error.hpp"

namespace astro::third_party {
namespace {

// Costanti attese: una versione nuova delle librerie le cambia insieme a third_party/<nome>/ (obbligo 11).
constexpr int kLibRawVersionNumber = 0x001603;
constexpr const char* kLibRawVersion = "0.22.3-Release";
constexpr unsigned kLibRawCapabilities = LIBRAW_CAPS_ZLIB | LIBRAW_CAPS_JPEG;
constexpr const char* kCfitsioVersion = "4.070";
constexpr int kCfitsioReentrant = 1;
constexpr const char* kWcslibVersion = "8.10";
constexpr std::array<int, 3> kWcslibVers{8, 10, 0};

constexpr const char* kPointLibRawVersionNumber = "third_party.libraw_version_number";
constexpr const char* kPointCfitsioVersionMilli = "third_party.cfitsio_version_milli";

std::once_flag g_once;
std::atomic<bool> g_verified{false};
std::mutex g_injector_mutex;
fault_injector g_injector;

int injected(const char* point) {
  const std::lock_guard<std::mutex> lock(g_injector_mutex);
  return g_injector ? g_injector(point) : 0;
}

void report(const char* field, const std::string& got, const std::string& want) {
  std::fprintf(stderr, "third_party: %s osservato '%s', atteso '%s'\n", field, got.c_str(), want.c_str());
}

std::string vers_text(const std::array<int, 3>& v) {
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]);
}

observed read_libraries() {
  observed o;
  o.libraw_version_number = LibRaw::versionNumber();
  if (const int v = injected(kPointLibRawVersionNumber); v != 0) o.libraw_version_number = v;
  o.libraw_version = LibRaw::version();
  o.libraw_capabilities = libraw_capabilities();

  float fits_version = 0.0f;
  fits_get_version(&fits_version);
  char text[32];
  if (const int milli = injected(kPointCfitsioVersionMilli); milli != 0) {
    std::snprintf(text, sizeof text, "%d.%03d", milli / 1000, milli % 1000);
  } else {
    std::snprintf(text, sizeof text, "%.3f", static_cast<double>(fits_version));
  }
  o.cfitsio_version = text;
  o.cfitsio_reentrant = fits_is_reentrant();

  int vers[3] = {-1, -1, -1};
  const char* wcs = wcslib_version(vers);
  o.wcslib_version = wcs != nullptr ? wcs : "";
  o.wcslib_vers = {vers[0], vers[1], vers[2]};
  return o;
}

}  // namespace

void check(const observed& values) {
  if (values.libraw_version_number != kLibRawVersionNumber)
    report("LibRaw::versionNumber", std::to_string(values.libraw_version_number), std::to_string(kLibRawVersionNumber));
  ASTRO_ASSERT(values.libraw_version_number == kLibRawVersionNumber);
  if (values.libraw_version != kLibRawVersion) report("LibRaw::version", values.libraw_version, kLibRawVersion);
  ASTRO_ASSERT(values.libraw_version == kLibRawVersion);
  if (values.libraw_capabilities != kLibRawCapabilities)
    report("libraw_capabilities", std::to_string(values.libraw_capabilities), std::to_string(kLibRawCapabilities));
  ASTRO_ASSERT(values.libraw_capabilities == kLibRawCapabilities);
  if (values.cfitsio_version != kCfitsioVersion) report("fits_get_version", values.cfitsio_version, kCfitsioVersion);
  ASTRO_ASSERT(values.cfitsio_version == kCfitsioVersion);
  if (values.cfitsio_reentrant != kCfitsioReentrant)
    report("fits_is_reentrant", std::to_string(values.cfitsio_reentrant), std::to_string(kCfitsioReentrant));
  ASTRO_ASSERT(values.cfitsio_reentrant == kCfitsioReentrant);
  if (values.wcslib_version != kWcslibVersion) report("wcslib_version", values.wcslib_version, kWcslibVersion);
  ASTRO_ASSERT(values.wcslib_version == kWcslibVersion);
  if (values.wcslib_vers != kWcslibVers) report("wcslib_version vers", vers_text(values.wcslib_vers), vers_text(kWcslibVers));
  ASTRO_ASSERT(values.wcslib_vers == kWcslibVers);
}

void verify_once() {
  std::call_once(g_once, [] {
    check(read_libraries());
    g_verified.store(true, std::memory_order_release);
  });
}

bool verified() noexcept { return g_verified.load(std::memory_order_acquire); }

void set_fault_injector(fault_injector injector) {
  const std::lock_guard<std::mutex> lock(g_injector_mutex);
  g_injector = std::move(injector);
}

}  // namespace astro::third_party
