// T-U12-05 e T-U12-06: stampa i valori che il processo vede dalle tre librerie collegate staticamente.
#include <cstdio>

#include <fitsio.h>
#include <libraw/libraw.h>
#include <wcslib/wcslib.h>

int main() {
  float fits_version = 0;
  fits_get_version(&fits_version);
  int vers[3] = {-1, -1, -1};
  const char* wcs_version = wcslib_version(vers);
  std::printf("libraw_version_number=%d\n", LibRaw::versionNumber());
  std::printf("libraw_version=%s\n", LibRaw::version());
  std::printf("libraw_capabilities=%u\n", libraw_capabilities());
  std::printf("cfitsio_version=%.3f\n", static_cast<double>(fits_version));
  std::printf("cfitsio_reentrant=%d\n", fits_is_reentrant());
  std::printf("wcslib_version=%s\n", wcs_version);
  std::printf("wcslib_vers=%d,%d,%d\n", vers[0], vers[1], vers[2]);
  return 0;
}
