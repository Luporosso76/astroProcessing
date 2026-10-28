#pragma once

#include <gtest/gtest.h>

#include <string>

#include "astro/error.hpp"

#define EXPECT_STOP(expr, cat, expected_code)                                                   \
  do {                                                                                          \
    try {                                                                                       \
      (void)(expr);                                                                             \
      ADD_FAILURE() << "stop_error attesto, nessuna eccezione: " #expr;                         \
    } catch (const ::astro::stop_error& e) {                                                    \
      EXPECT_EQ(e.category(), (cat)) << e.what();                                               \
      EXPECT_EQ(e.code(), std::string(expected_code)) << e.what();                              \
    }                                                                                           \
  } while (0)
