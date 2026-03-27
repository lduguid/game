

/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "sine_math.hpp"
#include <cmath>
#include <cstring>
#include <vector>

/* Before SDL_Init: steady SDL_AppIterate (not only on events). */
namespace {
struct MainCallbackRateHint {
  MainCallbackRateHint() { SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60"); }
};
const MainCallbackRateHint g_main_callback_rate_hint;
}  // namespace

namespace sine_scatter {

/** Per-animation: math in `series`, plus phase speed and rendering-only fields. */
struct Config {
  sine_math::SeriesParams series;
  float phase_speed = 2.0f;
  float dot_extent = 2.0f;
  int trail_len = 20;
  Uint8 dot_r = 100;
  Uint8 dot_g = 200;
  Uint8 dot_b = 255;
  Uint8 trail_r = 70;
  Uint8 trail_g = 140;
  Uint8 trail_b = 180;
};

/** Self-contained animated scatter: trail state + draw into a rectangular plot region. */
class Instance {
public:
  explicit Instance(Config config)
      : config_(std::move(config))
  {
    trail_.resize(static_cast<size_t>(config_.series.num_samples) * static_cast<size_t>(config_.trail_len));
  }

  void draw(SDL_Renderer* renderer, const SDL_FRect& plot_rect, float time_seconds, bool draw_axes = true)
  {
    const int n = config_.series.num_samples;
    const int tl = config_.trail_len;
    const float phase = sine_math::phase_at_time(time_seconds, config_.phase_speed);

    const float plot_left = plot_rect.x;
    const float plot_top = plot_rect.y;
    const float plot_w = plot_rect.w;
    const float plot_h = plot_rect.h;

    const sine_math::PlotRect pr{plot_left, plot_top, plot_w, plot_h};
    std::vector<float> radii = sine_math::compute_radii(config_.series, phase);
    const sine_math::RadiusRange range = sine_math::compute_radius_range(radii);
    std::vector<sine_math::Point2f> plot_points = sine_math::compute_plot_points(config_.series, radii, pr, range);

    std::vector<SDL_FPoint> points(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      points[static_cast<size_t>(i)].x = plot_points[static_cast<size_t>(i)].x;
      points[static_cast<size_t>(i)].y = plot_points[static_cast<size_t>(i)].y;
    }

    if (draw_axes) {
      SDL_SetRenderDrawColor(renderer, 120, 120, 128, 255);
      SDL_RenderLine(renderer, plot_left, plot_top + plot_h, plot_left + plot_w, plot_top + plot_h);
      SDL_RenderLine(renderer, plot_left, plot_top, plot_left, plot_top + plot_h);
    }

    for (int i = 0; i < n; ++i) {
      SDL_FPoint* row = &trail_[static_cast<size_t>(i * tl)];
      if (!trail_ready_) {
        for (int j = 0; j < tl; ++j) {
          row[j] = points[static_cast<size_t>(i)];
        }
      } else {
        std::memmove(row, row + 1, (size_t)(tl - 1) * sizeof(SDL_FPoint));
        row[tl - 1] = points[static_cast<size_t>(i)];
      }
    }
    trail_ready_ = true;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < n; ++i) {
      const SDL_FPoint* row = &trail_[static_cast<size_t>(i * tl)];
      for (int s = 0; s < tl - 1; ++s) {
        const int a = 28 + (int)(140 * s / SDL_max(1, tl - 2));
        SDL_SetRenderDrawColor(renderer, config_.trail_r, config_.trail_g, config_.trail_b, a);
        SDL_RenderLine(renderer, row[s].x, row[s].y, row[s + 1].x, row[s + 1].y);
      }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(renderer, config_.dot_r, config_.dot_g, config_.dot_b, 255);
    const float ext = config_.dot_extent;
    for (int i = 0; i < n; ++i) {
      float cx = points[static_cast<size_t>(i)].x;
      float cy = points[static_cast<size_t>(i)].y;
      const SDL_FRect dot = {cx - ext, cy - ext, 2.0f * ext + 1.0f, 2.0f * ext + 1.0f};
      SDL_RenderFillRect(renderer, &dot);
    }
  }

  const Config& cfg() const { return config_; }

private:
  Config config_;
  std::vector<SDL_FPoint> trail_;
  bool trail_ready_ = false;
};

}  // namespace sine_scatter

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

namespace {
sine_scatter::Config make_config_a()
{
  sine_scatter::Config c;
  c.series.num_samples = 100;
  c.series.freq_step = 0.1f;
  c.series.base_frequency = 10.0f;
  c.series.amplitude = 5.0f;
  c.series.min_radius = 1.0f;
  c.series.max_radius = 10.0f;
  c.phase_speed = 2.0f;
  c.dot_extent = 2.0f;
  c.trail_len = 20;
  c.dot_r = 100;
  c.dot_g = 200;
  c.dot_b = 255;
  c.trail_r = 70;
  c.trail_g = 140;
  c.trail_b = 180;
  return c;
}

sine_scatter::Config make_config_b()
{
  sine_scatter::Config c;
  c.series.num_samples = 100;
  c.series.freq_step = 0.1f;
  c.series.base_frequency = 3.0f;
  c.series.amplitude = 4.0f;
  c.series.min_radius = 1.0f;
  c.series.max_radius = 10.0f;
  c.phase_speed = 3.5f;
  c.dot_extent = 2.0f;
  c.trail_len = 20;
  c.dot_r = 255;
  c.dot_g = 180;
  c.dot_b = 120;
  c.trail_r = 160;
  c.trail_g = 100;
  c.trail_b = 60;
  return c;
}
}  // namespace

static sine_scatter::Instance g_sine_a(make_config_a());
static sine_scatter::Instance g_sine_b(make_config_b());

/** true = stacked split panes; false = both waves in one plot (overlay). Toggle with F1. */
static bool g_split_view = true;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
  if (!SDL_CreateWindowAndRenderer("Frequency vs Radius", 800, 600, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
    SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }
  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
    if (event->key.key == SDLK_F1) {
      g_split_view = !g_split_view;
      return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_ESCAPE) {
      return SDL_APP_SUCCESS;
    }
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  int w = 0;
  int h = 0;
  SDL_SetRenderScale(renderer, 1.0f, 1.0f);
  SDL_GetRenderOutputSize(renderer, &w, &h);

  const float margin_left = 56.0f;
  const float margin_right = 24.0f;
  const float margin_top = 36.0f;
  const float margin_bottom = 48.0f;
  const float plot_left = margin_left;
  const float plot_top = margin_top;
  const float plot_w = (float)w - margin_left - margin_right;
  const float plot_h = (float)h - margin_top - margin_bottom;
  const float mid_gap = 8.0f;
  const float half_h = SDL_max(1.0f, (plot_h - mid_gap) * 0.5f);

  const float t_sec = SDL_GetTicks() / 1000.0f;

  SDL_SetRenderDrawColor(renderer, 24, 24, 28, 255);
  SDL_RenderClear(renderer);

  if (g_split_view) {
    const SDL_FRect plot_a = {plot_left, plot_top, plot_w, half_h};
    const SDL_FRect plot_b = {plot_left, plot_top + half_h + mid_gap, plot_w, half_h};
    g_sine_a.draw(renderer, plot_a, t_sec, true);
    g_sine_b.draw(renderer, plot_b, t_sec, true);
  } else {
    const SDL_FRect plot_both = {plot_left, plot_top, plot_w, plot_h};
    g_sine_a.draw(renderer, plot_both, t_sec, true);
    g_sine_b.draw(renderer, plot_both, t_sec, false);
  }

  SDL_SetRenderDrawColor(renderer, 200, 200, 210, 255);
  SDL_RenderDebugText(renderer, 8.0f, 8.0f,
                      g_split_view ? "Split view  [F1: overlay]" : "Overlay   [F1: split]");
  SDL_RenderDebugText(renderer, 8.0f, 22.0f, "Esc: quit");
  SDL_RenderDebugText(renderer, plot_left + plot_w * 0.5f - 40.0f, (float)h - margin_bottom + 8.0f, "Frequency");
  SDL_RenderDebugText(renderer, 4.0f, plot_top + plot_h * 0.5f - 8.0f, "Radius");

  SDL_RenderPresent(renderer);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
}
