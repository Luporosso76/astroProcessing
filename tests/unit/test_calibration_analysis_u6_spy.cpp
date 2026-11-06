// Spia dell'opzione ASTRO_U6_TESTS (ON di default). T-M3A-CAL-08, T-M3A-TYP-18, T-M3A-C01-06 e la parte di analisi di
// T-M3A-C01-07 stanno in test_calibration_analysis_u6.cpp, che entra nella suite solo con l'opzione accesa. Con l'opzione
// spenta, o senza astro/calibration_analysis.hpp, questo caso è rosso: il gate non chiude senza quei casi.
#include <gtest/gtest.h>

// Nome senza ID del registro: accesa l'opzione la spia è verde e non deve contare come prova di un ID.
TEST(CalibrationAnalysisU6Spy, U6_tests_target_is_built) {
#if defined(__has_include)
#if __has_include("astro/calibration_analysis.hpp")
#if defined(ASTRO_U6_TESTS_ENABLED) && ASTRO_U6_TESTS_ENABLED
  SUCCEED();
#else
  FAIL() << "opzione ASTRO_U6_TESTS spenta: test_calibration_analysis_u6 (CAL-08, TYP-18, C01-06, C01-07) fuori dalla suite";
#endif
#else
  FAIL() << "astro/calibration_analysis.hpp assente: test_calibration_analysis_u6.cpp (CAL-08, TYP-18, C01-06, C01-07) "
            "non compila";
#endif
#else
  FAIL() << "__has_include non disponibile";
#endif
}
