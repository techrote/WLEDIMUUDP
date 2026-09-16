#include <wledimuudp/firmware_support.hpp>

#include <algorithm>
#include <cmath>

namespace wledimuudp::firmware {
namespace {

float component(const motion::Vec3 &value, const std::uint8_t index) noexcept {
  switch (index) {
  case 0U:
    return value.x;
  case 1U:
    return value.y;
  case 2U:
    return value.z;
  default:
    return 0.0F;
  }
}

float signed_component(const motion::Vec3 &value, const std::uint8_t index,
                       const std::int8_t sign) noexcept {
  if (sign != 1 && sign != -1) {
    return 0.0F;
  }
  return component(value, index) * static_cast<float>(sign);
}

std::int16_t decode_i16_le(const std::uint8_t low, const std::uint8_t high) noexcept {
  const std::int32_t low_bits = static_cast<std::int32_t>(low);
  const std::int32_t high_bits = static_cast<std::int32_t>(high);
  std::int32_t value = low_bits | (high_bits << 8);
  if (value >= 32768) {
    value -= 65536;
  }
  return static_cast<std::int16_t>(value);
}

bool finite_calibration(const motion::MotionCalibration &calibration) noexcept {
  const bool finite_bias = std::isfinite(calibration.gyro_bias_dps.x) &&
                           std::isfinite(calibration.gyro_bias_dps.y) &&
                           std::isfinite(calibration.gyro_bias_dps.z);
  const bool finite_scalars = std::isfinite(calibration.gravity_reference_g) &&
                              std::isfinite(calibration.accel_noise_floor_g) &&
                              std::isfinite(calibration.gyro_noise_floor_dps);
  return finite_bias && finite_scalars;
}

} // namespace

motion::Vec3 apply_axis_transform(const motion::Vec3 &value,
                                  const AxisTransform &transform) noexcept {
  return {
      signed_component(value, transform.source[0], transform.sign[0]),
      signed_component(value, transform.source[1], transform.sign[1]),
      signed_component(value, transform.source[2], transform.sign[2]),
  };
}

bool decode_qmi8658_motion_data(const std::uint8_t *data, const std::size_t size,
                                const std::uint64_t timestamp_us, const Qmi8658Config &config,
                                const AxisTransform &transform, motion::ImuSample &out) noexcept {
  if (data == nullptr || size != kQmi8658MotionDataSize || !(config.accel_lsb_per_g > 0.0F) ||
      !(config.gyro_lsb_per_dps > 0.0F) || !std::isfinite(config.accel_lsb_per_g) ||
      !std::isfinite(config.gyro_lsb_per_dps)) {
    out = {};
    out.timestamp_us = timestamp_us;
    out.valid = false;
    return false;
  }

  motion::Vec3 accel{
      static_cast<float>(decode_i16_le(data[0], data[1])) / config.accel_lsb_per_g,
      static_cast<float>(decode_i16_le(data[2], data[3])) / config.accel_lsb_per_g,
      static_cast<float>(decode_i16_le(data[4], data[5])) / config.accel_lsb_per_g,
  };
  motion::Vec3 gyro{
      static_cast<float>(decode_i16_le(data[6], data[7])) / config.gyro_lsb_per_dps,
      static_cast<float>(decode_i16_le(data[8], data[9])) / config.gyro_lsb_per_dps,
      static_cast<float>(decode_i16_le(data[10], data[11])) / config.gyro_lsb_per_dps,
  };

  out.timestamp_us = timestamp_us;
  out.accel_g = apply_axis_transform(accel, transform);
  out.gyro_dps = apply_axis_transform(gyro, transform);
  out.valid = motion::finite_sample(out);
  return out.valid;
}

StartupCalibration::StartupCalibration(CalibrationPolicy policy)
    : policy_(policy), accumulator_(std::max<std::size_t>(policy.minimum_samples, 1U)) {}

void StartupCalibration::reset() {
  accumulator_.reset();
  motion_detected_ = false;
  invalid_samples_ = 0U;
}

bool StartupCalibration::add(const motion::ImuSample &sample) {
  if (!motion::finite_sample(sample)) {
    ++invalid_samples_;
    return false;
  }

  const float accel_magnitude = motion::magnitude(sample.accel_g);
  const float gyro_magnitude = motion::magnitude(sample.gyro_dps);
  if (std::fabs(accel_magnitude - 1.0F) > policy_.max_accel_magnitude_deviation_g ||
      gyro_magnitude > policy_.max_stationary_gyro_dps) {
    motion_detected_ = true;
  }
  return accumulator_.add(sample);
}

bool StartupCalibration::ready() const {
  return accumulator_.ready();
}

std::size_t StartupCalibration::sample_count() const {
  return accumulator_.sample_count();
}

std::size_t StartupCalibration::invalid_sample_count() const {
  return invalid_samples_;
}

CalibrationResult StartupCalibration::finalize(motion::MotionCalibration &out) const {
  if (!ready()) {
    return CalibrationResult::kNotReady;
  }
  if (motion_detected_) {
    return CalibrationResult::kMotionDetected;
  }

  motion::MotionCalibration candidate{};
  if (!accumulator_.finalize(candidate) || !finite_calibration(candidate)) {
    return CalibrationResult::kInvalidCalibration;
  }
  if (candidate.accel_noise_floor_g > policy_.max_accel_noise_floor_g) {
    return CalibrationResult::kAccelTooNoisy;
  }
  if (candidate.gyro_noise_floor_dps > policy_.max_gyro_noise_floor_dps) {
    return CalibrationResult::kGyroTooNoisy;
  }

  out = candidate;
  return CalibrationResult::kAccepted;
}

const CalibrationPolicy &StartupCalibration::policy() const {
  return policy_;
}

ReconnectGate::ReconnectGate(const std::uint32_t interval_ms)
    : interval_ms_(std::max<std::uint32_t>(interval_ms, 1U)) {}

void ReconnectGate::reset() {
  next_attempt_ms_ = 0U;
  armed_ = false;
}

bool ReconnectGate::should_attempt(const std::uint32_t now_ms, const bool connected) noexcept {
  if (connected) {
    armed_ = false;
    return false;
  }

  if (!armed_) {
    next_attempt_ms_ = now_ms + interval_ms_;
    armed_ = true;
    return true;
  }

  if (static_cast<std::int32_t>(now_ms - next_attempt_ms_) >= 0) {
    next_attempt_ms_ = now_ms + interval_ms_;
    return true;
  }
  return false;
}

void MicrosExtender::reset() {
  previous_ = 0U;
  epoch_ = 0U;
  initialized_ = false;
}

std::uint64_t MicrosExtender::extend(const std::uint32_t raw_micros) noexcept {
  if (initialized_ && raw_micros < previous_) {
    epoch_ += (std::uint64_t{1U} << 32U);
  }
  previous_ = raw_micros;
  initialized_ = true;
  return epoch_ + raw_micros;
}

FixedRateGate::FixedRateGate(const std::uint64_t period_us)
    : period_us_(std::max<std::uint64_t>(period_us, 1U)) {}

void FixedRateGate::reset() {
  next_due_us_ = 0U;
  initialized_ = false;
}

bool FixedRateGate::due(const std::uint64_t now_us) noexcept {
  if (!initialized_) {
    initialized_ = true;
    next_due_us_ = now_us + period_us_;
    return true;
  }
  if (now_us < next_due_us_) {
    return false;
  }

  const std::uint64_t elapsed = now_us - next_due_us_;
  next_due_us_ += (elapsed / period_us_ + 1U) * period_us_;
  return true;
}

std::uint64_t FixedRateGate::period_us() const noexcept {
  return period_us_;
}

bool can_emit_packet(const SenderMode mode, const bool wifi_connected, const bool sensor_healthy,
                     const bool calibrated, const bool features_valid) noexcept {
  if (!wifi_connected) {
    return false;
  }
  if (mode == SenderMode::kDiagnostic) {
    return true;
  }
  return sensor_healthy && calibrated && features_valid;
}

protocol::SyntheticAudioFrame make_diagnostic_frame() noexcept {
  protocol::SyntheticAudioFrame frame{};
  frame.sample_raw = 72.0F;
  frame.sample_smoothed = 64.0F;
  frame.sample_peak = false;
  frame.bands[0] = 20U;
  frame.bands[1] = 28U;
  frame.bands[2] = 40U;
  frame.bands[3] = 54U;
  frame.bands[4] = 72U;
  frame.bands[5] = 96U;
  frame.bands[6] = 128U;
  frame.bands[7] = 160U;
  frame.bands[8] = 160U;
  frame.bands[9] = 128U;
  frame.bands[10] = 96U;
  frame.bands[11] = 72U;
  frame.bands[12] = 54U;
  frame.bands[13] = 40U;
  frame.bands[14] = 28U;
  frame.bands[15] = 20U;
  frame.magnitude = 88.0F;
  frame.major_peak = 740.0F;
  return frame;
}

} // namespace wledimuudp::firmware
