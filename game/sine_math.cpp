//
#include "sine_math.hpp"

#include <algorithm>
#include <cmath>

namespace sine_math {

float sine_mapping(float frequency, float base_frequency, float amplitude, float phase,
                   float min_radius, float max_radius)
{
  float angle = kTwoPi * (frequency / base_frequency) + phase;
  return amplitude * std::sin(angle) + (max_radius - min_radius);
}

float phase_at_time(float time_seconds, float phase_speed_rad_per_s)
{
  return std::fmod(time_seconds * phase_speed_rad_per_s, kTwoPi);
}

std::vector<float> compute_radii(const SeriesParams& p, float phase)
{
  std::vector<float> radii(static_cast<size_t>(p.num_samples));
  for (int i = 0; i < p.num_samples; ++i) {
    float frequency = static_cast<float>(i) * p.freq_step;
    radii[static_cast<size_t>(i)] = sine_mapping(frequency, p.base_frequency, p.amplitude, phase, p.min_radius, p.max_radius);
  }

  return radii;
}

RadiusRange compute_radius_range(const std::vector<float>& radii)
{
  RadiusRange r{};
  if (radii.empty()) {
    return r;
  }

  r.min_v = r.max_v = radii[0];
  for (float v : radii) {
    r.min_v = std::min(r.min_v, v);
    r.max_v = std::max(r.max_v, v);
  }
  
  return r;
}

float RadiusRange::span() const
{
  float s = max_v - min_v;
  return s > 0.0f ? s : 1.0f;
}

std::vector<Point2f> compute_plot_points(const SeriesParams& p, const std::vector<float>& radii,
                                           const PlotRect& rect, const RadiusRange& range)
{
  const int n = p.num_samples;
  const float freq_min = 0.0f;
  const float freq_max = static_cast<float>(n - 1) * p.freq_step;
  const float r_span = range.span();

  std::vector<Point2f> points(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    float frequency = static_cast<float>(i) * p.freq_step;
    float t_x = (frequency - freq_min) / (freq_max - freq_min);
    float px = rect.x + t_x * rect.w;
    float t_y = (radii[static_cast<size_t>(i)] - range.min_v) / r_span;
    float py = rect.y + (1.0f - t_y) * rect.h;
    points[static_cast<size_t>(i)] = {px, py};
  }

  return points;
}

}  // namespace sine_math
