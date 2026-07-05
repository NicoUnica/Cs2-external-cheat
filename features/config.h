#pragma once
#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <string>

struct Config {
    uint32_t magic = 0x48424E58;
    uint32_t version = 2;
    bool menuOpen = false;

    bool espEnabled     = true;
    bool teamCheck      = true;
    bool espBones       = true;
    bool espName        = true;
    bool espWeapon      = true;
    bool espDistance    = true;
    bool espMoney       = true;
    bool espFlash       = true;
    bool espDefusing    = true;
    bool espHealth      = true;

    bool   obsBypass    = false;
    bool   bombTimerEnabled = true;

    float colEspBone[4]     = {0.6f, 0.2f, 0.8f, 0.9f};
    float colEspBoneHidden[4]={0.3f,  0.3f,  0.35f, 0.7f};
    float colEspName[4]     = {1.f, 1.f, 1.f, 1.f};
    float colEspWeapon[4]   = {0.9f, 0.85f, 0.2f, 1.f};
    float colEspDist[4]     = {0.8f, 0.8f, 0.8f, 1.f};

    void Save(const std::string &path) {
        std::ofstream f(path, std::ios::binary);
        if (!f) return;
        f.write((const char*)this, sizeof(*this));
    }

    void Load(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return;
        uint32_t m = 0, v = 0;
        f.read((char*)&m, 4);
        f.read((char*)&v, 4);
        if (m != magic || v != version) return;
        f.seekg(0);
        f.read((char*)this, sizeof(*this));
    }
};

extern Config g_cfg;
