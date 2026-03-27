/**
 * Console unit tests for sine_math (no SDL / graphics).
 * Build: game_tests project, or: cl /EHsc sine_math.cpp sine_math_tests.cpp //
 */
#include "sine_math.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

int g_failures = 0;

void expect_true(bool cond, const char* expr, const char* file, int line)
{
  if (!cond) {
    std::fprintf(stderr, "FAIL %s:%d  (%s)\n", file, line, expr);
    ++g_failures;
  }
}

void expect_near(float a, float b, float eps, const char* file, int line)
{
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "FAIL %s:%d  expected %g got %g (eps %g)\n", file, line, static_cast<double>(b),
                 static_cast<double>(a), static_cast<double>(eps));
    ++g_failures;
  }
}

#define EXPECT_TRUE(x) expect_true((x), #x, __FILE__, __LINE__)
#define EXPECT_NEAR(a, b, eps) expect_near((a), (b), (eps), __FILE__, __LINE__)

void test_sine_mapping_matches_hand_computed()
{
  /* frequency 0, base 1, amplitude 5, phase 0, min 1 max 10 -> 5*sin(0) + 9 = 9 */
  float r = sine_math::sine_mapping(0.f, 1.f, 5.f, 0.f, 1.f, 10.f);
  EXPECT_NEAR(r, 9.f, 1e-5f);
}

void test_phase_at_time_wraps()
{
  float p0 = sine_math::phase_at_time(0.f, 2.f);
  EXPECT_NEAR(p0, 0.f, 1e-5f);
  /* t * 2 = pi/2  =>  t = pi/4 = kTwoPi/8 */
  float p1 = sine_math::phase_at_time(sine_math::kTwoPi / 8.f, 2.f);
  EXPECT_NEAR(p1, sine_math::kPi * 0.5f, 1e-4f);
}

void test_compute_radii_length_and_monotonic_index()
{
  sine_math::SeriesParams p;
  p.num_samples = 5;
  p.freq_step = 0.1f;
  p.base_frequency = 1.f;
  p.amplitude = 1.f;
  p.min_radius = 0.f;
  p.max_radius = 0.f; /* radius = sin(angle) */

  auto radii = sine_math::compute_radii(p, 0.f);
  EXPECT_TRUE(radii.size() == 5);
  EXPECT_NEAR(radii[0], 0.f, 1e-5f); /* sin(0)=0 */
}

void test_radius_range_and_plot_points()
{
  sine_math::SeriesParams p;
  p.num_samples = 3;
  p.freq_step = 1.f;
  p.base_frequency = 1.f;
  p.amplitude = 0.f;
  p.min_radius = 0.f;
  p.max_radius = 0.f; /* all radii 0 */

  auto radii = sine_math::compute_radii(p, 0.f);
  auto range = sine_math::compute_radius_range(radii);
  EXPECT_NEAR(range.min_v, 0.f, 1e-5f);
  EXPECT_NEAR(range.max_v, 0.f, 1e-5f);
  EXPECT_NEAR(range.span(), 1.f, 1e-5f); /* degenerate -> 1 */

  sine_math::PlotRect rect{0.f, 0.f, 100.f, 50.f};
  auto pts = sine_math::compute_plot_points(p, radii, rect, range);
  EXPECT_TRUE(pts.size() == 3);
  /* All radii equal (min==max): t_y=0 -> y at min-radius edge (bottom of rect in SDL coords). */
  EXPECT_NEAR(pts[0].y, 50.f, 1e-4f);
}

}  // namespace

int main()
{
  test_sine_mapping_matches_hand_computed();
  test_phase_at_time_wraps();
  test_compute_radii_length_and_monotonic_index();
  test_radius_range_and_plot_points();

  if (g_failures != 0) {
    std::fprintf(stderr, "\n%d test(s) failed.\n", g_failures);
    return 1;
  }
  std::printf("All sine_math tests passed.\n");
  return 0;
}
