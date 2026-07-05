#include "read.h"
#include "../features/config.h"
#include "../sdk/math.h"
#include "../sdk/netvars.h"
#include "process.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <ctime>

PlayerData g_players[65] = {};
LocalPlayer g_localPlayer = {};
ViewMatrix g_viewMatrix = {};
C4Data g_c4 = {};
std::mutex g_dataMutex;

static std::atomic<bool> g_memRunning(false);
static std::thread g_memThread;

extern HANDLE g_hProcess;
extern uintptr_t g_clientBase;

static void ReadWeaponName(uintptr_t pawn, char *out, size_t maxLen) {
    out[0] = 0;
    if (!pawn) return;

    uintptr_t weapSvc = RPM<uintptr_t>(pawn + netvars::m_pWeaponServices);
    if (!weapSvc) return;

    uint32_t weapHandle = RPM<uint32_t>(weapSvc + netvars::m_hActiveWeapon);
    if (!weapHandle || weapHandle == 0xFFFFFFFF) return;

    uintptr_t entList = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwEntityList);
    if (!entList) return;

    uint32_t idx = weapHandle & 0x7FFF;
    uintptr_t listEntry = RPM<uintptr_t>(entList + 0x10 + 8 * (idx >> 9));
    if (!listEntry) return;

    uintptr_t weapEntity = RPM<uintptr_t>(listEntry + 0x70 * (idx & 0x1FF));
    if (!weapEntity) return;

    uintptr_t identity = RPM<uintptr_t>(weapEntity + 0x10);
    if (!identity) return;

    uintptr_t namePtr = RPM<uintptr_t>(identity + 0x20);
    if (!namePtr) return;

    char raw[128] = {};
    ReadProcessMemory(g_hProcess, (LPCVOID)namePtr, raw, 127, nullptr);
    raw[127] = 0;

    const char *src = raw;
    if (strncmp(src, "weapon_", 7) == 0) src += 7;

    struct { const char *from; const char *to; } remap[] = {
        {"ak47","AK-47"},{"m4a1","M4A4"},{"m4a1_silencer","M4A1-S"},
        {"awp","AWP"},{"deagle","Desert Eagle"},{"glock","Glock-18"},
        {"usp_silencer","USP-S"},{"hkp2000","P2000"},{"p250","P250"},
        {"fiveseven","Five-SeveN"},{"tec9","Tec-9"},{"cz75a","CZ75-Auto"},
        {"revolver","R8 Revolver"},{"elite","Dual Berettas"},
        {"mp9","MP9"},{"mac10","MAC-10"},{"mp5sd","MP5-SD"},{"ump45","UMP-45"}
    };
    for (int i = 0; i < (sizeof(remap)/sizeof(remap[0])); i++) {
        if (_stricmp(src, remap[i].from) == 0) {
            strncpy_s(out, maxLen, remap[i].to, maxLen-1);
            return;
        }
    }
    strncpy_s(out, maxLen, src, maxLen-1);
    if (out[0] >= 'a' && out[0] <= 'z') out[0] -= 32;
}

struct CachedEntity {
    uint32_t pawnHandle = 0;
    uintptr_t pawnPtr = 0;
    uintptr_t controller = 0;
};
static CachedEntity g_cachedEnts[65] = {};

void UpdateEntityCache() {
    if (!g_hProcess || !g_clientBase) return;

    uintptr_t entityList = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwEntityList);
    if (!entityList) return;

    uintptr_t lpController = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwLocalPlayerController);
    uintptr_t lpPawn = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwLocalPlayerPawn);

    LocalPlayer lp = {};
    if (lpPawn) {
        lp.pawn = lpPawn;
        lp.health = RPM<int>(lpPawn + netvars::m_iHealth);
        lp.team = RPM<uint8_t>(lpController + netvars::m_iTeamNum);
        lp.origin = RPM<Vector3>(lpPawn + netvars::m_vOldOrigin);
        lp.vecViewOffset = RPM<Vector3>(lpPawn + netvars::m_vecViewOffset);
        lp.flags = RPM<uint32_t>(lpPawn + netvars::m_fFlags);
        lp.isValid = (lp.health > 0 && lp.health <= 100);
        lp.viewAngles = RPM<Vector3>(lpPawn + netvars::m_angEyeAngles);

        uintptr_t punchSvc = RPM<uintptr_t>(lpPawn + netvars::m_pAimPunchServices);
        if (punchSvc)
            lp.aimPunch = RPM<Vector3>(punchSvc + netvars::m_aimPunchCache);
        else
            lp.aimPunch = {0,0,0};

        lp.crosshairId = RPM<uint32_t>(lpPawn + netvars::m_iIDEntIndex);

        uintptr_t camServices = RPM<uintptr_t>(lpPawn + netvars::m_pCameraServices);
        if (camServices)
            lp.fov = (float)RPM<uint32_t>(camServices + netvars::m_iFOV);
        if (lp.fov < 10.f || lp.fov > 170.f) lp.fov = 90.f;
    }

    static uintptr_t lastLpController = 0;
    if (lpController != lastLpController || lp.playerIndex == -1) {
        lp.playerIndex = -1;
        for (int i = 1; i <= 64; i++) {
            uintptr_t entry = RPM<uintptr_t>(entityList + 0x10 + 8 * (i >> 9));
            if (!entry) continue;
            uintptr_t ctrl = RPM<uintptr_t>(entry + 0x70 * (i & 0x1FF));
            if (ctrl && ctrl == lpController) {
                lp.playerIndex = i;
                lastLpController = lpController;
                break;
            }
        }
    }

    long long currentMs = (long long)GetTickCount64();
    PlayerData newPlayers[65] = {};

    for (int i = 1; i <= 64; i++) {
        uintptr_t entry = RPM<uintptr_t>(entityList + 0x10 + 8 * (i >> 9));
        if (!entry) continue;

        uintptr_t controller = RPM<uintptr_t>(entry + 0x70 * (i & 0x1FF));
        if (!controller || controller == lpController) continue;

        uint32_t pawnHandle = RPM<uint32_t>(controller + netvars::m_hPawn);
        if (!pawnHandle || pawnHandle == 0xFFFFFFFF) {
            g_cachedEnts[i] = {0,0,0};
            continue;
        }

        uintptr_t pawn = 0;
        if (g_cachedEnts[i].pawnHandle == pawnHandle && g_cachedEnts[i].pawnPtr != 0) {
            pawn = g_cachedEnts[i].pawnPtr;
        } else {
            uint32_t pIdx = pawnHandle & 0x7FFF;
            uintptr_t pEntry = RPM<uintptr_t>(entityList + 0x10 + 8 * (pIdx >> 9));
            if (pEntry) {
                pawn = RPM<uintptr_t>(pEntry + 0x70 * (pIdx & 0x1FF));
                g_cachedEnts[i] = {pawnHandle, pawn, controller};
            }
        }
        if (!pawn) continue;

        PlayerData &p = newPlayers[i];
        p.pawn = pawn;
        p.controller = controller;
        struct { int hp; char pad[4]; uint8_t life; } b1;
        if (ReadProcessMemory(g_hProcess, (LPCVOID)(pawn + netvars::m_iHealth), &b1, sizeof(b1), nullptr)) {
            p.health = b1.hp;
            p.isAlive = (b1.life == 0 && b1.hp > 0);
            p.isValid = (b1.hp > 0 && b1.hp <= 100);
        }
        p.team = RPM<uint8_t>(controller + netvars::m_iTeamNum);
        uintptr_t moneyServices = RPM<uintptr_t>(controller + netvars::m_pInGameMoneyServices);
        p.money = moneyServices ? RPM<int>(moneyServices + netvars::m_iAccount) : 0;
        p.flashDuration = RPM<float>(pawn + netvars::m_flFlashDuration);
        p.isDefusing = false;
        if (!p.isValid) continue;

        uint32_t masks[2];
        uintptr_t spottedState = pawn + netvars::m_entitySpottedState;
        if (ReadProcessMemory(g_hProcess, (LPCVOID)(spottedState + netvars::m_bSpottedByMask), masks, sizeof(masks), nullptr)) {
            p.spottedMask[0] = masks[0];
            p.spottedMask[1] = masks[1];
            if (lp.playerIndex > 0) {
                int bit = lp.playerIndex - 1;
                p.isVisible = (masks[bit / 32] & (1 << (bit % 32)));
            } else {
                p.isVisible = (masks[0] != 0);
            }
        }
        if (p.isVisible) p.lastVisibleMs = currentMs;

        p.origin = RPM<Vector3>(pawn + netvars::m_vOldOrigin);
        p.flags = RPM<uint32_t>(pawn + netvars::m_fFlags);
        if (lp.isValid) p.distance = Distance3D(lp.origin, p.origin);

        static int cycle = 0;
        if (cycle % 4 == 0) {
            uintptr_t namePtr = RPM<uintptr_t>(controller + netvars::m_sSanitizedPlayerName);
            if (namePtr) ReadProcessMemory(g_hProcess, (LPCVOID)namePtr, p.name, 127, nullptr);
            ReadWeaponName(pawn, p.weaponName, sizeof(p.weaponName));
        } else {
            std::lock_guard<std::mutex> lock(g_dataMutex);
            strncpy_s(p.name, g_players[i].name, 127);
            strncpy_s(p.weaponName, g_players[i].weaponName, 31);
        }

        uintptr_t gameSN = RPM<uintptr_t>(pawn + netvars::m_pGameSceneNode);
        if (gameSN) {
            uintptr_t boneArr = RPM<uintptr_t>(gameSN + netvars::m_modelState + netvars::m_boneArrayPtr);
            if (boneArr) {
                struct BoneData { Vector3 pos; char pad[20]; } bones[64];
                if (ReadProcessMemory(g_hProcess, (LPCVOID)boneArr, bones, sizeof(bones), nullptr)) {
                    for (int b = 0; b < 64; b++) {
                        if (bones[b].pos.IsZero()) { p.bonePositions[b] = {0,0,0}; continue; }
                        float dx = bones[b].pos.x - p.origin.x;
                        float dy = bones[b].pos.y - p.origin.y;
                        float dz = bones[b].pos.z - p.origin.z;
                        if ((dx*dx + dy*dy + dz*dz) < 14400.f)
                            p.bonePositions[b] = bones[b].pos;
                        else
                            p.bonePositions[b] = {0,0,0};
                    }
                    p.headPos = p.bonePositions[HEAD];
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(g_dataMutex);
    g_localPlayer = lp;
    memcpy(g_players, newPlayers, sizeof(newPlayers));
}

void UpdateC4Data() {
    uintptr_t listBase = RPM<uintptr_t>(g_clientBase + game_offsets::offsets::client_dll::dwPlantedC4);
    if (!listBase) {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        g_c4 = {};
        return;
    }

    uintptr_t c4Ent = RPM<uintptr_t>(listBase);
    if (!c4Ent) {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        g_c4 = {};
        return;
    }

    bool isTicking = RPM<bool>(c4Ent + netvars::m_bBombTicking);
    if (!isTicking) {
        std::lock_guard<std::mutex> lock(g_dataMutex);
        g_c4 = {};
        return;
    }

    C4Data data;
    data.isPlanted       = true;
    data.isTicking       = isTicking;
    data.bombSite        = RPM<int>  (c4Ent + netvars::m_nBombSite);
    data.blowTime        = RPM<float>(c4Ent + netvars::m_flC4Blow);
    data.timerLength     = RPM<float>(c4Ent + netvars::m_flTimerLength);
    data.hasExploded     = RPM<bool> (c4Ent + netvars::m_bHasExploded);
    data.isBeingDefused  = RPM<bool> (c4Ent + netvars::m_bBeingDefused);
    data.defuseLength    = RPM<float>(c4Ent + netvars::m_flDefuseLength);
    data.defuseCountDown = RPM<float>(c4Ent + netvars::m_flDefuseCountDown);
    data.isDefused       = RPM<bool> (c4Ent + netvars::m_bBombDefused);
    data.defuserHandle   = RPM<uint32_t>(c4Ent + netvars::m_hBombDefuser);

    uintptr_t sceneNode = RPM<uintptr_t>(c4Ent + netvars::m_pGameSceneNode);
    if (sceneNode) data.origin = RPM<Vector3>(sceneNode + netvars::m_vecAbsOrigin);

    std::lock_guard<std::mutex> lock(g_dataMutex);
    g_c4 = data;
}

static void MemoryThreadFunc() {
    while (g_memRunning.load()) {
        UpdateEntityCache();
        UpdateC4Data();
        Sleep(10 + (rand() % 7));
    }
}

void StartMemoryThread() {
    srand(static_cast<unsigned int>(time(nullptr)));
    g_memRunning = true;
    g_memThread = std::thread([]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        MemoryThreadFunc();
    });
}

void StopMemoryThread() {
    g_memRunning = false;
    if (g_memThread.joinable()) g_memThread.join();
}
