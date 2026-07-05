#pragma once
#include "../sdk/structs.h"
#include <Windows.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline float NormalizeYaw(float angle) {
  if (!std::isfinite(angle)) return 0.f;
  angle = fmodf(angle, 360.f);
  if (angle > 180.f) angle -= 360.f;
  if (angle < -180.f) angle += 360.f;
  return angle;
}

inline float NormalizePitch(float angle) {
  if (angle > 89.f)
    angle = 89.f;
  if (angle < -89.f)
    angle = -89.f;
  return angle;
}

inline bool WorldToScreen(const Vector3 &worldPos, Vector2 &screenPos,
                          const ViewMatrix &vm, int screenW, int screenH) {
  float w = vm.m[3][0] * worldPos.x + vm.m[3][1] * worldPos.y +
            vm.m[3][2] * worldPos.z + vm.m[3][3];
  if (w < 0.001f)
    return false;

  float x = vm.m[0][0] * worldPos.x + vm.m[0][1] * worldPos.y +
            vm.m[0][2] * worldPos.z + vm.m[0][3];
  float y = vm.m[1][0] * worldPos.x + vm.m[1][1] * worldPos.y +
            vm.m[1][2] * worldPos.z + vm.m[1][3];

  screenPos.x = (screenW / 2.f) + (x / w) * (screenW / 2.f);
  screenPos.y = (screenH / 2.f) - (y / w) * (screenH / 2.f);
  return true;
}

inline Vector3 CalcAngle(const Vector3 &src, const Vector3 &dst) {
  Vector3 angle;
  Vector3 delta = {dst.x - src.x, dst.y - src.y, dst.z - src.z};
  float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
  angle.x = -atan2f(delta.z, hyp) * (180.f / (float)M_PI);
  angle.y = atan2f(delta.y, delta.x) * (180.f / (float)M_PI);
  angle.z = 0.f;
  return angle;
}

inline float CalcFOV(const Vector3 &currentAngles,
                     const Vector3 &targetAngles) {
  Vector3 delta = {targetAngles.x - currentAngles.x,
                   targetAngles.y - currentAngles.y, 0.f};
  delta.y = NormalizeYaw(delta.y);
  delta.x = NormalizePitch(delta.x);
  return sqrtf(delta.x * delta.x + delta.y * delta.y);
}

inline float Distance3D(const Vector3 &a, const Vector3 &b) {
  return sqrtf(powf(a.x - b.x, 2) + powf(a.y - b.y, 2) + powf(a.z - b.z, 2));
}

inline Vector3 SmoothAngles(const Vector3 &current, const Vector3 &target,
                            float smooth) {
  if (smooth <= 1.f)
    return target;
  Vector3 delta = {target.x - current.x, target.y - current.y, 0.f};
  delta.y = NormalizeYaw(delta.y);
  delta.x = NormalizePitch(delta.x);
  Vector3 smoothed = current;
  smoothed.x += delta.x / smooth;
  smoothed.y += delta.y / smooth;
  return smoothed;
}
