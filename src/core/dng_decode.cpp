// This product includes DNG technology under license by Adobe.
//
// Decoder DNG del progetto (docs/contracts/U13-dng-decoder.md): instradamento fra LibRaw e decoder del progetto (§3),
// parser dell'IFD raw e stop sui tag (§4, §6), segmenti deflate (§5), immagine, metadati e campi del record (§4, §8).
// Specifica: DNG 1.7.1.0, nel perimetro di §4; ogni valore fuori perimetro è uno stop, mai un passaggio a LibRaw. Ogni
// lettura dal file passa da `reader`, che verifica i limiti del buffer prima di leggere.

#include "dng_internal.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "astro/error.hpp"
#include "astro/memory_pool.hpp"

namespace astro::dng_detail {
namespace {

using context = stop_error::context_list;

// Tipi TIFF 6.0 (p. 15-16) letti dal parser.
constexpr std::uint16_t t_byte = 1, t_ascii = 2, t_short = 3, t_long = 4, t_rational = 5, t_srational = 10, t_ifd = 13;
constexpr std::uint32_t bit(std::uint16_t type) { return 1u << type; }
constexpr std::uint32_t any_count = 0xFFFFFFFFu;

constexpr const char* route_function = "dng_route";
constexpr const char* tags_function = "dng_tags";

[[noreturn]] void data_error(const char* function, const char* message, context ctx) {
  ctx.insert(ctx.begin(), {"funzione", function});
  throw stop_error(error_category::integrity, "raw.data_error", message, std::move(ctx));
}

// Byte per valore dei tipi TIFF 6.0; 0 per un tipo sconosciuto.
std::uint64_t type_bytes(std::uint16_t type) {
  switch (type) {
    case 1: case 2: case 6: case 7: return 1;
    case 3: case 8: return 2;
    case 4: case 9: case 11: case 13: return 4;
    case 5: case 10: case 12: return 8;
    default: return 0;
  }
}

// Voce di una tabella IFD; `field` è la posizione dei 4 byte del valore o dell'offset dei valori.
struct entry {
  std::uint16_t tag = 0;
  std::uint16_t type = 0;
  std::uint32_t count = 0;
  std::uint64_t field = 0;
};

// Voce risolta: i `count` valori stanno tutti nel buffer, da `pos`.
struct field {
  std::uint16_t tag = 0;
  std::uint16_t type = 0;
  std::uint32_t count = 0;
  std::uint64_t pos = 0;
};

struct ifd_table {
  std::uint32_t offset = 0;
  std::vector<entry> entries;
  std::uint32_t next = 0;
};

class reader {
 public:
  reader(std::span<const std::uint8_t> bytes, bool little_endian, const char* function)
      : b_(bytes), le_(little_endian), function_(function) {}

  std::uint64_t size() const { return b_.size(); }

  // Intero senza segno di `n` byte nell'ordine del file. Chi chiama ha già verificato che i byte stiano nel buffer.
  std::uint32_t uint(std::uint64_t pos, unsigned n) const {
    ASTRO_ASSERT(n <= 4 && pos <= b_.size() && n <= b_.size() - pos);
    std::uint32_t v = 0;
    for (unsigned i = 0; i < n; ++i) v |= std::uint32_t{b_[pos + i]} << (8 * (le_ ? i : n - 1 - i));
    return v;
  }

  // Tabella IFD all'offset `offset` (TIFF 6.0 p. 14): conteggio, voci e campo del prossimo IFD dentro il buffer, dopo
  // l'intestazione di 8 byte.
  ifd_table table(std::uint32_t offset) const {
    if (!table_fits(offset))
      data_error(function_, "IFD fuori dal buffer",
                 {{"offset", std::to_string(offset)}, {"byte", std::to_string(b_.size())}});
    const std::uint32_t n = uint(offset, 2);
    if (!table_fits(offset, n))
      data_error(function_, "tabella IFD oltre il buffer",
                 {{"offset", std::to_string(offset)}, {"voci", std::to_string(n)}, {"byte", std::to_string(b_.size())}});
    return parse_table(offset, n);
  }

  // Come `table`, senza stop: assente se la tabella non sta nel buffer (IFD dei metadati, §4).
  std::optional<ifd_table> table_if_inside(std::uint32_t offset) const {
    if (!table_fits(offset) || !table_fits(offset, uint(offset, 2))) return std::nullopt;
    return parse_table(offset, uint(offset, 2));
  }

  // Voce `tag` di `t`, se presente: tipo fra `types` (maschera di `bit`), conteggio in [min_count, max_count], valori
  // non oltre 2^32 byte e dentro il buffer. Un tag ripetuto nella tabella è un errore di struttura.
  std::optional<field> get(const ifd_table& t, std::uint16_t tag, std::uint32_t types, std::uint32_t min_count,
                           std::uint32_t max_count) const {
    lookup found = find_field(t, tag, types, min_count, max_count);
    if (found.problem != nullptr) data_error(function_, found.problem, std::move(found.described));
    return found.value;
  }

  // Come `get`, senza stop: un tag che `get` rifiuterebbe è `malformed` (metadati, §4).
  enum class status { absent, malformed, present };
  std::pair<status, field> get_metadata(const ifd_table& t, std::uint16_t tag, std::uint32_t types,
                                        std::uint32_t min_count, std::uint32_t max_count) const {
    const lookup found = find_field(t, tag, types, min_count, max_count);
    if (found.problem != nullptr) return {status::malformed, field{}};
    if (!found.value) return {status::absent, field{}};
    return {status::present, *found.value};
  }

  // Valore `k` di un tag BYTE, SHORT, LONG o IFD.
  std::uint32_t value(const field& f, std::uint32_t k) const {
    const auto n = static_cast<unsigned>(type_bytes(f.type));
    ASTRO_ASSERT(k < f.count && (n == 1 || n == 2 || n == 4));
    return uint(f.pos + std::uint64_t{k} * n, n);
  }

  // Numeratore e denominatore del valore `k` di un tag RATIONAL o SRATIONAL, come bit del file.
  std::pair<std::uint32_t, std::uint32_t> rational(const field& f, std::uint32_t k) const {
    ASTRO_ASSERT(k < f.count && type_bytes(f.type) == 8);
    const std::uint64_t at = f.pos + 8 * std::uint64_t{k};
    return {uint(at, 4), uint(at + 4, 4)};
  }

 private:
  // Esito della ricerca di un tag: assente, voce valida, oppure il difetto con il contesto dello stop.
  struct lookup {
    std::optional<field> value;
    const char* problem = nullptr;
    context described;
  };

  // Con `n` assente: il conteggio delle voci sta nel buffer; con `n`: tutta la tabella di `n` voci.
  bool table_fits(std::uint32_t offset, std::optional<std::uint32_t> n = std::nullopt) const {
    if (offset < 8 || std::uint64_t{offset} + 2 > b_.size()) return false;
    return !n || std::uint64_t{offset} + 2 + 12 * std::uint64_t{*n} + 4 <= b_.size();
  }

  ifd_table parse_table(std::uint32_t offset, std::uint32_t n) const {
    ifd_table t;
    t.offset = offset;
    t.entries.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      const std::uint64_t at = std::uint64_t{offset} + 2 + 12 * std::uint64_t{i};
      t.entries.push_back(
          {static_cast<std::uint16_t>(uint(at, 2)), static_cast<std::uint16_t>(uint(at + 2, 2)), uint(at + 4, 4), at + 8});
    }
    t.next = uint(std::uint64_t{offset} + 2 + 12 * std::uint64_t{n}, 4);
    return t;
  }

  lookup find_field(const ifd_table& t, std::uint16_t tag, std::uint32_t types, std::uint32_t min_count,
                    std::uint32_t max_count) const {
    const entry* found = nullptr;
    for (const entry& e : t.entries) {
      if (e.tag != tag) continue;
      if (found != nullptr)
        return {std::nullopt, "tag ripetuto nell'IFD", {{"tag", std::to_string(tag)}, {"ifd", std::to_string(t.offset)}}};
      found = &e;
    }
    if (found == nullptr) return {};
    const entry& e = *found;
    context described = {{"tag", std::to_string(tag)}, {"tipo", std::to_string(e.type)},
                         {"conteggio", std::to_string(e.count)}, {"ifd", std::to_string(t.offset)}};
    if (e.type >= 32 || (types & bit(e.type)) == 0) return {std::nullopt, "tipo del tag non ammesso", std::move(described)};
    if (e.count < min_count || e.count > max_count)
      return {std::nullopt, "conteggio del tag non ammesso", std::move(described)};
    const std::uint64_t bytes = std::uint64_t{e.count} * type_bytes(e.type);
    if (bytes > (std::uint64_t{1} << 32)) return {std::nullopt, "valori del tag oltre 2^32 byte", std::move(described)};
    std::uint64_t pos = e.field;
    if (bytes > 4) {
      pos = uint(e.field, 4);
      if (pos > b_.size() || bytes > b_.size() - pos) {
        described.emplace_back("offset", std::to_string(pos));
        return {std::nullopt, "valori del tag oltre il buffer", std::move(described)};
      }
    }
    return {field{tag, e.type, e.count, pos}, nullptr, {}};
  }

  std::span<const std::uint8_t> b_;
  bool le_;
  const char* function_;
};

// Primo valore di un tag SHORT dell'instradamento, o il predefinito del TIFF se il tag manca.
std::uint32_t first_short(const reader& r, const ifd_table& t, std::uint16_t tag, std::uint32_t max_count,
                          std::uint32_t predefined) {
  const std::optional<field> f = r.get(t, tag, bit(t_short), 1, max_count);
  return f ? r.value(*f, 0) : predefined;
}

// NewSubFileType (254, LONG); assente se manca il tag.
std::optional<std::uint32_t> new_subfile_type(const reader& r, const ifd_table& t) {
  const std::optional<field> f = r.get(t, 254, bit(t_long), 1, 1);
  if (!f) return std::nullopt;
  return r.value(*f, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// Tag dell'IFD raw (§4): lettura strutturale, prima di ogni decisione sui valori.
// ---------------------------------------------------------------------------------------------------------------------

struct tag_rule {
  std::uint16_t tag;
  std::uint32_t types;
  std::uint32_t min_count;
  std::uint32_t max_count;
};

// Tipi e conteggi della specifica DNG 1.7.1.0 e del TIFF 6.0. I conteggi che dipendono da altri tag (segmenti,
// CFAPattern, BlackLevel, BlackLevelDelta, WhiteLevel, LinearizationTable) si verificano nella riga del valore.
constexpr std::uint32_t short_or_long = bit(t_short) | bit(t_long);
constexpr tag_rule raw_rules[] = {
    {256, short_or_long, 1, 1},          // ImageWidth
    {257, short_or_long, 1, 1},          // ImageLength
    {258, bit(t_short), 1, any_count},   // BitsPerSample
    {262, bit(t_short), 1, 1},           // PhotometricInterpretation
    {273, short_or_long, 1, any_count},  // StripOffsets
    {277, bit(t_short), 1, 1},           // SamplesPerPixel
    {278, short_or_long, 1, 1},          // RowsPerStrip
    {279, short_or_long, 1, any_count},  // StripByteCounts
    {284, bit(t_short), 1, 1},           // PlanarConfiguration
    {317, bit(t_short), 1, 1},           // Predictor
    {322, short_or_long, 1, 1},          // TileWidth
    {323, short_or_long, 1, 1},          // TileLength
    {324, bit(t_long), 1, any_count},    // TileOffsets
    {325, short_or_long, 1, any_count},  // TileByteCounts
    {339, bit(t_short), 1, any_count},   // SampleFormat
    {33421, bit(t_short), 2, 2},         // CFARepeatPatternDim
    {33422, bit(t_byte), 1, any_count},  // CFAPattern
    {50710, bit(t_byte), 1, any_count},  // CFAPlaneColor
    {50711, bit(t_short), 1, 1},         // CFALayout
    {50712, bit(t_short), 0, any_count},  // LinearizationTable
    {50713, bit(t_short), 2, 2},         // BlackLevelRepeatDim
    {50714, short_or_long | bit(t_rational), 1, any_count},  // BlackLevel
    {50715, bit(t_srational), 1, any_count},  // BlackLevelDeltaH
    {50716, bit(t_srational), 1, any_count},  // BlackLevelDeltaV
    {50717, short_or_long, 1, any_count},     // WhiteLevel
    {50718, bit(t_rational), 2, 2},           // DefaultScale
    {50829, short_or_long, 4, 4},             // ActiveArea
    {50974, short_or_long, 2, 2},             // SubTileBlockSize
    {50975, short_or_long, 1, 1},             // RowInterleaveFactor
    {52547, short_or_long, 1, 1},             // ColumnInterleaveFactor
};
// Tag di IFD0.
constexpr tag_rule ifd0_rules[] = {
    {50706, bit(t_byte), 4, 4},  // DNGVersion
    {50707, bit(t_byte), 4, 4},  // DNGBackwardVersion
};

class raw_tags {
 public:
  raw_tags(const reader& r, const ifd_table& raw, const ifd_table& ifd0) : r_(r) {
    for (const tag_rule& rule : raw_rules)
      if (const auto f = r.get(raw, rule.tag, rule.types, rule.min_count, rule.max_count)) fields_.push_back(*f);
    for (const tag_rule& rule : ifd0_rules)
      if (const auto f = r.get(ifd0, rule.tag, rule.types, rule.min_count, rule.max_count)) fields_.push_back(*f);
  }

  const reader& r() const { return r_; }

  std::optional<field> find(std::uint16_t tag) const {
    for (const field& f : fields_)
      if (f.tag == tag) return f;
    return std::nullopt;
  }
  bool has(std::uint16_t tag) const { return find(tag).has_value(); }

  field required(std::uint16_t tag) const {
    const std::optional<field> f = find(tag);
    if (!f) data_error(tags_function, "tag obbligatorio assente", {{"tag", std::to_string(tag)}});
    return *f;
  }
  // Primo valore del tag, o `predefined` se il tag manca.
  std::uint32_t one(std::uint16_t tag, std::uint32_t predefined) const {
    const std::optional<field> f = find(tag);
    return f ? r_.value(*f, 0) : predefined;
  }

 private:
  const reader& r_;
  std::vector<field> fields_;
};

[[noreturn]] void tag_stop(error_category category, const char* code, const char* message, std::uint16_t tag,
                           context extra = {}) {
  context c = {{"funzione", tags_function}, {"tag", std::to_string(tag)}};
  c.insert(c.end(), extra.begin(), extra.end());
  throw stop_error(category, code, message, std::move(c));
}
[[noreturn]] void tag_data_error(const char* message, std::uint16_t tag, context extra = {}) {
  tag_stop(error_category::integrity, "raw.data_error", message, tag, std::move(extra));
}
[[noreturn]] void tag_unsupported(const char* code, const char* message, std::uint16_t tag, context extra = {}) {
  tag_stop(error_category::unsupported, code, message, tag, std::move(extra));
}

std::string dotted(const std::array<std::uint32_t, 4>& v) {
  return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." + std::to_string(v[3]);
}

// Segmenti dell'IFD raw (§4, riga «Segmenti»): offset e conteggi verificati dentro il buffer.
struct segment_plan {
  bool tiles = false;
  std::uint32_t tile_width = 0;   // tasselli
  std::uint32_t tile_length = 0;  // tasselli
  std::uint32_t rows_per_strip = 0;  // strisce, già limitato a ImageLength
  std::uint32_t rows_per_strip_written = 0;  // strisce, valore del tag (record, §8)
  std::uint64_t count = 0;
  field offsets;
  field byte_counts;
  std::uint64_t buffer_bytes = 0;  // S di §7: uscita del segmento più grande
};

// Valori dei tag nel perimetro di §4, ricavati dopo i controlli.
struct checked_ifd {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  segment_plan segments;
  std::array<std::uint8_t, 4> dng_version{};
  std::array<std::uint8_t, 4> dng_backward_version{};
  std::uint32_t predictor = 1;
  cfa_pattern pattern = cfa_pattern::rggb;
  std::array<std::uint8_t, 4> cell{};  // CFAPattern: colore dei siti (0, 0), (0, 1), (1, 0), (1, 1) all'origine di ActiveArea
  std::uint32_t top = 0, left = 0, bottom = 0, right = 0;  // ActiveArea
  std::uint32_t black_rows = 1, black_cols = 1;
  std::optional<field> black;  // BlackLevel, R · C valori interi verificati; assente: nero 0 in ogni sito
  std::uint32_t white = 0xFFFFFFFFu;
  bool white_written = false;  // WhiteLevel presente nel file
  std::optional<field> linearization;
};

segment_plan check_segments(const raw_tags& t, std::uint32_t width, std::uint32_t height, std::uint64_t max_pixels) {
  const reader& r = t.r();
  const bool strips = t.has(273) || t.has(278) || t.has(279);
  const bool tiles = t.has(322) || t.has(323) || t.has(324) || t.has(325);
  if (strips && tiles) tag_data_error("strisce e tasselli insieme", 273, {{"tag_tasselli", "324"}});
  if (!strips && !tiles) tag_data_error("nessun tag di strisce né di tasselli", 273, {{"tag_tasselli", "324"}});
  segment_plan p;
  p.tiles = tiles;
  std::uint64_t segment_pixels = 0;
  std::uint16_t offsets_tag = 273, counts_tag = 279, size_tag = 278;
  if (strips) {
    for (const std::uint16_t tag : {273, 278, 279})
      if (!t.has(tag)) tag_data_error("insieme delle strisce incompleto", tag);
    const std::uint32_t rows = t.one(278, 0);
    if (rows == 0) tag_data_error("RowsPerStrip nullo", 278);
    p.rows_per_strip = std::min(rows, height);
    p.rows_per_strip_written = rows;
    p.count = (std::uint64_t{height} + rows - 1) / rows;
    segment_pixels = std::uint64_t{width} * p.rows_per_strip;
  } else {
    for (const std::uint16_t tag : {322, 323, 324, 325})
      if (!t.has(tag)) tag_data_error("insieme dei tasselli incompleto", tag);
    offsets_tag = 324;
    counts_tag = 325;
    size_tag = 322;
    p.tile_width = t.one(322, 0);
    p.tile_length = t.one(323, 0);
    if (p.tile_width == 0) tag_data_error("TileWidth nullo", 322);
    if (p.tile_length == 0) tag_data_error("TileLength nullo", 323);
    // TIFF 6.0 p. 67: TileWidth e TileLength multipli di 16.
    if (p.tile_width % 16 != 0)
      tag_unsupported("raw.dng_nonconformant", "TileWidth non multiplo di 16", 322,
                      {{"valore", std::to_string(p.tile_width)}});
    if (p.tile_length % 16 != 0)
      tag_unsupported("raw.dng_nonconformant", "TileLength non multiplo di 16", 323,
                      {{"valore", std::to_string(p.tile_length)}});
    const std::uint64_t across = (std::uint64_t{width} + p.tile_width - 1) / p.tile_width;
    const std::uint64_t down = (std::uint64_t{height} + p.tile_length - 1) / p.tile_length;
    p.count = across * down;
    segment_pixels = std::uint64_t{p.tile_width} * p.tile_length;
  }
  p.offsets = *t.find(offsets_tag);
  p.byte_counts = *t.find(counts_tag);
  for (const field* f : {&p.offsets, &p.byte_counts})
    if (f->count != p.count)
      tag_data_error("numero di segmenti diverso da quello delle dimensioni", f->tag,
                     {{"conteggio", std::to_string(f->count)}, {"atteso", std::to_string(p.count)}});
  for (std::uint32_t k = 0; k < p.count; ++k) {
    const std::uint64_t offset = r.value(p.offsets, k);
    const std::uint64_t bytes = r.value(p.byte_counts, k);
    if (offset > r.size())
      tag_data_error("segmento oltre il buffer", offsets_tag,
                     {{"segmento", std::to_string(k)}, {"offset", std::to_string(offset)}});
    if (bytes > r.size() - offset)
      tag_data_error("segmento oltre il buffer", counts_tag,
                     {{"segmento", std::to_string(k)}, {"offset", std::to_string(offset)},
                      {"byte_segmento", std::to_string(bytes)}});
  }
  if (segment_pixels > max_pixels)
    tag_stop(error_category::limit_exceeded, "raw.max_pixels", "segmento oltre il limite di pixel", size_tag,
             {{"pixel", std::to_string(segment_pixels)}, {"limite", std::to_string(max_pixels)}});
  p.buffer_bytes = 4 * segment_pixels;
  return p;
}

// Cella CFA 2×2 (0 rosso, 1 verde, 2 blu, riga per riga) come pattern Bayer di ADR-03.
std::optional<cfa_pattern> bayer_cell(const std::array<std::uint32_t, 4>& cell) {
  static constexpr std::pair<cfa_pattern, std::array<std::uint32_t, 4>> cells[] = {{cfa_pattern::rggb, {0, 1, 1, 2}},
                                                                                {cfa_pattern::bggr, {2, 1, 1, 0}},
                                                                                {cfa_pattern::grbg, {1, 0, 2, 1}},
                                                                                {cfa_pattern::gbrg, {1, 2, 0, 1}}};
  for (const auto& [pattern, values] : cells)
    if (values == cell) return pattern;
  return std::nullopt;
}

// Valori SRATIONAL di BlackLevelDeltaH o BlackLevelDeltaV (§4): `expected` valori, denominatori non nulli, tutti nulli.
void check_black_delta(const raw_tags& t, std::uint16_t tag, std::uint32_t expected) {
  const std::optional<field> f = t.find(tag);
  if (!f) return;
  if (f->count != expected)
    tag_data_error("conteggio di BlackLevelDelta diverso dalla dimensione di ActiveArea", tag,
                   {{"conteggio", std::to_string(f->count)}, {"atteso", std::to_string(expected)}});
  for (std::uint32_t k = 0; k < f->count; ++k)
    if (t.r().rational(*f, k).second == 0) tag_data_error("denominatore nullo", tag, {{"indice", std::to_string(k)}});
  for (std::uint32_t k = 0; k < f->count; ++k)
    if (t.r().rational(*f, k).first != 0)
      tag_unsupported("raw.black_level", "BlackLevelDelta non nullo", tag, {{"indice", std::to_string(k)}});
}

// §6 e §4, nell'ordine del contratto: Compression 52546, poi le righe della tabella; il primo stop vale.
checked_ifd check_tags(const raw_tags& t, const route_detail& route, std::uint64_t max_pixels) {
  const reader& r = t.r();
  checked_ifd d;

  // §6: nessun DNG JPEG XL nel perimetro; si ferma sui tag, prima dei segmenti.
  if (route.result.compression == std::optional<std::uint32_t>(52546)) {
    const std::uint32_t photometric = r.value(t.required(262), 0);
    const std::uint32_t samples = t.one(277, 1);
    const context c = {{"photometric_interpretation", std::to_string(photometric)},
                       {"samples_per_pixel", std::to_string(samples)}};
    // DNG 1.7.1.0 p. 21: interpretazioni ammesse per JPEG XL; 32803 (CFA) non è fra queste.
    static constexpr std::uint32_t jxl_photometric[] = {0, 1, 2, 4, 34892, 51177, 52527};
    if (std::find(std::begin(jxl_photometric), std::end(jxl_photometric), photometric) != std::end(jxl_photometric))
      tag_unsupported("raw.not_cfa", "DNG JPEG XL non CFA, fuori dal prodotto", 259, c);
    tag_unsupported("raw.dng_nonconformant", "DNG JPEG XL con interpretazione fotometrica non ammessa", 259, c);
  }

  // DNGBackwardVersion; predefinito DNGVersion con gli ultimi due byte a 0.
  {
    const field version = t.required(50706);
    std::array<std::uint32_t, 4> backward{r.value(version, 0), r.value(version, 1), 0, 0};
    if (const std::optional<field> b = t.find(50707))
      for (std::uint32_t k = 0; k < 4; ++k) backward[k] = r.value(*b, k);
    if (backward > std::array<std::uint32_t, 4>{1, 7, 1, 0})
      tag_unsupported("raw.dng_version", "DNGBackwardVersion oltre 1.7.1.0", 50707,
                      {{"dng_backward_version", dotted(backward)}});
    for (std::uint32_t k = 0; k < 4; ++k) {
      d.dng_version[k] = static_cast<std::uint8_t>(r.value(version, k));
      d.dng_backward_version[k] = static_cast<std::uint8_t>(backward[k]);
    }
  }

  // ImageWidth, ImageLength.
  d.width = r.value(t.required(256), 0);
  d.height = r.value(t.required(257), 0);
  if (d.width == 0) tag_data_error("ImageWidth nullo", 256);
  if (d.height == 0) tag_data_error("ImageLength nullo", 257);
  if (const std::uint64_t pixels = std::uint64_t{d.width} * d.height; pixels > max_pixels)
    throw stop_error(error_category::limit_exceeded, "raw.max_pixels", "dimensioni dichiarate oltre il limite",
                     {{"funzione", tags_function}, {"pixel", std::to_string(pixels)},
                      {"limite", std::to_string(max_pixels)}});

  // PhotometricInterpretation, SamplesPerPixel.
  {
    const std::uint32_t photometric = r.value(t.required(262), 0);
    const std::uint32_t samples = t.one(277, 1);
    if (photometric != 32803 || samples != 1)
      tag_unsupported("raw.not_cfa", "dati raw non CFA a un campione per pixel", 262,
                      {{"photometric_interpretation", std::to_string(photometric)},
                       {"samples_per_pixel", std::to_string(samples)}});
  }

  // BitsPerSample (un valore: SamplesPerPixel è 1).
  for (const std::uint16_t tag : {258, 339})
    if (const std::optional<field> f = t.find(tag); f && f->count != 1)
      tag_data_error("conteggio diverso da SamplesPerPixel", tag, {{"conteggio", std::to_string(f->count)}});
  if (const std::uint32_t bits = t.one(258, 1); bits != 32)
    tag_unsupported("raw.dng_nonconformant", "deflate intero con BitsPerSample diverso da 32", 258,
                    {{"valore", std::to_string(bits)}});

  // Predictor (DNG 1.7.1.0 p. 22, campioni interi).
  d.predictor = t.one(317, 1);
  if (d.predictor != 1 && d.predictor != 2 && d.predictor != 34892 && d.predictor != 34893)
    tag_unsupported("raw.dng_nonconformant", "Predictor non ammesso per campioni interi", 317,
                    {{"valore", std::to_string(d.predictor)}});

  // PlanarConfiguration.
  if (const std::uint32_t planar = t.one(284, 1); planar != 1)
    tag_unsupported("raw.dng_layout", "PlanarConfiguration diversa da 1", 284, {{"valore", std::to_string(planar)}});

  d.segments = check_segments(t, d.width, d.height, max_pixels);

  // RowInterleaveFactor, ColumnInterleaveFactor, SubTileBlockSize.
  for (const std::uint16_t tag : {50975, 52547})
    if (const std::uint32_t v = t.one(tag, 1); v != 1)
      tag_unsupported("raw.dng_layout", "interlacciamento non supportato", tag, {{"valore", std::to_string(v)}});
  if (const std::optional<field> f = t.find(50974); f && (r.value(*f, 0) != 1 || r.value(*f, 1) != 1))
    tag_unsupported("raw.dng_layout", "SubTileBlockSize diverso da (1, 1)", 50974,
                    {{"valore", std::to_string(r.value(*f, 0)) + "," + std::to_string(r.value(*f, 1))}});

  // CFARepeatPatternDim, CFAPattern, CFAPlaneColor, CFALayout.
  {
    const field dim = t.required(33421);
    if (r.value(dim, 0) != 2 || r.value(dim, 1) != 2)
      tag_unsupported("raw.cfa_pattern", "CFARepeatPatternDim diverso da 2 × 2", 33421,
                      {{"valore", std::to_string(r.value(dim, 0)) + "x" + std::to_string(r.value(dim, 1))}});
    const field pattern = t.required(33422);
    if (pattern.count != 4)
      tag_data_error("conteggio di CFAPattern diverso da CFARepeatPatternDim", 33422,
                     {{"conteggio", std::to_string(pattern.count)}});
    const std::array<std::uint32_t, 4> cell{r.value(pattern, 0), r.value(pattern, 1), r.value(pattern, 2),
                                            r.value(pattern, 3)};
    const std::optional<cfa_pattern> bayer = bayer_cell(cell);
    if (!bayer) tag_unsupported("raw.cfa_pattern", "cella CFA non Bayer 2 × 2", 33422, {{"cella", dotted(cell)}});
    d.pattern = *bayer;
    for (std::size_t k = 0; k < 4; ++k) d.cell[k] = static_cast<std::uint8_t>(cell[k]);
    if (const std::optional<field> planes = t.find(50710);
        planes && (planes->count != 3 || r.value(*planes, 0) != 0 || r.value(*planes, 1) != 1 ||
                   r.value(*planes, 2) != 2))
      tag_unsupported("raw.cfa_pattern", "CFAPlaneColor diverso da (0, 1, 2)", 50710);
    if (const std::uint32_t layout = t.one(50711, 1); layout != 1)
      tag_unsupported("raw.dng_layout", "CFALayout diverso da 1", 50711, {{"valore", std::to_string(layout)}});
  }

  // ActiveArea (top, left, bottom, right); predefinito tutta l'immagine.
  d.bottom = d.height;
  d.right = d.width;
  if (const std::optional<field> area = t.find(50829)) {
    d.top = r.value(*area, 0);
    d.left = r.value(*area, 1);
    d.bottom = r.value(*area, 2);
    d.right = r.value(*area, 3);
    if (d.top >= d.bottom || d.bottom > d.height || d.left >= d.right || d.right > d.width)
      tag_data_error("ActiveArea fuori dall'immagine", 50829,
                     {{"valore", std::to_string(d.top) + "," + std::to_string(d.left) + "," + std::to_string(d.bottom) +
                                     "," + std::to_string(d.right)}});
  }

  // DefaultScale: due valori uguali.
  if (const std::optional<field> scale = t.find(50718)) {
    const auto [hn, hd] = r.rational(*scale, 0);
    const auto [vn, vd] = r.rational(*scale, 1);
    if (hd == 0 || vd == 0) tag_data_error("denominatore nullo", 50718);
    if (std::uint64_t{hn} * vd != std::uint64_t{vn} * hd)
      tag_unsupported("raw.geometry", "DefaultScale con valori diversi", 50718);
  }

  // BlackLevelRepeatDim, BlackLevel: R · C valori interi.
  if (const std::optional<field> dim = t.find(50713)) {
    d.black_rows = r.value(*dim, 0);
    d.black_cols = r.value(*dim, 1);
    if (d.black_rows == 0 || d.black_cols == 0) tag_data_error("BlackLevelRepeatDim nullo", 50713);
  }
  const std::uint64_t black_count = std::uint64_t{d.black_rows} * d.black_cols;
  if (const std::optional<field> black = t.find(50714)) {
    if (black->count != black_count)
      tag_data_error("conteggio di BlackLevel diverso da BlackLevelRepeatDim", 50714,
                     {{"conteggio", std::to_string(black->count)}, {"atteso", std::to_string(black_count)}});
    if (black->type == t_rational) {
      for (std::uint32_t k = 0; k < black->count; ++k)
        if (r.rational(*black, k).second == 0) tag_data_error("denominatore nullo", 50714, {{"indice", std::to_string(k)}});
      for (std::uint32_t k = 0; k < black->count; ++k) {
        const auto [num, den] = r.rational(*black, k);
        if (num % den != 0)
          tag_unsupported("raw.black_level", "BlackLevel non intero", 50714,
                          {{"indice", std::to_string(k)}, {"valore", std::to_string(num) + "/" + std::to_string(den)}});
      }
    }
    // Il motivo del nero dell'immagine ha al massimo decoder_state::max_black_pattern valori (ADR-03.b §b.2).
    if (black_count > 1 && black_count > decoder_state::max_black_pattern)
      tag_unsupported("raw.black_level", "motivo del nero oltre il massimo rappresentabile", 50714,
                      {{"conteggio", std::to_string(black_count)},
                       {"limite", std::to_string(decoder_state::max_black_pattern)}});
    d.black = black;
  }

  // BlackLevelDeltaH (conteggio: larghezza di ActiveArea), BlackLevelDeltaV (altezza).
  check_black_delta(t, 50715, d.right - d.left);
  check_black_delta(t, 50716, d.bottom - d.top);

  // WhiteLevel: un valore.
  if (const std::optional<field> white = t.find(50717)) {
    if (white->count != 1) tag_data_error("WhiteLevel con più valori", 50717, {{"conteggio", std::to_string(white->count)}});
    d.white = r.value(*white, 0);
    d.white_written = true;
  }

  // LinearizationTable: SHORT, da 1 a 65 536 voci.
  if (const std::optional<field> table = t.find(50712)) {
    if (table->count == 0 || table->count > 65536)
      tag_data_error("LinearizationTable con un numero di voci non ammesso", 50712,
                     {{"conteggio", std::to_string(table->count)}});
    d.linearization = table;
  }
  return d;
}

// ---------------------------------------------------------------------------------------------------------------------
// Metadati (§4): un tag assente lascia il campo assente; un tag malformato lo lascia assente e il suo nome entra in
// `metadata_rejected`. Nessun metadato ferma la decodifica.
// ---------------------------------------------------------------------------------------------------------------------

// Giorni dal 1970-01-01 della data civile proleptica gregoriana (algoritmo `days_from_civil` di H. Hinnant).
std::int64_t days_from_civil(std::int64_t y, std::uint32_t m, std::uint32_t d) {
  y -= m <= 2 ? 1 : 0;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const auto yoe = static_cast<std::uint32_t>(y - era * 400);
  const std::uint32_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const std::uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

// `AAAA:MM:GG HH:MM:SS` (EXIF) come `timegm` dei campi civili scritti, senza fuso; assente se il testo non è in quel
// formato o la data non è nel calendario.
std::optional<std::int64_t> civil_seconds(const std::string& text) {
  static constexpr char shape[] = "dddd:dd:dd dd:dd:dd";
  if (text.size() != sizeof(shape) - 1) return std::nullopt;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const bool digit = text[i] >= '0' && text[i] <= '9';
    if (shape[i] == 'd' ? !digit : text[i] != shape[i]) return std::nullopt;
  }
  const auto num = [&text](std::size_t at, std::size_t n) {
    std::uint32_t v = 0;
    for (std::size_t i = at; i < at + n; ++i) v = v * 10 + static_cast<std::uint32_t>(text[i] - '0');
    return v;
  };
  const std::uint32_t year = num(0, 4), month = num(5, 2), day = num(8, 2);
  const std::uint32_t hour = num(11, 2), minute = num(14, 2), second = num(17, 2);
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  static constexpr std::uint32_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return std::nullopt;
  const std::uint32_t days = month_days[month - 1] + (month == 2 && leap ? 1 : 0);
  if (day < 1 || day > days || hour > 23 || minute > 59 || second > 59) return std::nullopt;
  return days_from_civil(year, month, day) * 86400 + std::int64_t{hour} * 3600 + std::int64_t{minute} * 60 + second;
}

class metadata_reader {
 public:
  metadata_reader(const reader& r, std::vector<std::string>& rejected) : r_(r), rejected_(rejected) {}

  // Voce valida del tag, o assente; malformata: assente e `name` in `metadata_rejected`.
  std::optional<field> get(const ifd_table& t, std::uint16_t tag, const char* name, std::uint32_t types,
                           std::uint32_t min_count, std::uint32_t max_count) const {
    const auto [state, f] = r_.get_metadata(t, tag, types, min_count, max_count);
    switch (state) {
      case reader::status::absent: return std::nullopt;
      case reader::status::malformed: reject(name); return std::nullopt;
      case reader::status::present: return f;
    }
    std::abort();
  }

  // Testo ASCII del tag fino al primo NUL.
  std::optional<std::string> text(const ifd_table& t, std::uint16_t tag, const char* name) const {
    const std::optional<field> f = get(t, tag, name, bit(t_ascii), 1, any_count);
    if (!f) return std::nullopt;
    std::string out;
    for (std::uint32_t k = 0; k < f->count; ++k) {
      const auto c = static_cast<char>(r_.value(*f, k));
      if (c == '\0') break;
      out += c;
    }
    return out;
  }

  // Due valori SHORT, LONG o RATIONAL; RATIONAL con denominatore nullo: malformato. Un RATIONAL di valore intero si
  // registra come {valore, 1}, come il record lo scrive (intero), così lo stato si ricostruisce identico dal record;
  // uno non intero resta come scritto, non ridotto.
  std::optional<std::array<dng_rational, 2>> pair(const ifd_table& t, std::uint16_t tag, const char* name) const {
    const std::optional<field> f = get(t, tag, name, short_or_long | bit(t_rational), 2, 2);
    if (!f) return std::nullopt;
    std::array<dng_rational, 2> out{};
    for (std::uint32_t k = 0; k < 2; ++k) {
      if (f->type == t_rational) {
        const auto [num, den] = r_.rational(*f, k);
        if (den == 0) {
          reject(name);
          return std::nullopt;
        }
        out[k] = num % den == 0 ? dng_rational{num / den, 1} : dng_rational{num, den};
      } else {
        out[k] = {r_.value(*f, k), 1};
      }
    }
    return out;
  }

  void reject(const char* name) const { rejected_.emplace_back(name); }

 private:
  const reader& r_;
  std::vector<std::string>& rejected_;
};

metadata_source from_tag(const char* name) { return {metadata_origin::decoder, name}; }

// Make, Model, Orientation ed ExifIFD in IFD0; DefaultCropOrigin, DefaultCropSize e OpcodeList1-3 nell'IFD raw.
void read_metadata(const reader& r, const ifd_table& ifd0, const ifd_table& raw, frame_metadata& m,
                   dng_decoder_fields& fields) {
  const metadata_reader mr(r, fields.metadata_rejected);
  const std::string make = mr.text(ifd0, 271, "Make").value_or(std::string());
  const std::string model = mr.text(ifd0, 272, "Model").value_or(std::string());
  // Stessa unione del percorso LibRaw: un solo valore non vuoto resta da solo, senza spazio.
  if (!make.empty() || !model.empty())
    m.camera = metadata_value<std::string>{make.empty() ? model : model.empty() ? make : make + " " + model,
                                           from_tag("Make Model")};
  if (const std::optional<field> f = mr.get(ifd0, 274, "Orientation", bit(t_short), 1, 1))
    fields.orientation = r.value(*f, 0);

  if (const std::optional<field> pointer = mr.get(ifd0, 34665, "ExifIFD", bit(t_long) | bit(t_ifd), 1, 1)) {
    const std::optional<ifd_table> exif = r.table_if_inside(r.value(*pointer, 0));
    if (!exif) {
      mr.reject("ExifIFD");
    } else {
      if (const std::optional<field> f = mr.get(*exif, 33434, "ExposureTime", bit(t_rational), 1, 1)) {
        const auto [num, den] = r.rational(*f, 0);
        if (den == 0) {
          mr.reject("ExposureTime");
        } else {
          m.exposure_s = metadata_value<double>{static_cast<double>(num) / den, from_tag("ExposureTime")};
        }
      }
      // EXIF 2.3: conteggio qualsiasi; il valore di sensibilità è il primo.
      if (const std::optional<field> f = mr.get(*exif, 34855, "PhotographicSensitivity", bit(t_short), 1, any_count))
        m.gain = metadata_value<double>{static_cast<double>(r.value(*f, 0)), from_tag("PhotographicSensitivity")};
      if (const std::optional<std::string> text = mr.text(*exif, 36867, "DateTimeOriginal")) {
        if (const std::optional<std::int64_t> t = civil_seconds(*text)) {
          m.capture_time_libraw = metadata_value<std::int64_t>{*t, from_tag("DateTimeOriginal")};
        } else {
          mr.reject("DateTimeOriginal");
        }
      }
    }
  }

  fields.default_crop_origin = mr.pair(raw, 50719, "DefaultCropOrigin");
  fields.default_crop_size = mr.pair(raw, 50720, "DefaultCropSize");
  static constexpr std::pair<std::uint16_t, const char*> opcode_tags[] = {
      {51008, "OpcodeList1"}, {51009, "OpcodeList2"}, {51022, "OpcodeList3"}};
  for (const auto& [tag, name] : opcode_tags)
    if (std::any_of(raw.entries.begin(), raw.entries.end(), [tag = tag](const entry& e) { return e.tag == tag; }))
      fields.opcode_lists.emplace_back(name);
}

// ---------------------------------------------------------------------------------------------------------------------
// Segmenti deflate (§5) e immagine (§4).
// ---------------------------------------------------------------------------------------------------------------------

constexpr std::uint64_t zlib_cap = 65536;

// Gestore di memoria di zlib per un flusso (§5): conta i byte richiesti e rifiuta oltre `limit`. Ogni blocco porta in
// testa la propria dimensione, letta da `zlib_free`.
struct zlib_memory {
  std::uint64_t limit = 0;
  std::uint64_t held = 0;
  std::uint64_t peak = 0;
  bool refused = false;
  const std::function<bool(std::size_t)>* system_refuses = nullptr;  // gancio dei test, se montato
};
constexpr std::size_t zlib_header = alignof(std::max_align_t);
static_assert(zlib_header >= sizeof(std::uint64_t));

voidpf zlib_alloc(voidpf opaque, uInt items, uInt size) {
  zlib_memory& m = *static_cast<zlib_memory*>(opaque);
  const std::uint64_t bytes = std::uint64_t{items} * size;
  if (bytes > m.limit - m.held) {
    m.refused = true;
    return Z_NULL;
  }
  if (m.system_refuses != nullptr && (*m.system_refuses)(static_cast<std::size_t>(bytes))) return Z_NULL;
  void* block = std::malloc(static_cast<std::size_t>(bytes) + zlib_header);
  if (block == nullptr) return Z_NULL;
  std::memcpy(block, &bytes, sizeof bytes);
  m.held += bytes;
  m.peak = std::max(m.peak, m.held);
  return static_cast<std::uint8_t*>(block) + zlib_header;
}

void zlib_free(voidpf opaque, voidpf address) {
  zlib_memory& m = *static_cast<zlib_memory*>(opaque);
  std::uint8_t* block = static_cast<std::uint8_t*>(address) - zlib_header;
  std::uint64_t bytes = 0;
  std::memcpy(&bytes, block, sizeof bytes);
  m.held -= bytes;
  std::free(block);
}

// Esito di zlib → stop di §5: memoria rifiutata dal tetto (Z_MEM_ERROR) limit_exceeded raw.too_big, ogni altro esito
// integrity raw.data_error. Z_MEM_ERROR senza rifiuto del tetto è memoria negata dal sistema: stesso stop, con
// `rifiuto` = sistema nel contesto.
[[noreturn]] void zlib_stop(const char* function, int rc, const char* msg, std::uint64_t segment, const zlib_memory& mem,
                            context extra = {}) {
  context c = {{"funzione", function}, {"codice", std::to_string(rc)}, {"msg", msg != nullptr ? msg : ""},
               {"segmento", std::to_string(segment)}};
  c.insert(c.end(), extra.begin(), extra.end());
  if (rc == Z_MEM_ERROR) {
    c.emplace_back("tetto", std::to_string(mem.limit));
    c.emplace_back("rifiuto", mem.refused ? "tetto" : "sistema");
    throw stop_error(error_category::limit_exceeded, "raw.too_big",
                     mem.refused ? "memoria di zlib oltre il tetto" : "memoria di zlib negata dal sistema", std::move(c));
  }
  throw stop_error(error_category::integrity, "raw.data_error", "flusso deflate del segmento non valido", std::move(c));
}

struct inflate_end {
  z_stream& z;
  ~inflate_end() { inflateEnd(&z); }
};

std::uint32_t load_u32(const std::uint8_t* p, bool little_endian) {
  return little_endian ? std::uint32_t{p[0]} | std::uint32_t{p[1]} << 8 | std::uint32_t{p[2]} << 16 |
                             std::uint32_t{p[3]} << 24
                       : std::uint32_t{p[3]} | std::uint32_t{p[2]} << 8 | std::uint32_t{p[1]} << 16 |
                             std::uint32_t{p[0]} << 24;
}

// Distanza del campione di riferimento dei predittori orizzontali (DNG 1.7.1.0 p. 22); 0 per Predictor 1.
std::uint32_t predictor_step(std::uint32_t predictor) {
  switch (predictor) {
    case 1: return 0;
    case 2: return 1;
    case 34892: return 2;
    case 34893: return 4;
    default: std::abort();  // check_tags ammette solo i quattro valori
  }
}

image decode_checked(std::span<const std::uint8_t> bytes, const route_detail& route, const raw_decode_hooks& hooks,
                     const raw_decode_context& ctx) {
  const reader r(bytes, route.little_endian, tags_function);
  const ifd_table raw = r.table(route.raw_ifd);
  const ifd_table ifd0 = route.ifd0 == route.raw_ifd ? raw : r.table(route.ifd0);
  const raw_tags tags(r, raw, ifd0);
  const checked_ifd d = check_tags(tags, route, ctx.max_pixels);
  const segment_plan& seg = d.segments;
  const std::uint32_t w = d.right - d.left, h = d.bottom - d.top;

  // §7: campioni f32 e maschera dell'area importata, buffer del segmento, tetto di zlib; prima del primo segmento.
  const std::uint64_t needed = 6 * std::uint64_t{w} * h + seg.buffer_bytes + zlib_cap;
  reservation held;
  if (ctx.pool != nullptr) {
    const std::uint64_t held_by_caller = ctx.held_bytes.value_or(bytes.size());
    if (held_by_caller + needed > ctx.pool->budget_bytes())
      throw stop_error(error_category::budget, "sched.job_exceeds_budget", "budget sotto la prenotazione del decoder",
                       {{"minimo", std::to_string(held_by_caller + needed)},
                        {"file", std::to_string(bytes.size())},
                        {"budget", std::to_string(ctx.pool->budget_bytes())}});
    held = ctx.pool->reserve(needed, "raw_decode", ctx.cancel);
  }

  // §4, p. 99: con la tabella il campione è la voce min(v, N − 1).
  std::vector<std::uint16_t> table;
  if (d.linearization) {
    table.resize(d.linearization->count);
    for (std::uint32_t k = 0; k < d.linearization->count; ++k) table[k] = static_cast<std::uint16_t>(r.value(*d.linearization, k));
  }

  image img = make_image(image_geometry(w, h), sample_type::f32, plane_layout::cfa, cfa_layout{d.pattern, 0, 0});
  float* px = img.data<float>();
  const std::uint32_t step = predictor_step(d.predictor);
  const std::uint64_t across = seg.tiles ? (std::uint64_t{d.width} + seg.tile_width - 1) / seg.tile_width : 1;
  const std::uint32_t row_width = seg.tiles ? seg.tile_width : d.width;

  // §5: un flusso zlib completo per segmento, un solo inflate(Z_FINISH) con l'uscita della dimensione esatta; i segmenti
  // si decodificano uno alla volta nello stesso buffer.
  std::vector<std::uint8_t> out(static_cast<std::size_t>(seg.buffer_bytes));
  for (std::uint32_t k = 0; k < seg.count; ++k) {
    if (ctx.cancel != nullptr && ctx.cancel->requested())
      throw stop_error(error_category::cancelled, "raw.cancelled", "decodifica annullata",
                       {{"segmento", std::to_string(k)}});
    // Origine del segmento nell'immagine memorizzata e righe decodificate (nei tasselli di bordo anche il riempimento).
    std::uint64_t row0 = 0, col0 = 0, rows = 0;
    if (seg.tiles) {
      row0 = (k / across) * seg.tile_length;
      col0 = (k % across) * seg.tile_width;
      rows = seg.tile_length;
    } else {
      row0 = std::uint64_t{k} * seg.rows_per_strip;
      rows = std::min<std::uint64_t>(seg.rows_per_strip, d.height - row0);
    }
    const std::uint64_t expected = 4 * std::uint64_t{row_width} * rows;
    // `avail_out` di zlib è un uInt: un `max_pixels` del contesto oltre 2^30 è un errore di configurazione.
    ASTRO_ASSERT(expected <= std::numeric_limits<uInt>::max());

    zlib_memory mem;
    mem.limit = hooks.zlib_memory_limit ? hooks.zlib_memory_limit(zlib_cap) : zlib_cap;
    if (hooks.zlib_system_refuses) mem.system_refuses = &hooks.zlib_system_refuses;
    z_stream z{};
    z.zalloc = zlib_alloc;
    z.zfree = zlib_free;
    z.opaque = &mem;
    if (const int rc = inflateInit(&z); rc != Z_OK) zlib_stop("inflateInit", rc, z.msg, k, mem);
    const inflate_end end{z};
    // zlib non scrive nell'ingresso: `next_in` non è const solo per la firma della libreria.
    z.next_in = const_cast<Bytef*>(bytes.data() + r.value(seg.offsets, k));
    z.avail_in = static_cast<uInt>(r.value(seg.byte_counts, k));
    z.next_out = out.data();
    z.avail_out = static_cast<uInt>(expected);
    int rc = inflate(&z, Z_FINISH);
    if (hooks.inflate_return) rc = hooks.inflate_return(k, rc);
    if (rc != Z_STREAM_END || z.total_out != expected)
      zlib_stop("inflate", rc, z.msg, k, mem,
                {{"uscita", std::to_string(z.total_out)}, {"attesa", std::to_string(expected)}});

    // Predittore riga per riga del segmento, modulo 2^32 (H1), sul posto: ogni campione già ricostruito si riscrive
    // nell'ordine della macchina. Righe e colonne fuori da ActiveArea (riempimento compreso) non si importano.
    for (std::uint64_t i = 0; i < rows; ++i) {
      const std::uint64_t y = row0 + i;
      if (y < d.top || y >= d.bottom) continue;
      std::uint8_t* line = out.data() + 4 * std::uint64_t{row_width} * i;
      const std::uint64_t col_end = std::min<std::uint64_t>(row_width, d.right - std::min<std::uint64_t>(col0, d.right));
      for (std::uint64_t c = 0; c < col_end; ++c) {
        std::uint32_t v = load_u32(line + 4 * c, route.little_endian);
        if (step != 0 && c >= step) {
          std::uint32_t left = 0;
          std::memcpy(&left, line + 4 * (c - step), 4);
          v += left;
        }
        std::memcpy(line + 4 * c, &v, 4);
        const std::uint64_t x = col0 + c;
        if (x < d.left) continue;
        std::uint32_t sample = v;
        if (!table.empty()) {
          sample = table[std::min<std::uint64_t>(v, table.size() - 1)];
        } else if (v > (std::uint32_t{1} << 24)) {
          throw stop_error(error_category::unsupported, "raw.sample_range",
                           "campione importato oltre 2^24 senza LinearizationTable: non esatto in f32",
                           {{"funzione", "dng_decode"}, {"segmento", std::to_string(k)}, {"riga", std::to_string(y)},
                            {"colonna", std::to_string(x)}, {"valore", std::to_string(v)}});
        }
        px[(y - d.top) * w + (x - d.left)] = static_cast<float>(sample);
      }
    }
    if (hooks.after_segment) hooks.after_segment(k, mem.peak);
  }

  decoder_state st;
  st.source = {metadata_origin::decoder, ""};
  st.raw_width = d.width;
  st.raw_height = d.height;
  st.window = {d.left, d.top, w, h};
  // §4: rappresentazione di LibRaw (ADR-03.b §b.2): il verde sulla riga 1 della cella ha indice 3.
  for (std::size_t site = 0; site < 4; ++site)
    st.cfa_color_index[site] = site >= 2 && d.cell[site] == 1 ? std::uint8_t{3} : d.cell[site];
  // §4: nero dichiarato, mai sottratto; motivo R × C ancorato all'origine di ActiveArea, cioè dell'area importata.
  if (d.black) {
    const auto value = [&r, &d](std::uint32_t k) {
      if (d.black->type != t_rational) return r.value(*d.black, k);
      const auto [num, den] = r.rational(*d.black, k);
      return num / den;
    };
    if (d.black->count == 1) {
      st.black = value(0);
    } else {
      st.black_pattern_rows = d.black_rows;
      st.black_pattern_cols = d.black_cols;
      st.black_pattern.reserve(d.black->count);
      for (std::uint32_t k = 0; k < d.black->count; ++k) st.black_pattern.push_back(value(k));
    }
  }
  st.white = d.white;
  // ADR-08.b §2.4 sulla tabella estesa a 65 536 voci con l'ultima.
  if (!table.empty()) {
    std::vector<std::uint16_t> curve(0x10000);
    for (std::size_t i = 0; i < curve.size(); ++i) curve[i] = table[std::min(i, table.size() - 1)];
    st.curve_sha256 = curve_digest(std::span<const std::uint16_t, 0x10000>(curve.data(), 0x10000));
  }
  st.linearization = st.curve_sha256 ? decoder_linearization::decoder_curve : decoder_linearization::identity;
  st.route = route.result;

  dng_decoder_fields fields;
  fields.dng_version = d.dng_version;
  fields.dng_backward_version = d.dng_backward_version;
  fields.tiles = seg.tiles;
  fields.rows_per_strip = seg.rows_per_strip_written;
  fields.tile_width = seg.tile_width;
  fields.tile_length = seg.tile_length;
  fields.segment_count = seg.count;
  fields.predictor = d.predictor;
  fields.little_endian = route.little_endian;
  fields.zlib_runtime_version = zlibVersion();
  read_metadata(r, ifd0, raw, img.metadata, fields);
  // Senza il tag il bianco è il predefinito di §4 (2^32 − 1): la parola chiave lo dichiara.
  img.metadata.saturation_adu = metadata_value<double>{static_cast<double>(d.white),
                                                       from_tag(d.white_written ? "WhiteLevel" : "WhiteLevel predefinito")};
  st.dng = std::move(fields);

  img.unit = pixel_unit::adu;
  img.lin = linearity::linear;
  img.lin_origin.value = metadata_origin::decoder;
  img.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::decoder, ""}};
  img.decoder_state = std::move(st);
  return img;
}

}  // namespace

route_detail route(std::span<const std::uint8_t> bytes) {
  route_detail d;
  d.result.reason = "not_tiff";
  // §3 punto 1: solo il TIFF classico; BigTIFF (`II+\0`, `MM\0+`) resta a LibRaw.
  if (bytes.size() < 8) return d;
  const bool ii = bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 42 && bytes[3] == 0;
  const bool mm = bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0 && bytes[3] == 42;
  if (!ii && !mm) return d;
  d.little_endian = ii;
  const reader r(bytes, ii, route_function);

  // §3 punto 2: DNGVersion in IFD0; senza, il file è un TIFF per LibRaw e il resto non si legge.
  d.ifd0 = r.uint(4, 4);
  const ifd_table ifd0 = r.table(d.ifd0);
  if (!r.get(ifd0, 50706, bit(t_byte), 4, 4)) {
    d.result.reason = "tiff_not_dng";
    return d;
  }

  // §3 punto 3: IFD raw = IFD0 e `SubIFDs` con NewSubFileType 0 (predefinito), IFD della catena con il tag uguale a 0.
  std::uint32_t raw_count = 0, project_count = 0;
  std::uint32_t compression = 0, sample_format = 0, bits = 0;
  const auto consider = [&](const ifd_table& t) {
    const std::uint32_t c = first_short(r, t, 259, 1, 1);
    const std::uint32_t s = first_short(r, t, 339, any_count, 1);
    const std::uint32_t b = first_short(r, t, 258, any_count, 1);
    if (raw_count++ == 0) {
      compression = c;
      sample_format = s;
      bits = b;
      d.raw_ifd = t.offset;
    }
    // §3 punto 4: insieme del progetto.
    if ((c == 8 && s == 1) || c == 52546) ++project_count;
  };
  // U13 §3 punto 2: ogni offset si legge e si conta una volta sola, per il tetto e per i punti 3-5; `read_next` tiene
  // il «prossimo IFD» delle tabelle già lette, così la catena prosegue anche da un IFD letto prima come `SubIFDs`.
  std::map<std::uint32_t, std::uint32_t> read_next{{d.ifd0, ifd0.next}};
  const auto read_once = [&](std::uint32_t offset) -> std::optional<ifd_table> {
    if (read_next.count(offset) != 0) return std::nullopt;
    if (read_next.size() >= raw_limits::max_route_ifds)
      throw stop_error(error_category::limit_exceeded, "raw.max_ifds", "troppi IFD nel DNG",
                       {{"funzione", route_function}, {"offset", std::to_string(offset)},
                        {"limite", std::to_string(raw_limits::max_route_ifds)}});
    ifd_table t = r.table(offset);
    read_next.emplace(offset, t.next);
    return t;
  };
  if (new_subfile_type(r, ifd0).value_or(0) == 0) consider(ifd0);
  if (const std::optional<field> sub = r.get(ifd0, 330, bit(t_long) | bit(t_ifd), 1, any_count)) {
    for (std::uint32_t k = 0; k < sub->count; ++k)
      if (const std::optional<ifd_table> t = read_once(r.value(*sub, k)); t && new_subfile_type(r, *t).value_or(0) == 0)
        consider(*t);
  }
  // Ciclo: un offset che torna nella catena (IFD0 compreso).
  std::set<std::uint32_t> chain{d.ifd0};
  for (std::uint32_t next = ifd0.next; next != 0;) {
    if (!chain.insert(next).second)
      data_error(route_function, "ciclo nella catena degli IFD", {{"offset", std::to_string(next)}});
    if (const std::optional<ifd_table> t = read_once(next)) {
      next = t->next;
      if (new_subfile_type(r, *t) == std::optional<std::uint32_t>(0)) consider(*t);
    } else {
      next = read_next.at(next);
    }
  }

  // §3 punto 5.
  if (project_count > 0 && raw_count > 1)
    throw stop_error(error_category::unsupported, "raw.multi_image", "DNG con più IFD raw",
                     {{"funzione", route_function}, {"ifd_raw", std::to_string(raw_count)},
                      {"ifd_progetto", std::to_string(project_count)}});
  if (raw_count == 1) {
    d.result.compression = compression;
    d.result.sample_format = sample_format;
    d.result.bits_per_sample = bits;
  }
  if (project_count == 1) {
    d.result.decoder = raw_decoder_kind::astro_dng;
    d.result.reason.clear();
  } else {
    d.result.reason = "dng_libraw_compression";
  }
  return d;
}

image decode(std::span<const std::uint8_t> bytes, const route_detail& route, const raw_decode_hooks& hooks,
             const raw_decode_context& ctx) {
  ASTRO_ASSERT(route.result.decoder == raw_decoder_kind::astro_dng && route.result.compression &&
               route.result.sample_format && route.result.bits_per_sample);
  try {
    return decode_checked(bytes, route, hooks, ctx);
  } catch (stop_error& stop) {
    // §8: provenance dell'arresto con il decoder e i campi dell'instradamento.
    stop.add_context("decoder", "astro_dng");
    stop.add_context("compression", std::to_string(*route.result.compression));
    stop.add_context("sample_format", std::to_string(*route.result.sample_format));
    stop.add_context("bits_per_sample", std::to_string(*route.result.bits_per_sample));
    throw;
  }
}

}  // namespace astro::dng_detail

namespace astro {

dng_route_result dng_route(std::span<const std::uint8_t> bytes) { return dng_detail::route(bytes).result; }

}  // namespace astro
