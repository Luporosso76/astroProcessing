#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>

#include "fits_internal.hpp"

namespace astro::fits_detail {

void fail(error_category category, const char* code, std::string message, stop_error::context_list context) {
  throw stop_error(category, code, std::move(message), std::move(context));
}

void malformed(std::string message, stop_error::context_list context) {
  fail(error_category::invalid_input, "fits.malformed", std::move(message), std::move(context));
}

std::string trim(std::string_view s) {
  std::size_t b = 0;
  std::size_t e = s.size();
  while (b < e && s[b] == ' ') ++b;
  while (e > b && s[e - 1] == ' ') --e;
  return std::string(s.substr(b, e - b));
}

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

// Il campo keyword è allineato a sinistra e contiene solo A-Z, 0-9, '-', '_' (FITS 4.0 §4.1.2.1):
// uno spazio iniziale o una minuscola farebbero leggere la card in modo diverso a CFITSIO.
std::string keyword_field(std::string_view card) {
  std::string_view field = card.substr(0, std::min<std::size_t>(8, card.size()));
  while (!field.empty() && field.back() == ' ') field.remove_suffix(1);
  for (char c : field) {
    const bool legal = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    if (!legal) malformed("keyword non conforme", {{"card", std::string(card.substr(0, 8))}});
  }
  return std::string(field);
}

parsed_card parse_card(const std::string& card) {
  parsed_card p;
  p.key = keyword_field(card);
  p.has_value = card.size() >= 10 && card[8] == '=' && card[9] == ' ';
  if (!p.has_value) return p;
  std::size_t i = 10;
  while (i < card.size() && card[i] == ' ') ++i;
  if (i < card.size() && card[i] == '\'') {
    p.is_string = true;
    ++i;
    std::string out;
    bool closed = false;
    while (i < card.size()) {
      if (card[i] == '\'') {
        if (i + 1 < card.size() && card[i + 1] == '\'') {
          out += '\'';
          i += 2;
          continue;
        }
        closed = true;
        break;
      }
      out += card[i++];
    }
    if (!closed) malformed("stringa non chiusa in una card", {{"keyword", p.key}});
    while (!out.empty() && out.back() == ' ') out.pop_back();
    p.text = out;
  } else {
    std::size_t slash = card.find('/', i);
    p.text = trim(std::string_view(card).substr(i, slash == std::string::npos ? std::string::npos : slash - i));
  }
  return p;
}

bool is_structural_key(const std::string& key) {
  if (key == "SIMPLE" || key == "BITPIX" || key == "NAXIS" || key == "EXTEND" || key == "BSCALE" ||
      key == "BZERO" || key == "PCOUNT" || key == "GCOUNT" || key == "XTENSION" || key == "CHECKSUM" ||
      key == "DATASUM") {
    return true;
  }
  if (key.size() > 5 && key.compare(0, 5, "NAXIS") == 0) {
    return std::all_of(key.begin() + 5, key.end(), [](char c) { return c >= '0' && c <= '9'; });
  }
  return false;
}

std::optional<std::int64_t> try_integer(const parsed_card& c) {
  if (!c.has_value || c.is_string || c.text.empty()) return std::nullopt;
  errno = 0;
  char* end = nullptr;
  long long v = std::strtoll(c.text.c_str(), &end, 10);
  if (errno != 0 || end == c.text.c_str() || *end != '\0') return std::nullopt;
  return v;
}

std::optional<double> try_real(const parsed_card& c) {
  if (!c.has_value || c.is_string || c.text.empty()) return std::nullopt;
  std::string t = c.text;
  std::replace(t.begin(), t.end(), 'D', 'E');
  std::replace(t.begin(), t.end(), 'd', 'e');
  errno = 0;
  char* end = nullptr;
  double v = std::strtod(t.c_str(), &end);
  if (errno != 0 || end == t.c_str() || *end != '\0' || !std::isfinite(v)) return std::nullopt;
  return v;
}

namespace {

std::int64_t required_integer(const parsed_card& c) {
  auto v = try_integer(c);
  if (!v) malformed("valore intero non valido", {{"keyword", c.key}, {"valore", c.text}});
  return *v;
}

double required_real(const parsed_card& c) {
  auto v = try_real(c);
  if (!v) malformed("valore numerico non valido", {{"keyword", c.key}, {"valore", c.text}});
  return *v;
}

bool is_valid_bitpix(std::int64_t b) {
  return b == 8 || b == 16 || b == 32 || b == 64 || b == -32 || b == -64;
}

struct header_state {
  std::uint64_t ncards = 0;
  std::int64_t naxis = -1;
  std::uint64_t required = 3;
  std::int64_t pcount = 0;
  std::int64_t gcount = 1;
  std::uint64_t samples = 1;
  bool seen_bscale = false;
  bool seen_bzero = false;
  bool seen_blank = false;
  bool seen_extname = false;
  bool seen_pcount = false;
  bool seen_gcount = false;
};

void once(bool& flag, const std::string& key) {
  if (flag) malformed("keyword strutturale duplicata", {{"keyword", key}});
  flag = true;
}

void first_card(const parsed_card& p, bool primary) {
  if (primary) {
    if (p.key != "SIMPLE" || p.is_string || p.text != "T") malformed("prima card diversa da SIMPLE = T");
    return;
  }
  if (p.key != "XTENSION") malformed("estensione senza XTENSION");
  if (p.text != "IMAGE") {
    fail(error_category::unsupported, "fits.hdu_kind", "estensione non immagine", {{"xtension", p.text}});
  }
}

void naxis_card(const parsed_card& p, std::size_t hdu_index, header_state& st) {
  if (p.key != "NAXIS") malformed("terza card diversa da NAXIS");
  st.naxis = required_integer(p);
  if (st.naxis < 0) malformed("NAXIS negativo");
  if (st.naxis == 0) {
    fail(error_category::unsupported, "fits.hdu_kind", "NAXIS = 0: nessuna immagine",
         {{"hdu", std::to_string(hdu_index)}});
  }
  if (st.naxis > static_cast<std::int64_t>(fits_limits::max_naxis)) {
    fail(error_category::limit_exceeded, "fits.max_naxis", "NAXIS oltre il limite",
         {{"naxis", std::to_string(st.naxis)}, {"limite", std::to_string(fits_limits::max_naxis)}});
  }
  st.required = 3 + static_cast<std::uint64_t>(st.naxis);
}

void axis_card(const parsed_card& p, std::uint64_t index, hdu_scan& h, header_state& st) {
  if (p.key != "NAXIS" + std::to_string(index)) malformed("NAXISn fuori ordine");
  const std::int64_t n = required_integer(p);
  if (n < 1) malformed("lunghezza d'asse non positiva", {{"asse", std::to_string(index)}});
  const auto length = static_cast<std::uint64_t>(n);
  if (index == 3) {
    if (length > fits_limits::max_planes) {
      fail(error_category::limit_exceeded, "fits.max_planes", "troppi piani",
           {{"valore", std::to_string(length)}, {"limite", std::to_string(fits_limits::max_planes)}});
    }
  } else if (length > fits_limits::max_axis_length) {
    fail(error_category::limit_exceeded, "fits.max_axis_length", "asse oltre il limite",
         {{"asse", std::to_string(index)},
          {"valore", std::to_string(length)},
          {"limite", std::to_string(fits_limits::max_axis_length)}});
  }
  if (__builtin_mul_overflow(st.samples, length, &st.samples) || st.samples > fits_limits::max_samples) {
    fail(error_category::limit_exceeded, "fits.max_samples", "troppi campioni",
         {{"limite", std::to_string(fits_limits::max_samples)}});
  }
  h.axes.push_back(length);
}

void trailing_card(const parsed_card& p, hdu_scan& h, header_state& st) {
  const std::string& k = p.key;
  if (is_structural_key(k)) {
    if (k == "BSCALE") {
      once(st.seen_bscale, k);
      h.bscale = required_real(p);
      if (h.bscale == 0.0) malformed("BSCALE nullo");
    } else if (k == "BZERO") {
      once(st.seen_bzero, k);
      h.bzero = required_real(p);
    } else if (k == "PCOUNT") {
      once(st.seen_pcount, k);
      st.pcount = required_integer(p);
      if (st.pcount < 0) malformed("PCOUNT negativo");
    } else if (k == "GCOUNT") {
      once(st.seen_gcount, k);
      st.gcount = required_integer(p);
      if (st.gcount < 0) malformed("GCOUNT negativo");
    } else if (k != "EXTEND" && k != "CHECKSUM" && k != "DATASUM") {
      malformed("keyword strutturale duplicata", {{"keyword", k}});
    }
  } else if (k == "BLANK") {
    once(st.seen_blank, k);
    h.has_blank = true;
  } else if (k == "EXTNAME") {
    once(st.seen_extname, k);
    if (p.has_value) h.extname = p.text;
  }
}

void process_card(const std::string& card, std::uint64_t i, bool primary, std::size_t hdu_index, hdu_scan& h,
                  header_state& st) {
  parsed_card p = parse_card(card);
  if (i == 0) {
    first_card(p, primary);
  } else if (i == 1) {
    if (p.key != "BITPIX") malformed("seconda card diversa da BITPIX");
    const std::int64_t b = required_integer(p);
    if (!is_valid_bitpix(b)) malformed("BITPIX non valido", {{"bitpix", std::to_string(b)}});
    h.bitpix = static_cast<int>(b);
  } else if (i == 2) {
    naxis_card(p, hdu_index, st);
  } else if (i < st.required) {
    axis_card(p, i - 2, h, st);
  } else {
    trailing_card(p, h, st);
  }
  h.cards.push_back(card);
}

// Legge i blocchi di un header a partire da `off`; ritorna l'offset del primo byte dopo l'header.
std::uint64_t scan_header(std::uint64_t size, const read_fn& read, std::uint64_t off, bool primary,
                          std::size_t hdu_index, hdu_scan& h, header_state& st) {
  bool end = false;
  std::array<char, block_size> block{};
  while (!end) {
    if (off + block_size > size) malformed("header troncato");
    read(off, block.data(), block.size());
    off += block_size;
    for (std::size_t c = 0; c < block_size / card_size; ++c) {
      std::string card(block.data() + c * card_size, card_size);
      if (card.compare(0, 8, "END     ") == 0) {
        end = true;
        break;
      }
      if (++st.ncards > fits_limits::max_header_cards) {
        fail(error_category::limit_exceeded, "fits.max_header_cards", "troppe card nell'header",
             {{"limite", std::to_string(fits_limits::max_header_cards)}});
      }
      process_card(card, st.ncards - 1, primary, hdu_index, h, st);
    }
  }
  if (st.ncards < st.required) malformed("header privo delle card obbligatorie");
  return off;
}

std::vector<hdu_scan> scan_fits(std::uint64_t size, const read_fn& read) {
  std::vector<hdu_scan> hdus;
  std::uint64_t off = 0;
  if (size == 0) malformed("file vuoto");
  while (off < size) {
    if (hdus.size() >= fits_limits::max_hdus) {
      fail(error_category::limit_exceeded, "fits.max_hdus", "troppi HDU",
           {{"limite", std::to_string(fits_limits::max_hdus)}});
    }
    const bool primary = hdus.empty();
    hdu_scan h;
    h.header_offset = off;
    header_state st;
    off = scan_header(size, read, off, primary, hdus.size(), h, st);
    if (st.pcount != 0 || st.gcount != 1) {
      fail(error_category::unsupported, "fits.hdu_kind", "HDU con PCOUNT/GCOUNT non standard (gruppi o heap)");
    }
    h.data_bytes = st.samples * static_cast<std::uint64_t>(std::abs(h.bitpix) / 8);
    h.data_offset = off;
    h.data_padded = (h.data_bytes + block_size - 1) / block_size * block_size;
    if (h.data_padded > size - off) malformed("dati troncati");
    off += h.data_padded;
    hdus.push_back(std::move(h));
  }
  return hdus;
}

void classify_extensions(scan_result& r) {
  const hdu_scan& p = r.hdus[0];
  for (std::size_t k = 1; k < r.hdus.size(); ++k) {
    const hdu_scan& e = r.hdus[k];
    const bool is_invalid = e.extname == "INVALID";
    if (!is_invalid && e.extname != "VARIANCE") {
      fail(error_category::unsupported, "fits.extension_kind", "estensione non riconosciuta",
           {{"hdu", std::to_string(k)}, {"extname", e.extname}});
    }
    int& slot = is_invalid ? r.invalid_hdu : r.variance_hdu;
    if (slot != -1) malformed("estensione duplicata", {{"extname", e.extname}});
    slot = static_cast<int>(k);
    if (e.width() != p.width() || e.height() != p.height() || e.planes() != p.planes() ||
        e.axes.size() != p.axes.size()) {
      fail(error_category::invalid_input, "fits.extension_geometry",
           "estensione con geometria diversa dal primario", {{"extname", e.extname}});
    }
    if (is_invalid) {
      if (e.bitpix != 16 || e.bscale != 1.0 || e.bzero != 32768.0) {
        fail(error_category::invalid_input, "fits.extension_encoding", "INVALID deve essere BITPIX 16, BZERO 32768");
      }
      r.plan.has_invalid = true;
    } else {
      if (e.bitpix != (r.plan.type == sample_type::f32 ? -32 : -64) || e.bscale != 1.0 || e.bzero != 0.0) {
        fail(error_category::invalid_input, "fits.extension_encoding",
             "VARIANCE: BITPIX uguale al tipo dei campioni e scala identita'");
      }
      r.plan.has_variance = true;
    }
  }
}

}  // namespace

sample_type type_for_encoding(int bitpix, double bscale, double bzero) {
  switch (bitpix) {
    case 8: return (bscale == 1.0 && (bzero == 0.0 || bzero == -128.0)) ? sample_type::f32 : sample_type::f64;
    case 16: return (bscale == 1.0 && (bzero == 0.0 || bzero == 32768.0)) ? sample_type::f32 : sample_type::f64;
    case 32: return sample_type::f64;
    case 64:
      if (bscale != 1.0 || bzero != 0.0) {
        fail(error_category::unsupported, "fits.int64_scaling", "BITPIX 64 con BSCALE/BZERO non identita'");
      }
      return sample_type::f64;
    case -32:
    case -64:
      if (bscale != 1.0 || bzero != 0.0) {
        fail(error_category::unsupported, "fits.float_scaling", "BITPIX float con BSCALE/BZERO non identita'");
      }
      return bitpix == -32 ? sample_type::f32 : sample_type::f64;
  }
  malformed("BITPIX non valido");
}

scan_result analyse(std::uint64_t size, const read_fn& read) {
  if (size > fits_limits::max_file_bytes) {
    fail(error_category::limit_exceeded, "fits.max_file_bytes", "file oltre il limite",
         {{"limite", std::to_string(fits_limits::max_file_bytes)}});
  }
  scan_result r;
  r.hdus = scan_fits(size, read);
  const hdu_scan& p = r.hdus[0];
  for (const hdu_scan& h : r.hdus) {
    if (h.has_blank && h.bitpix > 0) {
      fail(error_category::unsupported, "fits.blank", "BLANK in immagine intera: pixel indefiniti non supportati");
    }
  }
  r.plan.file_bytes = size;
  r.plan.samples = p.samples();
  r.plan.width = p.width();
  r.plan.height = p.height();
  r.plan.planes = p.planes();
  r.plan.type = type_for_encoding(p.bitpix, p.bscale, p.bzero);
  r.plan.bitpix = p.bitpix;
  classify_extensions(r);
  const std::uint64_t n = r.plan.samples;
  const std::uint64_t elem = sample_size(r.plan.type);
  r.plan.peak_bytes = size + n * elem + n * 2 + (r.plan.has_variance ? n * elem : 0);
  return r;
}

}  // namespace astro::fits_detail
