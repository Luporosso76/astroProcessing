#include "astro/image.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <new>
#include <string>

namespace astro {

namespace {

std::atomic<std::uint64_t> g_allocated_bytes{0};

[[noreturn]] void fail(error_category cat, const char* code, const char* msg) {
  throw stop_error(cat, code, msg);
}

std::size_t checked_bytes(std::uint64_t count, std::size_t element) {
  std::uint64_t bytes = 0;
  if (__builtin_mul_overflow(count, static_cast<std::uint64_t>(element), &bytes) ||
      bytes > static_cast<std::uint64_t>(PTRDIFF_MAX)) {
    fail(error_category::limit_exceeded, "image.size_overflow",
         "dimensione in byte oltre il limite indirizzabile");
  }
  return static_cast<std::size_t>(bytes);
}

// Limite del motivo del nero: LIBRAW_CBLACK_SIZE = 4104 meno i sei elementi iniziali (ADR-03.b §b.2).
constexpr std::uint64_t max_black_pattern = 4098;

// Canali della cella 2x2 del pattern, ordine (0, 0) (0, 1) (1, 0) (1, 1): 0 rosso, 1 verde, 2 blu.
std::array<int, 4> pattern_cell(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return {0, 1, 1, 2};
    case cfa_pattern::bggr: return {2, 1, 1, 0};
    case cfa_pattern::grbg: return {1, 0, 2, 1};
    case cfa_pattern::gbrg: return {1, 2, 0, 1};
  }
  std::abort();
}

// Canale del sito (riga r, colonna c): pattern traslato di (c + x_offset, r + y_offset) come in ADR-03.a.
int cfa_site_channel(const cfa_layout& cfa, unsigned r, unsigned c) {
  return pattern_cell(cfa.pattern)[((r + cfa.y_offset) & 1u) * 2 + ((c + cfa.x_offset) & 1u)];
}

[[noreturn]] void bad_decoder_state(const char* field) {
  throw stop_error(error_category::invalid_input, "image.decoder_state", "stato del decoder incoerente",
                   {{"campo", field}});
}

// Chiamata dopo il controllo del layout: con layout cfa, `cfa` e' presente.
void validate_decoder_state(const image& img) {
  if (!img.decoder_state) return;
  const decoder_state& s = *img.decoder_state;
  if (img.layout != plane_layout::cfa) bad_decoder_state("layout");
  if (!img.phase || img.phase->value != process_phase::raw) bad_decoder_state("phase");
  if (s.source.origin != metadata_origin::decoder) bad_decoder_state("source");
  const decoder_window& w = s.window;
  if (w.width != img.geometry.width || w.height != img.geometry.height) bad_decoder_state("window");
  if (std::uint64_t{w.x0} + w.width > s.raw_width || std::uint64_t{w.y0} + w.height > s.raw_height) {
    bad_decoder_state("window");
  }
  const std::uint64_t cells = std::uint64_t{s.black_pattern_rows} * s.black_pattern_cols;
  if ((s.black_pattern_rows == 0) != (s.black_pattern_cols == 0) || cells > max_black_pattern ||
      s.black_pattern.size() != cells) {
    bad_decoder_state("black_pattern");
  }
  if (s.curve_sha256.has_value() != (s.linearization == decoder_linearization::decoder_curve)) {
    bad_decoder_state("curve_sha256");
  }
  for (unsigned site = 0; site < 4; ++site) {
    const int channel = cfa_site_channel(*img.cfa, site / 2, site % 2);
    const std::uint8_t index = s.cfa_color_index[site];
    if (channel == 1 ? index != 1 && index != 3 : index != channel) bad_decoder_state("cfa_color_index");
  }
}

void validate_measure(const image& img) {
  if (!img.measure) return;
  const std::vector<measure_limit>& l = img.measure->limits;
  for (std::size_t i = 0; i < l.size(); ++i) {
    if (is_derived_limit(l[i]) || (i > 0 && l[i] <= l[i - 1])) {
      throw stop_error(error_category::invalid_input, "image.measure",
                       "measure: codice derivato, duplicato o fuori ordine canonico", {{"codice", to_string(l[i])}});
    }
  }
}

template <class T>
bool is_nonfinite(const std::byte* data, std::size_t i) {
  T v;
  std::memcpy(&v, data + i * sizeof(T), sizeof(T));
  return !std::isfinite(v);
}

template <class T>
bool is_nan(const std::byte* data, std::size_t i) {
  T v;
  std::memcpy(&v, data + i * sizeof(T), sizeof(T));
  return std::isnan(v);
}

}  // namespace

std::uint64_t image_geometry::sample_count() const {
  if (width == 0 || height == 0 || planes == 0) {
    fail(error_category::invalid_input, "image.geometry", "width, height e planes devono essere almeno 1");
  }
  std::uint64_t n = static_cast<std::uint64_t>(width) * height;
  if (__builtin_mul_overflow(n, static_cast<std::uint64_t>(planes), &n)) {
    fail(error_category::limit_exceeded, "image.size_overflow", "numero di campioni oltre 2^64");
  }
  return n;
}

aligned_buffer::aligned_buffer(std::size_t bytes) : size_(bytes) {
  if (bytes == 0) return;
  std::size_t padded = (bytes + alignment - 1) / alignment * alignment;
  void* p = std::aligned_alloc(alignment, padded);
  if (p == nullptr) throw std::bad_alloc();
  std::memset(p, 0, padded);
  data_ = static_cast<std::byte*>(p);
  g_allocated_bytes.fetch_add(padded, std::memory_order_relaxed);
}

aligned_buffer::aligned_buffer(const aligned_buffer& other) : aligned_buffer(other.size_) {
  if (size_ != 0) std::memcpy(data_, other.data_, size_);
}

aligned_buffer::aligned_buffer(aligned_buffer&& other) noexcept
    : data_(other.data_), size_(other.size_) {
  other.data_ = nullptr;
  other.size_ = 0;
}

aligned_buffer& aligned_buffer::operator=(const aligned_buffer& other) {
  if (this != &other) {
    aligned_buffer copy(other);
    *this = std::move(copy);
  }
  return *this;
}

aligned_buffer& aligned_buffer::operator=(aligned_buffer&& other) noexcept {
  if (this != &other) {
    std::free(data_);
    data_ = other.data_;
    size_ = other.size_;
    other.data_ = nullptr;
    other.size_ = 0;
  }
  return *this;
}

aligned_buffer::~aligned_buffer() { std::free(data_); }

bool operator==(const aligned_buffer& a, const aligned_buffer& b) noexcept {
  return a.size_ == b.size_ && (a.size_ == 0 || std::memcmp(a.data_, b.data_, a.size_) == 0);
}

invalid_mask::invalid_mask(const image_geometry& geometry)
    : width(geometry.width), height(geometry.height), planes(geometry.planes) {
  std::uint64_t n = geometry.sample_count();
  g_allocated_bytes.fetch_add(checked_bytes(n, sizeof(std::uint16_t)), std::memory_order_relaxed);
  bits.assign(static_cast<std::size_t>(n), 0);
}

edit_mask::edit_mask(std::uint32_t width, std::uint32_t height) : width_(width), height_(height) {
  if (width == 0 || height == 0) {
    fail(error_category::invalid_input, "image.geometry", "width e height devono essere almeno 1");
  }
  std::uint64_t n = static_cast<std::uint64_t>(width) * height;
  checked_bytes(n, sizeof(float));
  values_.assign(static_cast<std::size_t>(n), 0.0f);
}

float edit_mask::at(std::uint32_t x, std::uint32_t y) const {
  if (x >= width_ || y >= height_) fail(error_category::invalid_input, "mask.edit_index", "indice fuori maschera");
  return values_[static_cast<std::size_t>(y) * width_ + x];
}

void edit_mask::set(std::uint32_t x, std::uint32_t y, float value) {
  if (x >= width_ || y >= height_) fail(error_category::invalid_input, "mask.edit_index", "indice fuori maschera");
  if (!(value >= 0.0f && value <= 1.0f)) {
    fail(error_category::invalid_input, "mask.edit_range", "valore di edit_mask fuori da [0, 1]");
  }
  values_[static_cast<std::size_t>(y) * width_ + x] = value;
}

const char* to_string(metadata_origin origin) noexcept {
  switch (origin) {
    case metadata_origin::header: return "header";
    case metadata_origin::declared: return "declared";
    case metadata_origin::decoder: return "decoder";
  }
  std::abort();
}

std::uint64_t allocated_sample_bytes() noexcept { return g_allocated_bytes.load(std::memory_order_relaxed); }

bool equivalent_content(const image& a, const image& b) {
  return a.geometry == b.geometry && a.layout == b.layout && a.type == b.type && a.unit == b.unit &&
         a.lin == b.lin && a.phase == b.phase && a.cfa == b.cfa && a.decoder_state == b.decoder_state && a.measure == b.measure &&
         a.samples == b.samples && a.invalid == b.invalid && a.variance == b.variance && a.metadata == b.metadata;
}

image make_image(const image_geometry& geometry, sample_type type, plane_layout layout,
                 std::optional<cfa_layout> cfa) {
  std::uint64_t n = geometry.sample_count();
  image img;
  img.geometry = geometry;
  img.type = type;
  img.layout = layout;
  img.cfa = cfa;
  img.samples = aligned_buffer(checked_bytes(n, sample_size(type)));
  img.invalid = invalid_mask(geometry);
  validate_image(img);
  return img;
}

void validate_image(const image& img) {
  const std::uint64_t n = img.geometry.sample_count();
  if (img.samples.size() != checked_bytes(n, sample_size(img.type))) {
    fail(error_category::invalid_input, "image.samples_size", "buffer dei campioni di dimensione errata");
  }
  if (reinterpret_cast<std::uintptr_t>(img.samples.data()) % aligned_buffer::alignment != 0) {
    fail(error_category::invalid_input, "image.alignment", "buffer dei campioni non allineato a 64 byte");
  }
  if (img.invalid.width != img.geometry.width || img.invalid.height != img.geometry.height ||
      img.invalid.planes != img.geometry.planes || img.invalid.bits.size() != n) {
    fail(error_category::invalid_input, "image.mask_size", "invalid_mask con geometria diversa dai campioni");
  }
  if (img.variance && img.variance->samples.size() != img.samples.size()) {
    fail(error_category::invalid_input, "image.variance_size", "varianza con dimensione diversa dai campioni");
  }
  if (img.cfa && img.geometry.planes != 1) {
    fail(error_category::invalid_input, "image.cfa_planes", "cfa richiede planes == 1");
  }
  if (img.cfa && (img.cfa->x_offset > 1 || img.cfa->y_offset > 1)) {
    fail(error_category::invalid_input, "image.cfa_offset", "offset cfa diverso da 0 e 1");
  }
  const std::uint32_t p = img.geometry.planes;
  bool ok = false;
  switch (img.layout) {
    case plane_layout::mono: ok = p == 1 && !img.cfa; break;
    case plane_layout::cfa: ok = p == 1 && img.cfa.has_value(); break;
    case plane_layout::rgb: ok = p == 3; break;
    case plane_layout::multi_plane: ok = p > 1; break;
  }
  if (!ok) fail(error_category::invalid_input, "image.layout", "plane_layout incoerente con planes e cfa");
  validate_decoder_state(img);
  validate_measure(img);
}

bool operator_output_invariants_hold(const image& img) {
  validate_image(img);
  const std::size_t n = img.invalid.bits.size();
  for (std::size_t i = 0; i < n; ++i) {
    const bool nonfinite = img.type == sample_type::f32
                               ? is_nonfinite<float>(img.samples.data(), i)
                               : is_nonfinite<double>(img.samples.data(), i);
    const std::uint16_t bits = img.invalid.bits[i];
    if (nonfinite && bits == 0) return false;
    if (bits != 0) {
      const bool nan = img.type == sample_type::f32 ? is_nan<float>(img.samples.data(), i)
                                                    : is_nan<double>(img.samples.data(), i);
      if (!nan) return false;
      if (img.variance) {
        const bool vnan = img.type == sample_type::f32 ? is_nan<float>(img.variance->samples.data(), i)
                                                       : is_nan<double>(img.variance->samples.data(), i);
        if (!vnan) return false;
      }
    }
  }
  return true;
}

const char* to_string(measure_limit limit) noexcept {
  switch (limit) {
    case measure_limit::history_unknown: return "history.unknown";
    case measure_limit::calib_omitted_dark_subtraction: return "calib.omitted.dark_subtraction";
    case measure_limit::calib_omitted_bias_subtraction: return "calib.omitted.bias_subtraction";
    case measure_limit::calib_omitted_dark_flat_subtraction: return "calib.omitted.dark_flat_subtraction";
    case measure_limit::calib_omitted_flat_bias_subtraction: return "calib.omitted.flat_bias_subtraction";
    case measure_limit::calib_omitted_flat_normalization: return "calib.omitted.flat_normalization";
    case measure_limit::calib_omitted_flat_division: return "calib.omitted.flat_division";
    case measure_limit::calib_offset_known_not_removed: return "calib.offset.known_not_removed";
    case measure_limit::calib_offset_unknown: return "calib.offset.unknown";
    case measure_limit::calib_assumption_h_dprime: return "calib.assumption.h_dprime";
    case measure_limit::calib_assumption_flat_thermal: return "calib.assumption.flat_thermal";
    case measure_limit::calib_assumption_decoder_black: return "calib.assumption.decoder_black";
    case measure_limit::linearity_nonlinear: return "linearity.nonlinear";
    case measure_limit::linearity_unknown: return "linearity.unknown";
    case measure_limit::variance_absent: return "variance.absent";
    case measure_limit::variance_scope_unknown: return "variance.scope_unknown";
    case measure_limit::variance_marginal: return "variance.marginal";
    case measure_limit::variance_fixed_weight_approximation: return "variance.fixed_weight_approximation";
    case measure_limit::samples_interpolated: return "samples.interpolated";
  }
  std::abort();
}

bool is_derived_limit(measure_limit limit) noexcept {
  return limit >= measure_limit::linearity_nonlinear && limit <= measure_limit::variance_marginal;
}

std::vector<measure_limit> history_limits(const image& img) {
  if (img.measure) return img.measure->limits;
  if (img.phase && img.phase->value == process_phase::raw) return {};
  return {measure_limit::history_unknown};
}

std::vector<measure_limit> measure_limits(const image& img) {
  std::vector<measure_limit> out = history_limits(img);
  if (img.lin == linearity::nonlinear) out.push_back(measure_limit::linearity_nonlinear);
  if (!img.lin) out.push_back(measure_limit::linearity_unknown);
  if (!img.variance) out.push_back(measure_limit::variance_absent);
  std::sort(out.begin(), out.end());
  return out;
}

measure_capability merge_measure(const std::vector<const image*>& inputs, const std::vector<measure_limit>& own) {
  measure_capability m;
  for (const image* in : inputs) {
    const std::vector<measure_limit> h = history_limits(*in);
    m.limits.insert(m.limits.end(), h.begin(), h.end());
  }
  for (measure_limit l : own) {
    if (is_derived_limit(l)) {
      throw stop_error(error_category::invalid_input, "image.measure", "codice derivato tra quelli aggiunti dall'operatore",
                       {{"codice", to_string(l)}});
    }
    m.limits.push_back(l);
  }
  std::sort(m.limits.begin(), m.limits.end());
  m.limits.erase(std::unique(m.limits.begin(), m.limits.end()), m.limits.end());
  return m;
}

measure_assessment assess_measure(const image& img, const measure_acceptance& acceptance) {
  measure_assessment r;
  r.limits = measure_limits(img);
  for (measure_limit l : r.limits) {
    if (std::find(acceptance.accepted.begin(), acceptance.accepted.end(), l) == acceptance.accepted.end()) {
      r.blocking.push_back(l);
    }
  }
  return r;
}

void require_measure_ready(const image& img, const measure_acceptance& acceptance) {
  const measure_assessment r = assess_measure(img, acceptance);
  if (r.blocking.empty()) return;
  std::string codes;
  for (measure_limit l : r.blocking) codes += std::string(codes.empty() ? "" : ", ") + to_string(l);
  throw stop_error(error_category::incompatible, "measure.limit_blocked", "limitazione non accettata dalla misura",
                   {{"codici", codes}});
}

}  // namespace astro
