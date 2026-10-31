// U1b (M3.a): capacità di misura `measure`. T-M3A-TYP-15 e T-M3A-TYP-16
// (docs/contracts/ADR-03-pixel-maschere.md, parte .b: §b.13, §b.16, §b.17).
// Valori attesi scritti a mano dal contratto; la verità di riferimento per le card FITS è il writer di syn_fits.hpp.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "astro/fits.hpp"
#include "astro/fits_blocks.hpp"
#include "astro/hash.hpp"
#include "astro/image.hpp"
#include "expect_stop.hpp"
#include "syn_fits.hpp"

namespace {

using namespace astro;
namespace fx = astro_fixture;
using ml = measure_limit;

constexpr std::uint64_t big_budget = std::uint64_t{1} << 40;

image decode(const fx::bytes& b) { return decode_fits(b, big_budget); }

// Codici di §b.13 nell'ordine canonico della tabella. Il testo è quello del contratto, scritto a mano.
struct code_row {
  ml limit;
  const char* text;
  bool stored;
};

const std::vector<code_row>& table() {
  static const std::vector<code_row> t = {
      {ml::history_unknown, "history.unknown", true},
      {ml::calib_omitted_dark_subtraction, "calib.omitted.dark_subtraction", true},
      {ml::calib_omitted_bias_subtraction, "calib.omitted.bias_subtraction", true},
      {ml::calib_omitted_dark_flat_subtraction, "calib.omitted.dark_flat_subtraction", true},
      {ml::calib_omitted_flat_bias_subtraction, "calib.omitted.flat_bias_subtraction", true},
      {ml::calib_omitted_flat_normalization, "calib.omitted.flat_normalization", true},
      {ml::calib_omitted_flat_division, "calib.omitted.flat_division", true},
      {ml::calib_offset_known_not_removed, "calib.offset.known_not_removed", true},
      {ml::calib_offset_unknown, "calib.offset.unknown", true},
      {ml::calib_assumption_h_dprime, "calib.assumption.h_dprime", true},
      {ml::calib_assumption_flat_thermal, "calib.assumption.flat_thermal", true},
      {ml::calib_assumption_decoder_black, "calib.assumption.decoder_black", true},
      {ml::linearity_nonlinear, "linearity.nonlinear", false},
      {ml::linearity_unknown, "linearity.unknown", false},
      {ml::variance_absent, "variance.absent", false},
      {ml::variance_scope_unknown, "variance.scope_unknown", false},
      {ml::variance_marginal, "variance.marginal", false},
      {ml::variance_fixed_weight_approximation, "variance.fixed_weight_approximation", true},
      {ml::samples_interpolated, "samples.interpolated", true},
  };
  return t;
}

std::vector<code_row> stored_rows() {
  std::vector<code_row> r;
  for (const code_row& c : table()) {
    if (c.stored) r.push_back(c);
  }
  return r;
}

std::vector<code_row> derived_rows() {
  std::vector<code_row> r;
  for (const code_row& c : table()) {
    if (!c.stored) r.push_back(c);
  }
  return r;
}

using limits_t = std::vector<ml>;

// Immagine di base di U1b: lin = linear, varianza assente, measure vuota. Le sole limitazioni derivate: `variance.absent`.
image base_image() {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.lin = linearity::linear;
  img.lin_origin.value = metadata_origin::header;
  img.measure = measure_capability{};
  return img;
}

image with_limits(limits_t limits) {
  image img = base_image();
  img.measure = measure_capability{std::move(limits)};
  return img;
}

image plain_image(std::optional<process_phase> phase) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  if (phase) img.phase = metadata_value<process_phase>{*phase, {metadata_origin::header, "ASTRPHAS"}};
  return img;
}

std::string joined_context(const stop_error& e) {
  std::string all;
  for (const auto& kv : e.context()) all += kv.second + "\n";
  return all;
}

// ---------------------------------------------------------------- tabella dei codici

TEST(Measure, T_M3A_TYP_15_table_has_19_codes_of_which_14_stored) {
  EXPECT_EQ(table().size(), 19u);
  EXPECT_EQ(stored_rows().size(), 14u);
  EXPECT_EQ(derived_rows().size(), 5u);
}

TEST(Measure, T_M3A_TYP_15_every_code_has_the_contract_text_and_canonical_order) {
  for (const code_row& c : table()) {
    SCOPED_TRACE(c.text);
    EXPECT_STREQ(to_string(c.limit), c.text);
  }
  for (std::size_t i = 1; i < table().size(); ++i) {
    EXPECT_LT(static_cast<int>(table()[i - 1].limit), static_cast<int>(table()[i].limit)) << table()[i].text;
  }
}

// ---------------------------------------------------------------- T-M3A-TYP-15: validate_image

TEST(Measure, T_M3A_TYP_15_absent_empty_and_canonical_measure_are_valid) {
  image img = base_image();
  img.measure.reset();
  EXPECT_NO_THROW(validate_image(img));
  img.measure = measure_capability{};
  EXPECT_NO_THROW(validate_image(img));
  limits_t all;
  for (const code_row& c : stored_rows()) all.push_back(c.limit);
  EXPECT_NO_THROW(validate_image(with_limits(all)));
  EXPECT_NO_THROW(validate_image(with_limits({ml::history_unknown})));
}

TEST(Measure, T_M3A_TYP_15_each_derived_code_is_refused) {
  for (const code_row& c : derived_rows()) {
    SCOPED_TRACE(c.text);
    EXPECT_STOP(validate_image(with_limits({c.limit})), error_category::invalid_input, "image.measure");
  }
  EXPECT_STOP(validate_image(with_limits({ml::history_unknown, ml::variance_marginal, ml::samples_interpolated})),
              error_category::invalid_input, "image.measure");
}

TEST(Measure, T_M3A_TYP_15_each_stored_code_duplicated_is_refused) {
  for (const code_row& c : stored_rows()) {
    SCOPED_TRACE(c.text);
    EXPECT_STOP(validate_image(with_limits({c.limit, c.limit})), error_category::invalid_input, "image.measure");
  }
  EXPECT_STOP(validate_image(with_limits({ml::history_unknown, ml::calib_offset_unknown, ml::history_unknown})),
              error_category::invalid_input, "image.measure");
}

TEST(Measure, T_M3A_TYP_15_out_of_order_is_refused) {
  EXPECT_STOP(validate_image(with_limits({ml::calib_assumption_h_dprime, ml::history_unknown})),
              error_category::invalid_input, "image.measure");
  EXPECT_STOP(validate_image(with_limits({ml::samples_interpolated, ml::variance_fixed_weight_approximation})),
              error_category::invalid_input, "image.measure");
  EXPECT_STOP(validate_image(with_limits({ml::calib_omitted_flat_division, ml::calib_omitted_dark_subtraction})),
              error_category::invalid_input, "image.measure");
  // ogni adiacente della lista dei 14 invertito, uno alla volta
  const auto rows = stored_rows();
  for (std::size_t i = 1; i < rows.size(); ++i) {
    SCOPED_TRACE(rows[i].text);
    EXPECT_STOP(validate_image(with_limits({rows[i].limit, rows[i - 1].limit})), error_category::invalid_input,
                "image.measure");
  }
}

// ---------------------------------------------------------------- T-M3A-TYP-15: FITS

std::optional<std::string> find_card(const fx::bytes& file, const std::string& key) {
  const fx::raw_primary_view v = fx::parse_hdu(file);
  std::string padded = key;
  padded.resize(8, ' ');
  for (const std::string& c : v.cards) {
    if (c.size() >= 10 && c.compare(0, 8, padded) == 0 && c.compare(8, 2, "= ") == 0) return c;
  }
  return std::nullopt;
}

std::string trimmed(std::string s) {
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

// La card scritta dal prodotto comincia con la card dell'oracle (il commento, se c'è, viene dopo).
bool card_starts_with(const std::optional<std::string>& card, const std::string& expected) {
  const std::string e = trimmed(expected);
  return card.has_value() && card->compare(0, e.size(), e) == 0;
}

std::string ml_key(std::size_t n) {
  char b[32];
  std::snprintf(b, sizeof b, "ASTRML%02zu", n);
  return b;
}

fx::bytes file_with(std::vector<std::string> cards) {
  fx::raw_hdu h;
  h.bitpix = 16;
  h.axes = {2, 2};
  h.cards = std::move(cards);
  h.data.assign(2 * 2 * 2, 0);
  return fx::build_fits({h});
}

void expect_no_measure_cards(const fx::bytes& file) {
  EXPECT_FALSE(find_card(file, "ASTRMCAP").has_value());
  for (std::size_t n = 1; n <= 99; ++n) EXPECT_FALSE(find_card(file, ml_key(n)).has_value()) << ml_key(n);
}

TEST(Measure, T_M3A_TYP_15_fits_with_measure_absent_writes_no_card_and_reads_absent) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  ASSERT_FALSE(img.measure.has_value());
  const fx::bytes file = encode_fits(img);
  expect_no_measure_cards(file);
  const image back = decode(file);
  EXPECT_FALSE(back.measure.has_value());
}

TEST(Measure, T_M3A_TYP_15_fits_with_zero_codes_writes_only_astrmcap) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.measure = measure_capability{};
  const fx::bytes file = encode_fits(img);
  EXPECT_TRUE(card_starts_with(find_card(file, "ASTRMCAP"), fx::card_bool("ASTRMCAP", true)));
  for (std::size_t n = 1; n <= 99; ++n) EXPECT_FALSE(find_card(file, ml_key(n)).has_value()) << ml_key(n);
  const image back = decode(file);
  ASSERT_TRUE(back.measure.has_value());
  EXPECT_TRUE(back.measure->limits.empty());
  EXPECT_TRUE(equivalent_content(back, img));
}

TEST(Measure, T_M3A_TYP_15_fits_with_one_code_writes_astrml01) {
  for (const code_row& c : stored_rows()) {
    SCOPED_TRACE(c.text);
    image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
    img.measure = measure_capability{{c.limit}};
    const fx::bytes file = encode_fits(img);
    EXPECT_TRUE(card_starts_with(find_card(file, "ASTRMCAP"), fx::card_bool("ASTRMCAP", true)));
    EXPECT_TRUE(card_starts_with(find_card(file, "ASTRML01"), fx::card_str("ASTRML01", c.text)));
    EXPECT_FALSE(find_card(file, "ASTRML02").has_value());
    const image back = decode(file);
    ASSERT_TRUE(back.measure.has_value());
    EXPECT_EQ(back.measure->limits, limits_t{c.limit});
    EXPECT_TRUE(equivalent_content(back, img));
  }
}

TEST(Measure, T_M3A_TYP_15_fits_with_14_codes_numbers_them_without_gaps_in_canonical_order) {
  const auto rows = stored_rows();
  ASSERT_EQ(rows.size(), 14u);
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  limits_t all;
  for (const code_row& c : rows) all.push_back(c.limit);
  img.measure = measure_capability{all};
  const fx::bytes file = encode_fits(img);
  EXPECT_TRUE(card_starts_with(find_card(file, "ASTRMCAP"), fx::card_bool("ASTRMCAP", true)));
  for (std::size_t i = 0; i < rows.size(); ++i) {
    SCOPED_TRACE(ml_key(i + 1));
    EXPECT_TRUE(card_starts_with(find_card(file, ml_key(i + 1)), fx::card_str(ml_key(i + 1), rows[i].text)));
  }
  for (std::size_t n = 15; n <= 99; ++n) EXPECT_FALSE(find_card(file, ml_key(n)).has_value()) << ml_key(n);
  const image back = decode(file);
  ASSERT_TRUE(back.measure.has_value());
  EXPECT_EQ(back.measure->limits, all);
  EXPECT_TRUE(equivalent_content(back, img));
  EXPECT_EQ(back.measure, img.measure);
}

TEST(Measure, T_M3A_TYP_15_measure_cards_are_interpreted_keywords) {
  const image img = decode(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "history.unknown")}));
  for (const std::string& k : img.uninterpreted_keywords) {
    EXPECT_NE(k, "ASTRMCAP");
    EXPECT_NE(k, "ASTRML01");
  }
}

// File scritti dall'oracle (non dal prodotto): letture valide.
TEST(Measure, T_M3A_TYP_15_read_valid_files_from_the_oracle_writer) {
  const image none = decode(file_with({}));
  EXPECT_FALSE(none.measure.has_value());
  const image empty = decode(file_with({fx::card_bool("ASTRMCAP", true)}));
  ASSERT_TRUE(empty.measure.has_value());
  EXPECT_TRUE(empty.measure->limits.empty());
  const image two = decode(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "calib.omitted.flat_division"),
                                      fx::card_str("ASTRML02", "samples.interpolated")}));
  ASSERT_TRUE(two.measure.has_value());
  EXPECT_EQ(two.measure->limits, (limits_t{ml::calib_omitted_flat_division, ml::samples_interpolated}));
}

// ---- ogni caso di lettura di §b.16, uno alla volta

#define EXPECT_KEYWORDS_STOP(file) EXPECT_STOP(decode(file), error_category::invalid_input, "fits.measure_keywords")

TEST(Measure, T_M3A_TYP_15_read_astrmcap_false) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_bool("ASTRMCAP", false)}));
}

TEST(Measure, T_M3A_TYP_15_read_astrmcap_false_with_a_code) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_bool("ASTRMCAP", false), fx::card_str("ASTRML01", "history.unknown")}));
}

TEST(Measure, T_M3A_TYP_15_read_code_card_without_astrmcap) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_str("ASTRML01", "history.unknown")}));
}

TEST(Measure, T_M3A_TYP_15_read_numbering_with_a_gap) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "history.unknown"),
                                  fx::card_str("ASTRML03", "calib.offset.unknown")}));
}

TEST(Measure, T_M3A_TYP_15_read_numbering_not_starting_at_01) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML02", "history.unknown")}));
}

TEST(Measure, T_M3A_TYP_15_read_non_canonical_order) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "calib.assumption.h_dprime"),
                                  fx::card_str("ASTRML02", "history.unknown")}));
}

TEST(Measure, T_M3A_TYP_15_read_duplicated_code) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "history.unknown"),
                                  fx::card_str("ASTRML02", "history.unknown")}));
}

TEST(Measure, T_M3A_TYP_15_read_each_derived_code) {
  for (const code_row& c : derived_rows()) {
    SCOPED_TRACE(c.text);
    EXPECT_STOP(decode(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", c.text)})),
                error_category::unsupported, "fits.measure_code");
  }
}

TEST(Measure, T_M3A_TYP_15_read_unknown_code) {
  EXPECT_STOP(decode(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "calib.unheard.of")})),
              error_category::unsupported, "fits.measure_code");
  EXPECT_STOP(decode(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "history.unknown"),
                                fx::card_str("ASTRML02", "zzz")})),
              error_category::unsupported, "fits.measure_code");
}

TEST(Measure, T_M3A_TYP_15_read_astrmcap_as_string) {
  EXPECT_STOP(decode(file_with({fx::card_str("ASTRMCAP", "T")})), error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(file_with({fx::card_str("ASTRMCAP", "TRUE")})), error_category::invalid_input,
              "fits.keyword_value");
}

TEST(Measure, T_M3A_TYP_15_read_code_card_that_is_not_a_string) {
  EXPECT_STOP(decode(file_with({fx::card_bool("ASTRMCAP", true), fx::card_int("ASTRML01", 1)})),
              error_category::invalid_input, "fits.keyword_value");
  EXPECT_STOP(decode(file_with({fx::card_bool("ASTRMCAP", true), fx::card_bool("ASTRML01", true)})),
              error_category::invalid_input, "fits.keyword_value");
}

// ---------------------------------------------------------------- T-M3A-TYP-15: equivalent_content

TEST(Measure, T_M3A_TYP_15_equivalent_content_distinguishes_a_single_code) {
  const image a = with_limits({ml::calib_offset_unknown});
  const image b = with_limits({ml::calib_offset_unknown, ml::samples_interpolated});
  const image c = with_limits({ml::history_unknown});
  const image d = with_limits({ml::calib_offset_known_not_removed});
  ASSERT_NO_THROW(validate_image(a));
  ASSERT_NO_THROW(validate_image(b));
  ASSERT_NO_THROW(validate_image(c));
  ASSERT_NO_THROW(validate_image(d));
  EXPECT_FALSE(equivalent_content(a, b));
  EXPECT_FALSE(equivalent_content(b, a));
  EXPECT_FALSE(equivalent_content(a, c));
  EXPECT_FALSE(equivalent_content(a, d));
  EXPECT_FALSE(equivalent_content(with_limits({}), a));
  EXPECT_TRUE(equivalent_content(a, with_limits({ml::calib_offset_unknown})));
}

TEST(Measure, T_M3A_TYP_15_equivalent_content_distinguishes_present_from_absent) {
  const image present = base_image();
  image absent = base_image();
  absent.measure.reset();
  ASSERT_NO_THROW(validate_image(present));
  ASSERT_NO_THROW(validate_image(absent));
  EXPECT_FALSE(equivalent_content(present, absent));
  EXPECT_FALSE(equivalent_content(absent, present));
  EXPECT_TRUE(equivalent_content(present, base_image()));
  EXPECT_TRUE(equivalent_content(absent, absent));
}

// ---------------------------------------------------------------- T-M3A-TYP-16: history_limits

TEST(Measure, T_M3A_TYP_16_history_limits_measure_absent_by_phase) {
  EXPECT_EQ(history_limits(plain_image(process_phase::raw)), limits_t{});
  EXPECT_EQ(history_limits(plain_image(process_phase::calibrated)), limits_t{ml::history_unknown});
  EXPECT_EQ(history_limits(plain_image(process_phase::prestacked)), limits_t{ml::history_unknown});
  EXPECT_EQ(history_limits(plain_image(std::nullopt)), limits_t{ml::history_unknown});
}

TEST(Measure, T_M3A_TYP_16_history_limits_measure_present_is_returned_as_is) {
  for (std::optional<process_phase> phase :
       {std::optional<process_phase>{process_phase::raw}, std::optional<process_phase>{process_phase::calibrated},
        std::optional<process_phase>{process_phase::prestacked}, std::optional<process_phase>{}}) {
    image empty = plain_image(phase);
    empty.measure = measure_capability{};
    EXPECT_EQ(history_limits(empty), limits_t{});
    image two = plain_image(phase);
    two.measure = measure_capability{{ml::calib_assumption_h_dprime, ml::samples_interpolated}};
    EXPECT_EQ(history_limits(two), (limits_t{ml::calib_assumption_h_dprime, ml::samples_interpolated}));
  }
}

// ---------------------------------------------------------------- T-M3A-TYP-16: measure_limits

TEST(Measure, T_M3A_TYP_16_base_image_has_only_variance_absent) {
  const image img = base_image();
  ASSERT_NO_THROW(validate_image(img));
  EXPECT_EQ(measure_limits(img), limits_t{ml::variance_absent});
}

TEST(Measure, T_M3A_TYP_16_each_derived_condition_alone) {
  {
    image img = base_image();
    img.lin = linearity::nonlinear;
    EXPECT_EQ(measure_limits(img), (limits_t{ml::linearity_nonlinear, ml::variance_absent}));
  }
  {
    image img = base_image();
    img.lin.reset();
    img.lin_origin.value.reset();
    EXPECT_EQ(measure_limits(img), (limits_t{ml::linearity_unknown, ml::variance_absent}));
  }
}

TEST(Measure, T_M3A_TYP_16_constructible_combination_without_linearity_and_variance) {
  image img = base_image();
  img.lin.reset();
  img.lin_origin.value.reset();
  EXPECT_EQ(measure_limits(img), (limits_t{ml::linearity_unknown, ml::variance_absent}));
}

TEST(Measure, T_M3A_TYP_16_stored_and_derived_are_merged_in_canonical_order) {
  image img = with_limits({ml::history_unknown, ml::calib_assumption_h_dprime, ml::variance_fixed_weight_approximation});
  img.lin = linearity::nonlinear;
  EXPECT_EQ(measure_limits(img),
            (limits_t{ml::history_unknown, ml::calib_assumption_h_dprime, ml::linearity_nonlinear, ml::variance_absent,
                      ml::variance_fixed_weight_approximation}));
}

TEST(Measure, T_M3A_TYP_16_measure_absent_takes_history_from_the_phase) {
  image raw = base_image();
  raw.measure.reset();
  raw.phase = metadata_value<process_phase>{process_phase::raw, {metadata_origin::header, "ASTRPHAS"}};
  EXPECT_EQ(measure_limits(raw), limits_t{ml::variance_absent});
  image calibrated = base_image();
  calibrated.measure.reset();
  calibrated.phase = metadata_value<process_phase>{process_phase::calibrated, {metadata_origin::header, "ASTRPHAS"}};
  EXPECT_EQ(measure_limits(calibrated), (limits_t{ml::history_unknown, ml::variance_absent}));
  calibrated.lin = linearity::nonlinear;
  EXPECT_EQ(measure_limits(calibrated),
            (limits_t{ml::history_unknown, ml::linearity_nonlinear, ml::variance_absent}));
}

// ---------------------------------------------------------------- T-M3A-TYP-16: merge_measure

TEST(Measure, T_M3A_TYP_16_merge_overlapping_inputs_and_an_own_code) {
  const image a = with_limits({ml::calib_omitted_dark_subtraction, ml::calib_assumption_h_dprime});
  const image b = with_limits({ml::calib_omitted_dark_subtraction, ml::samples_interpolated});
  const measure_capability m = merge_measure({&a, &b}, {ml::calib_offset_unknown});
  EXPECT_EQ(m.limits, (limits_t{ml::calib_omitted_dark_subtraction, ml::calib_offset_unknown,
                                ml::calib_assumption_h_dprime, ml::samples_interpolated}));
  const measure_capability swapped = merge_measure({&b, &a}, {ml::calib_offset_unknown});
  EXPECT_EQ(swapped.limits, m.limits);
}

TEST(Measure, T_M3A_TYP_16_merge_sorts_and_deduplicates_the_own_codes) {
  const image a = with_limits({ml::history_unknown});
  const measure_capability m =
      merge_measure({&a}, {ml::samples_interpolated, ml::history_unknown, ml::samples_interpolated});
  EXPECT_EQ(m.limits, (limits_t{ml::history_unknown, ml::samples_interpolated}));
}

TEST(Measure, T_M3A_TYP_16_merge_takes_history_from_inputs_without_measure) {
  const image raw = plain_image(process_phase::raw);
  const image calibrated = plain_image(process_phase::calibrated);
  const image unphased = plain_image(std::nullopt);
  EXPECT_EQ(merge_measure({&raw}, {}).limits, limits_t{});
  EXPECT_EQ(merge_measure({&raw, &calibrated}, {}).limits, limits_t{ml::history_unknown});
  EXPECT_EQ(merge_measure({&unphased}, {ml::calib_assumption_flat_thermal}).limits,
            (limits_t{ml::history_unknown, ml::calib_assumption_flat_thermal}));
}

TEST(Measure, T_M3A_TYP_16_merge_never_drops_a_stored_code_and_adds_no_derived_one) {
  limits_t all;
  for (const code_row& c : stored_rows()) all.push_back(c.limit);
  image a = with_limits(all);
  a.lin = linearity::nonlinear;
  const measure_capability m = merge_measure({&a}, {});
  EXPECT_EQ(m.limits, all);
}

TEST(Measure, T_M3A_TYP_16_merge_without_inputs) {
  EXPECT_EQ(merge_measure({}, {}).limits, limits_t{});
  EXPECT_EQ(merge_measure({}, {ml::calib_assumption_decoder_black, ml::calib_omitted_flat_division}).limits,
            (limits_t{ml::calib_omitted_flat_division, ml::calib_assumption_decoder_black}));
}

TEST(Measure, T_M3A_TYP_16_merge_output_is_valid_in_an_image) {
  const image a = with_limits({ml::calib_omitted_flat_bias_subtraction});
  const image b = with_limits({ml::calib_assumption_flat_thermal});
  image out = base_image();
  out.measure = merge_measure({&a, &b}, {ml::history_unknown});
  EXPECT_NO_THROW(validate_image(out));
}

// ---------------------------------------------------------------- T-M3A-TYP-16: assess_measure, require_measure_ready

// Limitazioni: h_dprime (memorizzato), nonlinear e variance.absent (derivati), samples.interpolated (memorizzato).
image assessed_image() {
  image img = with_limits({ml::calib_assumption_h_dprime, ml::samples_interpolated});
  img.lin = linearity::nonlinear;
  return img;
}

const limits_t assessed_limits = {ml::calib_assumption_h_dprime, ml::linearity_nonlinear, ml::variance_absent,
                                  ml::samples_interpolated};

TEST(Measure, T_M3A_TYP_16_assess_with_empty_acceptance_blocks_everything) {
  const measure_assessment r = assess_measure(assessed_image(), measure_acceptance{});
  EXPECT_EQ(r.limits, assessed_limits);
  EXPECT_EQ(r.blocking, assessed_limits);
}

TEST(Measure, T_M3A_TYP_16_assess_with_complete_acceptance_blocks_nothing) {
  const measure_assessment r = assess_measure(assessed_image(), measure_acceptance{assessed_limits});
  EXPECT_EQ(r.limits, assessed_limits);
  EXPECT_EQ(r.blocking, limits_t{});
}

TEST(Measure, T_M3A_TYP_16_assess_with_partial_acceptance_blocks_the_rest_in_canonical_order) {
  const measure_assessment r =
      assess_measure(assessed_image(), measure_acceptance{{ml::variance_absent, ml::calib_assumption_h_dprime}});
  EXPECT_EQ(r.limits, assessed_limits);
  EXPECT_EQ(r.blocking, (limits_t{ml::linearity_nonlinear, ml::samples_interpolated}));
}

TEST(Measure, T_M3A_TYP_16_assess_ignores_accepted_codes_the_image_does_not_have) {
  const measure_assessment r = assess_measure(
      assessed_image(), measure_acceptance{{ml::history_unknown, ml::variance_marginal, ml::linearity_nonlinear}});
  EXPECT_EQ(r.limits, assessed_limits);
  EXPECT_EQ(r.blocking, (limits_t{ml::calib_assumption_h_dprime, ml::variance_absent, ml::samples_interpolated}));
}

TEST(Measure, T_M3A_TYP_16_require_ready_with_blocking_stops_with_the_codes_in_the_context) {
  try {
    require_measure_ready(assessed_image(), measure_acceptance{{ml::variance_absent, ml::calib_assumption_h_dprime}});
    ADD_FAILURE() << "stop_error atteso";
  } catch (const stop_error& e) {
    EXPECT_EQ(e.category(), error_category::incompatible);
    EXPECT_EQ(e.code(), "measure.limit_blocked");
    const std::string ctx = joined_context(e);
    EXPECT_NE(ctx.find("linearity.nonlinear"), std::string::npos) << ctx;
    EXPECT_NE(ctx.find("samples.interpolated"), std::string::npos) << ctx;
    EXPECT_EQ(ctx.find("variance.absent"), std::string::npos) << ctx;
    EXPECT_EQ(ctx.find("calib.assumption.h_dprime"), std::string::npos) << ctx;
  }
}

TEST(Measure, T_M3A_TYP_16_require_ready_with_empty_blocking_does_not_stop) {
  EXPECT_NO_THROW(require_measure_ready(base_image(), measure_acceptance{{ml::variance_absent}}));
  EXPECT_NO_THROW(require_measure_ready(assessed_image(), measure_acceptance{assessed_limits}));
  EXPECT_STOP(require_measure_ready(assessed_image(), measure_acceptance{}), error_category::incompatible,
              "measure.limit_blocked");
}

// ---------------------------------------------------------------- varianza, FITS, lettore a blocchi, merge

TEST(Measure, T_M3A_TYP_16_variance_present_removes_variance_absent) {
  image img = base_image();
  img.variance = variance_plane{aligned_buffer(img.samples.size())};
  ASSERT_NO_THROW(validate_image(img));
  EXPECT_EQ(measure_limits(img), limits_t{});
  img.lin = linearity::nonlinear;
  EXPECT_EQ(measure_limits(img), limits_t{ml::linearity_nonlinear});
  EXPECT_NO_THROW(require_measure_ready(img, measure_acceptance{{ml::linearity_nonlinear}}));
}

TEST(Measure, T_M3A_TYP_15_read_astrml99_alone_is_a_numbering_gap) {
  EXPECT_KEYWORDS_STOP(file_with({fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML99", "history.unknown")}));
}

TEST(Measure, T_M3A_TYP_15_writer_stops_on_a_stale_measure_card_in_header_cards) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.measure = measure_capability{{ml::history_unknown}};
  img.header_cards = {fx::card_bool("ASTRMCAP", true), fx::card_str("ASTRML01", "calib.offset.unknown")};
  EXPECT_STOP(encode_fits(img), error_category::invalid_input, "fits.header_inconsistent");
}

TEST(Measure, T_M3A_TYP_15_stream_writer_keeps_the_measure_cards) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.measure = measure_capability{{ml::history_unknown, ml::samples_interpolated}};
  EXPECT_NO_THROW(fits_stream_writer(img, false));
}

TEST(Measure, T_M3A_TYP_15_block_source_header_carries_the_measure) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.measure = measure_capability{{ml::calib_offset_unknown, ml::samples_interpolated}};
  const fx::bytes file = encode_fits(img);
  const std::filesystem::path dir = std::filesystem::path(ASTRO_TEST_TMP) / "measure";
  std::filesystem::create_directories(dir);
  const std::filesystem::path path = dir / "measure.fits";
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }
  fits_block_source src(path.string(), digest_of(file), {}, 2, false);
  EXPECT_EQ(src.header().measure, img.measure);
}

TEST(Measure, T_M3A_TYP_16_merge_refuses_a_derived_code_of_its_own) {
  const image a = with_limits({ml::history_unknown});
  EXPECT_STOP(merge_measure({&a}, {ml::variance_absent}), error_category::invalid_input, "image.measure");
  EXPECT_STOP(merge_measure({}, {ml::linearity_unknown}), error_category::invalid_input, "image.measure");
}

// OB-20 (BASE-confini §2): ogni uscita di operatore porta `measure`, anche vuoto; senza, le invarianti non valgono.
TEST(MeasureOutput, T_M3BASE_OB_20_output_with_empty_measure_holds_the_invariants) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  img.measure = measure_capability{};
  EXPECT_TRUE(operator_output_invariants_hold(img));
}

TEST(MeasureOutput, T_M3BASE_OB_20_output_without_measure_violates_the_invariants) {
  image img = make_image({4, 3, 1}, sample_type::f32, plane_layout::mono);
  ASSERT_FALSE(img.measure.has_value());
  EXPECT_FALSE(operator_output_invariants_hold(img));
}

}  // namespace
