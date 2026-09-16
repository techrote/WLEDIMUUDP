#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/motion_features.hpp>
#include <wledimuudp/synthetic_audio_mapper.hpp>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using wledimuudp::mapping::SyntheticAudioMapper;
using wledimuudp::motion::MotionFeatures;
using wledimuudp::protocol::AudioSyncV2Packet;
using wledimuudp::protocol::SyntheticAudioFrame;

void print_usage() {
  std::cout << "Usage: wledimuudp-mapper <still|sway|spin|shake|impact|tilt-left|tilt-right>\n";
}

MotionFeatures make_preset(const std::string_view name, bool &ok) {
  MotionFeatures features{};
  features.input_valid = true;
  features.orientation = {0.0F, 0.0F, 1.0F};
  ok = true;

  if (name == "still") {
    return features;
  }
  if (name == "sway") {
    features.linear_accel_magnitude_g = 0.16F;
    features.jerk_g_per_s = 1.4F;
    features.motion_energy_instant = 0.24F;
    features.motion_energy_smoothed = 0.18F;
    return features;
  }
  if (name == "spin") {
    features.angular_speed_dps = 120.0F;
    features.motion_energy_instant = 1.5F;
    features.motion_energy_smoothed = 1.1F;
    return features;
  }
  if (name == "shake") {
    features.linear_accel_magnitude_g = 0.30F;
    features.jerk_g_per_s = 22.0F;
    features.angular_speed_dps = 65.0F;
    features.motion_energy_instant = 2.2F;
    features.motion_energy_smoothed = 1.4F;
    return features;
  }
  if (name == "impact") {
    features.linear_accel_magnitude_g = 0.80F;
    features.jerk_g_per_s = 30.0F;
    features.motion_energy_instant = 3.0F;
    features.motion_energy_smoothed = 1.2F;
    features.impact = true;
    return features;
  }
  if (name == "tilt-left" || name == "tilt-right") {
    features.linear_accel_magnitude_g = 0.18F;
    features.motion_energy_instant = 0.5F;
    features.motion_energy_smoothed = 0.35F;
    features.orientation =
        name == "tilt-left" ? wledimuudp::motion::Vec3{-0.8F, 0.0F, 0.6F}
                            : wledimuudp::motion::Vec3{0.8F, 0.0F, 0.6F};
    return features;
  }

  ok = false;
  return {};
}

void print_frame(const SyntheticAudioFrame &frame) {
  std::cout << std::fixed << std::setprecision(2) << "raw=" << frame.sample_raw
            << " smooth=" << frame.sample_smoothed << " peak=" << (frame.sample_peak ? 1 : 0)
            << " magnitude=" << frame.magnitude << " major_peak=" << frame.major_peak << " bands=[";
  for (std::size_t index = 0; index < frame.bands.size(); ++index) {
    if (index != 0U) {
      std::cout << ',';
    }
    std::cout << frame.bands[index];
  }
  std::cout << "]\n";
}

void print_packet(const AudioSyncV2Packet &packet) {
  std::cout << "hex=";
  for (const auto byte : packet) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(byte);
  }
  std::cout << std::dec << '\n';
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    print_usage();
    return 2;
  }

  bool ok = false;
  const auto features = make_preset(argv[1], ok);
  if (!ok) {
    std::cerr << "unknown mapper preset: " << argv[1] << '\n';
    print_usage();
    return 2;
  }

  SyntheticAudioMapper mapper;
  const auto frame = mapper.map(features);
  const auto packet = wledimuudp::protocol::encode_audio_sync_v2(frame);
  std::cout << "profile=balanced-v" << wledimuudp::mapping::kBalancedProfileVersion
            << " preset=" << argv[1] << '\n';
  print_frame(frame);
  print_packet(packet);
  return 0;
}
