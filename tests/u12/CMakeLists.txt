# T-U12-01…12 (docs/contracts/U12-third-party.md): dipendono da third_party/{libraw,cfitsio,wcslib} e dalle librerie costruite nel
# progetto. I test sono script Python (python3 -S) che leggono la directory di build; i due di build completa sono `slow`.
set(U12_PY ${ASTRO_PYTHON3} -S)
set(U12_DIR ${PROJECT_SOURCE_DIR}/tests/u12)

add_test(NAME T-U12-01.layout COMMAND ${U12_PY} ${U12_DIR}/test_u12_01_layout.py)
add_test(NAME T-U12-02.offline_build COMMAND ${U12_PY} ${U12_DIR}/test_u12_02_build.py offline ${CMAKE_CXX_COMPILER})
add_test(NAME T-U12-02.hash_tamper COMMAND ${U12_PY} ${U12_DIR}/test_u12_02_build.py tamper ${CMAKE_CXX_COMPILER})
add_test(NAME T-U12-03.system_headers
  COMMAND ${U12_PY} ${U12_DIR}/test_u12_03_headers.py ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${CMAKE_CXX_COMPILER})
add_test(NAME T-U12-04.static_link COMMAND ${U12_PY} ${U12_DIR}/test_u12_binaries.py 04 ${CMAKE_BINARY_DIR})
add_test(NAME T-U12-05.versions COMMAND ${U12_PY} ${U12_DIR}/test_u12_runtime.py 05 ${CMAKE_BINARY_DIR} ${CMAKE_CXX_COMPILER})
add_test(NAME T-U12-06.capabilities COMMAND ${U12_PY} ${U12_DIR}/test_u12_runtime.py 06 ${CMAKE_BINARY_DIR} ${CMAKE_CXX_COMPILER})
add_test(NAME T-U12-05.order_cfitsio_static COMMAND ${U12_PY} ${U12_DIR}/test_u12_order_static.py)
add_test(NAME T-U12-07.no_network_libs COMMAND ${U12_PY} ${U12_DIR}/test_u12_binaries.py 07 ${CMAKE_BINARY_DIR})
add_test(NAME T-U12-08.no_openmp COMMAND ${U12_PY} ${U12_DIR}/test_u12_binaries.py 08 ${CMAKE_BINARY_DIR})
add_test(NAME T-U12-09.no_rawspeed_data COMMAND ${U12_PY} ${U12_DIR}/test_u12_binaries.py 09 ${CMAKE_BINARY_DIR})
add_test(NAME T-U12-10.licenses_table COMMAND ${U12_PY} ${U12_DIR}/test_u12_docs.py 10)
add_test(NAME T-U12-11.constants COMMAND ${U12_PY} ${U12_DIR}/test_u12_docs.py 11)
# T-U12-12 legge i NEEDED degli eseguibili di release: esiste solo nella build release, che ne costruisce gli artefatti.
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  add_test(NAME T-U12-12.system_licenses COMMAND ${U12_PY} ${U12_DIR}/test_u12_docs.py 12 ${CMAKE_BINARY_DIR})
  set_tests_properties(T-U12-12.system_licenses PROPERTIES LABELS fast TIMEOUT 300)
endif()

# Death test e verified() (obblighi 5 e 6): il target non e' in `all`; lo costruisce il test T-U12-05.build_verify_test, cosi' un errore
# di compilazione e' un test rosso e non blocca la build del resto.
add_executable(test_third_party_verify EXCLUDE_FROM_ALL test_third_party_verify.cpp ${PROJECT_SOURCE_DIR}/tests/unit/main_utc0.cpp)
target_link_libraries(test_third_party_verify PRIVATE astrocore GTest::gtest)
target_include_directories(test_third_party_verify PRIVATE ${PROJECT_SOURCE_DIR}/tests/fixtures ${PROJECT_SOURCE_DIR}/tests/unit ${PROJECT_SOURCE_DIR}/src/core)
add_test(NAME T-U12-05.build_verify_test
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test_third_party_verify)
add_test(NAME T-U12-05.check_versions_death COMMAND test_third_party_verify --gtest_filter=U12Check.Version*)
add_test(NAME T-U12-06.check_capabilities_death COMMAND test_third_party_verify --gtest_filter=U12Check.Capab*)
add_test(NAME T-U12-05.verified_after_use COMMAND test_third_party_verify --gtest_filter=U12Verified.*)
add_test(NAME T-U12-05.check_good_values COMMAND test_third_party_verify --gtest_filter=U12Check.GoodValuesPass)
add_test(NAME T-U12-05.injected_values_stop COMMAND test_third_party_verify --gtest_filter=U12VerifiedInjected.*)
set_tests_properties(T-U12-05.build_verify_test PROPERTIES FIXTURES_SETUP u12_verify_bin LABELS fast TIMEOUT 900
  RESOURCE_LOCK u12_build)
set_tests_properties(T-U12-05.check_versions_death T-U12-06.check_capabilities_death T-U12-05.verified_after_use
  T-U12-05.check_good_values T-U12-05.injected_values_stop
  PROPERTIES FIXTURES_REQUIRED u12_verify_bin LABELS fast TIMEOUT 300)

set_tests_properties(
  T-U12-01.layout T-U12-05.order_cfitsio_static T-U12-03.system_headers T-U12-04.static_link T-U12-05.versions T-U12-06.capabilities
  T-U12-07.no_network_libs T-U12-08.no_openmp T-U12-09.no_rawspeed_data
  T-U12-10.licenses_table T-U12-11.constants
  PROPERTIES LABELS fast TIMEOUT 300)
set_tests_properties(T-U12-02.offline_build T-U12-02.hash_tamper PROPERTIES LABELS slow TIMEOUT 3600 RESOURCE_LOCK u12_build)
