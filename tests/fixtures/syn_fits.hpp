#pragma once

// FIX-SYN-FITS-01: writer FITS minimale e generatori deterministici (per indice, nessun RNG).
// Indipendente da astrocore e da CFITSIO: l'oracle non condivide codice con il prodotto.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace astro_fixture {

using bytes = std::vector<std::uint8_t>;

inline std::string pad_card(std::string s) {
  s.resize(80, ' ');
  return s;
}

inline std::string key_field(const std::string& key) {
  std::string k = key;
  k.resize(8, ' ');
  return k + "= ";
}

inline std::string card_int(const std::string& key, long long v) {
  char b[32];
  std::snprintf(b, sizeof b, "%20lld", v);
  return pad_card(key_field(key) + b);
}

inline std::string card_real(const std::string& key, double v) {
  char b[40];
  std::snprintf(b, sizeof b, "%20.15G", v);
  return pad_card(key_field(key) + b);
}

inline std::string card_bool(const std::string& key, bool v) {
  return pad_card(key_field(key) + "                   " + (v ? "T" : "F"));
}

inline std::string card_str(const std::string& key, const std::string& v) {
  std::string q;
  for (char c : v) {
    q += c;
    if (c == '\'') q += '\'';
  }
  if (q.size() < 8) q.resize(8, ' ');
  return pad_card(key_field(key) + "'" + q + "'");
}

inline std::string card_text(const std::string& key, const std::string& text) {
  std::string k = key;
  k.resize(8, ' ');
  return pad_card(k + text);
}

inline void put_be(bytes& out, std::uint64_t v, int n) {
  for (int i = n - 1; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

inline std::uint32_t bits_of(float f) {
  std::uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

inline std::uint64_t bits_of(double d) {
  std::uint64_t u;
  std::memcpy(&u, &d, 8);
  return u;
}

struct raw_hdu {
  int bitpix = 16;
  std::vector<std::uint64_t> axes;
  std::vector<std::string> cards;  // card extra, gia' da 80 caratteri
  bytes data;                      // data unit non imbottita, big-endian
  bool primary = true;
  std::string xtension = "IMAGE";
  long long pcount = 0;
};

inline void append_header(bytes& out, const raw_hdu& h) {
  std::vector<std::string> cards;
  cards.push_back(h.primary ? card_bool("SIMPLE", true) : card_str("XTENSION", h.xtension));
  cards.push_back(card_int("BITPIX", h.bitpix));
  cards.push_back(card_int("NAXIS", static_cast<long long>(h.axes.size())));
  for (std::size_t i = 0; i < h.axes.size(); ++i) {
    cards.push_back(card_int("NAXIS" + std::to_string(i + 1), static_cast<long long>(h.axes[i])));
  }
  if (!h.primary) {
    cards.push_back(card_int("PCOUNT", h.pcount));
    cards.push_back(card_int("GCOUNT", 1));
  }
  for (const auto& c : h.cards) cards.push_back(c);
  cards.push_back(pad_card("END"));
  for (const auto& c : cards) out.insert(out.end(), c.begin(), c.end());
  while (out.size() % 2880 != 0) out.push_back(' ');
}

inline bytes build_fits(const std::vector<raw_hdu>& hdus) {
  bytes out;
  for (const auto& h : hdus) {
    append_header(out, h);
    out.insert(out.end(), h.data.begin(), h.data.end());
    while (out.size() % 2880 != 0) out.push_back(0);
  }
  return out;
}

struct raw_primary_view {
  int bitpix = 0;
  std::vector<std::uint64_t> axes;
  std::vector<std::string> cards;  // senza END
  bytes data;                      // data unit non imbottita
};

// Parser minimo dell'HDU indicato, per confrontare i byte scritti dal prodotto.
inline raw_primary_view parse_hdu(const bytes& file, std::size_t hdu_index = 0) {
  raw_primary_view v;
  std::size_t off = 0;
  for (std::size_t k = 0;; ++k) {
    raw_primary_view cur;
    bool end = false;
    while (!end) {
      for (std::size_t c = 0; c < 36; ++c) {
        std::string card(reinterpret_cast<const char*>(file.data()) + off + c * 80, 80);
        if (card.compare(0, 8, "END     ") == 0) {
          end = true;
          break;
        }
        cur.cards.push_back(card);
        auto value = [&] { return std::stoll(card.substr(10, 20)); };
        std::string key = card.substr(0, 8);
        if (key == "BITPIX  ") cur.bitpix = static_cast<int>(value());
        else if (key.compare(0, 5, "NAXIS") == 0 && key != "NAXIS   ") cur.axes.push_back(static_cast<std::uint64_t>(value()));
      }
      off += 2880;
    }
    std::uint64_t n = 1;
    for (auto a : cur.axes) n *= a;
    std::size_t len = n * static_cast<std::size_t>(std::abs(cur.bitpix) / 8);
    cur.data.assign(file.begin() + static_cast<long>(off), file.begin() + static_cast<long>(off + len));
    if (k == hdu_index) return cur;
    off += (len + 2879) / 2880 * 2880;
  }
}

// DATASUM: somma a 32 bit con riporto circolare delle parole big-endian, data unit imbottita a zero.
inline std::uint32_t datasum(const bytes& data) {
  std::uint64_t sum = 0;
  for (std::size_t i = 0; i < data.size(); i += 4) {
    std::uint32_t w = 0;
    for (std::size_t j = 0; j < 4; ++j) w = (w << 8) | (i + j < data.size() ? data[i + j] : 0);
    sum += w;
  }
  while (sum >> 32) sum = (sum & 0xFFFFFFFFu) + (sum >> 32);
  return static_cast<std::uint32_t>(sum);
}

// ---------------------------------------------------------------- frame sintetici

enum class frame_kind { light, dark, flat, dark_flat, bias };

struct synth_frame {
  bytes file;
  std::vector<std::uint16_t> adu;  // verita': campioni attesi, riga per riga
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  double exposure_s = 0;
  std::string imagetyp;
};

inline std::uint32_t synth_bias(std::uint32_t x, std::uint32_t y) { return 1000 + (7 * x + 13 * y) % 16; }
inline std::uint32_t synth_dark_rate(std::uint32_t x, std::uint32_t y) { return (3 * x + 5 * y) % 8; }
inline std::uint32_t synth_flat_response(std::uint32_t x, std::uint32_t y) { return 8192 + 64 * ((x + y) % 32); }
inline std::uint32_t synth_sky(std::uint32_t x, std::uint32_t y, std::uint32_t index) {
  return 16 * ((x * y + index) % 64);
}

inline synth_frame make_synth_frame(frame_kind kind, std::uint32_t width, std::uint32_t height,
                                    std::uint32_t index) {
  synth_frame f;
  f.width = width;
  f.height = height;
  switch (kind) {
    case frame_kind::light: f.exposure_s = 60; f.imagetyp = "Light Frame"; break;
    case frame_kind::dark: f.exposure_s = 60; f.imagetyp = "Dark Frame"; break;
    case frame_kind::flat: f.exposure_s = 5; f.imagetyp = "Flat Field"; break;
    case frame_kind::dark_flat: f.exposure_s = 5; f.imagetyp = "Dark Flat"; break;
    case frame_kind::bias: f.exposure_s = 0; f.imagetyp = "Bias Frame"; break;
  }
  const auto exposure = static_cast<std::uint32_t>(f.exposure_s);
  raw_hdu h;
  h.bitpix = 16;
  h.axes = {width, height};
  h.cards = {card_real("BSCALE", 1.0), card_real("BZERO", 32768.0), card_str("IMAGETYP", f.imagetyp),
             card_real("EXPTIME", f.exposure_s), card_real("GAIN", 100.0), card_real("EGAIN", 1.5),
             card_real("OFFSET", 10.0), card_real("CCD-TEMP", -10.0), card_str("FILTER", "L"),
             card_str("INSTRUME", "SYN-CAM"), card_int("FRAMENO", index)};
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      std::uint32_t v = synth_bias(x, y);
      if (kind != frame_kind::bias) v += synth_dark_rate(x, y) * exposure;
      if (kind == frame_kind::light) v += synth_sky(x, y, index);
      if (kind == frame_kind::flat) v += synth_flat_response(x, y) + index % 4;
      f.adu.push_back(static_cast<std::uint16_t>(v));
      put_be(h.data, static_cast<std::uint16_t>(v - 32768u), 2);
    }
  }
  f.file = build_fits({h});
  return f;
}

}  // namespace astro_fixture
