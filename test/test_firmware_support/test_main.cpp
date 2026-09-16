#include <unity.h>

#include <array>
#include <cstdint>
#include <cstring>

#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/firmware_support.hpp>

namespace {

using wledimuudp::firmware::AxisTransform;
using wledimuudp::firmware::CalibrationResult;
using wledimuudp::firmware::can_emit_packet;
using wledimuudp::firmware::decode_qmi8658_motion_data;
using wledimuudp::firmware::FixedRateGate;
using wledimuudp::firmware::MicrosExtender;
using wledimuudp::firmware::ReconnectGate;
using wledimuudp::firmware::SenderMode;
using wledimuudp::firmware::StartupCalibration;
using wledimuudp::motion::ImuSample;
using wledimuudp::motion::MotionCalibration;

void put_i16_le(std::array<std::uint8_t, 12> &data, const std::size_t offset,
                const std::int16_t value) {
  const std::uint16_t bits = static_cast<std::uint16_t>(value);
  data[offset] = static_cast<std::uint8_t>(bits & 0xFFU);
  data[offset + 1U] = static_cast<std::uint8_t>((bits >> 8U) & 0xFFU);
}

void test_reference_board_profile_locks_waveshare_pins() {
  const auto &profile = wledimuudp::firmware::kWaveshareEsp32S3Matrix;
  TEST_ASSERT_EQUAL_STRING("waveshare-esp32-s3-matrix", profile.name);
  TEST_ASSERT_EQUAL_INT8(11, profile.i2c_sda);
  TEST_ASSERT_EQUAL_INT8(12, profile.i2c_scl);
  TEST_ASSERT_EQUAL_INT8(10, profile.imu_int1);
  TEST_ASSERT_EQUAL_INT8(13, profile.imu_int2);
  TEST_ASSERT_EQUAL_UINT32(400000U, profile.i2c_frequency_hz);
  TEST_ASSERT_EQUAL_HEX8(0x6B, profile.imu_address);
}

void test_axis_transform_is_explicit_signed_permutation() {
  const wledimuudp::motion::Vec3 input{1.0F, 2.0F, 3.0F};
  const AxisTransform transform{{2U, 0U, 1U}, {-1, 1, -1}};
  const auto output = wledimuudp::firmware::apply_axis_transform(input, transform);
  TEST_ASSERT_EQUAL_FLOAT(-3.0F, output.x);
  TEST_ASSERT_EQUAL_FLOAT(1.0F, output.y);
  TEST_ASSERT_EQUAL_FLOAT(-2.0F, output.z);
}

void test_reference_qmi8658_configuration_is_reviewable() {
  const auto &config = wledimuudp::firmware::kReferenceQmi8658Config;
  TEST_ASSERT_EQUAL_HEX8(0x6B, config.primary_address);
  TEST_ASSERT_EQUAL_HEX8(0x6A, config.alternate_address);
  TEST_ASSERT_EQUAL_HEX8(0x05, config.who_am_i_value);
  TEST_ASSERT_EQUAL_HEX8(0x40, config.ctrl1);
  TEST_ASSERT_EQUAL_HEX8(0x25, config.ctrl2);
  TEST_ASSERT_EQUAL_HEX8(0x65, config.ctrl3);
  TEST_ASSERT_EQUAL_HEX8(0x00, config.ctrl5);
  TEST_ASSERT_EQUAL_HEX8(0x03, config.ctrl7);
  TEST_ASSERT_EQUAL_FLOAT(4096.0F, config.accel_lsb_per_g);
  TEST_ASSERT_EQUAL_FLOAT(32.0F, config.gyro_lsb_per_dps);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 224.2F, config.effective_six_dof_odr_hz);
}

void test_qmi8658_raw_decode_converts_units_and_axes() {
  std::array<std::uint8_t, 12> data{};
  put_i16_le(data, 0U, 4096);
  put_i16_le(data, 2U, -2048);
  put_i16_le(data, 4U, 1024);
  put_i16_le(data, 6U, 3200);
  put_i16_le(data, 8U, -1600);
  put_i16_le(data, 10U, 800);

  ImuSample sample{};
  const AxisTransform transform{{1U, 0U, 2U}, {1, -1, 1}};
  const auto &config = wledimuudp::firmware::kReferenceQmi8658Config;
  const bool decoded =
      decode_qmi8658_motion_data(data.data(), data.size(), 123456U, config, transform, sample);
  TEST_ASSERT_TRUE(decoded);
  TEST_ASSERT_TRUE(sample.valid);
  TEST_ASSERT_EQUAL_UINT64(123456U, sample.timestamp_us);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, -0.5F, sample.accel_g.x);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, -1.0F, sample.accel_g.y);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.25F, sample.accel_g.z);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, -50.0F, sample.gyro_dps.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, -100.0F, sample.gyro_dps.y);
  TEST_ASSERT_FLOAT_WITHIN(0.001F, 25.0F, sample.gyro_dps.z);
}

void test_qmi8658_raw_decode_rejects_malformed_input() {
  std::array<std::uint8_t, 12> data{};
  ImuSample sample{};
  const auto &config = wledimuudp::firmware::kReferenceQmi8658Config;
  TEST_ASSERT_FALSE(decode_qmi8658_motion_data(data.data(), 11U, 42U, config, {}, sample));
  TEST_ASSERT_FALSE(sample.valid);
  TEST_ASSERT_EQUAL_UINT64(42U, sample.timestamp_us);
}

void test_stationary_startup_calibration_is_accepted() {
  StartupCalibration session;
  for (std::uint64_t index = 0U; index < 256U; ++index) {
    const float dither = (index % 2U == 0U) ? 0.001F : -0.001F;
    ImuSample sample{};
    sample.timestamp_us = index * 4460U + 1U;
    sample.accel_g = {dither, 0.0F, 1.0F + dither};
    sample.gyro_dps = {0.20F + dither, -0.10F, 0.05F};
    sample.valid = true;
    TEST_ASSERT_TRUE(session.add(sample));
  }
  TEST_ASSERT_TRUE(session.ready());
  TEST_ASSERT_EQUAL_UINT32(256U, session.sample_count());
  MotionCalibration calibration{};
  TEST_ASSERT_EQUAL(CalibrationResult::kAccepted, session.finalize(calibration));
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 0.20F, calibration.gyro_bias_dps.x);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, -0.10F, calibration.gyro_bias_dps.y);
  TEST_ASSERT_FLOAT_WITHIN(0.01F, 1.0F, calibration.gravity_reference_g);
}

void test_moving_startup_calibration_is_rejected() {
  StartupCalibration session;
  for (std::uint64_t index = 0U; index < 256U; ++index) {
    ImuSample sample{index * 4460U + 1U, {0.0F, 0.0F, 1.0F}, {}, true};
    if (index == 80U) {
      sample.gyro_dps.x = 20.0F;
    }
    TEST_ASSERT_TRUE(session.add(sample));
  }
  MotionCalibration calibration{};
  TEST_ASSERT_EQUAL(CalibrationResult::kMotionDetected, session.finalize(calibration));
}

void test_invalid_calibration_samples_are_counted_not_accepted() {
  StartupCalibration session;
  ImuSample invalid{1U, {0.0F, 0.0F, 1.0F}, {}, false};
  TEST_ASSERT_FALSE(session.add(invalid));
  TEST_ASSERT_EQUAL_UINT32(1U, session.invalid_sample_count());
  TEST_ASSERT_EQUAL_UINT32(0U, session.sample_count());
}

void test_network_defaults_and_send_safety_gate() {
  const auto &network = wledimuudp::firmware::kDefaultNetworkConfig;
  TEST_ASSERT_EQUAL_UINT8(239U, network.multicast_address[0]);
  TEST_ASSERT_EQUAL_UINT8(0U, network.multicast_address[1]);
  TEST_ASSERT_EQUAL_UINT8(0U, network.multicast_address[2]);
  TEST_ASSERT_EQUAL_UINT8(1U, network.multicast_address[3]);
  TEST_ASSERT_EQUAL_UINT16(11988U, network.port);
  TEST_ASSERT_EQUAL_UINT16(50U, network.packet_rate_hz);

  TEST_ASSERT_TRUE(can_emit_packet(SenderMode::kLive, true, true, true, true));
  TEST_ASSERT_FALSE(can_emit_packet(SenderMode::kLive, true, false, true, true));
  TEST_ASSERT_FALSE(can_emit_packet(SenderMode::kLive, true, true, false, true));
  TEST_ASSERT_FALSE(can_emit_packet(SenderMode::kLive, false, true, true, true));
  TEST_ASSERT_TRUE(can_emit_packet(SenderMode::kDiagnostic, true, false, false, false));
}

void test_reconnect_gate_is_bounded_and_resets_when_connected() {
  ReconnectGate gate(5000U);
  TEST_ASSERT_TRUE(gate.should_attempt(100U, false));
  TEST_ASSERT_FALSE(gate.should_attempt(1000U, false));
  TEST_ASSERT_TRUE(gate.should_attempt(5100U, false));
  TEST_ASSERT_FALSE(gate.should_attempt(5200U, true));
  TEST_ASSERT_TRUE(gate.should_attempt(5300U, false));
}

void test_micros_extender_preserves_monotonic_time_across_wrap() {
  MicrosExtender clock;
  const std::uint64_t before = clock.extend(0xFFFFFFF0U);
  const std::uint64_t after = clock.extend(0x00000020U);
  TEST_ASSERT_TRUE(after > before);
  TEST_ASSERT_EQUAL_UINT64((std::uint64_t{1U} << 32U) + 0x20U, after);
}

void test_fixed_rate_gate_skips_backlog_bursts() {
  FixedRateGate gate(5000U);
  TEST_ASSERT_TRUE(gate.due(1000U));
  TEST_ASSERT_FALSE(gate.due(2000U));
  TEST_ASSERT_TRUE(gate.due(6000U));
  TEST_ASSERT_TRUE(gate.due(30000U));
  TEST_ASSERT_FALSE(gate.due(30001U));
  TEST_ASSERT_EQUAL_UINT64(5000U, gate.period_us());
}

void test_diagnostic_frame_is_deterministic_and_canonical_encodable() {
  const auto first = wledimuudp::firmware::make_diagnostic_frame();
  const auto second = wledimuudp::firmware::make_diagnostic_frame();
  const auto a = wledimuudp::protocol::encode_audio_sync_v2(first);
  const auto b = wledimuudp::protocol::encode_audio_sync_v2(second);
  TEST_ASSERT_EQUAL_UINT32(wledimuudp::protocol::kAudioSyncV2PacketSize, a.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(a.data(), b.data(), a.size());
  TEST_ASSERT_FALSE(first.sample_peak);
  for (const auto band : first.bands) {
    TEST_ASSERT_TRUE(band <= 254U);
  }
}

} // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reference_board_profile_locks_waveshare_pins);
  RUN_TEST(test_axis_transform_is_explicit_signed_permutation);
  RUN_TEST(test_reference_qmi8658_configuration_is_reviewable);
  RUN_TEST(test_qmi8658_raw_decode_converts_units_and_axes);
  RUN_TEST(test_qmi8658_raw_decode_rejects_malformed_input);
  RUN_TEST(test_stationary_startup_calibration_is_accepted);
  RUN_TEST(test_moving_startup_calibration_is_rejected);
  RUN_TEST(test_invalid_calibration_samples_are_counted_not_accepted);
  RUN_TEST(test_network_defaults_and_send_safety_gate);
  RUN_TEST(test_reconnect_gate_is_bounded_and_resets_when_connected);
  RUN_TEST(test_micros_extender_preserves_monotonic_time_across_wrap);
  RUN_TEST(test_fixed_rate_gate_skips_backlog_bursts);
  RUN_TEST(test_diagnostic_frame_is_deterministic_and_canonical_encodable);
  return UNITY_END();
}
