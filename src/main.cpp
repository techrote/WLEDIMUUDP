#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <cstdint>
#include <cstring>

#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/firmware_support.hpp>
#include <wledimuudp/motion_features.hpp>
#include <wledimuudp/synthetic_audio_mapper.hpp>

#include "qmi8658_adapter.hpp"

#if __has_include("wifi.local.hpp")
#include "wifi.local.hpp"
#else
#include "wifi.example.hpp"
#endif

namespace {

using wledimuudp::firmware::CalibrationResult;
using wledimuudp::firmware::FixedRateGate;
using wledimuudp::firmware::MicrosExtender;
using wledimuudp::firmware::ReconnectGate;
using wledimuudp::firmware::SenderMode;
using wledimuudp::firmware::StartupCalibration;
using wledimuudp::mapping::SyntheticAudioMapper;
using wledimuudp::motion::MotionFeatureExtractor;
using wledimuudp::motion::MotionFeatures;
using wledimuudp::platform::Qmi8658Adapter;
using wledimuudp::protocol::encode_audio_sync_v2;

constexpr std::uint64_t kImuAcquisitionPeriodUs = 4460U;
constexpr std::uint64_t kStatusPeriodUs = 1000000U;
constexpr std::uint64_t kSensorRetryPeriodUs = 1000000U;
constexpr std::uint32_t kSerialBaud = 115200U;

constexpr std::uint64_t packet_period_us() {
  return wledimuudp::config::kPacketRateHz > 0U
             ? 1000000ULL / static_cast<std::uint64_t>(wledimuudp::config::kPacketRateHz)
             : 20000ULL;
}

struct FirmwareCounters {
  std::uint32_t sensor_samples{0U};
  std::uint32_t sensor_errors{0U};
  std::uint32_t packets_generated{0U};
  std::uint32_t packets_sent{0U};
  std::uint32_t send_errors{0U};
  std::uint32_t reconnect_attempts{0U};
  std::uint32_t skipped_sends{0U};
  std::uint32_t sensor_samples_window{0U};
  std::uint32_t packets_sent_window{0U};
};

Qmi8658Adapter imu;
MotionFeatureExtractor motion;
SyntheticAudioMapper mapper;
StartupCalibration calibration;
MicrosExtender monotonic_clock;
FixedRateGate imu_gate(kImuAcquisitionPeriodUs);
FixedRateGate send_gate(packet_period_us());
FixedRateGate status_gate(kStatusPeriodUs);
ReconnectGate reconnect_gate(wledimuudp::config::kReconnectIntervalMs);
WiFiUDP udp;

FirmwareCounters counters{};
MotionFeatures latest_features{};
SenderMode sender_mode = wledimuudp::config::kDiagnosticModeOnBoot ? SenderMode::kDiagnostic
                                                                   : SenderMode::kLive;
bool sensor_healthy = false;
bool calibrated = false;
bool latest_features_valid = false;
bool wifi_was_connected = false;
std::uint64_t next_sensor_probe_us = 0U;

bool credentials_configured() {
  return wledimuudp::config::kWifiSsid[0] != '\0' &&
         std::strcmp(wledimuudp::config::kWifiSsid, "CHANGE_ME") != 0;
}

IPAddress multicast_address() {
  const auto &address = wledimuudp::config::kMulticastAddress;
  return {address[0], address[1], address[2], address[3]};
}

const char *calibration_result_name(const CalibrationResult result) {
  switch (result) {
  case CalibrationResult::kAccepted:
    return "accepted";
  case CalibrationResult::kNotReady:
    return "not-ready";
  case CalibrationResult::kMotionDetected:
    return "motion-detected";
  case CalibrationResult::kAccelTooNoisy:
    return "accel-too-noisy";
  case CalibrationResult::kGyroTooNoisy:
    return "gyro-too-noisy";
  case CalibrationResult::kInvalidCalibration:
    return "invalid";
  }
  return "unknown";
}

void print_network_target() {
  const auto target = multicast_address();
  Serial.print("Audio Sync target=");
  Serial.print(target);
  Serial.print(':');
  Serial.print(wledimuudp::config::kMulticastPort);
  Serial.print(" packet_rate_hz=");
  Serial.println(wledimuudp::config::kPacketRateHz);
}

void print_commands() {
  Serial.println("Commands: d=diagnostic pattern, l=live IMU, r=recalibrate, ?=help");
}

void begin_calibration(const char *reason) {
  calibration.reset();
  motion.reset();
  mapper.reset();
  latest_features = {};
  latest_features_valid = false;
  calibrated = false;
  Serial.print("Calibration started: ");
  Serial.print(reason);
  Serial.print("; hold board stationary for ");
  Serial.print(calibration.policy().minimum_samples);
  Serial.println(" valid samples.");
}

void print_calibration(const wledimuudp::motion::MotionCalibration &value) {
  Serial.print("Calibration accepted gyro_bias_dps=");
  Serial.print(value.gyro_bias_dps.x, 4);
  Serial.print(',');
  Serial.print(value.gyro_bias_dps.y, 4);
  Serial.print(',');
  Serial.print(value.gyro_bias_dps.z, 4);
  Serial.print(" gravity_g=");
  Serial.print(value.gravity_reference_g, 4);
  Serial.print(" accel_noise_g=");
  Serial.print(value.accel_noise_floor_g, 5);
  Serial.print(" gyro_noise_dps=");
  Serial.println(value.gyro_noise_floor_dps, 4);
}

void handle_serial_commands() {
  while (Serial.available() > 0) {
    const int command = Serial.read();
    if (command == 'd' || command == 'D') {
      sender_mode = SenderMode::kDiagnostic;
      Serial.println("Sender mode=diagnostic; known synthetic frame does not require IMU input.");
    } else if (command == 'l' || command == 'L') {
      sender_mode = SenderMode::kLive;
      Serial.println("Sender mode=live IMU.");
    } else if (command == 'r' || command == 'R') {
      if (sensor_healthy) {
        begin_calibration("serial request");
      } else {
        Serial.println("Recalibration unavailable: IMU is not healthy.");
      }
    } else if (command == '?') {
      print_commands();
    }
  }
}

void service_wifi(const std::uint32_t now_ms) {
  if (!credentials_configured()) {
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != wifi_was_connected) {
    wifi_was_connected = connected;
    if (connected) {
      Serial.print("Wi-Fi connected local_ip=");
      Serial.println(WiFi.localIP());
      print_network_target();
    } else {
      Serial.println("Wi-Fi disconnected; packet transmission paused.");
    }
  }

  if (reconnect_gate.should_attempt(now_ms, connected)) {
    ++counters.reconnect_attempts;
    WiFi.mode(WIFI_STA);
    WiFi.begin(wledimuudp::config::kWifiSsid, wledimuudp::config::kWifiPassword);
    Serial.print("Wi-Fi connect attempt #");
    Serial.println(counters.reconnect_attempts);
  }
}

void mark_sensor_failed(const std::uint64_t now_us) {
  sensor_healthy = false;
  calibrated = false;
  latest_features_valid = false;
  mapper.reset();
  next_sensor_probe_us = now_us + kSensorRetryPeriodUs;
  Serial.print("IMU read failed status=");
  Serial.print(wledimuudp::platform::qmi8658_status_name(imu.status()));
  Serial.println("; live packets paused until re-probe and recalibration.");
}

void service_sensor(const std::uint64_t now_us) {
  if (!sensor_healthy) {
    if (now_us < next_sensor_probe_us) {
      return;
    }
    if (imu.begin()) {
      sensor_healthy = true;
      Serial.print("QMI8658 ready address=0x");
      Serial.println(imu.address(), HEX);
      begin_calibration("sensor ready");
    } else {
      next_sensor_probe_us = now_us + kSensorRetryPeriodUs;
      Serial.print("QMI8658 probe failed status=");
      Serial.println(wledimuudp::platform::qmi8658_status_name(imu.status()));
    }
    return;
  }

  if (!imu_gate.due(now_us)) {
    return;
  }

  wledimuudp::motion::ImuSample sample{};
  if (!imu.read(now_us, sample)) {
    ++counters.sensor_errors;
    mark_sensor_failed(now_us);
    return;
  }

  ++counters.sensor_samples;
  ++counters.sensor_samples_window;

  if (!calibrated) {
    calibration.add(sample);
    if (!calibration.ready()) {
      return;
    }

    wledimuudp::motion::MotionCalibration accepted{};
    const CalibrationResult result = calibration.finalize(accepted);
    if (result == CalibrationResult::kAccepted) {
      motion.set_calibration(accepted);
      calibrated = true;
      print_calibration(accepted);
    } else {
      Serial.print("Calibration rejected reason=");
      Serial.print(calibration_result_name(result));
      Serial.println("; keep device still, retrying.");
      calibration.reset();
    }
    return;
  }

  latest_features = motion.process(sample);
  latest_features_valid = latest_features.input_valid;
}

bool transmit_frame(const wledimuudp::protocol::SyntheticAudioFrame &frame) {
  const auto packet = encode_audio_sync_v2(frame);
  const auto target = multicast_address();
  if (udp.beginPacketMulticast(target, wledimuudp::config::kMulticastPort, WiFi.localIP()) != 1) {
    ++counters.send_errors;
    return false;
  }

  const std::size_t written = udp.write(packet.data(), packet.size());
  const int result = udp.endPacket();
  if (written != packet.size() || result != 1) {
    ++counters.send_errors;
    return false;
  }

  ++counters.packets_sent;
  ++counters.packets_sent_window;
  return true;
}

void service_sender(const std::uint64_t now_us) {
  if (!send_gate.due(now_us)) {
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!wledimuudp::firmware::can_emit_packet(sender_mode, connected, sensor_healthy, calibrated,
                                             latest_features_valid)) {
    ++counters.skipped_sends;
    return;
  }

  const auto frame = sender_mode == SenderMode::kDiagnostic
                         ? wledimuudp::firmware::make_diagnostic_frame()
                         : mapper.map(latest_features);
  ++counters.packets_generated;
  transmit_frame(frame);
}

void service_status(const std::uint64_t now_us) {
  if (!status_gate.due(now_us)) {
    return;
  }

  Serial.print("status mode=");
  Serial.print(sender_mode == SenderMode::kDiagnostic ? "diagnostic" : "live");
  Serial.print(" imu=");
  Serial.print(wledimuudp::platform::qmi8658_status_name(imu.status()));
  Serial.print(" calibrated=");
  Serial.print(calibrated ? "yes" : "no");
  Serial.print(" wifi=");
  Serial.print(WiFi.status() == WL_CONNECTED ? "connected" : "offline");
  Serial.print(" imu_samples_1s=");
  Serial.print(counters.sensor_samples_window);
  Serial.print(" packets_sent_1s=");
  Serial.print(counters.packets_sent_window);
  Serial.print(" sent_total=");
  Serial.print(counters.packets_sent);
  Serial.print(" send_errors=");
  Serial.print(counters.send_errors);
  Serial.print(" sensor_errors=");
  Serial.print(counters.sensor_errors);
  Serial.print(" reconnects=");
  Serial.print(counters.reconnect_attempts);
  Serial.print(" skipped=");
  Serial.println(counters.skipped_sends);

  counters.sensor_samples_window = 0U;
  counters.packets_sent_window = 0U;
}

} // namespace

void setup() {
  Serial.begin(kSerialBaud);
  delay(100U);

  Serial.println("WLEDIMUUDP WU-005 reference sender");
  Serial.print("board=");
  Serial.println(wledimuudp::firmware::kWaveshareEsp32S3Matrix.name);
  Serial.print("QMI8658 I2C SDA=");
  Serial.print(wledimuudp::firmware::kWaveshareEsp32S3Matrix.i2c_sda);
  Serial.print(" SCL=");
  Serial.print(wledimuudp::firmware::kWaveshareEsp32S3Matrix.i2c_scl);
  Serial.print(" bus_hz=");
  Serial.println(wledimuudp::firmware::kWaveshareEsp32S3Matrix.i2c_frequency_hz);
  Serial.print("QMI8658 profile accel=+-8g gyro=+-1024dps effective_6dof_odr_hz=");
  Serial.println(wledimuudp::firmware::kReferenceQmi8658Config.effective_six_dof_odr_hz, 1);
  Serial.print("acquisition_period_us=");
  Serial.print(kImuAcquisitionPeriodUs);
  Serial.print(" send_period_us=");
  Serial.println(send_gate.period_us());
  print_network_target();
  print_commands();

  if (!credentials_configured()) {
    Serial.println("Wi-Fi credentials are not configured; copy config/wifi.example.hpp to wifi.local.hpp.");
  }

  const std::uint64_t now_us = monotonic_clock.extend(micros());
  sensor_healthy = imu.begin();
  if (sensor_healthy) {
    Serial.print("QMI8658 ready address=0x");
    Serial.println(imu.address(), HEX);
    begin_calibration("startup");
  } else {
    next_sensor_probe_us = now_us + kSensorRetryPeriodUs;
    Serial.print("QMI8658 startup probe failed status=");
    Serial.println(wledimuudp::platform::qmi8658_status_name(imu.status()));
  }
}

void loop() {
  const std::uint64_t now_us = monotonic_clock.extend(micros());
  handle_serial_commands();
  service_wifi(millis());
  service_sensor(now_us);
  service_sender(now_us);
  service_status(now_us);
  yield();
}
