#include "qmi8658_adapter.hpp"

#include <array>

namespace wledimuudp::platform {

Qmi8658Adapter::Qmi8658Adapter(firmware::Qmi8658Config config) : config_(config) {}

bool Qmi8658Adapter::begin(TwoWire &wire, const firmware::BoardProfile &profile) {
  wire_ = &wire;
  profile_ = profile;
  status_ = Qmi8658Status::kUninitialized;

  if (!bus_started_) {
    if (!wire_->begin(profile_.i2c_sda, profile_.i2c_scl, profile_.i2c_frequency_hz)) {
      status_ = Qmi8658Status::kBusInitFailed;
      ++errors_;
      return false;
    }
    bus_started_ = true;
  }

  std::uint8_t identity = 0U;
  const std::uint8_t primary = profile_.imu_address;
  if (probe(primary, identity) && identity == config_.who_am_i_value) {
    address_ = primary;
  } else {
    identity = 0U;
    if (config_.alternate_address == primary || !probe(config_.alternate_address, identity)) {
      status_ = Qmi8658Status::kNotFound;
      ++errors_;
      return false;
    }
    if (identity != config_.who_am_i_value) {
      status_ = Qmi8658Status::kWrongIdentity;
      ++errors_;
      return false;
    }
    address_ = config_.alternate_address;
  }

  if (!configure()) {
    status_ = Qmi8658Status::kConfigureFailed;
    ++errors_;
    return false;
  }

  delay(75U);
  status_ = Qmi8658Status::kOk;
  return true;
}

bool Qmi8658Adapter::read(const std::uint64_t timestamp_us, motion::ImuSample &out) {
  std::array<std::uint8_t, firmware::kQmi8658MotionDataSize> data{};
  if (status_ != Qmi8658Status::kOk ||
      !read_block(firmware::kQmi8658AccelXLowRegister, data.data(), data.size())) {
    out = {};
    out.timestamp_us = timestamp_us;
    out.valid = false;
    status_ = Qmi8658Status::kReadFailed;
    ++errors_;
    return false;
  }

  if (!firmware::decode_qmi8658_motion_data(data.data(), data.size(), timestamp_us, config_,
                                            profile_.axes, out)) {
    status_ = Qmi8658Status::kReadFailed;
    ++errors_;
    return false;
  }

  ++successful_reads_;
  return true;
}

Qmi8658Status Qmi8658Adapter::status() const noexcept {
  return status_;
}

bool Qmi8658Adapter::healthy() const noexcept {
  return status_ == Qmi8658Status::kOk;
}

std::uint8_t Qmi8658Adapter::address() const noexcept {
  return address_;
}

std::uint32_t Qmi8658Adapter::successful_reads() const noexcept {
  return successful_reads_;
}

std::uint32_t Qmi8658Adapter::errors() const noexcept {
  return errors_;
}

bool Qmi8658Adapter::probe(const std::uint8_t address, std::uint8_t &identity) {
  return read_register(address, firmware::kQmi8658WhoAmIRegister, identity);
}

bool Qmi8658Adapter::configure() {
  return write_register(firmware::kQmi8658Ctrl1Register, config_.ctrl1) &&
         write_register(firmware::kQmi8658Ctrl2Register, config_.ctrl2) &&
         write_register(firmware::kQmi8658Ctrl3Register, config_.ctrl3) &&
         write_register(firmware::kQmi8658Ctrl5Register, config_.ctrl5) &&
         write_register(firmware::kQmi8658Ctrl7Register, config_.ctrl7);
}

bool Qmi8658Adapter::write_register(const std::uint8_t reg, const std::uint8_t value) {
  if (wire_ == nullptr || address_ == 0U) {
    return false;
  }
  wire_->beginTransmission(address_);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission(true) == 0U;
}

bool Qmi8658Adapter::read_register(const std::uint8_t address, const std::uint8_t reg,
                                   std::uint8_t &value) {
  if (wire_ == nullptr) {
    return false;
  }
  wire_->beginTransmission(address);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0U) {
    return false;
  }
  if (wire_->requestFrom(address, static_cast<std::size_t>(1U), true) != 1U ||
      wire_->available() < 1) {
    return false;
  }
  value = static_cast<std::uint8_t>(wire_->read());
  return true;
}

bool Qmi8658Adapter::read_block(const std::uint8_t reg, std::uint8_t *data,
                                const std::size_t size) {
  if (wire_ == nullptr || data == nullptr || size == 0U || size > 255U) {
    return false;
  }
  wire_->beginTransmission(address_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0U) {
    return false;
  }

  const std::size_t received = wire_->requestFrom(address_, size, true);
  if (received != size) {
    return false;
  }
  for (std::size_t index = 0U; index < size; ++index) {
    if (wire_->available() < 1) {
      return false;
    }
    data[index] = static_cast<std::uint8_t>(wire_->read());
  }
  return true;
}

const char *qmi8658_status_name(const Qmi8658Status status) noexcept {
  switch (status) {
  case Qmi8658Status::kUninitialized:
    return "uninitialized";
  case Qmi8658Status::kOk:
    return "ok";
  case Qmi8658Status::kBusInitFailed:
    return "bus-init-failed";
  case Qmi8658Status::kNotFound:
    return "not-found";
  case Qmi8658Status::kWrongIdentity:
    return "wrong-identity";
  case Qmi8658Status::kConfigureFailed:
    return "configure-failed";
  case Qmi8658Status::kReadFailed:
    return "read-failed";
  }
  return "unknown";
}

} // namespace wledimuudp::platform
