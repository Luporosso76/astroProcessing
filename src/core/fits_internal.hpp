#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <fitsio.h>

#include "astro/fits.hpp"

namespace astro::fits_detail {

constexpr std::uint64_t block_size = 2880;
constexpr std::size_t card_size = 80;
constexpr std::uint64_t max_exact_integer = std::uint64_t{1} << 53;

[[noreturn]] void fail(error_category category, const char* code, std::string message,
                       stop_error::context_list context = {});
[[noreturn]] void malformed(std::string message, stop_error::context_list context = {});

std::string trim(std::string_view s);
std::string upper(std::string s);

struct parsed_card {
  std::string key;
  bool has_value = false;
  bool is_string = false;
  std::string text;
};

parsed_card parse_card(const std::string& card);
bool is_structural_key(const std::string& key);

// Tipi numerici: nullopt se il testo non e' un intero (o reale) FITS completo.
std::optional<std::int64_t> try_integer(const parsed_card& card);
std::optional<double> try_real(const parsed_card& card);

struct hdu_scan {
  std::uint64_t header_offset = 0;
  std::uint64_t data_offset = 0;
  std::uint64_t data_bytes = 0;
  std::uint64_t data_padded = 0;
  int bitpix = 0;
  std::vector<std::uint64_t> axes;
  double bscale = 1.0;
  double bzero = 0.0;
  bool has_blank = false;
  std::string extname;
  std::vector<std::string> cards;  // senza END

  std::uint32_t width() const { return static_cast<std::uint32_t>(axes[0]); }
  std::uint32_t height() const { return axes.size() > 1 ? static_cast<std::uint32_t>(axes[1]) : 1; }
  std::uint32_t planes() const { return axes.size() > 2 ? static_cast<std::uint32_t>(axes[2]) : 1; }
  std::uint64_t samples() const { return std::uint64_t{width()} * height() * planes(); }
};

using read_fn = std::function<void(std::uint64_t offset, void* dst, std::size_t n)>;

struct scan_result {
  std::vector<hdu_scan> hdus;
  fits_plan plan;
  int invalid_hdu = -1;
  int variance_hdu = -1;
};

// Scansione dei soli header con tutti i limiti di fits_limits; legge tramite `read`.
scan_result analyse(std::uint64_t size, const read_fn& read);

sample_type type_for_encoding(int bitpix, double bscale, double bzero);

struct header_info {
  std::optional<pixel_unit> unit;
  std::optional<linearity> lin;
  // Origine della linearita': assente se non e' ne' nell'header ne' dichiarata (classe `unknown`).
  std::optional<metadata_origin> lin_origin;
  std::optional<metadata_value<process_phase>> phase;
  std::optional<cfa_layout> cfa;
  std::optional<std::uint32_t> binning_x;
  std::optional<std::uint32_t> binning_y;
  plane_layout layout = plane_layout::mono;
  frame_metadata metadata;
  std::vector<std::string> header_cards;
  std::vector<std::string> uninterpreted_keywords;
};

header_info interpret_header(const std::vector<std::string>& cards, std::uint32_t planes,
                             const fits_read_options& options);

// Card dei campi interpretati dell'immagine che non sono gia' in `existing`.
std::vector<std::string> missing_cards(const image& img, const std::vector<std::string>& existing);

// Stop `fits.header_inconsistent` se i campi interpretati dell'immagine non coincidono con l'header.
void check_header_matches(const image& img, const header_info& info);

// Lo scrittore FITS non rappresenta `decoder_state` (ADR-03.b §b.2): stop `fits.decoder_state` prima di ogni buffer.
void reject_decoder_state(const image& img);

// Confronta ogni HDU letto da CFITSIO (offset, BITPIX, assi) con la scansione: differenza = fits.structure_mismatch.
void verify_structure(fitsfile* f, const std::vector<hdu_scan>& hdus);

}  // namespace astro::fits_detail
