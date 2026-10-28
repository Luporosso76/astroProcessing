#include <gtest/gtest.h>

#include "astro/error.hpp"

namespace {

using astro::error_category;
using astro::stop_error;

TEST(Error, StopErrorCarriesCategoryCodeAndContext) {
  stop_error e(error_category::limit_exceeded, "fits.max_naxis", "troppi assi", {{"naxis", "4"}});
  EXPECT_EQ(e.category(), error_category::limit_exceeded);
  EXPECT_EQ(e.code(), "fits.max_naxis");
  EXPECT_EQ(e.message(), "troppi assi");
  ASSERT_EQ(e.context().size(), 1u);
  EXPECT_EQ(e.context()[0].first, "naxis");
  EXPECT_NE(std::string(e.what()).find("fits.max_naxis"), std::string::npos);
  EXPECT_NE(std::string(e.what()).find("limit_exceeded"), std::string::npos);
}

TEST(Error, IntermediateCatchKeepsCategoryAndCodeAddingContext) {
  try {
    try {
      throw stop_error(error_category::io, "input.missing", "assente");
    } catch (stop_error& e) {
      e.add_context("percorso", "x.fits");
      throw;
    }
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::io);
    EXPECT_EQ(e.code(), "input.missing");
    ASSERT_EQ(e.context().size(), 1u);
    EXPECT_EQ(e.context()[0].second, "x.fits");
  }
}

TEST(Error, CategoryNamesAreStable) {
  EXPECT_STREQ(astro::to_string(error_category::invalid_input), "invalid_input");
  EXPECT_STREQ(astro::to_string(error_category::resume_mismatch), "resume_mismatch");
  EXPECT_STREQ(astro::to_string(error_category::security), "security");
}

TEST(ErrorDeathTest, AssertAbortsOnViolatedInvariant) {
  EXPECT_DEATH(ASTRO_ASSERT(1 == 2), "ASTRO_ASSERT");
  ASTRO_ASSERT(1 == 1);
}

}  // namespace
