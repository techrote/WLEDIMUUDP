#include <wledimuudp/synthetic_audio_mapper.hpp>

#include <algorithm>
#include <cmath>

namespace wledimuudp::mapping {
namespace {

float clamp01(const float value) noexcept {
  if (!std::isfinite(value)) {
    return 0.0F;
  }
  return std::clamp(value, 0.0F, 1.0F);
}

float safe_positive(const float value) noexcept {
  return std::isfinite(value) && value > 0.0F ? value : 0.0F;
}

float safe_scale(const float value, const float full_scale) noexcept {
  if (!(full_scale > 0.0F) || !std::isfinite(full_scale)) {
    return 0.0F;
  }
  return clamp01(safe_positive(value) / full_scale);
}

std::array<float, 4> group_shape(const motion::Vec3 &orientation,
                                 const float bias_strength) noexcept {
  const float raw_bias = std::clamp(orientation.x * 0.75F + orientation.y * 0.25F, -1.0F, 1.0F);
  const float bias = raw_bias * std::clamp(bias_strength, 0.0F, 1.0F);
  const float position = 1.5F + bias * 1.5F;

  std::array<float, 4> weights{};
  float maximum = 0.0F;
  for (std::size_t index = 0; index < weights.size(); ++index) {
    const float distance = std::fabs(static_cast<float>(index) - position);
    weights[index] = std::max(0.0F, 1.0F - distance / 3.0F);
    maximum = std::max(maximum, weights[index]);
  }
  if (maximum > 0.0F) {
    for (float &weight : weights) {
      weight /= maximum;
    }
  }
  return weights;
}

std::uint16_t band_value(const float energy, const float gain, const float weight) noexcept {
  const float scaled = clamp01(energy * std::max(gain, 0.0F)) * clamp01(weight) * 254.0F;
  return static_cast<std::uint16_t>(std::lround(std::clamp(scaled, 0.0F, 254.0F)));
}

void write_group(protocol::SyntheticAudioFrame &frame, const std::size_t offset, const float energy,
                 const float gain, const std::array<float, 4> &shape) noexcept {
  for (std::size_t index = 0; index < shape.size(); ++index) {
    frame.bands[offset + index] = band_value(energy, gain, shape[index]);
  }
}

float level_value(const float energy, const float gain) noexcept {
  const float value = safe_positive(energy) * std::max(gain, 0.0F);
  if (!std::isfinite(value)) {
    return 0.0F;
  }
  return std::clamp(value, 0.0F, 255.0F);
}

} // namespace

SyntheticAudioMapper::SyntheticAudioMapper(MappingConfig config) : config_(config) {
  reset();
}

void SyntheticAudioMapper::reset() {
  previous_impact_ = false;
  last_frame_ = {};
  last_frame_.major_peak =
      std::isfinite(config_.quiet_major_peak_hz) && config_.quiet_major_peak_hz > 0.0F
          ? config_.quiet_major_peak_hz
          : 1.0F;
}

const MappingConfig &SyntheticAudioMapper::config() const {
  return config_;
}

protocol::SyntheticAudioFrame SyntheticAudioMapper::map(const motion::MotionFeatures &features) {
  if (!features.input_valid) {
    auto rejected = last_frame_;
    rejected.sample_peak = false;
    return rejected;
  }

  const float translation =
      safe_scale(features.linear_accel_magnitude_g, config_.translation_full_scale_g);
  const float rotation = safe_scale(features.angular_speed_dps, config_.rotation_full_scale_dps);
  const float shake = safe_scale(features.jerk_g_per_s, config_.shake_full_scale_g_per_s);
  const float impact = features.impact ? 1.0F : 0.0F;
  const auto shape = group_shape(features.orientation, config_.orientation_bias_strength);

  protocol::SyntheticAudioFrame frame{};
  frame.sample_raw = level_value(features.motion_energy_instant, config_.level_gain);
  frame.sample_smoothed = level_value(features.motion_energy_smoothed, config_.level_gain);
  frame.sample_peak = features.impact && !previous_impact_;

  write_group(frame, 0U, translation, config_.translation_band_gain, shape);
  write_group(frame, 4U, rotation, config_.rotation_band_gain, shape);
  write_group(frame, 8U, shake, config_.shake_band_gain, shape);
  const float transient =
      clamp01(impact + shake * std::max(config_.shake_to_impact_crossfeed, 0.0F));
  write_group(frame, 12U, transient, config_.impact_band_gain, shape);

  frame.magnitude = spectral_magnitude(frame);
  frame.major_peak = spectral_major_peak(frame, config_.quiet_major_peak_hz);

  previous_impact_ = features.impact;
  last_frame_ = frame;
  return frame;
}

float spectral_magnitude(const protocol::SyntheticAudioFrame &frame) noexcept {
  float sum_squares = 0.0F;
  for (const auto band : frame.bands) {
    const float value = static_cast<float>(std::min<std::uint16_t>(band, 254U));
    sum_squares += value * value;
  }
  return std::sqrt(sum_squares / static_cast<float>(frame.bands.size()));
}

float spectral_major_peak(const protocol::SyntheticAudioFrame &frame,
                          const float quiet_value_hz) noexcept {
  float weighted_sum = 0.0F;
  float total = 0.0F;
  for (std::size_t index = 0; index < frame.bands.size(); ++index) {
    const float weight = static_cast<float>(std::min<std::uint16_t>(frame.bands[index], 254U));
    weighted_sum += weight * kBalancedBandCentersHz[index];
    total += weight;
  }

  if (!(total > 0.0F) || !std::isfinite(total) || !std::isfinite(weighted_sum)) {
    return std::isfinite(quiet_value_hz) && quiet_value_hz > 0.0F ? quiet_value_hz : 1.0F;
  }
  const float centroid = weighted_sum / total;
  return std::isfinite(centroid) && centroid > 0.0F ? centroid : 1.0F;
}

} // namespace wledimuudp::mapping
