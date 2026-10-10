// voronoi.cpp - "Voronoi Cells": living stained-glass cells drifting and reshaping
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Voronoi Cells Settings",
    2, { { L"&Palette:", L"Palette", 0, L"Jewel|Ocean|Autumn|Pastel" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Cells", L"Cells", 40, L"Few", L"Many" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullVoronoi"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace rest;

class SaverScene : public RestScene {
    void RestSettings(float& every, float& len, int& band) const override {
        every = 60 * (1 + g_cfg.slider[2] * 14 / 100.0f);
        len = 5 + g_cfg.slider[3] * 55 / 100.0f;
        band = (int)g_cfg.choice[1];
    }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }
    float Amt() const { return g_cfg.slider[1] / 100.0f; }
    DWORD Style() const { return g_cfg.choice[0]; }
    float MinDim() const { return (float)(width < height ? width : height); }

    struct Seed { float x, y, vx, vy, hue; };
    std::vector<Seed> seeds;
    std::vector<Vertex> verts;
    void Reseed() override {
        seeds.resize(12 + (int)(Amt() * 40));
        for (auto& s : seeds) s = { RandF(-1, 1), RandF(-1, 1), RandF(-0.05f, 0.05f), RandF(-0.05f, 0.05f), RandF(0, 1) };
    }
    Color Pal(float u) const {
        switch (Style()) {
        case 1:  return Hsv(0.5f + 0.15f * sinf(u * 6.28f), 0.75f, 0.85f);
        case 2:  return Hsv(0.02f + 0.1f * u, 0.85f, 0.85f);
        case 3:  return Hsv(u, 0.35f, 1);
        default: return Hsv(u, 0.85f, 0.9f);
        }
    }
    void Draw(Renderer& r, float dt) override {
        // GPU Voronoi: a cone under every seed; the depth test keeps the
        // nearest one, which draws exact cell borders for free.
        float aspect = (float)width / height;
        verts.clear();
        const int segs = 40;
        for (auto& s : seeds) {
            s.x += s.vx * dt * Spd(); s.y += s.vy * dt * Spd();
            if (fabsf(s.x) > 1) { s.vx = -s.vx; s.x = s.x > 0 ? 1.0f : -1.0f; }
            if (fabsf(s.y) > 1) { s.vy = -s.vy; s.y = s.y > 0 ? 1.0f : -1.0f; }
            Color c = Pal(s.hue + t * 0.01f), edge = c * 0.25f;
            for (int i = 0; i < segs; i++) {
                float a0 = 2 * kPi * i / segs, a1 = 2 * kPi * (i + 1) / segs, R = 3;
                PushVertex(verts, Vec3(s.x, s.y, 0.01f), Vec3(0, 0, 1), 0, 0, c);
                PushVertex(verts, Vec3(s.x + cosf(a0) * R / aspect, s.y + sinf(a0) * R, 0.99f), Vec3(0, 0, 1), 0, 0, edge);
                PushVertex(verts, Vec3(s.x + cosf(a1) * R / aspect, s.y + sinf(a1) * R, 0.99f), Vec3(0, 0, 1), 0, 0, edge);
            }
        }
        r.SetCamera(Mat4::Identity(), Mat4::Identity());
        DrawParams p; p.lit = false;
        r.Draw(verts.data(), verts.size(), p);
    }

};

Scene* CreateScene() { return new SaverScene; }
