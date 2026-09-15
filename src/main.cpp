#include <Arduino.h>

#include <wledimuudp/audio_sync_v2.hpp>

using wledimuudp::protocol::encode_audio_sync_v2;
using wledimuudp::protocol::kAudioSyncV2PacketSize;
using wledimuudp::protocol::SyntheticAudioFrame;

void setup() {
  Serial.begin(115200);
  delay(50);

  SyntheticAudioFrame frame{};
  frame.sample_raw = 16.0F;
  frame.sample_smoothed = 8.0F;
  frame.sample_peak = true;
  frame.bands[0] = 32U;
  frame.bands[8] = 64U;
  frame.magnitude = 24.0F;
  frame.major_peak = 440.0F;

  const auto packet = encode_audio_sync_v2(frame);
  static_assert(kAudioSyncV2PacketSize == 44U, "Unexpected Audio Sync V2 size");

  Serial.print("WLEDIMUUDP WU-001 protocol smoke packet bytes=");
  Serial.println(static_cast<unsigned int>(packet.size()));
  Serial.println("No Wi-Fi, IMU, or LED path is enabled in WU-001.");
}

void loop() {
  delay(1000);
}
