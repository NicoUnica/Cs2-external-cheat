#pragma once
#include "../sdk/offsets.h"
#include "../sdk/structs.h"
#include <Windows.h>
#include <cstdint>
#include <mutex>
#include <string>

extern PlayerData g_players[65];
extern LocalPlayer g_localPlayer;
extern ViewMatrix g_viewMatrix;
extern C4Data g_c4;
extern std::mutex g_dataMutex;

template <typename T> T RPM(uintptr_t address) {
  T buffer{};
  ReadProcessMemory(g_hProcess, (LPCVOID)address, &buffer, sizeof(T), nullptr);
  return buffer;
}

template <typename T> bool RPM_Safe(uintptr_t address, T &out) {
  if (!address)
    return false;
  return ReadProcessMemory(g_hProcess, (LPCVOID)address, &out, sizeof(T),
                           nullptr) != 0;
}

template <typename T> void WPM(uintptr_t address, T value) {
  WriteProcessMemory(g_hProcess, (LPVOID)address, &value, sizeof(T), nullptr);
}

std::string RPM_String(uintptr_t address, size_t maxLen = 128);

void StartMemoryThread();
void StopMemoryThread();

void UpdateEntityCache();
void UpdateC4Data();
