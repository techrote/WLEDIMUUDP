#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <wledimuudp/audio_sync_v2.hpp>

namespace wledimuudp::host {

enum class Pattern : std::uint8_t {
  kSilence = 0,
  kLow,
  kMedium,
  kHigh,
  kLevelRamp,
  kSingleBand,
  kTwoBand,
  kBroadbandPulse,
  kPeakPulse,
  kMajorPeakSweep,
  kScripted,
};

inline constexpr std::uint32_t kScriptSegmentFrames = 16U;
inline constexpr std::uint32_t kScriptCycleFrames = 10U * kScriptSegmentFrames;
inline constexpr std::uint16_t kDefaultProbeRateHz = 50U;
inline constexpr std::uint16_t kMaxProbeRateHz = 50U;

enum class HostCommand : std::uint8_t {
  kHelp = 0,
  kSend,
  kListen,
  kDecode,
};

enum class DecodeInput : std::uint8_t {
  kNone = 0,
  kHex,
  kFile,
};

struct HostOptions {
  HostCommand command = HostCommand::kHelp;
  Pattern pattern = Pattern::kScripted;
  std::array<std::uint8_t, 4> address = protocol::kDefaultMulticastAddress;
  std::uint16_t port = protocol::kDefaultAudioSyncPort;
  std::uint16_t rate_hz = kDefaultProbeRateHz;
  std::uint32_t frames = kScriptCycleFrames;
  std::uint32_t count = 10U;
  std::uint32_t timeout_ms = 3000U;
  bool dry_run = false;
  DecodeInput decode_input = DecodeInput::kNone;
  std::string input{};
};

struct ParseResult {
  bool ok = false;
  HostOptions options{};
  std::string error{};
};

[[nodiscard]] const char *pattern_name(Pattern pattern) noexcept;
[[nodiscard]] bool parse_pattern(std::string_view text, Pattern &pattern) noexcept;
[[nodiscard]] protocol::SyntheticAudioFrame make_pattern_frame(Pattern pattern,
                                                               std::uint32_t index) noexcept;
[[nodiscard]] protocol::AudioSyncV2Packet make_pattern_packet(Pattern pattern,
                                                              std::uint32_t index) noexcept;

[[nodiscard]] bool parse_ipv4(std::string_view text, std::array<std::uint8_t, 4> &address) noexcept;
[[nodiscard]] std::string format_ipv4(const std::array<std::uint8_t, 4> &address);
[[nodiscard]] bool is_multicast(const std::array<std::uint8_t, 4> &address) noexcept;

[[nodiscard]] std::string packet_to_hex(const protocol::AudioSyncV2Packet &packet);
[[nodiscard]] bool packet_from_hex(std::string_view text, protocol::AudioSyncV2Packet &packet,
                                   std::string *error = nullptr);

[[nodiscard]] ParseResult parse_host_options(int argc, const char *const argv[]);

class UdpSocket {
public:
  UdpSocket() noexcept;
  ~UdpSocket();

  UdpSocket(const UdpSocket &) = delete;
  UdpSocket &operator=(const UdpSocket &) = delete;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] int last_error() const noexcept;
  [[nodiscard]] bool set_receive_timeout(std::uint32_t timeout_ms) noexcept;
  [[nodiscard]] bool bind_any(std::uint16_t port) noexcept;
  [[nodiscard]] std::uint16_t local_port() noexcept;
  [[nodiscard]] bool join_multicast(const std::array<std::uint8_t, 4> &group) noexcept;
  [[nodiscard]] bool send_to(const std::array<std::uint8_t, 4> &address, std::uint16_t port,
                             const std::uint8_t *data, std::size_t size) noexcept;
  [[nodiscard]] int receive(std::uint8_t *data, std::size_t capacity,
                            std::array<std::uint8_t, 4> &source_address,
                            std::uint16_t &source_port) noexcept;

private:
  std::uintptr_t handle_ = 0;
  int last_error_ = 0;
};

} // namespace wledimuudp::host
