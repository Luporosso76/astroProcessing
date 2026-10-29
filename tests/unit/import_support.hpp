#pragma once

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

#include "astro/fits.hpp"
#include "astro/hash.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace imp {

using namespace astro;
namespace fx = astro_fixture;
namespace fs = std::filesystem;

constexpr std::uint64_t MiB = std::uint64_t{1} << 20;

inline fs::path fresh(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "import" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

inline void write_file(const fs::path& path, const fx::bytes& b) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

inline fx::bytes read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return fx::bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline std::string text_of(const fs::path& path) {
  const fx::bytes b = read_file(path);
  return std::string(b.begin(), b.end());
}

// Valore fisico di un campione: (piano, x, y) -> valore. Deve essere rappresentabile esattamente nel BITPIX scelto.
using value_fn = std::function<double(std::uint32_t, std::uint32_t, std::uint32_t)>;

struct gen {
  int bitpix = 16;
  std::uint32_t w = 0, h = 0, planes = 1;
  double bscale = 1.0, bzero = 0.0;
  std::vector<std::string> cards;
  value_fn value;
};

inline void put_sample(fx::bytes& out, const gen& g, double v) {
  switch (g.bitpix) {
    case 8: fx::put_be(out, static_cast<std::uint64_t>(std::llround((v - g.bzero) / g.bscale)), 1); break;
    case 16: fx::put_be(out, static_cast<std::uint64_t>(std::llround((v - g.bzero) / g.bscale)), 2); break;
    case 32: fx::put_be(out, static_cast<std::uint64_t>(std::llround((v - g.bzero) / g.bscale)), 4); break;
    case 64: fx::put_be(out, static_cast<std::uint64_t>(std::llround((v - g.bzero) / g.bscale)), 8); break;
    case -32: fx::put_be(out, fx::bits_of(static_cast<float>(v)), 4); break;
    default: fx::put_be(out, fx::bits_of(v), 8); break;
  }
}

inline fx::raw_hdu primary_of(const gen& g) {
  fx::raw_hdu h;
  h.bitpix = g.bitpix;
  h.axes = {g.w, g.h};
  if (g.planes > 1) h.axes.push_back(g.planes);
  h.cards = g.cards;
  if (g.bscale != 1.0) h.cards.push_back(fx::card_real("BSCALE", g.bscale));
  if (g.bzero != 0.0) h.cards.push_back(fx::card_real("BZERO", g.bzero));
  for (std::uint32_t p = 0; p < g.planes; ++p) {
    for (std::uint32_t y = 0; y < g.h; ++y) {
      for (std::uint32_t x = 0; x < g.w; ++x) put_sample(h.data, g, g.value ? g.value(p, x, y) : 0.0);
    }
  }
  return h;
}

inline fx::raw_hdu invalid_ext_of(const gen& g) {
  fx::raw_hdu e;
  e.primary = false;
  e.bitpix = 16;
  e.axes = {g.w, g.h};
  if (g.planes > 1) e.axes.push_back(g.planes);
  e.cards = {fx::card_real("BZERO", 32768.0), fx::card_str("EXTNAME", "INVALID")};
  for (std::uint64_t i = 0; i < std::uint64_t{g.w} * g.h * g.planes; ++i) fx::put_be(e.data, 0x8000u, 2);
  return e;
}

inline fx::bytes make_fits(const gen& g, bool with_invalid = false) {
  std::vector<fx::raw_hdu> hdus{primary_of(g)};
  if (with_invalid) hdus.push_back(invalid_ext_of(g));
  return fx::build_fits(hdus);
}

inline std::string phase_card(const char* phase) { return fx::card_str("ASTRPHAS", phase); }

}  // namespace imp
