// Caso spia di U5 (M3.a): finché `ASTRO_U5_TESTS` è spenta, i test di G-OFFSET (test_calibration_offset.cpp) non vengono
// compilati perché usano i simboli di `astro/calibration_analysis.hpp`, che U5 deve ancora scrivere. Il codice di U5 porta
// l'opzione a ON e toglie questo file insieme al ramo `else()` del CMake.
#include <gtest/gtest.h>

TEST(U5Spy, T_M3A_C01_01_03_target_not_built) {
  FAIL() << "T-M3A-C01-01 e T-M3A-C01-03 non compilati: ASTRO_U5_TESTS è spenta perché manca "
            "src/core/calibration_analysis.cpp (U5)";
}
