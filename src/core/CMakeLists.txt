string(TOUPPER "${CMAKE_BUILD_TYPE}" ASTRO_BUILD_TYPE_UPPER)
string(JOIN " " ASTRO_BUILD_FLAGS
  "-std=c++20" ${ASTRO_WARNING_FLAGS} ${ASTRO_FP_FLAGS}
  "${CMAKE_CXX_FLAGS}" "${CMAKE_CXX_FLAGS_${ASTRO_BUILD_TYPE_UPPER}}")
string(STRIP "${ASTRO_BUILD_FLAGS}" ASTRO_BUILD_FLAGS)
set(ASTRO_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

configure_file(build_info_generated.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/build_info_generated.cpp @ONLY)

add_library(astrocore STATIC
  build_info.cpp
  error.cpp
  image.cpp
  fits.cpp
  fits_scan.cpp
  fits_header.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/build_info_generated.cpp)
target_include_directories(astrocore PUBLIC include)
target_compile_features(astrocore PUBLIC cxx_std_20)
target_link_libraries(astrocore PUBLIC PkgConfig::CFITSIO)
