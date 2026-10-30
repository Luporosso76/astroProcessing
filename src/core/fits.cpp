#include "astro/fits.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>

#include <fitsio.h>

#include "astro/hash.hpp"
#include "astro/input_format.hpp"
#include "astro/job_probe.hpp"
#include "fits_internal.hpp"

namespace astro {

using namespace fits_detail;

namespace {

constexpr std::size_t chunk_elements = std::size_t{1} << 16;

// ---------------------------------------------------------------- CFITSIO

[[noreturn]] void cfitsio_fail(int status, const char* operation, error_category category) {
  char text[FLEN_STATUS] = {};
  fits_get_errstatus(status, text);
  std::string detail;
  char msg[FLEN_ERRMSG] = {};
  while (fits_read_errmsg(msg) != 0) {
    if (!detail.empty()) detail += " | ";
    detail += msg;
  }
  fits_clear_errmsg();
  fail(category, "fits.cfitsio_status", "errore CFITSIO",
       {{"operazione", operation},
        {"cfitsio_status", std::to_string(status)},
        {"cfitsio_testo", text},
        {"cfitsio_messaggi", detail}});
}

// Lettura: un file che CFITSIO non accetta e' un input non valido.
void check_read(int status, const char* operation) {
  if (status != 0) cfitsio_fail(status, operation, error_category::invalid_input);
}

// Scrittura: un errore CFITSIO su dati gia' validati e' un guasto di I/O, non un input errato.
void check_write(int status, const char* operation) {
  if (status != 0) cfitsio_fail(status, operation, error_category::io);
}

class fits_handle {
 public:
  fits_handle() = default;
  fits_handle(const fits_handle&) = delete;
  fits_handle& operator=(const fits_handle&) = delete;
  ~fits_handle() {
    if (f_ != nullptr) {
      int st = 0;
      fits_close_file(f_, &st);
      fits_clear_errmsg();
    }
  }
  fitsfile*& ptr() { return f_; }
  fitsfile* get() const { return f_; }
  void close_checked() {
    int st = 0;
    fitsfile* f = f_;
    f_ = nullptr;
    if (f != nullptr) fits_close_file(f, &st);
    check_write(st, "fits_close_file");
  }

 private:
  fitsfile* f_ = nullptr;
};

// CFITSIO conserva gli indirizzi di buf_ e size_: l'oggetto non e' ne' copiabile ne' spostabile.
class memory_fits {
 public:
  explicit memory_fits(std::span<const std::uint8_t> bytes)
      : buf_(const_cast<std::uint8_t*>(bytes.data())), size_(bytes.size()) {
    int st = 0;
    fits_open_memfile(&handle_.ptr(), "memory", READONLY, &buf_, &size_, 0, nullptr, &st);
    check_read(st, "fits_open_memfile");
  }
  memory_fits(const memory_fits&) = delete;
  memory_fits& operator=(const memory_fits&) = delete;
  fitsfile* get() const { return handle_.get(); }

 private:
  void* buf_;
  std::size_t size_;
  fits_handle handle_;
};

struct malloc_block {
  void* ptr = nullptr;
  malloc_block() = default;
  malloc_block(const malloc_block&) = delete;
  malloc_block& operator=(const malloc_block&) = delete;
  ~malloc_block() { std::free(ptr); }
};

read_fn memory_reader(std::span<const std::uint8_t> bytes) {
  return [bytes](std::uint64_t offset, void* dst, std::size_t n) {
    std::memcpy(dst, bytes.data() + offset, n);
  };
}

void move_to_hdu(fitsfile* f, std::size_t index) {
  int st = 0;
  int type = 0;
  fits_movabs_hdu(f, static_cast<int>(index) + 1, &type, &st);
  check_read(st, "fits_movabs_hdu");
  if (type != IMAGE_HDU) fail(error_category::unsupported, "fits.hdu_kind", "HDU non immagine");
}

// ---------------------------------------------------------------- verifiche del file aperto

void verify_checksums(fitsfile* f, std::size_t hdu_count) {
  for (std::size_t k = 0; k < hdu_count; ++k) {
    move_to_hdu(f, k);
    int dataok = 0;
    int hduok = 0;
    int st = 0;
    fits_verify_chksum(f, &dataok, &hduok, &st);
    check_read(st, "fits_verify_chksum");
    if (dataok == -1 || hduok == -1) {
      fail(error_category::integrity, "fits.checksum", "CHECKSUM o DATASUM non corrispondente",
           {{"hdu", std::to_string(k)}});
    }
  }
}

// ---------------------------------------------------------------- lettura campioni

template <class Raw, class Consume>
void read_chunks(fitsfile* f, int datatype, std::uint64_t n, Consume&& consume) {
  std::vector<Raw> raw(static_cast<std::size_t>(std::min<std::uint64_t>(n, chunk_elements)));
  for (std::uint64_t done = 0; done < n;) {
    const std::uint64_t count = std::min<std::uint64_t>(n - done, raw.size());
    int st = 0;
    int anynul = 0;
    fits_read_img(f, datatype, static_cast<LONGLONG>(done) + 1, static_cast<LONGLONG>(count), nullptr, raw.data(),
                  &anynul, &st);
    check_read(st, "fits_read_img");
    consume(done, count, raw.data());
    done += count;
  }
}

// Ogni campione deve essere invertibile esattamente con la formula del writer, altrimenti il file non
// si puo' riscrivere senza perdita.
template <class Raw, class Out>
void read_integers(fitsfile* f, int datatype, const hdu_scan& h, Out* out) {
  // bscale = 1 e bzero intero entro 2^31: somma di interi sotto 2^53, esatta e invertibile per costruzione.
  const bool exact_by_construction =
      h.bscale == 1.0 && h.bzero == std::floor(h.bzero) && std::fabs(h.bzero) <= 2147483648.0;
  read_chunks<Raw>(f, datatype, h.samples(), [&](std::uint64_t done, std::uint64_t count, const Raw* raw) {
    for (std::uint64_t i = 0; i < count; ++i) {
      if constexpr (std::is_same_v<Raw, LONGLONG>) {
        constexpr auto limit = static_cast<LONGLONG>(max_exact_integer);
        if (raw[i] > limit || raw[i] < -limit) {
          fail(error_category::unsupported, "fits.int64_range", "BITPIX 64 con modulo oltre 2^53");
        }
      }
      const auto r = static_cast<double>(raw[i]);
      const double v = r * h.bscale + h.bzero;
      if (!exact_by_construction &&
          (std::round((v - h.bzero) / h.bscale) != r || static_cast<double>(static_cast<Out>(v)) != v)) {
        fail(error_category::unsupported, "fits.scaling_not_exact", "BSCALE/BZERO non invertibili sul campione",
             {{"indice", std::to_string(done + i)}});
      }
      out[done + i] = static_cast<Out>(v);
    }
  });
}

template <class Out>
void read_samples(fitsfile* f, const hdu_scan& h, Out* out) {
  int st = 0;
  fits_set_bscale(f, 1.0, 0.0, &st);
  check_read(st, "fits_set_bscale");
  switch (h.bitpix) {
    case 8: read_integers<unsigned char>(f, TBYTE, h, out); break;
    case 16: read_integers<short>(f, TSHORT, h, out); break;
    case 32: read_integers<int>(f, TINT, h, out); break;
    case 64: read_integers<LONGLONG>(f, TLONGLONG, h, out); break;
    default: {
      int anynul = 0;
      fits_read_img(f, h.bitpix == -32 ? TFLOAT : TDOUBLE, 1, static_cast<LONGLONG>(h.samples()), nullptr, out,
                    &anynul, &st);
      check_read(st, "fits_read_img");
    }
  }
}

void read_invalid_mask(fitsfile* f, const hdu_scan& h, image& img) {
  int st = 0;
  fits_set_bscale(f, 1.0, 0.0, &st);
  check_read(st, "fits_set_bscale");
  read_chunks<short>(f, TSHORT, h.samples(), [&](std::uint64_t done, std::uint64_t count, const short* raw) {
    for (std::uint64_t i = 0; i < count; ++i) {
      const auto bits = static_cast<std::uint16_t>(static_cast<std::uint16_t>(raw[i]) ^ 0x8000u);
      if ((bits & invalid_reason::reserved_mask) != 0) {
        fail(error_category::invalid_input, "fits.invalid_reserved_bits", "bit riservato acceso in INVALID",
             {{"indice", std::to_string(done + i)}});
      }
      img.invalid.bits[done + i] = bits;
    }
  });
}

template <class T>
void flag_nonfinite(image& img) {
  const T* s = img.data<T>();
  for (std::size_t i = 0; i < img.invalid.bits.size(); ++i) {
    if (!std::isfinite(s[i]) && img.invalid.bits[i] == 0) img.invalid.bits[i] = invalid_reason::source_nonfinite;
  }
}

// ---------------------------------------------------------------- scrittura campioni

struct raw_range {
  double lo;
  double hi;
};

raw_range range_for(int bitpix) {
  switch (bitpix) {
    case 8: return {0.0, 255.0};
    case 16: return {-32768.0, 32767.0};
    case 32: return {-2147483648.0, 2147483647.0};
    default: return {-static_cast<double>(max_exact_integer), static_cast<double>(max_exact_integer)};
  }
}

template <class Raw, class In>
void write_integers(fitsfile* f, int datatype, const source_encoding& enc, const In* in, std::uint64_t n) {
  const raw_range range = range_for(enc.bitpix);
  std::vector<Raw> raw(static_cast<std::size_t>(std::min<std::uint64_t>(n, chunk_elements)));
  for (std::uint64_t done = 0; done < n;) {
    const std::uint64_t count = std::min<std::uint64_t>(n - done, raw.size());
    for (std::uint64_t i = 0; i < count; ++i) {
      const auto v = static_cast<double>(in[done + i]);
      auto where = [&] {
        return stop_error::context_list{{"indice", std::to_string(done + i)}, {"bitpix", std::to_string(enc.bitpix)}};
      };
      if (!std::isfinite(v)) {
        fail(error_category::numeric, "fits.nonfinite_in_integer_encoding",
             "campione non finito non rappresentabile in BITPIX intero", where());
      }
      const double r = std::round((v - enc.bzero) / enc.bscale);
      if (r < range.lo || r > range.hi) {
        fail(error_category::numeric, "fits.value_out_of_range", "campione fuori dal range dell'encoding", where());
      }
      if (r * enc.bscale + enc.bzero != v) {
        fail(error_category::numeric, "fits.precision_loss", "campione non rappresentabile esattamente nell'encoding", where());
      }
      raw[i] = static_cast<Raw>(r);
    }
    int st = 0;
    fits_write_img(f, datatype, static_cast<LONGLONG>(done) + 1, static_cast<LONGLONG>(count), raw.data(), &st);
    check_write(st, "fits_write_img");
    done += count;
  }
}

template <class In>
void write_samples(fitsfile* f, const source_encoding& enc, const In* in, std::uint64_t n) {
  int st = 0;
  fits_set_bscale(f, 1.0, 0.0, &st);
  check_write(st, "fits_set_bscale");
  switch (enc.bitpix) {
    case 8: write_integers<unsigned char>(f, TBYTE, enc, in, n); break;
    case 16: write_integers<short>(f, TSHORT, enc, in, n); break;
    case 32: write_integers<int>(f, TINT, enc, in, n); break;
    case 64: write_integers<LONGLONG>(f, TLONGLONG, enc, in, n); break;
    default:
      fits_write_img(f, sizeof(In) == 4 ? TFLOAT : TDOUBLE, 1, static_cast<LONGLONG>(n), const_cast<In*>(in), &st);
      check_write(st, "fits_write_img");
  }
}

std::uint32_t resolved_naxis(const image& img, const source_encoding& enc) {
  const image_geometry& g = img.geometry;
  const std::uint32_t derived = g.planes > 1 ? 3 : 2;
  const std::uint32_t naxis = enc.naxis == 0 ? derived : enc.naxis;
  const bool ok = (naxis == 1 && g.height == 1 && g.planes == 1) || (naxis == 2 && g.planes == 1) || naxis == 3;
  if (!ok) fail(error_category::invalid_input, "fits.encoding", "naxis incompatibile con la geometria");
  return naxis;
}

void create_hdu(fitsfile* f, int bitpix, std::uint32_t naxis, const image_geometry& g) {
  long naxes[3] = {static_cast<long>(g.width), static_cast<long>(g.height), static_cast<long>(g.planes)};
  int st = 0;
  fits_create_img(f, bitpix, static_cast<int>(naxis), naxes, &st);
  check_write(st, "fits_create_img");
}

void write_scale_keys(fitsfile* f, double bscale, double bzero) {
  int st = 0;
  if (bscale != 1.0) fits_write_key_dbl(f, "BSCALE", bscale, -17, nullptr, &st);
  check_write(st, "BSCALE");
  if (bzero != 0.0) fits_write_key_dbl(f, "BZERO", bzero, -17, nullptr, &st);
  check_write(st, "BZERO");
}

std::uint64_t padded_bytes(int bitpix, std::uint64_t count) {
  const std::uint64_t data = count * static_cast<std::uint64_t>(std::abs(bitpix) / 8);
  return (data + block_size - 1) / block_size * block_size;
}

// Fine (padded) dell'HDU corrente, a dati scritti.
std::uint64_t hdu_end(fitsfile* f, int bitpix, std::uint64_t samples) {
  LONGLONG head = 0, data = 0, end = 0;
  int st = 0;
  fits_get_hduaddrll(f, &head, &data, &end, &st);
  check_write(st, "fits_get_hduaddrll");
  return static_cast<std::uint64_t>(data) + padded_bytes(bitpix, samples);
}

source_encoding resolve_encoding(const image& img) {
  source_encoding enc =
      img.encoding.value_or(source_encoding{img.type == sample_type::f32 ? -32 : -64, 0.0, 1.0, 0});
  const bool valid_bitpix = enc.bitpix == 8 || enc.bitpix == 16 || enc.bitpix == 32 || enc.bitpix == 64 ||
                            enc.bitpix == -32 || enc.bitpix == -64;
  if (!valid_bitpix || enc.bscale == 0.0 || !std::isfinite(enc.bscale) || !std::isfinite(enc.bzero)) {
    fail(error_category::invalid_input, "fits.encoding", "encoding di origine non valido");
  }
  if (type_for_encoding(enc.bitpix, enc.bscale, enc.bzero) != img.type) {
    fail(error_category::invalid_input, "fits.encoding", "encoding incompatibile con il tipo dei campioni");
  }
  enc.naxis = resolved_naxis(img, enc);
  return enc;
}

// header_cards validate piu' le card dei campi interpretati che l'header non contiene ancora.
std::vector<std::string> compose_cards(const image& img) {
  for (const std::string& card : img.header_cards) {
    if (card.size() != card_size) fail(error_category::invalid_input, "fits.card_length", "card diversa da 80 caratteri");
    const std::string key = parse_card(card).key;
    if (is_structural_key(key) || key == "END") {
      fail(error_category::invalid_input, "fits.card_structural", "card strutturale in header_cards", {{"keyword", key}});
    }
  }
  std::vector<std::string> cards = img.header_cards;
  for (std::string& c : missing_cards(img, img.header_cards)) cards.push_back(std::move(c));
  return cards;
}

void write_primary(fitsfile* f, const image& img, const source_encoding& enc, const std::vector<std::string>& cards) {
  create_hdu(f, enc.bitpix, enc.naxis, img.geometry);
  int st = 0;
  // CFITSIO aggiunge due COMMENT standard al primario: non appartengono al contenuto dell'immagine.
  for (int i = 0; i < 2; ++i) {
    fits_delete_key(f, "COMMENT", &st);
    check_write(st, "fits_delete_key");
  }
  write_scale_keys(f, enc.bscale, enc.bzero);
  for (const std::string& card : cards) {
    fits_write_record(f, card.c_str(), &st);
    check_write(st, "fits_write_record");
  }
  const std::uint64_t n = img.geometry.sample_count();
  if (img.type == sample_type::f32) write_samples(f, enc, img.data<float>(), n);
  else write_samples(f, enc, img.data<double>(), n);
}

void begin_extension(fitsfile* f, const image& img, const source_encoding& enc, int bitpix, const char* name) {
  create_hdu(f, bitpix, enc.naxis, img.geometry);
  if (bitpix == 16) write_scale_keys(f, 1.0, 32768.0);
  int st = 0;
  fits_write_key_str(f, "EXTNAME", name, nullptr, &st);
  check_write(st, "EXTNAME");
  fits_set_bscale(f, 1.0, 0.0, &st);
  check_write(st, "fits_set_bscale");
}

void write_invalid(fitsfile* f, const image& img, const source_encoding& enc) {
  begin_extension(f, img, enc, 16, "INVALID");
  const std::uint64_t n = img.invalid.bits.size();
  std::vector<short> raw(static_cast<std::size_t>(std::min<std::uint64_t>(n, chunk_elements)));
  for (std::uint64_t done = 0; done < n;) {
    const std::uint64_t count = std::min<std::uint64_t>(n - done, raw.size());
    for (std::uint64_t i = 0; i < count; ++i) {
      raw[i] = static_cast<short>(static_cast<std::uint16_t>(img.invalid.bits[done + i] ^ 0x8000u));
    }
    int st = 0;
    fits_write_img(f, TSHORT, static_cast<LONGLONG>(done) + 1, static_cast<LONGLONG>(count), raw.data(), &st);
    check_write(st, "fits_write_img");
    done += count;
  }
}

void write_variance(fitsfile* f, const image& img, const source_encoding& enc) {
  const int bitpix = img.type == sample_type::f32 ? -32 : -64;
  begin_extension(f, img, enc, bitpix, "VARIANCE");
  const source_encoding identity{bitpix, 0.0, 1.0, enc.naxis};
  const std::uint64_t n = img.geometry.sample_count();
  if (img.type == sample_type::f32) write_samples(f, identity, img.variance_data<float>(), n);
  else write_samples(f, identity, img.variance_data<double>(), n);
}

// Rilegge il file scritto con lo stesso parser del lettore e confronta i campi interpretati.
void verify_written(const image& img, const std::vector<std::uint8_t>& out) {
  const scan_result scan = analyse(out.size(), memory_reader(out));
  check_header_matches(img, interpret_header(scan.hdus[0].cards, scan.hdus[0].planes(), {}));
}

}  // namespace

namespace fits_detail {

void verify_structure(fitsfile* f, const std::vector<hdu_scan>& hdus) {
  for (std::size_t k = 0; k < hdus.size(); ++k) {
    const hdu_scan& h = hdus[k];
    move_to_hdu(f, k);
    LONGLONG head = 0, data = 0, end = 0;
    int st = 0;
    fits_get_hduaddrll(f, &head, &data, &end, &st);
    check_read(st, "fits_get_hduaddrll");
    int bitpix = 0, naxis = 0;
    LONGLONG naxes[3] = {0, 0, 0};
    fits_get_img_paramll(f, 3, &bitpix, &naxis, naxes, &st);
    check_read(st, "fits_get_img_paramll");
    bool same = static_cast<std::uint64_t>(head) == h.header_offset &&
                static_cast<std::uint64_t>(data) == h.data_offset &&
                static_cast<std::uint64_t>(end) == h.data_offset + h.data_padded && bitpix == h.bitpix &&
                static_cast<std::size_t>(naxis) == h.axes.size();
    for (std::size_t i = 0; same && i < h.axes.size(); ++i) same = static_cast<std::uint64_t>(naxes[i]) == h.axes[i];
    if (!same) {
      fail(error_category::invalid_input, "fits.structure_mismatch",
           "CFITSIO e la scansione dell'header leggono HDU diversi", {{"hdu", std::to_string(k)}});
    }
  }
}

}  // namespace fits_detail

// ---------------------------------------------------------------- input da file

namespace {

// Stop con il codice proprio del formato dichiarato all'apertura: `<prefisso><suffix>`.
[[noreturn]] void format_fail(const input_format& format, error_category category, const char* suffix,
                              std::string message, stop_error::context_list context = {}) {
  throw stop_error(category, std::string(format.prefix) + suffix, std::move(message), std::move(context));
}

// Errore di sistema su un input aperto: io `<prefisso dell'input><suffix>`, con `errno` in coda al contesto. Il
// prefisso viene solo dall'input, mai dal chiamante.
[[noreturn]] void input_syscall_fail(const fits_input& input, const char* suffix, const char* message, int e,
                                     stop_error::context_list context = {}) {
  context.emplace_back("errno", std::to_string(e));
  format_fail(input.format(), error_category::io, suffix, message, std::move(context));
}

// Fine file prima della dimensione aperta: un file cambiato dall'apertura (dimensione, mtime o ctime) e' integrity
// (input.size_changed, input.modified); solo un file con identita' invariata resta io input.short_read.
[[noreturn]] void unexpected_eof(const fits_input& input) {
  fits_detail::check_unchanged(input);
  fail(error_category::io, "input.short_read", "lettura incompleta con dimensione invariata");
}

void pread_exact(const fits_input& input, int fd, std::uint64_t offset, void* dst, std::size_t n,
                 const fault_injector& fault, const fits_input::pread_observer& observer) {
  job_probe::notify("input.read");
  if (observer) {
    const int injected = observer(offset, n);
    if (injected != 0) input_syscall_fail(input, ".read", "pread non riuscita", injected);
  }
  auto* p = static_cast<char*>(dst);
  std::size_t done = 0;
  while (done < n) {
    const bool eof = fault && fault("input.pread") != 0;
    const ssize_t r = eof ? 0 : ::pread(fd, p + done, n - done, static_cast<off_t>(offset + done));
    if (r < 0) {
      if (errno == EINTR) continue;
      input_syscall_fail(input, ".read", "pread non riuscita", errno);
    }
    if (r == 0) unexpected_eof(input);
    done += static_cast<std::size_t>(r);
  }
}

}  // namespace

fits_input::fits_input(fits_input&& other) noexcept
    : fd_(other.fd_), size_(other.size_), mtime_ns_(other.mtime_ns_), ctime_ns_(other.ctime_ns_),
      format_(other.format_), fault_(std::move(other.fault_)), observer_(std::move(other.observer_)) {
  other.fd_ = -1;
}

fits_input& fits_input::operator=(fits_input&& other) noexcept {
  if (this != &other) {
    if (fd_ >= 0) ::close(fd_);
    fd_ = other.fd_;
    size_ = other.size_;
    mtime_ns_ = other.mtime_ns_;
    ctime_ns_ = other.ctime_ns_;
    format_ = other.format_;
    fault_ = std::move(other.fault_);
    observer_ = std::move(other.observer_);
    other.fd_ = -1;
  }
  return *this;
}

fits_input::~fits_input() {
  if (fd_ >= 0) ::close(fd_);
}

namespace fits_detail {

void read_exact(const fits_input& input, std::uint64_t offset, void* dst, std::size_t n) {
  if (offset > input.size_ || n > input.size_ - offset) {
    fail(error_category::invalid_input, "fits.malformed", "lettura oltre la fine del file",
         {{"offset", std::to_string(offset)}, {"lunghezza", std::to_string(n)}});
  }
  pread_exact(input, input.fd_, offset, dst, n, input.fault_, input.observer_);
}

void check_unchanged(const fits_input& input) {
  struct stat sb {};
  if (::fstat(input.fd_, &sb) != 0) input_syscall_fail(input, ".fstat", "fstat non riuscita", errno);
  if (static_cast<std::uint64_t>(sb.st_size) != input.size_) {
    fail(error_category::integrity, "input.size_changed", "la dimensione del file e' cambiata dopo fstat",
         {{"atteso", std::to_string(input.size_)}, {"trovata", std::to_string(sb.st_size)}});
  }
  const auto ns = [](const struct timespec& t) { return std::int64_t{t.tv_sec} * 1000000000 + t.tv_nsec; };
  if (ns(sb.st_mtim) != input.mtime_ns_ || ns(sb.st_ctim) != input.ctime_ns_) {
    fail(error_category::integrity, "input.modified", "il file e' stato modificato dopo l'apertura");
  }
}

std::pair<std::uint64_t, std::uint64_t> identity_of(const fits_input& input) {
  struct stat sb {};
  if (::fstat(input.fd_, &sb) != 0) input_syscall_fail(input, ".fstat", "fstat non riuscita", errno);
  return {static_cast<std::uint64_t>(sb.st_dev), static_cast<std::uint64_t>(sb.st_ino)};
}

void check_path_names(const std::string& path, const fits_input& input) {
  struct stat named {};
  if (::stat(path.c_str(), &named) != 0) {
    const int e = errno;
    const char* message = "file dell'ingresso non accessibile";
    if (e == ENOENT) {
      fail(error_category::io, "input.missing", message, {{"percorso", path}, {"errno", std::to_string(e)}});
    }
    input_syscall_fail(input, ".fstat", message, e, {{"percorso", path}});
  }
  const auto [dev, ino] = identity_of(input);
  if (static_cast<std::uint64_t>(named.st_dev) != dev || static_cast<std::uint64_t>(named.st_ino) != ino) {
    fail(error_category::integrity, "input.modified", "il percorso indica un file diverso da quello letto",
         {{"percorso", path}});
  }
}

}  // namespace fits_detail

fits_input open_input(const std::string& path, const input_format& format) {
  job_probe::notify("input.open", path);
  job_probe::notify("input.open_format", std::string(format.prefix) + "\t" + path);
  int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
  if (fd < 0) {
    const int e = errno;
    stop_error::context_list context{{"percorso", path}, {"errno", std::to_string(e)}};
    if (e == ENOENT) fail(error_category::io, "input.missing", "apertura del file non riuscita", std::move(context));
    format_fail(format, error_category::io, ".open", "apertura del file non riuscita", std::move(context));
  }
  struct stat sb {};
  if (::fstat(fd, &sb) != 0) {
    const int e = errno;
    ::close(fd);
    format_fail(format, error_category::io, ".fstat", "fstat non riuscita",
                {{"percorso", path}, {"errno", std::to_string(e)}});
  }
  if (!S_ISREG(sb.st_mode)) {
    ::close(fd);
    format_fail(format, error_category::invalid_input, ".not_regular", "il file non e' regolare", {{"percorso", path}});
  }
  const auto size = static_cast<std::uint64_t>(sb.st_size);
  if (size > format.max_file_bytes) {
    ::close(fd);
    format_fail(format, error_category::limit_exceeded, ".max_file_bytes", "file oltre il limite",
                {{"dimensione", std::to_string(size)}, {"limite", std::to_string(format.max_file_bytes)}});
  }
  const auto ns = [](const struct timespec& t) { return std::int64_t{t.tv_sec} * 1000000000 + t.tv_nsec; };
  return fits_input(fd, size, ns(sb.st_mtim), ns(sb.st_ctim), &format);
}

fits_input open_fits_input(const std::string& path) { return open_input(path, fits_input_format()); }

fits_plan plan_fits(const fits_input& input) {
  const read_fn read = [&input](std::uint64_t offset, void* dst, std::size_t n) {
    pread_exact(input, input.fd_, offset, dst, n, input.fault_, input.observer_);
  };
  return analyse(input.size_, read).plan;
}

header_peek peek_header(const fits_input& input, const fits_read_options& options) {
  const read_fn read = [&input](std::uint64_t offset, void* dst, std::size_t n) { read_exact(input, offset, dst, n); };
  const scan_result scan = analyse(input.size_bytes(), read);
  const header_info info = interpret_header(scan.hdus[0].cards, scan.hdus[0].planes(), options);
  return header_peek{info.phase, info.lin};
}

fits_plan plan_fits(std::span<const std::uint8_t> bytes) {
  return analyse(bytes.size(), memory_reader(bytes)).plan;
}

std::vector<std::uint8_t> read_input_bytes(const fits_input& input, std::uint64_t budget_bytes) {
  if (input.size_ > budget_bytes) {
    fail(error_category::budget, "sched.job_exceeds_budget", "dimensione del file oltre il budget",
         {{"dimensione", std::to_string(input.size_)}, {"budget", std::to_string(budget_bytes)}});
  }
  std::vector<std::uint8_t> buffer(static_cast<std::size_t>(input.size_));
  pread_exact(input, input.fd_, 0, buffer.data(), buffer.size(), input.fault_, input.observer_);
  std::uint8_t probe = 0;
  ssize_t extra;
  do {
    extra = ::pread(input.fd_, &probe, 1, static_cast<off_t>(buffer.size()));
  } while (extra < 0 && errno == EINTR);
  if (extra < 0) input_syscall_fail(input, ".read", "pread non riuscita", errno);
  if (extra > 0) {
    fail(error_category::integrity, "input.size_changed", "il file e' cresciuto dopo fstat",
         {{"atteso", std::to_string(buffer.size())}});
  }
  return buffer;
}

std::vector<std::uint8_t> read_fits_bytes(const fits_input& input, std::uint64_t budget_bytes) {
  const fits_plan plan = plan_fits(input);
  if (plan.peak_bytes > budget_bytes) {
    fail(error_category::budget, "sched.job_exceeds_budget", "picco di memoria oltre il budget",
         {{"picco", std::to_string(plan.peak_bytes)}, {"budget", std::to_string(budget_bytes)}});
  }
  return read_input_bytes(input, budget_bytes);
}

// ---------------------------------------------------------------- decode

image decode_fits(std::span<const std::uint8_t> bytes, std::uint64_t budget_bytes, const fits_read_options& options) {
  if (job_probe::active()) job_probe::notify("input.samples", "sha256:" + sha256_hex(bytes));
  const scan_result scan = analyse(bytes.size(), memory_reader(bytes));
  if (scan.plan.peak_bytes > budget_bytes) {
    fail(error_category::budget, "sched.job_exceeds_budget", "picco di memoria oltre il budget",
         {{"picco", std::to_string(scan.plan.peak_bytes)}, {"budget", std::to_string(budget_bytes)}});
  }
  const hdu_scan& primary = scan.hdus[0];
  const header_info info = interpret_header(primary.cards, primary.planes(), options);

  memory_fits memory(bytes);
  fitsfile* f = memory.get();
  verify_structure(f, scan.hdus);
  verify_checksums(f, scan.hdus.size());

  image_geometry geometry(primary.width(), primary.height(), primary.planes());
  geometry.binning_x = info.binning_x;
  geometry.binning_y = info.binning_y;
  image img = make_image(geometry, scan.plan.type, info.layout, info.cfa);
  img.unit = info.unit;
  img.lin = info.lin;
  if (info.lin) img.lin_origin.value = info.lin_origin;
  img.phase = info.phase;
  img.metadata = info.metadata;
  img.header_cards = info.header_cards;
  img.uninterpreted_keywords = info.uninterpreted_keywords;
  img.encoding = source_encoding{primary.bitpix, primary.bzero, primary.bscale,
                                 static_cast<std::uint32_t>(primary.axes.size())};

  move_to_hdu(f, 0);
  if (img.type == sample_type::f32) read_samples(f, primary, img.data<float>());
  else read_samples(f, primary, img.data<double>());

  if (scan.invalid_hdu >= 0) {
    const auto k = static_cast<std::size_t>(scan.invalid_hdu);
    move_to_hdu(f, k);
    read_invalid_mask(f, scan.hdus[k], img);
  }
  if (scan.variance_hdu >= 0) {
    const auto k = static_cast<std::size_t>(scan.variance_hdu);
    move_to_hdu(f, k);
    img.variance = variance_plane{aligned_buffer(img.samples.size())};
    if (img.type == sample_type::f32) read_samples(f, scan.hdus[k], img.variance_data<float>());
    else read_samples(f, scan.hdus[k], img.variance_data<double>());
  }
  if (img.type == sample_type::f32) flag_nonfinite<float>(img);
  else flag_nonfinite<double>(img);
  return img;
}

// ---------------------------------------------------------------- encode

std::vector<std::uint8_t> encode_fits(const image& img) {
  validate_image(img);
  const source_encoding enc = resolve_encoding(img);
  const std::vector<std::string> cards = compose_cards(img);
  bool any_invalid = false;
  for (std::uint16_t b : img.invalid.bits) {
    if ((b & invalid_reason::reserved_mask) != 0) {
      fail(error_category::invalid_input, "image.mask_reserved_bits", "bit riservato acceso in invalid_mask");
    }
    any_invalid = any_invalid || b != 0;
  }

  malloc_block mem;
  mem.ptr = std::malloc(block_size);
  if (mem.ptr == nullptr) throw std::bad_alloc();
  std::size_t buf_size = block_size;
  fits_handle handle;
  int st = 0;
  fits_create_memfile(&handle.ptr(), &mem.ptr, &buf_size, std::size_t{1} << 20, std::realloc, &st);
  check_write(st, "fits_create_memfile");
  fitsfile* f = handle.get();
  const std::uint64_t n = img.geometry.sample_count();

  write_primary(f, img, enc, cards);
  std::uint64_t total = hdu_end(f, enc.bitpix, n);
  if (any_invalid) {
    write_invalid(f, img, enc);
    total = hdu_end(f, 16, n);
  }
  if (img.variance) {
    write_variance(f, img, enc);
    total = hdu_end(f, img.type == sample_type::f32 ? -32 : -64, n);
  }
  handle.close_checked();

  if (buf_size < total) fail(error_category::io, "fits.write", "buffer CFITSIO piu' corto del file atteso");
  std::vector<std::uint8_t> out(static_cast<std::size_t>(total));
  std::memcpy(out.data(), mem.ptr, out.size());
  verify_written(img, out);
  return out;
}

// ---------------------------------------------------------------- lettura a fasce di righe

namespace {

template <class T>
T load_big_endian(const std::uint8_t* p) {
  using U = std::conditional_t<sizeof(T) == 1, std::uint8_t,
            std::conditional_t<sizeof(T) == 2, std::uint16_t, std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>>;
  U raw = 0;
  for (std::size_t i = 0; i < sizeof(U); ++i) raw = static_cast<U>((raw << 8) | p[i]);
  T out;
  std::memcpy(&out, &raw, sizeof(T));
  return out;
}

// Stessa conversione di read_integers: valore r * bscale + bzero convertito al tipo dei campioni.
template <class Raw, class Out>
void integer_rows(const std::uint8_t* src, std::size_t count, double bscale, double bzero, double* values) {
  for (std::size_t i = 0; i < count; ++i) {
    const auto r = static_cast<double>(load_big_endian<Raw>(src + i * sizeof(Raw)));
    values[i] = static_cast<double>(static_cast<Out>(r * bscale + bzero));
  }
}

template <class Raw>
void float_rows(const std::uint8_t* src, std::size_t count, double* values) {
  for (std::size_t i = 0; i < count; ++i) values[i] = static_cast<double>(load_big_endian<Raw>(src + i * sizeof(Raw)));
}

}  // namespace

fits_row_layout plan_fits_rows(const fits_input& input) {
  const read_fn read = [&input](std::uint64_t offset, void* dst, std::size_t n) { read_exact(input, offset, dst, n); };
  const scan_result scan = analyse(input.size_bytes(), read);
  const hdu_scan& p = scan.hdus[0];
  if (p.planes() != 1) {
    fail(error_category::unsupported, "fits.row_planes", "lettura a righe di un solo piano",
         {{"piani", std::to_string(p.planes())}});
  }
  fits_row_layout l;
  l.width = p.width();
  l.height = p.height();
  l.bitpix = p.bitpix;
  l.bscale = p.bscale;
  l.bzero = p.bzero;
  l.type = scan.plan.type;
  l.data_offset = p.data_offset;
  if (scan.invalid_hdu >= 0) l.invalid_offset = scan.hdus[static_cast<std::size_t>(scan.invalid_hdu)].data_offset;
  return l;
}

std::uint64_t fits_row_scratch_bytes(const fits_row_layout& layout, std::uint32_t rows) {
  const std::uint64_t elem = static_cast<std::uint64_t>(std::max(std::abs(layout.bitpix) / 8, 2));
  return std::uint64_t{layout.width} * rows * elem;
}

void read_fits_rows(const fits_input& input, const fits_row_layout& layout, std::uint32_t y0, std::uint32_t rows,
                    std::byte* scratch, double* values, std::uint16_t* bits) {
  if (job_probe::active()) job_probe::notify("input.samples", "ino:" + std::to_string(fits_detail::identity_of(input).second));
  ASTRO_ASSERT(y0 <= layout.height && rows <= layout.height - y0);
  const std::size_t count = static_cast<std::size_t>(layout.width) * rows;
  const std::size_t elem = static_cast<std::size_t>(std::abs(layout.bitpix) / 8);
  const std::uint64_t first = std::uint64_t{y0} * layout.width;
  auto* raw = reinterpret_cast<std::uint8_t*>(scratch);
  read_exact(input, layout.data_offset + first * elem, raw, count * elem);
  const bool f32 = layout.type == sample_type::f32;
  switch (layout.bitpix) {
    case 8:
      f32 ? integer_rows<std::uint8_t, float>(raw, count, layout.bscale, layout.bzero, values)
          : integer_rows<std::uint8_t, double>(raw, count, layout.bscale, layout.bzero, values);
      break;
    case 16:
      f32 ? integer_rows<std::int16_t, float>(raw, count, layout.bscale, layout.bzero, values)
          : integer_rows<std::int16_t, double>(raw, count, layout.bscale, layout.bzero, values);
      break;
    case 32: integer_rows<std::int32_t, double>(raw, count, layout.bscale, layout.bzero, values); break;
    case 64: integer_rows<std::int64_t, double>(raw, count, layout.bscale, layout.bzero, values); break;
    case -32: float_rows<float>(raw, count, values); break;
    default: float_rows<double>(raw, count, values); break;
  }
  if (layout.invalid_offset) {
    read_exact(input, *layout.invalid_offset + first * 2, raw, count * 2);
    for (std::size_t i = 0; i < count; ++i) {
      const auto b = static_cast<std::uint16_t>(load_big_endian<std::uint16_t>(raw + i * 2) ^ 0x8000u);
      if ((b & invalid_reason::reserved_mask) != 0) {
        fail(error_category::invalid_input, "fits.invalid_reserved_bits", "bit riservato acceso in INVALID",
             {{"indice", std::to_string(first + i)}});
      }
      bits[i] = b;
    }
  } else {
    std::fill(bits, bits + count, std::uint16_t{0});
  }
  for (std::size_t i = 0; i < count; ++i) {
    if (!std::isfinite(values[i]) && bits[i] == 0) bits[i] = invalid_reason::source_nonfinite;
  }
}

}  // namespace astro
