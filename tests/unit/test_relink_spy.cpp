// Caso spia di U9 (F-IO-08): finché il prodotto non dichiara le funzioni del contratto, i test di T-M3A-07, 08 e 09
// (dietro ASTRO_U9_RELINK_TESTS, spenta di default) non compilano; questo caso resta rosso nella build di default.
#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

namespace {

std::string header_text() {
  std::ifstream in(std::string(ASTRO_SOURCE_DIR) + "/src/core/include/astro/sqlite_dag_store.hpp", std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

TEST(RelinkSpy, T_M3A_07_08_09_product_api_declared_and_tests_enabled) {
  const std::string text = header_text();
  ASSERT_FALSE(text.empty());
  for (const char* symbol : {"relink_input(", "input_status(", "blocked_slots(", "input_status_canonical("}) {
    EXPECT_NE(text.find(symbol), std::string::npos)
        << "sqlite_dag_store.hpp non dichiara «" << symbol << "…» (F-IO-08, RL-02, RL-03, RL-05): i test di U9 non si compilano";
  }
#ifndef ASTRO_U9_RELINK_TESTS_ENABLED
  ADD_FAILURE() << "test_relink, test_relink_status, test_relink_blocked e test_relink_nostat non sono nella build: "
                   "configurare con -DASTRO_U9_RELINK_TESTS=ON; chi implementa U9 porta l'opzione a ON di default";
#endif
}

}  // namespace
