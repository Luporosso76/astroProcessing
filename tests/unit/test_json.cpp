#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <limits>

#include "astro/json.hpp"
#include "expect_stop.hpp"

namespace {

using namespace astro;

std::string number_of(std::uint64_t bits) { return ecmascript_number(std::bit_cast<double>(bits)); }

TEST(Json, T_M2B_02_rfc8785_section_3_2_2_example) {
  json numbers = json::array();
  numbers.push(json::number(333333333.33333329));
  numbers.push(json::number(1E30));
  numbers.push(json::number(4.50));
  numbers.push(json::number(2e-3));
  numbers.push(json::number(0.000000000000000000000000001));
  json literals = json::array();
  literals.push(json::null());
  literals.push(json::boolean(true));
  literals.push(json::boolean(false));
  json doc = json::object();
  doc.set("numbers", numbers);
  doc.set("string", json::string(std::string("\xE2\x82\xAC$\x0F\nA'B\"\\\\\"/")));
  doc.set("literals", literals);
  const std::string expected =
      "{\"literals\":[null,true,false],\"numbers\":[333333333.3333333,1e+30,4.5,0.002,1e-27],"
      "\"string\":\"\xE2\x82\xAC$\\u000f\\nA'B\\\"\\\\\\\\\\\"/\"}";
  EXPECT_EQ(canonical_json(doc), expected);
}

TEST(Json, T_M2B_02_rfc8785_section_3_2_3_utf16_key_order) {
  json doc = json::object();
  doc.set("\xE2\x82\xAC", json::string("Euro Sign"));
  doc.set("\r", json::string("Carriage Return"));
  doc.set("\xEF\xAC\xB3", json::string("Hebrew Letter Dalet With Dagesh"));
  doc.set("1", json::string("One"));
  doc.set("\xF0\x9F\x98\x80", json::string("Emoji: Grinning Face"));
  doc.set("\xC2\x80", json::string("Control"));
  doc.set("\xC3\xB6", json::string("Latin Small Letter O With Diaeresis"));
  const std::string expected =
      "{\"\\r\":\"Carriage Return\",\"1\":\"One\",\"\xC2\x80\":\"Control\","
      "\"\xC3\xB6\":\"Latin Small Letter O With Diaeresis\",\"\xE2\x82\xAC\":\"Euro Sign\","
      "\"\xF0\x9F\x98\x80\":\"Emoji: Grinning Face\",\"\xEF\xAC\xB3\":\"Hebrew Letter Dalet With Dagesh\"}";
  EXPECT_EQ(canonical_json(doc), expected);
}

TEST(Json, T_M2B_02_rfc8785_appendix_b_number_serialization) {
  EXPECT_EQ(number_of(0x0000000000000000ull), "0");
  EXPECT_EQ(number_of(0x8000000000000000ull), "0");
  EXPECT_EQ(number_of(0x0000000000000001ull), "5e-324");
  EXPECT_EQ(number_of(0x8000000000000001ull), "-5e-324");
  EXPECT_EQ(number_of(0x7fefffffffffffffull), "1.7976931348623157e+308");
  EXPECT_EQ(number_of(0xffefffffffffffffull), "-1.7976931348623157e+308");
  EXPECT_EQ(number_of(0x4340000000000000ull), "9007199254740992");
  EXPECT_EQ(number_of(0xc340000000000000ull), "-9007199254740992");
  EXPECT_EQ(number_of(0x4430000000000000ull), "295147905179352830000");
  EXPECT_EQ(number_of(0x44b52d02c7e14af5ull), "9.999999999999997e+22");
  EXPECT_EQ(number_of(0x44b52d02c7e14af6ull), "1e+23");
  EXPECT_EQ(number_of(0x44b52d02c7e14af7ull), "1.0000000000000001e+23");
  EXPECT_EQ(number_of(0x444b1ae4d6e2ef4eull), "999999999999999700000");
  EXPECT_EQ(number_of(0x444b1ae4d6e2ef4full), "999999999999999900000");
  EXPECT_EQ(number_of(0x444b1ae4d6e2ef50ull), "1e+21");
  EXPECT_EQ(number_of(0x3eb0c6f7a0b5ed8cull), "9.999999999999997e-7");
  EXPECT_EQ(number_of(0x3eb0c6f7a0b5ed8dull), "0.000001");
  EXPECT_EQ(number_of(0x41b3de4355555553ull), "333333333.3333332");
  EXPECT_EQ(number_of(0x41b3de4355555554ull), "333333333.33333325");
  EXPECT_EQ(number_of(0x41b3de4355555555ull), "333333333.3333333");
  EXPECT_EQ(number_of(0x41b3de4355555556ull), "333333333.3333334");
  EXPECT_EQ(number_of(0x41b3de4355555557ull), "333333333.33333343");
  EXPECT_EQ(number_of(0xbecbf647612f3696ull), "-0.0000033333333333333333");
  EXPECT_EQ(number_of(0x43143ff3c1cb0959ull), "1424953923781206.2");
}

TEST(Json, T_M2B_02_escapes_and_literals) {
  json doc = json::array();
  doc.push(json::string(std::string("\b\t\n\f\r\"\\ \x01\x1f\x7f/", 12)));
  doc.push(json::integer(-7));
  doc.push(json::integer(9007199254740992));
  doc.push(json::string(""));
  doc.push(json::object());
  doc.push(json::array());
  EXPECT_EQ(canonical_json(doc),
            "[\"\\b\\t\\n\\f\\r\\\"\\\\ \\u0001\\u001f\x7f/\",-7,9007199254740992,\"\",{},[]]");
}

TEST(Json, T_M2B_02_nan_and_inf_are_rejected) {
  EXPECT_STOP(json::number(std::numeric_limits<double>::quiet_NaN()), error_category::invalid_input,
              "dag.param_nonfinite");
  EXPECT_STOP(json::number(std::numeric_limits<double>::infinity()), error_category::invalid_input,
              "dag.param_nonfinite");
  EXPECT_STOP(json::number(-std::numeric_limits<double>::infinity()), error_category::invalid_input,
              "dag.param_nonfinite");
}

TEST(Json, T_M2B_02_integers_beyond_2_pow_53_are_rejected_not_rounded) {
  EXPECT_STOP(json::integer(9007199254740993), error_category::invalid_input, "dag.param_integer_range");
  EXPECT_STOP(json::integer(-9007199254740993), error_category::invalid_input, "dag.param_integer_range");
  EXPECT_STOP(json::uinteger(std::numeric_limits<std::uint64_t>::max()), error_category::invalid_input,
              "dag.param_integer_range");
  EXPECT_NO_THROW(json::uinteger(9007199254740992ull));
}

TEST(Json, T_M2B_02_invalid_utf8_and_duplicate_keys_are_rejected) {
  EXPECT_STOP(json::string("\xC0\x80"), error_category::invalid_input, "dag.param_invalid_utf8");
  EXPECT_STOP(json::string("\xED\xA0\x80"), error_category::invalid_input, "dag.param_invalid_utf8");
  EXPECT_STOP(json::string("\xF4\x90\x80\x80"), error_category::invalid_input, "dag.param_invalid_utf8");
  EXPECT_STOP(json::string("\xE2\x82"), error_category::invalid_input, "dag.param_invalid_utf8");
  json o = json::object();
  o.set("a", json::null());
  EXPECT_STOP(o.set("a", json::null()), error_category::invalid_input, "dag.param_duplicate_key");
  json k = json::object();
  EXPECT_STOP(k.set("\xFF", json::null()), error_category::invalid_input, "dag.param_invalid_utf8");
}

}  // namespace
