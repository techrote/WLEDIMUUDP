#pragma once

#include <array>
#include <cstdint>

// Copy this file to config/wifi.local.hpp and replace only the local copy.
// config/wifi.local.hpp is ignored by Git and must never be committed with real credentials.
// Keep the validation assertions when editing the local copy: they enforce the stock-WLED
// Audio Sync V2 release contract before a bad configuration reaches runtime.

namespace wledimuudp::config {

inline constexpr char kWifiSsid[] = "CHANGE_ME";
inline constexpr char kWifiPassword[] = "CHANGE_ME";

inline constexpr std::array<std::uint8_t, 4> kMulticastAddress{239U, 0U, 0U, 1U};
inline constexpr std::uint16_t kMulticastPort = 11988U;
inline constexpr std::uint16_t kPacketRateHz = 50U;
inline constexpr std::uint32_t kReconnectIntervalMs = 5000U;
inline constexpr bool kDiagnosticModeOnBoot = false;

static_assert(kMulticastAddress[0] >= 224U && kMulticastAddress[0] <= 239U,
              "kMulticastAddress must be an IPv4 multicast address (224.0.0.0/4)");
static_assert(kMulticastPort > 0U, "kMulticastPort must be non-zero");
static_assert(kPacketRateHz >= 1U && kPacketRateHz <= 50U,
              "kPacketRateHz must remain within the documented stock-WLED 1..50 Hz range");
static_assert(kReconnectIntervalMs > 0U, "kReconnectIntervalMs must be non-zero");

} // namespace wledimuudp::config
