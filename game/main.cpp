
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <gl/GLU.h>

#include "camera.hpp"
#include "sine_math.hpp"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <random>
#include <vector>

namespace {
struct MainCallbackRateHint {
  MainCallbackRateHint() { SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60"); }
};
const MainCallbackRateHint g_main_callback_rate_hint;
}  // namespace

struct Vec3 {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
};

namespace sine_scatter {

/** Half-length along world +Z/-Z through each dot. Kept small vs wave spacing (~7) so split waves do not overlap. */
static constexpr float kDotZHalfDepth = 2.5f;

/** Extra world-X offset for wave B when 3D overlay mode (split_layout false). */
static constexpr float k3dOverlayXShift = 6.f;

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

class Instance {
public:
  explicit Instance(Config config)
      : config_(std::move(config))
  {
    const int n = config_.series.num_samples;
    const int tl = config_.trail_len;
    trail_3d_.resize(static_cast<size_t>(n * tl));
    trail_2d_.resize(static_cast<size_t>(n * tl));
  }

  void reset_trails()
  {
    trail_3d_ready_ = false;
    trail_2d_ready_ = false;
  }

  void draw_gl_3d(float time_seconds, float z_center, float x_overlay_shift, bool draw_axes_here)
  {
    const int n = config_.series.num_samples;
    const int tl = config_.trail_len;
    const float phase = sine_math::phase_at_time(time_seconds, config_.phase_speed);

    std::vector<float> radii = sine_math::compute_radii(config_.series, phase);
    const sine_math::RadiusRange range = sine_math::compute_radius_range(radii);

    std::vector<Vec3> points(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      float u = (n <= 1) ? 0.f : static_cast<float>(i) / static_cast<float>(n - 1);
      float x = (u - 0.5f) * 22.f + x_overlay_shift;
      float rn = (radii[static_cast<size_t>(i)] - range.min_v) / range.span();
      float y = rn * 12.f - 1.5f;
      float z = z_center;
      points[static_cast<size_t>(i)] = {x, y, z};
    }

    for (int i = 0; i < n; ++i) {
      Vec3* row = &trail_3d_[static_cast<size_t>(i * tl)];
      if (!trail_3d_ready_) {
        for (int j = 0; j < tl; ++j) {
          row[j] = points[static_cast<size_t>(i)];
        }
      } else {
        std::memmove(row, row + 1, (size_t)(tl - 1) * sizeof(Vec3));
        row[tl - 1] = points[static_cast<size_t>(i)];
      }
    }
    trail_3d_ready_ = true;

    if (draw_axes_here) {
      glLineWidth(1.5f);
      glBegin(GL_LINES);
      glColor3f(0.45f, 0.45f, 0.5f);
      glVertex3f(0.f, 0.f, 0.f);
      glVertex3f(8.f, 0.f, 0.f);
      glColor3f(0.35f, 0.55f, 0.35f);
      glVertex3f(0.f, 0.f, 0.f);
      glVertex3f(0.f, 8.f, 0.f);
      glColor3f(0.4f, 0.45f, 0.55f);
      glVertex3f(0.f, 0.f, 0.f);
      glVertex3f(0.f, 0.f, 8.f);
      glEnd();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.2f);
    for (int i = 0; i < n; ++i) {
      const Vec3* row = &trail_3d_[static_cast<size_t>(i * tl)];
      for (int s = 0; s < tl - 1; ++s) {
        float t = static_cast<float>(s) / static_cast<float>(SDL_max(1, tl - 2));
        float a = 0.15f + 0.55f * t;
        glColor4f(config_.trail_r / 255.f, config_.trail_g / 255.f, config_.trail_b / 255.f, a);
        glBegin(GL_LINES);
        glVertex3f(row[s].x, row[s].y, row[s].z);
        glVertex3f(row[s + 1].x, row[s + 1].y, row[s + 1].z);
        glEnd();
      }
    }
    glDisable(GL_BLEND);

    /* Depth stub along local Z through each sample (±kDotZHalfDepth from the dot center). */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < n; ++i) {
      const Vec3& p = points[static_cast<size_t>(i)];
      glColor4f(config_.dot_r / 255.f * 0.45f, config_.dot_g / 255.f * 0.45f, config_.dot_b / 255.f * 0.45f, 0.9f);
      glVertex3f(p.x, p.y, p.z - kDotZHalfDepth);
      glVertex3f(p.x, p.y, p.z + kDotZHalfDepth);
    }
    glEnd();
    glDisable(GL_BLEND);
    glLineWidth(1.f);

    glPointSize(SDL_max(2.0f, config_.dot_extent * 2.0f));
    glColor3ub(config_.dot_r, config_.dot_g, config_.dot_b);
    glBegin(GL_POINTS);
    for (int i = 0; i < n; ++i) {
      const Vec3& p = points[static_cast<size_t>(i)];
      glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
    glPointSize(1.f);
  }

  /** Orthographic 2D plot in pixel space (y downward), matching former SDL_Renderer layout. */
  void draw_gl_2d(float time_seconds, const sine_math::PlotRect& plot_rect, float x_shift_px, bool draw_axes_here)
  {
    const int n = config_.series.num_samples;
    const int tl = config_.trail_len;
    const float phase = sine_math::phase_at_time(time_seconds, config_.phase_speed);

    sine_math::PlotRect pr = plot_rect;
    pr.x += x_shift_px;

    std::vector<float> radii = sine_math::compute_radii(config_.series, phase);
    const sine_math::RadiusRange range = sine_math::compute_radius_range(radii);
    std::vector<sine_math::Point2f> pts = sine_math::compute_plot_points(config_.series, radii, pr, range);

    for (int i = 0; i < n; ++i) {
      sine_math::Point2f* row = &trail_2d_[static_cast<size_t>(i * tl)];
      if (!trail_2d_ready_) {
        for (int j = 0; j < tl; ++j) {
          row[j] = pts[static_cast<size_t>(i)];
        }
      } else {
        std::memmove(row, row + 1, (size_t)(tl - 1) * sizeof(sine_math::Point2f));
        row[tl - 1] = pts[static_cast<size_t>(i)];
      }
    }
    trail_2d_ready_ = true;

    if (draw_axes_here) {
      glLineWidth(1.5f);
      glColor3f(0.47f, 0.47f, 0.52f);
      glBegin(GL_LINE_LOOP);
      glVertex2f(pr.x, pr.y);
      glVertex2f(pr.x + pr.w, pr.y);
      glVertex2f(pr.x + pr.w, pr.y + pr.h);
      glVertex2f(pr.x, pr.y + pr.h);
      glEnd();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.2f);
    for (int i = 0; i < n; ++i) {
      const sine_math::Point2f* row = &trail_2d_[static_cast<size_t>(i * tl)];
      for (int s = 0; s < tl - 1; ++s) {
        float t = static_cast<float>(s) / static_cast<float>(SDL_max(1, tl - 2));
        float a = 0.15f + 0.55f * t;
        glColor4f(config_.trail_r / 255.f, config_.trail_g / 255.f, config_.trail_b / 255.f, a);
        glBegin(GL_LINES);
        glVertex2f(row[s].x, row[s].y);
        glVertex2f(row[s + 1].x, row[s + 1].y);
        glEnd();
      }
    }
    glDisable(GL_BLEND);

    const float ext = config_.dot_extent;
    glColor3ub(config_.dot_r, config_.dot_g, config_.dot_b);
    for (int i = 0; i < n; ++i) {
      float cx = pts[static_cast<size_t>(i)].x;
      float cy = pts[static_cast<size_t>(i)].y;
      glRectf(cx - ext, cy - ext, cx + ext + 1.f, cy + ext + 1.f);
    }
  }

  const Config& cfg() const { return config_; }

private:
  Config config_;
  std::vector<Vec3> trail_3d_;
  std::vector<sine_math::Point2f> trail_2d_;
  bool trail_3d_ready_ = false;
  bool trail_2d_ready_ = false;
};

}  // namespace sine_scatter

static SDL_Window* window = nullptr;
static SDL_GLContext gl_context = nullptr;
static FlyCamera g_camera;
static float g_mouse_dx = 0.f;
static float g_mouse_dy = 0.f;
static Uint64 g_last_ticks = 0;

/** false = 2D ortho plot; true = 3D perspective + fly camera (F2). */
static bool g_view_3d = true;

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

/** In 3D: waves separated along Z vs overlay. In 2D: stacked panes vs overlaid X shift. */
static bool g_split_layout = true;

/** 3D only: F3 toggles origin X/Y/Z axis lines drawn with the first wave. */
static bool g_show_3d_axes = true;

static GLuint g_tilemap_tex = 0;
static float g_bg_scroll_u = 0.f;

namespace {
struct TwinkleStar {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float phase = 0.f;
  float period_sec = 2.f;
  float size_peak = 4.f;
  float r = 1.f;
  float g = 1.f;
  float b = 1.f;
};

static constexpr int kTwinkleStarCount = 112;
static std::vector<TwinkleStar> g_twinkle_stars;
static std::mt19937 g_twinkle_rng{std::random_device{}()};

static void twinkle_star_respawn(TwinkleStar& s)
{
  std::uniform_real_distribution<float> ux(-118.f, 118.f);
  std::uniform_real_distribution<float> uy(-34.f, 54.f);
  std::uniform_real_distribution<float> uz(-50.f, -6.f);
  std::uniform_real_distribution<float> u_period(1.15f, 3.6f);
  std::uniform_real_distribution<float> u_peak(2.2f, 8.5f);
  std::uniform_real_distribution<float> u_tint(0.82f, 1.f);
  s.x = ux(g_twinkle_rng);
  s.y = uy(g_twinkle_rng);
  s.z = uz(g_twinkle_rng);
  s.period_sec = u_period(g_twinkle_rng);
  s.size_peak = u_peak(g_twinkle_rng);
  const float tr = u_tint(g_twinkle_rng);
  const float tg = u_tint(g_twinkle_rng);
  const float tb = u_tint(g_twinkle_rng);
  s.r = 0.88f + 0.12f * tr;
  s.g = 0.88f + 0.12f * tg;
  s.b = 0.92f + 0.08f * tb;
}

static void init_twinkle_stars()
{
  g_twinkle_stars.resize(static_cast<size_t>(kTwinkleStarCount));
  std::uniform_real_distribution<float> u_phase(0.f, 1.f);
  for (auto& s : g_twinkle_stars) {
    twinkle_star_respawn(s);
    s.phase = u_phase(g_twinkle_rng);
  }
}

static void update_twinkle_stars(float dt)
{
  for (auto& s : g_twinkle_stars) {
    if (s.period_sec <= 1e-4f) {
      s.period_sec = 1.f;
    }
    s.phase += dt / s.period_sec;
    if (s.phase >= 1.f) {
      s.phase = std::fmod(s.phase, 1.f);
      twinkle_star_respawn(s);
    }
  }
}

/** Additive-ish points between background and waves; envelope sin(pi*phase) for grow then fade. */
static void draw_twinkle_stars_3d()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  constexpr float kPi = 3.14159265f;
  for (const auto& s : g_twinkle_stars) {
    const float u = std::sin(kPi * s.phase);
    if (u <= 0.004f) {
      continue;
    }
    const float a = u * u;
    const float sz = 0.6f + (s.size_peak - 0.6f) * u;
    glPointSize(SDL_max(1.f, sz));
    glColor4f(s.r * u, s.g * u, s.b * u, a);
    glBegin(GL_POINTS);
    glVertex3f(s.x, s.y, s.z);
    glEnd();
  }
  glPointSize(1.f);
  glDisable(GL_BLEND);
  glColor3f(1.f, 1.f, 1.f);
}
}  // namespace

static void init_tilemap_texture()
{
  constexpr int res = 64;
  std::vector<unsigned char> pix(static_cast<size_t>(res * res * 3));
  for (int y = 0; y < res; ++y) {
    for (int x = 0; x < res; ++x) {
      float fx = x / static_cast<float>(res - 1);
      float fy = y / static_cast<float>(res - 1);
      float wave = 0.5f + 0.5f * std::sin((fx * 10.f + fy * 6.5f) * 3.1415926f);
      int cx = (x / 8) & 1;
      int cy = (y / 8) & 1;
      float chk = (cx ^ cy) != 0 ? 0.9f : 0.55f;
      unsigned char r = static_cast<unsigned char>(28 + wave * chk * 95);
      unsigned char g = static_cast<unsigned char>(32 + wave * chk * 85);
      unsigned char b = static_cast<unsigned char>(48 + wave * chk * 115);
      size_t i = (static_cast<size_t>(y * res + x)) * 3u;
      pix[i] = r;
      pix[i + 1] = g;
      pix[i + 2] = b;
    }
  }
  glGenTextures(1, &g_tilemap_tex);
  glBindTexture(GL_TEXTURE_2D, g_tilemap_tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, res, res, 0, GL_RGB, GL_UNSIGNED_BYTE, pix.data());
  glBindTexture(GL_TEXTURE_2D, 0);
}

/** Large XY plane behind the graph; UV tiling + texture-matrix scroll (smooth rightward drift). */
static void draw_3d_scrolling_background(float dt)
{
  if (g_tilemap_tex == 0) {
    return;
  }
  g_bg_scroll_u += dt * 0.44f;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, g_tilemap_tex);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glColor3f(0.5f, 0.52f, 0.58f);

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glTranslatef(-g_bg_scroll_u, 0.f, 0.f);
  glMatrixMode(GL_MODELVIEW);

  glBegin(GL_QUADS);
  const float z = -52.f;
  const float x0 = -130.f;
  const float x1 = 130.f;
  const float y0 = -40.f;
  const float y1 = 60.f;
  const float tu = 32.f;
  const float tv = 12.f;
  glTexCoord2f(0.f, 0.f);
  glVertex3f(x0, y0, z);
  glTexCoord2f(tu, 0.f);
  glVertex3f(x1, y0, z);
  glTexCoord2f(tu, tv);
  glVertex3f(x1, y1, z);
  glTexCoord2f(0.f, tv);
  glVertex3f(x0, y1, z);
  glEnd();

  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);

  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  glColor3f(1.f, 1.f, 1.f);
}

static void shutdown_tilemap_texture()
{
  if (g_tilemap_tex != 0 && window != nullptr && gl_context != nullptr) {
    SDL_GL_MakeCurrent(window, gl_context);
    glDeleteTextures(1, &g_tilemap_tex);
    g_tilemap_tex = 0;
  }
}

static void setup_viewport_3d(int w, int h)
{
  if (h <= 0) {
    h = 1;
  }
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  const double aspect = static_cast<double>(w) / static_cast<double>(h);
  gluPerspective(60.0, aspect, 0.1, 500.0);
  glMatrixMode(GL_MODELVIEW);
}

static void setup_viewport_2d(int w, int h)
{
  if (h <= 0) {
    h = 1;
  }
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, static_cast<double>(w), static_cast<double>(h), 0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void apply_camera_gl(const FlyCamera& cam)
{
  glLoadIdentity();
  float fx = std::cos(cam.yaw) * std::cos(cam.pitch);
  float fy = std::sin(cam.pitch);
  float fz = std::sin(cam.yaw) * std::cos(cam.pitch);
  gluLookAt(cam.pos[0], cam.pos[1], cam.pos[2], cam.pos[0] + fx, cam.pos[1] + fy, cam.pos[2] + fz, 0.0, 1.0,
            0.0);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  window = SDL_CreateWindow("Sine scatter — F2 2D/3D", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    SDL_Log("Couldn't create OpenGL window: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) {
    SDL_Log("Couldn't create GL context: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_GL_SetSwapInterval(1)) {
    SDL_Log("Warning: vsync not enabled: %s", SDL_GetError());
  }

  SDL_SetWindowRelativeMouseMode(window, g_view_3d);

  int w = 0, h = 0;
  SDL_GetWindowSize(window, &w, &h);
  if (g_view_3d) {
    setup_viewport_3d(w, h);
  } else {
    setup_viewport_2d(w, h);
  }

  glEnable(GL_DEPTH_TEST);
  glClearColor(0.09f, 0.09f, 0.11f, 1.f);

  init_tilemap_texture();
  init_twinkle_stars();

  g_last_ticks = SDL_GetTicks();
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;
  }
  if (event->type == SDL_EVENT_MOUSE_MOTION) {
    g_mouse_dx += event->motion.xrel;
    g_mouse_dy += event->motion.yrel;
    return SDL_APP_CONTINUE;
  }
  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
    if (event->key.key == SDLK_F1) {
      g_split_layout = !g_split_layout;
      return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_F2) {
      const bool entering_3d = !g_view_3d;
      g_view_3d = !g_view_3d;
      /* 2D overlay uses one plot for both waves; if we keep split_layout false in 3D, overlay
       * puts both waves near the same X/Z — re-enable Z-split when coming back from 2D. */
      if (g_view_3d && entering_3d) {
        g_split_layout = true;
      }
      g_mouse_dx = 0.f;
      g_mouse_dy = 0.f;
      SDL_SetWindowRelativeMouseMode(window, g_view_3d);
      g_sine_a.reset_trails();
      g_sine_b.reset_trails();
      return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_F3) {
      g_show_3d_axes = !g_show_3d_axes;
      return SDL_APP_CONTINUE;
    }
    if (event->key.key == SDLK_ESCAPE) {
      return SDL_APP_SUCCESS;
    }
  }
  if (event->type == SDL_EVENT_WINDOW_RESIZED) {
    const int rw = static_cast<int>(event->window.data1);
    const int rh = static_cast<int>(event->window.data2);
    if (g_view_3d) {
      setup_viewport_3d(rw, rh);
    } else {
      setup_viewport_2d(rw, rh);
    }
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  Uint64 now = SDL_GetTicks();
  float dt = (g_last_ticks > 0) ? (static_cast<float>(now - g_last_ticks) / 1000.f) : (1.f / 60.f);
  g_last_ticks = now;
  if (dt > 0.1f) {
    dt = 0.1f;
  }

  if (g_view_3d) {
    g_camera.apply_mouse(g_mouse_dx, g_mouse_dy);
    g_camera.update(dt, SDL_GetKeyboardState(nullptr));
  }
  g_mouse_dx = 0.f;
  g_mouse_dy = 0.f;

  int w = 0, h = 0;
  SDL_GetWindowSize(window, &w, &h);
  if (w > 0 && h > 0) {
    if (g_view_3d) {
      setup_viewport_3d(w, h);
    } else {
      setup_viewport_2d(w, h);
    }
  }

  const float t_sec = SDL_GetTicks() / 1000.0f;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (g_view_3d) {
    glEnable(GL_DEPTH_TEST);
    apply_camera_gl(g_camera);
    draw_3d_scrolling_background(dt);
    update_twinkle_stars(dt);
    draw_twinkle_stars_3d();
    if (g_split_layout) {
      g_sine_a.draw_gl_3d(t_sec, -3.5f, 0.f, g_show_3d_axes);
      g_sine_b.draw_gl_3d(t_sec, 3.5f, 0.f, false);
    } else {
      g_sine_a.draw_gl_3d(t_sec, 0.f, 0.f, g_show_3d_axes);
      g_sine_b.draw_gl_3d(t_sec, 0.f, sine_scatter::k3dOverlayXShift, false);
    }
  } else {
    glDisable(GL_DEPTH_TEST);
    glLoadIdentity();
    const float margin_left = 56.f;
    const float margin_right = 24.f;
    const float margin_top = 36.f;
    const float margin_bottom = 48.f;
    const float plot_left = margin_left;
    const float plot_top = margin_top;
    const float plot_w = static_cast<float>(w) - margin_left - margin_right;
    const float plot_h = static_cast<float>(h) - margin_top - margin_bottom;
    const float mid_gap = 8.f;
    const float half_h = SDL_max(1.f, (plot_h - mid_gap) * 0.5f);

    if (g_split_layout) {
      const sine_math::PlotRect plot_a = {plot_left, plot_top, plot_w, half_h};
      const sine_math::PlotRect plot_b = {plot_left, plot_top + half_h + mid_gap, plot_w, half_h};
      g_sine_a.draw_gl_2d(t_sec, plot_a, 0.f, true);
      g_sine_b.draw_gl_2d(t_sec, plot_b, 0.f, true);
    } else {
      const sine_math::PlotRect plot_both = {plot_left, plot_top, plot_w, plot_h};
      g_sine_a.draw_gl_2d(t_sec, plot_both, 0.f, true);
      g_sine_b.draw_gl_2d(t_sec, plot_both, 0.f, false);
    }
  }

  SDL_GL_SwapWindow(window);

  char title[240];
  std::snprintf(title, sizeof(title),
                "%s | F1:%s | F2:%s%s | Esc quit", g_view_3d ? "3D fly" : "2D plot",
                g_split_layout ? "overlay" : "split", g_view_3d ? "2D" : "3D",
                g_view_3d ? (g_show_3d_axes ? " | F3:hide axes" : " | F3:show axes") : "");
  SDL_SetWindowTitle(window, title);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  if (window) {
    SDL_SetWindowRelativeMouseMode(window, false);
  }
  shutdown_tilemap_texture();
  if (gl_context) {
    SDL_GL_DestroyContext(gl_context);
    gl_context = nullptr;
  }
  window = nullptr;
}
