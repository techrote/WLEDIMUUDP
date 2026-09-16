#include <unity.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include <wledimuudp/motion_features.hpp>

#include "../fixtures/motion_traces.hpp"

namespace {

using wledimuudp::motion::CalibrationAccumulator;
using wledimuudp::motion::ImuSample;
using wledimuudp::motion::MotionCalibration;
using wledimuudp::motion::MotionFeatureExtractor;
using wledimuudp::motion::MotionFeatures;
using wledimuudp::test_fixture::MotionTraceKind;

MotionFeatures run_trace(MotionFeatureExtractor &extractor, const MotionTraceKind kind) {
  MotionFeatures last{};
  for (const auto &sample : wledimuudp::test_fixture::make_motion_trace(kind)) {
    last = extractor.process(sample);
  }
  return last;
}

void assert_finite_features(const MotionFeatures &features) {
  const std::array<float, 18> values{
      features.gravity_g.x,
      features.gravity_g.y,
      features.gravity_g.z,
      features.gravity_magnitude_g,
      features.gravity_confidence,
      features.linear_accel_g.x,
      features.linear_accel_g.y,
      features.linear_accel_g.z,
      features.linear_accel_magnitude_g,
      features.jerk_g_per_s,
      features.gyro_dps.x,
      features.gyro_dps.y,
      features.gyro_dps.z,
      features.angular_speed_dps,
      features.orientation.x,
      features.orientation.y,
      features.orientation.z,
      features.motion_energy_smoothed,
  };
  for (const float value : values) {
    TEST_ASSERT_TRUE(std::isfinite(value));
  }
  TEST_ASSERT_TRUE(features.gravity_confidence >= 0.0F && features.gravity_confidence <= 1.0F);
  TEST_ASSERT_TRUE(features.stillness_confidence >= 0.0F &&
                   features.stillness_confidence <= 1.0F);
}

void test_fixture_corpus_is_complete_and_reusable() {
  constexpr std::array<MotionTraceKind, 9> kinds{
      MotionTraceKind::kStationaryLevel, MotionTraceKind::kStationaryTilted,
      MotionTraceKind::kSlowRoll, MotionTraceKind::kTranslationalSway,
      MotionTraceKind::kConstantSpin, MotionTraceKind::kTap, MotionTraceKind::kShake,
      MotionTraceKind::kMotionThenStill, MotionTraceKind::kMalformedInjection,
  };
  for (const auto kind : kinds) {
    const auto trace = wledimuudp::test_fixture::make_motion_trace(kind);
    TEST_ASSERT_TRUE(trace.size() >= 400U);
    TEST_ASSERT_TRUE(trace.front().timestamp_us < trace.back().timestamp_us);
  }
}

void test_stationary_level_converges_to_stillness() {
  MotionFeatureExtractor extractor;
  const auto last = run_trace(extractor, MotionTraceKind::kStationaryLevel);
  TEST_ASSERT_TRUE(last.input_valid);
  TEST_ASSERT_TRUE(last.still);
  TEST_ASSERT_TRUE(last.motion_energy_instant < 0.001F);
  TEST_ASSERT_TRUE(last.motion_energy_smoothed < 0.001F);
  TEST_ASSERT_FLOAT_WITHIN(0.002F, 1.0F, last.gravity_magnitude_g);
  TEST_ASSERT_TRUE(last.gravity_confidence > 0.99F);
  assert_finite_features(last);
}

void test_stationary_tilt_changes_orientation_without_false_motion() {
  MotionFeatureExtractor level_extractor;
  const auto level = run_trace(level_extractor, MotionTraceKind::kStationaryLevel);
  MotionFeatureExtractor tilted_extractor;
  const auto tilted = run_trace(tilted_extractor, MotionTraceKind::kStationaryTilted);

  TEST_ASSERT_TRUE(tilted.still);
  TEST_ASSERT_TRUE(tilted.motion_energy_smoothed < 0.001F);
  TEST_ASSERT_TRUE(std::fabs(tilted.orientation.x) > 0.45F);
  TEST_ASSERT_TRUE(std::fabs(level.orientation.x) < 0.01F);
}

void test_constant_spin_is_distinct_from_translation() {
  MotionFeatureExtractor spin_extractor;
  MotionFeatureExtractor sway_extractor;
  const auto spin = run_trace(spin_extractor, MotionTraceKind::kConstantSpin);
  const auto sway = run_trace(sway_extractor, MotionTraceKind::kTranslationalSway);

  TEST_ASSERT_TRUE(spin.angular_speed_dps > 100.0F);
  TEST_ASSERT_TRUE(spin.linear_accel_magnitude_g < 0.001F);
  TEST_ASSERT_TRUE(sway.angular_speed_dps < 0.001F);
  TEST_ASSERT_TRUE(sway.linear_accel_magnitude_g > 0.01F);
  TEST_ASSERT_TRUE(spin.motion_energy_instant > 0.5F);
}

void test_impact_latch_is_bounded_and_refractory() {
  MotionFeatureExtractor extractor;
  std::size_t asserted = 0U;
  std::size_t trigger_windows = 0U;
  bool previous = false;
  for (const auto &sample : wledimuudp::test_fixture::make_motion_trace(MotionTraceKind::kTap)) {
    const auto features = extractor.process(sample);
    if (features.impact) {
      ++asserted;
    }
    if (features.impact && !previous) {
      ++trigger_windows;
    }
    previous = features.impact;
  }
  TEST_ASSERT_TRUE(asserted >= 1U);
  TEST_ASSERT_TRUE(asserted <= 20U);
  TEST_ASSERT_EQUAL_UINT32(1U, trigger_windows);
}

void test_motion_then_stillness_decays_predictably() {
  MotionFeatureExtractor extractor;
  float peak_smoothed = 0.0F;
  MotionFeatures last{};
  for (const auto &sample :
       wledimuudp::test_fixture::make_motion_trace(MotionTraceKind::kMotionThenStill)) {
    last = extractor.process(sample);
    peak_smoothed = std::max(peak_smoothed, last.motion_energy_smoothed);
  }
  TEST_ASSERT_TRUE(peak_smoothed > 0.2F);
  TEST_ASSERT_TRUE(last.still);
  TEST_ASSERT_TRUE(last.motion_energy_smoothed < peak_smoothed * 0.01F);
}

void test_malformed_samples_do_not_poison_state() {
  MotionFeatureExtractor extractor;
  std::size_t rejected = 0U;
  MotionFeatures last{};
  for (const auto &sample :
       wledimuudp::test_fixture::make_motion_trace(MotionTraceKind::kMalformedInjection)) {
    last = extractor.process(sample);
    if (!last.input_valid) {
      ++rejected;
    }
    assert_finite_features(last);
  }
  TEST_ASSERT_EQUAL_UINT32(2U, rejected);
  TEST_ASSERT_TRUE(last.input_valid);
  TEST_ASSERT_TRUE(last.still);
}

void test_non_monotonic_timestamp_is_rejected_without_state_advance() {
  MotionFeatureExtractor extractor;
  ImuSample first{10000U, {0.0F, 0.0F, 1.0F}, {}, true};
  ImuSample second{15000U, {0.1F, 0.0F, 1.0F}, {}, true};
  TEST_ASSERT_TRUE(extractor.process(first).input_valid);
  const auto accepted = extractor.process(second);
  TEST_ASSERT_TRUE(accepted.input_valid);

  ImuSample stale{15000U, {9.0F, 9.0F, 9.0F}, {500.0F, 0.0F, 0.0F}, true};
  const auto rejected = extractor.process(stale);
  TEST_ASSERT_FALSE(rejected.input_valid);
  TEST_ASSERT_EQUAL_FLOAT(accepted.motion_energy_smoothed, rejected.motion_energy_smoothed);
}

void test_calibration_estimates_bias_gravity_and_noise_floor() {
  CalibrationAccumulator accumulator(100U);
  for (std::uint64_t i = 0; i < 120U; ++i) {
    const float noise = (i % 2U == 0U) ? 0.002F : -0.002F;
    ImuSample sample{};
    sample.timestamp_us = (i + 1U) * 5000U;
    sample.accel_g = {noise, 0.0F, 1.0F + noise};
    sample.gyro_dps = {0.42F + noise, -0.18F, 0.09F - noise};
    TEST_ASSERT_TRUE(accumulator.add(sample));
  }
  TEST_ASSERT_TRUE(accumulator.ready());
  TEST_ASSERT_EQUAL_UINT32(120U, accumulator.sample_count());
  MotionCalibration calibration{};
  TEST_ASSERT_TRUE(accumulator.finalize(calibration));
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 0.42F, calibration.gyro_bias_dps.x);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, -0.18F, calibration.gyro_bias_dps.y);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 1.0F, calibration.gravity_reference_g);
  TEST_ASSERT_TRUE(calibration.accel_noise_floor_g > 0.0F);
  TEST_ASSERT_TRUE(calibration.gyro_noise_floor_dps > 0.0F);
}

void test_fixed_timestamped_trace_is_exactly_repeatable() {
  MotionFeatureExtractor first;
  MotionFeatureExtractor second;
  const auto trace = wledimuudp::test_fixture::make_motion_trace(MotionTraceKind::kShake);
  for (const auto &sample : trace) {
    const auto a = first.process(sample);
    const auto b = second.process(sample);
    TEST_ASSERT_EQUAL_UINT64(a.timestamp_us, b.timestamp_us);
    TEST_ASSERT_EQUAL_FLOAT(a.motion_energy_instant, b.motion_energy_instant);
    TEST_ASSERT_EQUAL_FLOAT(a.motion_energy_smoothed, b.motion_energy_smoothed);
    TEST_ASSERT_EQUAL_FLOAT(a.orientation.x, b.orientation.x);
    TEST_ASSERT_EQUAL_FLOAT(a.orientation.y, b.orientation.y);
    TEST_ASSERT_EQUAL_FLOAT(a.orientation.z, b.orientation.z);
    TEST_ASSERT_EQUAL(a.impact, b.impact);
    TEST_ASSERT_EQUAL(a.still, b.still);
  }
}

void test_timing_jitter_remains_finite_and_reaches_stillness() {
  MotionFeatureExtractor extractor;
  MotionFeatures last{};
  std::uint64_t timestamp = 0U;
  for (std::size_t i = 0; i < 500U; ++i) {
    timestamp += (i % 2U == 0U) ? 4500U : 5500U;
    ImuSample sample{timestamp, {0.0F, 0.0F, 1.0F}, {}, true};
    last = extractor.process(sample);
    assert_finite_features(last);
  }
  TEST_ASSERT_TRUE(last.still);
  TEST_ASSERT_TRUE(last.motion_energy_smoothed < 0.001F);
}

} // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fixture_corpus_is_complete_and_reusable);
  RUN_TEST(test_stationary_level_converges_to_stillness);
  RUN_TEST(test_stationary_tilt_changes_orientation_without_false_motion);
  RUN_TEST(test_constant_spin_is_distinct_from_translation);
  RUN_TEST(test_impact_latch_is_bounded_and_refractory);
  RUN_TEST(test_motion_then_stillness_decays_predictably);
  RUN_TEST(test_malformed_samples_do_not_poison_state);
  RUN_TEST(test_non_monotonic_timestamp_is_rejected_without_state_advance);
  RUN_TEST(test_calibration_estimates_bias_gravity_and_noise_floor);
  RUN_TEST(test_fixed_timestamped_trace_is_exactly_repeatable);
  RUN_TEST(test_timing_jitter_remains_finite_and_reaches_stillness);
  return UNITY_END();
}
