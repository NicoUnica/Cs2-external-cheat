#pragma once
#include <Windows.h>
#include <cstdint>
#include <Psapi.h>
#include <vector>
#include <string>

extern HANDLE g_hProcess;
extern uintptr_t g_clientBase;
extern int g_screenW;
extern int g_screenH;
extern HWND g_csgoHwnd;

DWORD GetProcessIdByName(const wchar_t *procName);
HANDLE OpenGameProcess(DWORD pid);
uintptr_t GetModuleBase(HANDLE hProc, const wchar_t *modName);
bool InitMemory();
void UpdateOverlayPosition(HWND overlayHwnd);
bool IsGameForeground();
void CleanupMemory();
uintptr_t SigScan(uintptr_t moduleBase, const char *pattern);
