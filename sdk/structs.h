#pragma once
#include <cmath>
#include <cstdint>
#include <string>

struct Vector2 {
  float x, y;
  Vector2() : x(0), y(0) {}
  Vector2(float x, float y) : x(x), y(y) {}
};

struct Vector3 {
  float x, y, z;
  Vector3() : x(0), y(0), z(0) {}
  Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
  bool IsZero() const { return x == 0 && y == 0 && z == 0; }

  Vector3 operator-(const Vector3 &v) const {
    return Vector3(x - v.x, y - v.y, z - v.z);
  }
  Vector3 operator+(const Vector3 &v) const {
    return Vector3(x + v.x, y + v.y, z + v.z);
  }
  Vector3 operator*(float f) const { return Vector3(x * f, y * f, z * f); }
};

struct ViewMatrix {
  float m[4][4];
};

struct PlayerData {
  uintptr_t pawn;
  uintptr_t controller;
  int health;
  uint8_t team;
  bool isAlive;
  bool isValid;
  bool isVisible;
  uint32_t flags;
  uint32_t spottedMask[2];
  long long lastVisibleMs;
  Vector3 origin;
  Vector3 headPos;
  Vector3 bonePositions[64];
  char name[128];
  char weaponName[128];
  float distance;
  uint32_t crosshairId;
  int money;
  float flashDuration;
  bool isDefusing;
};

struct LocalPlayer {
  uintptr_t pawn;
  int health;
  uint8_t team;
  int playerIndex;
  bool isValid;
  uint32_t flags;
  Vector3 origin;
  Vector3 vecViewOffset;
  Vector3 viewAngles;
  Vector3 aimPunch;
  uint32_t crosshairId;
  float sensitivity;
  float fov;
};

struct C4Data {
  bool isPlanted     = false;
  bool isTicking     = false;
  bool isDefused     = false;
  bool hasExploded   = false;
  bool isBeingDefused= false;
  int  bombSite      = -1;
  float blowTime     = 0.f;
  float timerLength  = 40.f;
  float defuseCountDown = 0.f;
  float defuseLength = 0.f;
  Vector3 origin     = {};
  uint32_t defuserHandle = 0;
};

enum BoneIndex : int {
    PELVIS = 1,
    SPINE0 = 2,
    SPINE1 = 3,
    SPINE2 = 4,
    NECK = 6,
    HEAD = 7,
    SHOULDER_L = 9,
    ELBOW_L = 10,
    HAND_L = 11,
    SHOULDER_R = 13,
    ELBOW_R = 14,
    HAND_R = 15,
    HIP_L = 17,
    KNEE_L = 18,
    FOOT_HEEL_L = 19,
    HIP_R = 20,
    KNEE_R = 21,
    FOOT_HEEL_R = 22,
    CHEST = 23,
    FOOT_TOES_L = 24,
    FOOT_TOES_R = 25
};
