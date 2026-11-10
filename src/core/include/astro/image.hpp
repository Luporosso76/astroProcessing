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
// Classe di linearità di un'immagine: `unknown` è un valore esplicito e non è mai trattato come `linear` (ADR-03).
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
constexpr std::uint16_t support_incomplete = 1u << 9;  // ADR-03.b §b.6
constexpr std::uint16_t reserved_mask = 0xFC00;      // bit 10-15
// Nomi dei bit dichiarati, nell'ordine dei bit (bit k = bit_names[k]): i conteggi per bit dei report usano questa
// tabella.
inline constexpr std::array<const char*, 10> bit_names = {
    "source_nonfinite", "source_flagged", "saturated",    "master_invalid",   "q_nonpositive",
    "q_nonfinite",      "q_below_min",    "q_unreliable", "result_nonfinite", "support_incomplete"};
static_assert(static_cast<std::uint16_t>(~reserved_mask) == (1u << bit_names.size()) - 1u,
              "bit_names e reserved_mask devono coprire gli stessi bit");
using bit_counts = std::array<std::uint64_t, bit_names.size()>;
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

// Frazione geometrica coperta di ogni pixel (ADR-03.b §b.5): un campione per pixel e per piano, tipo dei campioni.
// Gli invarianti di valore e l'HDU FITS sono di T-M3A-TYP-07 (M3.g): finché mancano, lo scrittore FITS la rifiuta.
struct coverage_plane {
  aligned_buffer samples;
  friend bool operator==(const coverage_plane&, const coverage_plane&) = default;
};

// `decoder`: valore del decoder RAW; `metadata_source.keyword` è il nome del campo di LibRaw (ADR-03.b §b.1).
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
  // L'origine è provenienza (ADR-05), non contenuto: l'uguaglianza confronta i valori.
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
  // `other.timestamp` di LibRaw: fuso orario non dichiarato, non è un istante UTC e non va in DATE-OBS (ADR-03.b §b.1).
  std::optional<metadata_value<std::int64_t>> capture_time_libraw;
  friend bool operator==(const frame_metadata&, const frame_metadata&) = default;
};

// Un valore dichiarato dalla ricetta non sostituisce mai un valore diverso già presente.
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

// Decoder scelto dall'instradamento (docs/contracts/U13-dng-decoder.md §3).
enum class raw_decoder_kind { libraw, astro_dng };

// Esito dell'instradamento (U13 §3, §8). `reason` per libraw: "not_tiff", "tiff_not_dng" o "dng_libraw_compression";
// vuoto per astro_dng. I tre campi dell'IFD raw (predefiniti del TIFF se il tag manca) sono presenti solo per un DNG con
// esattamente un IFD raw.
struct dng_route_result {
  raw_decoder_kind decoder = raw_decoder_kind::libraw;
  std::string reason;
  std::optional<std::uint32_t> compression;
  std::optional<std::uint32_t> sample_format;
  std::optional<std::uint32_t> bits_per_sample;
  friend bool operator==(const dng_route_result&, const dng_route_result&) = default;
};

// Valore di un tag DNG registrato e non applicato (DefaultCropOrigin, DefaultCropSize): SHORT, LONG e RATIONAL di valore
// intero con denominatore 1, RATIONAL non intero come scritto.
struct dng_rational {
  std::uint32_t num = 0;
  std::uint32_t den = 1;
  friend bool operator==(const dng_rational&, const dng_rational&) = default;
};

// Campi propri del decoder DNG del progetto (U13 §8), presenti solo con `route.decoder` = astro_dng.
struct dng_decoder_fields {
  std::array<std::uint8_t, 4> dng_version{};
  std::array<std::uint8_t, 4> dng_backward_version{};  // scritto, o DNGVersion con gli ultimi due byte a 0
  bool tiles = false;
  std::uint32_t rows_per_strip = 0;  // strisce: valore scritto
  std::uint32_t tile_width = 0;      // tasselli
  std::uint32_t tile_length = 0;     // tasselli
  std::uint64_t segment_count = 0;
  std::uint32_t predictor = 1;
  bool little_endian = true;  // ordine dei byte del file: II
  std::string zlib_runtime_version;  // zlibVersion() della decodifica (`zlib_version` è una macro di zlib.h)
  // Registrati, mai applicati (§4); assenti se il tag manca o è malformato.
  std::optional<std::uint32_t> orientation;
  std::optional<std::array<dng_rational, 2>> default_crop_origin;
  std::optional<std::array<dng_rational, 2>> default_crop_size;
  // Nomi TIFF dei tag di metadato malformati, nell'ordine di lettura: il campo corrispondente resta assente.
  std::vector<std::string> metadata_rejected;
  // Nomi TIFF di OpcodeList1, OpcodeList2, OpcodeList3 presenti nell'IFD raw: mai applicati.
  std::vector<std::string> opcode_lists;
  friend bool operator==(const dng_decoder_fields&, const dng_decoder_fields&) = default;
};

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
  // Limite del motivo del nero: LIBRAW_CBLACK_SIZE = 4104 meno i sei elementi iniziali (ADR-03.b §b.2).
  static constexpr std::uint64_t max_black_pattern = 4098;
  std::uint32_t black_pattern_rows = 0;
  std::uint32_t black_pattern_cols = 0;
  std::vector<std::uint32_t> black_pattern;
  std::uint32_t white = 0;
  std::optional<std::uint32_t> data_maximum;
  std::optional<std::array<std::int64_t, 4>> linear_max;
  decoder_linearization linearization = decoder_linearization::identity;
  std::optional<std::array<std::uint8_t, 32>> curve_sha256;
  // Bit di `process_warnings` di LibRaw dopo `unpack`, tutti fra quelli ammessi da ADR-08.b §2.5; 0 se nessuno.
  std::uint32_t process_warnings = 0;
  // `sizes.raw_inset_crops[2]` ({cleft, ctop, cwidth, cheight}) e `sizes.flip` di LibRaw: registrati, mai applicati (§2.3, §2.9).
  std::array<std::array<std::uint32_t, 4>, 2> raw_inset_crops{};
  std::int32_t flip = 0;
  // Decoder che ha prodotto l'immagine e campi dell'instradamento (U13 §8); `dng` presente solo con astro_dng.
  dng_route_result route;
  std::optional<dng_decoder_fields> dng;
  metadata_source source;
  // `source` è provenienza (origine e keyword): l'uguaglianza non la confronta.
  friend bool operator==(const decoder_state& a, const decoder_state& b) {
    const auto content = [](const decoder_state& s) {
      return std::tie(s.raw_width, s.raw_height, s.window, s.black, s.cfa_color_index, s.black_per_color,
                      s.black_pattern_rows, s.black_pattern_cols, s.black_pattern, s.white, s.data_maximum,
                      s.linear_max, s.linearization, s.curve_sha256, s.process_warnings, s.raw_inset_crops, s.flip,
                      s.route, s.dng);
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
  // Origine di `lin` (header, dichiarata o decoder); vuota se `lin` è assente.
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
  std::optional<coverage_plane> coverage;
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

// Contenuto scientifico: geometria, layout, tipo, unità, linearità, fase (valore, senza origine), cfa, decoder_state (senza
// `source`), measure, campioni, maschera, varianza, coverage e valori dei metadati. Esclusi header_cards, uninterpreted_keywords, encoding e origine
// dei metadati.
bool equivalent_content(const image& a, const image& b);

// Byte di campioni, maschere e varianze allocati da make_image, aligned_buffer e invalid_mask dall'avvio.
std::uint64_t allocated_sample_bytes() noexcept;

// Immagine azzerata, maschera vuota. Geometria nulla: invalid_input image.geometry; overflow: limit_exceeded image.size_overflow.
image make_image(const image_geometry& geometry, sample_type type, plane_layout layout,
                 std::optional<cfa_layout> cfa = std::nullopt);

// Coerenza di buffer, maschere, layout e cfa. Violazione: invalid_input, codice image.*.
void validate_image(const image& img);

// Invarianti di `decoder_state` (ADR-03.b §b.2) che non dipendono dall'immagine: `dng` presente se e solo se il decoder è
// `astro_dng`; finestra non vuota dentro il buffer grezzo, senza overflow; motivo del nero di `rows · cols ≤ 4098` valori,
// mai una sola dimensione nulla; `curve_sha256` presente se e solo se `decoder_curve`; `cfa_color_index` con un sito 0, un
// sito 2 e due siti in {1, 3} (l'unica forma coerente con un motivo di Bayer). Restituisce il campo violato, o nullptr.
const char* decoder_state_violation(const decoder_state& state) noexcept;

// Invarianti di un'immagine prodotta da un operatore: `measure` presente (anche vuoto); un campione non finito ha almeno
// un bit acceso; un pixel con un bit acceso ha campione e varianza NaN.
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
