#include "astro/fits_blocks.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "astro/job_probe.hpp"
#include "fits_internal.hpp"

namespace astro {

using namespace fits_detail;

namespace {

using digest_bytes = std::array<std::uint8_t, 32>;

digest_bytes binary_of(const std::string& hex) {
  digest_bytes out{};
  const auto nibble = [](char c) { return static_cast<std::uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10); };
  for (std::size_t i = 0; i < out.size(); ++i) out[i] = static_cast<std::uint8_t>(nibble(hex[2 * i]) << 4 | nibble(hex[2 * i + 1]));
  return out;
}

std::string hex_of(const digest_bytes& d) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string s(64, '0');
  for (std::size_t i = 0; i < d.size(); ++i) {
    s[2 * i] = digits[d[i] >> 4];
    s[2 * i + 1] = digits[d[i] & 15];
  }
  return s;
}

std::span<const std::uint8_t> as_u8(std::span<const std::byte> b) {
  return {reinterpret_cast<const std::uint8_t*>(b.data()), b.size()};
}

std::uint64_t load_be(const std::uint8_t* p, std::size_t n) {
  std::uint64_t v = 0;
  for (std::size_t i = 0; i < n; ++i) v = (v << 8) | p[i];
  return v;
}

// Somma in complemento a uno a 32 bit delle parole big-endian (convenzione FITS Checksum), a byte qualunque.
class ones_sum {
 public:
  void add(std::span<const std::uint8_t> bytes) noexcept {
    for (const std::uint8_t b : bytes) {
      word_ = (word_ << 8) | b;
      if (++filled_ == 4) {
        sum_ += word_;
        sum_ = (sum_ & 0xFFFFFFFFu) + (sum_ >> 32);
        word_ = 0;
        filled_ = 0;
      }
    }
  }
  std::uint32_t value() const noexcept {
    std::uint64_t s = sum_;
    while (s >> 32) s = (s & 0xFFFFFFFFu) + (s >> 32);
    return static_cast<std::uint32_t>(s);
  }

 private:
  std::uint64_t sum_ = 0;
  std::uint32_t word_ = 0;
  int filled_ = 0;
};

std::uint32_t ones_add(std::uint32_t a, std::uint32_t b) {
  std::uint64_t s = std::uint64_t{a} + b;
  while (s >> 32) s = (s & 0xFFFFFFFFu) + (s >> 32);
  return static_cast<std::uint32_t>(s);
}

std::optional<std::string> card_value(const hdu_scan& h, const char* key) {
  for (const std::string& c : h.cards) {
    const parsed_card p = parse_card(c);
    if (p.key == key && p.has_value) return p.text;
  }
  return std::nullopt;
}

// Hash dei blocchi di header letti dall'analisi, per tratti contigui nell'ordine di lettura.
struct header_run {
  std::uint64_t begin = 0, end = 0;
  std::string sha256;
};

}  // namespace

struct fits_block_source::impl {
  std::string path;
  content_digest expected;
  std::uint32_t tile_rows = 1;
  bool read_variance = false;
  fits_input input;
  scan_result scan;
  image header;
  std::vector<header_run> runs;
  std::uint32_t w = 0, h = 0;
  std::uint64_t elem = 4, raw_elem = 4, tiles = 0;
  bool verified = false;
  // Regioni lette: 0 primario, poi INVALID (se c'e'), poi VARIANCE (se letta). Digest di ogni blocco per regione.
  struct region {
    const hdu_scan* hdu = nullptr;
    std::uint64_t bytes_per_sample = 0;
    const char* name = "";
    std::vector<digest_bytes> tiles;
  };
  std::vector<region> regions;

  explicit impl(fits_input in) : input(std::move(in)) {}

  std::pair<std::uint32_t, std::uint32_t> rows_of(std::uint64_t tile) const {
    const std::uint64_t y0 = tile * tile_rows;
    return {static_cast<std::uint32_t>(y0), static_cast<std::uint32_t>(std::min<std::uint64_t>(h, y0 + tile_rows))};
  }
};

fits_block_source::fits_block_source(const std::string& path, const content_digest& expected,
                                     const fits_read_options& options, std::uint32_t tile_rows, bool read_variance)
    : impl_(std::make_unique<impl>(open_fits_input(path))) {
  impl& p = *impl_;
  p.path = path;
  p.expected = expected;
  p.tile_rows = tile_rows;
  p.read_variance = read_variance;
  if (tile_rows < 1) fail(error_category::invalid_input, "sched.tile_rows", "tile_rows deve essere almeno 1");
  struct pending_run {
    std::uint64_t begin = 0, end = 0;
    std::unique_ptr<sha256_stream> hash;
  };
  std::vector<pending_run> runs;
  const read_fn read = [&](std::uint64_t offset, void* dst, std::size_t n) {
    read_exact(p.input, offset, dst, n);
    if (runs.empty() || runs.back().end != offset) runs.push_back({offset, offset, std::make_unique<sha256_stream>()});
    runs.back().hash->update({static_cast<const std::uint8_t*>(dst), n});
    runs.back().end = offset + n;
  };
  p.scan = analyse(p.input.size_bytes(), read);
  for (pending_run& r : runs) p.runs.push_back({r.begin, r.end, r.hash->finish().sha256});
  const hdu_scan& primary = p.scan.hdus[0];
  const header_info info = interpret_header(primary.cards, primary.planes(), options);
  // Stesse verifiche di layout e CFA di make_image in decode_fits, su una geometria di un campione per piano.
  (void)make_image(image_geometry(1, 1, primary.planes()), p.scan.plan.type, info.layout, info.cfa);
  image& img = p.header;
  img.geometry = image_geometry(primary.width(), primary.height(), primary.planes());
  img.geometry.binning_x = info.binning_x;
  img.geometry.binning_y = info.binning_y;
  img.type = p.scan.plan.type;
  img.layout = info.layout;
  img.cfa = info.cfa;
  img.unit = info.unit;
  img.lin = info.lin;
  if (info.lin) img.lin_origin.value = info.lin_origin;
  img.phase = info.phase;
  img.metadata = info.metadata;
  img.header_cards = info.header_cards;
  img.uninterpreted_keywords = info.uninterpreted_keywords;
  img.encoding = source_encoding{primary.bitpix, primary.bzero, primary.bscale, static_cast<std::uint32_t>(primary.axes.size())};
  if (p.scan.variance_hdu >= 0) img.variance = variance_plane{};
  p.w = primary.width();
  p.h = primary.height() * primary.planes();
  p.elem = sample_size(img.type);
  p.raw_elem = static_cast<std::uint64_t>(std::abs(primary.bitpix) / 8);
  p.tiles = (std::uint64_t{p.h} + tile_rows - 1) / tile_rows;
  p.regions.push_back({&primary, p.raw_elem, "primary", {}});
  if (p.scan.invalid_hdu >= 0) p.regions.push_back({&p.scan.hdus[static_cast<std::size_t>(p.scan.invalid_hdu)], 2, "INVALID", {}});
  if (read_variance && p.scan.variance_hdu >= 0) {
    p.regions.push_back({&p.scan.hdus[static_cast<std::size_t>(p.scan.variance_hdu)], p.elem, "VARIANCE", {}});
  }
}

fits_block_source::fits_block_source(fits_block_source&&) noexcept = default;
fits_block_source& fits_block_source::operator=(fits_block_source&&) noexcept = default;
fits_block_source::~fits_block_source() = default;

const image& fits_block_source::header() const noexcept { return impl_->header; }
const fits_plan& fits_block_source::plan() const noexcept { return impl_->scan.plan; }
std::uint32_t fits_block_source::tile_rows() const noexcept { return impl_->tile_rows; }
std::uint64_t fits_block_source::tile_count() const noexcept { return impl_->tiles; }
bool fits_block_source::has_invalid() const noexcept { return impl_->scan.invalid_hdu >= 0; }
bool fits_block_source::reads_variance() const noexcept { return impl_->read_variance && impl_->scan.variance_hdu >= 0; }
std::uint64_t fits_block_source::raw_bytes_per_row() const noexcept {
  std::uint64_t per_sample = 0;
  for (const auto& r : impl_->regions) per_sample += r.bytes_per_sample;
  return impl_->w * per_sample;
}
std::uint64_t fits_block_source::band_bytes_per_row() const noexcept {
  return impl_->w * (impl_->elem + 2 + (reads_variance() ? impl_->elem : 0));
}
std::uint64_t fits_block_source::region_count() const noexcept { return impl_->regions.size(); }

void fits_block_source::verify(std::span<std::byte> work, const std::function<void()>& check_cancel) {
  impl& p = *impl_;
  if (work.size() < 8) fail(error_category::invalid_input, "fits.block_work", "buffer della passata sotto 8 byte");
  sha256_stream whole;
  std::optional<stop_error> header_changed, checksum_error, sample_error;
  const hdu_scan& primary = p.scan.hdus[0];
  const bool exact_by_construction =
      primary.bscale == 1.0 && primary.bzero == std::floor(primary.bzero) && std::fabs(primary.bzero) <= 2147483648.0;
  const bool integer_primary = primary.bitpix > 0;
  std::uint64_t pos = 0;
  // Legge [pos, end) in pezzi multipli di `align`, passando ogni pezzo a `use` (offset assoluto, byte).
  const auto stream = [&](std::uint64_t end, std::uint64_t align, const auto& use) {
    const std::size_t piece = static_cast<std::size_t>(work.size() / align * align);
    while (pos < end) {
      const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(piece, end - pos));
      if (check_cancel) check_cancel();
      read_exact(p.input, pos, work.data(), n);
      const std::span<const std::uint8_t> bytes = as_u8(work.first(n));
      whole.update(bytes);
      use(pos, bytes);
      pos += n;
    }
  };
  for (std::size_t k = 0; k < p.scan.hdus.size(); ++k) {
    const hdu_scan& hdu = p.scan.hdus[k];
    const std::optional<std::string> datasum = card_value(hdu, "DATASUM");
    const bool has_checksum = card_value(hdu, "CHECKSUM").has_value();
    const bool sums = datasum || has_checksum;
    ones_sum header_sum, data_sum;
    sha256_stream header_hash;
    stream(hdu.data_offset, 1, [&](std::uint64_t, std::span<const std::uint8_t> b) {
      header_hash.update(b);
      if (sums) header_sum.add(b);
    });
    const std::string header_digest = header_hash.finish().sha256;
    const bool header_ok = std::any_of(p.runs.begin(), p.runs.end(), [&](const header_run& r) {
      return r.begin == hdu.header_offset && r.end == hdu.data_offset && r.sha256 == header_digest;
    });
    if (!header_ok && !header_changed) {
      header_changed = stop_error(error_category::integrity, "input.modified",
                                  "l'header letto da P0 differisce da quello analizzato", {{"percorso", p.path}, {"hdu", std::to_string(k)}});
    }
    impl::region* region = nullptr;
    for (auto& r : p.regions) {
      if (r.hdu == &hdu) region = &r;
    }
    const std::uint64_t data_end = hdu.data_offset + hdu.data_bytes;
    if (region == nullptr) {
      stream(data_end, 1, [&](std::uint64_t, std::span<const std::uint8_t> b) {
        if (sums) data_sum.add(b);
      });
    } else {
      region->tiles.clear();
      const std::uint64_t row_bytes = std::uint64_t{p.w} * region->bytes_per_sample;
      for (std::uint64_t t = 0; t < p.tiles; ++t) {
        const auto [y0, y1] = p.rows_of(t);
        sha256_stream tile_hash;
        stream(hdu.data_offset + std::uint64_t{y1} * row_bytes, region->bytes_per_sample,
               [&](std::uint64_t off, std::span<const std::uint8_t> b) {
                 tile_hash.update(b);
                 if (sums) data_sum.add(b);
                 if (sample_error) return;
                 if ((&hdu == &primary && integer_primary) || (region->bytes_per_sample == 2 && &hdu != &primary)) {
                   if (job_probe::active()) job_probe::notify("input.samples", "ino:" + std::to_string(fits_detail::identity_of(p.input).second));
                 }
                 const std::uint64_t first = (off - hdu.data_offset) / region->bytes_per_sample;
                 const std::uint64_t count = b.size() / region->bytes_per_sample;
                 for (std::uint64_t i = 0; i < count && !sample_error; ++i) {
                   const std::uint8_t* s = b.data() + i * region->bytes_per_sample;
                   if (&hdu == &primary && integer_primary) {
                     std::int64_t r = 0;
                     switch (hdu.bitpix) {
                       case 8: r = s[0]; break;
                       case 16: r = static_cast<std::int16_t>(load_be(s, 2)); break;
                       case 32: r = static_cast<std::int32_t>(load_be(s, 4)); break;
                       default: r = static_cast<std::int64_t>(load_be(s, 8));
                     }
                     if (hdu.bitpix == 64) {
                       constexpr auto limit = static_cast<std::int64_t>(max_exact_integer);
                       if (r > limit || r < -limit) {
                         sample_error = stop_error(error_category::unsupported, "fits.int64_range", "BITPIX 64 con modulo oltre 2^53");
                         continue;
                       }
                     }
                     const auto rd = static_cast<double>(r);
                     const double v = rd * hdu.bscale + hdu.bzero;
                     const bool f32 = p.header.type == sample_type::f32;
                     const bool representable = f32 ? static_cast<double>(static_cast<float>(v)) == v : true;
                     if (!exact_by_construction && (std::round((v - hdu.bzero) / hdu.bscale) != rd || !representable)) {
                       sample_error = stop_error(error_category::unsupported, "fits.scaling_not_exact",
                                                 "BSCALE/BZERO non invertibili sul campione", {{"indice", std::to_string(first + i)}});
                     }
                   } else if (region->bytes_per_sample == 2 && &hdu != &primary) {
                     const auto bits = static_cast<std::uint16_t>(load_be(s, 2) ^ 0x8000u);
                     if ((bits & invalid_reason::reserved_mask) != 0) {
                       sample_error = stop_error(error_category::invalid_input, "fits.invalid_reserved_bits",
                                                 "bit riservato acceso in INVALID", {{"indice", std::to_string(first + i)}});
                     }
                   }
                 }
               });
        region->tiles.push_back(binary_of(tile_hash.finish().sha256));
      }
    }
    stream(hdu.data_offset + hdu.data_padded, 1, [&](std::uint64_t, std::span<const std::uint8_t> b) {
      if (sums) data_sum.add(b);
    });
    if (!checksum_error && sums) {
      const std::uint32_t data_value = data_sum.value();
      const bool data_bad = datasum && static_cast<double>(data_value) != std::strtod(datasum->c_str(), nullptr);
      const std::uint32_t hdu_value = ones_add(header_sum.value(), data_value);
      const bool hdu_bad = has_checksum && hdu_value != 0 && hdu_value != 0xFFFFFFFFu;
      if (data_bad || hdu_bad) {
        checksum_error = stop_error(error_category::integrity, "fits.checksum", "CHECKSUM o DATASUM non corrispondente",
                                    {{"hdu", std::to_string(k)}});
      }
    }
  }
  require_input_digest(whole.finish(), p.expected, p.path);
  if (header_changed) throw *header_changed;
  if (checksum_error) throw *checksum_error;
  if (sample_error) throw *sample_error;
  p.verified = true;
}

void fits_block_source::read_tile(std::uint64_t tile, std::span<std::byte> raw, row_band& out) const {
  const impl& p = *impl_;
  if (job_probe::active()) job_probe::notify("input.samples", "ino:" + std::to_string(fits_detail::identity_of(p.input).second));
  ASTRO_ASSERT(p.verified && tile < p.tiles);
  const auto [y0, y1] = p.rows_of(tile);
  out.row_begin = y0;
  out.row_end = y1;
  const std::uint64_t n = std::uint64_t{y1 - y0} * p.w;
  const bool f32 = p.header.type == sample_type::f32;
  bool mask_read = false;
  for (const auto& r : p.regions) {
    const std::uint64_t bytes = n * r.bytes_per_sample;
    ASTRO_ASSERT(bytes <= raw.size());
    read_exact(p.input, r.hdu->data_offset + std::uint64_t{y0} * p.w * r.bytes_per_sample, raw.data(), bytes);
    const std::span<const std::uint8_t> got = as_u8(raw.first(bytes));
    sha256_stream hash;
    hash.update(got);
    const std::string digest = hash.finish().sha256;
    if (binary_of(digest) != r.tiles[tile]) {
      throw stop_error(error_category::integrity, "input.consumed_digest",
                       "i byte letti differiscono da quelli verificati nella passata P0",
                       {{"input", p.path}, {"blocco", std::to_string(tile)}, {"regione", r.name},
                        {"atteso", hex_of(r.tiles[tile])}, {"letto", digest}});
    }
    const std::uint8_t* s = got.data();
    if (r.hdu == &p.scan.hdus[0]) {
      const hdu_scan& h = *r.hdu;
      for (std::uint64_t i = 0; i < n; ++i, s += r.bytes_per_sample) {
        double v = 0;
        switch (h.bitpix) {
          case 8: v = static_cast<double>(s[0]) * h.bscale + h.bzero; break;
          case 16: v = static_cast<double>(static_cast<std::int16_t>(load_be(s, 2))) * h.bscale + h.bzero; break;
          case 32: v = static_cast<double>(static_cast<std::int32_t>(load_be(s, 4))) * h.bscale + h.bzero; break;
          case 64: v = static_cast<double>(static_cast<std::int64_t>(load_be(s, 8))) * h.bscale + h.bzero; break;
          case -32: {
            const auto u = static_cast<std::uint32_t>(load_be(s, 4));
            float f;
            std::memcpy(&f, &u, 4);
            static_cast<float*>(out.samples)[i] = f;
            continue;
          }
          default: {
            const std::uint64_t u = load_be(s, 8);
            double d;
            std::memcpy(&d, &u, 8);
            static_cast<double*>(out.samples)[i] = d;
            continue;
          }
        }
        if (f32) static_cast<float*>(out.samples)[i] = static_cast<float>(v);
        else static_cast<double*>(out.samples)[i] = v;
      }
    } else if (r.bytes_per_sample == 2 && std::string_view(r.name) == "INVALID") {
      for (std::uint64_t i = 0; i < n; ++i) out.mask[i] = static_cast<std::uint16_t>(load_be(s + 2 * i, 2) ^ 0x8000u);
      mask_read = true;
    } else {
      for (std::uint64_t i = 0; i < n; ++i) {
        if (f32) {
          const auto u = static_cast<std::uint32_t>(load_be(s + 4 * i, 4));
          std::memcpy(static_cast<float*>(out.variance) + i, &u, 4);
        } else {
          const std::uint64_t u = load_be(s + 8 * i, 8);
          std::memcpy(static_cast<double*>(out.variance) + i, &u, 8);
        }
      }
    }
  }
  if (!mask_read) std::fill(out.mask, out.mask + n, std::uint16_t{0});
  for (std::uint64_t i = 0; i < n; ++i) {
    const double v = f32 ? static_cast<double>(static_cast<const float*>(out.samples)[i]) : static_cast<const double*>(out.samples)[i];
    if (!std::isfinite(v) && out.mask[i] == 0) out.mask[i] = invalid_reason::source_nonfinite;
  }
}

void fits_block_source::check_identity() const {
  check_path_names(impl_->path, impl_->input);
  check_unchanged(impl_->input);
}

std::uint64_t primary_sample_bytes(const fits_input& input) {
  const read_fn read = [&input](std::uint64_t offset, void* dst, std::size_t n) { read_exact(input, offset, dst, n); };
  return static_cast<std::uint64_t>(std::abs(analyse(input.size_bytes(), read).hdus[0].bitpix) / 8);
}

// ---------------------------------------------------------------- FITS in streaming

fits_stream_writer::fits_stream_writer(const image& header_only, bool with_invalid) : with_invalid_(with_invalid) {
  // Lo staging contiene campioni in virgola mobile del tipo dell'immagine: solo l'encoding identita' corrispondente.
  const int float_bitpix = header_only.type == sample_type::f32 ? -32 : -64;
  if (header_only.encoding && (header_only.encoding->bitpix != float_bitpix || header_only.encoding->bzero != 0.0 ||
                               header_only.encoding->bscale != 1.0)) {
    fail(error_category::invalid_input, "fits.encoding",
         "FITS in streaming: ammesso solo l'encoding in virgola mobile identita' del tipo dei campioni",
         {{"bitpix", std::to_string(header_only.encoding->bitpix)}});
  }
  const image_geometry& g = header_only.geometry;
  image_geometry one(g.width, 1, g.planes);
  one.binning_x = g.binning_x;
  one.binning_y = g.binning_y;
  image t = make_image(one, header_only.type, header_only.layout, header_only.cfa);
  t.unit = header_only.unit;
  t.lin = header_only.lin;
  t.lin_origin = header_only.lin_origin;
  t.phase = header_only.phase;
  t.metadata = header_only.metadata;
  t.header_cards = header_only.header_cards;
  t.uninterpreted_keywords = header_only.uninterpreted_keywords;
  t.encoding = header_only.encoding;
  if (header_only.variance) t.variance = variance_plane{aligned_buffer(t.samples.size())};
  if (with_invalid) t.invalid.bits[0] = invalid_reason::source_flagged;
  const std::vector<std::uint8_t> encoded = encode_fits(t);
  const scan_result scan = analyse(encoded.size(), [&](std::uint64_t off, void* dst, std::size_t n) {
    std::memcpy(dst, encoded.data() + off, n);
  });
  elem_ = sample_size(header_only.type);
  samples_ = g.sample_count();
  const std::string height = std::to_string(g.height);
  for (const hdu_scan& h : scan.hdus) {
    std::vector<std::uint8_t> bytes(encoded.begin() + static_cast<std::ptrdiff_t>(h.header_offset),
                                    encoded.begin() + static_cast<std::ptrdiff_t>(h.data_offset));
    bool patched = false;
    for (std::size_t c = 0; c + card_size <= bytes.size(); c += card_size) {
      if (std::memcmp(bytes.data() + c, "NAXIS2  = ", 10) != 0) continue;
      std::memset(bytes.data() + c + 10, ' ', 20);
      std::memcpy(bytes.data() + c + 30 - height.size(), height.data(), height.size());
      patched = true;
      break;
    }
    if (!patched && g.height != 1) fail(error_category::invalid_input, "fits.encoding", "header di uscita senza NAXIS2");
    headers_.push_back(std::move(bytes));
  }
  // Rilettura con lo stesso parser, come encode_fits: struttura e campi interpretati.
  std::uint64_t total = 0;
  for (std::size_t k = 0; k < headers_.size(); ++k) total += headers_[k].size() + padded(k);
  file_bytes_ = total;
  const scan_result check = analyse(total, [&](std::uint64_t off, void* dst, std::size_t n) {
    std::uint64_t base = 0;
    for (std::size_t k = 0; k < headers_.size(); ++k) {
      if (off >= base && off + n <= base + headers_[k].size()) {
        std::memcpy(dst, headers_[k].data() + (off - base), n);
        return;
      }
      base += headers_[k].size() + padded(k);
    }
    fail(error_category::integrity, "fits.write", "rilettura dell'header fuori dagli header composti");
  });
  if (check.hdus.size() != headers_.size()) fail(error_category::integrity, "fits.write", "numero di HDU composti diverso");
  for (const hdu_scan& h : check.hdus) {
    if (h.width() != g.width || h.height() != g.height || h.planes() != g.planes) {
      fail(error_category::integrity, "fits.write", "geometria dell'header composto diversa dall'immagine");
    }
  }
  check_header_matches(header_only, interpret_header(check.hdus[0].cards, check.hdus[0].planes(), {}));
}

std::uint64_t fits_stream_writer::data_bytes(std::size_t hdu) const noexcept {
  const bool is_invalid = with_invalid_ && hdu == 1;
  return samples_ * (is_invalid ? 2 : elem_);
}

std::uint64_t fits_stream_writer::padded(std::size_t hdu) const noexcept {
  return (data_bytes(hdu) + block_size - 1) / block_size * block_size;
}

std::uint64_t fits_stream_writer::staging_offset(std::size_t hdu) const noexcept {
  if (hdu == 0) return 0;
  if (with_invalid_ && hdu == 1) return samples_ * elem_;
  return samples_ * (elem_ + 2);
}

std::uint64_t fits_stream_writer::file_bytes() const noexcept { return file_bytes_; }

std::function<std::size_t(std::span<std::uint8_t>)> fits_stream_writer::reader(staging_reader read) const {
  struct cursor {
    std::size_t hdu = 0;
    int part = 0;  // 0 header, 1 dati, 2 riempimento
    std::uint64_t done = 0;
  };
  return [this, read = std::move(read), c = cursor{}](std::span<std::uint8_t> out) mutable -> std::size_t {
    std::size_t filled = 0;
    while (filled < out.size() && c.hdu < headers_.size()) {
      const std::uint64_t part_size = c.part == 0 ? headers_[c.hdu].size()
                                      : c.part == 1 ? data_bytes(c.hdu) : padded(c.hdu) - data_bytes(c.hdu);
      if (c.done == part_size) {
        c.done = 0;
        if (++c.part == 3) {
          c.part = 0;
          ++c.hdu;
        }
        continue;
      }
      const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(part_size - c.done, out.size() - filled));
      std::uint8_t* dst = out.data() + filled;
      if (c.part == 0) {
        std::memcpy(dst, headers_[c.hdu].data() + c.done, n);
      } else if (c.part == 1) {
        read(staging_offset(c.hdu) + c.done, std::span<std::byte>(reinterpret_cast<std::byte*>(dst), n));
      } else {
        std::memset(dst, 0, n);
      }
      c.done += n;
      filled += n;
    }
    return filled;
  };
}

}  // namespace astro
