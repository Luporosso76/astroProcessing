// Caso spia di U8a: finché `test_batch_classification` (T-M3A-05a, 05b, 05c, 06b) non è compilato, la build di default è
// rossa. Il target dipende dai simboli di U8a (astro/batch_classification.hpp, append_classifications) e sta dietro
// l'opzione ASTRO_U8A_TESTS, spenta di default. Il codice di U8a accende l'opzione e rimuove questo file con la
// definizione ASTRO_U8A_TESTS_ENABLED.
#include <gtest/gtest.h>

TEST(U8aSpy, batch_classification_tests_are_not_compiled_until_u8a_exists) {
#if ASTRO_U8A_TESTS_ENABLED
  SUCCEED();
#else
  FAIL() << "test_batch_classification non compilato: mancano i simboli di U8a (batch_classification.hpp, "
            "sqlite_dag_store::append_classifications). Accendere -DASTRO_U8A_TESTS=ON dopo l'implementazione.";
#endif
}
