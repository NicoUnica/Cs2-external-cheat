#pragma once
#include <cstdint>


namespace netvars {

constexpr uintptr_t m_pGameSceneNode  = 0x330;
constexpr uintptr_t m_iMaxHealth      = 0x348;
constexpr uintptr_t m_iHealth         = 0x34C;
constexpr uintptr_t m_lifeState       = 0x354;
constexpr uintptr_t m_iTeamNum        = 0x3EB;
constexpr uintptr_t m_fFlags          = 0x3F8;

constexpr uintptr_t m_pWeaponServices = 0x11E0;
constexpr uintptr_t m_pCameraServices = 0x1218;
constexpr uintptr_t m_vOldOrigin      = 0x1390;
constexpr uintptr_t m_vecViewOffset   = 0xE70;

constexpr uintptr_t m_flFlashMaxAlpha = 0x13FC;
constexpr uintptr_t m_flFlashDuration = 0x1400;

constexpr uintptr_t m_pAimPunchServices   = 0x1490;
constexpr uintptr_t m_entitySpottedState  = 0x1CC8;
constexpr uintptr_t m_iShotsFired         = 0x1C64;
constexpr uintptr_t m_angEyeAngles        = 0x3320;
constexpr uintptr_t m_iIDEntIndex         = 0x33FC;

constexpr uintptr_t m_vecAbsOrigin = 0xC8;

constexpr uintptr_t m_modelState  = 0x150;

constexpr uintptr_t m_boneArrayPtr = 0x80;

constexpr uintptr_t m_hActiveWeapon = 0x60;

constexpr uintptr_t m_hPawn             = 0x6BC;
constexpr uintptr_t m_iDesiredFOV       = 0x78C;
constexpr uintptr_t m_sSanitizedPlayerName = 0x860;
constexpr uintptr_t m_pInGameMoneyServices = 0x808;
constexpr uintptr_t m_iAccount          = 0x40;

constexpr uintptr_t m_iFOV = 0x290;

constexpr uintptr_t m_aimPunchCache = 0x50;

constexpr uintptr_t m_bSpotted       = 0x8;
constexpr uintptr_t m_bSpottedByMask = 0xC;

constexpr uintptr_t m_AttributeManager  = 0x1180;
constexpr uintptr_t m_nFallbackPaintKit = 0x1658;
constexpr uintptr_t m_nFallbackSeed     = 0x165C;
constexpr uintptr_t m_flFallbackWear    = 0x1660;
constexpr uintptr_t m_nFallbackStatTrak = 0x1664;

constexpr uintptr_t m_Item = 0x50;

constexpr uintptr_t m_iItemIDHigh = 0x1D0;
constexpr uintptr_t m_iItemIDLow  = 0x1D4;

constexpr uintptr_t m_bBombTicking     = 0x1160;
constexpr uintptr_t m_nBombSite        = 0x1164;
constexpr uintptr_t m_flC4Blow         = 0x1190;
constexpr uintptr_t m_bHasExploded     = 0x1195;
constexpr uintptr_t m_flTimerLength    = 0x1198;
constexpr uintptr_t m_bBeingDefused    = 0x119C;
constexpr uintptr_t m_flDefuseLength   = 0x11AC;
constexpr uintptr_t m_flDefuseCountDown= 0x11B0;
constexpr uintptr_t m_bBombDefused     = 0x11B4;
constexpr uintptr_t m_hBombDefuser     = 0x11B8;

}
