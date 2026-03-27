#pragma once

#include <vector>

namespace sine_math {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

/** Same formula as the original Python: amplitude * sin(angle) + (max_radius - min_radius). */
float sine_mapping(float frequency, float base_frequency, float amplitude, float phase,
                   float min_radius, float max_radius);

/** Phase used in the animation: wraps to [0, 2*pi). */
float phase_at_time(float time_seconds, float phase_speed_rad_per_s);

/** Parameters for the frequency–radius series only (no rendering). */
struct SeriesParams {
  int num_samples = 100;
  float freq_step = 0.1f;
  float base_frequency = 1.0f;
  float amplitude = 5.0f;
  float min_radius = 1.0f;
  float max_radius = 10.0f;
};

std::vector<float> compute_radii(const SeriesParams& p, float phase);

struct RadiusRange {
  float min_v = 0.f;
  float max_v = 0.f;
  /** Same fallback as the renderer when all radii are equal. */
  float span() const;
};

RadiusRange compute_radius_range(const std::vector<float>& radii);

/** Plot area in arbitrary float coordinates (e.g. pixels). */
struct PlotRect {
  float x = 0.f;
  float y = 0.f;
  float w = 0.f;
  float h = 0.f;
};

struct Point2f {
  float x = 0.f;
  float y = 0.f;
};

/** Map frequencies and radii into plot coordinates (y increases downward, like SDL). */
std::vector<Point2f> compute_plot_points(const SeriesParams& p, const std::vector<float>& radii,
                                         const PlotRect& rect, const RadiusRange& range);

}  // namespace sine_math
