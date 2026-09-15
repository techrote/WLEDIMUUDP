#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wledimuudp::protocol {

inline constexpr std::size_t kAudioSyncV2PacketSize = 44;
inline constexpr std::array<std::uint8_t, 6> kAudioSyncV2Header{0x30, 0x30, 0x30, 0x30, 0x32, 0x00};
inline constexpr std::array<std::uint8_t, 4> kDefaultMulticastAddress{239, 0, 0, 1};
inline constexpr std::uint16_t kDefaultAudioSyncPort = 11988;

struct SyntheticAudioFrame {
  float sample_raw = 0.0F;
  float sample_smoothed = 0.0F;
  bool sample_peak = false;
  std::array<std::uint16_t, 16> bands{};
  float magnitude = 0.0F;
  float major_peak = 1.0F;
};

using AudioSyncV2Packet = std::array<std::uint8_t, kAudioSyncV2PacketSize>;

enum class DecodeError : std::uint8_t {
  kNone = 0,
  kNullData,
  kWrongSize,
  kWrongHeader,
  kNonZeroReserved,
  kNonFiniteFloat,
  kInvalidMajorPeak,
};

struct DecodeResult {
  DecodeError error = DecodeError::kNone;
  SyntheticAudioFrame frame{};

  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == DecodeError::kNone;
  }
};

[[nodiscard]] AudioSyncV2Packet encode_audio_sync_v2(const SyntheticAudioFrame &frame) noexcept;

[[nodiscard]] DecodeResult decode_audio_sync_v2(const std::uint8_t *data,
                                                std::size_t size) noexcept;

[[nodiscard]] inline DecodeResult decode_audio_sync_v2(const AudioSyncV2Packet &packet) noexcept {
  return decode_audio_sync_v2(packet.data(), packet.size());
}

} // namespace wledimuudp::protocol
