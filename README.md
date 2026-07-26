# Nico CS2 External

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![ImGui](https://img.shields.io/badge/ImGui-1.90-brightgreen.svg)](https://github.com/ocornut/imgui)
[![DirectX](https://img.shields.io/badge/DirectX-11-purple.svg)](https://docs.microsoft.com/en-us/windows/win32/direct3d11)
[![Status](https://img.shields.io/badge/Status-Active-success.svg)]()

Overlay externo para CS2 con ImGui y DirectX 11.

## Qué hace

### ESP
- Skeleton ESP 
- Name ESP
- Weapon ESP
- Distance ESP
- Health ESP
- Money ESP
- Flash ESP
- Defusing ESP
- Team Check
- Dynamic Scaling
- Opacity Fading

### Extras
- Spectator List
- Watermark
- Stream Proof
- Save/Load Config
- Colores personalizados

## Funciones

### esp.cpp
- DrawESP()
- DrawWatermark()
- DrawSpectatorList()

### process.cpp
- GetProcessIdByName()
- OpenGameProcess()
- GetModuleBase()
- InitMemory()
- UpdateOverlayPosition()
- IsGameForeground()
- CleanupMemory()
- SigScan()

### read.cpp
- ReadWeaponName()
- UpdateEntityCache()
- StartMemoryThread()
- StopMemoryThread()

### config.h
- Config::Save()
- Config::Load()

### main.cpp
- DrawLogo()
- WinMain()

## Dependencias

- ImGui
- DirectX 11
- Windows SDK

## Notas

- Solo para fines educativos
- Requiere CS2 ejecutándose

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/ae363782-2c04-4ca3-822d-4574117197b6" />
