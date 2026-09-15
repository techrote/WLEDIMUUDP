#include <wledimuudp/audio_sync_v2.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>

namespace wledimuudp::protocol {
namespace {

static_assert(CHAR_BIT == 8, "Audio Sync V2 requires 8-bit bytes");
static_assert(sizeof(float) == 4, "Audio Sync V2 requires 32-bit float");
static_assert(std::numeric_limits<float>::is_iec559,
              "Audio Sync V2 requires IEEE-754 binary32 float");

constexpr std::size_t kSampleRawOffset = 8;
constexpr std::size_t kSampleSmoothedOffset = 12;
constexpr std::size_t kSamplePeakOffset = 16;
constexpr std::size_t kBandsOffset = 18;
constexpr std::size_t kMagnitudeOffset = 36;
constexpr std::size_t kMajorPeakOffset = 40;

float sanitize_level(float value) noexcept {
  if (!std::isfinite(value) || value <= 0.0F) {
    return 0.0F;
  }
  return std::min(value, 255.0F);
}

float sanitize_major_peak(float value) noexcept {
  if (!std::isfinite(value) || value <= 0.0F) {
    return 1.0F;
  }
  return value;
}

std::uint8_t sanitize_band(std::uint16_t value) noexcept {
  return static_cast<std::uint8_t>(std::min<std::uint16_t>(value, 254U));
}

void write_float_le(AudioSyncV2Packet &packet, std::size_t offset, float value) noexcept {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  packet[offset] = static_cast<std::uint8_t>(bits & 0xFFU);
  packet[offset + 1] = static_cast<std::uint8_t>((bits >> 8U) & 0xFFU);
  packet[offset + 2] = static_cast<std::uint8_t>((bits >> 16U) & 0xFFU);
  packet[offset + 3] = static_cast<std::uint8_t>((bits >> 24U) & 0xFFU);
}

float read_float_le(const std::uint8_t *data, std::size_t offset) noexcept {
  const std::uint32_t bits = static_cast<std::uint32_t>(data[offset]) |
                             (static_cast<std::uint32_t>(data[offset + 1]) << 8U) |
                             (static_cast<std::uint32_t>(data[offset + 2]) << 16U) |
                             (static_cast<std::uint32_t>(data[offset + 3]) << 24U);
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

bool header_matches(const std::uint8_t *data) noexcept {
  return std::equal(kAudioSyncV2Header.begin(), kAudioSyncV2Header.end(), data);
}

bool reserved_bytes_are_zero(const std::uint8_t *data) noexcept {
  return data[6] == 0U && data[7] == 0U && data[17] == 0U && data[34] == 0U && data[35] == 0U;
}

} // namespace

AudioSyncV2Packet encode_audio_sync_v2(const SyntheticAudioFrame &frame) noexcept {
  AudioSyncV2Packet packet{};
  std::copy(kAudioSyncV2Header.begin(), kAudioSyncV2Header.end(), packet.begin());

  write_float_le(packet, kSampleRawOffset, sanitize_level(frame.sample_raw));
  write_float_le(packet, kSampleSmoothedOffset, sanitize_level(frame.sample_smoothed));
  packet[kSamplePeakOffset] = frame.sample_peak ? 1U : 0U;

  for (std::size_t index = 0; index < frame.bands.size(); ++index) {
    packet[kBandsOffset + index] = sanitize_band(frame.bands[index]);
  }

  write_float_le(packet, kMagnitudeOffset, sanitize_level(frame.magnitude));
  write_float_le(packet, kMajorPeakOffset, sanitize_major_peak(frame.major_peak));
  return packet;
}

DecodeResult decode_audio_sync_v2(const std::uint8_t *data, std::size_t size) noexcept {
  DecodeResult result{};
  if (data == nullptr) {
    result.error = DecodeError::kNullData;
    return result;
  }
  if (size != kAudioSyncV2PacketSize) {
    result.error = DecodeError::kWrongSize;
    return result;
  }
  if (!header_matches(data)) {
    result.error = DecodeError::kWrongHeader;
    return result;
  }
  if (!reserved_bytes_are_zero(data)) {
    result.error = DecodeError::kNonZeroReserved;
    return result;
  }

  result.frame.sample_raw = read_float_le(data, kSampleRawOffset);
  result.frame.sample_smoothed = read_float_le(data, kSampleSmoothedOffset);
  result.frame.magnitude = read_float_le(data, kMagnitudeOffset);
  result.frame.major_peak = read_float_le(data, kMajorPeakOffset);

  if (!std::isfinite(result.frame.sample_raw) || !std::isfinite(result.frame.sample_smoothed) ||
      !std::isfinite(result.frame.magnitude) || !std::isfinite(result.frame.major_peak)) {
    result.error = DecodeError::kNonFiniteFloat;
    return result;
  }
  if (result.frame.major_peak <= 0.0F) {
    result.error = DecodeError::kInvalidMajorPeak;
    return result;
  }

  result.frame.sample_peak = data[kSamplePeakOffset] != 0U;
  for (std::size_t index = 0; index < result.frame.bands.size(); ++index) {
    result.frame.bands[index] = data[kBandsOffset + index];
  }
  return result;
}

} // namespace wledimuudp::protocol
