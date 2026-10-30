// Main condiviso dei test che collegano il decoder RAW (ADR-08.b §2.6): TZ=UTC0 prima di ogni thread, poi l'ambiente non
// si tocca piu'.
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

int main(int argc, char** argv) {
  if (setenv("TZ", "UTC0", 1) != 0) {
    std::fputs("setenv(TZ) fallita\n", stderr);
    return 2;
  }
  tzset();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
