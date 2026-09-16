#pragma once

#include <cstddef>
#include <cstdint>

namespace wledimuudp::motion {

struct Vec3 {
  float x{0.0F};
  float y{0.0F};
  float z{0.0F};
};

struct ImuSample {
  std::uint64_t timestamp_us{0U};
  Vec3 accel_g{};
  Vec3 gyro_dps{};
  bool valid{true};
};

struct MotionCalibration {
  Vec3 gyro_bias_dps{};
  float gravity_reference_g{1.0F};
  float accel_noise_floor_g{0.003F};
  float gyro_noise_floor_dps{0.15F};
};

struct MotionConfig {
  float gravity_time_constant_s{0.35F};
  float energy_smoothing_time_constant_s{0.18F};
  float gravity_confidence_tolerance_g{0.25F};
  float accel_deadband_multiplier{2.5F};
  float gyro_deadband_multiplier{2.5F};
  float jerk_deadband_g_per_s{0.50F};
  float accel_energy_weight{1.0F};
  float jerk_energy_weight{0.08F};
  float angular_energy_weight{0.0125F};
  float stillness_enter_threshold{0.050F};
  float stillness_exit_threshold{0.080F};
  float stillness_hold_s{0.45F};
  float impact_accel_threshold_g{0.65F};
  float impact_jerk_threshold_g_per_s{8.0F};
  float impact_release_accel_g{0.20F};
  float impact_latch_s{0.08F};
  float impact_refractory_s{0.22F};
  float max_dt_s{0.10F};
};

struct MotionFeatures {
  std::uint64_t timestamp_us{0U};
  bool input_valid{false};
  Vec3 gravity_g{};
  float gravity_magnitude_g{0.0F};
  float gravity_confidence{0.0F};
  Vec3 linear_accel_g{};
  float linear_accel_magnitude_g{0.0F};
  float jerk_g_per_s{0.0F};
  Vec3 gyro_dps{};
  float angular_speed_dps{0.0F};
  Vec3 orientation{};
  float motion_energy_instant{0.0F};
  float motion_energy_smoothed{0.0F};
  bool impact{false};
  bool still{false};
  float stillness_confidence{0.0F};
};

class CalibrationAccumulator {
public:
  explicit CalibrationAccumulator(std::size_t minimum_samples = 64U);

  void reset();
  bool add(const ImuSample &sample);
  bool ready() const;
  std::size_t sample_count() const;
  bool finalize(MotionCalibration &out) const;

private:
  std::size_t minimum_samples_;
  std::size_t count_{0U};
  Vec3 gyro_sum_{};
  float accel_magnitude_sum_{0.0F};
  float accel_magnitude_sq_sum_{0.0F};
  float gyro_deviation_sq_sum_{0.0F};
};

class MotionFeatureExtractor {
public:
  explicit MotionFeatureExtractor(MotionConfig config = {}, MotionCalibration calibration = {});

  void reset();
  void set_calibration(const MotionCalibration &calibration);
  const MotionCalibration &calibration() const;
  const MotionConfig &config() const;
  MotionFeatures process(const ImuSample &sample);

private:
  MotionConfig config_{};
  MotionCalibration calibration_{};
  MotionFeatures last_{};
  Vec3 gravity_{};
  Vec3 previous_linear_{};
  bool initialized_{false};
  std::uint64_t last_timestamp_us_{0U};
  float quiet_elapsed_s_{0.0F};
  std::uint64_t impact_latched_until_us_{0U};
  std::uint64_t impact_refractory_until_us_{0U};
};

bool finite_sample(const ImuSample &sample);
float magnitude(const Vec3 &value);

} // namespace wledimuudp::motion
