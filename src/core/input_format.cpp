#include "astro/input_format.hpp"

#include "astro/fits.hpp"

namespace astro {

namespace {

constexpr input_format fits_format{"fits", fits_limits::max_file_bytes, true};
constexpr input_format raw_format{"raw", raw_limits::max_file_bytes, false};

}  // namespace

const input_format& fits_input_format() { return fits_format; }

const input_format& raw_input_format() { return raw_format; }

const std::vector<const input_format*>& input_formats() {
  static const std::vector<const input_format*> formats{&fits_format, &raw_format};
  return formats;
}

}  // namespace astro
