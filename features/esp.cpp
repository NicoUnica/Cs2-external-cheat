#include "esp.h"
#include "../memory/process.h"
#include "../memory/read.h"
#include "../sdk/math.h"
#include "config.h"
#include "imgui.h"
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>

extern int g_screenW;
extern int g_screenH;

void DrawWatermark(ImDrawList* dl) {
    const float PAD   = 10.f;
    const float PW    = 42.f;
    const float PH    = 22.f;
    float px = PAD;
    float py = PAD;

    ImU32 bgColor = IM_COL32(15, 15, 20, 255);
    dl->AddRectFilled({px, py}, {px + PW, py + PH}, bgColor, 0.f);
    dl->AddRectFilledMultiColor({px, py}, {px + PW, py + 1},
        IM_COL32(153, 51, 204, 255), IM_COL32(204, 102, 255, 255),
        IM_COL32(204, 102, 255, 255), IM_COL32(153, 51, 204, 255));

    ImVec2 textSize = ImGui::CalcTextSize("Nico");
    float textX = px + (PW - textSize.x) / 2;
    dl->AddText(ImGui::GetFont(), 13.f, {textX, py + 4}, IM_COL32(255, 255, 255, 255), "Nico");
}

void DrawBombTimer(ImDrawList* dl) {
    C4Data c4;
    {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        c4 = g_c4;
    }

    static bool s_wasPlanted = false;
    if (!c4.isPlanted) {
        s_wasPlanted = false;
        return;
    }

    if (c4.isDefused || c4.hasExploded) {
        s_wasPlanted = false;
        return;
    }

    static float s_levelStartWallSec = 0.f;
    static float s_levelStartGame    = 0.f;
    static float s_lastBlowTime = 0.f;
    
    if (!s_wasPlanted || c4.blowTime != s_lastBlowTime) {
        s_wasPlanted = true;
        s_lastBlowTime      = c4.blowTime;
        s_levelStartGame    = c4.blowTime - c4.timerLength;
        s_levelStartWallSec = GetTickCount64() / 1000.f - s_levelStartGame;
    }

    const float PAD   = 10.f;
    const float PW    = 160.f;
    const float PH    = 25.f;
    float px = PAD;
    float py = g_screenH / 2 - PH / 2;

    ImU32 bgColor = IM_COL32(15, 15, 20, 255);
    dl->AddRectFilled({px, py}, {px + PW, py + PH}, bgColor, 0.f);
    dl->AddRectFilledMultiColor({px, py}, {px + PW, py + 2},
        IM_COL32(153, 51, 204, 255), IM_COL32(204, 102, 255, 255),
        IM_COL32(204, 102, 255, 255), IM_COL32(153, 51, 204, 255));

    ImU32 whiteCol = IM_COL32(255, 255, 255, 255);

    const char* siteStr = (c4.bombSite == 0) ? "A" : "B";
    char textBuf[32];
    snprintf(textBuf, sizeof(textBuf), "bomb site %s", siteStr);
    
    dl->AddText(ImGui::GetFont(), 13.f, {px + 8, py + 6}, whiteCol, textBuf);
}


static inline ImU32 C4(const float *c) {
  return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255),
                  (int)(c[3] * 255));
}

void DrawESP() {
  if (!g_cfg.espEnabled)
    return;

  ViewMatrix vm;
  LocalPlayer lp;
  PlayerData players[65];
  {
    std::lock_guard<std::mutex> lock(g_dataMutex);
    vm = g_viewMatrix;
    lp = g_localPlayer;
    memcpy(players, g_players, sizeof(players));
  }
  if (!lp.isValid)
    return;

  LONGLONG nowMs = GetTickCount64();

  ImDrawList *dl = ImGui::GetBackgroundDrawList();

  for (int i = 1; i <= 64; i++) {
    const PlayerData &p = players[i];
    if (!p.isValid || !p.isAlive || p.distance > 10000.f)
      continue;

    bool isTeammate = (lp.team >= 2 && p.team >= 2 && p.team == lp.team);
    if (g_cfg.teamCheck && isTeammate)
      continue;

    Vector3 headW = p.headPos;
    if (headW.x == 0 && headW.y == 0) {
      headW = p.origin;
      headW.z += 72.f;
    }
    Vector3 footW = p.origin;

    struct BoneConnection { int a, b; };
    BoneConnection connections[] = {
        { PELVIS, SPINE1 }, { SPINE1, SPINE2 }, { SPINE2, CHEST }, { CHEST, NECK }, { NECK, HEAD },
        { NECK, SHOULDER_L }, { SHOULDER_L, ELBOW_L }, { ELBOW_L, HAND_L },
        { NECK, SHOULDER_R }, { SHOULDER_R, ELBOW_R }, { ELBOW_R, HAND_R },
        { PELVIS, HIP_L }, { HIP_L, KNEE_L }, { KNEE_L, FOOT_HEEL_L },
        { PELVIS, HIP_R }, { HIP_R, KNEE_R }, { KNEE_R, FOOT_HEEL_R }
    };

    static const int usedBones[] = {
        PELVIS, SPINE1, SPINE2, CHEST, NECK, HEAD,
        SHOULDER_L, ELBOW_L, HAND_L, SHOULDER_R, ELBOW_R, HAND_R,
        HIP_L, KNEE_L, FOOT_HEEL_L, HIP_R, KNEE_R, FOOT_HEEL_R
    };

    Vector2 screenBones[64];
    bool validBonesScreen[64] = {false};
    bool hasValidBones = false;

    for (int bi = 0; bi < sizeof(usedBones)/sizeof(usedBones[0]); bi++) {
      int b = usedBones[bi];
      float distSq = p.bonePositions[b].x * p.bonePositions[b].x + 
                     p.bonePositions[b].y * p.bonePositions[b].y + 
                     p.bonePositions[b].z * p.bonePositions[b].z;
      if (distSq > 10.f) {
        if (WorldToScreen(p.bonePositions[b], screenBones[b], vm, g_screenW, g_screenH)) {
          validBonesScreen[b] = true;
          hasValidBones = true;
        }
      }
    }

    Vector2 headS, footS;
    
    Vector3 topW = p.bonePositions[HEAD];
    if (topW.IsZero()) {
        topW = p.origin;
        topW.z += 70.f;
    }

    Vector3 botW = p.origin;
    botW.z -= 4.f;

    bool headInFront = WorldToScreen(topW, headS, vm, g_screenW, g_screenH);
    bool footInFront = WorldToScreen(botW, footS, vm, g_screenW, g_screenH);

    if (!headInFront && !footInFront) continue;

    float h = footS.y - headS.y;
    float w = h * 0.45f;
    float x = footS.x - (w * 0.5f);
    float y = headS.y;

    bool isOffScreen = (x + w < 0 || x > g_screenW || y + h < 0 || y > g_screenH);

    if (g_cfg.espBones && hasValidBones && !isOffScreen) {
      ImU32 boneCol = C4(g_cfg.colEspBone);
      
      float distMeters = p.distance / 39.37f;
      float boneScale = 1.0f;
      if (distMeters > 100.f) boneScale = 1.1f;
      else if (distMeters > 50.f) boneScale = 1.05f;
      else if (distMeters > 25.f) boneScale = 1.02f;

      for (const auto& conn : connections) {
        if (validBonesScreen[conn.a] && validBonesScreen[conn.b]) {
            float dx = p.bonePositions[conn.a].x - p.bonePositions[conn.b].x;
          float dy = p.bonePositions[conn.a].y - p.bonePositions[conn.b].y;
          float dz = p.bonePositions[conn.a].z - p.bonePositions[conn.b].z;
          if ((dx*dx + dy*dy + dz*dz) > 2500.f) continue;

          ImVec2 p1 = {screenBones[conn.a].x, screenBones[conn.a].y};
          ImVec2 p2 = {screenBones[conn.b].x, screenBones[conn.b].y};

          float lineWidth = 1.5f * boneScale;
          dl->AddLine(p1, p2, boneCol, lineWidth);
        }
      }

    }

    if (!isOffScreen) {
      float distMeters = p.distance / 39.37f;
      float scale = 0.7f;
      if (distMeters > 150.f) scale = 0.35f;
      else if (distMeters > 100.f) scale = 0.45f;
      else if (distMeters > 50.f) scale = 0.55f;
      else if (distMeters > 25.f) scale = 0.65f;
      
      float tX = x + w + 4.f, tY = y;
      float lineHeight = 13.f * scale;
      
      ImGui::SetWindowFontScale(scale);
      
      if (g_cfg.espName) {
        dl->AddText({tX, tY}, C4(g_cfg.colEspName), p.name);
        tY += lineHeight;
      }
      if (g_cfg.espWeapon) {
        dl->AddText({tX, tY}, C4(g_cfg.colEspWeapon), p.weaponName);
        tY += lineHeight;
      }
      if (g_cfg.espDistance) {
        char b[32];
        snprintf(b, sizeof(b), "[%.1fm]", distMeters);
        dl->AddText({tX, tY}, C4(g_cfg.colEspDist), b);
        tY += lineHeight;
      }
      if (g_cfg.espHealth) {
        char b[32];
        snprintf(b, sizeof(b), "%d", p.health);
        ImU32 healthCol = (p.health > 75) ? IM_COL32(0, 255, 0, 255) : (p.health > 25) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
        dl->AddText({tX, tY}, healthCol, b);
        tY += lineHeight;
      }
      if (g_cfg.espMoney) {
        char b[32];
        snprintf(b, sizeof(b), "$%d", p.money);
        dl->AddText({tX, tY}, IM_COL32(255, 215, 0, 255), b);
        tY += lineHeight;
      }
      if (g_cfg.espFlash && p.flashDuration > 0.f) {
        char b[32];
        snprintf(b, sizeof(b), "FLASH: %.1fs", p.flashDuration);
        dl->AddText({tX, tY}, IM_COL32(255, 255, 255, 255), b);
        tY += lineHeight;
      }
      if (g_cfg.espDefusing && g_c4.defuserHandle == p.pawn) {
        dl->AddText({tX, tY}, IM_COL32(255, 100, 100, 255), "DEFUSING");
      }
      
      ImGui::SetWindowFontScale(1.f);
    }
  }
}
