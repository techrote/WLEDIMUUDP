#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/motion_features.hpp>

namespace wledimuudp::firmware {

struct AxisTransform {
  std::array<std::uint8_t, 3> source{0U, 1U, 2U};
  std::array<std::int8_t, 3> sign{1, 1, 1};
};

struct BoardProfile {
  const char *name;
  std::int8_t i2c_sda;
  std::int8_t i2c_scl;
  std::int8_t imu_int1;
  std::int8_t imu_int2;
  std::uint32_t i2c_frequency_hz;
  std::uint8_t imu_address;
  AxisTransform axes;
};

inline constexpr BoardProfile kWaveshareEsp32S3Matrix{
    "waveshare-esp32-s3-matrix", 11, 12, 10, 13, 400000U, 0x6BU, {}};

motion::Vec3 apply_axis_transform(const motion::Vec3 &value,
                                  const AxisTransform &transform) noexcept;

struct Qmi8658Config {
  std::uint8_t primary_address{0x6BU};
  std::uint8_t alternate_address{0x6AU};
  std::uint8_t who_am_i_value{0x05U};
  std::uint8_t ctrl1{0x40U};
  std::uint8_t ctrl2{0x25U};
  std::uint8_t ctrl3{0x65U};
  std::uint8_t ctrl5{0x00U};
  std::uint8_t ctrl7{0x03U};
  float accel_lsb_per_g{4096.0F};
  float gyro_lsb_per_dps{32.0F};
  float effective_six_dof_odr_hz{224.2F};
};

inline constexpr Qmi8658Config kReferenceQmi8658Config{};

inline constexpr std::uint8_t kQmi8658WhoAmIRegister = 0x00U;
inline constexpr std::uint8_t kQmi8658Ctrl1Register = 0x02U;
inline constexpr std::uint8_t kQmi8658Ctrl2Register = 0x03U;
inline constexpr std::uint8_t kQmi8658Ctrl3Register = 0x04U;
inline constexpr std::uint8_t kQmi8658Ctrl5Register = 0x06U;
inline constexpr std::uint8_t kQmi8658Ctrl7Register = 0x08U;
inline constexpr std::uint8_t kQmi8658AccelXLowRegister = 0x35U;
inline constexpr std::size_t kQmi8658MotionDataSize = 12U;

bool decode_qmi8658_motion_data(const std::uint8_t *data, std::size_t size,
                                std::uint64_t timestamp_us, const Qmi8658Config &config,
                                const AxisTransform &transform, motion::ImuSample &out) noexcept;

struct CalibrationPolicy {
  std::size_t minimum_samples{256U};
  float max_stationary_gyro_dps{5.0F};
  float max_accel_magnitude_deviation_g{0.08F};
  float max_accel_noise_floor_g{0.035F};
  float max_gyro_noise_floor_dps{1.5F};
};

enum class CalibrationResult : std::uint8_t {
  kAccepted = 0U,
  kNotReady,
  kMotionDetected,
  kAccelTooNoisy,
  kGyroTooNoisy,
  kInvalidCalibration,
};

class StartupCalibration {
public:
  explicit StartupCalibration(CalibrationPolicy policy = {});

  void reset();
  bool add(const motion::ImuSample &sample);
  bool ready() const;
  std::size_t sample_count() const;
  std::size_t invalid_sample_count() const;
  CalibrationResult finalize(motion::MotionCalibration &out) const;
  const CalibrationPolicy &policy() const;

private:
  CalibrationPolicy policy_{};
  motion::CalibrationAccumulator accumulator_;
  bool motion_detected_{false};
  std::size_t invalid_samples_{0U};
};

struct NetworkConfig {
  std::array<std::uint8_t, 4> multicast_address{protocol::kDefaultMulticastAddress};
  std::uint16_t port{protocol::kDefaultAudioSyncPort};
  std::uint16_t packet_rate_hz{50U};
  std::uint32_t reconnect_interval_ms{5000U};
};

inline constexpr NetworkConfig kDefaultNetworkConfig{};

class ReconnectGate {
public:
  explicit ReconnectGate(std::uint32_t interval_ms = 5000U);

  void reset();
  bool should_attempt(std::uint32_t now_ms, bool connected) noexcept;

private:
  std::uint32_t interval_ms_{5000U};
  std::uint32_t next_attempt_ms_{0U};
  bool armed_{false};
};

class MicrosExtender {
public:
  void reset();
  std::uint64_t extend(std::uint32_t raw_micros) noexcept;

private:
  std::uint32_t previous_{0U};
  std::uint64_t epoch_{0U};
  bool initialized_{false};
};

class FixedRateGate {
public:
  explicit FixedRateGate(std::uint64_t period_us);

  void reset();
  bool due(std::uint64_t now_us) noexcept;
  std::uint64_t period_us() const noexcept;

private:
  std::uint64_t period_us_{1U};
  std::uint64_t next_due_us_{0U};
  bool initialized_{false};
};

enum class SenderMode : std::uint8_t {
  kLive = 0U,
  kDiagnostic,
};

bool can_emit_packet(SenderMode mode, bool wifi_connected, bool sensor_healthy, bool calibrated,
                     bool features_valid) noexcept;

protocol::SyntheticAudioFrame make_diagnostic_frame() noexcept;

} // namespace wledimuudp::firmware
