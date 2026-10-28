#include "astro/json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <numeric>

#include "astro/error.hpp"

namespace astro {

namespace {

constexpr std::int64_t max_safe_integer = std::int64_t{1} << 53;

[[noreturn]] void fail(const char* code, const char* message) {
  throw stop_error(error_category::invalid_input, code, message);
}

// UTF-8 -> UTF-16 con validazione stretta (no overlong, no surrogati, <= U+10FFFF).
std::u16string utf8_to_utf16(const std::string& text) {
  std::u16string out;
  out.reserve(text.size());
  std::size_t i = 0;
  while (i < text.size()) {
    const auto b0 = static_cast<unsigned char>(text[i]);
    std::uint32_t cp = 0;
    std::size_t extra = 0;
    std::uint32_t minimum = 0;
    if (b0 < 0x80) {
      cp = b0;
    } else if (b0 >= 0xC2 && b0 <= 0xDF) {
      cp = b0 & 0x1Fu; extra = 1; minimum = 0x80;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
      cp = b0 & 0x0Fu; extra = 2; minimum = 0x800;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
      cp = b0 & 0x07u; extra = 3; minimum = 0x10000;
    } else {
      fail("dag.param_invalid_utf8", "sequenza UTF-8 non valida");
    }
    if (extra != 0 && i + extra >= text.size()) {
      fail("dag.param_invalid_utf8", "sequenza UTF-8 troncata");
    }
    for (std::size_t k = 1; k <= extra; ++k) {
      const auto bk = static_cast<unsigned char>(text[i + k]);
      if ((bk & 0xC0) != 0x80) fail("dag.param_invalid_utf8", "byte di continuazione non valido");
      cp = (cp << 6) | (bk & 0x3Fu);
    }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
      fail("dag.param_invalid_utf8", "punto di codice non valido");
    }
    i += extra + 1;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
      out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
    } else {
      out.push_back(static_cast<char16_t>(cp));
    }
  }
  return out;
}

void append_string(std::string& out, const std::string& text) {
  static constexpr char hex[] = "0123456789abcdef";
  out.push_back('"');
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\t': out += "\\t"; break;
      case '\n': out += "\\n"; break;
      case '\f': out += "\\f"; break;
      case '\r': out += "\\r"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out.push_back(hex[c >> 4]);
          out.push_back(hex[c & 0x0f]);
        } else {
          out.push_back(ch);
        }
    }
  }
  out.push_back('"');
}

void serialize(std::string& out, const json& value);

void serialize_object(std::string& out, const json& value) {
  const auto& keys = value.keys();
  std::vector<std::u16string> utf16;
  utf16.reserve(keys.size());
  for (const auto& key : keys) utf16.push_back(utf8_to_utf16(key));
  std::vector<std::size_t> order(keys.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::sort(order.begin(), order.end(),
            [&](std::size_t a, std::size_t b) { return utf16[a] < utf16[b]; });
  out.push_back('{');
  bool first = true;
  for (const std::size_t index : order) {
    if (!first) out.push_back(',');
    first = false;
    append_string(out, keys[index]);
    out.push_back(':');
    serialize(out, value.items()[index]);
  }
  out.push_back('}');
}

void serialize(std::string& out, const json& value) {
  switch (value.type()) {
    case json::kind::null: out += "null"; return;
    case json::kind::boolean: out += value.as_bool() ? "true" : "false"; return;
    case json::kind::integer: out += std::to_string(value.as_integer()); return;
    case json::kind::number: out += ecmascript_number(value.as_number()); return;
    case json::kind::string: append_string(out, value.as_string()); return;
    case json::kind::array: {
      out.push_back('[');
      bool first = true;
      for (const auto& item : value.items()) {
        if (!first) out.push_back(',');
        first = false;
        serialize(out, item);
      }
      out.push_back(']');
      return;
    }
    case json::kind::object: serialize_object(out, value); return;
  }
  std::abort();
}

}  // namespace

json json::boolean(bool value) {
  json j;
  j.kind_ = kind::boolean;
  j.bool_ = value;
  return j;
}

json json::integer(std::int64_t value) {
  if (value > max_safe_integer || value < -max_safe_integer) {
    fail("dag.param_integer_range", "intero oltre 2^53: usare una stringa");
  }
  json j;
  j.kind_ = kind::integer;
  j.int_ = value;
  return j;
}

json json::uinteger(std::uint64_t value) {
  if (value > static_cast<std::uint64_t>(max_safe_integer)) {
    fail("dag.param_integer_range", "intero oltre 2^53: usare una stringa");
  }
  return integer(static_cast<std::int64_t>(value));
}

json json::number(double value) {
  if (!std::isfinite(value)) fail("dag.param_nonfinite", "parametro non finito (NaN o Inf)");
  json j;
  j.kind_ = kind::number;
  j.number_ = value;
  return j;
}

json json::string(std::string value) {
  (void)utf8_to_utf16(value);
  json j;
  j.kind_ = kind::string;
  j.string_ = std::move(value);
  return j;
}

json json::array() {
  json j;
  j.kind_ = kind::array;
  return j;
}

json json::object() {
  json j;
  j.kind_ = kind::object;
  return j;
}

json& json::push(json value) {
  ASTRO_ASSERT(kind_ == kind::array);
  items_.push_back(std::move(value));
  return *this;
}

json& json::set(std::string key, json value) {
  ASTRO_ASSERT(kind_ == kind::object);
  (void)utf8_to_utf16(key);
  if (std::find(keys_.begin(), keys_.end(), key) != keys_.end()) {
    throw stop_error(error_category::invalid_input, "dag.param_duplicate_key", "chiave duplicata",
                     {{"chiave", key}});
  }
  keys_.push_back(std::move(key));
  items_.push_back(std::move(value));
  return *this;
}

bool json::as_bool() const {
  ASTRO_ASSERT(kind_ == kind::boolean);
  return bool_;
}

std::int64_t json::as_integer() const {
  ASTRO_ASSERT(kind_ == kind::integer);
  return int_;
}

double json::as_number() const {
  ASTRO_ASSERT(kind_ == kind::number);
  return number_;
}

const std::string& json::as_string() const {
  ASTRO_ASSERT(kind_ == kind::string);
  return string_;
}

const std::vector<json>& json::items() const {
  ASTRO_ASSERT(kind_ == kind::array || kind_ == kind::object);
  return items_;
}

const std::vector<std::string>& json::keys() const {
  ASTRO_ASSERT(kind_ == kind::object);
  return keys_;
}

const json* json::find(const std::string& key) const noexcept {
  if (kind_ != kind::object) return nullptr;
  for (std::size_t i = 0; i < keys_.size(); ++i) {
    if (keys_[i] == key) return &items_[i];
  }
  return nullptr;
}

std::string ecmascript_number(double value) {
  ASTRO_ASSERT(std::isfinite(value));
  if (value == 0) return "0";
  char buf[64];
  const auto res = std::to_chars(buf, buf + sizeof buf, std::fabs(value), std::chars_format::scientific);
  ASTRO_ASSERT(res.ec == std::errc());
  const std::string sci(buf, res.ptr);
  const std::size_t e = sci.find('e');
  std::string digits = sci.substr(0, e);
  digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());
  const int n = std::atoi(sci.c_str() + e + 1) + 1;
  const int k = static_cast<int>(digits.size());
  std::string out = value < 0 ? "-" : "";
  if (k <= n && n <= 21) {
    out += digits + std::string(static_cast<std::size_t>(n - k), '0');
  } else if (0 < n && n <= 21) {
    out += digits.substr(0, static_cast<std::size_t>(n)) + "." + digits.substr(static_cast<std::size_t>(n));
  } else if (-6 < n && n <= 0) {
    out += "0." + std::string(static_cast<std::size_t>(-n), '0') + digits;
  } else {
    const int exponent = n - 1;
    out += digits.substr(0, 1);
    if (k > 1) out += "." + digits.substr(1);
    out += exponent < 0 ? "e-" : "e+";
    out += std::to_string(std::abs(exponent));
  }
  return out;
}

std::string canonical_json(const json& value) {
  std::string out;
  serialize(out, value);
  return out;
}

}  // namespace astro
