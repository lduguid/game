#pragma once

#include <SDL3/SDL.h>
#include <cmath>

/** Simple fly camera: W/S forward/back along view, A/D strafe, mouse steers yaw/pitch. */
struct FlyCamera {
  float pos[3] = {0.f, 6.f, 18.f};
  float yaw = 0.f;
  float pitch = -0.25f;
  float move_speed = 14.f;
  float mouse_sens = 0.0018f;

  void apply_mouse(float dx, float dy)
  {
    yaw += dx * mouse_sens;
    pitch -= dy * mouse_sens;
    const float lim = 1.553343f; /* ~89 deg */
    if (pitch > lim) {
      pitch = lim;
    }
    if (pitch < -lim) {
      pitch = -lim;
    }
  }

  void update(float dt, const bool* keys)
  {
    if (!keys) {
      return;
    }
    float fx = std::cos(yaw) * std::cos(pitch);
    float fy = std::sin(pitch);
    float fz = std::sin(yaw) * std::cos(pitch);
    float rx = -std::sin(yaw);
    float rz = std::cos(yaw);
    float sp = move_speed * dt;
    if (keys[SDL_SCANCODE_W]) {
      pos[0] += fx * sp;
      pos[1] += fy * sp;
      pos[2] += fz * sp;
    }
    if (keys[SDL_SCANCODE_S]) {
      pos[0] -= fx * sp;
      pos[1] -= fy * sp;
      pos[2] -= fz * sp;
    }
    if (keys[SDL_SCANCODE_A]) {
      pos[0] -= rx * sp;
      pos[2] -= rz * sp;
    }
    if (keys[SDL_SCANCODE_D]) {
      pos[0] += rx * sp;
      pos[2] += rz * sp;
    }
  }
};
