#include <unity.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/motion_features.hpp>
#include <wledimuudp/synthetic_audio_mapper.hpp>

#include "../fixtures/motion_traces.hpp"

namespace {

using wledimuudp::mapping::MappingConfig;
using wledimuudp::mapping::SyntheticAudioMapper;
using wledimuudp::motion::MotionFeatureExtractor;
using wledimuudp::protocol::SyntheticAudioFrame;
using wledimuudp::test_fixture::MotionTraceKind;

struct SpectrumTotals {
  double low{0.0};
  double rotation{0.0};
  double shake{0.0};
  double impact{0.0};
  double all{0.0};
  std::size_t peak_frames{0U};
  SyntheticAudioFrame last{};
};

double group_sum(const SyntheticAudioFrame &frame, const std::size_t begin) {
  double total = 0.0;
  for (std::size_t index = begin; index < begin + 4U; ++index) {
    total += frame.bands[index];
  }
  return total;
}

SpectrumTotals run_trace(const MotionTraceKind kind, const MappingConfig config = {}) {
  MotionFeatureExtractor extractor;
  SyntheticAudioMapper mapper(config);
  SpectrumTotals totals{};
  for (const auto &sample : wledimuudp::test_fixture::make_motion_trace(kind)) {
    const auto features = extractor.process(sample);
    totals.last = mapper.map(features);
    totals.low += group_sum(totals.last, 0U);
    totals.rotation += group_sum(totals.last, 4U);
    totals.shake += group_sum(totals.last, 8U);
    totals.impact += group_sum(totals.last, 12U);
    totals.all += group_sum(totals.last, 0U) + group_sum(totals.last, 4U) +
                  group_sum(totals.last, 8U) + group_sum(totals.last, 12U);
    if (totals.last.sample_peak) {
      ++totals.peak_frames;
    }
  }
  return totals;
}

void assert_frame_finite_and_bounded(const SyntheticAudioFrame &frame) {
  TEST_ASSERT_TRUE(std::isfinite(frame.sample_raw));
  TEST_ASSERT_TRUE(std::isfinite(frame.sample_smoothed));
  TEST_ASSERT_TRUE(std::isfinite(frame.magnitude));
  TEST_ASSERT_TRUE(std::isfinite(frame.major_peak));
  TEST_ASSERT_TRUE(frame.sample_raw >= 0.0F && frame.sample_raw <= 255.0F);
  TEST_ASSERT_TRUE(frame.sample_smoothed >= 0.0F && frame.sample_smoothed <= 255.0F);
  TEST_ASSERT_TRUE(frame.magnitude >= 0.0F && frame.magnitude <= 254.0F);
  TEST_ASSERT_TRUE(frame.major_peak > 0.0F);
  for (const auto band : frame.bands) {
    TEST_ASSERT_TRUE(band <= 254U);
  }
}

void test_balanced_profile_contract_and_centers_are_stable() {
  TEST_ASSERT_EQUAL_UINT16(1U, wledimuudp::mapping::kBalancedProfileVersion);
  TEST_ASSERT_EQUAL_UINT32(16U, wledimuudp::mapping::kBalancedBandCentersHz.size());
  for (std::size_t index = 1U; index < wledimuudp::mapping::kBalancedBandCentersHz.size();
       ++index) {
    TEST_ASSERT_TRUE(wledimuudp::mapping::kBalancedBandCentersHz[index] >
                     wledimuudp::mapping::kBalancedBandCentersHz[index - 1U]);
  }
  const SyntheticAudioMapper mapper;
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 72.0F, mapper.config().level_gain);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.0F, mapper.config().quiet_major_peak_hz);
}

void test_stationary_level_converges_to_silence() {
  const auto totals = run_trace(MotionTraceKind::kStationaryLevel);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, totals.last.sample_raw);
  TEST_ASSERT_TRUE(totals.last.sample_smoothed < 0.01F);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, totals.last.magnitude);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.0F, totals.last.major_peak);
  TEST_ASSERT_EQUAL_UINT32(0U, totals.peak_frames);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, totals.all);
}

void test_stationary_tilt_does_not_create_false_level() {
  const auto totals = run_trace(MotionTraceKind::kStationaryTilted);
  TEST_ASSERT_TRUE(totals.last.sample_raw < 0.01F);
  TEST_ASSERT_TRUE(totals.last.sample_smoothed < 0.01F);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, totals.last.magnitude);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, totals.all);
}

void test_orientation_shapes_existing_energy_without_changing_level() {
  wledimuudp::motion::MotionFeatures left{};
  left.input_valid = true;
  left.linear_accel_magnitude_g = 0.22F;
  left.motion_energy_instant = 0.75F;
  left.motion_energy_smoothed = 0.50F;
  left.orientation = {-0.8F, 0.0F, 0.6F};

  auto right = left;
  right.orientation = {0.8F, 0.0F, 0.6F};

  SyntheticAudioMapper left_mapper;
  SyntheticAudioMapper right_mapper;
  const auto left_frame = left_mapper.map(left);
  const auto right_frame = right_mapper.map(right);

  TEST_ASSERT_EQUAL_FLOAT(left_frame.sample_raw, right_frame.sample_raw);
  TEST_ASSERT_EQUAL_FLOAT(left_frame.sample_smoothed, right_frame.sample_smoothed);
  TEST_ASSERT_TRUE(left_frame.bands[0] > right_frame.bands[0]);
  TEST_ASSERT_TRUE(right_frame.bands[3] > left_frame.bands[3]);
  TEST_ASSERT_TRUE(left_frame.major_peak < right_frame.major_peak);
}

void test_spin_and_translation_have_distinct_group_distributions() {
  const auto spin = run_trace(MotionTraceKind::kConstantSpin);
  const auto sway = run_trace(MotionTraceKind::kTranslationalSway);

  TEST_ASSERT_TRUE(spin.rotation > spin.low * 10.0);
  TEST_ASSERT_TRUE(sway.low > sway.rotation * 10.0);
  TEST_ASSERT_TRUE(spin.rotation > 1000.0);
  TEST_ASSERT_TRUE(sway.low > 1000.0);
}

void test_shake_is_broader_and_higher_band_than_slow_roll() {
  const auto shake = run_trace(MotionTraceKind::kShake);
  const auto roll = run_trace(MotionTraceKind::kSlowRoll);

  TEST_ASSERT_TRUE(shake.shake > roll.shake * 3.0);
  TEST_ASSERT_TRUE(shake.impact > roll.impact * 3.0);
  TEST_ASSERT_TRUE(shake.all > roll.all);
}

void test_tap_produces_one_shot_peak_and_transient_bands() {
  const auto tap = run_trace(MotionTraceKind::kTap);
  TEST_ASSERT_EQUAL_UINT32(1U, tap.peak_frames);
  TEST_ASSERT_TRUE(tap.impact > 0.0);
  TEST_ASSERT_TRUE(tap.shake > 0.0);
}

void test_motion_then_stillness_clears_spectrum_and_peak() {
  const auto totals = run_trace(MotionTraceKind::kMotionThenStill);
  TEST_ASSERT_TRUE(totals.all > 1000.0);
  TEST_ASSERT_TRUE(totals.last.sample_raw < 0.01F);
  TEST_ASSERT_TRUE(totals.last.sample_smoothed < 0.01F);
  TEST_ASSERT_FALSE(totals.last.sample_peak);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.0F, totals.last.magnitude);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 1.0F, totals.last.major_peak);
}

void test_all_trace_outputs_are_finite_and_bounded() {
  constexpr std::array<MotionTraceKind, 9> kinds{
      MotionTraceKind::kStationaryLevel, MotionTraceKind::kStationaryTilted,
      MotionTraceKind::kSlowRoll, MotionTraceKind::kTranslationalSway,
      MotionTraceKind::kConstantSpin, MotionTraceKind::kTap,
      MotionTraceKind::kShake, MotionTraceKind::kMotionThenStill,
      MotionTraceKind::kMalformedInjection,
  };

  for (const auto kind : kinds) {
    MotionFeatureExtractor extractor;
    SyntheticAudioMapper mapper;
    for (const auto &sample : wledimuudp::test_fixture::make_motion_trace(kind)) {
      assert_frame_finite_and_bounded(mapper.map(extractor.process(sample)));
    }
  }
}

void test_major_peak_tracks_synthetic_spectrum_centroid() {
  SyntheticAudioFrame low{};
  low.bands[0] = 200U;
  SyntheticAudioFrame high{};
  high.bands[15] = 200U;
  SyntheticAudioFrame middle{};
  middle.bands[7] = 100U;
  middle.bands[8] = 100U;

  const float low_peak = wledimuudp::mapping::spectral_major_peak(low, 1.0F);
  const float middle_peak = wledimuudp::mapping::spectral_major_peak(middle, 1.0F);
  const float high_peak = wledimuudp::mapping::spectral_major_peak(high, 1.0F);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, wledimuudp::mapping::kBalancedBandCentersHz[0], low_peak);
  TEST_ASSERT_TRUE(low_peak < middle_peak);
  TEST_ASSERT_TRUE(middle_peak < high_peak);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, wledimuudp::mapping::kBalancedBandCentersHz[15], high_peak);
}

void test_profile_config_changes_are_deterministic() {
  MappingConfig config{};
  config.translation_band_gain = 0.40F;
  config.level_gain = 40.0F;

  const auto first = run_trace(MotionTraceKind::kTranslationalSway, config);
  const auto second = run_trace(MotionTraceKind::kTranslationalSway, config);
  const auto baseline = run_trace(MotionTraceKind::kTranslationalSway);

  TEST_ASSERT_EQUAL_FLOAT(first.last.sample_raw, second.last.sample_raw);
  TEST_ASSERT_EQUAL_FLOAT(first.last.sample_smoothed, second.last.sample_smoothed);
  TEST_ASSERT_EQUAL_FLOAT(first.last.major_peak, second.last.major_peak);
  TEST_ASSERT_FLOAT_WITHIN(0.001, first.low, second.low);
  TEST_ASSERT_TRUE(first.low < baseline.low);
}

void test_mapper_to_encoder_is_byte_identical_for_fixed_trace() {
  MotionFeatureExtractor first_extractor;
  MotionFeatureExtractor second_extractor;
  SyntheticAudioMapper first_mapper;
  SyntheticAudioMapper second_mapper;
  const auto trace = wledimuudp::test_fixture::make_motion_trace(MotionTraceKind::kShake);

  for (const auto &sample : trace) {
    const auto first_frame = first_mapper.map(first_extractor.process(sample));
    const auto second_frame = second_mapper.map(second_extractor.process(sample));
    const auto first_packet = wledimuudp::protocol::encode_audio_sync_v2(first_frame);
    const auto second_packet = wledimuudp::protocol::encode_audio_sync_v2(second_frame);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(first_packet.data(), second_packet.data(), first_packet.size());
    const auto decoded = wledimuudp::protocol::decode_audio_sync_v2(first_packet);
    TEST_ASSERT_TRUE(decoded.ok());
  }
}

void test_invalid_feature_snapshot_does_not_retrigger_peak() {
  SyntheticAudioMapper mapper;
  wledimuudp::motion::MotionFeatures impact{};
  impact.input_valid = true;
  impact.impact = true;
  impact.motion_energy_instant = 1.0F;
  impact.motion_energy_smoothed = 0.5F;
  impact.orientation = {0.0F, 0.0F, 1.0F};

  const auto first = mapper.map(impact);
  TEST_ASSERT_TRUE(first.sample_peak);

  auto invalid = impact;
  invalid.input_valid = false;
  const auto rejected = mapper.map(invalid);
  TEST_ASSERT_FALSE(rejected.sample_peak);

  const auto resumed = mapper.map(impact);
  TEST_ASSERT_FALSE(resumed.sample_peak);
}

} // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_balanced_profile_contract_and_centers_are_stable);
  RUN_TEST(test_stationary_level_converges_to_silence);
  RUN_TEST(test_stationary_tilt_does_not_create_false_level);
  RUN_TEST(test_orientation_shapes_existing_energy_without_changing_level);
  RUN_TEST(test_spin_and_translation_have_distinct_group_distributions);
  RUN_TEST(test_shake_is_broader_and_higher_band_than_slow_roll);
  RUN_TEST(test_tap_produces_one_shot_peak_and_transient_bands);
  RUN_TEST(test_motion_then_stillness_clears_spectrum_and_peak);
  RUN_TEST(test_all_trace_outputs_are_finite_and_bounded);
  RUN_TEST(test_major_peak_tracks_synthetic_spectrum_centroid);
  RUN_TEST(test_profile_config_changes_are_deterministic);
  RUN_TEST(test_mapper_to_encoder_is_byte_identical_for_fixed_trace);
  RUN_TEST(test_invalid_feature_snapshot_does_not_retrigger_peak);
  return UNITY_END();
}
