# I temporanei di gtest (CaptureStderr, death test) e dei processi figli vanno in TEST_TMPDIR/TMPDIR dei preset di test, mai in /tmp.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test_tmp/sys")
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
        test_budget test_resources test_pool test_spill test_scheduler test_calibration test_calibration_job test_job_alloc
        test_inventory test_phase test_import test_recovery test_master test_blocks test_calibration_r32 test_simd)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE astrocore GTest::gtest_main)
  target_include_directories(${t} PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
  target_compile_definitions(${t} PRIVATE
    ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
  gtest_discover_tests(${t} PROPERTIES LABELS fast)
endforeach()
target_link_libraries(test_fits PRIVATE OpenSSL::Crypto)
target_link_libraries(test_blocks PRIVATE PkgConfig::CFITSIO)
target_link_libraries(test_budget PRIVATE SQLite::SQLite3)
target_link_libraries(test_resources PRIVATE SQLite::SQLite3)
foreach(t test_store test_store_history test_store_contract test_store_crash test_export test_store_review test_job_alloc
        test_import test_recovery)
  target_link_libraries(${t} PRIVATE SQLite::SQLite3)
endforeach()

# Suite di conformita' della base comune (docs/contracts/BASE-confini.md §3): T-M3BASE-01…17 e 19 per ogni voce del
# registro dei tipi di job. I casi dei debiti aperti (§4) restano rossi finche' l'unita' di codice non li paga.
add_executable(test_conformance test_conformance.cpp)
target_link_libraries(test_conformance PRIVATE astrocore GTest::gtest_main SQLite::SQLite3)
target_include_directories(test_conformance PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_conformance PRIVATE
  ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
gtest_discover_tests(test_conformance NO_PRETTY_VALUES PROPERTIES LABELS fast)

# Casi che pagano il debito «Prove» (§4): mutazioni sopravvissute alla suite sopra. Il file include test_conformance.cpp per
# il banco di prova; il filtro limita ctest ai casi nuovi.
add_executable(test_conformance_gaps test_conformance_gaps.cpp)
target_link_libraries(test_conformance_gaps PRIVATE astrocore GTest::gtest_main SQLite::SQLite3)
target_include_directories(test_conformance_gaps PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_conformance_gaps PRIVATE
  ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
gtest_discover_tests(test_conformance_gaps NO_PRETTY_VALUES TEST_FILTER "*ConformanceGaps*:HashGaps.*"
  PROPERTIES LABELS fast)

# T-M3BASE-20…22 (emendamento L1, unita' U0 di plans/M3a.md): lettura degli input indipendente dal formato. Il file
# include test_conformance.cpp per il banco di prova; il filtro limita ctest ai casi nuovi.
add_executable(test_conformance_format test_conformance_format.cpp)
target_link_libraries(test_conformance_format PRIVATE astrocore GTest::gtest_main SQLite::SQLite3)
target_include_directories(test_conformance_format PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_conformance_format PRIVATE
  ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
gtest_discover_tests(test_conformance_format NO_PRETTY_VALUES TEST_FILTER "*ConformanceFormat*"
  PROPERTIES LABELS fast)

# T-M3BASE-18: un job per ogni tipo registrato sotto strace, zero chiamate di rete.
add_executable(conformance_network conformance_network.cpp)
target_link_libraries(conformance_network PRIVATE astrocore)
target_include_directories(conformance_network PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures)
add_test(NAME T-M3BASE-18.network
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_m2f02_network.sh $<TARGET_FILE:conformance_network>
    ${PROJECT_SOURCE_DIR}/tmp/m3base-network-${CMAKE_BUILD_TYPE})
set_tests_properties(T-M3BASE-18.network PROPERTIES LABELS fast TIMEOUT 300)

# Completezza del registro dei tipi di job (§3.1, punto 3) e controllo negativo dello strumento.
add_test(NAME T-M3BASE.job_types_registry COMMAND python3 -S ${PROJECT_SOURCE_DIR}/tools/check_job_types.py ${PROJECT_SOURCE_DIR})
add_test(NAME T-M3BASE.job_types_registry_negative
  COMMAND python3 -S ${PROJECT_SOURCE_DIR}/tools/check_job_types.py --self-test ${PROJECT_SOURCE_DIR}/tmp)
set_tests_properties(T-M3BASE.job_types_registry T-M3BASE.job_types_registry_negative PROPERTIES LABELS fast TIMEOUT 60)

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
add_test(NAME T-M2E-13.real_palomar_lfc COMMAND test_calibration_real --gtest_filter=CalibReal.T_M2E_13_*)
set_tests_properties(T-M2E-13.real_palomar_lfc PROPERTIES LABELS slow TIMEOUT 1800
  FAIL_REGULAR_EXPRESSION "\\] 0 tests from 0 test suites ran")
# T-M2R2-09: master del prodotto dai frame grezzi di PUB-LFC-01, poi T-M2E-13 su quei master.
add_test(NAME T-M2R2-09.real_palomar_product_masters COMMAND test_calibration_real --gtest_filter=CalibReal.T_M2R2_09_*)
set_tests_properties(T-M2R2-09.real_palomar_product_masters PROPERTIES LABELS slow TIMEOUT 1800
  FAIL_REGULAR_EXPRESSION "\\] 0 tests from 0 test suites ran")

# T-M2R2-04 e T-M2E-21 sui master del prodotto: Monte Carlo con seed fisso. Lunghi: `ctest -L slow`.
add_executable(test_master_stats test_master_stats.cpp)
target_link_libraries(test_master_stats PRIVATE astrocore GTest::gtest_main)
target_include_directories(test_master_stats PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_master_stats PRIVATE ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
add_test(NAME T-M2R2-04.monte_carlo COMMAND test_master_stats --gtest_filter=MasterStats.chi2_*:MasterStats.T_M2R2_04_*)
set_tests_properties(T-M2R2-04.monte_carlo PROPERTIES LABELS slow TIMEOUT 1800
  FAIL_REGULAR_EXPRESSION "\\] 0 tests from 0 test suites ran")
add_test(NAME T-M2E-21.product_masters_monte_carlo COMMAND test_master_stats --gtest_filter=MasterStats.T_M2E_21_*)
set_tests_properties(T-M2E-21.product_masters_monte_carlo PROPERTIES LABELS slow TIMEOUT 1800
  FAIL_REGULAR_EXPRESSION "\\] 0 tests from 0 test suites ran")

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

# U1 (M3.a): tipi del decoder (ADR-03.b §b.1, §b.2, §b.8, §b.9).
add_executable(test_decoder_types test_decoder_types.cpp)
target_link_libraries(test_decoder_types PRIVATE astrocore GTest::gtest_main)
target_include_directories(test_decoder_types PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
gtest_discover_tests(test_decoder_types PROPERTIES LABELS fast)

# U10 (G-PACK parte 1): T-M3A-PACK-01…05, 07, 09 e T-M3A-LIC-01 su verify_pack (src/core/pack.cpp).
add_executable(test_pack test_pack.cpp)
target_link_libraries(test_pack PRIVATE astrocore GTest::gtest_main SQLite::SQLite3 OpenSSL::Crypto ${CMAKE_DL_LIBS})
target_include_directories(test_pack PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
target_compile_definitions(test_pack PRIVATE
  ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
gtest_discover_tests(test_pack NO_PRETTY_VALUES PROPERTIES LABELS fast)

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
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_master.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_simd.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_master_stats.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_import_real.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_recovery.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_calibration_stats.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_calibration_real.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/m2e_workflow.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_m1d_protocol.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_conformance.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_conformance_gaps.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_conformance_format.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/conformance_network.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_decoder_types.dir
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_pack.dir)
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

# T-M2R5-01 (ADR-07, aggiunte di R5): le suite di calibrazione (intera e a blocchi), del job e del master sul secondo percorso.
foreach(t test_calibration test_calibration_job test_master test_calibration_r32)
  add_test(NAME T-M2R5-01.avx2.${t} COMMAND ${t})
  set_tests_properties(T-M2R5-01.avx2.${t} PROPERTIES LABELS fast TIMEOUT 1800 ENVIRONMENT ASTRO_TEST_SIMD=avx2)
endforeach()

# T-M2R5-05 e T-M2R5-07 (parte binaria): istruzioni vietate nei kernel, crtfastmath e fast-math nella build.
set(ASTRO_KERNEL_OBJECTS ${CMAKE_BINARY_DIR}/src/core/CMakeFiles/astrocore.dir)
add_test(NAME T-M2R5-05.kernel_instructions
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_simd_binary.sh objects
    ${ASTRO_KERNEL_OBJECTS}/kernels_scalar.cpp.o ${ASTRO_KERNEL_OBJECTS}/kernels_avx2.cpp.o)
add_test(NAME T-M2R5-05.kernel_instructions_negative
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_simd_binary.sh --self-test objects ${CMAKE_CXX_COMPILER})
add_test(NAME T-M2R5-07.no_crtfastmath
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_simd_binary.sh fastmath
    ${CMAKE_BINARY_DIR}/build.ninja ${CMAKE_BINARY_DIR}/CMakeFiles/rules.ninja --
    $<TARGET_FILE:test_simd> $<TARGET_FILE:test_calibration> $<TARGET_FILE:test_master>)
add_test(NAME T-M2R5-07.no_crtfastmath_negative
  COMMAND ${PROJECT_SOURCE_DIR}/tools/check_simd_binary.sh --self-test fastmath ${CMAKE_CXX_COMPILER})
set_tests_properties(T-M2R5-05.kernel_instructions T-M2R5-05.kernel_instructions_negative
  T-M2R5-07.no_crtfastmath T-M2R5-07.no_crtfastmath_negative PROPERTIES LABELS fast TIMEOUT 120)

# Self-test degli strumenti di processo (tools/trace.py, tools/lint.sh, tools/check_m2_trace.py) su dati sintetici in tmp/.
add_test(NAME TOOLS.trace_selftest COMMAND python3 -S ${PROJECT_SOURCE_DIR}/tools/trace.py selftest)
add_test(NAME TOOLS.lint_selftest COMMAND ${PROJECT_SOURCE_DIR}/tools/lint.sh --selftest)
add_test(NAME TOOLS.check_m2_trace COMMAND python3 -S ${PROJECT_SOURCE_DIR}/tests/tools/test_check_m2_trace.py)
set_tests_properties(TOOLS.trace_selftest TOOLS.lint_selftest TOOLS.check_m2_trace PROPERTIES LABELS fast TIMEOUT 120)
