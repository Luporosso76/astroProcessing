#pragma once

// Input sintetici della suite di conformita' (docs/contracts/BASE-confini.md §3.1, punto 2): un file FITS per ruolo
// dichiarato dal registro dei tipi di job, con il digest del file scritto. Frame f32 13x6 del modello FIX-SYN-FITS-01
// (header di calibrazione completi, fase raw, linearita' dichiarata): 6 righe, quindi 6 blocchi con tile_rows = 1.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "astro/hash.hpp"
#include "astro/job_types.hpp"
#include "syn_fits.hpp"

namespace conformance_fixture {

namespace fs = std::filesystem;
namespace fx = astro_fixture;

constexpr std::uint32_t width = 13;
constexpr std::uint32_t height = 6;
constexpr int bitpix = -32;

enum class kind { light, dark, flat, dark_flat };

inline fx::bytes frame(kind k, std::uint32_t index) {
  fx::calib_header h;
  h.egain = 2.0;
  std::vector<double> v(static_cast<std::size_t>(width) * height);
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const double b = fx::synth_bias(x, y), d = fx::synth_dark_rate(x, y), q = fx::synth_flat_response(x, y);
      double& s = v[static_cast<std::size_t>(y) * width + x];
      switch (k) {
        case kind::light: s = b + 60 * d + q * fx::synth_sky(x, y, 0) / 8192.0; break;
        case kind::dark: s = b + 60 * d + index; break;
        case kind::flat: s = b + 5 * d + q; break;
        case kind::dark_flat: s = b + 5 * d; break;
      }
    }
  }
  switch (k) {
    case kind::light: h.imagetyp = "Light Frame"; h.exposure = 60; break;
    case kind::dark: h.imagetyp = "Dark Frame"; h.exposure = 60; break;
    case kind::flat: h.imagetyp = "Flat Field"; h.exposure = 5; break;
    case kind::dark_flat: h.imagetyp = "Dark Flat"; h.exposure = 5; break;
  }
  h.extra = {fx::card_int("FRAMENO", static_cast<long long>(index))};
  return fx::float_frame(width, height, bitpix, v, fx::calib_cards(h));
}

// Frame del ruolo: `file` e `light` sono il light; `frame<k>` il dark k-esimo di un gruppo di master.
inline fx::bytes frame_for_role(const std::string& role) {
  if (role == "file" || role == "light") return frame(kind::light, 0);
  if (role == "dark") return frame(kind::dark, 0);
  if (role == "flat") return frame(kind::flat, 0);
  if (role == "dark_flat") return frame(kind::dark_flat, 0);
  if (role.rfind("frame", 0) == 0 && role.size() == 6 && role[5] >= '0' && role[5] <= '9') {
    return frame(kind::dark, static_cast<std::uint32_t>(role[5] - '0'));
  }
  throw std::runtime_error("ruolo senza fixture: " + role);
}

// Scrive in `dir` un file per ruolo e restituisce percorsi e digest.
inline astro::job_type_inputs write_inputs(const astro::job_type_entry& entry, const fs::path& dir) {
  fs::create_directories(dir);
  astro::job_type_inputs inputs;
  for (const std::string& role : entry.input_roles) {
    const fx::bytes b = frame_for_role(role);
    const fs::path p = fs::absolute(dir / (role + ".fits"));
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    out.close();
    if (!out) throw std::runtime_error("scrittura della fixture non riuscita: " + p.string());
    inputs[role] = astro::job_type_input{p.string(), astro::digest_of(b)};
  }
  return inputs;
}

}  // namespace conformance_fixture
