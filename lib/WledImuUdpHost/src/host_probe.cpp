#include <wledimuudp/host_probe.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace wledimuudp::host {
namespace {

constexpr std::uintptr_t kInvalidSocketHandle = std::numeric_limits<std::uintptr_t>::max();
constexpr std::array<Pattern, 10> kScriptPatterns{
    Pattern::kSilence,   Pattern::kLow,
    Pattern::kMedium,    Pattern::kHigh,
    Pattern::kLevelRamp, Pattern::kSingleBand,
    Pattern::kTwoBand,   Pattern::kBroadbandPulse,
    Pattern::kPeakPulse, Pattern::kMajorPeakSweep,
};

protocol::SyntheticAudioFrame make_constant_frame(float level, std::uint16_t band,
                                                  float major_peak) noexcept {
  protocol::SyntheticAudioFrame frame{};
  frame.sample_raw = level;
  frame.sample_smoothed = level;
  frame.magnitude = level;
  frame.major_peak = major_peak;
  frame.bands.fill(band);
  return frame;
}

bool parse_u32(std::string_view text, std::uint32_t minimum, std::uint32_t maximum,
               std::uint32_t &value) noexcept {
  if (text.empty()) {
    return false;
  }
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed < minimum ||
      parsed > maximum) {
    return false;
  }
  value = parsed;
  return true;
}

ParseResult parse_failure(std::string message) {
  ParseResult result{};
  result.error = std::move(message);
  return result;
}

std::uint8_t hex_nibble(char value) noexcept {
  if (value >= '0' && value <= '9') {
    return static_cast<std::uint8_t>(value - '0');
  }
  if (value >= 'a' && value <= 'f') {
    return static_cast<std::uint8_t>(10 + value - 'a');
  }
  return static_cast<std::uint8_t>(10 + value - 'A');
}

std::uint32_t ipv4_host_value(const std::array<std::uint8_t, 4> &address) noexcept {
  return (static_cast<std::uint32_t>(address[0]) << 24U) |
         (static_cast<std::uint32_t>(address[1]) << 16U) |
         (static_cast<std::uint32_t>(address[2]) << 8U) | static_cast<std::uint32_t>(address[3]);
}

sockaddr_in make_sockaddr(const std::array<std::uint8_t, 4> &address, std::uint16_t port) noexcept {
  sockaddr_in target{};
  target.sin_family = AF_INET;
  target.sin_port = htons(port);
  target.sin_addr.s_addr = htonl(ipv4_host_value(address));
  return target;
}

std::array<std::uint8_t, 4> unpack_ipv4(std::uint32_t network_value) noexcept {
  const std::uint32_t host_value = ntohl(network_value);
  return {static_cast<std::uint8_t>((host_value >> 24U) & 0xFFU),
          static_cast<std::uint8_t>((host_value >> 16U) & 0xFFU),
          static_cast<std::uint8_t>((host_value >> 8U) & 0xFFU),
          static_cast<std::uint8_t>(host_value & 0xFFU)};
}

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidNativeSocket = INVALID_SOCKET;

bool socket_runtime_ready() noexcept {
  static const bool ready = [] {
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  return ready;
}

int socket_last_error() noexcept {
  return WSAGetLastError();
}

void close_native_socket(NativeSocket socket) noexcept {
  closesocket(socket);
}

NativeSocket native_socket(std::uintptr_t handle) noexcept {
  return static_cast<NativeSocket>(handle);
}
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidNativeSocket = -1;

bool socket_runtime_ready() noexcept {
  return true;
}

int socket_last_error() noexcept {
  return errno;
}

void close_native_socket(NativeSocket socket) noexcept {
  close(socket);
}

NativeSocket native_socket(std::uintptr_t handle) noexcept {
  return static_cast<NativeSocket>(handle);
}
#endif

} // namespace

const char *pattern_name(Pattern pattern) noexcept {
  switch (pattern) {
  case Pattern::kSilence:
    return "silence";
  case Pattern::kLow:
    return "low";
  case Pattern::kMedium:
    return "medium";
  case Pattern::kHigh:
    return "high";
  case Pattern::kLevelRamp:
    return "ramp";
  case Pattern::kSingleBand:
    return "single-band";
  case Pattern::kTwoBand:
    return "two-band";
  case Pattern::kBroadbandPulse:
    return "broadband-pulse";
  case Pattern::kPeakPulse:
    return "peak-pulse";
  case Pattern::kMajorPeakSweep:
    return "major-peak-sweep";
  case Pattern::kScripted:
    return "scripted";
  }
  return "unknown";
}

bool parse_pattern(std::string_view text, Pattern &pattern) noexcept {
  for (const Pattern candidate : kScriptPatterns) {
    if (text == pattern_name(candidate)) {
      pattern = candidate;
      return true;
    }
  }
  if (text == pattern_name(Pattern::kScripted)) {
    pattern = Pattern::kScripted;
    return true;
  }
  return false;
}

protocol::SyntheticAudioFrame make_pattern_frame(Pattern pattern, std::uint32_t index) noexcept {
  if (pattern == Pattern::kScripted) {
    const std::uint32_t cycle_index = index % kScriptCycleFrames;
    const std::size_t segment = static_cast<std::size_t>(cycle_index / kScriptSegmentFrames);
    const std::uint32_t local_index = cycle_index % kScriptSegmentFrames;
    return make_pattern_frame(kScriptPatterns[segment], local_index);
  }

  switch (pattern) {
  case Pattern::kSilence:
    return {};
  case Pattern::kLow:
    return make_constant_frame(32.0F, 24U, 120.0F);
  case Pattern::kMedium:
    return make_constant_frame(96.0F, 72U, 440.0F);
  case Pattern::kHigh:
    return make_constant_frame(192.0F, 160U, 1000.0F);
  case Pattern::kLevelRamp: {
    protocol::SyntheticAudioFrame frame{};
    const std::uint16_t level = static_cast<std::uint16_t>((index % 64U) * 4U);
    frame.sample_raw = static_cast<float>(level);
    frame.sample_smoothed = static_cast<float>(level) * 0.75F;
    frame.magnitude = static_cast<float>(level);
    frame.major_peak = 80.0F + static_cast<float>(index % 64U) * 40.0F;
    for (std::size_t band = 0; band < frame.bands.size(); ++band) {
      frame.bands[band] = static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(level) * static_cast<std::uint32_t>(band + 4U)) / 19U);
    }
    return frame;
  }
  case Pattern::kSingleBand: {
    protocol::SyntheticAudioFrame frame{};
    const std::size_t active = static_cast<std::size_t>(index % 16U);
    frame.sample_raw = 110.0F;
    frame.sample_smoothed = 96.0F;
    frame.magnitude = 220.0F;
    frame.major_peak = 80.0F + static_cast<float>(active) * 600.0F;
    frame.bands[active] = 220U;
    return frame;
  }
  case Pattern::kTwoBand: {
    protocol::SyntheticAudioFrame frame{};
    const std::size_t first = static_cast<std::size_t>(index % 8U);
    const std::size_t second = 15U - first;
    frame.sample_raw = 140.0F;
    frame.sample_smoothed = 120.0F;
    frame.magnitude = 220.0F;
    frame.major_peak = 500.0F + static_cast<float>(first) * 350.0F;
    frame.bands[first] = 180U;
    frame.bands[second] = 220U;
    return frame;
  }
  case Pattern::kBroadbandPulse: {
    const bool pulse = (index % 16U) < 4U;
    if (!pulse) {
      return {};
    }
    protocol::SyntheticAudioFrame frame = make_constant_frame(220.0F, 230U, 600.0F);
    frame.sample_smoothed = 140.0F;
    frame.magnitude = 240.0F;
    return frame;
  }
  case Pattern::kPeakPulse: {
    protocol::SyntheticAudioFrame frame = make_constant_frame(16.0F, 8U, 120.0F);
    if ((index % 16U) == 0U) {
      frame.sample_raw = 240.0F;
      frame.sample_smoothed = 80.0F;
      frame.sample_peak = true;
      frame.magnitude = 254.0F;
      frame.major_peak = 4000.0F;
      frame.bands[13] = 254U;
    }
    return frame;
  }
  case Pattern::kMajorPeakSweep: {
    protocol::SyntheticAudioFrame frame{};
    const std::size_t active = static_cast<std::size_t>(index % 16U);
    frame.sample_raw = 96.0F;
    frame.sample_smoothed = 96.0F;
    frame.magnitude = 160.0F;
    frame.major_peak = 80.0F + static_cast<float>(active) * 600.0F;
    frame.bands[active] = 220U;
    if (active > 0U) {
      frame.bands[active - 1U] = 100U;
    }
    if (active + 1U < frame.bands.size()) {
      frame.bands[active + 1U] = 100U;
    }
    return frame;
  }
  case Pattern::kScripted:
    break;
  }
  return {};
}

protocol::AudioSyncV2Packet make_pattern_packet(Pattern pattern, std::uint32_t index) noexcept {
  return protocol::encode_audio_sync_v2(make_pattern_frame(pattern, index));
}

bool parse_ipv4(std::string_view text, std::array<std::uint8_t, 4> &address) noexcept {
  std::array<std::uint8_t, 4> parsed{};
  std::size_t start = 0;
  for (std::size_t index = 0; index < parsed.size(); ++index) {
    const std::size_t dot = text.find('.', start);
    const bool last = index + 1U == parsed.size();
    if ((last && dot != std::string_view::npos) || (!last && dot == std::string_view::npos)) {
      return false;
    }
    const std::size_t end = last ? text.size() : dot;
    std::uint32_t octet = 0;
    if (!parse_u32(text.substr(start, end - start), 0U, 255U, octet)) {
      return false;
    }
    parsed[index] = static_cast<std::uint8_t>(octet);
    start = end + 1U;
  }
  address = parsed;
  return true;
}

std::string format_ipv4(const std::array<std::uint8_t, 4> &address) {
  std::ostringstream stream;
  stream << static_cast<unsigned>(address[0]) << '.' << static_cast<unsigned>(address[1]) << '.'
         << static_cast<unsigned>(address[2]) << '.' << static_cast<unsigned>(address[3]);
  return stream.str();
}

bool is_multicast(const std::array<std::uint8_t, 4> &address) noexcept {
  return address[0] >= 224U && address[0] <= 239U;
}

std::string packet_to_hex(const protocol::AudioSyncV2Packet &packet) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (const std::uint8_t value : packet) {
    stream << std::setw(2) << static_cast<unsigned>(value);
  }
  return stream.str();
}

bool packet_from_hex(std::string_view text, protocol::AudioSyncV2Packet &packet,
                     std::string *error) {
  std::string compact;
  compact.reserve(text.size());
  for (const char value : text) {
    const unsigned char byte = static_cast<unsigned char>(value);
    if (std::isxdigit(byte) != 0) {
      compact.push_back(value);
    } else if (std::isspace(byte) == 0 && value != ':' && value != '-') {
      if (error != nullptr) {
        *error = "hex input contains a non-hex character";
      }
      return false;
    }
  }

  if (compact.size() != protocol::kAudioSyncV2PacketSize * 2U) {
    if (error != nullptr) {
      *error = "hex input must contain exactly 44 bytes";
    }
    return false;
  }

  for (std::size_t index = 0; index < packet.size(); ++index) {
    const char high = compact[index * 2U];
    const char low = compact[index * 2U + 1U];
    packet[index] = static_cast<std::uint8_t>((hex_nibble(high) << 4U) | hex_nibble(low));
  }
  return true;
}

ParseResult parse_host_options(int argc, const char *const argv[]) {
  ParseResult result{};
  result.ok = true;
  if (argc <= 1) {
    return result;
  }

  const std::string_view command(argv[1]);
  if (command == "help" || command == "--help" || command == "-h") {
    return result;
  }
  if (command == "send") {
    result.options.command = HostCommand::kSend;
  } else if (command == "listen") {
    result.options.command = HostCommand::kListen;
  } else if (command == "decode") {
    result.options.command = HostCommand::kDecode;
  } else {
    return parse_failure("unknown command: " + std::string(command));
  }

  for (int index = 2; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--help" || argument == "-h") {
      result.options.command = HostCommand::kHelp;
      return result;
    }
    if (argument == "--dry-run" && result.options.command == HostCommand::kSend) {
      result.options.dry_run = true;
      continue;
    }
    if (index + 1 >= argc) {
      return parse_failure("missing value for " + std::string(argument));
    }
    const std::string_view value(argv[++index]);

    if (result.options.command == HostCommand::kSend && argument == "--pattern") {
      if (!parse_pattern(value, result.options.pattern)) {
        return parse_failure("unknown pattern: " + std::string(value));
      }
      continue;
    }
    if (result.options.command == HostCommand::kSend && argument == "--address") {
      if (!parse_ipv4(value, result.options.address)) {
        return parse_failure("invalid IPv4 address: " + std::string(value));
      }
      continue;
    }
    if (result.options.command == HostCommand::kListen && argument == "--group") {
      if (!parse_ipv4(value, result.options.address) || !is_multicast(result.options.address)) {
        return parse_failure("invalid multicast group: " + std::string(value));
      }
      continue;
    }
    if ((result.options.command == HostCommand::kSend ||
         result.options.command == HostCommand::kListen) &&
        argument == "--port") {
      std::uint32_t port = 0;
      if (!parse_u32(value, 1U, 65535U, port)) {
        return parse_failure("port must be in 1..65535");
      }
      result.options.port = static_cast<std::uint16_t>(port);
      continue;
    }
    if (result.options.command == HostCommand::kSend && argument == "--rate") {
      std::uint32_t rate = 0;
      if (!parse_u32(value, 1U, kMaxProbeRateHz, rate)) {
        return parse_failure("rate must be in 1..50 Hz");
      }
      result.options.rate_hz = static_cast<std::uint16_t>(rate);
      continue;
    }
    if (result.options.command == HostCommand::kSend && argument == "--frames") {
      if (!parse_u32(value, 1U, 1000000U, result.options.frames)) {
        return parse_failure("frames must be in 1..1000000");
      }
      continue;
    }
    if (result.options.command == HostCommand::kListen && argument == "--count") {
      if (!parse_u32(value, 1U, 1000000U, result.options.count)) {
        return parse_failure("count must be in 1..1000000");
      }
      continue;
    }
    if (result.options.command == HostCommand::kListen && argument == "--timeout-ms") {
      if (!parse_u32(value, 1U, 60000U, result.options.timeout_ms)) {
        return parse_failure("timeout-ms must be in 1..60000");
      }
      continue;
    }
    if (result.options.command == HostCommand::kDecode &&
        (argument == "--hex" || argument == "--file")) {
      if (result.options.decode_input != DecodeInput::kNone) {
        return parse_failure("decode accepts exactly one of --hex or --file");
      }
      result.options.decode_input = argument == "--hex" ? DecodeInput::kHex : DecodeInput::kFile;
      result.options.input = std::string(value);
      continue;
    }
    return parse_failure("unknown option for command: " + std::string(argument));
  }

  if (result.options.command == HostCommand::kDecode &&
      result.options.decode_input == DecodeInput::kNone) {
    return parse_failure("decode requires --hex or --file");
  }
  return result;
}

UdpSocket::UdpSocket() noexcept : handle_(kInvalidSocketHandle) {
  if (!socket_runtime_ready()) {
    last_error_ = -1;
    return;
  }
  const NativeSocket socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket == kInvalidNativeSocket) {
    last_error_ = socket_last_error();
    return;
  }
  handle_ = static_cast<std::uintptr_t>(socket);

  const int reuse = 1;
  static_cast<void>(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<const char *>(&reuse), sizeof(reuse)));
  const int ttl = 1;
  static_cast<void>(setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL,
                               reinterpret_cast<const char *>(&ttl), sizeof(ttl)));
}

UdpSocket::~UdpSocket() {
  if (valid()) {
    close_native_socket(native_socket(handle_));
  }
}

bool UdpSocket::valid() const noexcept {
  return handle_ != kInvalidSocketHandle;
}

int UdpSocket::last_error() const noexcept {
  return last_error_;
}

bool UdpSocket::set_receive_timeout(std::uint32_t timeout_ms) noexcept {
  if (!valid()) {
    return false;
  }
#ifdef _WIN32
  const DWORD timeout = timeout_ms;
  const int result = setsockopt(native_socket(handle_), SOL_SOCKET, SO_RCVTIMEO,
                                reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
  timeval timeout{};
  timeout.tv_sec = static_cast<time_t>(timeout_ms / 1000U);
  timeout.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000U) * 1000U);
  const int result =
      setsockopt(native_socket(handle_), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
  if (result != 0) {
    last_error_ = socket_last_error();
    return false;
  }
  return true;
}

bool UdpSocket::bind_any(std::uint16_t port) noexcept {
  if (!valid()) {
    return false;
  }
  sockaddr_in local{};
  local.sin_family = AF_INET;
  local.sin_port = htons(port);
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  const int result =
      bind(native_socket(handle_), reinterpret_cast<const sockaddr *>(&local), sizeof(local));
  if (result != 0) {
    last_error_ = socket_last_error();
    return false;
  }
  return true;
}

std::uint16_t UdpSocket::local_port() noexcept {
  if (!valid()) {
    return 0U;
  }
  sockaddr_in local{};
#ifdef _WIN32
  int length = sizeof(local);
#else
  socklen_t length = sizeof(local);
#endif
  if (getsockname(native_socket(handle_), reinterpret_cast<sockaddr *>(&local), &length) != 0) {
    last_error_ = socket_last_error();
    return 0U;
  }
  return ntohs(local.sin_port);
}

bool UdpSocket::join_multicast(const std::array<std::uint8_t, 4> &group) noexcept {
  if (!valid() || !is_multicast(group)) {
    return false;
  }
  ip_mreq membership{};
  membership.imr_multiaddr.s_addr = htonl(ipv4_host_value(group));
  membership.imr_interface.s_addr = htonl(INADDR_ANY);
  const int result = setsockopt(native_socket(handle_), IPPROTO_IP, IP_ADD_MEMBERSHIP,
                                reinterpret_cast<const char *>(&membership), sizeof(membership));
  if (result != 0) {
    last_error_ = socket_last_error();
    return false;
  }
  return true;
}

bool UdpSocket::send_to(const std::array<std::uint8_t, 4> &address, std::uint16_t port,
                        const std::uint8_t *data, std::size_t size) noexcept {
  if (!valid() || data == nullptr ||
      size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  const sockaddr_in target = make_sockaddr(address, port);
#ifdef _WIN32
  const int sent =
      sendto(native_socket(handle_), reinterpret_cast<const char *>(data), static_cast<int>(size),
             0, reinterpret_cast<const sockaddr *>(&target), sizeof(target));
#else
  const ssize_t sent = sendto(native_socket(handle_), data, size, 0,
                              reinterpret_cast<const sockaddr *>(&target), sizeof(target));
#endif
  if (sent < 0 || static_cast<std::size_t>(sent) != size) {
    last_error_ = socket_last_error();
    return false;
  }
  return true;
}

int UdpSocket::receive(std::uint8_t *data, std::size_t capacity,
                       std::array<std::uint8_t, 4> &source_address,
                       std::uint16_t &source_port) noexcept {
  if (!valid() || data == nullptr ||
      capacity > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return -1;
  }
  sockaddr_in source{};
#ifdef _WIN32
  int source_length = sizeof(source);
  const int received =
      recvfrom(native_socket(handle_), reinterpret_cast<char *>(data), static_cast<int>(capacity),
               0, reinterpret_cast<sockaddr *>(&source), &source_length);
#else
  socklen_t source_length = sizeof(source);
  const ssize_t received = recvfrom(native_socket(handle_), data, capacity, 0,
                                    reinterpret_cast<sockaddr *>(&source), &source_length);
#endif
  if (received < 0) {
    last_error_ = socket_last_error();
    return -1;
  }
  source_address = unpack_ipv4(source.sin_addr.s_addr);
  source_port = ntohs(source.sin_port);
  return static_cast<int>(received);
}

} // namespace wledimuudp::host
