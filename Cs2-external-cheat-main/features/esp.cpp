#include "esp.h"
#include "../memory/process.h"
#include "../memory/read.h"
#include "../sdk/math.h"
#include "config.h"
#include "imgui.h"
#include "../sdk/netvars.h"
#include "../sdk/offsets.hpp"
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <mutex>

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


static inline ImU32 C4(const float *c) {
  return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255),
                  (int)(c[3] * 255));
}

void DrawSpectatorList() {
    std::vector<std::string> spectators;
    
    ViewMatrix vm;
    LocalPlayer lp;
    PlayerData players[65];
    {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        vm = g_viewMatrix;
        lp = g_localPlayer;
        memcpy(players, g_players, sizeof(players));
    }
    
    if (!lp.isValid) return;
    
    uintptr_t entityList = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwEntityList);
    if (!entityList) return;
    
    uintptr_t localController = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwLocalPlayerController);
    uintptr_t localPawn = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwLocalPlayerPawn);
    
    uint32_t localHandlePlayer = 0;
    uint32_t localHandlePawn = 0;
    if (localController) {
        localHandlePlayer = RPM<uint32_t>(localController + netvars::m_hPawn);
        localHandlePawn = RPM<uint32_t>(localController + netvars::m_hPawn);
    }
    
    for (int i = 1; i <= 64; i++) {
        uintptr_t entry = RPM<uintptr_t>(entityList + 0x10 + 8 * (i >> 9));
        if (!entry) continue;
        
        uintptr_t controller = RPM<uintptr_t>(entry + 0x70 * (i & 0x1FF));
        if (!controller || controller == localController) continue;
        
        char name[128]{};
        uintptr_t namePtr = RPM<uintptr_t>(controller + netvars::m_sSanitizedPlayerName);
        if (namePtr) ReadProcessMemory(g_hProcess, (LPCVOID)namePtr, name, 127, nullptr);
        if (!name[0]) continue;
        
        uint32_t pawnHandle = RPM<uint32_t>(controller + netvars::m_hPawn);
        if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;
        
        uint32_t pIdx = pawnHandle & 0x7FFF;
        uintptr_t pEntry = RPM<uintptr_t>(entityList + 0x10 + 8 * (pIdx >> 9));
        if (!pEntry) continue;
        uintptr_t pawn = RPM<uintptr_t>(pEntry + 0x70 * (pIdx & 0x1FF));
        if (!pawn || pawn == localPawn) continue;
        
        int health = RPM<int>(pawn + netvars::m_iHealth);
        int team = RPM<int>(controller + netvars::m_iTeamNum);
        if (health > 0 && team != 1) continue;
        
        uintptr_t obsSvc = RPM<uintptr_t>(pawn + netvars::m_pObserverServices);
        if (!obsSvc) continue;
        
        uint32_t obsTarget = RPM<uint32_t>(obsSvc + netvars::m_hObserverTarget);
        if (!obsTarget || obsTarget == 0xFFFFFFFF) continue;
        
        bool match = false;
        if (localHandlePawn && obsTarget == localHandlePawn) match = true;
        if (!match && localHandlePlayer && obsTarget == localHandlePlayer) match = true;
        
        if (match) {
            bool dup = false;
            for (const auto& s : spectators)
                if (s == name) { dup = true; break; }
            if (!dup) spectators.push_back(name);
        }
    }
    
    if (spectators.empty()) return;
    
    const float MW = 180.f;
    const float PH = 35.f + 10.f + spectators.size() * 16.f; // Header + spacing + names (no extra padding)
    
    ImGui::SetNextWindowSize({MW, PH}, ImGuiCond_Always);
    ImGui::SetNextWindowPos({10.f, (g_screenH - PH) / 2.f}, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.f, 10.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.f, 6.f});
    ImGui::Begin("##spectators", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    ImGui::PopStyleVar(2);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();

    // Header background
    dl->AddRectFilled({wp.x, wp.y}, {wp.x + ws.x, wp.y + 35}, IM_COL32(15, 15, 20, 255));
    dl->AddRectFilledMultiColor({wp.x, wp.y}, {wp.x + ws.x, wp.y + 2},
        IM_COL32(153, 51, 204, 255), IM_COL32(204, 102, 255, 255),
        IM_COL32(204, 102, 255, 255), IM_COL32(153, 51, 204, 255));
    
    char title[64];
    snprintf(title, sizeof(title), "Spectators (%d)", (int)spectators.size());
    
    // Center title
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = wp.x + (ws.x - titleSize.x) / 2.f;
    dl->AddText(ImGui::GetFont(), 14.f, {titleX, wp.y + 8}, IM_COL32(255, 255, 255, 255), title);

    ImGui::Dummy({0, 10});
    
    for (const auto& s : spectators) {
        ImGui::Text("%s", s.c_str());
    }
    
    ImGui::End();
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

  ImDrawList *dl = ImGui::GetBackgroundDrawList();

  // Pre-calculate bone connections
  struct BoneConnection { int a, b; };
  static const BoneConnection connections[] = {
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

  // DragonBurn-style depth scaling constants
  constexpr float DEPTH_SCALE = 800.0f;  // Base depth scale factor
  constexpr float MIN_SCALE = 0.4f;       // Minimum scale
  constexpr float MAX_SCALE = 1.3f;       // Maximum scale
  constexpr float DISTANCE_DROPOFF = 0.7f; // How much distance affects scaling

  for (int i = 1; i <= 64; i++) {
    const PlayerData &p = players[i];
    if (!p.isValid || !p.isAlive || p.distance > 10000.f)
      continue;

    bool isTeammate = (lp.team >= 2 && p.team >= 2 && p.team == lp.team);
    if (g_cfg.teamCheck && isTeammate)
      continue;

    // Get head and foot positions
    Vector3 topW = p.bonePositions[HEAD];
    if (topW.IsZero()) {
        topW = p.origin;
        topW.z += 75.f;
    }

    Vector3 botW = p.origin;
    botW.z -= 5.f;

    Vector2 headS, footS;
    bool headInFront = WorldToScreen(topW, headS, vm, g_screenW, g_screenH);
    bool footInFront = WorldToScreen(botW, footS, vm, g_screenW, g_screenH);

    // Skip if both are behind camera
    if (!headInFront && !footInFront) continue;

    // Calculate box dimensions
    float h, w, x, y;
    
    if (headInFront && footInFront) {
      h = footS.y - headS.y;
      w = h * 0.45f;
      x = footS.x - (w * 0.5f);
      y = headS.y;
    } else if (headInFront) {
      h = 100.f;
      w = h * 0.45f;
      x = headS.x - (w * 0.5f);
      y = headS.y;
    } else {
      h = 100.f;
      w = h * 0.45f;
      x = footS.x - (w * 0.5f);
      y = footS.y - h;
    }

    // Clamp box dimensions
    h = fmaxf(20.f, fminf(500.f, h));
    w = h * 0.45f;

    // Check if box is off screen
    bool isOffScreen = (x + w < 0 || x > g_screenW || y + h < 0 || y > g_screenH);
    if (isOffScreen) continue;

    // DragonBurn-style smooth depth-based scaling
    float distMeters = p.distance / 39.37f;
    float avgDepth = fmaxf(1.0f, distMeters * 10.0f); // Convert to approximate depth units
    
    // Calculate raw scale factor based on depth
    float rawFactor = DEPTH_SCALE / avgDepth;
    rawFactor = fmaxf(0.1f, fminf(3.0f, rawFactor));
    
    // Apply smooth distance dropoff
    float scale = 1.0f + (rawFactor - 1.0f) * DISTANCE_DROPOFF;
    scale = fmaxf(MIN_SCALE, fminf(MAX_SCALE, scale));
    
    // Calculate font scale and line width
    float fontScale = 0.65f * scale; // Reduced text size
    float lineWidth = 1.5f * scale;
    
    // Calculate opacity based on distance (fade out at very far distances)
    float opacity = 1.0f;
    if (distMeters > 200.0f) {
      opacity = fmaxf(0.3f, 1.0f - (distMeters - 200.0f) / 300.0f);
    }
    
    ImGui::SetWindowFontScale(fontScale);
    float lineHeight = 14.f * fontScale;

    // Draw skeleton (only if enabled and bones are valid)
    if (g_cfg.espBones) {
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

      if (hasValidBones) {
        ImU32 boneCol = C4(g_cfg.colEspBone);
        // Apply opacity to bone color
        boneCol = (boneCol & 0x00FFFFFF) | ((ImU32)(opacity * 255) << 24);
        
        for (const auto& conn : connections) {
          if (validBonesScreen[conn.a] && validBonesScreen[conn.b]) {
            float dx = p.bonePositions[conn.a].x - p.bonePositions[conn.b].x;
            float dy = p.bonePositions[conn.a].y - p.bonePositions[conn.b].y;
            float dz = p.bonePositions[conn.a].z - p.bonePositions[conn.b].z;
            if ((dx*dx + dy*dy + dz*dz) > 2500.f) continue;

            ImVec2 p1 = {screenBones[conn.a].x, screenBones[conn.a].y};
            ImVec2 p2 = {screenBones[conn.b].x, screenBones[conn.b].y};

            dl->AddLine(p1, p2, boneCol, lineWidth);
          }
        }
      }
    }

    // Draw text info
    float centerX = x + w / 2;
    
    // Clamp center position
    centerX = fmaxf(10.f, fminf(g_screenW - 10.f, centerX));
    
    // Helper to apply opacity to color
    auto applyOpacity = [](ImU32 col, float op) -> ImU32 {
      return (col & 0x00FFFFFF) | ((ImU32)(op * 255) << 24);
    };
    
    // Draw all info to the right of the box
    float tX = x + w + 5.f, tY = y;
    
    // Draw name first (at the top right)
    if (g_cfg.espName) {
      ImU32 nameCol = applyOpacity(C4(g_cfg.colEspName), opacity);
      dl->AddText({tX, tY}, nameCol, p.name);
      tY += lineHeight;
    }
    
    if (g_cfg.espWeapon) {
      ImU32 weaponCol = applyOpacity(C4(g_cfg.colEspWeapon), opacity);
      dl->AddText({tX, tY}, weaponCol, p.weaponName);
      tY += lineHeight;
    }
    if (g_cfg.espDistance) {
      char b[32];
      snprintf(b, sizeof(b), "%.1fm", distMeters);
      ImU32 distCol = applyOpacity(C4(g_cfg.colEspDist), opacity);
      dl->AddText({tX, tY}, distCol, b);
      tY += lineHeight;
    }
    if (g_cfg.espHealth) {
      char b[32];
      snprintf(b, sizeof(b), "%d HP", p.health);
      ImU32 healthCol = (p.health > 75) ? IM_COL32(0, 255, 0, 255) : (p.health > 25) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
      healthCol = applyOpacity(healthCol, opacity);
      dl->AddText({tX, tY}, healthCol, b);
      tY += lineHeight;
    }
    if (g_cfg.espMoney) {
      char b[32];
      snprintf(b, sizeof(b), "$%d", p.money);
      ImU32 moneyCol = applyOpacity(IM_COL32(255, 215, 0, 255), opacity);
      dl->AddText({tX, tY}, moneyCol, b);
      tY += lineHeight;
    }
    if (g_cfg.espFlash && p.flashDuration > 0.f) {
      char b[32];
      snprintf(b, sizeof(b), "FLASH %.1fs", p.flashDuration);
      ImU32 flashCol = applyOpacity(IM_COL32(255, 255, 255, 255), opacity);
      dl->AddText({tX, tY}, flashCol, b);
      tY += lineHeight;
    }
    
    ImGui::SetWindowFontScale(1.f);
  }
}
