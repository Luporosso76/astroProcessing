// T-U12-03: le versioni fissate nei header risolti sotto build/. Compilata con -fsyntax-only da test_u12_03_headers.py.
#include <fitsio.h>
#include <libraw/libraw.h>
#include <wcslib/wcsconfig.h>
#include <wcslib/wcslib.h>

static_assert(LIBRAW_VERSION == LIBRAW_MAKE_VERSION(0, 22, 3), "LibRaw non e' 0.22.3");
static_assert(CFITSIO_MAJOR == 4, "CFITSIO_MAJOR non e' 4");
static_assert(CFITSIO_MINOR == 7, "CFITSIO_MINOR non e' 7");
static_assert(CFITSIO_MICRO == 0, "CFITSIO_MICRO non e' 0");
static_assert(WCSLIB_VERSION == 8.10, "WCSLIB non e' 8.10");
