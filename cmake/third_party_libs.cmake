# Librerie compilate nel progetto (contratto U12): tarball upstream in third_party/<nome>/, costruiti dentro build/ con
# ExternalProject, collegati staticamente. Nessuna ricerca della versione di sistema e nessun accesso alla rete.
include(ExternalProject)

set(ASTRO_TP_ROOT "${CMAKE_BINARY_DIR}/third_party")
set(ASTRO_TP_JOBS 4 CACHE STRING "Processi paralleli per la build delle librerie di third_party")
set(ASTRO_TP_CC "${CMAKE_C_COMPILER}")
set(ASTRO_TP_CXX "${CMAKE_CXX_COMPILER}")

# DOWNLOAD_EXTRACT_TIMESTAMP TRUE: i file estratti mantengono le date del tarball. Con la data di estrazione i file generati
# di autotools (configure, Makefile.in) risultano piu' vecchi dei sorgenti e `make` rilancia `./configure` senza le
# opzioni del prodotto (prefisso e utilita' di WCSLIB diversi).

# libreria statica importata: header e archivio nel prefisso d'installazione sotto build/
function(astro_tp_import target project dir archive include_dir)
  set(prefix "${ASTRO_TP_ROOT}/${dir}")
  file(MAKE_DIRECTORY "${prefix}/${include_dir}")
  add_library(${target} STATIC IMPORTED GLOBAL)
  set_target_properties(${target} PROPERTIES
    IMPORTED_LOCATION "${prefix}/lib/${archive}"
    INTERFACE_INCLUDE_DIRECTORIES "${prefix}/${include_dir}")
  add_dependencies(${target} ${project})
  set_property(TARGET ${target} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${ARGN})
endfunction()

# LibRaw 0.22.3 (LGPL-2.1): variante libraw_r senza OpenMP, solo zlib e JPEG.
ExternalProject_Add(libraw_project
  PREFIX "${ASTRO_TP_ROOT}/libraw"
  URL "${CMAKE_SOURCE_DIR}/third_party/libraw/LibRaw-0.22.3.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL_HASH SHA256=789f3c33466210a1a0907f747a97c715d21bbf4af712f257fcbbd90111b227e4
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    --enable-static --disable-shared --with-pic --disable-openmp --enable-zlib --enable-jpeg --disable-lcms
    --disable-examples "CC=${ASTRO_TP_CC}" "CXX=${ASTRO_TP_CXX}" "CFLAGS=-O2 -g" "CXXFLAGS=-O2 -g"
  BUILD_COMMAND make -j${ASTRO_TP_JOBS}
  INSTALL_COMMAND make install
  BUILD_BYPRODUCTS "${ASTRO_TP_ROOT}/libraw/lib/libraw_r.a"
)

# CFITSIO 4.7.0 (licenza NASA): senza curl, bzip2 e driver di rete, con pthread (fits_is_reentrant() = 1).
ExternalProject_Add(cfitsio_project
  PREFIX "${ASTRO_TP_ROOT}/cfitsio"
  URL "${CMAKE_SOURCE_DIR}/third_party/cfitsio/cfitsio-4.7.0.tar.gz"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL_HASH SHA256=ce573bbea8e75b429f8c3d3e86498741ba3dc9628a1530d2f65268397ad059e8
  CMAKE_GENERATOR "${CMAKE_GENERATOR}"
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON "-DCMAKE_C_COMPILER=${ASTRO_TP_CC}"
    -DBUILD_SHARED_LIBS=OFF -DUSE_CURL=OFF -DUSE_BZIP2=OFF -DUSE_PTHREADS=ON -DTESTS=OFF -DUTILS=OFF
    -DCMAKE_HAVE_GETHOSTBYNAME=OFF -DCMAKE_HAVE_CONNECT=OFF
  BUILD_BYPRODUCTS "${ASTRO_TP_ROOT}/cfitsio/lib/libcfitsio.a"
)

# WCSLIB 8.10 (LGPL-3.0-or-later): senza CFITSIO, PGPLOT, Fortran e utilità; si collega libwcs.a.
ExternalProject_Add(wcslib_project
  PREFIX "${ASTRO_TP_ROOT}/wcslib"
  URL "${CMAKE_SOURCE_DIR}/third_party/wcslib/wcslib-8.10.tar.bz2"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL_HASH SHA256=b394d3306da9a52410f056136911496e599b5c48f1cff2f221563697077bbd8d
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    --without-cfitsio --without-pgplot --disable-fortran --disable-utils
    "CC=${ASTRO_TP_CC}" "CFLAGS=-O2 -g -fPIC"
  BUILD_COMMAND make -j${ASTRO_TP_JOBS}
  # `make install` crea libwcs.a come collegamento a libwcs-8.10.a: il prodotto collega un file regolare libwcs.a.
  INSTALL_COMMAND make install
    COMMAND ${CMAKE_COMMAND} -E rm -f <INSTALL_DIR>/lib/libwcs.a
    COMMAND ${CMAKE_COMMAND} -E copy <INSTALL_DIR>/lib/libwcs-8.10.a <INSTALL_DIR>/lib/libwcs.a
  BUILD_BYPRODUCTS "${ASTRO_TP_ROOT}/wcslib/lib/libwcs.a"
)

find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(Threads REQUIRED)

astro_tp_import(astro_libraw libraw_project libraw libraw_r.a include JPEG::JPEG ZLIB::ZLIB m Threads::Threads)
astro_tp_import(astro_cfitsio cfitsio_project cfitsio libcfitsio.a include ZLIB::ZLIB m Threads::Threads)
astro_tp_import(astro_wcslib wcslib_project wcslib libwcs.a include m)
