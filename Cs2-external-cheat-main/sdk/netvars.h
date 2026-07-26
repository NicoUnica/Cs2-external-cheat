#pragma once
#include <cstdint>


namespace netvars {

constexpr uintptr_t m_pGameSceneNode  = 0x330;
constexpr uintptr_t m_iMaxHealth      = 0x348;
constexpr uintptr_t m_iHealth         = 0x34C;
constexpr uintptr_t m_lifeState       = 0x354;
constexpr uintptr_t m_iTeamNum        = 0x3E7;
constexpr uintptr_t m_fFlags          = 0x3F4;

constexpr uintptr_t m_pWeaponServices = 0x1208;
constexpr uintptr_t m_pCameraServices = 0x1240;
constexpr uintptr_t m_vOldOrigin      = 0x13B8;
constexpr uintptr_t m_vecViewOffset   = 0xE78;

constexpr uintptr_t m_flFlashMaxAlpha = 0x1424;
constexpr uintptr_t m_flFlashDuration = 0x1428;

constexpr uintptr_t m_pAimPunchServices   = 0x14B8;
constexpr uintptr_t m_entitySpottedState  = 0x1C58;
constexpr uintptr_t m_iShotsFired         = 0x1C84;
constexpr uintptr_t m_angEyeAngles        = 0x3340;
constexpr uintptr_t m_iIDEntIndex         = 0x341C;

constexpr uintptr_t m_vecAbsOrigin = 0xC8;

constexpr uintptr_t m_modelState  = 0x140;

constexpr uintptr_t m_boneArrayPtr = 0x80;

constexpr uintptr_t m_hActiveWeapon = 0x60;

constexpr uintptr_t m_hPawn             = 0x6BC;
constexpr uintptr_t m_iDesiredFOV       = 0x290;
constexpr uintptr_t m_sSanitizedPlayerName = 0x868;
constexpr uintptr_t m_pInGameMoneyServices = 0x810;
constexpr uintptr_t m_iAccount          = 0x40;

constexpr uintptr_t m_iFOV = 0x290;

constexpr uintptr_t m_aimPunchCache = 0x50;

constexpr uintptr_t m_bSpotted       = 0x8;
constexpr uintptr_t m_bSpottedByMask = 0xC;

constexpr uintptr_t m_AttributeManager  = 0x11A8;
constexpr uintptr_t m_nFallbackPaintKit = 0x1680;
constexpr uintptr_t m_nFallbackSeed     = 0x1684;
constexpr uintptr_t m_flFallbackWear    = 0x1688;
constexpr uintptr_t m_nFallbackStatTrak = 0x168C;

constexpr uintptr_t m_Item = 0x50;

constexpr uintptr_t m_iItemIDHigh = 0x1D0;
constexpr uintptr_t m_iItemIDLow  = 0x1D4;

constexpr uintptr_t m_pObserverServices = 0x1220;
constexpr uintptr_t m_hObserverTarget   = 0x4C;

}
