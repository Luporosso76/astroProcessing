#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "astro/error.hpp"

namespace astro {

enum class sample_type { f32, f64 };
enum class pixel_unit { adu, electron, dimensionless };
enum class linearity { linear, nonlinear };
// Classe di linearita' di un'immagine: `unknown` e' un valore esplicito e non e' mai trattato come `linear` (ADR-03).
enum class linearity_class { linear, nonlinear, unknown };
enum class process_phase { raw, calibrated, prestacked };
enum class cfa_pattern { rggb, bggr, grbg, gbrg };
enum class plane_layout { mono, cfa, rgb, multi_plane };

constexpr std::size_t sample_size(sample_type type) noexcept {
  return type == sample_type::f32 ? 4 : 8;
}

struct cfa_layout {
  cfa_pattern pattern;
  std::uint8_t x_offset;
  std::uint8_t y_offset;
  friend bool operator==(const cfa_layout&, const cfa_layout&) = default;
};

struct image_geometry {
  image_geometry() = default;
  image_geometry(std::uint32_t w, std::uint32_t h, std::uint32_t p = 1) : width(w), height(h), planes(p) {}

  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t planes = 1;
  std::optional<std::uint32_t> binning_x;
  std::optional<std::uint32_t> binning_y;

  // width * height * planes con controllo di overflow; geometria con una dimensione nulla: invalid_input.
  std::uint64_t sample_count() const;
  friend bool operator==(const image_geometry&, const image_geometry&) = default;
};

namespace invalid_reason {
constexpr std::uint16_t source_nonfinite = 1u << 0;
constexpr std::uint16_t source_flagged = 1u << 1;
constexpr std::uint16_t saturated = 1u << 2;
constexpr std::uint16_t master_invalid = 1u << 3;
constexpr std::uint16_t q_nonpositive = 1u << 4;
constexpr std::uint16_t q_nonfinite = 1u << 5;
constexpr std::uint16_t q_below_min = 1u << 6;
constexpr std::uint16_t q_unreliable = 1u << 7;
constexpr std::uint16_t result_nonfinite = 1u << 8;
constexpr std::uint16_t reserved_mask = 0xFE00;
}  // namespace invalid_reason

// Buffer contiguo allineato a 64 byte, posseduto, azzerato alla creazione.
class aligned_buffer {
 public:
  static constexpr std::size_t alignment = 64;

  aligned_buffer() = default;
  explicit aligned_buffer(std::size_t bytes);
  aligned_buffer(const aligned_buffer& other);
  aligned_buffer(aligned_buffer&& other) noexcept;
  aligned_buffer& operator=(const aligned_buffer& other);
  aligned_buffer& operator=(aligned_buffer&& other) noexcept;
  ~aligned_buffer();

  std::size_t size() const noexcept { return size_; }
  std::byte* data() noexcept { return data_; }
  const std::byte* data() const noexcept { return data_; }

  template <class T>
  T* as() noexcept {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);
    return reinterpret_cast<T*>(data_);
  }
  template <class T>
  const T* as() const noexcept {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);
    return reinterpret_cast<const T*>(data_);
  }

  friend bool operator==(const aligned_buffer& a, const aligned_buffer& b) noexcept;

 private:
  std::byte* data_ = nullptr;
  std::size_t size_ = 0;
};

// Un uint16_t per pixel e per piano, bit di invalid_reason. Tipo distinto da edit_mask.
struct invalid_mask {
  invalid_mask() = default;
  explicit invalid_mask(const image_geometry& geometry);

  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint32_t planes = 0;
  std::vector<std::uint16_t> bits;
  friend bool operator==(const invalid_mask&, const invalid_mask&) = default;
};

// Un float in [0, 1] per pixel. Nessuna conversione da o verso invalid_mask.
class edit_mask {
 public:
  edit_mask(std::uint32_t width, std::uint32_t height);

  std::uint32_t width() const noexcept { return width_; }
  std::uint32_t height() const noexcept { return height_; }
  float at(std::uint32_t x, std::uint32_t y) const;
  void set(std::uint32_t x, std::uint32_t y, float value);

 private:
  std::uint32_t width_;
  std::uint32_t height_;
  std::vector<float> values_;
};

struct variance_plane {
  aligned_buffer samples;
  friend bool operator==(const variance_plane&, const variance_plane&) = default;
};

// `decoder`: valore del decoder RAW; `metadata_source.keyword` e' il nome del campo di LibRaw (ADR-03.b §b.1).
enum class metadata_origin { header, declared, decoder };

const char* to_string(metadata_origin origin) noexcept;

struct metadata_source {
  metadata_origin origin = metadata_origin::header;
  std::string keyword;
  friend bool operator==(const metadata_source&, const metadata_source&) = default;
};

template <class T>
struct metadata_value {
  T value;
  metadata_source source;
  // L'origine e' provenienza (ADR-05), non contenuto: l'uguaglianza confronta i valori.
  friend bool operator==(const metadata_value& a, const metadata_value& b) { return a.value == b.value; }
};

struct frame_metadata {
  std::optional<metadata_value<double>> exposure_s;
  std::optional<metadata_value<double>> gain;
  std::optional<metadata_value<double>> conversion_gain_e_per_adu;
  std::optional<metadata_value<double>> offset_adu;
  std::optional<metadata_value<double>> temperature_c;
  std::optional<metadata_value<std::string>> filter;
  std::optional<metadata_value<std::string>> camera;
  std::optional<metadata_value<std::string>> imagetyp;
  std::optional<metadata_value<double>> saturation_adu;
  std::optional<metadata_value<double>> pedestal_adu;
  std::optional<metadata_value<std::int64_t>> roi_x;
  std::optional<metadata_value<std::int64_t>> roi_y;
  std::optional<metadata_value<std::string>> readout_mode;
  std::optional<metadata_value<std::string>> row_order;
  // `other.timestamp` di LibRaw: fuso orario non dichiarato, non e' un istante UTC e non va in DATE-OBS (ADR-03.b §b.1).
  std::optional<metadata_value<std::int64_t>> capture_time_libraw;
  friend bool operator==(const frame_metadata&, const frame_metadata&) = default;
};

// Un valore dichiarato dalla ricetta non sostituisce mai un valore diverso gia' presente.
template <class T>
void declare_metadata(std::optional<metadata_value<T>>& field, const T& value,
                      const std::string& name) {
  if (field) {
    if (field->value == value) return;
    throw stop_error(error_category::incompatible, "metadata.declared_conflict",
                     "valore dichiarato diverso da quello presente",
                     {{"campo", name}});
  }
  field = metadata_value<T>{value, {metadata_origin::declared, {}}};
}

// Area visibile importata nel buffer grezzo del decoder (ADR-08.b §2.3).
struct decoder_window {
  std::uint32_t x0 = 0;
  std::uint32_t y0 = 0;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  friend bool operator==(const decoder_window&, const decoder_window&) = default;
};

enum class decoder_linearization { identity, decoder_curve };

// Stato lasciato dal decoder RAW nella rappresentazione di LibRaw (ADR-03.b §b.2): il nero del singolo campione non si
// valuta qui. Invarianti in validate_image (`image.decoder_state`).
struct decoder_state {
  std::uint32_t raw_width = 0;
  std::uint32_t raw_height = 0;
  decoder_window window;
  std::uint32_t black = 0;
  // FC(r, c) dei siti (0, 0), (0, 1), (1, 0), (1, 1) della cella all'origine dell'area visibile.
  std::array<std::uint8_t, 4> cfa_color_index{};
  std::array<std::uint32_t, 4> black_per_color{};
  std::uint32_t black_pattern_rows = 0;
  std::uint32_t black_pattern_cols = 0;
  std::vector<std::uint32_t> black_pattern;
  std::uint32_t white = 0;
  std::optional<std::uint32_t> data_maximum;
  std::optional<std::array<std::int64_t, 4>> linear_max;
  decoder_linearization linearization = decoder_linearization::identity;
  std::optional<std::array<std::uint8_t, 32>> curve_sha256;
  metadata_source source;
  // `source` e' provenienza (origine e keyword): l'uguaglianza non la confronta.
  friend bool operator==(const decoder_state& a, const decoder_state& b) {
    const auto content = [](const decoder_state& s) {
      return std::tie(s.raw_width, s.raw_height, s.window, s.black, s.cfa_color_index, s.black_per_color,
                      s.black_pattern_rows, s.black_pattern_cols, s.black_pattern, s.white, s.data_maximum,
                      s.linear_max, s.linearization, s.curve_sha256);
    };
    return content(a) == content(b);
  }
};

// Origine di un valore che non fa parte del contenuto: l'uguaglianza non la confronta.
struct origin_note {
  std::optional<metadata_origin> value;
  friend bool operator==(const origin_note&, const origin_note&) noexcept { return true; }
};

struct source_encoding {
  int bitpix = 0;
  double bzero = 0.0;
  double bscale = 1.0;
  // Numero di assi dichiarato dal file (1, 2 o 3); 0 = derivato dalla geometria (3 se planes > 1, altrimenti 2).
  std::uint32_t naxis = 0;
  friend bool operator==(const source_encoding&, const source_encoding&) = default;
};

// Limitazioni note di una misura (ADR-03.b §b.13), nell'ordine canonico della tabella. Memorizzate: dalla storia
// dell'immagine, in `image.measure`; derivate: calcolate dai campi dell'immagine, mai memorizzate.
enum class measure_limit {
  history_unknown, calib_omitted_dark_subtraction, calib_omitted_bias_subtraction, calib_omitted_dark_flat_subtraction,
  calib_omitted_flat_bias_subtraction, calib_omitted_flat_normalization, calib_omitted_flat_division,
  calib_offset_known_not_removed, calib_offset_unknown, calib_assumption_h_dprime, calib_assumption_flat_thermal,
  calib_assumption_decoder_black, linearity_nonlinear, linearity_unknown, variance_absent, variance_scope_unknown,
  variance_marginal, variance_fixed_weight_approximation, samples_interpolated
};
const char* to_string(measure_limit limit) noexcept;
// Vero per i cinque codici derivati (linearity.*, variance.absent, variance.scope_unknown, variance.marginal).
bool is_derived_limit(measure_limit limit) noexcept;

// Soli codici memorizzati, in ordine canonico, senza duplicati (violazione: invalid_input image.measure).
struct measure_capability {
  std::vector<measure_limit> limits;
  friend bool operator==(const measure_capability&, const measure_capability&) = default;
};
struct measure_acceptance { std::vector<measure_limit> accepted; };
struct measure_assessment { std::vector<measure_limit> limits; std::vector<measure_limit> blocking; };

struct image {
  image_geometry geometry;
  plane_layout layout = plane_layout::mono;
  sample_type type = sample_type::f32;
  std::optional<pixel_unit> unit;
  std::optional<linearity> lin;
  // Origine di `lin` (header, dichiarata o decoder); vuota se `lin` e' assente.
  origin_note lin_origin;
  // Fase di processo dichiarata (header `ASTRPHAS`, dichiarazione o decoder); assente = non dichiarata, mai indovinata.
  std::optional<metadata_value<process_phase>> phase;
  std::optional<cfa_layout> cfa;
  // Solo nelle immagini prodotte dal decoder RAW; lo scrittore FITS non lo rappresenta (ADR-03.b §b.2).
  std::optional<astro::decoder_state> decoder_state;
  // Assente = capacità non valutata, mai «nessuna limitazione».
  std::optional<astro::measure_capability> measure;
  aligned_buffer samples;
  invalid_mask invalid;
  std::optional<variance_plane> variance;
  frame_metadata metadata;
  std::vector<std::string> header_cards;
  std::vector<std::string> uninterpreted_keywords;
  std::optional<source_encoding> encoding;

  // Accesso tipizzato: il tipo richiesto deve coincidere con `type` (violazione: ASTRO_ASSERT).
  template <class T>
  T* data() noexcept {
    ASTRO_ASSERT(type_matches<T>());
    return samples.as<T>();
  }
  template <class T>
  const T* data() const noexcept {
    ASTRO_ASSERT(type_matches<T>());
    return samples.as<T>();
  }
  template <class T>
  T* variance_data() noexcept {
    ASTRO_ASSERT(type_matches<T>() && variance.has_value());
    return variance->samples.as<T>();
  }
  template <class T>
  const T* variance_data() const noexcept {
    ASTRO_ASSERT(type_matches<T>() && variance.has_value());
    return variance->samples.as<T>();
  }

  friend bool operator==(const image&, const image&) = default;

 private:
  template <class T>
  bool type_matches() const noexcept {
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);
    return std::is_same_v<T, float> == (type == sample_type::f32);
  }
};

// Contenuto scientifico: geometria, layout, tipo, unita', linearita', fase (valore, senza origine), cfa, decoder_state (senza
// `source`), measure, campioni, maschera, varianza e valori dei metadati. Esclusi header_cards, uninterpreted_keywords, encoding e origine
// dei metadati.
bool equivalent_content(const image& a, const image& b);

// Byte di campioni, maschere e varianze allocati da make_image, aligned_buffer e invalid_mask dall'avvio.
std::uint64_t allocated_sample_bytes() noexcept;

// Immagine azzerata, maschera vuota. Geometria nulla: invalid_input image.geometry; overflow: limit_exceeded image.size_overflow.
image make_image(const image_geometry& geometry, sample_type type, plane_layout layout,
                 std::optional<cfa_layout> cfa = std::nullopt);

// Coerenza di buffer, maschere, layout e cfa. Violazione: invalid_input, codice image.*.
void validate_image(const image& img);

// Invarianti di un'immagine prodotta da un operatore: un campione non finito ha almeno un bit acceso;
// un pixel con un bit acceso ha campione e varianza NaN.
bool operator_output_invariants_hold(const image& img);

// `measure->limits` se presente; altrimenti `{history.unknown}` se la fase non è `raw` o è assente, vuoto se è `raw`.
std::vector<measure_limit> history_limits(const image& img);
// Unione, in ordine canonico, di `history_limits` e dei codici derivati veri.
std::vector<measure_limit> measure_limits(const image& img);
// Unione, in ordine canonico e senza duplicati, di `history_limits` degli ingressi e dei codici memorizzati propri.
// Un codice proprio derivato: invalid_input image.measure.
measure_capability merge_measure(const std::vector<const image*>& inputs, const std::vector<measure_limit>& own);
// `blocking` = i codici di `limits` assenti da `accepted`.
measure_assessment assess_measure(const image& img, const measure_acceptance& acceptance);
// `blocking` non vuoto: incompatible `measure.limit_blocked`, codici nel contesto.
void require_measure_ready(const image& img, const measure_acceptance& acceptance);

}  // namespace astro
