string(TOUPPER "${CMAKE_BUILD_TYPE}" ASTRO_BUILD_TYPE_UPPER)
string(JOIN " " ASTRO_BUILD_FLAGS
  "-std=c++20" ${ASTRO_WARNING_FLAGS} ${ASTRO_FP_FLAGS}
  "${CMAKE_CXX_FLAGS}" "${CMAKE_CXX_FLAGS_${ASTRO_BUILD_TYPE_UPPER}}")
string(STRIP "${ASTRO_BUILD_FLAGS}" ASTRO_BUILD_FLAGS)
set(ASTRO_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

configure_file(build_info_generated.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/build_info_generated.cpp @ONLY)

set(ASTRO_SOURCE_REVISION "" CACHE STRING "Identificatore del sorgente quando non e' un checkout git")
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
  RESULT_VARIABLE ASTRO_GIT_RC OUTPUT_QUIET ERROR_QUIET)
if(ASTRO_SOURCE_REVISION AND ASTRO_GIT_RC EQUAL 0)
  message(FATAL_ERROR "ASTRO_SOURCE_REVISION passato in un checkout git: due fonti in conflitto")
endif()
if(NOT ASTRO_SOURCE_REVISION AND NOT ASTRO_GIT_RC EQUAL 0)
  message(FATAL_ERROR "Il sorgente non e' un checkout git: passare -DASTRO_SOURCE_REVISION=<id>")
endif()
if(ASTRO_SOURCE_REVISION AND NOT ASTRO_SOURCE_REVISION MATCHES "^[A-Za-z0-9._+-]+$")
  message(FATAL_ERROR "ASTRO_SOURCE_REVISION non valido: ammessi [A-Za-z0-9._+-]")
endif()
set(ASTRO_REVISION_FILE ${CMAKE_CURRENT_BINARY_DIR}/source_revision.cpp)
add_custom_target(astro_source_revision
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DOUTPUT=${ASTRO_REVISION_FILE}
    -DEXPLICIT_REVISION=${ASTRO_SOURCE_REVISION} -P ${CMAKE_SOURCE_DIR}/cmake/source_revision.cmake
  BYPRODUCTS ${ASTRO_REVISION_FILE}
  VERBATIM)

add_library(astrocore STATIC
  build_info.cpp
  error.cpp
  image.cpp
  fits.cpp
  fits_scan.cpp
  fits_header.cpp
  hash.cpp
  json.cpp
  json_parse.cpp
  node.cpp
  dag_store.cpp
  job.cpp
  budget.cpp
  memory_pool.cpp
  spill.cpp
  checkpoint.cpp
  scheduler.cpp
  provenance.cpp
  sqlite_internal.cpp
  sqlite_project.cpp
  sqlite_dag_store.cpp
  sqlite_files.cpp
  sqlite_history.cpp
  project_export.cpp
  project_verify.cpp
  calibration.cpp
  calibration_report.cpp
  calibration_job.cpp
  operations.cpp
  inventory.cpp
  thumbnail_cache.cpp
  import_job.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/build_info_generated.cpp
  ${ASTRO_REVISION_FILE})
add_dependencies(astrocore astro_source_revision)
target_include_directories(astrocore PUBLIC include)
target_compile_features(astrocore PUBLIC cxx_std_20)
target_link_libraries(astrocore PUBLIC PkgConfig::CFITSIO)
target_link_libraries(astrocore PRIVATE OpenSSL::Crypto SQLite::SQLite3)
