#include <gtest/gtest.h>

#include <regex>

#include "astro/build_info.hpp"

namespace {

TEST(BuildInfo, VersionIsSemantic) {
  EXPECT_TRUE(std::regex_match(astro::build_info().version, std::regex(R"(\d+\.\d+\.\d+)")))
      << astro::build_info().version;
}

TEST(BuildInfo, CompilerIsGcc15) {
  EXPECT_EQ(astro::build_info().compiler.rfind("GNU 15.", 0), 0u)
      << astro::build_info().compiler;
}

TEST(BuildInfo, StandardIsCxx20) {
  EXPECT_EQ(__cplusplus, 202002L);
  EXPECT_EQ(astro::build_info().cplusplus, 202002L);
}

TEST(BuildInfo, FastMathIsOff) {
#ifdef __FAST_MATH__
  FAIL() << "__FAST_MATH__ defined";
#endif
  const std::string flags = astro::build_info().flags;
  EXPECT_NE(flags.find("-fno-fast-math"), std::string::npos);
  EXPECT_NE(flags.find("-ffp-contract=off"), std::string::npos);
  EXPECT_EQ(flags.find("-ffast-math"), std::string::npos);
}

TEST(BuildInfo, WarningsAreErrors) {
  const std::string flags = astro::build_info().flags;
  for (const char* f : {"-Wall", "-Wextra", "-Wpedantic", "-Werror", "-std=c++20"}) {
    EXPECT_NE(flags.find(f), std::string::npos) << f;
  }
}

TEST(BuildInfo, T_M2B_SourceRevisionIsNeverEmpty) {
  const std::string r = astro::build_info().revision;
  EXPECT_FALSE(r.empty());
#ifndef ASTRO_EXPLICIT_REVISION
  EXPECT_TRUE(std::regex_match(r, std::regex("[0-9a-f]{40}"))) << r;
#endif
}

TEST(BuildInfo, BuildTypeIsReported) {
  const std::string t = astro::build_info().build_type;
  EXPECT_TRUE(t == "Debug" || t == "RelWithDebInfo") << t;
}

#ifdef ASTRO_SANITIZED
TEST(BuildInfo, SanitizersAreActiveInDevBuild) {
#ifndef __SANITIZE_ADDRESS__
  FAIL() << "ASTRO_SANITIZED without AddressSanitizer";
#endif
  const std::string flags = astro::build_info().flags;
  EXPECT_NE(flags.find("-fsanitize=address,undefined"), std::string::npos) << flags;
  EXPECT_NE(flags.find("-fno-sanitize-recover=all"), std::string::npos) << flags;
}
#endif

}  // namespace
