#pragma once
#include <Windows.h>
#include <d3d11.h>

extern HWND g_overlayHwnd;
extern ID3D11Device *g_pd3dDevice;
extern ID3D11DeviceContext *g_pd3dContext;
extern IDXGISwapChain *g_pSwapChain;

bool InitOverlay(HINSTANCE hInstance);

void ShutdownOverlay();

void ResizeRenderTarget();

void RenderFrame();
