#include <unity.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/host_probe.hpp>

namespace {

using wledimuudp::host::DecodeInput;
using wledimuudp::host::HostCommand;
using wledimuudp::host::Pattern;
using wledimuudp::host::UdpSocket;
using wledimuudp::protocol::AudioSyncV2Packet;
using wledimuudp::protocol::DecodeError;

constexpr std::array<Pattern, 11> kAllPatterns{
    Pattern::kSilence,   Pattern::kLow,
    Pattern::kMedium,    Pattern::kHigh,
    Pattern::kLevelRamp, Pattern::kSingleBand,
    Pattern::kTwoBand,   Pattern::kBroadbandPulse,
    Pattern::kPeakPulse, Pattern::kMajorPeakSweep,
    Pattern::kScripted,
};

void assert_packet_equal(const AudioSyncV2Packet &expected, const AudioSyncV2Packet &actual) {
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.data(), actual.data(), expected.size());
}

void test_all_patterns_are_deterministic() {
  for (const Pattern pattern : kAllPatterns) {
    for (const std::uint32_t index : {0U, 7U, 19U, 159U, 160U}) {
      const AudioSyncV2Packet first = wledimuudp::host::make_pattern_packet(pattern, index);
      const AudioSyncV2Packet second = wledimuudp::host::make_pattern_packet(pattern, index);
      assert_packet_equal(first, second);
      TEST_ASSERT_TRUE(wledimuudp::protocol::decode_audio_sync_v2(first).ok());
    }
  }
}

void test_constant_patterns_have_ordered_levels() {
  const auto silence = wledimuudp::host::make_pattern_frame(Pattern::kSilence, 0U);
  const auto low = wledimuudp::host::make_pattern_frame(Pattern::kLow, 0U);
  const auto medium = wledimuudp::host::make_pattern_frame(Pattern::kMedium, 0U);
  const auto high = wledimuudp::host::make_pattern_frame(Pattern::kHigh, 0U);

  TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, silence.sample_raw);
  TEST_ASSERT_TRUE(low.sample_raw < medium.sample_raw);
  TEST_ASSERT_TRUE(medium.sample_raw < high.sample_raw);
  TEST_ASSERT_TRUE(low.bands[0] < medium.bands[0]);
  TEST_ASSERT_TRUE(medium.bands[0] < high.bands[0]);
}

void test_motionless_patterns_expose_expected_signatures() {
  const auto single = wledimuudp::host::make_pattern_frame(Pattern::kSingleBand, 5U);
  std::size_t non_zero = 0;
  for (const std::uint16_t band : single.bands) {
    if (band != 0U) {
      ++non_zero;
    }
  }
  TEST_ASSERT_EQUAL_UINT32(1U, non_zero);
  TEST_ASSERT_EQUAL_UINT16(220U, single.bands[5]);

  const auto peak = wledimuudp::host::make_pattern_frame(Pattern::kPeakPulse, 0U);
  const auto after_peak = wledimuudp::host::make_pattern_frame(Pattern::kPeakPulse, 1U);
  TEST_ASSERT_TRUE(peak.sample_peak);
  TEST_ASSERT_FALSE(after_peak.sample_peak);

  const auto sweep_start = wledimuudp::host::make_pattern_frame(Pattern::kMajorPeakSweep, 0U);
  const auto sweep_end = wledimuudp::host::make_pattern_frame(Pattern::kMajorPeakSweep, 15U);
  TEST_ASSERT_TRUE(sweep_end.major_peak > sweep_start.major_peak);
}

void test_scripted_sequence_has_locked_order_and_cycle() {
  constexpr std::array<Pattern, 10> expected{
      Pattern::kSilence,   Pattern::kLow,
      Pattern::kMedium,    Pattern::kHigh,
      Pattern::kLevelRamp, Pattern::kSingleBand,
      Pattern::kTwoBand,   Pattern::kBroadbandPulse,
      Pattern::kPeakPulse, Pattern::kMajorPeakSweep,
  };

  for (std::size_t segment = 0; segment < expected.size(); ++segment) {
    const std::uint32_t index =
        static_cast<std::uint32_t>(segment) * wledimuudp::host::kScriptSegmentFrames;
    assert_packet_equal(wledimuudp::host::make_pattern_packet(expected[segment], 0U),
                        wledimuudp::host::make_pattern_packet(Pattern::kScripted, index));
  }
  assert_packet_equal(wledimuudp::host::make_pattern_packet(Pattern::kScripted, 0U),
                      wledimuudp::host::make_pattern_packet(Pattern::kScripted,
                                                            wledimuudp::host::kScriptCycleFrames));
}

void test_pattern_packet_uses_canonical_encoder() {
  const auto frame = wledimuudp::host::make_pattern_frame(Pattern::kTwoBand, 6U);
  const AudioSyncV2Packet expected = wledimuudp::protocol::encode_audio_sync_v2(frame);
  const AudioSyncV2Packet actual = wledimuudp::host::make_pattern_packet(Pattern::kTwoBand, 6U);
  assert_packet_equal(expected, actual);
}

void test_send_cli_defaults_and_overrides() {
  const char *defaults[] = {"host", "send"};
  const auto default_result = wledimuudp::host::parse_host_options(2, defaults);
  TEST_ASSERT_TRUE(default_result.ok);
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(HostCommand::kSend),
                          static_cast<std::uint8_t>(default_result.options.command));
  TEST_ASSERT_EQUAL_UINT16(11988U, default_result.options.port);
  TEST_ASSERT_EQUAL_UINT16(50U, default_result.options.rate_hz);
  TEST_ASSERT_EQUAL_UINT32(wledimuudp::host::kScriptCycleFrames, default_result.options.frames);
  TEST_ASSERT_EQUAL_STRING("239.0.0.1",
                           wledimuudp::host::format_ipv4(default_result.options.address).c_str());

  const char *overrides[] = {"host",      "send",   "--pattern", "single-band", "--address",
                             "127.0.0.1", "--port", "12000",     "--rate",      "25",
                             "--frames",  "7",      "--dry-run"};
  const auto override_result = wledimuudp::host::parse_host_options(13, overrides);
  TEST_ASSERT_TRUE(override_result.ok);
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(Pattern::kSingleBand),
                          static_cast<std::uint8_t>(override_result.options.pattern));
  TEST_ASSERT_EQUAL_UINT16(12000U, override_result.options.port);
  TEST_ASSERT_EQUAL_UINT16(25U, override_result.options.rate_hz);
  TEST_ASSERT_EQUAL_UINT32(7U, override_result.options.frames);
  TEST_ASSERT_TRUE(override_result.options.dry_run);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1",
                           wledimuudp::host::format_ipv4(override_result.options.address).c_str());
}

void test_cli_rejects_invalid_arguments() {
  const char *bad_rate[] = {"host", "send", "--rate", "51"};
  TEST_ASSERT_FALSE(wledimuudp::host::parse_host_options(4, bad_rate).ok);

  const char *bad_port[] = {"host", "send", "--port", "0"};
  TEST_ASSERT_FALSE(wledimuudp::host::parse_host_options(4, bad_port).ok);

  const char *bad_address[] = {"host", "send", "--address", "999.0.0.1"};
  TEST_ASSERT_FALSE(wledimuudp::host::parse_host_options(4, bad_address).ok);

  const char *bad_pattern[] = {"host", "send", "--pattern", "random"};
  TEST_ASSERT_FALSE(wledimuudp::host::parse_host_options(4, bad_pattern).ok);

  const char *unknown[] = {"host", "send", "--wat", "1"};
  TEST_ASSERT_FALSE(wledimuudp::host::parse_host_options(4, unknown).ok);
}

void test_listen_and_decode_cli_contracts() {
  const char *listen[] = {"host",  "listen",  "--group", "239.1.2.3",    "--port",
                          "12001", "--count", "4",       "--timeout-ms", "750"};
  const auto listen_result = wledimuudp::host::parse_host_options(10, listen);
  TEST_ASSERT_TRUE(listen_result.ok);
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(HostCommand::kListen),
                          static_cast<std::uint8_t>(listen_result.options.command));
  TEST_ASSERT_EQUAL_UINT16(12001U, listen_result.options.port);
  TEST_ASSERT_EQUAL_UINT32(4U, listen_result.options.count);
  TEST_ASSERT_EQUAL_UINT32(750U, listen_result.options.timeout_ms);
  TEST_ASSERT_EQUAL_STRING("239.1.2.3",
                           wledimuudp::host::format_ipv4(listen_result.options.address).c_str());

  const char *decode[] = {"host", "decode", "--hex", "00"};
  const auto decode_result = wledimuudp::host::parse_host_options(4, decode);
  TEST_ASSERT_TRUE(decode_result.ok);
  TEST_ASSERT_EQUAL_UINT8(static_cast<std::uint8_t>(DecodeInput::kHex),
                          static_cast<std::uint8_t>(decode_result.options.decode_input));

  const char *missing[] = {"host", "decode"};
  TEST_ASSERT_FALSE(wledimuudp::host::parse_host_options(2, missing).ok);

  const char *both[] = {"host", "decode", "--hex", "00", "--file", "packet.bin"};
  TEST_ASSERT_FALSE(wledimuudp::host::parse_host_options(6, both).ok);
}

void test_hex_round_trip_and_canonical_malformed_rejection() {
  const AudioSyncV2Packet packet =
      wledimuudp::host::make_pattern_packet(Pattern::kMajorPeakSweep, 8U);
  const std::string hex = wledimuudp::host::packet_to_hex(packet);
  TEST_ASSERT_EQUAL_UINT32(88U, hex.size());

  AudioSyncV2Packet parsed{};
  std::string error;
  TEST_ASSERT_TRUE(wledimuudp::host::packet_from_hex(hex, parsed, &error));
  assert_packet_equal(packet, parsed);
  TEST_ASSERT_TRUE(wledimuudp::protocol::decode_audio_sync_v2(parsed).ok());

  parsed[0] = 0x31U;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<std::uint8_t>(DecodeError::kWrongHeader),
      static_cast<std::uint8_t>(wledimuudp::protocol::decode_audio_sync_v2(parsed).error));

  TEST_ASSERT_FALSE(wledimuudp::host::packet_from_hex("0011", parsed, &error));
}

void test_udp_loopback_preserves_exact_canonical_packet() {
  UdpSocket receiver;
  UdpSocket sender;
  TEST_ASSERT_TRUE(receiver.valid());
  TEST_ASSERT_TRUE(sender.valid());
  TEST_ASSERT_TRUE(receiver.bind_any(0U));
  TEST_ASSERT_TRUE(receiver.set_receive_timeout(1000U));
  const std::uint16_t port = receiver.local_port();
  TEST_ASSERT_TRUE(port != 0U);

  const std::array<std::uint8_t, 4> loopback{127U, 0U, 0U, 1U};
  const AudioSyncV2Packet sent = wledimuudp::host::make_pattern_packet(Pattern::kSingleBand, 3U);
  TEST_ASSERT_TRUE(sender.send_to(loopback, port, sent.data(), sent.size()));

  std::array<std::uint8_t, 64> buffer{};
  std::array<std::uint8_t, 4> source{};
  std::uint16_t source_port = 0U;
  const int received = receiver.receive(buffer.data(), buffer.size(), source, source_port);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(sent.size()), received);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(sent.data(), buffer.data(), sent.size());
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", wledimuudp::host::format_ipv4(source).c_str());
  TEST_ASSERT_TRUE(source_port != 0U);
}

} // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all_patterns_are_deterministic);
  RUN_TEST(test_constant_patterns_have_ordered_levels);
  RUN_TEST(test_motionless_patterns_expose_expected_signatures);
  RUN_TEST(test_scripted_sequence_has_locked_order_and_cycle);
  RUN_TEST(test_pattern_packet_uses_canonical_encoder);
  RUN_TEST(test_send_cli_defaults_and_overrides);
  RUN_TEST(test_cli_rejects_invalid_arguments);
  RUN_TEST(test_listen_and_decode_cli_contracts);
  RUN_TEST(test_hex_round_trip_and_canonical_malformed_rejection);
  RUN_TEST(test_udp_loopback_preserves_exact_canonical_packet);
  return UNITY_END();
}
