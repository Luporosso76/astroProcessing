#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

#include <optional>

#include "astro/fits.hpp"
#include "fits_internal.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;
namespace fs = std::filesystem;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 40;

image decode(const fx::bytes& b, const fits_read_options& o = {}) { return decode_fits(b, big_budget, o); }

image read_file(const std::string& path) {
  fits_input input = open_fits_input(path);
  return decode(read_fits_bytes(input, big_budget));
}

std::string tmp_dir() {
  fs::path p = fs::path(ASTRO_TEST_TMP) / "fits";
  fs::create_directories(p);
  return p.string();
}

std::string tmp_file(const std::string& name) { return (fs::path(tmp_dir()) / name).string(); }

void write_file(const std::string& path, const fx::bytes& b) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  ASSERT_TRUE(out.good());
}

double sample(const image& img, std::size_t i) {
  if (img.type == sample_type::f32) return img.data<float>()[i];
  return img.data<double>()[i];
}

fx::raw_hdu raw_image(int bitpix, std::vector<std::uint64_t> axes, std::vector<std::string> cards = {}) {
  fx::raw_hdu h;
  h.bitpix = bitpix;
  h.axes = std::move(axes);
  h.cards = std::move(cards);
  std::uint64_t n = 1;
  for (auto a : h.axes) n *= a;
  h.data.assign(n * static_cast<std::uint64_t>(std::abs(bitpix) / 8), 0);
  return h;
}

fx::raw_hdu extension(const std::string& extname, int bitpix, std::vector<std::uint64_t> axes) {
  fx::raw_hdu h = raw_image(bitpix, std::move(axes), {fx::card_str("EXTNAME", extname)});
  h.primary = false;
  return h;
}

std::int64_t raw_int(int bitpix, std::size_t i) {
  const auto k = static_cast<std::int64_t>(i);
  switch (bitpix) {
    case 8: return i == 0 ? 0 : i == 1 ? 255 : (k * 37) % 256;
    case 16: return i == 0 ? -32768 : i == 1 ? 32767 : ((k * 7919) % 65536) - 32768;
    case 32:
      return i == 0 ? INT32_MIN : i == 1 ? INT32_MAX : ((k * 104729 * 4099) % 4294967296LL) - 2147483648LL;
    default: {
      const std::int64_t p53 = std::int64_t{1} << 53;
      return i == 0 ? -p53 : i == 1 ? p53 : (k * 1234567891LL) % (std::int64_t{1} << 40) - (std::int64_t{1} << 39);
    }
  }
}

struct encoding_case {
  int bitpix;
  double bscale;
  double bzero;
  sample_type type;
};

TEST(Fits, T_M2A_02_roundtrip_is_bit_exact_for_every_bitpix) {
  const encoding_case cases[] = {
      {8, 1, 0, sample_type::f32},        {8, 1, -128, sample_type::f32},
      {8, 2, 5, sample_type::f64},        {16, 1, 0, sample_type::f32},
      {16, 1, 32768, sample_type::f32},   {16, 0.5, 100, sample_type::f64},
      {16, 1, 100, sample_type::f64},     {32, 1, 0, sample_type::f64},
      {32, 1, 2147483648.0, sample_type::f64}, {64, 1, 0, sample_type::f64},
  };
  constexpr std::uint64_t w = 7, h = 5, n = w * h;
  for (const auto& c : cases) {
    SCOPED_TRACE("bitpix " + std::to_string(c.bitpix) + " bscale " + std::to_string(c.bscale) + " bzero " +
                 std::to_string(c.bzero));
    std::vector<std::string> cards;
    if (c.bscale != 1 || c.bzero != 0) {
      cards = {fx::card_real("BSCALE", c.bscale), fx::card_real("BZERO", c.bzero)};
    }
    fx::raw_hdu hdu = raw_image(c.bitpix, {w, h}, cards);
    hdu.data.clear();
    for (std::size_t i = 0; i < n; ++i) {
      fx::put_be(hdu.data, static_cast<std::uint64_t>(raw_int(c.bitpix, i)), c.bitpix / 8);
    }
    const fx::bytes file = fx::build_fits({hdu});

    image img = decode(file);
    EXPECT_EQ(img.type, c.type);
    ASSERT_TRUE(img.encoding.has_value());
    EXPECT_EQ(img.encoding->bitpix, c.bitpix);
    EXPECT_EQ(img.encoding->bscale, c.bscale);
    EXPECT_EQ(img.encoding->bzero, c.bzero);
    for (std::size_t i = 0; i < n; ++i) {
      EXPECT_EQ(sample(img, i), static_cast<double>(raw_int(c.bitpix, i)) * c.bscale + c.bzero) << "i=" << i;
    }
    const fx::bytes out = encode_fits(img);
    fx::raw_primary_view v = fx::parse_hdu(out);
    EXPECT_EQ(v.bitpix, c.bitpix);
    EXPECT_EQ(v.data, hdu.data);
    EXPECT_TRUE(decode(out) == img);
  }
}

TEST(Fits, T_M2A_02_float_roundtrip_preserves_every_bit_pattern) {
  constexpr std::uint64_t n = 12;
  const std::uint32_t f32_patterns[n] = {0x7fc00001u, 0x7f800000u, 0xff800000u, 0x80000000u,
                                         0x00000001u, 0x007fffffu, 0x3f800000u, 0xc2f6e979u,
                                         0x7f7fffffu, 0x7fa00000u, 0xffc12345u, 0x00000000u};
  const std::uint64_t f64_patterns[n] = {0x7ff8000000000001ull, 0x7ff0000000000000ull, 0xfff0000000000000ull,
                                         0x8000000000000000ull, 0x0000000000000001ull, 0x000fffffffffffffull,
                                         0x3ff0000000000000ull, 0xc05edd2f1a9fbe77ull, 0x7fefffffffffffffull,
                                         0x7ff4000000000000ull, 0xfff8badc0ffee000ull, 0x0000000000000000ull};
  for (int bitpix : {-32, -64}) {
    SCOPED_TRACE(bitpix);
    fx::raw_hdu hdu = raw_image(bitpix, {4, 3});
    hdu.data.clear();
    for (std::size_t i = 0; i < n; ++i) {
      if (bitpix == -32) fx::put_be(hdu.data, f32_patterns[i], 4);
      else fx::put_be(hdu.data, f64_patterns[i], 8);
    }
    image img = decode(fx::build_fits({hdu}));
    EXPECT_EQ(img.type, bitpix == -32 ? sample_type::f32 : sample_type::f64);
    for (std::size_t i = 0; i < n; ++i) {
      if (bitpix == -32) {
        EXPECT_EQ(fx::bits_of(img.data<float>()[i]), f32_patterns[i]) << i;
      } else {
        EXPECT_EQ(fx::bits_of(img.data<double>()[i]), f64_patterns[i]) << i;
      }
    }
    EXPECT_EQ(fx::parse_hdu(encode_fits(img)).data, hdu.data);
  }
}

TEST(Fits, T_M2A_02_out_of_contract_encodings_are_rejected) {
  auto with = [](int bitpix, std::vector<std::string> cards, std::int64_t first = 0) {
    fx::raw_hdu h = raw_image(bitpix, {2, 2}, std::move(cards));
    h.data.clear();
    for (int i = 0; i < 4; ++i) fx::put_be(h.data, static_cast<std::uint64_t>(i == 0 ? first : 0), std::abs(bitpix) / 8);
    return fx::build_fits({h});
  };
  EXPECT_STOP(decode(with(64, {}, (std::int64_t{1} << 53) + 1)), error_category::unsupported,
              "fits.int64_range");
  EXPECT_STOP(decode(with(64, {}, -(std::int64_t{1} << 53) - 1)), error_category::unsupported,
              "fits.int64_range");
  EXPECT_STOP(decode(with(64, {fx::card_real("BZERO", 9223372036854775808.0)})),
              error_category::unsupported, "fits.int64_scaling");
  EXPECT_STOP(decode(with(-32, {fx::card_real("BZERO", 1.0)})), error_category::unsupported,
              "fits.float_scaling");
  EXPECT_STOP(decode(with(-64, {fx::card_real("BSCALE", 2.0)})), error_category::unsupported,
              "fits.float_scaling");
  EXPECT_STOP(decode(with(16, {fx::card_real("BSCALE", 0.0)})), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with(16, {fx::card_int("BLANK", -32768)})), error_category::unsupported, "fits.blank");
}

TEST(Fits, T_M2A_02_writer_refuses_values_the_encoding_cannot_hold) {
  fx::raw_hdu hdu = raw_image(16, {2, 2}, {fx::card_real("BZERO", 32768.0)});
  image img = decode(fx::build_fits({hdu}));
  img.data<float>()[1] = 70000.0f;
  EXPECT_STOP(encode_fits(img), error_category::numeric, "fits.value_out_of_range");
  img.data<float>()[1] = std::numeric_limits<float>::quiet_NaN();
  EXPECT_STOP(encode_fits(img), error_category::numeric, "fits.nonfinite_in_integer_encoding");
  img.data<float>()[1] = 10.4f;
  EXPECT_STOP(encode_fits(img), error_category::numeric, "fits.precision_loss");
  img.data<float>()[1] = 10.0f;
  EXPECT_EQ(decode(encode_fits(img)).data<float>()[1], 10.0f);
  img.data<float>()[1] = 10.4f;
  img.encoding->bitpix = -32;
  img.encoding->bzero = 0;
  EXPECT_EQ(decode(encode_fits(img)).data<float>()[1], 10.4f);
  img.encoding->bitpix = -64;
  EXPECT_STOP(encode_fits(img), error_category::invalid_input, "fits.encoding");
}

TEST(Fits, T_M2A_03_cards_are_kept_in_order_and_uninterpreted_are_listed) {
  const std::vector<std::string> extra = {
      fx::card_str("OBJECT", "M31"),
      fx::card_text("COMMENT", "prima nota"),
      fx::card_real("EXPTIME", 12.5),
      fx::card_text("HISTORY", "una storia"),
      fx::card_int("MYKEY", 42),
      fx::card_text("HIERARCH", "ESO DET CHIP1 ID = 'abc'"),
      fx::card_text("        ", ""),
      fx::card_str("OBJECT", "duplicato"),
      fx::card_str("BUNIT", "ADU"),
      fx::card_real("MYKEY2", 1.5),
  };
  std::vector<std::string> cards = {fx::card_real("BSCALE", 1.0), fx::card_real("BZERO", 32768.0)};
  cards.insert(cards.end(), extra.begin(), extra.end());
  image img = decode(fx::build_fits({raw_image(16, {3, 3}, cards)}));
  EXPECT_EQ(img.header_cards, extra);
  const std::vector<std::string> expected_uninterpreted = {"OBJECT", "COMMENT", "HISTORY", "MYKEY",
                                                           "HIERARCH", "MYKEY2"};
  EXPECT_EQ(img.uninterpreted_keywords, expected_uninterpreted);
  EXPECT_EQ(img.unit, pixel_unit::adu);
  EXPECT_EQ(img.metadata.exposure_s->value, 12.5);

  image again = decode(encode_fits(img));
  EXPECT_EQ(again.header_cards, extra);
  EXPECT_EQ(again.uninterpreted_keywords, expected_uninterpreted);
  // le card riscritte sono verbatim nel file
  fx::raw_primary_view v = fx::parse_hdu(encode_fits(img));
  std::vector<std::string> tail(v.cards.end() - static_cast<long>(extra.size()), v.cards.end());
  EXPECT_EQ(tail, extra);
}

TEST(Fits, T_M2A_03_writer_rejects_malformed_or_structural_cards) {
  image img = decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_real("BZERO", 32768.0)})}));
  img.header_cards = {"CORTA"};
  EXPECT_STOP(encode_fits(img), error_category::invalid_input, "fits.card_length");
  img.header_cards = {fx::card_int("NAXIS", 3)};
  EXPECT_STOP(encode_fits(img), error_category::invalid_input, "fits.card_structural");
}

TEST(Fits, T_M2A_04_missing_keywords_are_nullopt_and_present_ones_carry_their_source) {
  fx::synth_frame light = fx::make_synth_frame(fx::frame_kind::light, 8, 6, 3);
  image img = decode(light.file);
  const auto& m = img.metadata;
  ASSERT_TRUE(m.exposure_s && m.gain && m.conversion_gain_e_per_adu && m.offset_adu && m.temperature_c &&
              m.filter && m.camera && m.imagetyp);
  EXPECT_EQ(m.exposure_s->value, 60.0);
  EXPECT_EQ(m.exposure_s->source.origin, metadata_origin::header);
  EXPECT_EQ(m.exposure_s->source.keyword, "EXPTIME");
  EXPECT_EQ(m.gain->value, 100.0);
  EXPECT_EQ(m.conversion_gain_e_per_adu->value, 1.5);
  EXPECT_EQ(m.conversion_gain_e_per_adu->source.keyword, "EGAIN");
  EXPECT_EQ(m.offset_adu->value, 10.0);
  EXPECT_EQ(m.temperature_c->value, -10.0);
  EXPECT_EQ(m.filter->value, "L");
  EXPECT_EQ(m.camera->value, "SYN-CAM");
  EXPECT_EQ(m.imagetyp->value, "Light Frame");
  EXPECT_FALSE(m.saturation_adu.has_value());
  EXPECT_FALSE(m.pedestal_adu.has_value());
  EXPECT_FALSE(m.roi_x.has_value());
  EXPECT_FALSE(m.roi_y.has_value());
  EXPECT_FALSE(m.readout_mode.has_value());
  EXPECT_FALSE(m.row_order.has_value());
  EXPECT_FALSE(img.unit.has_value());
  EXPECT_FALSE(img.lin.has_value());
  EXPECT_FALSE(img.cfa.has_value());
  EXPECT_FALSE(img.geometry.binning_x.has_value());
  EXPECT_EQ(img.uninterpreted_keywords, std::vector<std::string>{"FRAMENO"});

  image full = decode(fx::build_fits({raw_image(
      16, {2, 2},
      {fx::card_real("SATURATE", 65000), fx::card_real("PEDESTAL", 100), fx::card_int("XORGSUBF", 3),
       fx::card_int("YORGSUBF", 4), fx::card_str("READOUTM", "Fast"), fx::card_str("ROWORDER", "TOP-DOWN"),
       fx::card_int("XBINNING", 2), fx::card_int("YBINNING", 3)})}));
  EXPECT_EQ(full.metadata.saturation_adu->value, 65000.0);
  EXPECT_EQ(full.metadata.pedestal_adu->value, 100.0);
  EXPECT_EQ(full.metadata.roi_x->value, 3);
  EXPECT_EQ(full.metadata.roi_y->value, 4);
  EXPECT_EQ(full.metadata.readout_mode->value, "Fast");
  EXPECT_EQ(full.metadata.row_order->value, "TOP-DOWN");
  EXPECT_EQ(full.geometry.binning_x, 2u);
  EXPECT_EQ(full.geometry.binning_y, 3u);
  EXPECT_TRUE(decode(encode_fits(full)) == full);
}

TEST(Fits, T_M2A_04_declared_value_conflicting_with_header_stops) {
  image img = decode(fx::make_synth_frame(fx::frame_kind::dark, 8, 6, 0).file);
  declare_metadata(img.metadata.exposure_s, 60.0, "exposure_s");
  EXPECT_STOP(declare_metadata(img.metadata.exposure_s, 61.0, "exposure_s"), error_category::incompatible,
              "metadata.declared_conflict");
  declare_metadata(img.metadata.saturation_adu, 65535.0, "saturation_adu");
  EXPECT_EQ(img.metadata.saturation_adu->source.origin, metadata_origin::declared);
  EXPECT_STOP(declare_metadata(img.metadata.saturation_adu, 60000.0, "saturation_adu"),
              error_category::incompatible, "metadata.declared_conflict");
  // un valore dichiarato viene scritto nella sua keyword; l'origine e' provenienza e non entra nel file
  image again = decode(encode_fits(img));
  ASSERT_TRUE(again.metadata.saturation_adu.has_value());
  EXPECT_EQ(again.metadata.saturation_adu->value, 65535.0);
  EXPECT_EQ(again.metadata.saturation_adu->source.origin, metadata_origin::header);
  EXPECT_EQ(again.metadata.saturation_adu->source.keyword, "SATURATE");
  EXPECT_TRUE(equivalent_content(again, img));
}

TEST(Fits, T_M2A_04_bad_keyword_values_stop_with_cfitsio_status_or_keyword_error) {
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_str("EXPTIME", "abc")})})),
              error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_real("XORGSUBF", 1.5)})})),
              error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_int("XBINNING", 0)})})),
              error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_str("BUNIT", "furlongs")})})),
              error_category::unsupported, "fits.bunit");
}

// ------------------------------------------------------------------ limiti

void make_sparse_file(const std::string& path, const fx::raw_hdu& h, std::uint64_t total_size) {
  fx::bytes header;
  fx::append_header(header, h);
  write_file(path, header);
  ASSERT_EQ(::truncate(path.c_str(), static_cast<off_t>(total_size)), 0) << "truncate non riuscita";
}

std::uint64_t padded_size(std::uint64_t header, std::uint64_t data) {
  return header + (data + 2879) / 2880 * 2880;
}

TEST(Fits, T_M2A_05_naxis_and_axis_and_plane_limits) {
  fx::raw_hdu ok3 = raw_image(8, {2, 2, 16});
  EXPECT_NO_THROW(plan_fits(fx::build_fits({ok3})));
  EXPECT_STOP(plan_fits(fx::build_fits({raw_image(8, {2, 2, 17})})), error_category::limit_exceeded,
              "fits.max_planes");
  EXPECT_STOP(plan_fits(fx::build_fits({raw_image(8, {1, 1, 1, 1})})), error_category::limit_exceeded,
              "fits.max_naxis");
  EXPECT_NO_THROW(plan_fits(fx::build_fits({raw_image(8, {65536, 1})})));
  EXPECT_NO_THROW(plan_fits(fx::build_fits({raw_image(8, {1, 65536})})));
  EXPECT_STOP(plan_fits(fx::build_fits({raw_image(8, {65537, 1})})), error_category::limit_exceeded,
              "fits.max_axis_length");
  EXPECT_STOP(plan_fits(fx::build_fits({raw_image(8, {1, 65537})})), error_category::limit_exceeded,
              "fits.max_axis_length");
}

TEST(Fits, T_M2A_05_max_samples_boundary_on_sparse_files) {
  const std::string path = tmp_file("samples.fits");
  fx::raw_hdu at_limit = raw_image(8, {65536, 4096});
  at_limit.data.clear();
  fx::bytes header;
  fx::append_header(header, at_limit);
  make_sparse_file(path, at_limit, padded_size(header.size(), fits_limits::max_samples));
  {
    fits_input in = open_fits_input(path);
    fits_plan plan = plan_fits(in);
    EXPECT_EQ(plan.samples, fits_limits::max_samples);
    EXPECT_EQ(plan.peak_bytes, plan.file_bytes + plan.samples * 4 + plan.samples * 2);
  }
  fx::raw_hdu over = at_limit;
  over.axes = {65536, 4097};
  make_sparse_file(path, over, padded_size(header.size(), 65536ull * 4097));
  {
    fits_input in = open_fits_input(path);
    EXPECT_STOP(plan_fits(in), error_category::limit_exceeded, "fits.max_samples");
    EXPECT_STOP(read_fits_bytes(in, big_budget), error_category::limit_exceeded, "fits.max_samples");
  }
  over.axes = {65536, 4096, 2};
  make_sparse_file(path, over, padded_size(header.size(), 65536ull * 4096 * 2));
  EXPECT_STOP(plan_fits(open_fits_input(path)), error_category::limit_exceeded, "fits.max_samples");
  // asse oltre il limite: stop al preflight, senza leggere il file (4 GiB sparsi)
  over.axes = {65537, 2};
  make_sparse_file(path, over, std::uint64_t{4} << 30);
  EXPECT_STOP(read_file(path), error_category::limit_exceeded, "fits.max_axis_length");
  fs::remove(path);
}

TEST(Fits, T_M2A_05_max_hdus_and_max_header_cards) {
  auto with_extensions = [](int count) {
    std::vector<fx::raw_hdu> hdus = {raw_image(8, {1, 1})};
    for (int i = 0; i < count; ++i) hdus.push_back(extension("X" + std::to_string(i), 8, {1, 1}));
    return fx::build_fits(hdus);
  };
  // 8 HDU: il limite passa, ma le estensioni non riconosciute sono comunque rifiutate
  EXPECT_STOP(plan_fits(with_extensions(7)), error_category::unsupported, "fits.extension_kind");
  EXPECT_STOP(plan_fits(with_extensions(8)), error_category::limit_exceeded, "fits.max_hdus");
  EXPECT_NO_THROW(plan_fits(with_extensions(0)));

  auto with_cards = [](int filler) {
    std::vector<std::string> cards(static_cast<std::size_t>(filler), fx::card_text("COMMENT", "x"));
    return fx::build_fits({raw_image(8, {1, 1}, cards)});
  };
  EXPECT_NO_THROW(decode(with_cards(9995)));  // 5 obbligatorie + 9995 = 10000
  EXPECT_STOP(plan_fits(with_cards(9996)), error_category::limit_exceeded, "fits.max_header_cards");
}

TEST(Fits, T_M2A_05_max_file_bytes_boundary) {
  const std::string path = tmp_file("huge.bin");
  { std::ofstream(path, std::ios::binary).put('\0'); }
  ASSERT_EQ(::truncate(path.c_str(), static_cast<off_t>(fits_limits::max_file_bytes)), 0);
  EXPECT_NO_THROW(open_fits_input(path));
  ASSERT_EQ(::truncate(path.c_str(), static_cast<off_t>(fits_limits::max_file_bytes + 1)), 0);
  EXPECT_STOP(open_fits_input(path), error_category::limit_exceeded, "fits.max_file_bytes");
  fs::remove(path);
  EXPECT_EQ(fits_limits::max_file_bytes, 4624ull * 1024 * 1024);
}

TEST(Fits, T_M2A_05_non_regular_and_missing_files_stop) {
  EXPECT_STOP(open_fits_input(tmp_dir()), error_category::invalid_input, "fits.not_regular");
  EXPECT_STOP(open_fits_input("/dev/null"), error_category::invalid_input, "fits.not_regular");
  const std::string fifo = tmp_file("pipe.fits");
  fs::remove(fifo);
  ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
  EXPECT_STOP(open_fits_input(fifo), error_category::invalid_input, "fits.not_regular");
  fs::remove(fifo);
  EXPECT_STOP(open_fits_input(tmp_file("assente.fits")), error_category::io, "input.missing");
}

TEST(Fits, T_M2A_short_read_with_unchanged_size_stops_as_io) {
  const std::string path = tmp_file("short_read.fits");
  const fx::synth_frame f = fx::make_synth_frame(fx::frame_kind::light, 32, 16, 1);
  write_file(path, f.file);
  {
    fits_input in = open_fits_input(path);
    int calls = 0;
    in.set_fault_injector([&](std::string_view point) { return point == "input.pread" ? (++calls, 5) : 0; });
    EXPECT_STOP(read_input_bytes(in, big_budget), error_category::io, "input.short_read");
    EXPECT_EQ(calls, 1);
  }
  {
    fits_input in = open_fits_input(path);
    int calls = 0;
    in.set_fault_injector([&](std::string_view) { return ++calls == 1 ? 5 : 0; });
    EXPECT_STOP(plan_fits(in), error_category::io, "input.short_read");
    EXPECT_EQ(calls, 1);
  }
}

TEST(Fits, T_M2A_05_file_changing_size_after_fstat_stops) {
  const std::string path = tmp_file("change.fits");
  const fx::synth_frame f = fx::make_synth_frame(fx::frame_kind::light, 32, 16, 1);
  write_file(path, f.file);
  {
    fits_input in = open_fits_input(path);
    EXPECT_EQ(in.size_bytes(), f.file.size());
    { std::ofstream a(path, std::ios::binary | std::ios::app); a << "extra"; }
    EXPECT_STOP(read_fits_bytes(in, big_budget), error_category::integrity, "input.size_changed");
  }
  write_file(path, f.file);
  {
    fits_input in = open_fits_input(path);
    ASSERT_EQ(::truncate(path.c_str(), static_cast<off_t>(f.file.size() - 2880)), 0);
    EXPECT_STOP(read_fits_bytes(in, big_budget), error_category::integrity, "input.size_changed");
  }
  write_file(path, f.file);
  {
    fits_input in = open_fits_input(path);
    EXPECT_EQ(read_fits_bytes(in, big_budget), f.file);
  }
  fs::remove(path);
}

TEST(Fits, T_M2A_05_peak_over_budget_stops_before_reading) {
  const std::string path = tmp_file("budget.fits");
  const fx::synth_frame f = fx::make_synth_frame(fx::frame_kind::light, 32, 16, 1);
  write_file(path, f.file);
  fits_input in = open_fits_input(path);
  const fits_plan plan = plan_fits(in);
  EXPECT_EQ(plan.samples, 32u * 16u);
  EXPECT_EQ(plan.type, sample_type::f32);
  EXPECT_EQ(plan.peak_bytes, f.file.size() + 512u * 4u + 512u * 2u);
  EXPECT_NO_THROW(read_fits_bytes(in, plan.peak_bytes));
  EXPECT_STOP(read_fits_bytes(in, plan.peak_bytes - 1), error_category::budget, "sched.job_exceeds_budget");
  // la lettura non e' tentata: con il file cresciuto vince il budget, non size_changed
  { std::ofstream a(path, std::ios::binary | std::ios::app); a << "extra"; }
  EXPECT_STOP(read_fits_bytes(in, 1), error_category::budget, "sched.job_exceeds_budget");
  fs::remove(path);
}

TEST(Fits, T_M2A_05_peak_counts_variance_extension) {
  image img = make_image({8, 4, 1}, sample_type::f64, plane_layout::mono);
  img.encoding = source_encoding{-64, 0, 1};
  img.variance = variance_plane{aligned_buffer(img.samples.size())};
  const fx::bytes out = encode_fits(img);
  fits_plan plan = plan_fits(out);
  EXPECT_TRUE(plan.has_variance);
  EXPECT_FALSE(plan.has_invalid);
  EXPECT_EQ(plan.peak_bytes, out.size() + 32u * 8u + 32u * 2u + 32u * 8u);
}

// ------------------------------------------------------------------ malformati

fx::bytes valid_small() {
  image img = make_image({6, 5, 1}, sample_type::f32, plane_layout::mono);
  img.encoding = source_encoding{-32, 0, 1};
  for (int i = 0; i < 30; ++i) img.data<float>()[i] = static_cast<float>(i) * 0.25f;
  img.invalid.bits[3] = invalid_reason::saturated;
  img.variance = variance_plane{aligned_buffer(img.samples.size())};
  for (int i = 0; i < 30; ++i) img.variance_data<float>()[i] = 1.0f;
  img.data<float>()[3] = std::numeric_limits<float>::quiet_NaN();
  img.variance_data<float>()[3] = std::numeric_limits<float>::quiet_NaN();
  return encode_fits(img);
}

TEST(Fits, T_M2A_06_truncated_and_malformed_files_stop_with_controlled_errors) {
  const fx::bytes good = valid_small();
  EXPECT_NO_THROW(decode(good));
  EXPECT_STOP(decode(fx::bytes{}), error_category::invalid_input, "fits.malformed");
  EXPECT_STOP(decode(fx::bytes(good.begin(), good.begin() + 1000)), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(fx::bytes(good.begin(), good.begin() + 2880)), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(fx::bytes(good.begin(), good.end() - 1)), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(fx::bytes(good.begin(), good.begin() + 2880 * 2 + 1000)),
              error_category::invalid_input, "fits.malformed");
  fx::bytes text(5760, 'x');
  EXPECT_STOP(decode(text), error_category::invalid_input, "fits.malformed");
  fx::bytes zeros(2880, 0);
  EXPECT_STOP(decode(zeros), error_category::invalid_input, "fits.malformed");

  auto with_card_replaced = [&](std::size_t card_index, const std::string& card) {
    fx::bytes b = good;
    std::memcpy(b.data() + card_index * 80, card.data(), 80);
    return b;
  };
  EXPECT_STOP(decode(with_card_replaced(0, fx::card_bool("SIMPLE", false))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(1, fx::card_int("BITPIX", 12))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(1, fx::card_int("BITPIX", 0))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(1, fx::card_int("BITPIX", -16))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(1, fx::card_str("BITPIX", "16"))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(2, fx::card_int("NAXIS", 999999999))), error_category::limit_exceeded,
              "fits.max_naxis");
  EXPECT_STOP(decode(with_card_replaced(2, fx::card_int("NAXIS", -1))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(2, fx::card_int("NAXIS", 0))), error_category::unsupported,
              "fits.hdu_kind");
  EXPECT_STOP(decode(with_card_replaced(3, fx::card_int("NAXIS1", 9999999999999LL))),
              error_category::limit_exceeded, "fits.max_axis_length");
  EXPECT_STOP(decode(with_card_replaced(3, fx::card_text("NAXIS1", "= 99999999999999999999999"))),
              error_category::invalid_input, "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(3, fx::card_int("NAXIS1", 0))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(3, fx::card_int("NAXIS1", -4))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(3, fx::card_int("NAXIS2", 6))), error_category::invalid_input,
              "fits.malformed");
  EXPECT_STOP(decode(with_card_replaced(3, fx::card_text("NAXIS1", "= 'abc"))), error_category::invalid_input,
              "fits.malformed");
  // END mancante: nessuna card END nel primo blocco e nessun altro blocco valido
  fx::bytes no_end(2880, ' ');
  std::memcpy(no_end.data(), fx::card_bool("SIMPLE", true).data(), 80);
  EXPECT_STOP(decode(no_end), error_category::invalid_input, "fits.malformed");
  // NAXIS1 enorme con dati assenti: limite prima della verifica dei dati
  EXPECT_STOP(decode(with_card_replaced(3, fx::card_int("NAXIS1", 65536))), error_category::invalid_input,
              "fits.malformed");
}

TEST(Fits, T_M2A_06_extension_errors) {
  const fx::raw_hdu primary = raw_image(16, {2, 2}, {fx::card_real("BZERO", 32768.0)});
  fx::raw_hdu inv = extension("INVALID", 16, {2, 2});
  inv.cards.push_back(fx::card_real("BZERO", 32768.0));
  inv.data = {0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0};
  EXPECT_STOP(decode(fx::build_fits({primary, extension("INVALID", 8, {2, 2})})),
              error_category::invalid_input, "fits.extension_encoding");
  EXPECT_STOP(decode(fx::build_fits({primary, extension("INVALID", 16, {2, 2})})),
              error_category::invalid_input, "fits.extension_encoding");
  EXPECT_STOP(decode(fx::build_fits({primary, extension("VARIANCE", -64, {2, 2})})),
              error_category::invalid_input, "fits.extension_encoding");
  fx::raw_hdu bad_geom = inv;
  bad_geom.axes = {3, 2};
  bad_geom.data.assign(12, 0);  // geometria 3x2
  EXPECT_STOP(decode(fx::build_fits({primary, bad_geom})), error_category::invalid_input,
              "fits.extension_geometry");
  EXPECT_STOP(decode(fx::build_fits({primary, inv, inv})), error_category::invalid_input, "fits.malformed");
  EXPECT_NO_THROW(decode(fx::build_fits({primary, inv})));
}

TEST(Fits, T_M2A_06_short_deterministic_fuzz_never_crashes_or_throws_foreign_exceptions) {
  const fx::bytes good = valid_small();
  std::uint64_t state = 0x9E3779B97F4A7C15ull;
  auto next = [&state] {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
  };
  std::size_t stopped = 0;
  std::size_t accepted = 0;
  for (int iter = 0; iter < 6000; ++iter) {
    fx::bytes b = good;
    const int edits = 1 + static_cast<int>(next() % 3);
    for (int e = 0; e < edits; ++e) {
      const bool in_header = (next() % 10) < 7;
      const std::size_t limit = in_header ? 2880 : b.size();
      b[next() % limit] = static_cast<std::uint8_t>(next());
    }
    if (iter % 5 == 0) b.resize(next() % b.size());
    try {
      image img = decode(b);
      ++accepted;
      validate_image(img);
    } catch (const stop_error&) {
      ++stopped;
    }
  }
  EXPECT_GT(stopped, 100u);
  EXPECT_GT(accepted, 10u);
}

// ------------------------------------------------------------------ CFA

fx::bytes cfa_file(const std::vector<std::string>& cards, std::uint64_t planes = 1) {
  fx::raw_hdu h = planes == 1 ? raw_image(16, {8, 6}, cards) : raw_image(16, {8, 6, planes}, cards);
  return fx::build_fits({h});
}

TEST(Fits, T_M2A_07_four_patterns_and_offsets) {
  const std::pair<const char*, cfa_pattern> patterns[] = {{"RGGB", cfa_pattern::rggb},
                                                          {"BGGR", cfa_pattern::bggr},
                                                          {"GRBG", cfa_pattern::grbg},
                                                          {"GBRG", cfa_pattern::gbrg}};
  for (const auto& [name, pattern] : patterns) {
    for (int xo = 0; xo <= 1; ++xo) {
      for (int yo = 0; yo <= 1; ++yo) {
        SCOPED_TRACE(std::string(name) + " " + std::to_string(xo) + std::to_string(yo));
        image img = decode(cfa_file({fx::card_str("BAYERPAT", name), fx::card_int("XBAYROFF", xo),
                                          fx::card_int("YBAYROFF", yo)}));
        ASSERT_TRUE(img.cfa.has_value());
        EXPECT_EQ(img.cfa->pattern, pattern);
        EXPECT_EQ(img.cfa->x_offset, xo);
        EXPECT_EQ(img.cfa->y_offset, yo);
        EXPECT_EQ(img.layout, plane_layout::cfa);
        EXPECT_EQ(img.geometry.planes, 1u);
        EXPECT_TRUE(decode(encode_fits(img)) == img);
      }
    }
  }
}

TEST(Fits, T_M2A_07_unknown_or_missing_pattern_and_bad_offsets_stop) {
  EXPECT_STOP(decode(cfa_file({fx::card_str("BAYERPAT", "XYZW"), fx::card_int("XBAYROFF", 0),
                                    fx::card_int("YBAYROFF", 0)})),
              error_category::unsupported, "fits.bayer_pattern");
  EXPECT_STOP(decode(cfa_file({fx::card_str("BAYERPAT", ""), fx::card_int("XBAYROFF", 0),
                                    fx::card_int("YBAYROFF", 0)})),
              error_category::unsupported, "fits.bayer_pattern");
  EXPECT_STOP(decode(cfa_file({fx::card_int("XBAYROFF", 0), fx::card_int("YBAYROFF", 0)})),
              error_category::metadata_missing, "fits.bayer_pattern_missing");
  EXPECT_STOP(decode(cfa_file({fx::card_int("YBAYROFF", 1)})), error_category::metadata_missing,
              "fits.bayer_pattern_missing");
  EXPECT_STOP(decode(cfa_file({fx::card_str("BAYERPAT", "RGGB")})), error_category::metadata_missing,
              "fits.bayer_offset_missing");
  EXPECT_STOP(decode(cfa_file({fx::card_str("BAYERPAT", "RGGB"), fx::card_int("XBAYROFF", 2),
                                    fx::card_int("YBAYROFF", 0)})),
              error_category::invalid_input, "fits.bayer_offset");
  EXPECT_STOP(decode(cfa_file({fx::card_str("BAYERPAT", "RGGB"), fx::card_int("XBAYROFF", 0),
                                    fx::card_int("YBAYROFF", 0)},
                                   3)),
              error_category::invalid_input, "fits.cfa_planes");
  image made = make_image({4, 4, 1}, sample_type::f32, plane_layout::cfa, cfa_layout{cfa_pattern::bggr, 1, 0});
  EXPECT_TRUE(equivalent_content(decode(encode_fits(made)), made));  // le card CFA vengono sintetizzate
  made.header_cards = {fx::card_str("BAYERPAT", "RGGB"), fx::card_int("XBAYROFF", 1), fx::card_int("YBAYROFF", 0)};
  made.uninterpreted_keywords = {};
  EXPECT_STOP(encode_fits(made), error_category::invalid_input, "fits.header_inconsistent");
}

// ------------------------------------------------------------------ maschere

TEST(Fits, T_M2A_08_read_image_keeps_marked_samples_and_flags_nonfinite) {
  constexpr std::uint64_t n = 12;
  const std::uint32_t patterns[n] = {0x3f800000u, 0x7fc00000u, 0x7f800000u, 0xff800000u, 0x40000000u, 0x00000001u,
                                     0x7fc00123u, 0x00000000u, 0xbf800000u, 0x7f7fffffu, 0x80000000u, 0x41200000u};
  fx::raw_hdu primary = raw_image(-32, {4, 3});
  primary.data.clear();
  for (auto p : patterns) fx::put_be(primary.data, p, 4);
  fx::raw_hdu inv = extension("INVALID", 16, {4, 3});
  inv.cards.push_back(fx::card_real("BZERO", 32768.0));
  inv.data.clear();
  const std::uint16_t file_bits[n] = {0, 0, 0, 0, invalid_reason::saturated, 0, 0, invalid_reason::source_flagged,
                                      0, 0, 0, invalid_reason::saturated | invalid_reason::master_invalid};
  for (auto b : file_bits) fx::put_be(inv.data, static_cast<std::uint16_t>(b ^ 0x8000u), 2);

  for (bool with_mask_hdu : {false, true}) {
    SCOPED_TRACE(with_mask_hdu);
    image img = decode(with_mask_hdu ? fx::build_fits({primary, inv}) : fx::build_fits({primary}));
    for (std::size_t i = 0; i < n; ++i) {
      EXPECT_EQ(fx::bits_of(img.data<float>()[i]), patterns[i]) << i;
      const bool nonfinite = !std::isfinite(img.data<float>()[i]);
      const std::uint16_t from_file = with_mask_hdu ? file_bits[i] : 0;
      const std::uint16_t expected = static_cast<std::uint16_t>(
          from_file | ((nonfinite && from_file == 0) ? invalid_reason::source_nonfinite : 0));
      EXPECT_EQ(img.invalid.bits[i], expected) << i;
    }
    EXPECT_EQ(encode_fits(img).size() % 2880, 0u);
    image again = decode(encode_fits(img));
    EXPECT_TRUE(again == img);
  }
}

TEST(Fits, T_M2A_08_reserved_bits_in_file_mask_stop) {
  for (std::uint16_t bad : {std::uint16_t{1u << 9}, std::uint16_t{1u << 15}, std::uint16_t{0xFE00u}}) {
    fx::raw_hdu primary = raw_image(-32, {2, 2});
    fx::raw_hdu inv = extension("INVALID", 16, {2, 2});
    inv.cards.push_back(fx::card_real("BZERO", 32768.0));
    inv.data.clear();
    for (int i = 0; i < 4; ++i) fx::put_be(inv.data, static_cast<std::uint16_t>((i == 2 ? bad : 0) ^ 0x8000u), 2);
    EXPECT_STOP(decode(fx::build_fits({primary, inv})), error_category::invalid_input,
                "fits.invalid_reserved_bits");
  }
  image img = make_image({2, 2, 1}, sample_type::f32, plane_layout::mono);
  img.encoding = source_encoding{-32, 0, 1};
  img.invalid.bits[1] = 1u << 10;
  EXPECT_STOP(encode_fits(img), error_category::invalid_input, "image.mask_reserved_bits");
}

TEST(Fits, T_M2A_09_invalid_and_variance_extensions_roundtrip_identically) {
  for (sample_type t : {sample_type::f32, sample_type::f64}) {
    SCOPED_TRACE(t == sample_type::f32 ? "f32" : "f64");
    image img = make_image({5, 4, 1}, t, plane_layout::mono);
    img.encoding = source_encoding{t == sample_type::f32 ? -32 : -64, 0, 1, 2};
    img.unit = pixel_unit::adu;
    img.header_cards = {fx::card_str("BUNIT", "adu"), fx::card_str("OBJECT", "t")};
    img.uninterpreted_keywords = {"OBJECT"};
    img.variance = variance_plane{aligned_buffer(img.samples.size())};
    for (int i = 0; i < 20; ++i) {
      if (t == sample_type::f32) {
        img.data<float>()[i] = 0.5f * static_cast<float>(i);
        img.variance_data<float>()[i] = 0.125f * static_cast<float>(i);
      } else {
        img.data<double>()[i] = 0.5 * i;
        img.variance_data<double>()[i] = 0.125 * i;
      }
    }
    const std::uint16_t masks[] = {invalid_reason::saturated,
                                   invalid_reason::master_invalid | invalid_reason::q_nonpositive,
                                   invalid_reason::result_nonfinite, invalid_reason::q_unreliable};
    const int at[] = {2, 7, 11, 19};
    for (int k = 0; k < 4; ++k) {
      img.invalid.bits[at[k]] = masks[k];
      if (t == sample_type::f32) {
        img.data<float>()[at[k]] = std::numeric_limits<float>::quiet_NaN();
        img.variance_data<float>()[at[k]] = std::numeric_limits<float>::quiet_NaN();
      } else {
        img.data<double>()[at[k]] = std::numeric_limits<double>::quiet_NaN();
        img.variance_data<double>()[at[k]] = std::numeric_limits<double>::quiet_NaN();
      }
    }
    EXPECT_TRUE(operator_output_invariants_hold(img));
    const fx::bytes out = encode_fits(img);
    image again = decode(out);
    EXPECT_TRUE(again == img);
    // layout dei HDU scritto: primario, INVALID (BITPIX 16, BZERO 32768), VARIANCE
    fx::raw_primary_view inv = fx::parse_hdu(out, 1);
    EXPECT_EQ(inv.bitpix, 16);
    fx::raw_primary_view var = fx::parse_hdu(out, 2);
    EXPECT_EQ(var.bitpix, t == sample_type::f32 ? -32 : -64);
    // verita' indipendente della maschera: parole big-endian con offset 32768
    for (int i = 0; i < 20; ++i) {
      const std::uint16_t word = static_cast<std::uint16_t>((inv.data[2 * i] << 8) | inv.data[2 * i + 1]);
      EXPECT_EQ(static_cast<std::uint16_t>(word ^ 0x8000u), img.invalid.bits[i]) << i;
    }
  }
}

TEST(Fits, T_M2A_09_image_without_masked_pixels_writes_no_invalid_hdu) {
  image img = make_image({3, 3, 1}, sample_type::f32, plane_layout::mono);
  img.encoding = source_encoding{-32, 0, 1, 2};
  const fx::bytes out = encode_fits(img);
  EXPECT_FALSE(plan_fits(out).has_invalid);
  EXPECT_FALSE(plan_fits(out).has_variance);
  EXPECT_TRUE(decode(out) == img);
}

TEST(Fits, T_M2A_10_writer_returns_the_buffer_that_rereads_identically) {
  for (auto kind : {fx::frame_kind::light, fx::frame_kind::dark, fx::frame_kind::flat, fx::frame_kind::dark_flat,
                    fx::frame_kind::bias}) {
    fx::synth_frame f = fx::make_synth_frame(kind, 33, 17, 2);
    image img = decode(f.file);
    const fx::bytes once = encode_fits(img);
    image again = decode(once);
    EXPECT_TRUE(again == img);
    EXPECT_EQ(encode_fits(again), once);
    // data unit identica all'originale scritto dal writer indipendente
    EXPECT_EQ(fx::parse_hdu(once).data, fx::parse_hdu(f.file).data);
    EXPECT_EQ(once.size() % 2880, 0u);
  }
}

TEST(Fits, T_M2A_11_wrong_checksum_stops_with_integrity) {
  const fx::raw_hdu base = raw_image(16, {4, 4}, {fx::card_real("BZERO", 32768.0)});
  fx::raw_hdu h = base;
  h.data.clear();
  for (int i = 0; i < 16; ++i) fx::put_be(h.data, static_cast<std::uint16_t>(i * 1000 + 1), 2);
  const std::uint32_t sum = fx::datasum(h.data);

  fx::raw_hdu good = h;
  good.cards.push_back(fx::card_str("DATASUM", std::to_string(sum)));
  EXPECT_NO_THROW(decode(fx::build_fits({good})));

  fx::raw_hdu bad = h;
  bad.cards.push_back(fx::card_str("DATASUM", std::to_string(sum + 1)));
  EXPECT_STOP(decode(fx::build_fits({bad})), error_category::integrity, "fits.checksum");

  fx::raw_hdu bad_hdu = h;
  bad_hdu.cards.push_back(fx::card_str("CHECKSUM", "AAAAAAAAAAAAAAAA"));
  EXPECT_STOP(decode(fx::build_fits({bad_hdu})), error_category::integrity, "fits.checksum");

  // il valore corretto con dati alterati
  fx::bytes corrupted = fx::build_fits({good});
  corrupted[2880 + 5] ^= 0x40;
  EXPECT_STOP(decode(corrupted), error_category::integrity, "fits.checksum");
}

TEST(Fits, T_M2A_11_checksum_keywords_are_not_rewritten) {
  fx::raw_hdu h = raw_image(16, {2, 2}, {fx::card_real("BZERO", 32768.0)});
  h.data.assign(8, 0);
  h.data[1] = 5;
  h.cards.push_back(fx::card_str("DATASUM", std::to_string(fx::datasum(h.data))));
  image img = decode(fx::build_fits({h}));
  for (const auto& c : img.header_cards) EXPECT_NE(c.substr(0, 8), "DATASUM ");
  EXPECT_NO_THROW(decode(encode_fits(img)));
}

TEST(Fits, T_M2A_12_unsupported_hdus_stop) {
  const fx::raw_hdu primary = raw_image(16, {2, 2});
  fx::raw_hdu table = raw_image(8, {8, 2});
  table.primary = false;
  table.xtension = "BINTABLE";
  table.cards = {fx::card_int("TFIELDS", 1)};
  EXPECT_STOP(decode(fx::build_fits({primary, table})), error_category::unsupported, "fits.hdu_kind");
  table.xtension = "TABLE";
  EXPECT_STOP(decode(fx::build_fits({primary, table})), error_category::unsupported, "fits.hdu_kind");

  fx::raw_hdu empty_primary = raw_image(8, {});
  EXPECT_STOP(decode(fx::build_fits({empty_primary})), error_category::unsupported, "fits.hdu_kind");
  fx::raw_hdu compressed = table;
  compressed.xtension = "BINTABLE";
  compressed.cards = {fx::card_bool("ZIMAGE", true), fx::card_str("ZCMPTYPE", "RICE_1")};
  EXPECT_STOP(decode(fx::build_fits({empty_primary, compressed})), error_category::unsupported,
              "fits.hdu_kind");
  EXPECT_STOP(decode(fx::build_fits({primary, extension("ALTRO", 16, {2, 2})})), error_category::unsupported,
              "fits.extension_kind");
  fx::raw_hdu with_heap = extension("INVALID", 16, {2, 2});
  with_heap.pcount = 4;
  EXPECT_STOP(decode(fx::build_fits({primary, with_heap})), error_category::unsupported, "fits.hdu_kind");
  // PCOUNT/GCOUNT non standard nel primario: CFITSIO sposterebbe l'inizio dei dati (gruppi casuali)
  EXPECT_STOP(decode(fx::build_fits({raw_image(-32, {2, 2}, {fx::card_int("PCOUNT", 3)})})),
              error_category::unsupported, "fits.hdu_kind");
  EXPECT_STOP(decode(fx::build_fits({raw_image(-32, {2, 2}, {fx::card_int("GCOUNT", 2)})})),
              error_category::unsupported, "fits.hdu_kind");
}

TEST(Fits, T_M2A_13_rgb_and_multi_plane_are_read_kept_and_rewritten_bit_exact) {
  constexpr std::uint32_t w = 5, h = 4;
  for (std::uint32_t planes : {3u, 2u, 5u, 16u}) {
    for (bool declared_rgb : {false, true}) {
      if (declared_rgb && planes != 3) continue;
      SCOPED_TRACE(std::to_string(planes) + (declared_rgb ? " rgb" : ""));
      std::vector<std::string> cards = {fx::card_real("BZERO", 32768.0)};
      if (declared_rgb) cards.push_back(fx::card_str("CTYPE3", "RGB"));
      fx::raw_hdu hdu = raw_image(16, {w, h, planes}, cards);
      hdu.data.clear();
      for (std::uint32_t p = 0; p < planes; ++p) {
        for (std::uint32_t y = 0; y < h; ++y) {
          for (std::uint32_t x = 0; x < w; ++x) {
            fx::put_be(hdu.data, static_cast<std::uint16_t>((p * 1000 + y * 10 + x) - 32768), 2);
          }
        }
      }
      const fx::bytes file = fx::build_fits({hdu});
      image img = decode(file);
      EXPECT_EQ(img.geometry.planes, planes);
      EXPECT_EQ(img.layout, declared_rgb ? plane_layout::rgb : plane_layout::multi_plane);
      for (std::uint32_t p = 0; p < planes; ++p) {
        for (std::uint32_t y = 0; y < h; ++y) {
          for (std::uint32_t x = 0; x < w; ++x) {
            EXPECT_EQ(sample(img, (static_cast<std::size_t>(p) * h + y) * w + x),
                      static_cast<double>(p * 1000 + y * 10 + x));
          }
        }
      }
      const fx::bytes out = encode_fits(img);
      fx::raw_primary_view v = fx::parse_hdu(out);
      EXPECT_EQ(v.axes, (std::vector<std::uint64_t>{w, h, planes}));
      EXPECT_EQ(v.data, hdu.data);
      image again = decode(out);
      EXPECT_TRUE(again == img);
      EXPECT_EQ(again.layout, img.layout);
    }
  }
  // CTYPE3 = RGB con planes != 3 non e' un rgb
  image five = decode(fx::build_fits({raw_image(16, {2, 2, 4}, {fx::card_str("CTYPE3", "RGB")})}));
  EXPECT_EQ(five.layout, plane_layout::multi_plane);
  EXPECT_EQ(five.header_cards.size(), 1u);
}

// ------------------------------------------------------------------ fixture FIX-SYN-FITS-01

TEST(Fits, FIX_SYN_FITS_01_frames_read_back_to_their_ground_truth) {
  for (auto kind : {fx::frame_kind::light, fx::frame_kind::dark, fx::frame_kind::flat, fx::frame_kind::dark_flat,
                    fx::frame_kind::bias}) {
    fx::synth_frame f = fx::make_synth_frame(kind, 37, 21, 5);
    image img = decode(f.file);
    ASSERT_EQ(img.samples.size(), f.adu.size() * 4);
    for (std::size_t i = 0; i < f.adu.size(); ++i) ASSERT_EQ(img.data<float>()[i], static_cast<float>(f.adu[i])) << i;
    EXPECT_EQ(img.metadata.imagetyp->value, f.imagetyp);
    EXPECT_EQ(img.metadata.exposure_s->value, f.exposure_s);
    EXPECT_EQ(img.encoding->bitpix, 16);
    EXPECT_EQ(img.encoding->bzero, 32768.0);
  }
  // determinismo: stessi parametri, stessi byte; indice diverso, light diversa
  EXPECT_EQ(fx::make_synth_frame(fx::frame_kind::light, 16, 16, 1).file, fx::make_synth_frame(fx::frame_kind::light, 16, 16, 1).file);
  EXPECT_NE(fx::make_synth_frame(fx::frame_kind::light, 16, 16, 1).file, fx::make_synth_frame(fx::frame_kind::light, 16, 16, 2).file);
  // relazioni note fra i master: light - dark = bias-free sky
  fx::synth_frame light = fx::make_synth_frame(fx::frame_kind::light, 16, 16, 7);
  fx::synth_frame dark = fx::make_synth_frame(fx::frame_kind::dark, 16, 16, 7);
  for (std::uint32_t y = 0; y < 16; ++y) {
    for (std::uint32_t x = 0; x < 16; ++x) {
      EXPECT_EQ(light.adu[y * 16 + x] - dark.adu[y * 16 + x], static_cast<int>(fx::synth_sky(x, y, 7)));
    }
  }
}

// ------------------------------------------------------------------ regressioni della review

fx::bytes int_image(int bitpix, std::vector<std::string> cards, const std::vector<std::uint64_t>& raw_words) {
  fx::raw_hdu h = raw_image(bitpix, {static_cast<std::uint64_t>(raw_words.size()), 1}, std::move(cards));
  h.data.clear();
  for (auto w : raw_words) fx::put_be(h.data, w, std::abs(bitpix) / 8);
  return fx::build_fits({h});
}

TEST(Fits, REVIEW_int64_extremes_stop_without_undefined_behaviour) {
  EXPECT_STOP(decode(int_image(64, {}, {0x8000000000000000ull, 0})), error_category::unsupported, "fits.int64_range");
  EXPECT_STOP(decode(int_image(64, {}, {0x7FFFFFFFFFFFFFFFull, 0})), error_category::unsupported, "fits.int64_range");
  EXPECT_NO_THROW(decode(int_image(64, {}, {1ull << 53, static_cast<std::uint64_t>(-(std::int64_t{1} << 53))})));
}

TEST(Fits, REVIEW_nonconforming_keyword_field_is_malformed) {
  auto shifted = [](std::string c) { return " " + c.substr(0, 79); };
  const std::vector<std::string> cards = {
      shifted(fx::card_real("BZERO", 5.0)),
      fx::card_real("bzero", 5.0),
      fx::card_real("EXP TIME", 1.0),
  };
  for (const std::string& c : cards) {
    SCOPED_TRACE(c.substr(0, 8));
    EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, {c})})), error_category::invalid_input, "fits.malformed");
  }
}

TEST(Fits, REVIEW_duplicate_structural_keywords_are_malformed) {
  const std::vector<std::vector<std::string>> cases = {
      {fx::card_real("BZERO", 0.0), fx::card_real("BZERO", 32768.0)},
      {fx::card_real("BSCALE", 1.0), fx::card_real("BSCALE", 2.0)},
      {fx::card_int("BLANK", 1), fx::card_int("BLANK", 2)},
      {fx::card_int("BITPIX", 16)},
      {fx::card_int("NAXIS", 2)},
      {fx::card_int("NAXIS1", 2)},
      {fx::card_bool("SIMPLE", true)},
      {fx::card_str("XTENSION", "IMAGE")},
      {fx::card_int("PCOUNT", 0), fx::card_int("PCOUNT", 0)},
      {fx::card_int("GCOUNT", 1), fx::card_int("GCOUNT", 1)},
  };
  for (const auto& cards : cases) {
    SCOPED_TRACE(cards[0].substr(0, 8));
    EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, cards)})), error_category::invalid_input, "fits.malformed");
  }
  fx::raw_hdu p = raw_image(16, {2, 2});
  fx::raw_hdu e = extension("INVALID", 16, {2, 2});
  e.cards.push_back(fx::card_str("EXTNAME", "VARIANCE"));
  EXPECT_STOP(decode(fx::build_fits({p, e})), error_category::invalid_input, "fits.malformed");
  fx::raw_hdu negative = extension("INVALID", 16, {2, 2});
  negative.pcount = -5;
  EXPECT_STOP(decode(fx::build_fits({p, negative})), error_category::invalid_input, "fits.malformed");
  fx::raw_hdu neg_g = extension("INVALID", 16, {2, 2});
  neg_g.cards.push_back(fx::card_int("GCOUNT", -1));
  EXPECT_STOP(decode(fx::build_fits({p, neg_g})), error_category::invalid_input, "fits.malformed");
}

TEST(Fits, REVIEW_non_invertible_scales_stop_instead_of_losing_data) {
  const std::vector<std::string> bad = {fx::card_real("BSCALE", 1e-10), fx::card_real("BZERO", 1e10)};
  EXPECT_STOP(decode(int_image(16, bad, {1, 2, 3, 30000})), error_category::unsupported, "fits.scaling_not_exact");
  const std::vector<std::string> huge = {fx::card_real("BSCALE", 1e308), fx::card_real("BZERO", 1e308)};
  EXPECT_STOP(decode(int_image(16, huge, {1, 30000})), error_category::unsupported, "fits.scaling_not_exact");
  const std::vector<std::string> tiny = {fx::card_real("BSCALE", 1e-300), fx::card_real("BZERO", 1.0)};
  EXPECT_STOP(decode(int_image(16, tiny, {5, 7})), error_category::unsupported, "fits.scaling_not_exact");
  const std::vector<std::string> sane = {fx::card_real("BSCALE", 1e300), fx::card_real("BZERO", 0.0)};
  fx::bytes file = int_image(16, sane, {1, 30000});
  image img = decode(file);
  EXPECT_EQ(fx::parse_hdu(encode_fits(img)).data, fx::parse_hdu(file).data);
}

TEST(Fits, REVIEW_variance_must_have_identity_scale) {
  fx::raw_hdu p = raw_image(-32, {2, 2});
  fx::raw_hdu v = extension("VARIANCE", -32, {2, 2});
  v.cards.push_back(fx::card_real("BSCALE", 2.0));
  v.cards.push_back(fx::card_real("BZERO", 7.0));
  EXPECT_STOP(decode(fx::build_fits({p, v})), error_category::invalid_input, "fits.extension_encoding");
}

TEST(Fits, REVIEW_keyword_types_are_checked_not_coerced) {
  auto one = [](const std::string& card) { return fx::build_fits({raw_image(16, {2, 2}, {card})}); };
  EXPECT_STOP(decode(one(fx::card_bool("EXPTIME", true))), error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(one(fx::card_str("EXPTIME", "60"))), error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(one(fx::card_real("FILTER", 3.0))), error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(one(fx::card_int("LINEAR", 1))), error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(one(fx::card_real("BUNIT", 1.0))), error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(one(fx::card_text("EXPTIME", "60 / senza indicatore di valore"))), error_category::invalid_input,
              "fits.keyword_value");
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {8, 6}, {fx::card_str("BAYERPAT", "RGGB"),
                                                           fx::card_bool("XBAYROFF", true), fx::card_int("YBAYROFF", 0)})})),
              error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_real("EXPTIME", 1), fx::card_real("EXPTIME", 2)})})),
              error_category::invalid_input, "fits.duplicate_keyword");
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_str("FILTER", "ab&"), fx::card_text("CONTINUE", "  'cd'")})})),
              error_category::unsupported, "fits.continue");
  EXPECT_NO_THROW(decode(one(fx::card_str("OBJECT", "x&"))));
}

TEST(Fits, REVIEW_linearity_and_declared_metadata_survive_the_roundtrip) {
  image img = make_image({2, 2, 1}, sample_type::f32, plane_layout::mono);
  img.lin = linearity::linear;
  img.unit = pixel_unit::electron;
  img.geometry.binning_x = 2;
  img.geometry.binning_y = 1;
  declare_metadata(img.metadata.exposure_s, 30.0, "exposure_s");
  declare_metadata(img.metadata.filter, std::string("Ha"), "filter");
  declare_metadata(img.metadata.roi_x, std::int64_t{12}, "roi_x");
  image back = decode(encode_fits(img));
  EXPECT_TRUE(equivalent_content(back, img));
  EXPECT_EQ(back.lin, linearity::linear);
  EXPECT_EQ(back.metadata.exposure_s->value, 30.0);
  EXPECT_EQ(back.metadata.filter->value, "Ha");
  EXPECT_EQ(back.metadata.roi_x->value, 12);
  EXPECT_EQ(back.unit, pixel_unit::electron);
  EXPECT_TRUE(decode(encode_fits(back)) == back);
  image nonlinear = decode(fx::build_fits({raw_image(16, {2, 2}, {fx::card_bool("LINEAR", false)})}));
  EXPECT_EQ(nonlinear.lin, linearity::nonlinear);
  // header e campo in conflitto: stop
  img.header_cards = {fx::card_real("EXPTIME", 31.0)};
  img.uninterpreted_keywords = {};
  EXPECT_STOP(encode_fits(img), error_category::invalid_input, "fits.header_inconsistent");
}

TEST(Fits, REVIEW_declared_bayer_offsets_come_from_the_read_request) {
  const fx::bytes file = fx::build_fits({raw_image(16, {8, 6}, {fx::card_str("BAYERPAT", "GRBG")})});
  EXPECT_STOP(decode(file), error_category::metadata_missing, "fits.bayer_offset_missing");
  fits_read_options o;
  o.declared_bayer_x_offset = 1;
  EXPECT_STOP(decode(file, o), error_category::metadata_missing, "fits.bayer_offset_missing");
  o.declared_bayer_y_offset = 0;
  image img = decode(file, o);
  ASSERT_TRUE(img.cfa.has_value());
  EXPECT_EQ(img.cfa->pattern, cfa_pattern::grbg);
  EXPECT_EQ(img.cfa->x_offset, 1);
  EXPECT_EQ(img.cfa->y_offset, 0);
  image back = decode(encode_fits(img));  // gli offset dichiarati finiscono nel file
  EXPECT_TRUE(equivalent_content(back, img));

  const fx::bytes with_header = cfa_file({fx::card_str("BAYERPAT", "RGGB"), fx::card_int("XBAYROFF", 0), fx::card_int("YBAYROFF", 0)});
  EXPECT_STOP(decode(with_header, o), error_category::incompatible, "metadata.declared_conflict");
  fits_read_options same;
  same.declared_bayer_x_offset = 0;
  same.declared_bayer_y_offset = 0;
  EXPECT_NO_THROW(decode(with_header, same));
  EXPECT_STOP(decode(fx::build_fits({raw_image(16, {2, 2})}), o), error_category::invalid_input,
              "fits.declared_offset_unused");
}

TEST(Fits, REVIEW_source_nonfinite_only_where_the_file_mask_is_empty) {
  fx::raw_hdu p = raw_image(-32, {2, 1});
  p.data.clear();
  fx::put_be(p.data, 0x7fc00000u, 4);
  fx::put_be(p.data, 0x7fc00000u, 4);
  fx::raw_hdu inv = extension("INVALID", 16, {2, 1});
  inv.cards.push_back(fx::card_real("BZERO", 32768.0));
  inv.data.clear();
  fx::put_be(inv.data, static_cast<std::uint16_t>(invalid_reason::q_nonpositive ^ 0x8000u), 2);
  fx::put_be(inv.data, static_cast<std::uint16_t>(0x8000u), 2);
  image img = decode(fx::build_fits({p, inv}));
  EXPECT_EQ(img.invalid.bits[0], invalid_reason::q_nonpositive);
  EXPECT_EQ(img.invalid.bits[1], invalid_reason::source_nonfinite);
  EXPECT_TRUE(decode(encode_fits(img)) == img);
  image produced = make_image({2, 1, 1}, sample_type::f32, plane_layout::mono);
  produced.encoding = source_encoding{-32, 0, 1, 2};
  produced.data<float>()[0] = std::numeric_limits<float>::quiet_NaN();
  produced.invalid.bits[0] = invalid_reason::q_nonpositive;
  EXPECT_TRUE(decode(encode_fits(produced)) == produced);
}

TEST(Fits, REVIEW_declared_geometry_and_rgb_card_are_preserved) {
  fx::raw_hdu line = raw_image(16, {6}, {fx::card_real("BZERO", 32768.0)});
  line.data.assign(12, 0);
  image one_axis = decode(fx::build_fits({line}));
  EXPECT_EQ(one_axis.geometry.width, 6u);
  EXPECT_EQ(one_axis.geometry.height, 1u);
  EXPECT_EQ(fx::parse_hdu(encode_fits(one_axis)).axes, (std::vector<std::uint64_t>{6}));

  fx::raw_hdu three = raw_image(16, {3, 2, 1}, {fx::card_real("BZERO", 32768.0)});
  image flat3 = decode(fx::build_fits({three}));
  EXPECT_EQ(flat3.geometry.planes, 1u);
  EXPECT_EQ(fx::parse_hdu(encode_fits(flat3)).axes, (std::vector<std::uint64_t>{3, 2, 1}));
  EXPECT_TRUE(decode(encode_fits(flat3)) == flat3);

  // CTYPE3 resta dov'e' nel file e non viene duplicato
  const std::vector<std::string> cards = {fx::card_str("OBJECT", "a"), fx::card_str("CTYPE3", "RGB"), fx::card_str("OBJECT2", "b")};
  image rgb = decode(fx::build_fits({raw_image(8, {2, 2, 3}, cards)}));
  EXPECT_EQ(rgb.layout, plane_layout::rgb);
  EXPECT_EQ(rgb.header_cards, cards);
  fx::raw_primary_view v = fx::parse_hdu(encode_fits(rgb));
  EXPECT_EQ(std::count_if(v.cards.begin(), v.cards.end(), [](const std::string& c) { return c.compare(0, 6, "CTYPE3") == 0; }), 1);
  // immagine costruita rgb senza card: la card viene sintetizzata una volta
  image made = make_image({2, 2, 3}, sample_type::f32, plane_layout::rgb);
  fx::raw_primary_view mv = fx::parse_hdu(encode_fits(made));
  EXPECT_EQ(std::count_if(mv.cards.begin(), mv.cards.end(), [](const std::string& c) { return c.compare(0, 6, "CTYPE3") == 0; }), 1);
  EXPECT_EQ(decode(encode_fits(made)).layout, plane_layout::rgb);
  EXPECT_TRUE(equivalent_content(decode(encode_fits(made)), made));
}

TEST(Fits, REVIEW_decode_rechecks_budget_on_the_buffer_it_will_decode) {
  // stessa dimensione di file, picchi diversi: il budget calcolato sul primo non vale per il secondo
  const fx::bytes a = fx::build_fits({raw_image(-64, {360, 1})});
  const fx::bytes b = fx::build_fits({raw_image(8, {2880, 1})});
  ASSERT_EQ(a.size(), b.size());
  const auto peak_a = plan_fits(a).peak_bytes;
  const auto peak_b = plan_fits(b).peak_bytes;
  ASSERT_GT(peak_b, peak_a);
  EXPECT_NO_THROW(decode_fits(a, peak_a));
  EXPECT_STOP(decode_fits(b, peak_a), error_category::budget, "sched.job_exceeds_budget");
  EXPECT_NO_THROW(decode_fits(b, peak_b));
}

TEST(Fits, REVIEW_variance_bytes_match_the_independent_oracle) {
  image img = make_image({3, 2, 1}, sample_type::f32, plane_layout::mono);
  img.encoding = source_encoding{-32, 0, 1, 2};
  img.variance = variance_plane{aligned_buffer(img.samples.size())};
  const float values[6] = {0.0f, 0.5f, 1.25f, 1e-30f, 3.0f, 1024.0f};
  fx::bytes expected;
  for (int i = 0; i < 6; ++i) {
    img.variance_data<float>()[i] = values[i];
    fx::put_be(expected, fx::bits_of(values[i]), 4);
  }
  const fx::bytes file = encode_fits(img);
  EXPECT_EQ(fx::parse_hdu(file, 1).data, expected);
}

TEST(Fits, REVIEW_failed_limit_checks_allocate_no_sample_buffers) {
  const std::uint64_t before = allocated_sample_bytes();
  EXPECT_STOP(decode(fx::build_fits({raw_image(8, {2, 2, 17})})), error_category::limit_exceeded, "fits.max_planes");
  EXPECT_STOP(decode(fx::build_fits({raw_image(8, {65537, 1})})), error_category::limit_exceeded, "fits.max_axis_length");
  EXPECT_STOP(decode(fx::build_fits({raw_image(8, {1, 1, 1, 1})})), error_category::limit_exceeded, "fits.max_naxis");
  fx::raw_hdu big = raw_image(8, {65536, 4097});
  big.data.clear();
  fx::bytes header_only;
  fx::append_header(header_only, big);
  EXPECT_STOP(decode(header_only), error_category::limit_exceeded, "fits.max_samples");
  const fx::bytes ok = fx::build_fits({raw_image(8, {4, 4})});
  EXPECT_STOP(decode_fits(ok, 1), error_category::budget, "sched.job_exceeds_budget");
  EXPECT_STOP(decode(fx::bytes(ok.begin(), ok.begin() + 1000)), error_category::invalid_input, "fits.malformed");
  EXPECT_EQ(allocated_sample_bytes(), before);

  const std::string path = tmp_file("noalloc.fits");
  big.axes = {65536, 4097};
  make_sparse_file(path, big, std::uint64_t{1} << 30);
  fits_input in = open_fits_input(path);
  EXPECT_STOP(read_fits_bytes(in, big_budget), error_category::limit_exceeded, "fits.max_samples");
  EXPECT_EQ(allocated_sample_bytes(), before);
  fs::remove(path);

  decode(ok);  // il contatore vede le allocazioni vere
  EXPECT_GT(allocated_sample_bytes(), before);
}

TEST(Fits, REVIEW_non_dyadic_scales_are_accepted_and_rewritten_bit_exact) {
  struct scale_case {
    int bitpix;
    double bscale;
    double bzero;
    std::vector<std::uint64_t> words;
  };
  const scale_case cases[] = {
      {16, 0.1, 0.0, {0x8000, 0x7FFF, 0x0000, 0x0001, 0xFFFF}},
      {32, 1e-3, 12345.678, {0x80000000ull, 0x7FFFFFFFull, 0, 1, 0xFFFFFFFFull}},
      {16, -1.0, 0.0, {0x8000, 0x7FFF, 0, 5}},
      {32, 1.0, 2147483648.0, {0x80000000ull, 0x7FFFFFFFull, 0, 7}},
      {32, 1.0, -2147483648.0, {0x80000000ull, 0x7FFFFFFFull, 0, 7}},
      {16, 0.1, 100.5, {0x8000, 0x7FFF, 3}},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE("bitpix " + std::to_string(c.bitpix) + " bscale " + std::to_string(c.bscale) + " bzero " +
                 std::to_string(c.bzero));
    const fx::bytes file = int_image(c.bitpix, {fx::card_real("BSCALE", c.bscale), fx::card_real("BZERO", c.bzero)}, c.words);
    image img = decode(file);
    EXPECT_EQ(img.type, sample_type::f64);
    const fx::bytes out = encode_fits(img);
    EXPECT_EQ(fx::parse_hdu(out).data, fx::parse_hdu(file).data);
    EXPECT_TRUE(decode(out) == img);
  }
}

TEST(Fits, REVIEW_structure_mismatch_between_cfitsio_and_scan_is_detected) {
  const fx::bytes file = fx::build_fits({raw_image(16, {2, 2}, {fx::card_real("BZERO", 32768.0)}),
                                         [] {
                                           fx::raw_hdu e = extension("INVALID", 16, {2, 2});
                                           e.cards.push_back(fx::card_real("BZERO", 32768.0));
                                           e.data = {0x80, 0, 0x80, 0, 0x80, 0, 0x80, 0};
                                           return e;
                                         }()});
  fits_detail::scan_result scan = fits_detail::analyse(file.size(), [&](std::uint64_t off, void* dst, std::size_t n) {
    std::memcpy(dst, file.data() + off, n);
  });
  void* buf = const_cast<std::uint8_t*>(file.data());
  std::size_t size = file.size();
  fitsfile* f = nullptr;
  int st = 0;
  ASSERT_EQ(fits_open_memfile(&f, "memory", READONLY, &buf, &size, 0, nullptr, &st), 0);
  EXPECT_NO_THROW(fits_detail::verify_structure(f, scan.hdus));
  auto altered = [&](auto mutate) {
    auto hdus = scan.hdus;
    mutate(hdus);
    return hdus;
  };
  EXPECT_STOP(fits_detail::verify_structure(f, altered([](auto& h) { h[0].data_offset += 2880; })),
              error_category::invalid_input, "fits.structure_mismatch");
  EXPECT_STOP(fits_detail::verify_structure(f, altered([](auto& h) { h[1].data_padded += 2880; })),
              error_category::invalid_input, "fits.structure_mismatch");
  EXPECT_STOP(fits_detail::verify_structure(f, altered([](auto& h) { h[0].bitpix = 32; })),
              error_category::invalid_input, "fits.structure_mismatch");
  EXPECT_STOP(fits_detail::verify_structure(f, altered([](auto& h) { h[0].axes = {4, 1}; })),
              error_category::invalid_input, "fits.structure_mismatch");
  EXPECT_STOP(fits_detail::verify_structure(f, altered([](auto& h) { h[1].header_offset += 1; })),
              error_category::invalid_input, "fits.structure_mismatch");
  fits_close_file(f, &st);
}

// ------------------------------------------------------------------ fuzz strutturale

TEST(Fits, REVIEW_structural_fuzz_accepts_only_what_it_can_write_back) {
  std::uint64_t state = 0xD1B54A32D192ED03ull;
  auto next = [&state] {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
  };
  const std::vector<std::string> vocabulary = {
      fx::card_real("BZERO", 32768.0), fx::card_real("BZERO", 0.0),     fx::card_real("BZERO", -128.0),
      fx::card_real("BSCALE", 1.0),    fx::card_real("BSCALE", 0.5),    fx::card_real("BSCALE", 1e-10),
      fx::card_real("BSCALE", 1e300),  fx::card_real("BZERO", 1e10),    fx::card_real("BSCALE", 0.0),
      fx::card_int("BLANK", 0),        fx::card_int("BITPIX", 16),      fx::card_int("NAXIS1", 4),
      fx::card_int("PCOUNT", -1),      fx::card_int("PCOUNT", 3),       fx::card_real("EXPTIME", 60.0),
      fx::card_str("EXPTIME", "60"),   fx::card_bool("EXPTIME", true),  fx::card_str("FILTER", "L"),
      fx::card_str("FILTER", "a&"),    fx::card_text("CONTINUE", "  'x'"), fx::card_str("BUNIT", "ADU"),
      fx::card_str("BUNIT", "?"),      fx::card_str("BAYERPAT", "RGGB"), fx::card_str("BAYERPAT", "ZZZZ"),
      fx::card_int("XBAYROFF", 0),     fx::card_int("YBAYROFF", 1),     fx::card_int("XBAYROFF", 7),
      fx::card_str("CTYPE3", "RGB"),   fx::card_bool("LINEAR", true),   fx::card_int("LINEAR", 1),
      fx::card_int("XBINNING", 2),     fx::card_int("XBINNING", -2),    fx::card_int("XORGSUBF", 5),
      fx::card_real("XORGSUBF", 5.5),  fx::card_str("EXTNAME", "INVALID"), fx::card_text("COMMENT", "c"),
      fx::card_str("DATASUM", "12345"), fx::card_str("CHECKSUM", "AAAAAAAAAAAAAAAA"),
  };
  const int bitpixes[] = {8, 16, 32, 64, -32, -64, 12, 0};
  const std::vector<std::vector<std::uint64_t>> shapes = {{3, 2}, {6}, {2, 2, 3}, {2, 2, 1}, {4, 1}, {0, 2}, {2, 2, 2, 2}};
  std::size_t accepted = 0;
  std::size_t stopped = 0;
  for (int iter = 0; iter < 4000; ++iter) {
    fx::raw_hdu h;
    h.bitpix = bitpixes[next() % 8];
    h.axes = shapes[next() % shapes.size()];
    const int extras = static_cast<int>(next() % 5);
    for (int k = 0; k < extras; ++k) h.cards.push_back(vocabulary[next() % vocabulary.size()]);
    std::uint64_t n = 1;
    for (auto a : h.axes) n *= a;
    const std::size_t bytes_per = static_cast<std::size_t>(std::abs(h.bitpix) / 8);
    h.data.resize(static_cast<std::size_t>(n) * bytes_per);
    const bool extreme = next() % 3 == 0;
    for (auto& b : h.data) b = extreme ? (next() % 2 ? 0x80 : 0x00) : static_cast<std::uint8_t>(next());
    std::vector<fx::raw_hdu> hdus = {h};
    if (next() % 3 == 0) {
      fx::raw_hdu e = h;
      e.primary = false;
      e.cards = {fx::card_str("EXTNAME", next() % 2 ? "INVALID" : "VARIANCE")};
      if (next() % 2) e.cards.push_back(fx::card_real("BZERO", 32768.0));
      e.bitpix = (next() % 2) ? 16 : h.bitpix;
      e.data.assign(static_cast<std::size_t>(n) * static_cast<std::size_t>(std::abs(e.bitpix) / 8), static_cast<std::uint8_t>(next() % 2 ? 0x80 : 0));
      hdus.push_back(e);
    }
    const fx::bytes file = fx::build_fits(hdus);
    std::optional<image> img;
    try {
      img = decode(file);
    } catch (const stop_error&) {
      ++stopped;
      continue;
    }
    ++accepted;
    validate_image(*img);
    try {
      const fx::bytes out = encode_fits(*img);
      ASSERT_TRUE(equivalent_content(decode(out), *img)) << "iter " << iter;
      ASSERT_EQ(fx::parse_hdu(out).data, h.data) << "iter " << iter;
    } catch (const stop_error& e) {
      ADD_FAILURE() << "file accettato ma non riscrivibile: " << e.what() << " iter " << iter;
    }
  }
  EXPECT_GT(stopped, 500u);
  EXPECT_GT(accepted, 100u);
}

// ------------------------------------------------------------------ dato reale

std::string sha256_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in.good());
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  std::vector<char> buf(1 << 20);
  while (in.read(buf.data(), static_cast<std::streamsize>(buf.size())) || in.gcount() > 0) {
    EVP_DigestUpdate(ctx, buf.data(), static_cast<std::size_t>(in.gcount()));
  }
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned len = 0;
  EVP_DigestFinal_ex(ctx, md, &len);
  EVP_MD_CTX_free(ctx);
  std::string hex;
  char b[3];
  for (unsigned i = 0; i < len; ++i) {
    std::snprintf(b, sizeof b, "%02x", md[i]);
    hex += b;
  }
  return hex;
}

std::string documented_sha256(const std::string& relative) {
  std::ifstream in(fs::path(ASTRO_SOURCE_DIR) / "docs/datasets/PUB-LFC-01.md");
  EXPECT_TRUE(in.good()) << "docs/datasets/PUB-LFC-01.md non leggibile";
  std::string line;
  while (std::getline(in, line)) {
    if (line.size() > 66 && line.compare(64, 2, "  ") == 0 && line.substr(66) == relative) return line.substr(0, 64);
  }
  return {};
}

TEST(Fits, REAL_DATA_PUB_LFC_01_light_is_read_with_documented_digest_and_header) {
  const std::string relative = "raw/example-cryo-LFC/ccd.037.0.fits";
  const fs::path path = fs::path(ASTRO_SOURCE_DIR) / "data/public/zenodo-3254683" / relative;
  ASSERT_TRUE(fs::exists(path)) << "PREREQUISITO MANCANTE: dataset PUB-LFC-01 assente in " << path.string()
                                << " (vedi docs/datasets/PUB-LFC-01.md)";
  const std::string expected = documented_sha256(relative);
  ASSERT_EQ(expected.size(), 64u) << "digest di " << relative << " non trovato in docs/datasets/PUB-LFC-01.md";
  EXPECT_EQ(sha256_file(path.string()), expected);

  image img = read_file(path.string());
  ASSERT_TRUE(img.encoding.has_value());
  EXPECT_EQ(img.encoding->bitpix, 16);
  EXPECT_EQ(img.encoding->bzero, 32768.0);
  EXPECT_EQ(img.geometry.width, 2080u);
  EXPECT_EQ(img.geometry.height, 4128u);
  EXPECT_EQ(img.geometry.planes, 1u);
  EXPECT_EQ(img.layout, plane_layout::mono);
  EXPECT_EQ(img.type, sample_type::f32);
  ASSERT_TRUE(img.metadata.imagetyp.has_value());
  EXPECT_EQ(img.metadata.imagetyp->value, "OBJECT");
  ASSERT_TRUE(img.metadata.exposure_s.has_value());
  EXPECT_DOUBLE_EQ(img.metadata.exposure_s->value, 300.062);
  EXPECT_TRUE(img.metadata.filter.has_value());
  EXPECT_FALSE(img.metadata.conversion_gain_e_per_adu.has_value());
  EXPECT_FALSE(img.cfa.has_value());
  EXPECT_FALSE(img.geometry.binning_x.has_value());

  // roundtrip: la data unit riscritta coincide byte per byte con quella del file originale
  std::ifstream in(path, std::ios::binary);
  fx::bytes original((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const fx::bytes out = encode_fits(img);
  EXPECT_EQ(fx::parse_hdu(out).data, fx::parse_hdu(original).data);
  EXPECT_TRUE(decode(out) == img);
}

}  // namespace
