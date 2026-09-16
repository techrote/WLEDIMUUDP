#include <Arduino.h>

#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/motion_features.hpp>
#include <wledimuudp/synthetic_audio_mapper.hpp>

using wledimuudp::mapping::SyntheticAudioMapper;
using wledimuudp::motion::ImuSample;
using wledimuudp::motion::MotionFeatureExtractor;
using wledimuudp::protocol::encode_audio_sync_v2;
using wledimuudp::protocol::kAudioSyncV2PacketSize;

void setup() {
  Serial.begin(115200);
  delay(50);

  MotionFeatureExtractor motion;
  SyntheticAudioMapper mapper;
  const ImuSample stationary{5000U, {0.0F, 0.0F, 1.0F}, {}, true};
  const auto features = motion.process(stationary);
  const auto frame = mapper.map(features);
  const auto packet = encode_audio_sync_v2(frame);
  static_assert(kAudioSyncV2PacketSize == 44U, "Unexpected Audio Sync V2 size");

  Serial.print("WLEDIMUUDP mapper smoke packet bytes=");
  Serial.println(static_cast<unsigned int>(packet.size()));
  Serial.print("WU-004 mapper smoke valid=");
  Serial.println(features.input_valid ? "yes" : "no");
  Serial.print("Balanced profile version=");
  Serial.println(static_cast<unsigned int>(wledimuudp::mapping::kBalancedProfileVersion));
  Serial.println("No Wi-Fi, live IMU adapter, or LED path is enabled yet.");
}

void loop() {
  delay(1000);
}
