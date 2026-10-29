#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fs = std::filesystem;
namespace fx = astro_fixture;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 40;

std::vector<std::uint8_t> pattern(std::size_t n) {
  std::vector<std::uint8_t> v(n);
  for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::uint8_t>((i * 31 + 7) & 0xff);
  return v;
}

std::vector<std::uint8_t> bytes_of(const std::string& s) { return {s.begin(), s.end()}; }

fs::path tmp_file(const std::string& name) {
  fs::path dir = fs::path(ASTRO_TEST_TMP) / "hash";
  fs::create_directories(dir);
  return dir / name;
}

void write_file(const fs::path& path, const std::vector<std::uint8_t>& b) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  ASSERT_TRUE(out.good());
}

TEST(Hash, T_M2B_01_nist_vectors) {
  EXPECT_EQ(sha256_hex(bytes_of("")), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(sha256_hex(bytes_of("abc")), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(sha256_hex(bytes_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  EXPECT_EQ(sha256_hex(std::vector<std::uint8_t>(1000000, 'a')),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Hash, T_M2B_01_chunk_boundaries_match_independent_digests) {
  struct row { std::size_t n; const char* sha; };
  const row rows[] = {
      {1048575, "4ddf525d0f1482ae9ed8ff016e61988dfab2707743585ac310be210689ddb8d6"},
      {1048576, "06b7bbfb7824aa03382051691630eb26de85102d1b08a81e907ec0744cd8a286"},
      {1048577, "0d09f3eabb5c78e5d435c41e3b3755f1c4d038657f739d0be0083c24e71235df"},
      {2097152, "15d80ba308304e391a817479e20a3922787105dfdff12e2251e83d80f8c32195"},
      {2621440, "a1a81b0201e686d9d62a38c2121217a3d25fe66bf3747a8df8b6de1b1c1ac91b"},
      {3158073, "5484bbdbfa09aaf96421d09d2c57be9d4fdbffe2ef1d33964a754591a0ac11ba"},
  };
  for (const auto& r : rows) {
    const auto data = pattern(r.n);
    EXPECT_EQ(sha256_hex(data), r.sha) << r.n;
    EXPECT_EQ(digest_of(data).size_bytes, r.n);
  }
}

TEST(Hash, T_M2B_01_is_sha256_hex_is_strict) {
  EXPECT_TRUE(is_sha256_hex(std::string(64, 'a')));
  EXPECT_FALSE(is_sha256_hex(std::string(64, 'A')));
  EXPECT_FALSE(is_sha256_hex(std::string(63, 'a')));
  EXPECT_FALSE(is_sha256_hex(std::string(64, 'g')));
}

TEST(Hash, read_verified_fits_returns_the_verified_buffer) {
  const auto file = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  const fs::path path = tmp_file("ok.fits");
  write_file(path, file);
  const auto bytes = read_verified_fits(path.string(), digest_of(file), big_budget);
  EXPECT_EQ(bytes, file);
  EXPECT_NO_THROW(decode_fits(bytes, big_budget));
}

TEST(Hash, T_M2B_07_same_size_and_mtime_different_content_is_detected_before_use) {
  auto file = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  const content_digest recorded = digest_of(file);
  const fs::path path = tmp_file("same_size.fits");
  write_file(path, file);
  const auto mtime = fs::last_write_time(path);
  file.back() ^= 0x01;
  write_file(path, file);
  fs::last_write_time(path, mtime);
  ASSERT_EQ(fs::file_size(path), recorded.size_bytes);
  ASSERT_EQ(fs::last_write_time(path), mtime);
  EXPECT_STOP(read_verified_fits(path.string(), recorded, big_budget), error_category::integrity,
              "input.digest_mismatch");
}

TEST(Hash, T_M2B_08_file_modified_after_import_stops_with_integrity) {
  auto file = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  const content_digest recorded = digest_of(file);
  const fs::path path = tmp_file("grown.fits");
  file.insert(file.end(), 2880, 0);
  write_file(path, file);
  EXPECT_STOP(read_verified_fits(path.string(), recorded, big_budget), error_category::integrity,
              "input.digest_mismatch");
}

TEST(Hash, T_M2B_08_same_size_altered_header_is_integrity_before_any_parse) {
  const auto original = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  const content_digest recorded = digest_of(original);
  const std::string header(original.begin(), original.begin() + 2880);
  const std::size_t naxis1 = header.find("NAXIS1  =");
  ASSERT_NE(naxis1, std::string::npos);
  struct variant { const char* name; std::size_t offset; std::uint8_t value; };
  const variant variants[] = {{"SIMPLE->XIMPLE", 0, 'X'}, {"NAXIS1", naxis1 + 29, '9'}, {"huge NAXIS1", naxis1 + 25, '9'}};
  for (const auto& v : variants) {
    auto altered = original;
    altered[v.offset] = v.value;
    ASSERT_NE(altered, original) << v.name;
    ASSERT_NE(digest_of(altered), recorded) << v.name;
    if (v.offset == 0) {
      ASSERT_THROW(plan_fits(altered), stop_error);  // un parse prima del digest darebbe fits.malformed
    }
    const fs::path path = tmp_file("altered.fits");
    write_file(path, altered);
    EXPECT_STOP(read_verified_fits(path.string(), recorded, big_budget), error_category::integrity,
                "input.digest_mismatch");
    EXPECT_STOP(read_verified_fits(path.string(), recorded, 1000), error_category::budget,
                "sched.job_exceeds_budget");
  }
}

TEST(Hash, read_verified_fits_budget_covers_the_file_and_the_planned_peak) {
  const auto file = fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file;
  const fs::path path = tmp_file("budget.fits");
  write_file(path, file);
  EXPECT_STOP(read_verified_fits(path.string(), digest_of(file), file.size() - 1), error_category::budget,
              "sched.job_exceeds_budget");
  EXPECT_STOP(read_verified_fits(path.string(), digest_of(file), file.size()), error_category::budget,
              "sched.job_exceeds_budget");
}

TEST(Hash, T_M2B_09_missing_file_is_io_input_missing) {
  EXPECT_STOP(read_verified_fits(tmp_file("assente.fits").string(), content_digest{std::string(64, '0'), 1}, big_budget),
              error_category::io, "input.missing");
}

TEST(Hash, stream_matches_one_shot_for_any_chunking) {
  std::vector<std::uint8_t> data(3 * 1024 * 1024 + 17);
  for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i * 31 + (i >> 8));
  for (const std::size_t chunk : {std::size_t{1}, std::size_t{4095}, std::size_t{1} << 20, data.size()}) {
    sha256_stream s;
    for (std::size_t off = 0; off < data.size(); off += chunk) {
      s.update({data.data() + off, std::min(chunk, data.size() - off)});
    }
    EXPECT_EQ(s.finish(), digest_of(data));
  }
  sha256_stream empty;
  EXPECT_EQ(empty.finish(), digest_of({}));
}

}  // namespace
