#include "dx11_renderer.h"
#include "../features/config.h"
#include "../features/esp.h"
#include "../memory/process.h"
#include "../memory/read.h"
#include "../sdk/offsets.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

HWND g_overlayHwnd = nullptr;
ID3D11Device *g_pd3dDevice = nullptr;
ID3D11DeviceContext *g_pd3dContext = nullptr;
IDXGISwapChain *g_pSwapChain = nullptr;
static ID3D11RenderTargetView *g_mainRTV = nullptr;

Config g_cfg;
static ImFont* g_fontBold    = nullptr;
static ImFont* g_fontDefault = nullptr;

extern int g_screenW;
extern int g_screenH;
extern HWND g_csgoHwnd;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pSwapChain && wParam != SIZE_MINIMIZED) ResizeRenderTarget();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool CreateDeviceAndSwapChain() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = (UINT)g_screenW;
    sd.BufferDesc.Height = (UINT)g_screenH;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_overlayHwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 2, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, &got, &g_pd3dContext);
    return SUCCEEDED(hr);
}

static void CreateRenderTarget() {
    ID3D11Texture2D *bb = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (bb) {
        g_pd3dDevice->CreateRenderTargetView(bb, nullptr, &g_mainRTV);
        bb->Release();
    }
}

void ResizeRenderTarget() {
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
    g_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
}

bool InitOverlay(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"hbnxOverlay";
    RegisterClassExW(&wc);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    g_overlayHwnd = CreateWindowExW(exStyle, L"hbnxOverlay", L"hbnx.wtf",
        WS_POPUP, 0, 0, g_screenW, g_screenH, nullptr, nullptr, hInstance, nullptr);
    if (!g_overlayHwnd) return false;

    SetLayeredWindowAttributes(g_overlayHwnd, 0, 255, LWA_ALPHA);
    MARGINS m = {-1};
    DwmExtendFrameIntoClientArea(g_overlayHwnd, &m);

    if (!CreateDeviceAndSwapChain()) return false;
    CreateRenderTarget();
    ShowWindow(g_overlayHwnd, SW_SHOW);
    UpdateWindow(g_overlayHwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.ConfigDebugIsDebuggerPresent = false;
    io.ConfigDebugHighlightIdConflicts = false;
    io.ConfigDebugHighlightIdConflictsShowItemPicker = false;
    io.ConfigDebugBeginReturnValueOnce = false;

    char fontPath[MAX_PATH];
    GetWindowsDirectoryA(fontPath, MAX_PATH);
    strcat_s(fontPath, "\\Fonts\\verdana.ttf");
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 3;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH  = false;
    g_fontDefault = io.Fonts->AddFontFromFileTTF(fontPath, 14.0f, &fontCfg);
    if (!g_fontDefault) g_fontDefault = io.Fonts->AddFontDefault();

    char fontPathB[MAX_PATH];
    GetWindowsDirectoryA(fontPathB, MAX_PATH);
    strcat_s(fontPathB, "\\Fonts\\segoeuib.ttf");
    ImFontConfig boldCfg;
    boldCfg.OversampleH = 3;
    boldCfg.OversampleV = 2;
    g_fontBold = io.Fonts->AddFontFromFileTTF(fontPathB, 14.0f, &boldCfg);
    if (!g_fontBold) g_fontBold = g_fontDefault;

    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding        = 0.f;
    s.FrameRounding         = 0.f;
    s.PopupRounding         = 0.f;
    s.ChildRounding         = 0.f;
    s.ScrollbarRounding     = 0.f;
    s.GrabRounding          = 0.f;
    s.WindowPadding         = {0.f, 0.f};
    s.FramePadding          = {6.f, 4.f};
    s.ItemSpacing           = {6.f, 4.f};
    s.ItemInnerSpacing      = {4.f, 3.f};
    s.ScrollbarSize         = 4.f;
    s.GrabMinSize           = 8.f;
    s.WindowBorderSize      = 0.f;
    s.FrameBorderSize       = 0.f;
    s.ChildBorderSize       = 0.f;
    s.PopupBorderSize       = 0.f;
    s.AntiAliasedLines      = true;
    s.AntiAliasedFill       = true;
    s.Alpha                 = 1.f;

    ImVec4 *c = s.Colors;
    c[ImGuiCol_Text]              = {0.88f, 0.88f, 0.95f, 1.f};
    c[ImGuiCol_TextDisabled]      = {0.42f, 0.42f, 0.50f, 1.f};
    c[ImGuiCol_WindowBg]          = {0.045f, 0.045f, 0.055f, 1.f};
    c[ImGuiCol_ChildBg]           = {0.06f, 0.06f, 0.08f, 1.f};
    c[ImGuiCol_PopupBg]           = {0.08f, 0.08f, 0.10f, 1.f};
    c[ImGuiCol_Border]            = {0.14f, 0.14f, 0.18f, 1.f};
    c[ImGuiCol_FrameBg]           = {0.07f, 0.07f, 0.11f, 1.f};
    c[ImGuiCol_FrameBgHovered]    = {0.11f, 0.11f, 0.18f, 1.f};
    c[ImGuiCol_FrameBgActive]     = {0.15f, 0.15f, 0.24f, 1.f};
    c[ImGuiCol_ScrollbarBg]       = {0.045f, 0.045f, 0.055f, 1.f};
    c[ImGuiCol_ScrollbarGrab]     = {0.24f, 0.24f, 0.34f, 1.f};
    c[ImGuiCol_ScrollbarGrabHovered]  = {0.34f, 0.34f, 0.48f, 1.f};
    c[ImGuiCol_ScrollbarGrabActive]   = {0.44f, 0.44f, 0.60f, 1.f};
    c[ImGuiCol_CheckMark]         = {0.60f, 0.20f, 0.80f, 1.f};
    c[ImGuiCol_SliderGrab]        = {0.35f, 0.60f, 1.00f, 1.f};
    c[ImGuiCol_SliderGrabActive]  = {0.50f, 0.75f, 1.00f, 1.f};
    c[ImGuiCol_Button]            = {0.09f, 0.09f, 0.13f, 1.f};
    c[ImGuiCol_ButtonHovered]     = {0.16f, 0.12f, 0.26f, 1.f};
    c[ImGuiCol_ButtonActive]      = {0.26f, 0.16f, 0.42f, 1.f};
    c[ImGuiCol_Header]            = {0.13f, 0.10f, 0.22f, 1.f};
    c[ImGuiCol_HeaderHovered]     = {0.20f, 0.14f, 0.34f, 1.f};
    c[ImGuiCol_HeaderActive]      = {0.28f, 0.18f, 0.46f, 1.f};
    c[ImGuiCol_Separator]         = {0.13f, 0.13f, 0.17f, 1.f};
    c[ImGuiCol_Tab]               = {0.06f, 0.06f, 0.08f, 1.f};
    c[ImGuiCol_TabHovered]        = {0.13f, 0.10f, 0.22f, 1.f};
    c[ImGuiCol_TabActive]         = {0.10f, 0.08f, 0.18f, 1.f};

    ImGui_ImplWin32_Init(g_overlayHwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
    return true;
}

static inline ImU32 C4(const float *c) {
    return IM_COL32((int)(c[0]*255), (int)(c[1]*255), (int)(c[2]*255), (int)(c[3]*255));
}

static inline ImU32 hbnx_col(int r, int g, int b, int a = 255) {
    return IM_COL32(r, g, b, a);
}

static bool Toggle(const char *label, bool *v) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushID(label);
    ImGui::InvisibleButton("##tg", {w, 18.f});
    bool changed = false;
    if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }
    ImGui::PopID();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 bg = *v ? hbnx_col(59,130,246) : hbnx_col(34,34,46);
    ImU32 fg = *v ? hbnx_col(255,255,255) : hbnx_col(180,180,195);
    float tx = p.x + w - 32.f;
    dl->AddText({p.x + 2, p.y + 2}, hbnx_col(220,220,235), label);
    dl->AddRectFilled({tx, p.y + 2}, {tx + 28, p.y + 16}, bg, 6.f);
    float cx = *v ? tx + 14 : tx + 3;
    dl->AddCircleFilled({cx + 6, p.y + 9}, 5, fg);
    return changed;
}

static void Section(const char *name) {
    ImGui::Dummy({0, 10});
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddText({p.x, p.y}, hbnx_col(140,140,165), name);
    float tw = ImGui::CalcTextSize(name).x;
    dl->AddRectFilled({p.x, p.y + 16}, {p.x + tw + 8, p.y + 18}, hbnx_col(59,130,246,180), 1.f);
    ImGui::Dummy({0, 22});
}

static const char* VkName(int vk) {
    switch (vk) {
    case VK_XBUTTON2: return "MB5";
    case VK_XBUTTON1: return "MB4";
    case VK_LBUTTON:  return "LMB";
    case VK_RBUTTON:  return "RMB";
    case VK_MBUTTON:  return "MMB";
    case VK_MENU:     return "ALT";
    case VK_CONTROL:  return "CTRL";
    case VK_SHIFT:    return "SHIFT";
    case VK_CAPITAL:  return "CAPS";
    case VK_F1: case VK_F2: case VK_F3: case VK_F4:
    case VK_F5: case VK_F6: case VK_F7: case VK_F8:
    case VK_F9: case VK_F10: case VK_F11: case VK_F12: {
        static char buf[4];
        snprintf(buf, 4, "F%d", vk - VK_F1 + 1);
        return buf;
    }
    default: {
        static char buf[8];
        buf[0] = (char)MapVirtualKeyA(vk, MAPVK_VK_TO_CHAR);
        buf[1] = 0;
        return buf[0] ? buf : "?";
    }
    }
}

static bool KeyButton(const char *label, int *key, bool *capturing) {
    char buf[64];
    if (*capturing) snprintf(buf, sizeof(buf), "[ press ]");
    else snprintf(buf, sizeof(buf), "%s: %s", label, VkName(*key));
    if (*capturing) {
        for (int vk = 1; vk < 256; vk++) {
            if (!(GetAsyncKeyState(vk) & 0x8000)) continue;
            if (vk == VK_ESCAPE) { *capturing = false; break; }
            *key = vk; *capturing = false; break;
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.20f, 1));
        ImGui::Button(buf, {150, 26});
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button(buf, {150, 26})) *capturing = true;
    }
    return *capturing;
}

static void DrawMenu() {
    const float MW = 280.f, MH = 400.f;

    ImGui::SetNextWindowSize({MW, MH}, ImGuiCond_Always);
    ImGui::SetNextWindowPos({(g_screenW - MW) / 2.f, (g_screenH - MH) / 2.f}, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {15.f, 15.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.f, 8.f});
    ImGui::Begin("##main", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    ImGui::PopStyleVar(2);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    dl->AddRectFilled({wp.x, wp.y}, {wp.x+MW, wp.y+35}, hbnx_col(15,15,20));
    dl->AddRectFilledMultiColor({wp.x, wp.y}, {wp.x+MW, wp.y+2},
        hbnx_col(153,51,204), hbnx_col(204,102,255), hbnx_col(204,102,255), hbnx_col(153,51,204));
    dl->AddText(g_fontBold, 16.f, {wp.x+12, wp.y+8}, hbnx_col(255,255,255), "Nico");

    ImGui::Dummy({0, 18});

    ImGui::Checkbox("ESP", &g_cfg.espEnabled);
    if (g_cfg.espEnabled) {
        ImGui::Checkbox("Names", &g_cfg.espName);
        ImGui::Checkbox("Bones", &g_cfg.espBones);
        ImGui::Checkbox("Weapon", &g_cfg.espWeapon);
        ImGui::Checkbox("Distance", &g_cfg.espDistance);
        ImGui::Checkbox("Health", &g_cfg.espHealth);
        ImGui::Checkbox("Money", &g_cfg.espMoney);
        ImGui::Checkbox("Flash", &g_cfg.espFlash);
        ImGui::Checkbox("Defusing", &g_cfg.espDefusing);
        ImGui::Checkbox("Team Check", &g_cfg.teamCheck);
        ImGui::Checkbox("Bomb Site", &g_cfg.bombTimerEnabled);
    }

    ImGui::Checkbox("Stream Proof", &g_cfg.obsBypass);

    ImGui::Dummy({0, 12});

    if (ImGui::Button("Save", {80, 22})) { g_cfg.Save("hbnx_config.bin"); }
    ImGui::SameLine();
    if (ImGui::Button("Load", {80, 22})) { g_cfg.Load("hbnx_config.bin"); }
    ImGui::SameLine();
    if (ImGui::Button("Reset", {80, 22})) { g_cfg = Config{}; g_cfg.menuOpen = true; }

    ImGui::Dummy({0, 8});
    ImGui::TextColored({0.6f,0.6f,0.7f,1}, "INSERT: menu | P: exit");

    ImGui::End();
}

static void SetClickthrough(bool through) {
    LONG_PTR ex = GetWindowLongPtrW(g_overlayHwnd, GWL_EXSTYLE);
    if (through) ex |= WS_EX_TRANSPARENT;
    else { ex &= ~WS_EX_TRANSPARENT; SetForegroundWindow(g_overlayHwnd); }
    SetWindowLongPtrW(g_overlayHwnd, GWL_EXSTYLE, ex);
}

static void UpdateStreamProof(bool enabled) {
    static bool last = false;
    if (enabled != last) {
        SetWindowDisplayAffinity(g_overlayHwnd, enabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
        SetWindowPos(g_overlayHwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        last = enabled;
    }
}

void RenderFrame() {
    static LONGLONG lastPosUpdate = 0;
    LONGLONG now = GetTickCount64();
    if (now - lastPosUpdate > 500) {
        UpdateOverlayPosition(g_overlayHwnd);
        lastPosUpdate = now;
    }

    bool gameActive = IsGameForeground();
    static bool wasActive = true;
    if (gameActive != wasActive) {
        ShowWindow(g_overlayHwnd, gameActive ? SW_SHOW : SW_HIDE);
        wasActive = gameActive;
    }
    if (!gameActive) { Sleep(16); return; }

    static bool insWas = false;
    bool insNow = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insNow && !insWas) {
        g_cfg.menuOpen = !g_cfg.menuOpen;
        SetClickthrough(!g_cfg.menuOpen);
    }
    insWas = insNow;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_clientBase)
        g_viewMatrix = RPM<ViewMatrix>(g_clientBase + game_offsets::offsets::client_dll::dwViewMatrix);

    if (g_cfg.menuOpen) DrawMenu();
    DrawESP();

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    DrawWatermark(dl);
    if (g_cfg.bombTimerEnabled)
        DrawBombTimer(dl);

    UpdateStreamProof(g_cfg.obsBypass);

    ImGui::Render();
    const float cc[4] = {0.f, 0.f, 0.f, 0.f};
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    g_pd3dContext->ClearRenderTargetView(g_mainRTV, cc);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_pSwapChain->Present(1, 0);
}

void ShutdownOverlay() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_mainRTV) { g_mainRTV->Release(); g_mainRTV = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_overlayHwnd) { DestroyWindow(g_overlayHwnd); g_overlayHwnd = nullptr; }
    UnregisterClassW(L"hbnxOverlay", GetModuleHandleW(nullptr));
}
