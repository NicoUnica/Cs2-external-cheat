#include "process.h"
#include <TlHelp32.h>
#include <iostream>
#include <vector>

HANDLE g_hProcess = nullptr;
uintptr_t g_clientBase = 0;
int g_screenW = 1920;
int g_screenH = 1080;
HWND g_csgoHwnd = nullptr;

DWORD GetProcessIdByName(const wchar_t *procName) {
  DWORD pid = 0;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return 0;
  PROCESSENTRY32W pe;
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, procName) == 0) {
        pid = pe.th32ProcessID;
        break;
      }
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
  return pid;
}

HANDLE OpenGameProcess(DWORD pid) {
  return OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                         PROCESS_QUERY_INFORMATION,
                     FALSE, pid);
}

uintptr_t GetModuleBase(HANDLE hProc, const wchar_t *modName) {
  HANDLE snap = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(hProc));
  if (snap == INVALID_HANDLE_VALUE)
    return 0;
  MODULEENTRY32W me;
  me.dwSize = sizeof(me);
  uintptr_t base = 0;
  if (Module32FirstW(snap, &me)) {
    do {
      if (_wcsicmp(me.szModule, modName) == 0) {
        base = (uintptr_t)me.modBaseAddr;
        break;
      }
    } while (Module32NextW(snap, &me));
  }
  CloseHandle(snap);
  return base;
}

bool InitMemory() {
  DWORD pid = GetProcessIdByName(L"cs2.exe");
  if (!pid)
    return false;

  g_hProcess = OpenGameProcess(pid);
  if (!g_hProcess || g_hProcess == INVALID_HANDLE_VALUE)
    return false;

  g_clientBase = GetModuleBase(g_hProcess, L"client.dll");
  if (!g_clientBase) {
    CleanupMemory();
    return false;
  }

  for (int i = 0; i < 10; i++) {
    g_csgoHwnd = FindWindowW(L"SDL_app", L"Counter-Strike 2");
    if (!g_csgoHwnd) g_csgoHwnd = FindWindowW(nullptr, L"Counter-Strike 2");
    
    if (g_csgoHwnd) break;
    
    struct EnumData { DWORD pid; HWND hwnd; };
    EnumData data = { pid, nullptr };
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
      EnumData* pData = (EnumData*)lp;
      DWORD windowPid;
      GetWindowThreadProcessId(hwnd, &windowPid);
      if (windowPid == pData->pid && IsWindowVisible(hwnd)) {
        pData->hwnd = hwnd;
        return FALSE; 
      }
      return TRUE;
    }, (LPARAM)&data);
    
    if (data.hwnd) {
        g_csgoHwnd = data.hwnd;
        break;
    }
    Sleep(500);
  }

  if (!g_csgoHwnd) return false;

  return true;
}

// Update overlay to match game window
void UpdateOverlayPosition(HWND overlayHwnd) {
  if (!g_csgoHwnd) {
    g_csgoHwnd = FindWindowW(nullptr, L"Counter-Strike 2");
    if (!g_csgoHwnd)
      return;
  }

  RECT r;
  if (!GetWindowRect(g_csgoHwnd, &r))
    return;
  int w = r.right - r.left;
  int h = r.bottom - r.top;
  if (w <= 0 || h <= 0)
    return;

  g_screenW = w;
  g_screenH = h;

  SetWindowPos(overlayHwnd, HWND_TOPMOST, r.left, r.top, w, h, SWP_NOACTIVATE);
}

bool IsGameForeground() {
  HWND fg = GetForegroundWindow();
  if (!fg) return false;
  
  if (fg == g_csgoHwnd) return true;

  DWORD fgPid;
  GetWindowThreadProcessId(fg, &fgPid);
  if (fgPid == GetCurrentProcessId()) return true;
  
  return false;
}

void CleanupMemory() {
  if (g_hProcess && g_hProcess != INVALID_HANDLE_VALUE)
    CloseHandle(g_hProcess);
  g_hProcess = nullptr;
  g_clientBase = 0;
}

uintptr_t SigScan(uintptr_t moduleBase, const char *pattern) {
    auto patternToByte = [](const char* pattern) {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?') ++current;
                bytes.push_back(-1);
            } else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    };

    MODULEINFO modInfo;
    GetModuleInformation(g_hProcess, (HMODULE)moduleBase, &modInfo, sizeof(MODULEINFO));
    
    auto patternBytes = patternToByte(pattern);
    std::vector<uint8_t> moduleBytes(modInfo.SizeOfImage);
    ReadProcessMemory(g_hProcess, (LPCVOID)moduleBase, moduleBytes.data(), modInfo.SizeOfImage, nullptr);

    for (size_t i = 0; i < modInfo.SizeOfImage - patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (moduleBytes[i + j] != patternBytes[j] && patternBytes[j] != -1) {
                found = false;
                break;
            }
        }
        if (found) return moduleBase + i;
    }
    return 0;
}
