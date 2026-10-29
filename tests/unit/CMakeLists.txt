include(GoogleTest)

# Ogni test ha l'etichetta `fast` (ciclo veloce, selezionata dai preset di test) oppure `slow` (`ctest -L slow`).

add_executable(test_build_info test_build_info.cpp)
target_link_libraries(test_build_info PRIVATE astrocore GTest::gtest_main)
if(ASTRO_SOURCE_REVISION)
  target_compile_definitions(test_build_info PRIVATE ASTRO_EXPLICIT_REVISION=1)
endif()
gtest_discover_tests(test_build_info PROPERTIES LABELS fast)

add_executable(test_deps test_deps.cpp)
target_link_libraries(test_deps PRIVATE
  PkgConfig::CFITSIO SQLite::SQLite3 OpenSSL::Crypto
  PkgConfig::FFTW3 PkgConfig::FFTW3F GTest::gtest_main)
gtest_discover_tests(test_deps PROPERTIES LABELS fast)

foreach(t test_error test_image test_fits test_json test_hash test_dag test_job test_store test_store_history
        test_store_contract test_store_crash test_export test_store_review
        test_budget test_pool test_spill test_scheduler test_calibration test_calibration_job test_job_alloc
        test_inventory test_phase test_import)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE astrocore GTest::gtest_main)
  target_include_directories(${t} PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
  target_compile_definitions(${t} PRIVATE
    ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
  gtest_discover_tests(${t} PROPERTIES LABELS fast)
endforeach()
target_link_libraries(test_fits PRIVATE OpenSSL::Crypto)
target_link_libraries(test_budget PRIVATE SQLite::SQLite3)
foreach(t test_store test_store_history test_store_contract test_store_crash test_export test_store_review test_job_alloc
        test_import)
  target_link_libraries(${t} PRIVATE SQLite::SQLite3)
endforeach()

# T-M2E-21 e T-M2E-26: Monte Carlo con seed fisso. Lunghi: `ctest -L slow`.
add_executable(test_calibration_stats test_calibration_stats.cpp)
target_link_libraries(test_calibration_stats PRIVATE astrocore GTest::gtest_main)
target_include_directories(test_calibration_stats PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_calibration_stats PRIVATE ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
add_test(NAME T-M2E-21-26.monte_carlo COMMAND test_calibration_stats)
set_tests_properties(T-M2E-21-26.monte_carlo PROPERTIES LABELS slow TIMEOUT 1800)

# T-M2E-13: dato reale PUB-LFC-01 (non versionato: il test fallisce se manca). Lungo: `ctest -L slow`.
add_executable(test_calibration_real test_calibration_real.cpp)
target_link_libraries(test_calibration_real PRIVATE astrocore GTest::gtest_main)
target_include_directories(test_calibration_real PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_calibration_real PRIVATE
  ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  ASTRO_TEST_TMP="${PROJECT_SOURCE_DIR}/tmp/m2e-real"
  ASTRO_REAL_OUT="${PROJECT_SOURCE_DIR}/tmp/m2e-real/report")
add_test(NAME T-M2E-13.real_palomar_lfc COMMAND test_calibration_real)
set_tests_properties(T-M2E-13.real_palomar_lfc PROPERTIES LABELS slow TIMEOUT 1800)

# T-M2R1-09: campioni pubblici PUB-LFC-01 e PUB-CFA-01 (non versionati: il test fallisce se mancano). Lungo: `ctest -L slow`.
add_executable(test_import_real test_import_real.cpp)
target_link_libraries(test_import_real PRIVATE astrocore GTest::gtest_main)
target_include_directories(test_import_real PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_import_real PRIVATE
  ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  ASTRO_TEST_TMP="${PROJECT_SOURCE_DIR}/tmp/r1-real")
add_test(NAME T-M2R1-09.public_samples COMMAND test_import_real)
set_tests_properties(T-M2R1-09.public_samples PROPERTIES LABELS slow TIMEOUT 1800)

# T-M2F-02: workflow M2.e completo sotto strace, zero chiamate di rete.
add_executable(m2e_workflow m2e_workflow.cpp)
target_link_libraries(m2e_workflow PRIVATE astrocore GTest::gtest)
target_include_directories(m2e_workflow PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(m2e_workflow PRIVATE ASTRO_TEST_TMP="${PROJECT_SOURCE_DIR}/tmp/m2e-workflow")
add_test(NAME T-M2F-02.m2e_network
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_m2f02_network.sh $<TARGET_FILE:m2e_workflow>
    ${PROJECT_SOURCE_DIR}/tmp/m2e-network-${CMAKE_BUILD_TYPE})
set_tests_properties(T-M2F-02.m2e_network PROPERTIES LABELS fast TIMEOUT 300)

# T-M2D-13: protocollo M1.d sul prodotto. Lungo: fuori dal ciclo veloce, si lancia con `ctest -L slow`.
add_executable(test_m1d_protocol test_m1d_protocol.cpp)
target_link_libraries(test_m1d_protocol PRIVATE astrocore GTest::gtest_main)
target_include_directories(test_m1d_protocol PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_m1d_protocol PRIVATE
  ASTRO_TEST_TMP="${PROJECT_SOURCE_DIR}/tmp/m2d-protocol"
  ASTRO_PROTOCOL_OUT="${PROJECT_SOURCE_DIR}/tmp/m2d-protocol")
add_test(NAME T-M2D-13.m1d_protocol COMMAND test_m1d_protocol)
set_tests_properties(T-M2D-13.m1d_protocol PROPERTIES LABELS slow TIMEOUT 1800)

add_test(NAME T-M2F-01.offline
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_offline.sh
    $<TARGET_FILE:astrocore>
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_build_info.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_deps.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_error.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_image.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_fits.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_json.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_hash.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_dag.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_job.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_store.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_store_history.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_store_contract.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_store_crash.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_export.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_store_review.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_budget.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_pool.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_spill.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_scheduler.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_calibration.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_calibration_job.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_job_alloc.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_inventory.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_phase.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_import.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_import_real.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_calibration_stats.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_calibration_real.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/m2e_workflow.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_m1d_protocol.dir)
set_tests_properties(T-M2F-01.offline PROPERTIES LABELS fast)

# T-M2R0-01 (NFR-LIC-01.c): ogni libreria caricata dai binari di test è in docs/licenses.md con file, hash e licenza.
find_program(ASTRO_PYTHON3 python3 REQUIRED)
add_test(NAME T-M2R0-01.licenses
  COMMAND ${ASTRO_PYTHON3} -S ${PROJECT_SOURCE_DIR}/tools/check_licenses.py
    ${PROJECT_SOURCE_DIR}/docs/licenses.md ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME T-M2R0-01.licenses_negative
  COMMAND ${ASTRO_PYTHON3} -S ${PROJECT_SOURCE_DIR}/tools/check_licenses.py --self-test
    ${PROJECT_SOURCE_DIR}/docs/licenses.md ${CMAKE_CURRENT_BINARY_DIR})

# T-M2R0-02 (NFR-SEC-02): nessun simbolo di esecuzione di processi o di caricamento dinamico nel core.
add_test(NAME T-M2R0-02.noexec
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_noexec.sh $<TARGET_FILE:astrocore>)
add_test(NAME T-M2R0-02.noexec_negative
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_noexec.sh --self-test ${CMAKE_CXX_COMPILER})

# T-M2R0-04 (ADR-02, D-04): il core è headless, senza dipendenze dal toolkit grafico.
add_test(NAME T-M2R0-04.headless
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_core_headless.sh $<TARGET_FILE:astrocore> $<TARGET_FILE:test_build_info>)
add_test(NAME T-M2R0-04.headless_negative
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_core_headless.sh --self-test ${CMAKE_CXX_COMPILER})

set_tests_properties(
  T-M2R0-01.licenses T-M2R0-01.licenses_negative
  T-M2R0-02.noexec T-M2R0-02.noexec_negative
  T-M2R0-04.headless T-M2R0-04.headless_negative
  PROPERTIES LABELS fast TIMEOUT 120)

# Self-test degli strumenti di processo (tools/trace.py, tools/lint.sh) su dati sintetici in tmp/.
add_test(NAME TOOLS.trace_selftest COMMAND python3 -S ${PROJECT_SOURCE_DIR}/tools/trace.py selftest)
add_test(NAME TOOLS.lint_selftest COMMAND ${PROJECT_SOURCE_DIR}/tools/lint.sh --selftest)
set_tests_properties(TOOLS.trace_selftest TOOLS.lint_selftest PROPERTIES LABELS fast TIMEOUT 120)
