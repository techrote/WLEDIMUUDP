#pragma once

#include <cstddef>
#include <cstdint>

#include <Wire.h>

#include <wledimuudp/firmware_support.hpp>
#include <wledimuudp/motion_features.hpp>

namespace wledimuudp::platform {

enum class Qmi8658Status : std::uint8_t {
  kUninitialized = 0U,
  kOk,
  kBusInitFailed,
  kNotFound,
  kWrongIdentity,
  kConfigureFailed,
  kReadFailed,
};

class Qmi8658Adapter {
public:
  explicit Qmi8658Adapter(firmware::Qmi8658Config config = {});

  bool begin(TwoWire &wire = Wire,
             const firmware::BoardProfile &profile = firmware::kWaveshareEsp32S3Matrix);
  bool read(std::uint64_t timestamp_us, motion::ImuSample &out);

  Qmi8658Status status() const noexcept;
  bool healthy() const noexcept;
  std::uint8_t address() const noexcept;
  std::uint32_t successful_reads() const noexcept;
  std::uint32_t errors() const noexcept;

private:
  bool probe(std::uint8_t address, std::uint8_t &identity);
  bool configure();
  bool write_register(std::uint8_t reg, std::uint8_t value);
  bool read_register(std::uint8_t address, std::uint8_t reg, std::uint8_t &value);
  bool read_block(std::uint8_t reg, std::uint8_t *data, std::size_t size);

  firmware::Qmi8658Config config_{};
  firmware::BoardProfile profile_{firmware::kWaveshareEsp32S3Matrix};
  TwoWire *wire_{nullptr};
  std::uint8_t address_{0U};
  Qmi8658Status status_{Qmi8658Status::kUninitialized};
  std::uint32_t successful_reads_{0U};
  std::uint32_t errors_{0U};
  bool bus_started_{false};
};

const char *qmi8658_status_name(Qmi8658Status status) noexcept;

} // namespace wledimuudp::platform
