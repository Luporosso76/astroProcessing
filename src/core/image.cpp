#include "astro/image.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

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

std::uint64_t allocated_sample_bytes() noexcept { return g_allocated_bytes.load(std::memory_order_relaxed); }

bool equivalent_content(const image& a, const image& b) {
  return a.geometry == b.geometry && a.layout == b.layout && a.type == b.type && a.unit == b.unit &&
         a.lin == b.lin && a.phase == b.phase && a.cfa == b.cfa && a.samples == b.samples && a.invalid == b.invalid &&
         a.variance == b.variance && a.metadata == b.metadata;
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

}  // namespace astro
