#pragma once

#include <array>
#include <cstdint>

// Copy this file to config/wifi.local.hpp and replace only the local copy.
// config/wifi.local.hpp is ignored by Git and must never be committed with real credentials.

namespace wledimuudp::config {

inline constexpr char kWifiSsid[] = "CHANGE_ME";
inline constexpr char kWifiPassword[] = "CHANGE_ME";

inline constexpr std::array<std::uint8_t, 4> kMulticastAddress{239U, 0U, 0U, 1U};
inline constexpr std::uint16_t kMulticastPort = 11988U;
inline constexpr std::uint16_t kPacketRateHz = 50U;
inline constexpr std::uint32_t kReconnectIntervalMs = 5000U;
inline constexpr bool kDiagnosticModeOnBoot = false;

} // namespace wledimuudp::config
