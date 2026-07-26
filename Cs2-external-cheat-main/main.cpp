#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <timeapi.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "features/config.h"
#include "memory/process.h"
#include "memory/read.h"
#include "overlay/dx11_renderer.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "winmm.lib")

void DrawLogo() {
    std::cout << "\033[97m" << R"(
        __   __  _______  __    _  __   __ 
       |  | |  ||  _    ||  |  | ||  |_|  |
       |  |_|  || |_|   ||   |_| ||       |
       |       ||       ||       ||       |
       |       ||  _    ||  _    | |     | 
       |   _   || |_|   || | |   ||   _   |
       |__| |__||_______||_|  |__||__| |__|
    )" << "\033[0m" << std::endl;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    timeBeginPeriod(1);

    std::cout << "Waiting for game...";
    while (true) {
        if (InitMemory()) break;
        Sleep(1000);
    }
    std::cout << "\rInjected!" << std::endl;
    Sleep(1500);

    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow) { ShowWindow(consoleWindow, SW_HIDE); }
    FreeConsole();

    if (!InitOverlay(hInstance)) {
        CleanupMemory();
        timeEndPeriod(1);
        return 1;
    }

    StartMemoryThread();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) break;
            continue;
        }
        if (GetAsyncKeyState(0x50) & 0x8000) { PostQuitMessage(0); break; }
        DWORD exitCode = 0;
        if (g_hProcess && GetExitCodeProcess(g_hProcess, &exitCode) && exitCode != STILL_ACTIVE) break;
        RenderFrame();
    }

    StopMemoryThread();
    ShutdownOverlay();
    CleanupMemory();
    timeEndPeriod(1);
    return 0;
}
