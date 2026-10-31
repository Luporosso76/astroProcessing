cmake_minimum_required(VERSION 3.25)
project(astroProcessor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# GoogleTest si compila dalla copia in third_party: il build non usa installazioni di sistema per i test.
set(ASTRO_GTEST_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/googletest")
if(NOT EXISTS "${ASTRO_GTEST_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Sorgenti GoogleTest assenti in ${ASTRO_GTEST_SOURCE_DIR}")
endif()
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${ASTRO_GTEST_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/googletest" EXCLUDE_FROM_ALL SYSTEM)

set(ASTRO_WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror)
set(ASTRO_FP_FLAGS -fno-fast-math -ffp-contract=off)
add_compile_options(${ASTRO_WARNING_FLAGS} ${ASTRO_FP_FLAGS})

find_package(PkgConfig REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3)
pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)

# LibRaw, CFITSIO e WCSLIB: tarball in third_party/, compilati in build/ e collegati staticamente (U12).
include(cmake/third_party_libs.cmake)

enable_testing()
add_subdirectory(src/core)
add_subdirectory(tests/unit)
