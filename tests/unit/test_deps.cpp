#include <fftw3.h>
#include <fitsio.h>
#include <gtest/gtest.h>
#include <openssl/evp.h>
#include <sqlite3.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace {

TEST(Deps, OpenSslSha256Nist) {
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  const char msg[] = "abc";
  ASSERT_EQ(EVP_Digest(msg, 3, md, &len, EVP_sha256(), nullptr), 1);
  ASSERT_EQ(len, 32u);
  std::string hex;
  char b[3];
  for (unsigned i = 0; i < len; ++i) {
    std::snprintf(b, sizeof b, "%02x", md[i]);
    hex += b;
  }
  EXPECT_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Deps, SqliteInMemoryRoundtrip) {
  sqlite3* db = nullptr;
  ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
  ASSERT_EQ(sqlite3_exec(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT);"
                             "INSERT INTO t(v) VALUES('m2');",
                         nullptr, nullptr, nullptr),
            SQLITE_OK);
  sqlite3_stmt* st = nullptr;
  ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT id, v FROM t", -1, &st, nullptr), SQLITE_OK);
  ASSERT_EQ(sqlite3_step(st), SQLITE_ROW);
  EXPECT_EQ(sqlite3_column_int(st, 0), 1);
  EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(st, 1)), "m2");
  EXPECT_EQ(sqlite3_step(st), SQLITE_DONE);
  sqlite3_finalize(st);
  sqlite3_close(db);
}

TEST(Deps, CfitsioVersion) {
  float v = 0;
  fits_get_version(&v);
  EXPECT_NEAR(v, 4.0603f, 0.0001f);
}

TEST(Deps, FftwDoubleDeltaIsAllOnes) {
  constexpr int n = 8;
  fftw_complex* in = fftw_alloc_complex(n);
  fftw_complex* out = fftw_alloc_complex(n);
  ASSERT_NE(in, nullptr);
  ASSERT_NE(out, nullptr);
  fftw_plan p = fftw_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  ASSERT_NE(p, nullptr);
  for (int i = 0; i < n; ++i) in[i][0] = in[i][1] = 0.0;
  in[0][0] = 1.0;
  fftw_execute(p);
  for (int i = 0; i < n; ++i) {
    EXPECT_DOUBLE_EQ(out[i][0], 1.0);
    EXPECT_DOUBLE_EQ(out[i][1], 0.0);
  }
  fftw_destroy_plan(p);
  fftw_free(in);
  fftw_free(out);
}

TEST(Deps, FftwFloatDeltaIsAllOnes) {
  constexpr int n = 8;
  fftwf_complex* in = fftwf_alloc_complex(n);
  fftwf_complex* out = fftwf_alloc_complex(n);
  ASSERT_NE(in, nullptr);
  ASSERT_NE(out, nullptr);
  fftwf_plan p = fftwf_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  ASSERT_NE(p, nullptr);
  for (int i = 0; i < n; ++i) in[i][0] = in[i][1] = 0.0f;
  in[0][0] = 1.0f;
  fftwf_execute(p);
  for (int i = 0; i < n; ++i) {
    EXPECT_FLOAT_EQ(out[i][0], 1.0f);
    EXPECT_FLOAT_EQ(out[i][1], 0.0f);
  }
  fftwf_destroy_plan(p);
  fftwf_free(in);
  fftwf_free(out);
}

}  // namespace
