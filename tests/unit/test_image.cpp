#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "astro/image.hpp"
#include "expect_stop.hpp"

namespace {

using namespace astro;

TEST(Image, T_M2A_01_geometry_rejects_zero_dimensions) {
  EXPECT_STOP(make_image({0, 4, 1}, sample_type::f32, plane_layout::mono), error_category::invalid_input,
              "image.geometry");
  EXPECT_STOP(make_image({4, 0, 1}, sample_type::f32, plane_layout::mono), error_category::invalid_input,
              "image.geometry");
  EXPECT_STOP(make_image({4, 4, 0}, sample_type::f32, plane_layout::mono), error_category::invalid_input,
              "image.geometry");
}

TEST(Image, T_M2A_01_size_overflow_is_detected_before_allocation) {
  constexpr auto max32 = std::numeric_limits<std::uint32_t>::max();
  EXPECT_STOP(make_image({max32, max32, max32}, sample_type::f64, plane_layout::multi_plane),
              error_category::limit_exceeded, "image.size_overflow");
  EXPECT_STOP(make_image({max32, max32, max32 - 1}, sample_type::f64, plane_layout::multi_plane),
              error_category::limit_exceeded, "image.size_overflow");
  EXPECT_STOP(make_image({max32, max32, 1}, sample_type::f64, plane_layout::mono),
              error_category::limit_exceeded, "image.size_overflow");
}

TEST(Image, T_M2A_01_buffers_have_contract_size_and_alignment) {
  for (sample_type t : {sample_type::f32, sample_type::f64}) {
    image img = make_image({7, 5, 3}, t, plane_layout::rgb);
    EXPECT_EQ(img.samples.size(), 7u * 5u * 3u * sample_size(t));
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(img.samples.data()) % 64, 0u);
    EXPECT_EQ(img.invalid.bits.size(), 7u * 5u * 3u);
    EXPECT_EQ(img.invalid.planes, 3u);
    EXPECT_FALSE(img.unit.has_value());
    EXPECT_FALSE(img.lin.has_value());
    EXPECT_FALSE(img.variance.has_value());
  }
}

TEST(Image, T_M2A_01_copy_is_deep_and_move_leaves_source_empty) {
  image a = make_image({4, 4, 1}, sample_type::f32, plane_layout::mono);
  a.data<float>()[3] = 2.5f;
  image b = a;
  b.data<float>()[3] = 9.0f;
  EXPECT_EQ(a.data<float>()[3], 2.5f);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(b.samples.data()) % 64, 0u);
  image c = std::move(a);
  EXPECT_EQ(c.data<float>()[3], 2.5f);
  EXPECT_EQ(a.samples.size(), 0u);
}

TEST(Image, T_M2A_01_layout_must_agree_with_planes_and_cfa) {
  EXPECT_STOP(make_image({4, 4, 1}, sample_type::f32, plane_layout::cfa), error_category::invalid_input,
              "image.layout");
  EXPECT_STOP(make_image({4, 4, 3}, sample_type::f32, plane_layout::mono), error_category::invalid_input,
              "image.layout");
  EXPECT_STOP(make_image({4, 4, 2}, sample_type::f32, plane_layout::rgb), error_category::invalid_input,
              "image.layout");
  EXPECT_STOP(make_image({4, 4, 1}, sample_type::f32, plane_layout::multi_plane),
              error_category::invalid_input, "image.layout");
  image img = make_image({4, 4, 1}, sample_type::f32, plane_layout::mono);
  img.cfa = cfa_layout{cfa_pattern::rggb, 0, 0};
  EXPECT_STOP(validate_image(img), error_category::invalid_input, "image.layout");
  img.layout = plane_layout::cfa;
  validate_image(img);
  img.cfa->x_offset = 2;
  EXPECT_STOP(validate_image(img), error_category::invalid_input, "image.cfa_offset");
  image planar = make_image({4, 4, 3}, sample_type::f32, plane_layout::rgb);
  planar.cfa = cfa_layout{cfa_pattern::rggb, 0, 0};
  EXPECT_STOP(validate_image(planar), error_category::invalid_input, "image.cfa_planes");
}

TEST(Image, T_M2A_01_mask_and_variance_must_match_samples) {
  image img = make_image({4, 4, 1}, sample_type::f32, plane_layout::mono);
  img.invalid.bits.pop_back();
  EXPECT_STOP(validate_image(img), error_category::invalid_input, "image.mask_size");
  image v = make_image({4, 4, 1}, sample_type::f32, plane_layout::mono);
  v.variance = variance_plane{aligned_buffer(8)};
  EXPECT_STOP(validate_image(v), error_category::invalid_input, "image.variance_size");
}

TEST(Image, InvalidReasonBitsMatchContract) {
  using namespace invalid_reason;
  EXPECT_EQ(source_nonfinite, 1u << 0);
  EXPECT_EQ(source_flagged, 1u << 1);
  EXPECT_EQ(saturated, 1u << 2);
  EXPECT_EQ(master_invalid, 1u << 3);
  EXPECT_EQ(q_nonpositive, 1u << 4);
  EXPECT_EQ(q_nonfinite, 1u << 5);
  EXPECT_EQ(q_below_min, 1u << 6);
  EXPECT_EQ(q_unreliable, 1u << 7);
  EXPECT_EQ(result_nonfinite, 1u << 8);
  EXPECT_EQ(reserved_mask, 0xFE00u);
}

TEST(Image, T_M2A_08_operator_output_invariants) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  image img = make_image({3, 3, 1}, sample_type::f32, plane_layout::mono);
  img.variance = variance_plane{aligned_buffer(img.samples.size())};
  EXPECT_TRUE(operator_output_invariants_hold(img));

  img.data<float>()[4] = nan;  // non finito senza bit
  EXPECT_FALSE(operator_output_invariants_hold(img));
  img.data<float>()[4] = std::numeric_limits<float>::infinity();
  img.invalid.bits[4] = invalid_reason::result_nonfinite;  // bit acceso ma campione Inf, non NaN
  EXPECT_FALSE(operator_output_invariants_hold(img));
  img.data<float>()[4] = nan;  // bit acceso e campione NaN ma varianza finita
  EXPECT_FALSE(operator_output_invariants_hold(img));
  img.variance_data<float>()[4] = nan;
  EXPECT_TRUE(operator_output_invariants_hold(img));
  img.invalid.bits[0] = invalid_reason::q_nonpositive;  // bit acceso, campione finito
  EXPECT_FALSE(operator_output_invariants_hold(img));
}

TEST(Image, T_M2A_08_edit_mask_is_a_distinct_type_without_conversions) {
  static_assert(!std::is_convertible_v<edit_mask, invalid_mask>);
  static_assert(!std::is_convertible_v<invalid_mask, edit_mask>);
  static_assert(!std::is_constructible_v<invalid_mask, edit_mask>);
  static_assert(!std::is_constructible_v<edit_mask, invalid_mask>);
  static_assert(!std::is_convertible_v<edit_mask, image_geometry>);
  static_assert(!std::is_assignable_v<invalid_mask&, edit_mask>);

  image img = make_image({3, 3, 1}, sample_type::f32, plane_layout::mono);
  img.invalid.bits[2] = invalid_reason::saturated;
  const image before = img;
  edit_mask edit(3, 3);
  edit.set(1, 1, 0.5f);
  edit.set(2, 2, 1.0f);
  EXPECT_EQ(edit.at(1, 1), 0.5f);
  EXPECT_TRUE(img == before);
  EXPECT_STOP(edit.set(0, 0, 1.5f), error_category::invalid_input, "mask.edit_range");
  EXPECT_STOP(edit.set(0, 0, -0.1f), error_category::invalid_input, "mask.edit_range");
  EXPECT_STOP(edit.set(0, 0, std::nanf("")), error_category::invalid_input, "mask.edit_range");
  EXPECT_STOP(edit.at(3, 0), error_category::invalid_input, "mask.edit_index");
}

TEST(Image, T_M2A_04_declared_metadata_never_overrides_a_different_value) {
  std::optional<metadata_value<double>> field;
  declare_metadata(field, 30.0, "exposure_s");
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->source.origin, metadata_origin::declared);
  declare_metadata(field, 30.0, "exposure_s");
  EXPECT_STOP(declare_metadata(field, 31.0, "exposure_s"), error_category::incompatible,
              "metadata.declared_conflict");

  std::optional<metadata_value<double>> from_header =
      metadata_value<double>{60.0, {metadata_origin::header, "EXPTIME"}};
  declare_metadata(from_header, 60.0, "exposure_s");
  EXPECT_EQ(from_header->source.origin, metadata_origin::header);
  EXPECT_STOP(declare_metadata(from_header, 59.0, "exposure_s"), error_category::incompatible,
              "metadata.declared_conflict");
}

}  // namespace

namespace {

TEST(ImageDeathTest, TypedAccessorAssertsOnTypeMismatch) {
  astro::image img = astro::make_image({2, 2, 1}, astro::sample_type::f32, astro::plane_layout::mono);
  EXPECT_DEATH((void)img.data<double>(), "ASTRO_ASSERT");
  EXPECT_DEATH((void)img.variance_data<float>(), "ASTRO_ASSERT");
  EXPECT_NE(img.data<float>(), nullptr);
}

TEST(Image, AllocationCounterTracksSampleBuffers) {
  const std::uint64_t before = astro::allocated_sample_bytes();
  astro::image img = astro::make_image({8, 8, 1}, astro::sample_type::f64, astro::plane_layout::mono);
  EXPECT_GE(astro::allocated_sample_bytes() - before, 8u * 8u * 8u + 8u * 8u * 2u);
}

}  // namespace
