include(GoogleTest)

add_executable(test_build_info test_build_info.cpp)
target_link_libraries(test_build_info PRIVATE astrocore GTest::gtest_main)
if(ASTRO_SOURCE_REVISION)
  target_compile_definitions(test_build_info PRIVATE ASTRO_EXPLICIT_REVISION=1)
endif()
gtest_discover_tests(test_build_info)

add_executable(test_deps test_deps.cpp)
target_link_libraries(test_deps PRIVATE
  PkgConfig::CFITSIO SQLite::SQLite3 OpenSSL::Crypto
  PkgConfig::FFTW3 PkgConfig::FFTW3F GTest::gtest_main)
gtest_discover_tests(test_deps)

foreach(t test_error test_image test_fits test_json test_hash test_dag test_job test_store test_store_history
        test_store_contract test_store_crash test_export test_store_review)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE astrocore GTest::gtest_main)
  target_include_directories(${t} PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/src/core)
  target_compile_definitions(${t} PRIVATE
    ASTRO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    ASTRO_TEST_TMP="${CMAKE_BINARY_DIR}/test_tmp")
  gtest_discover_tests(${t})
endforeach()
target_link_libraries(test_fits PRIVATE OpenSSL::Crypto)
foreach(t test_store test_store_history test_store_contract test_store_crash test_export test_store_review)
  target_link_libraries(${t} PRIVATE SQLite::SQLite3)
endforeach()

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
    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/test_store_review.dir)
