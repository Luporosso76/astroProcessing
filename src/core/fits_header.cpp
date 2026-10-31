#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>

#include "fits_internal.hpp"

namespace astro::fits_detail {

namespace {

// Unica tabella dei campi interpretati: ogni visitatore la percorre una sola volta per tipo di campo.
template <class F>
void for_each_field(F&& f) {
  f(&frame_metadata::exposure_s, "EXPTIME");
  f(&frame_metadata::gain, "GAIN");
  f(&frame_metadata::conversion_gain_e_per_adu, "EGAIN");
  f(&frame_metadata::offset_adu, "OFFSET");
  f(&frame_metadata::temperature_c, "CCD-TEMP");
  f(&frame_metadata::saturation_adu, "SATURATE");
  f(&frame_metadata::pedestal_adu, "PEDESTAL");
  f(&frame_metadata::filter, "FILTER");
  f(&frame_metadata::camera, "INSTRUME");
  f(&frame_metadata::imagetyp, "IMAGETYP");
  f(&frame_metadata::readout_mode, "READOUTM");
  f(&frame_metadata::row_order, "ROWORDER");
  f(&frame_metadata::roi_x, "XORGSUBF");
  f(&frame_metadata::roi_y, "YORGSUBF");
}

constexpr std::size_t max_measure_cards = 99;

constexpr const char* other_keys[] = {"BUNIT",    "LINEAR",   "BAYERPAT", "XBAYROFF", "YBAYROFF",
                                      "XBINNING", "YBINNING", "ASTRPHAS", "ASTRTSTP"};

[[noreturn]] void bad_value(const std::string& key, const char* expected) {
  fail(error_category::invalid_input, "fits.keyword_value", "tipo o valore di keyword non valido",
       {{"keyword", key}, {"atteso", expected}});
}

struct located {
  std::size_t index;
  parsed_card card;
};

class card_index {
 public:
  explicit card_index(const std::vector<std::string>& cards) {
    parsed_.reserve(cards.size());
    for (const std::string& c : cards) parsed_.push_back(parse_card(c));
  }

  const std::vector<parsed_card>& all() const { return parsed_; }

  std::optional<located> find(const std::string& key) const {
    std::optional<located> found;
    for (std::size_t i = 0; i < parsed_.size(); ++i) {
      if (parsed_[i].key != key) continue;
      if (found) fail(error_category::invalid_input, "fits.duplicate_keyword", "keyword interpretata duplicata", {{"keyword", key}});
      found = located{i, parsed_[i]};
    }
    if (!found) return found;
    if (!found->card.has_value) bad_value(key, "card con valore");
    const parsed_card& c = found->card;
    if (c.is_string && !c.text.empty() && c.text.back() == '&' && found->index + 1 < parsed_.size() &&
        parsed_[found->index + 1].key == "CONTINUE") {
      fail(error_category::unsupported, "fits.continue", "valore su card CONTINUE non supportato", {{"keyword", key}});
    }
    return found;
  }

 private:
  std::vector<parsed_card> parsed_;
};

double as_real(const located& l) {
  auto v = try_real(l.card);
  if (!v) bad_value(l.card.key, "reale");
  return *v;
}

std::int64_t as_integral(const located& l) {
  auto v = try_real(l.card);
  if (!v || *v != std::floor(*v) || std::fabs(*v) > static_cast<double>(max_exact_integer)) {
    bad_value(l.card.key, "intero");
  }
  return static_cast<std::int64_t>(*v);
}

// Card intera senza coercizione: un reale, anche di parte frazionaria nulla, e' un errore di tipo.
std::int64_t as_strict_integer(const located& l) {
  auto v = try_integer(l.card);
  if (!v) bad_value(l.card.key, "intero");
  return *v;
}

std::string as_string(const located& l) {
  if (!l.card.is_string) bad_value(l.card.key, "stringa");
  return l.card.text;
}

bool as_logical(const located& l) {
  if (l.card.is_string || (l.card.text != "T" && l.card.text != "F")) bad_value(l.card.key, "logico");
  return l.card.text == "T";
}

metadata_value<double> make_value(const located& l, double*) {
  return {as_real(l), {metadata_origin::header, l.card.key}};
}
metadata_value<std::string> make_value(const located& l, std::string*) {
  return {as_string(l), {metadata_origin::header, l.card.key}};
}
metadata_value<std::int64_t> make_value(const located& l, std::int64_t*) {
  return {as_integral(l), {metadata_origin::header, l.card.key}};
}

// ---- formattazione card

std::string finish_card(const std::string& key, const std::string& value) {
  std::string card = key;
  card.resize(8, ' ');
  card += "= " + value;
  if (card.size() > card_size) {
    fail(error_category::invalid_input, "fits.card_length", "valore troppo lungo per una card", {{"keyword", key}});
  }
  card.resize(card_size, ' ');
  return card;
}

std::string right_aligned(const std::string& v) { return std::string(v.size() < 20 ? 20 - v.size() : 0, ' ') + v; }

std::string format_card(const std::string& key, double v) {
  if (!std::isfinite(v)) fail(error_category::invalid_input, "fits.card_value", "valore non finito", {{"keyword", key}});
  char b[40];
  std::snprintf(b, sizeof b, "%.17G", v);
  return finish_card(key, right_aligned(b));
}

std::string format_card(const std::string& key, std::int64_t v) { return finish_card(key, right_aligned(std::to_string(v))); }

std::string format_card(const std::string& key, bool v) { return finish_card(key, right_aligned(v ? "T" : "F")); }

std::string format_card(const std::string& key, const std::string& v) {
  std::string q;
  for (char c : v) {
    if (c < 0x20 || c > 0x7e) {
      fail(error_category::invalid_input, "fits.card_value", "carattere non ASCII stampabile", {{"keyword", key}});
    }
    q += c;
    if (c == '\'') q += c;
  }
  if (q.size() < 8) q.resize(8, ' ');
  return finish_card(key, "'" + q + "'");
}

const char* unit_text(pixel_unit u) {
  switch (u) {
    case pixel_unit::adu: return "adu";
    case pixel_unit::electron: return "electron";
    case pixel_unit::dimensionless: return "dimensionless";
  }
  std::abort();
}

const char* phase_text(process_phase p) {
  switch (p) {
    case process_phase::raw: return "raw";
    case process_phase::calibrated: return "calibrated";
    case process_phase::prestacked: return "prestacked";
  }
  std::abort();
}

process_phase parse_phase(const std::string& raw) {
  std::string s = raw;
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (s == "raw") return process_phase::raw;
  if (s == "calibrated") return process_phase::calibrated;
  if (s == "prestacked") return process_phase::prestacked;
  fail(error_category::unsupported, "fits.phase", "ASTRPHAS non riconosciuta", {{"valore", raw}});
}

const char* pattern_text(cfa_pattern p) {
  switch (p) {
    case cfa_pattern::rggb: return "RGGB";
    case cfa_pattern::bggr: return "BGGR";
    case cfa_pattern::grbg: return "GRBG";
    case cfa_pattern::gbrg: return "GBRG";
  }
  std::abort();
}

cfa_pattern parse_pattern(const std::string& raw) {
  const std::string s = upper(raw);
  if (s == "RGGB") return cfa_pattern::rggb;
  if (s == "BGGR") return cfa_pattern::bggr;
  if (s == "GRBG") return cfa_pattern::grbg;
  if (s == "GBRG") return cfa_pattern::gbrg;
  fail(error_category::unsupported, "fits.bayer_pattern", "BAYERPAT non riconosciuto", {{"valore", s}});
}

std::optional<std::uint32_t> read_binning(const card_index& idx, const char* key) {
  auto l = idx.find(key);
  if (!l) return std::nullopt;
  const std::int64_t b = as_integral(*l);
  if (b < 1 || b > std::numeric_limits<std::int32_t>::max()) bad_value(key, "intero positivo");
  return static_cast<std::uint32_t>(b);
}

std::optional<std::uint8_t> read_offset(const card_index& idx, const char* key) {
  auto l = idx.find(key);
  if (!l) return std::nullopt;
  const std::int64_t v = as_integral(*l);
  if (v != 0 && v != 1) {
    fail(error_category::invalid_input, "fits.bayer_offset", "offset CFA diverso da 0 e 1", {{"keyword", key}});
  }
  return static_cast<std::uint8_t>(v);
}

std::uint8_t resolve_offset(std::optional<std::uint8_t> header, std::optional<std::uint8_t> declared,
                            const char* key) {
  if (header && declared && *header != *declared) {
    fail(error_category::incompatible, "metadata.declared_conflict", "offset CFA dichiarato diverso dall'header",
         {{"campo", key}});
  }
  if (header) return *header;
  if (declared) return *declared;
  fail(error_category::metadata_missing, "fits.bayer_offset_missing", "offset CFA assente", {{"keyword", key}});
}

void interpret_cfa(const card_index& idx, std::uint32_t planes, const fits_read_options& opt, header_info& info) {
  auto pattern = idx.find("BAYERPAT");
  const auto header_x = read_offset(idx, "XBAYROFF");
  const auto header_y = read_offset(idx, "YBAYROFF");
  const bool declared = opt.declared_bayer_x_offset || opt.declared_bayer_y_offset;
  if (!pattern) {
    if (header_x || header_y) {
      fail(error_category::metadata_missing, "fits.bayer_pattern_missing", "immagine dichiarata CFA senza BAYERPAT");
    }
    if (declared) {
      fail(error_category::invalid_input, "fits.declared_offset_unused", "offset CFA dichiarati per un'immagine non CFA");
    }
    return;
  }
  if (planes != 1) fail(error_category::invalid_input, "fits.cfa_planes", "immagine CFA con piu' di un piano");
  const cfa_pattern p = parse_pattern(as_string(*pattern));
  const std::uint8_t x = resolve_offset(header_x, opt.declared_bayer_x_offset, "XBAYROFF");
  const std::uint8_t y = resolve_offset(header_y, opt.declared_bayer_y_offset, "YBAYROFF");
  info.cfa = cfa_layout{p, x, y};
  info.layout = plane_layout::cfa;
}

std::string measure_key(std::size_t n) {
  char b[32];
  std::snprintf(b, sizeof b, "ASTRML%02zu", n);
  return b;
}

[[noreturn]] void bad_measure_keywords(const char* what) {
  fail(error_category::invalid_input, "fits.measure_keywords", "keyword della capacità di misura incoerenti",
       {{"motivo", what}});
}

measure_limit parse_measure_limit(const std::string& text) {
  for (int i = 0; i <= static_cast<int>(measure_limit::samples_interpolated); ++i) {
    const auto l = static_cast<measure_limit>(i);
    if (!is_derived_limit(l) && text == to_string(l)) return l;
  }
  fail(error_category::unsupported, "fits.measure_code", "codice di limitazione derivato o ignoto", {{"valore", text}});
}

std::optional<measure_capability> read_measure(const card_index& idx) {
  const auto cap = idx.find("ASTRMCAP");
  std::vector<located> codes;
  for (std::size_t n = 1; n <= max_measure_cards; ++n) {
    if (auto l = idx.find(measure_key(n))) {
      if (codes.size() != n - 1) bad_measure_keywords("numerazione con buchi");
      codes.push_back(*l);
    }
  }
  if (!cap) {
    if (!codes.empty()) bad_measure_keywords("ASTRMLnn senza ASTRMCAP");
    return std::nullopt;
  }
  if (!as_logical(*cap)) bad_measure_keywords("ASTRMCAP = F");
  measure_capability m;
  for (const located& c : codes) {
    const measure_limit l = parse_measure_limit(as_string(c));
    if (!m.limits.empty() && l <= m.limits.back()) bad_measure_keywords("ordine non canonico o codice duplicato");
    m.limits.push_back(l);
  }
  return m;
}

bool is_interpreted(const std::string& key, bool rgb) {
  if (rgb && key == "CTYPE3") return true;
  if (is_measure_key(key)) return true;
  if (std::any_of(std::begin(other_keys), std::end(other_keys), [&](const char* k) { return key == k; })) return true;
  bool found = false;
  for_each_field([&](auto, const char* k) { found = found || key == k; });
  return found;
}

}  // namespace

bool is_measure_key(const std::string& key) {
  if (key == "ASTRMCAP") return true;
  for (std::size_t n = 1; n <= max_measure_cards; ++n) {
    if (key == measure_key(n)) return true;
  }
  return false;
}

header_info interpret_header(const std::vector<std::string>& cards, std::uint32_t planes,
                             const fits_read_options& options) {
  header_info info;
  const card_index idx(cards);
  for_each_field([&](auto member, const char* key) {
    using mv = typename std::decay_t<decltype(info.metadata.*member)>::value_type;
    using value_type = decltype(mv::value);
    if (auto l = idx.find(key)) info.metadata.*member = make_value(*l, static_cast<value_type*>(nullptr));
  });
  if (auto l = idx.find("ASTRTSTP")) {
    info.metadata.capture_time_libraw =
        metadata_value<std::int64_t>{as_strict_integer(*l), {metadata_origin::header, "ASTRTSTP"}};
  }
  if (auto l = idx.find("BUNIT")) {
    const std::string u = upper(as_string(*l));
    if (u == "ADU") info.unit = pixel_unit::adu;
    else if (u == "ELECTRON") info.unit = pixel_unit::electron;
    else if (u == "DIMENSIONLESS") info.unit = pixel_unit::dimensionless;
    else fail(error_category::unsupported, "fits.bunit", "BUNIT non riconosciuta", {{"valore", u}});
  }
  if (auto l = idx.find("LINEAR")) {
    info.lin = as_logical(*l) ? linearity::linear : linearity::nonlinear;
    info.lin_origin = metadata_origin::header;
  }
  if (options.declared_linearity) {
    const linearity_class declared = *options.declared_linearity;
    if (info.lin) {
      const linearity_class found = *info.lin == linearity::linear ? linearity_class::linear : linearity_class::nonlinear;
      if (declared != found) {
        fail(error_category::incompatible, "metadata.declared_conflict", "linearita' dichiarata diversa dall'header",
             {{"campo", "linearity"}});
      }
    } else {
      info.lin_origin = metadata_origin::declared;
      if (declared == linearity_class::linear) info.lin = linearity::linear;
      if (declared == linearity_class::nonlinear) info.lin = linearity::nonlinear;
    }
  }
  if (auto l = idx.find("ASTRPHAS")) {
    info.phase = metadata_value<process_phase>{parse_phase(as_string(*l)), {metadata_origin::header, "ASTRPHAS"}};
  }
  if (options.declared_phase) {
    if (!info.phase) {
      info.phase = metadata_value<process_phase>{*options.declared_phase, {metadata_origin::declared, {}}};
    } else if (info.phase->value != *options.declared_phase) {
      fail(error_category::incompatible, "metadata.declared_conflict", "fase dichiarata diversa dall'header",
           {{"campo", "phase"}});
    }
  }
  info.measure = read_measure(idx);
  info.binning_x = read_binning(idx, "XBINNING");
  info.binning_y = read_binning(idx, "YBINNING");

  interpret_cfa(idx, planes, options, info);
  if (!info.cfa) {
    info.layout = planes == 1 ? plane_layout::mono : plane_layout::multi_plane;
    if (planes == 3) {
      if (auto l = idx.find("CTYPE3"); l && l->card.is_string && upper(l->card.text) == "RGB") {
        info.layout = plane_layout::rgb;
      }
    }
  }
  const bool rgb = info.layout == plane_layout::rgb;
  std::set<std::string> seen_set;
  for (std::size_t i = 0; i < cards.size(); ++i) {
    const std::string& key = idx.all()[i].key;
    if (is_structural_key(key)) continue;
    info.header_cards.push_back(cards[i]);
    if (key.empty() || is_interpreted(key, rgb) || !seen_set.insert(key).second) continue;
    info.uninterpreted_keywords.push_back(key);
  }
  return info;
}

std::vector<std::string> missing_cards(const image& img, const std::vector<std::string>& existing) {
  std::set<std::string> have;
  for (const std::string& c : existing) have.insert(parse_card(c).key);
  std::vector<std::string> out;
  auto add = [&](const char* key, auto&& value) {
    if (!have.count(key)) out.push_back(format_card(key, value));
  };
  for_each_field([&](auto member, const char* key) {
    if (const auto& v = img.metadata.*member) add(key, v->value);
  });
  if (const auto& t = img.metadata.capture_time_libraw) add("ASTRTSTP", t->value);
  if (img.unit) add("BUNIT", std::string(unit_text(*img.unit)));
  if (img.lin) add("LINEAR", *img.lin == linearity::linear);
  if (img.phase) add("ASTRPHAS", std::string(phase_text(img.phase->value)));
  if (img.measure) {
    add("ASTRMCAP", true);
    for (std::size_t i = 0; i < img.measure->limits.size(); ++i) {
      add(measure_key(i + 1).c_str(), std::string(to_string(img.measure->limits[i])));
    }
  }
  if (img.cfa) {
    add("BAYERPAT", std::string(pattern_text(img.cfa->pattern)));
    add("XBAYROFF", static_cast<std::int64_t>(img.cfa->x_offset));
    add("YBAYROFF", static_cast<std::int64_t>(img.cfa->y_offset));
  }
  if (img.geometry.binning_x) add("XBINNING", static_cast<std::int64_t>(*img.geometry.binning_x));
  if (img.geometry.binning_y) add("YBINNING", static_cast<std::int64_t>(*img.geometry.binning_y));
  if (img.layout == plane_layout::rgb) add("CTYPE3", std::string("RGB"));
  return out;
}

void check_header_matches(const image& img, const header_info& info) {
  auto bad = [](const char* what) {
    fail(error_category::invalid_input, "fits.header_inconsistent",
         "i campi interpretati dell'immagine non coincidono con l'header", {{"campo", what}});
  };
  if (img.unit != info.unit) bad("unit");
  if (img.lin != info.lin) bad("lin");
  if (img.phase != info.phase) bad("phase");
  if (img.cfa != info.cfa) bad("cfa");
  if (img.measure != info.measure) bad("measure");
  if (img.layout != info.layout) bad("layout");
  if (img.geometry.binning_x != info.binning_x || img.geometry.binning_y != info.binning_y) bad("binning");
  for_each_field([&](auto member, const char* key) {
    if (!(img.metadata.*member == info.metadata.*member)) bad(key);
  });
  if (!(img.metadata.capture_time_libraw == info.metadata.capture_time_libraw)) bad("ASTRTSTP");
  if (img.uninterpreted_keywords != info.uninterpreted_keywords) bad("uninterpreted_keywords");
}

}  // namespace astro::fits_detail
