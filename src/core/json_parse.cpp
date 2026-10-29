#include <charconv>
#include <cmath>

#include "astro/error.hpp"
#include "astro/json.hpp"

namespace astro {

namespace {

constexpr int max_depth = 64;
constexpr std::size_t max_object_keys = 4096;
constexpr std::int64_t max_safe_integer = std::int64_t{1} << 53;

class parser {
 public:
  explicit parser(std::string_view text) : text_(text) {}

  json run() {
    json value = parse_value(0);
    skip_space();
    if (pos_ != text_.size()) fail("dati dopo il valore");
    return value;
  }

 private:
  [[noreturn]] void fail(const char* message) const {
    throw stop_error(error_category::invalid_input, "json.malformed", message, {{"offset", std::to_string(pos_)}});
  }

  void skip_space() {
    while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' || text_[pos_] == '\r')) {
      ++pos_;
    }
  }

  bool consume(char c) {
    if (pos_ < text_.size() && text_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  void expect_literal(std::string_view literal) {
    if (text_.substr(pos_, literal.size()) != literal) fail("letterale non valido");
    pos_ += literal.size();
  }

  json parse_value(int depth) {
    if (depth > max_depth) {
      throw stop_error(error_category::limit_exceeded, "json.depth", "annidamento oltre il limite");
    }
    skip_space();
    if (pos_ >= text_.size()) fail("valore atteso");
    const char c = text_[pos_];
    if (c == '{') return parse_object(depth);
    if (c == '[') return parse_array(depth);
    if (c == '"') return json::string(parse_string());
    if (c == 't') { expect_literal("true"); return json::boolean(true); }
    if (c == 'f') { expect_literal("false"); return json::boolean(false); }
    if (c == 'n') { expect_literal("null"); return json::null(); }
    return parse_number();
  }

  json parse_object(int depth) {
    ++pos_;
    json out = json::object();
    skip_space();
    if (consume('}')) return out;
    for (std::size_t count = 0;; ++count) {
      if (count >= max_object_keys) {
        throw stop_error(error_category::limit_exceeded, "json.object_keys", "troppe chiavi in un oggetto");
      }
      skip_space();
      if (pos_ >= text_.size() || text_[pos_] != '"') fail("chiave attesa");
      std::string key = parse_string();
      skip_space();
      if (!consume(':')) fail("':' atteso");
      out.set(std::move(key), parse_value(depth + 1));
      skip_space();
      if (consume('}')) return out;
      if (!consume(',')) fail("',' o '}' attesi");
    }
  }

  json parse_array(int depth) {
    ++pos_;
    json out = json::array();
    skip_space();
    if (consume(']')) return out;
    for (;;) {
      out.push(parse_value(depth + 1));
      skip_space();
      if (consume(']')) return out;
      if (!consume(',')) fail("',' o ']' attesi");
    }
  }

  std::uint32_t parse_hex4() {
    if (pos_ + 4 > text_.size()) fail("escape \\u troncato");
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      const char h = text_[pos_++];
      v <<= 4;
      if (h >= '0' && h <= '9') v |= static_cast<std::uint32_t>(h - '0');
      else if (h >= 'a' && h <= 'f') v |= static_cast<std::uint32_t>(h - 'a' + 10);
      else if (h >= 'A' && h <= 'F') v |= static_cast<std::uint32_t>(h - 'A' + 10);
      else fail("cifra esadecimale non valida");
    }
    return v;
  }

  static void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  void parse_escape(std::string& out) {
    if (pos_ >= text_.size()) fail("escape troncato");
    const char e = text_[pos_++];
    switch (e) {
      case '"': out.push_back('"'); return;
      case '\\': out.push_back('\\'); return;
      case '/': out.push_back('/'); return;
      case 'b': out.push_back('\b'); return;
      case 'f': out.push_back('\f'); return;
      case 'n': out.push_back('\n'); return;
      case 'r': out.push_back('\r'); return;
      case 't': out.push_back('\t'); return;
      case 'u': break;
      default: fail("escape non valido");
    }
    std::uint32_t cp = parse_hex4();
    if (cp >= 0xDC00 && cp <= 0xDFFF) fail("surrogato basso isolato");
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (!consume('\\') || !consume('u')) fail("surrogato alto isolato");
      const std::uint32_t low = parse_hex4();
      if (low < 0xDC00 || low > 0xDFFF) fail("coppia di surrogati non valida");
      cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
    }
    append_utf8(out, cp);
  }

  std::string parse_string() {
    ++pos_;
    std::string out;
    for (;;) {
      if (pos_ >= text_.size()) fail("stringa non terminata");
      const auto c = static_cast<unsigned char>(text_[pos_++]);
      if (c == '"') return out;
      if (c < 0x20) fail("carattere di controllo nella stringa");
      if (c == '\\') parse_escape(out);
      else out.push_back(static_cast<char>(c));
    }
  }

  void skip_digits() {
    while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
  }

  json parse_number() {
    const std::size_t start = pos_;
    consume('-');
    if (consume('0')) {
      // una cifra iniziale zero non puo' essere seguita da altre cifre
    } else if (pos_ < text_.size() && text_[pos_] >= '1' && text_[pos_] <= '9') {
      skip_digits();
    } else {
      fail("numero non valido");
    }
    bool integral = true;
    if (consume('.')) {
      integral = false;
      const std::size_t before = pos_;
      skip_digits();
      if (pos_ == before) fail("cifre frazionarie attese");
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      integral = false;
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
      const std::size_t before = pos_;
      skip_digits();
      if (pos_ == before) fail("cifre dell'esponente attese");
    }
    const std::string token(text_.substr(start, pos_ - start));
    if (integral) {
      std::int64_t v = 0;
      const auto r = std::from_chars(token.data(), token.data() + token.size(), v);
      if (r.ec == std::errc() && v >= -max_safe_integer && v <= max_safe_integer) return json::integer(v);
    }
    double d = 0;
    const auto r = std::from_chars(token.data(), token.data() + token.size(), d);
    if (r.ec != std::errc() || r.ptr != token.data() + token.size() || !std::isfinite(d)) fail("numero non valido o non finito");
    return json::number(d);
  }

  std::string_view text_;
  std::size_t pos_ = 0;
};

}  // namespace

json parse_json(std::string_view text) { return parser(text).run(); }

}  // namespace astro
