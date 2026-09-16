#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <wledimuudp/motion_features.hpp>

namespace wledimuudp::test_fixture {

enum class MotionTraceKind : std::uint8_t {
  kStationaryLevel,
  kStationaryTilted,
  kSlowRoll,
  kTranslationalSway,
  kConstantSpin,
  kTap,
  kShake,
  kMotionThenStill,
  kMalformedInjection,
};

inline std::vector<motion::ImuSample> make_motion_trace(const MotionTraceKind kind) {
  constexpr std::uint64_t kStepUs = 5000U;
  constexpr float kPi = 3.14159265358979323846F;
  const std::size_t count = kind == MotionTraceKind::kMotionThenStill ? 600U : 400U;
  std::vector<motion::ImuSample> trace;
  trace.reserve(count);

  for (std::size_t i = 0; i < count; ++i) {
    const float t = static_cast<float>(i) * 0.005F;
    motion::ImuSample sample{};
    sample.timestamp_us = static_cast<std::uint64_t>(i + 1U) * kStepUs;
    sample.accel_g = {0.0F, 0.0F, 1.0F};
    sample.gyro_dps = {};

    switch (kind) {
    case MotionTraceKind::kStationaryLevel:
      break;
    case MotionTraceKind::kStationaryTilted: {
      constexpr float kAngle = 30.0F * kPi / 180.0F;
      sample.accel_g = {std::sin(kAngle), 0.0F, std::cos(kAngle)};
      break;
    }
    case MotionTraceKind::kSlowRoll: {
      const float angle = 0.35F * std::sin(2.0F * kPi * 0.35F * t);
      sample.accel_g = {0.0F, std::sin(angle), std::cos(angle)};
      sample.gyro_dps = {22.0F * std::cos(2.0F * kPi * 0.35F * t), 0.0F, 0.0F};
      break;
    }
    case MotionTraceKind::kTranslationalSway:
      sample.accel_g.x = 0.18F * std::sin(2.0F * kPi * 1.1F * t);
      break;
    case MotionTraceKind::kConstantSpin:
      sample.gyro_dps.z = 120.0F;
      break;
    case MotionTraceKind::kTap:
      if (i == 80U) {
        sample.accel_g.x = 1.25F;
      } else if (i == 81U) {
        sample.accel_g.x = -0.35F;
      }
      break;
    case MotionTraceKind::kShake:
      sample.accel_g.x = 0.42F * std::sin(2.0F * kPi * 8.0F * t);
      sample.accel_g.y = 0.31F * std::sin(2.0F * kPi * 11.0F * t + 0.7F);
      sample.gyro_dps.z = 75.0F * std::sin(2.0F * kPi * 6.0F * t);
      break;
    case MotionTraceKind::kMotionThenStill:
      if (i < 200U) {
        sample.accel_g.x = 0.22F * std::sin(2.0F * kPi * 1.3F * t);
        sample.gyro_dps.y = 45.0F * std::sin(2.0F * kPi * 0.8F * t);
      }
      break;
    case MotionTraceKind::kMalformedInjection:
      if (i == 120U) {
        sample.accel_g.x = std::numeric_limits<float>::quiet_NaN();
      } else if (i == 240U) {
        sample.gyro_dps.y = std::numeric_limits<float>::infinity();
      }
      break;
    }

    trace.push_back(sample);
  }
  return trace;
}

} // namespace wledimuudp::test_fixture
