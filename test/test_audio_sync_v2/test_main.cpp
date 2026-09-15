#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <wledimuudp/audio_sync_v2.hpp>

#include "golden_fixture.hpp"

using wledimuudp::protocol::AudioSyncV2Packet;
using wledimuudp::protocol::DecodeError;
using wledimuudp::protocol::SyntheticAudioFrame;
using wledimuudp::protocol::decode_audio_sync_v2;
using wledimuudp::protocol::encode_audio_sync_v2;
using wledimuudp::protocol::kAudioSyncV2Header;
using wledimuudp::protocol::kAudioSyncV2PacketSize;
using wledimuudp::test_fixture::kGoldenAudioSyncV2Packet;

namespace {

SyntheticAudioFrame make_golden_frame() {
  SyntheticAudioFrame frame{};
  frame.sample_raw = 1.0F;
  frame.sample_smoothed = 2.5F;
  frame.sample_peak = true;
  for (std::size_t index = 0; index < frame.bands.size(); ++index) {
    frame.bands[index] = static_cast<std::uint16_t>(index);
  }
  frame.magnitude = 16.0F;
  frame.major_peak = 440.0F;
  return frame;
}

void test_encoded_size_header_and_reserved_bytes() {
  const AudioSyncV2Packet packet = encode_audio_sync_v2(make_golden_frame());
  TEST_ASSERT_EQUAL_UINT32(44U, packet.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kAudioSyncV2Header.data(), packet.data(), kAudioSyncV2Header.size());
  TEST_ASSERT_EQUAL_UINT8(0U, packet[6]);
  TEST_ASSERT_EQUAL_UINT8(0U, packet[7]);
  TEST_ASSERT_EQUAL_UINT8(0U, packet[17]);
  TEST_ASSERT_EQUAL_UINT8(0U, packet[34]);
  TEST_ASSERT_EQUAL_UINT8(0U, packet[35]);
}

void test_exact_golden_packet() {
  const AudioSyncV2Packet packet = encode_audio_sync_v2(make_golden_frame());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(kGoldenAudioSyncV2Packet.data(), packet.data(), packet.size());
}

void test_known_float_bytes_and_offsets() {
  const AudioSyncV2Packet packet = encode_audio_sync_v2(make_golden_frame());
  const std::array<std::uint8_t, 4> raw{0x00, 0x00, 0x80, 0x3F};
  const std::array<std::uint8_t, 4> smoothed{0x00, 0x00, 0x20, 0x40};
  const std::array<std::uint8_t, 4> magnitude{0x00, 0x00, 0x80, 0x41};
  const std::array<std::uint8_t, 4> major_peak{0x00, 0x00, 0xDC, 0x43};

  TEST_ASSERT_EQUAL_UINT8_ARRAY(raw.data(), packet.data() + 8, raw.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(smoothed.data(), packet.data() + 12, smoothed.size());
  TEST_ASSERT_EQUAL_UINT8(1U, packet[16]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(magnitude.data(), packet.data() + 36, magnitude.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(major_peak.data(), packet.data() + 40, major_peak.size());
}

void test_band_offsets_and_clamping() {
  SyntheticAudioFrame frame{};
  for (std::size_t index = 0; index < frame.bands.size(); ++index) {
    frame.bands[index] = static_cast<std::uint16_t>(240U + index);
  }
  frame.bands[14] = 255U;
  frame.bands[15] = 1024U;

  const AudioSyncV2Packet packet = encode_audio_sync_v2(frame);
  for (std::size_t index = 0; index < 14; ++index) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(240U + index), packet[18 + index]);
  }
  TEST_ASSERT_EQUAL_UINT8(254U, packet[32]);
  TEST_ASSERT_EQUAL_UINT8(254U, packet[33]);
}

void test_non_finite_and_out_of_range_values_are_sanitised() {
  SyntheticAudioFrame frame{};
  frame.sample_raw = -10.0F;
  frame.sample_smoothed = 999.0F;
  frame.sample_peak = true;
  frame.magnitude = std::numeric_limits<float>::infinity();
  frame.major_peak = std::numeric_limits<float>::quiet_NaN();

  const AudioSyncV2Packet packet = encode_audio_sync_v2(frame);
  const auto decoded = decode_audio_sync_v2(packet);
  TEST_ASSERT_TRUE(decoded.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, decoded.frame.sample_raw);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 255.0F, decoded.frame.sample_smoothed);
  TEST_ASSERT_TRUE(decoded.frame.sample_peak);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, decoded.frame.magnitude);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.0F, decoded.frame.major_peak);
}

void test_repeated_encoding_is_deterministic() {
  const SyntheticAudioFrame frame = make_golden_frame();
  const AudioSyncV2Packet first = encode_audio_sync_v2(frame);
  const AudioSyncV2Packet second = encode_audio_sync_v2(frame);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(first.data(), second.data(), first.size());
}

void test_decoder_round_trips_golden_packet() {
  const auto decoded = decode_audio_sync_v2(kGoldenAudioSyncV2Packet.data(),
                                            kGoldenAudioSyncV2Packet.size());
  TEST_ASSERT_TRUE(decoded.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 1.0F, decoded.frame.sample_raw);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 2.5F, decoded.frame.sample_smoothed);
  TEST_ASSERT_TRUE(decoded.frame.sample_peak);
  for (std::size_t index = 0; index < decoded.frame.bands.size(); ++index) {
    TEST_ASSERT_EQUAL_UINT16(static_cast<std::uint16_t>(index), decoded.frame.bands[index]);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 16.0F, decoded.frame.magnitude);
  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 440.0F, decoded.frame.major_peak);
}

void test_decoder_rejects_wrong_size_and_null_data() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(DecodeError::kNullData),
                          static_cast<std::uint8_t>(decode_audio_sync_v2(nullptr, 44).error));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(DecodeError::kWrongSize),
      static_cast<std::uint8_t>(decode_audio_sync_v2(kGoldenAudioSyncV2Packet.data(), 43).error));
}

void test_decoder_rejects_wrong_header_and_reserved_bytes() {
  AudioSyncV2Packet packet = kGoldenAudioSyncV2Packet;
  packet[0] = 0x31;
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(DecodeError::kWrongHeader),
                          static_cast<std::uint8_t>(decode_audio_sync_v2(packet).error));

  packet = kGoldenAudioSyncV2Packet;
  packet[34] = 1U;
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(DecodeError::kNonZeroReserved),
                          static_cast<std::uint8_t>(decode_audio_sync_v2(packet).error));
}

void test_decoder_rejects_non_finite_and_invalid_major_peak() {
  AudioSyncV2Packet packet = kGoldenAudioSyncV2Packet;
  packet[8] = 0x00;
  packet[9] = 0x00;
  packet[10] = 0x80;
  packet[11] = 0x7F;
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(DecodeError::kNonFiniteFloat),
                          static_cast<std::uint8_t>(decode_audio_sync_v2(packet).error));

  packet = kGoldenAudioSyncV2Packet;
  packet[40] = 0x00;
  packet[41] = 0x00;
  packet[42] = 0x00;
  packet[43] = 0x00;
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(DecodeError::kInvalidMajorPeak),
                          static_cast<std::uint8_t>(decode_audio_sync_v2(packet).error));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_encoded_size_header_and_reserved_bytes);
  RUN_TEST(test_exact_golden_packet);
  RUN_TEST(test_known_float_bytes_and_offsets);
  RUN_TEST(test_band_offsets_and_clamping);
  RUN_TEST(test_non_finite_and_out_of_range_values_are_sanitised);
  RUN_TEST(test_repeated_encoding_is_deterministic);
  RUN_TEST(test_decoder_round_trips_golden_packet);
  RUN_TEST(test_decoder_rejects_wrong_size_and_null_data);
  RUN_TEST(test_decoder_rejects_wrong_header_and_reserved_bytes);
  RUN_TEST(test_decoder_rejects_non_finite_and_invalid_major_peak);
  return UNITY_END();
}
