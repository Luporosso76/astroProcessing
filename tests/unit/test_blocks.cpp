// T-M2R3-01 (ADR-06, estensione R3.2): lettore a blocchi contro decode_fits, digest dei byte consumati, FITS in
// streaming contro encode_fits, selezione esatta contro median_of.
#include <fitsio.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>

#include <unistd.h>

#include "astro/fits.hpp"
#include "astro/fits_blocks.hpp"
#include "astro/hash.hpp"
#include "calibration_internal.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;
namespace fs = std::filesystem;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 30;

fs::path fresh_dir(const std::string& name) {
  const fs::path dir = fs::path(ASTRO_TEST_TMP) / "blocks" / name;
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

void write_file(const fs::path& path, const fx::bytes& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void overwrite_byte(const fs::path& path, std::uint64_t offset, std::uint8_t value) {
  std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
  f.seekp(static_cast<std::streamoff>(offset));
  f.put(static_cast<char>(value));
}

struct encoding_case {
  int bitpix;
  double bscale;
  double bzero;
};

// Campione grezzo pseudo-casuale nel dominio del BITPIX, in big-endian.
void put_raw(fx::bytes& out, int bitpix, std::mt19937_64& rng, std::size_t i) {
  switch (bitpix) {
    case 8: fx::put_be(out, rng() & 0xFF, 1); break;
    case 16: fx::put_be(out, rng() & 0xFFFF, 2); break;
    case 32: fx::put_be(out, rng() & 0xFFFFFFFF, 4); break;
    case 64: {
      const auto v = static_cast<std::int64_t>(rng() % (std::uint64_t{1} << 53)) - (std::int64_t{1} << 52);
      fx::put_be(out, static_cast<std::uint64_t>(v), 8);
      break;
    }
    case -32: {
      float v = static_cast<float>(static_cast<std::int64_t>(rng() % 200001) - 100000) / 7.0f;
      if (i % 11 == 3) v = std::numeric_limits<float>::quiet_NaN();
      if (i % 13 == 5) v = std::numeric_limits<float>::infinity();
      fx::put_be(out, fx::bits_of(v), 4);
      break;
    }
    default: {
      double v = static_cast<double>(static_cast<std::int64_t>(rng() % 2000001) - 1000000) / 3.0;
      if (i % 9 == 2) v = std::numeric_limits<double>::quiet_NaN();
      fx::put_be(out, fx::bits_of(v), 8);
    }
  }
}

struct built {
  fx::bytes file;
  bool has_invalid = false;
  bool has_variance = false;
};

built build(const encoding_case& e, std::uint32_t w, std::uint32_t h, bool invalid, bool variance, std::uint64_t seed,
            std::uint16_t extra_bits = 0) {
  std::mt19937_64 rng(seed);
  fx::raw_hdu p;
  p.bitpix = e.bitpix;
  p.axes = {w, h};
  if (e.bscale != 1.0) p.cards.push_back(fx::card_real("BSCALE", e.bscale));
  if (e.bzero != 0.0) p.cards.push_back(fx::card_real("BZERO", e.bzero));
  p.cards.push_back(fx::card_real("EXPTIME", 60.0));
  p.cards.push_back(fx::card_str("FILTER", "L"));
  const std::size_t n = static_cast<std::size_t>(w) * h;
  for (std::size_t i = 0; i < n; ++i) put_raw(p.data, e.bitpix, rng, i);
  std::vector<fx::raw_hdu> hdus = {p};
  if (invalid) {
    fx::raw_hdu m;
    m.primary = false;
    m.bitpix = 16;
    m.axes = {w, h};
    m.cards = {fx::card_real("BZERO", 32768.0), fx::card_str("EXTNAME", "INVALID")};
    for (std::size_t i = 0; i < n; ++i) {
      std::uint16_t bits = i % 5 == 1 ? invalid_reason::source_flagged : 0;
      if (i == n / 2) bits |= extra_bits;
      fx::put_be(m.data, static_cast<std::uint16_t>(bits ^ 0x8000u), 2);
    }
    hdus.push_back(m);
  }
  const bool f32 = ((e.bitpix == 8 || e.bitpix == 16) && e.bscale == 1.0) || e.bitpix == -32;
  if (variance) {
    fx::raw_hdu v;
    v.primary = false;
    v.bitpix = f32 ? -32 : -64;
    v.axes = {w, h};
    v.cards = {fx::card_str("EXTNAME", "VARIANCE")};
    for (std::size_t i = 0; i < n; ++i) {
      const double var = static_cast<double>(i % 17) * 0.25;
      if (f32) fx::put_be(v.data, fx::bits_of(static_cast<float>(var)), 4);
      else fx::put_be(v.data, fx::bits_of(var), 8);
    }
    hdus.push_back(v);
  }
  return {fx::build_fits(hdus), invalid, variance};
}

// Byte dei campioni di righe [y0, y1) dell'immagine di riferimento.
template <class T>
void expect_band_equals(const image& ref, const row_band& b, bool variance) {
  const std::size_t w = ref.geometry.width;
  const std::size_t first = static_cast<std::size_t>(b.row_begin) * w;
  const std::size_t count = static_cast<std::size_t>(b.row_end - b.row_begin) * w;
  EXPECT_EQ(std::memcmp(ref.data<T>() + first, b.samples, count * sizeof(T)), 0) << "campioni righe " << b.row_begin;
  EXPECT_EQ(std::memcmp(ref.invalid.bits.data() + first, b.mask, count * 2), 0) << "maschera righe " << b.row_begin;
  if (variance) {
    EXPECT_EQ(std::memcmp(ref.variance_data<T>() + first, b.variance, count * sizeof(T)), 0) << "varianza righe " << b.row_begin;
  }
}

void expect_header_equals(const image& ref, const image& h) {
  EXPECT_EQ(h.geometry, ref.geometry);
  EXPECT_EQ(h.layout, ref.layout);
  EXPECT_EQ(h.type, ref.type);
  EXPECT_EQ(h.unit, ref.unit);
  EXPECT_EQ(h.lin, ref.lin);
  EXPECT_EQ(h.phase, ref.phase);
  EXPECT_EQ(h.cfa, ref.cfa);
  EXPECT_EQ(h.metadata, ref.metadata);
  EXPECT_EQ(h.header_cards, ref.header_cards);
  EXPECT_EQ(h.uninterpreted_keywords, ref.uninterpreted_keywords);
  EXPECT_EQ(h.encoding, ref.encoding);
  EXPECT_EQ(h.variance.has_value(), ref.variance.has_value());
  EXPECT_EQ(h.samples.size(), 0u);
  EXPECT_TRUE(h.invalid.bits.empty());
}

void read_all_tiles(const fits_block_source& src, const image& ref, bool variance) {
  const std::size_t w = ref.geometry.width;
  const std::size_t e = sample_size(ref.type);
  const std::size_t t = src.tile_rows();
  std::vector<std::byte> raw(t * src.raw_bytes_per_row());
  std::vector<std::byte> samples(t * w * e), var(t * w * e);
  std::vector<std::uint16_t> mask(t * w);
  for (std::uint64_t k = 0; k < src.tile_count(); ++k) {
    row_band b;
    b.samples = samples.data();
    b.mask = mask.data();
    b.variance = variance ? var.data() : nullptr;
    src.read_tile(k, raw, b);
    EXPECT_EQ(b.row_begin, k * t);
    EXPECT_EQ(b.row_end, std::min<std::uint64_t>(ref.geometry.height, (k + 1) * t));
    if (ref.type == sample_type::f32) expect_band_equals<float>(ref, b, variance);
    else expect_band_equals<double>(ref, b, variance);
  }
}

// ---------------------------------------------------------------- lettore contro decode_fits

TEST(Blocks, T_M2R3_01_row_reader_matches_decode_fits_on_every_encoding) {
  const fs::path dir = fresh_dir("encodings");
  const encoding_case cases[] = {{8, 1.0, 0.0},  {8, 1.0, -128.0}, {16, 1.0, 0.0},  {16, 1.0, 32768.0},
                                 {16, 2.0, 3.0}, {32, 1.0, 0.0},   {64, 1.0, 0.0},  {-32, 1.0, 0.0},
                                 {-64, 1.0, 0.0}};
  int index = 0;
  for (const encoding_case& e : cases) {
    for (const bool extensions : {false, true}) {
      SCOPED_TRACE("BITPIX " + std::to_string(e.bitpix) + " BSCALE " + std::to_string(e.bscale) + " BZERO " +
                   std::to_string(e.bzero) + (extensions ? " con INVALID e VARIANCE" : ""));
      const std::uint32_t w = 7, h = 10;
      const built b = build(e, w, h, extensions, extensions, 100 + index);
      const fs::path path = dir / ("f" + std::to_string(index++) + ".fits");
      write_file(path, b.file);
      const image ref = decode_fits(b.file, big_budget);
      for (const std::uint32_t t : {1u, 3u, h}) {
        fits_block_source src(path.string(), digest_of(b.file), {}, t, extensions);
        std::vector<std::byte> work(2880);
        src.verify(work);
        expect_header_equals(ref, src.header());
        EXPECT_EQ(src.tile_count(), (h + t - 1) / t);
        EXPECT_EQ(src.has_invalid(), extensions);
        EXPECT_EQ(src.reads_variance(), extensions);
        const std::uint64_t e_out = sample_size(ref.type);
        const std::uint64_t b_in = static_cast<std::uint64_t>(std::abs(e.bitpix) / 8);
        EXPECT_EQ(src.raw_bytes_per_row(), w * (b_in + (extensions ? 2 + e_out : 0)));
        EXPECT_EQ(src.band_bytes_per_row(), w * (e_out + 2 + (extensions ? e_out : 0)));
        EXPECT_EQ(src.region_count(), extensions ? 3u : 1u);
        read_all_tiles(src, ref, extensions);
        src.check_identity();
      }
    }
  }
}

stop_error capture(const std::function<void()>& f) {
  try {
    f();
  } catch (const stop_error& e) {
    return e;
  }
  ADD_FAILURE() << "stop_error atteso";
  return stop_error(error_category::integrity, "test.no_error", "nessun errore");
}

std::string context_value(const stop_error& e, const std::string& key) {
  for (const auto& [k, v] : e.context()) {
    if (k == key) return v;
  }
  return "<assente>";
}

// Stesso arresto di decode_fits: categoria, codice e contesto identici (presenza e valore di `indice`, `hdu`).
void expect_same_stop(const fx::bytes& file, const fs::path& path, bool read_variance = true) {
  write_file(path, file);
  const stop_error whole = capture([&] { (void)decode_fits(file, big_budget); });
  const stop_error blocks = capture([&] {
    fits_block_source src(path.string(), digest_of(file), {}, 2, read_variance);
    std::vector<std::byte> work(4096);
    src.verify(work);
  });
  EXPECT_EQ(blocks.category(), whole.category()) << blocks.what() << " / " << whole.what();
  EXPECT_EQ(blocks.code(), whole.code()) << blocks.what() << " / " << whole.what();
  EXPECT_EQ(blocks.context(), whole.context()) << blocks.what() << " / " << whole.what();
}

fx::bytes with_checksums(const fx::bytes& file) {
  std::size_t size = file.size();
  void* mem = std::malloc(size);
  std::memcpy(mem, file.data(), size);
  fitsfile* f = nullptr;
  int st = 0;
  fits_open_memfile(&f, "chk", READWRITE, &mem, &size, 2880, std::realloc, &st);
  int hdus = 0;
  fits_get_num_hdus(f, &hdus, &st);
  for (int k = 1; k <= hdus; ++k) {
    int type = 0;
    fits_movabs_hdu(f, k, &type, &st);
    fits_write_chksum(f, &st);
  }
  fits_close_file(f, &st);
  EXPECT_EQ(st, 0) << "CFITSIO status " << st;
  fx::bytes out(static_cast<const std::uint8_t*>(mem), static_cast<const std::uint8_t*>(mem) + size);
  std::free(mem);
  return out;
}

std::uint64_t data_offset_of(const fx::bytes& file, std::size_t hdu) {
  std::uint64_t off = 0;
  for (std::size_t k = 0;; ++k) {
    std::uint64_t naxis1 = 0, naxis2 = 1, bitpix = 0;
    bool end = false;
    while (!end) {
      for (int c = 0; c < 36; ++c) {
        const std::string card(reinterpret_cast<const char*>(file.data() + off + c * 80), 80);
        if (card.compare(0, 8, "END     ") == 0) end = true;
        if (card.compare(0, 8, "BITPIX  ") == 0) bitpix = static_cast<std::uint64_t>(std::abs(std::stoi(card.substr(10, 20))));
        if (card.compare(0, 8, "NAXIS1  ") == 0) naxis1 = std::stoull(card.substr(10, 20));
        if (card.compare(0, 8, "NAXIS2  ") == 0) naxis2 = std::stoull(card.substr(10, 20));
      }
      off += 2880;
    }
    if (k == hdu) return off;
    off += (naxis1 * naxis2 * bitpix / 8 + 2879) / 2880 * 2880;
  }
}

TEST(Blocks, T_M2R3_01_reader_stops_like_decode_fits) {
  const fs::path dir = fresh_dir("stops");
  // BSCALE non invertibile sul campione: fits.scaling_not_exact con lo stesso indice.
  expect_same_stop(build({16, 1e-300, 1e10}, 5, 4, false, false, 7).file, dir / "scaling.fits");
  EXPECT_NE(context_value(capture([&] { (void)decode_fits(build({16, 1e-300, 1e10}, 5, 4, false, false, 7).file, big_budget); }),
                          "indice"),
            "<assente>");
  // BITPIX 64 oltre 2^53: fits.int64_range.
  {
    fx::raw_hdu p;
    p.bitpix = 64;
    p.axes = {3, 2};
    for (int i = 0; i < 6; ++i) fx::put_be(p.data, i == 4 ? (std::uint64_t{1} << 60) : 5u, 8);
    expect_same_stop(fx::build_fits({p}), dir / "int64.fits");
  }
  // Bit riservato in INVALID: fits.invalid_reserved_bits con lo stesso indice.
  expect_same_stop(build({-32, 1.0, 0.0}, 6, 5, true, true, 9, 0x0200).file, dir / "reserved.fits");
  // DATASUM errato e campioni non invertibili: prima il checksum, come decode_fits.
  {
    fx::bytes file = build({16, 1e-300, 1e10}, 5, 4, false, false, 11).file;
    fx::raw_hdu p;
    p.bitpix = 16;
    p.axes = {5, 4};
    p.cards = {fx::card_real("BSCALE", 1e-300), fx::card_real("BZERO", 1e10), fx::card_str("DATASUM", "12345")};
    const fx::raw_primary_view v = fx::parse_hdu(file);
    p.data = v.data;
    expect_same_stop(fx::build_fits({p}), dir / "datasum_first.fits");
  }
}

TEST(Blocks, T_M2R3_01_checksums_are_verified_like_decode_fits) {
  const fs::path dir = fresh_dir("checksum");
  const fx::bytes good = with_checksums(build({-32, 1.0, 0.0}, 9, 6, true, true, 21).file);
  ASSERT_NE(std::string(good.begin(), good.end()).find("CHECKSUM"), std::string::npos);
  write_file(dir / "good.fits", good);
  const image ref = decode_fits(good, big_budget);
  fits_block_source src((dir / "good.fits").string(), digest_of(good), {}, 4, true);
  std::vector<std::byte> work(2880);
  src.verify(work);
  read_all_tiles(src, ref, true);
  // Un byte di dati alterato nell'HDU VARIANCE (non letto dai blocchi se non richiesto) e uno nell'header primario.
  // VARIANCE presente ma non richiesta: P0 controlla comunque digest e checksum dell'intero file.
  {
    fits_block_source unread((dir / "good.fits").string(), digest_of(good), {}, 4, false);
    unread.verify(work);
    EXPECT_FALSE(unread.reads_variance());
    EXPECT_EQ(unread.region_count(), 2u);
    read_all_tiles(unread, ref, false);
  }
  fx::bytes bad_data = good;
  bad_data[data_offset_of(good, 2) + 3] ^= 0x01;
  expect_same_stop(bad_data, dir / "bad_data.fits");
  expect_same_stop(bad_data, dir / "bad_data_unread.fits", false);
  fx::bytes bad_header = good;
  const std::string text(good.begin(), good.begin() + 2880);
  const std::size_t filter = text.find("FILTER  = 'L");
  ASSERT_NE(filter, std::string::npos);
  bad_header[filter + 11] = 'M';
  expect_same_stop(bad_header, dir / "bad_header.fits");
  expect_same_stop(bad_header, dir / "bad_header_unread.fits", false);
}

// ---------------------------------------------------------------- digest dei byte consumati

TEST(Blocks, T_M2R3_01_consumed_bytes_are_verified_against_the_record) {
  const fs::path dir = fresh_dir("consumed");
  const built b = build({-32, 1.0, 0.0}, 8, 9, true, true, 31);
  const fs::path path = dir / "in.fits";
  write_file(path, b.file);
  std::vector<std::byte> work(2880);
  // Digest della ricetta diverso dal file: P0 si ferma prima di ogni blocco.
  {
    content_digest wrong = digest_of(b.file);
    wrong.sha256[0] = wrong.sha256[0] == 'a' ? 'b' : 'a';
    fits_block_source src(path.string(), wrong, {}, 3, true);
    EXPECT_STOP(src.verify(work), error_category::integrity, "input.digest_mismatch");
  }
  // Byte alterato dopo P0 in ciascuna regione letta (primario, INVALID, VARIANCE): il blocco che lo contiene si ferma
  // con input, blocco e regione nel contesto; il blocco precedente resta leggibile.
  const std::pair<std::size_t, const char*> regions[] = {{0, "primary"}, {1, "INVALID"}, {2, "VARIANCE"}};
  for (const auto& [hdu, name] : regions) {
    SCOPED_TRACE(name);
    write_file(path, b.file);
    fits_block_source src(path.string(), digest_of(b.file), {}, 3, true);
    src.verify(work);
    const std::uint64_t bytes_per_sample = hdu == 1 ? 2 : 4;
    const std::uint64_t offset = data_offset_of(b.file, hdu) + 8 * 4 * bytes_per_sample;  // riga 4: blocco 1
    // Valore alterato ma valido per la regione (nessun bit riservato in INVALID): solo il digest lo rileva.
    overwrite_byte(path, offset + bytes_per_sample - 1, static_cast<std::uint8_t>(b.file[offset + bytes_per_sample - 1] ^ 0x01));
    std::vector<std::byte> raw(3 * src.raw_bytes_per_row()), samples(3 * 8 * 4), var(3 * 8 * 4);
    std::vector<std::uint16_t> mask(3 * 8);
    row_band band{0, 0, samples.data(), mask.data(), var.data()};
    src.read_tile(0, raw, band);
    const stop_error e = capture([&] { src.read_tile(1, raw, band); });
    EXPECT_EQ(e.category(), error_category::integrity);
    EXPECT_EQ(e.code(), "input.consumed_digest");
    EXPECT_EQ(context_value(e, "input"), path.string());
    EXPECT_EQ(context_value(e, "blocco"), "1");
    EXPECT_EQ(context_value(e, "regione"), name);
    EXPECT_STOP(src.check_identity(), error_category::integrity, "input.modified");
  }
  write_file(path, b.file);
  // Header analizzato diverso da quello di P0 (file sostituito fra apertura e P0 con lo stesso digest atteso).
  {
    fx::bytes other = b.file;
    const std::string text(other.begin(), other.begin() + 2880);
    const std::size_t filter = text.find("FILTER  = 'L");
    ASSERT_NE(filter, std::string::npos);
    other[filter + 11] = 'R';
    fits_block_source src(path.string(), digest_of(other), {}, 3, true);
    write_file(path, other);
    EXPECT_STOP(src.verify(work), error_category::integrity, "input.modified");
    write_file(path, b.file);
  }
  // File rimosso o sostituito prima della pubblicazione.
  {
    fits_block_source src(path.string(), digest_of(b.file), {}, 3, true);
    src.verify(work);
    fs::remove(path);
    EXPECT_STOP(src.check_identity(), error_category::io, "input.missing");
    write_file(path, b.file);
    EXPECT_STOP(src.check_identity(), error_category::integrity, "input.modified");
  }
}

// ---------------------------------------------------------------- FITS in streaming contro encode_fits

image header_only(const image& img) {
  image h;
  h.geometry = img.geometry;
  h.layout = img.layout;
  h.type = img.type;
  h.unit = img.unit;
  h.lin = img.lin;
  h.lin_origin = img.lin_origin;
  h.phase = img.phase;
  h.cfa = img.cfa;
  if (img.variance) h.variance = variance_plane{};
  h.metadata = img.metadata;
  h.header_cards = img.header_cards;
  h.uninterpreted_keywords = img.uninterpreted_keywords;
  h.encoding = img.encoding;
  return h;
}

// Staging indipendente dal prodotto: campioni, maschera ^ 0x8000 e varianza in big-endian, nell'ordine del contratto.
template <class T>
std::vector<std::byte> staging_of(const image& img) {
  fx::bytes out;
  const std::size_t n = img.geometry.sample_count();
  for (std::size_t i = 0; i < n; ++i) fx::put_be(out, fx::bits_of(img.data<T>()[i]), sizeof(T));
  for (std::size_t i = 0; i < n; ++i) fx::put_be(out, static_cast<std::uint16_t>(img.invalid.bits[i] ^ 0x8000u), 2);
  for (std::size_t i = 0; i < n; ++i) fx::put_be(out, fx::bits_of(img.variance_data<T>()[i]), sizeof(T));
  std::vector<std::byte> s(out.size());
  std::memcpy(s.data(), out.data(), out.size());
  return s;
}

template <class T>
void fill(image& img, bool invalid) {
  const std::size_t n = img.geometry.sample_count();
  for (std::size_t i = 0; i < n; ++i) {
    img.data<T>()[i] = static_cast<T>(static_cast<double>(i % 1000) * 0.37 - 11.0);
    img.variance_data<T>()[i] = static_cast<T>(static_cast<double>(i % 7) + 0.5);
    if (invalid && i % 9 == 4) {
      img.invalid.bits[i] = invalid_reason::q_below_min | invalid_reason::saturated;
      img.data<T>()[i] = std::numeric_limits<T>::quiet_NaN();
      img.variance_data<T>()[i] = std::numeric_limits<T>::quiet_NaN();
    }
  }
}

TEST(Blocks, T_M2R3_01_streamed_fits_is_byte_identical_to_encode_fits) {
  fx::calib_header hdr;
  hdr.bayer = "RGGB";
  hdr.bayer_x = 1;
  const fx::bytes light = fx::float_frame(5, 4, -32, std::vector<double>(20, 1.0), fx::calib_cards(hdr));
  const image meta = decode_fits(light, big_budget);
  for (const sample_type type : {sample_type::f32, sample_type::f64}) {
    for (const bool invalid : {false, true}) {
      for (const std::uint32_t height : {1u, 5u, 701u}) {
        SCOPED_TRACE(std::string(type == sample_type::f32 ? "f32" : "f64") + (invalid ? " con INVALID" : "") +
                     " altezza " + std::to_string(height));
        image_geometry geo(13, height, 1);
        geo.binning_x = meta.geometry.binning_x;
        geo.binning_y = meta.geometry.binning_y;
        image img = make_image(geo, type, meta.layout, meta.cfa);
        img.unit = meta.unit;
        img.lin = meta.lin;
        img.lin_origin = meta.lin_origin;
        img.phase = meta.phase;
        img.metadata = meta.metadata;
        img.header_cards = meta.header_cards;
        img.uninterpreted_keywords = meta.uninterpreted_keywords;
        img.variance = variance_plane{aligned_buffer(img.samples.size())};
        if (type == sample_type::f32) fill<float>(img, invalid);
        else fill<double>(img, invalid);
        const std::vector<std::uint8_t> expected = encode_fits(img);
        const std::vector<std::byte> staging =
            type == sample_type::f32 ? staging_of<float>(img) : staging_of<double>(img);
        const fits_stream_writer writer(header_only(img), invalid);
        EXPECT_EQ(writer.file_bytes(), expected.size());
        auto reader = writer.reader([&](std::uint64_t offset, std::span<std::byte> dst) {
          ASSERT_LE(offset + dst.size(), staging.size());
          std::memcpy(dst.data(), staging.data() + offset, dst.size());
        });
        std::vector<std::uint8_t> got;
        std::vector<std::uint8_t> chunk(1000);
        while (const std::size_t n = reader(chunk)) got.insert(got.end(), chunk.begin(), chunk.begin() + n);
        EXPECT_EQ(got.size(), expected.size());
        EXPECT_TRUE(got == expected) << "byte diversi da encode_fits";
        // Lo staging contiene float o double: un encoding intero di origine non si serializza in streaming.
        image integer_origin = header_only(img);
        integer_origin.encoding = source_encoding{16, 32768.0, 1.0, 2};
        EXPECT_STOP(fits_stream_writer(integer_origin, invalid), error_category::invalid_input, "fits.encoding");
        integer_origin.encoding = source_encoding{type == sample_type::f32 ? -32 : -64, 0.0, 1.0, 2};
        EXPECT_NO_THROW(fits_stream_writer(integer_origin, invalid));
      }
    }
  }
}

// ---------------------------------------------------------------- selezione esatta contro median_of

double select(const std::vector<double>& v, std::uint64_t rank) {
  calib_detail::kth_selector s;
  while (!s.done()) {
    for (const double x : v) s.add(x);
    s.end_pass(rank);
  }
  return s.result();
}

std::uint64_t bits(double d) {
  std::uint64_t u = 0;
  std::memcpy(&u, &d, 8);
  return u;
}

TEST(Blocks, T_M2R3_01_exact_selection_equals_the_sorted_rank_and_median_of) {
  std::mt19937_64 rng(5);
  const double specials[] = {0.0, -0.0, 1e-310, -1e-310, 1e300, -1e300, 3.0, 3.0, -2.5, 0.1};
  for (const std::size_t n : {1u, 2u, 3u, 4u, 10u, 257u, 1000u, 1001u}) {
    std::vector<double> v;
    for (std::size_t i = 0; i < n; ++i) {
      if (i % 4 == 0) v.push_back(specials[rng() % 10]);
      else v.push_back(static_cast<double>(static_cast<std::int64_t>(rng() % 2001) - 1000) / 8.0);
    }
    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    for (std::uint64_t k = 0; k < n; k += std::max<std::size_t>(1, n / 7)) {
      const double got = select(v, k);
      const double want = sorted[k] == 0 ? 0.0 : sorted[k];
      EXPECT_EQ(bits(got), bits(want)) << "n " << n << " rank " << k;
    }
    const std::size_t mid = n / 2;
    const double hi = select(v, mid);
    const double median = n % 2 == 1 ? hi : (select(v, mid - 1) + hi) * 0.5;
    std::vector<double> copy = v;
    const double whole = calib_detail::median_of(copy);
    EXPECT_EQ(bits(median == 0 ? 0.0 : median), bits(whole)) << "n " << n;
  }
  // Solo zeri con segno misto: +0 in entrambe le residenze.
  std::vector<double> zeros = {-0.0, 0.0, -0.0, -0.0};
  EXPECT_EQ(bits(select(zeros, 1)), bits(0.0));
  EXPECT_EQ(bits(calib_detail::median_of(zeros)), bits(0.0));
}

}  // namespace
