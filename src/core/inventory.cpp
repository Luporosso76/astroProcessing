#include "astro/inventory.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "astro/job_probe.hpp"
#include "fits_internal.hpp"

namespace astro {

using namespace fits_detail;

namespace {

constexpr std::uint64_t reserve_overhead = 4096;
constexpr std::uint64_t card_footprint = 128;
constexpr std::uint64_t digest_block_bytes = std::uint64_t{1} << 20;

[[noreturn]] void budget_stop(std::uint64_t minimum, std::uint64_t budget) {
  throw stop_error(error_category::budget, "sched.job_exceeds_budget", "budget sotto la prenotazione minima",
                   {{"minimo", std::to_string(minimum)}, {"budget", std::to_string(budget)}});
}

read_fn reader_of(const fits_input& input) {
  return [&input](std::uint64_t offset, void* dst, std::size_t n) { read_exact(input, offset, dst, n); };
}

template <class T>
T load_be(const std::uint8_t* p) {
  using U = std::conditional_t<sizeof(T) == 1, std::uint8_t,
            std::conditional_t<sizeof(T) == 2, std::uint16_t, std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>>;
  U raw = 0;
  for (std::size_t i = 0; i < sizeof(U); ++i) raw = static_cast<U>((raw << 8) | p[i]);
  T out;
  std::memcpy(&out, &raw, sizeof(T));
  return out;
}

// Somma, per ogni colonna di blocchi, i campioni finiti di una riga; ordine di colonna crescente.
template <class T>
void accumulate_row(const std::uint8_t* row, std::uint32_t width, std::uint32_t block_width, double bscale, double bzero,
                    double* sums, std::uint32_t* counts) {
  for (std::uint32_t x = 0; x < width; ++x) {
    const double v = static_cast<double>(load_be<T>(row + std::size_t{x} * sizeof(T))) * bscale + bzero;
    if (!std::isfinite(v)) continue;
    sums[x / block_width] += v;
    ++counts[x / block_width];
  }
}

void accumulate(int bitpix, const std::uint8_t* row, std::uint32_t width, std::uint32_t block_width, double bscale,
                double bzero, double* sums, std::uint32_t* counts) {
  switch (bitpix) {
    case 8: accumulate_row<std::uint8_t>(row, width, block_width, bscale, bzero, sums, counts); break;
    case 16: accumulate_row<std::int16_t>(row, width, block_width, bscale, bzero, sums, counts); break;
    case 32: accumulate_row<std::int32_t>(row, width, block_width, bscale, bzero, sums, counts); break;
    case 64: accumulate_row<std::int64_t>(row, width, block_width, bscale, bzero, sums, counts); break;
    case -32: accumulate_row<float>(row, width, block_width, bscale, bzero, sums, counts); break;
    default: accumulate_row<double>(row, width, block_width, bscale, bzero, sums, counts); break;
  }
}

}  // namespace

reservation reserve_or_stop(memory_pool& pool, std::uint64_t bytes, const char* label) {
  if (bytes > pool.budget_bytes()) budget_stop(bytes, pool.budget_bytes());
  std::optional<reservation> r = pool.try_reserve(bytes, label);
  if (!r) {
    throw stop_error(error_category::budget, "sched.pool_unavailable", "il pool non ha ora la memoria richiesta",
                     {{"richiesti", std::to_string(bytes)}, {"budget", std::to_string(pool.budget_bytes())}});
  }
  return std::move(*r);
}

std::uint64_t inventory_reserve_bytes(std::uint64_t file_bytes) {
  const std::uint64_t cards = std::min<std::uint64_t>(file_bytes / card_size,
                                                      std::uint64_t{fits_limits::max_hdus} * fits_limits::max_header_cards);
  return reserve_overhead + cards * card_footprint;
}

fits_inventory inventory_fits(const fits_input& input, const fits_read_options& options, memory_pool& pool) {
  const std::uint64_t reserved = inventory_reserve_bytes(input.size_bytes());
  const reservation header_memory = reserve_or_stop(pool, reserved, "inventory.header");
  const scan_result scan = analyse(input.size_bytes(), reader_of(input));
  const hdu_scan& primary = scan.hdus[0];
  const header_info info = interpret_header(primary.cards, primary.planes(), options);

  fits_inventory inv;
  inv.file_bytes = input.size_bytes();
  inv.reserved_bytes = reserved;
  inv.plan = scan.plan;
  for (std::size_t k = 0; k < scan.hdus.size(); ++k) {
    const hdu_scan& h = scan.hdus[k];
    hdu_inventory e;
    e.index = static_cast<std::uint32_t>(k);
    e.extname = h.extname;
    e.bitpix = h.bitpix;
    e.axes = h.axes;
    e.bscale = h.bscale;
    e.bzero = h.bzero;
    e.header_offset = h.header_offset;
    e.data_offset = h.data_offset;
    e.data_bytes = h.data_bytes;
    e.card_count = static_cast<std::uint32_t>(h.cards.size());
    inv.header_bytes_read += h.data_offset - h.header_offset;
    inv.hdus.push_back(std::move(e));
  }
  inv.layout = info.layout;
  inv.cfa = info.cfa;
  inv.unit = info.unit;
  inv.lin = info.lin;
  inv.lin_class = !info.lin ? linearity_class::unknown
                  : *info.lin == linearity::linear ? linearity_class::linear : linearity_class::nonlinear;
  inv.lin_origin = info.lin_origin;
  inv.phase = info.phase;
  inv.binning_x = info.binning_x;
  inv.binning_y = info.binning_y;
  inv.metadata = info.metadata;
  inv.header_cards = info.header_cards;
  inv.uninterpreted_keywords = info.uninterpreted_keywords;
  check_unchanged(input);
  return inv;
}

fits_preview make_preview(const fits_input& input, const preview_request& request, memory_pool& pool,
                          const cancel_token* cancel) {
  const auto in_range = [](std::uint32_t v) { return v >= 1 && v <= fits_limits::max_axis_length; };
  if (!in_range(request.block_width) || !in_range(request.block_height)) {
    throw stop_error(error_category::invalid_input, "preview.block", "dimensione del blocco fuori intervallo",
                     {{"larghezza", std::to_string(request.block_width)}, {"altezza", std::to_string(request.block_height)}});
  }
  const std::uint64_t header_bytes = inventory_reserve_bytes(input.size_bytes());
  reservation header_memory = reserve_or_stop(pool, header_bytes, "preview.header");

  std::uint32_t width, height, planes;
  int bitpix;
  double bscale, bzero;
  std::uint64_t data_offset;
  {
    const scan_result scan = analyse(input.size_bytes(), reader_of(input));
    const hdu_scan& p = scan.hdus[0];
    width = p.width();
    height = p.height();
    planes = p.planes();
    bitpix = p.bitpix;
    bscale = p.bscale;
    bzero = p.bzero;
    data_offset = p.data_offset;
  }
  const std::uint64_t elem = static_cast<std::uint64_t>(std::abs(bitpix) / 8);
  const std::uint32_t blocks_x = (width + request.block_width - 1) / request.block_width;
  const std::uint32_t blocks_y = (height + request.block_height - 1) / request.block_height;
  const std::uint64_t band_rows = std::min<std::uint64_t>(request.block_height, height);
  const std::uint64_t band_bytes = std::uint64_t{width} * band_rows * elem;
  const std::uint64_t sums_bytes = std::uint64_t{blocks_x} * (sizeof(double) + sizeof(std::uint32_t));
  const std::uint64_t out_bytes = std::uint64_t{blocks_x} * blocks_y * planes * (sizeof(float) + sizeof(std::uint32_t));
  const std::uint64_t rest_bytes = band_bytes + sums_bytes + out_bytes;
  const std::uint64_t need = header_bytes + rest_bytes;
  if (need > pool.budget_bytes()) budget_stop(need, pool.budget_bytes());
  reservation rest = reserve_or_stop(pool, rest_bytes, "preview.work");
  pool_buffer band(rest, static_cast<std::size_t>(band_bytes));
  pool_buffer sums_buffer(rest, static_cast<std::size_t>(sums_bytes));
  const reservation out_memory = rest.carve(out_bytes, "preview.out");

  fits_preview pv;
  pv.source_width = width;
  pv.source_height = height;
  pv.planes = planes;
  pv.block_width = request.block_width;
  pv.block_height = request.block_height;
  pv.width = blocks_x;
  pv.height = blocks_y;
  pv.reserved_bytes = need;
  pv.observed_peak_bytes = header_bytes + rest.committed_bytes();
  pv.samples.assign(std::size_t{blocks_x} * blocks_y * planes, std::numeric_limits<float>::quiet_NaN());
  pv.finite_counts.assign(pv.samples.size(), 0);

  auto* sums = reinterpret_cast<double*>(sums_buffer.data());
  auto* counts = reinterpret_cast<std::uint32_t*>(sums_buffer.data() + std::size_t{blocks_x} * sizeof(double));
  const auto* raw = reinterpret_cast<const std::uint8_t*>(band.data());
  for (std::uint32_t plane = 0; plane < planes; ++plane) {
    for (std::uint32_t by = 0; by < blocks_y; ++by) {
      if (cancel != nullptr && cancel->requested()) {
        throw stop_error(error_category::cancelled, "sched.cancelled", "annullamento durante l'anteprima");
      }
      const std::uint64_t y0 = std::uint64_t{by} * request.block_height;
      const std::uint64_t rows = std::min<std::uint64_t>(request.block_height, height - y0);
      const std::uint64_t offset = data_offset + ((std::uint64_t{plane} * height + y0) * width) * elem;
      read_exact(input, offset, band.data(), static_cast<std::size_t>(rows * width * elem));
      if (job_probe::active()) job_probe::notify("input.samples", "ino:" + std::to_string(fits_detail::identity_of(input).second));
      std::fill(sums, sums + blocks_x, 0.0);
      std::fill(counts, counts + blocks_x, 0u);
      for (std::uint64_t r = 0; r < rows; ++r) {
        accumulate(bitpix, raw + r * width * elem, width, request.block_width, bscale, bzero, sums, counts);
      }
      const std::size_t base = (std::size_t{plane} * blocks_y + by) * blocks_x;
      for (std::uint32_t bx = 0; bx < blocks_x; ++bx) {
        if (!std::isfinite(sums[bx])) {
          throw stop_error(error_category::numeric, "preview.sum_overflow", "somma del blocco non finita (overflow)",
                           {{"piano", std::to_string(plane)}, {"blocco_x", std::to_string(bx)}, {"blocco_y", std::to_string(by)}});
        }
        pv.finite_counts[base + bx] = counts[bx];
        if (counts[bx] == 0) continue;
        const float mean = static_cast<float>(sums[bx] / counts[bx]);
        if (!std::isfinite(mean)) {
          throw stop_error(error_category::numeric, "preview.f32_overflow", "media del blocco oltre il campo di f32",
                           {{"piano", std::to_string(plane)}, {"blocco_x", std::to_string(bx)}, {"blocco_y", std::to_string(by)}});
        }
        pv.samples[base + bx] = mean;
      }
    }
  }
  check_unchanged(input);
  return pv;
}

content_digest digest_fits_input(const fits_input& input, memory_pool& pool) {
  const std::uint64_t block = std::min<std::uint64_t>(digest_block_bytes, std::max<std::uint64_t>(input.size_bytes(), 1));
  reservation memory = reserve_or_stop(pool, block, "digest.block");
  pool_buffer buffer(memory, static_cast<std::size_t>(block));
  sha256_stream hasher;
  for (std::uint64_t done = 0; done < input.size_bytes();) {
    const std::size_t n = static_cast<std::size_t>(std::min<std::uint64_t>(block, input.size_bytes() - done));
    read_exact(input, done, buffer.data(), n);
    hasher.update({reinterpret_cast<const std::uint8_t*>(buffer.data()), n});
    done += n;
  }
  check_unchanged(input);
  return hasher.finish();
}

}  // namespace astro
