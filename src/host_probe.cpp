#include <wledimuudp/host_probe.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

using wledimuudp::host::DecodeInput;
using wledimuudp::host::HostCommand;
using wledimuudp::host::HostOptions;
using wledimuudp::host::UdpSocket;
using wledimuudp::protocol::AudioSyncV2Packet;
using wledimuudp::protocol::DecodeError;
using wledimuudp::protocol::DecodeResult;

void print_usage(std::ostream &output) {
  output
      << "WLEDIMUUDP host probe\n\n"
      << "Usage:\n"
      << "  wledimuudp-host send [--pattern NAME] [--address A.B.C.D] [--port N]\n"
      << "                        [--rate 1..50] [--frames N] [--dry-run]\n"
      << "  wledimuudp-host listen [--group A.B.C.D] [--port N] [--count N]\n"
      << "                          [--timeout-ms N]\n"
      << "  wledimuudp-host decode (--hex HEX | --file PATH)\n\n"
      << "Patterns:\n"
      << "  silence, low, medium, high, ramp, single-band, two-band,\n"
      << "  broadband-pulse, peak-pulse, major-peak-sweep, scripted\n\n"
      << "Defaults: multicast 239.0.0.1:11988, 50 Hz, scripted 160-frame cycle.\n";
}

const char *decode_error_name(DecodeError error) noexcept {
  switch (error) {
  case DecodeError::kNone:
    return "none";
  case DecodeError::kNullData:
    return "null-data";
  case DecodeError::kWrongSize:
    return "wrong-size";
  case DecodeError::kWrongHeader:
    return "wrong-header";
  case DecodeError::kNonZeroReserved:
    return "non-zero-reserved";
  case DecodeError::kNonFiniteFloat:
    return "non-finite-float";
  case DecodeError::kInvalidMajorPeak:
    return "invalid-major-peak";
  }
  return "unknown";
}

void print_semantic_frame(std::ostream &output, const DecodeResult &decoded) {
  if (!decoded.ok()) {
    output << "decode_error=" << decode_error_name(decoded.error);
    return;
  }

  const auto &frame = decoded.frame;
  output << std::fixed << std::setprecision(2) << "raw=" << frame.sample_raw
         << " smooth=" << frame.sample_smoothed << " peak=" << (frame.sample_peak ? 1 : 0)
         << " magnitude=" << frame.magnitude << " major_peak=" << frame.major_peak
         << " bands=[";
  for (std::size_t index = 0; index < frame.bands.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << frame.bands[index];
  }
  output << ']';
}

int decode_and_print(std::size_t index, const std::uint8_t *data, std::size_t size,
                     bool show_hex) {
  const DecodeResult decoded = wledimuudp::protocol::decode_audio_sync_v2(data, size);
  std::cout << "packet=" << index << ' ';
  if (show_hex && size == wledimuudp::protocol::kAudioSyncV2PacketSize) {
    AudioSyncV2Packet packet{};
    std::copy_n(data, packet.size(), packet.begin());
    std::cout << "hex=" << wledimuudp::host::packet_to_hex(packet) << ' ';
  }
  print_semantic_frame(std::cout, decoded);
  std::cout << '\n';
  return decoded.ok() ? 0 : 2;
}

int run_send(const HostOptions &options) {
  UdpSocket socket;
  if (!options.dry_run && !socket.valid()) {
    std::cerr << "socket creation failed, error=" << socket.last_error() << '\n';
    return 2;
  }

  const auto period = std::chrono::microseconds(1000000U / options.rate_hz);
  auto next_send = std::chrono::steady_clock::now();
  std::uint32_t sent = 0;

  for (std::uint32_t index = 0; index < options.frames; ++index) {
    const AudioSyncV2Packet packet = wledimuudp::host::make_pattern_packet(options.pattern, index);
    if (options.dry_run) {
      const DecodeResult decoded = wledimuudp::protocol::decode_audio_sync_v2(packet);
      std::cout << "frame=" << index << " pattern=" << wledimuudp::host::pattern_name(options.pattern)
                << " hex=" << wledimuudp::host::packet_to_hex(packet) << ' ';
      print_semantic_frame(std::cout, decoded);
      std::cout << '\n';
    } else {
      if (!socket.send_to(options.address, options.port, packet.data(), packet.size())) {
        std::cerr << "send failed at frame " << index << ", error=" << socket.last_error() << '\n';
        return 3;
      }
      ++sent;
      if (index == 0U || ((index + 1U) % options.rate_hz) == 0U || index + 1U == options.frames) {
        const auto frame = wledimuudp::host::make_pattern_frame(options.pattern, index);
        std::cout << "sent=" << sent << '/' << options.frames
                  << " destination=" << wledimuudp::host::format_ipv4(options.address) << ':'
                  << options.port << " pattern=" << wledimuudp::host::pattern_name(options.pattern)
                  << " raw=" << frame.sample_raw << " peak=" << (frame.sample_peak ? 1 : 0)
                  << " major_peak=" << frame.major_peak << '\n';
      }
    }

    if (!options.dry_run && index + 1U < options.frames) {
      next_send += period;
      std::this_thread::sleep_until(next_send);
    }
  }

  if (!options.dry_run) {
    std::cout << "send complete: " << sent << " packet(s) at " << options.rate_hz << " Hz\n";
  }
  return 0;
}

int run_listen(const HostOptions &options) {
  UdpSocket socket;
  if (!socket.valid()) {
    std::cerr << "socket creation failed, error=" << socket.last_error() << '\n';
    return 2;
  }
  if (!socket.bind_any(options.port)) {
    std::cerr << "bind failed on port " << options.port << ", error=" << socket.last_error() << '\n';
    return 2;
  }
  if (!socket.join_multicast(options.address)) {
    std::cerr << "multicast join failed for " << wledimuudp::host::format_ipv4(options.address)
              << ", error=" << socket.last_error() << '\n';
    return 2;
  }
  if (!socket.set_receive_timeout(options.timeout_ms)) {
    std::cerr << "receive timeout configuration failed, error=" << socket.last_error() << '\n';
    return 2;
  }

  std::cout << "listening on " << wledimuudp::host::format_ipv4(options.address) << ':' << options.port
            << " count=" << options.count << " timeout_ms=" << options.timeout_ms << '\n';

  std::array<std::uint8_t, 2048> buffer{};
  const auto start = std::chrono::steady_clock::now();
  auto previous = start;
  bool have_previous = false;
  std::uint32_t valid = 0;
  std::uint32_t malformed = 0;

  for (std::uint32_t index = 0; index < options.count; ++index) {
    std::array<std::uint8_t, 4> source{};
    std::uint16_t source_port = 0;
    const int received = socket.receive(buffer.data(), buffer.size(), source, source_port);
    if (received < 0) {
      std::cerr << "receive stopped after " << index << " packet(s), error=" << socket.last_error()
                << '\n';
      return 3;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
    const auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - previous).count();
    const DecodeResult decoded = wledimuudp::protocol::decode_audio_sync_v2(
        buffer.data(), static_cast<std::size_t>(received));

    std::cout << "packet=" << index << " t_ms=" << (static_cast<double>(elapsed) / 1000.0);
    if (have_previous) {
      std::cout << " delta_ms=" << (static_cast<double>(delta) / 1000.0);
    }
    std::cout << " from=" << wledimuudp::host::format_ipv4(source) << ':' << source_port
              << " bytes=" << received << ' ';
    print_semantic_frame(std::cout, decoded);
    std::cout << '\n';

    if (decoded.ok()) {
      ++valid;
    } else {
      ++malformed;
    }
    previous = now;
    have_previous = true;
  }

  std::cout << "listen complete: valid=" << valid << " malformed=" << malformed << '\n';
  return malformed == 0U ? 0 : 2;
}

int run_decode(const HostOptions &options) {
  if (options.decode_input == DecodeInput::kHex) {
    AudioSyncV2Packet packet{};
    std::string error;
    if (!wledimuudp::host::packet_from_hex(options.input, packet, &error)) {
      std::cerr << "decode input error: " << error << '\n';
      return 2;
    }
    return decode_and_print(0U, packet.data(), packet.size(), true);
  }

  std::ifstream input(options.input, std::ios::binary);
  if (!input) {
    std::cerr << "cannot open packet file: " << options.input << '\n';
    return 2;
  }
  const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),
                                        std::istreambuf_iterator<char>()};
  if (bytes.empty() || (bytes.size() % wledimuudp::protocol::kAudioSyncV2PacketSize) != 0U) {
    std::cerr << "packet file must contain one or more complete 44-byte packets\n";
    return 2;
  }

  int status = 0;
  const std::size_t count = bytes.size() / wledimuudp::protocol::kAudioSyncV2PacketSize;
  for (std::size_t index = 0; index < count; ++index) {
    const std::uint8_t *packet =
        bytes.data() + index * wledimuudp::protocol::kAudioSyncV2PacketSize;
    status = std::max(status, decode_and_print(index, packet,
                                               wledimuudp::protocol::kAudioSyncV2PacketSize, true));
  }
  std::cout << "decoded " << count << " packet(s)\n";
  return status;
}

} // namespace

int main(int argc, const char *argv[]) {
  const auto parsed = wledimuudp::host::parse_host_options(argc, argv);
  if (!parsed.ok) {
    std::cerr << "error: " << parsed.error << "\n\n";
    print_usage(std::cerr);
    return 2;
  }

  switch (parsed.options.command) {
  case HostCommand::kHelp:
    print_usage(std::cout);
    return 0;
  case HostCommand::kSend:
    return run_send(parsed.options);
  case HostCommand::kListen:
    return run_listen(parsed.options);
  case HostCommand::kDecode:
    return run_decode(parsed.options);
  }
  return 2;
}
