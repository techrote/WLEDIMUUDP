#pragma once

#include <array>
#include <cstdint>

#include <wledimuudp/audio_sync_v2.hpp>
#include <wledimuudp/motion_features.hpp>

namespace wledimuudp::mapping {

inline constexpr std::uint16_t kBalancedProfileVersion = 1U;

inline constexpr std::array<float, 16> kBalancedBandCentersHz{
    65.0F,   92.0F,   131.0F,  185.0F,  262.0F,  370.0F,  523.0F,  740.0F,
    1047.0F, 1480.0F, 2093.0F, 2960.0F, 4186.0F, 5920.0F, 8372.0F, 11840.0F,
};

enum class MappingProfile : std::uint8_t {
  kBalancedV1 = 1,
};

struct MappingConfig {
  MappingProfile profile{MappingProfile::kBalancedV1};
  float level_gain{72.0F};
  float translation_full_scale_g{0.22F};
  float rotation_full_scale_dps{120.0F};
  float shake_full_scale_g_per_s{18.0F};
  float translation_band_gain{1.00F};
  float rotation_band_gain{0.95F};
  float shake_band_gain{0.90F};
  float impact_band_gain{1.00F};
  float shake_to_impact_crossfeed{0.20F};
  float orientation_bias_strength{0.65F};
  float quiet_major_peak_hz{1.0F};
};

class SyntheticAudioMapper {
public:
  explicit SyntheticAudioMapper(MappingConfig config = {});

  void reset();
  const MappingConfig &config() const;
  protocol::SyntheticAudioFrame map(const motion::MotionFeatures &features);

private:
  MappingConfig config_{};
  protocol::SyntheticAudioFrame last_frame_{};
  bool previous_impact_{false};
};

float spectral_magnitude(const protocol::SyntheticAudioFrame &frame) noexcept;
float spectral_major_peak(const protocol::SyntheticAudioFrame &frame, float quiet_value_hz) noexcept;

} // namespace wledimuudp::mapping
