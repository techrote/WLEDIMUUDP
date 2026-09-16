#include <wledimuudp/motion_features.hpp>

#include <algorithm>
#include <cmath>

namespace wledimuudp::motion {
namespace {

constexpr float kMicrosToSeconds = 1.0e-6F;

Vec3 add(const Vec3 &a, const Vec3 &b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 subtract(const Vec3 &a, const Vec3 &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 scale(const Vec3 &value, const float scalar) {
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float clamp01(const float value) {
  return std::clamp(value, 0.0F, 1.0F);
}

float deadband(const float value, const float threshold) {
  return value > threshold ? value - threshold : 0.0F;
}

float low_pass_alpha(const float dt_s, const float tau_s) {
  if (!(dt_s > 0.0F)) {
    return 0.0F;
  }
  if (!(tau_s > 0.0F)) {
    return 1.0F;
  }
  return dt_s / (tau_s + dt_s);
}

bool finite_vec(const Vec3 &value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vec3 normalise_or_zero(const Vec3 &value) {
  const float length = magnitude(value);
  if (!(length > 1.0e-6F) || !std::isfinite(length)) {
    return {};
  }
  return scale(value, 1.0F / length);
}

} // namespace

float magnitude(const Vec3 &value) {
  return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

bool finite_sample(const ImuSample &sample) {
  return sample.valid && finite_vec(sample.accel_g) && finite_vec(sample.gyro_dps);
}

CalibrationAccumulator::CalibrationAccumulator(const std::size_t minimum_samples)
    : minimum_samples_(std::max<std::size_t>(minimum_samples, 1U)) {}

void CalibrationAccumulator::reset() {
  count_ = 0U;
  gyro_sum_ = {};
  accel_magnitude_sum_ = 0.0F;
  accel_magnitude_sq_sum_ = 0.0F;
  gyro_deviation_sq_sum_ = 0.0F;
}

bool CalibrationAccumulator::add(const ImuSample &sample) {
  if (!finite_sample(sample)) {
    return false;
  }

  const float accel_magnitude = magnitude(sample.accel_g);
  const float gyro_magnitude = magnitude(sample.gyro_dps);
  gyro_sum_ = add(gyro_sum_, sample.gyro_dps);
  accel_magnitude_sum_ += accel_magnitude;
  accel_magnitude_sq_sum_ += accel_magnitude * accel_magnitude;
  gyro_deviation_sq_sum_ += gyro_magnitude * gyro_magnitude;
  ++count_;
  return true;
}

bool CalibrationAccumulator::ready() const {
  return count_ >= minimum_samples_;
}

std::size_t CalibrationAccumulator::sample_count() const {
  return count_;
}

bool CalibrationAccumulator::finalize(MotionCalibration &out) const {
  if (!ready()) {
    return false;
  }

  const float count = static_cast<float>(count_);
  out.gyro_bias_dps = scale(gyro_sum_, 1.0F / count);
  out.gravity_reference_g = accel_magnitude_sum_ / count;

  const float accel_variance = std::max(
      0.0F, accel_magnitude_sq_sum_ / count - out.gravity_reference_g * out.gravity_reference_g);
  out.accel_noise_floor_g = std::max(0.0005F, std::sqrt(accel_variance));

  const float mean_gyro_magnitude_sq = gyro_deviation_sq_sum_ / count;
  const float bias_magnitude_sq = out.gyro_bias_dps.x * out.gyro_bias_dps.x +
                                  out.gyro_bias_dps.y * out.gyro_bias_dps.y +
                                  out.gyro_bias_dps.z * out.gyro_bias_dps.z;
  out.gyro_noise_floor_dps =
      std::max(0.01F, std::sqrt(std::max(0.0F, mean_gyro_magnitude_sq - bias_magnitude_sq)));
  return true;
}

MotionFeatureExtractor::MotionFeatureExtractor(MotionConfig config, MotionCalibration calibration)
    : config_(config), calibration_(calibration) {
  reset();
}

void MotionFeatureExtractor::reset() {
  last_ = {};
  gravity_ = {};
  previous_linear_ = {};
  initialized_ = false;
  last_timestamp_us_ = 0U;
  quiet_elapsed_s_ = 0.0F;
  impact_latched_until_us_ = 0U;
  impact_refractory_until_us_ = 0U;
}

void MotionFeatureExtractor::set_calibration(const MotionCalibration &calibration) {
  calibration_ = calibration;
  reset();
}

const MotionCalibration &MotionFeatureExtractor::calibration() const {
  return calibration_;
}

const MotionConfig &MotionFeatureExtractor::config() const {
  return config_;
}

MotionFeatures MotionFeatureExtractor::process(const ImuSample &sample) {
  if (!finite_sample(sample) || (initialized_ && sample.timestamp_us <= last_timestamp_us_)) {
    MotionFeatures rejected = last_;
    rejected.input_valid = false;
    return rejected;
  }

  const Vec3 corrected_gyro = subtract(sample.gyro_dps, calibration_.gyro_bias_dps);

  if (!initialized_) {
    initialized_ = true;
    last_timestamp_us_ = sample.timestamp_us;
    gravity_ = sample.accel_g;
    previous_linear_ = {};

    last_.timestamp_us = sample.timestamp_us;
    last_.input_valid = true;
    last_.gravity_g = gravity_;
    last_.gravity_magnitude_g = magnitude(gravity_);
    const float gravity_error =
        std::fabs(last_.gravity_magnitude_g - calibration_.gravity_reference_g);
    last_.gravity_confidence =
        clamp01(1.0F - gravity_error / std::max(config_.gravity_confidence_tolerance_g, 1.0e-6F));
    last_.gyro_dps = corrected_gyro;
    last_.angular_speed_dps =
        deadband(magnitude(corrected_gyro),
                 calibration_.gyro_noise_floor_dps * config_.gyro_deadband_multiplier);
    last_.orientation = normalise_or_zero(gravity_);
    return last_;
  }

  float dt_s = static_cast<float>(sample.timestamp_us - last_timestamp_us_) * kMicrosToSeconds;
  dt_s = std::min(dt_s, std::max(config_.max_dt_s, 1.0e-4F));
  last_timestamp_us_ = sample.timestamp_us;

  const float gravity_alpha = low_pass_alpha(dt_s, config_.gravity_time_constant_s);
  gravity_ = add(gravity_, scale(subtract(sample.accel_g, gravity_), gravity_alpha));

  const Vec3 linear = subtract(sample.accel_g, gravity_);
  const float linear_magnitude = deadband(magnitude(linear), calibration_.accel_noise_floor_g *
                                                                 config_.accel_deadband_multiplier);
  const float jerk =
      deadband(magnitude(subtract(linear, previous_linear_)) / std::max(dt_s, 1.0e-6F),
               config_.jerk_deadband_g_per_s);
  previous_linear_ = linear;

  const float angular_speed =
      deadband(magnitude(corrected_gyro),
               calibration_.gyro_noise_floor_dps * config_.gyro_deadband_multiplier);
  const float instant_energy = std::max(0.0F, linear_magnitude * config_.accel_energy_weight +
                                                  jerk * config_.jerk_energy_weight +
                                                  angular_speed * config_.angular_energy_weight);
  const float energy_alpha = low_pass_alpha(dt_s, config_.energy_smoothing_time_constant_s);
  const float smoothed_energy =
      last_.motion_energy_smoothed + energy_alpha * (instant_energy - last_.motion_energy_smoothed);

  const bool impact_trigger = sample.timestamp_us >= impact_refractory_until_us_ &&
                              linear_magnitude >= config_.impact_accel_threshold_g &&
                              jerk >= config_.impact_jerk_threshold_g_per_s;
  if (impact_trigger) {
    impact_latched_until_us_ =
        sample.timestamp_us + static_cast<std::uint64_t>(config_.impact_latch_s * 1.0e6F);
    impact_refractory_until_us_ =
        sample.timestamp_us + static_cast<std::uint64_t>(config_.impact_refractory_s * 1.0e6F);
  }
  if (linear_magnitude <= config_.impact_release_accel_g &&
      sample.timestamp_us >= impact_latched_until_us_) {
    impact_latched_until_us_ = 0U;
  }

  if (instant_energy <= config_.stillness_enter_threshold) {
    quiet_elapsed_s_ += dt_s;
  } else if (instant_energy >= config_.stillness_exit_threshold) {
    quiet_elapsed_s_ = 0.0F;
  }
  const bool still = quiet_elapsed_s_ >= config_.stillness_hold_s;
  const float stillness_confidence =
      clamp01(quiet_elapsed_s_ / std::max(config_.stillness_hold_s, 1.0e-6F));

  MotionFeatures out{};
  out.timestamp_us = sample.timestamp_us;
  out.input_valid = true;
  out.gravity_g = gravity_;
  out.gravity_magnitude_g = magnitude(gravity_);
  const float gravity_error = std::fabs(out.gravity_magnitude_g - calibration_.gravity_reference_g);
  out.gravity_confidence =
      clamp01(1.0F - gravity_error / std::max(config_.gravity_confidence_tolerance_g, 1.0e-6F));
  out.linear_accel_g = linear;
  out.linear_accel_magnitude_g = linear_magnitude;
  out.jerk_g_per_s = jerk;
  out.gyro_dps = corrected_gyro;
  out.angular_speed_dps = angular_speed;
  out.orientation = normalise_or_zero(gravity_);
  out.motion_energy_instant = instant_energy;
  out.motion_energy_smoothed = std::max(0.0F, smoothed_energy);
  out.impact = impact_latched_until_us_ != 0U && sample.timestamp_us < impact_latched_until_us_;
  out.still = still;
  out.stillness_confidence = stillness_confidence;
  last_ = out;
  return out;
}

} // namespace wledimuudp::motion
