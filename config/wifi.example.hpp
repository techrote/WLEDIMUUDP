#pragma once

// Copy this file to config/wifi.local.hpp when Wi-Fi transport is implemented.
// config/wifi.local.hpp is ignored by Git and must never contain committed credentials.

namespace wledimuudp::config {

inline constexpr char kWifiSsid[] = "CHANGE_ME";
inline constexpr char kWifiPassword[] = "CHANGE_ME";

}  // namespace wledimuudp::config
