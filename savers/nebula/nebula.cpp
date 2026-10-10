// nebula.cpp - "Space Nebula": drifting through glowing clouds of interstellar gas and stars
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Space Nebula Settings",
    2, { { L"&Nebula:", L"Nebula", 0, L"Orion pink|Carina blue|Emerald|Fire" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Density", L"Density", 50, L"Wispy", L"Thick" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullNebula"; }
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

    struct Cloud { float x, y, z, size, rot, spin; int tex; Color c; };
    struct Star { float x, y, z, b; };
    std::vector<Cloud> clouds;
    std::vector<Star> stars;
    Texture* tex[4] = {};
    Texture* dot = nullptr;
    ~SaverScene() { for (auto* x : tex) delete x; delete dot; }
    void Setup(Renderer& r) override { for (int i = 0; i < 4; i++) tex[i] = aero::Cloud(r, (unsigned)RandI(1, 99999)); dot = aero::SoftDot(r); }
    Color Pal(float u) const {
        switch (Style()) {
        case 1:  return Lerp(Color(0.2f, 0.45f, 1), Color(0.6f, 0.3f, 0.9f), u);
        case 2:  return Lerp(Color(0.1f, 0.8f, 0.5f), Color(0.2f, 0.5f, 0.9f), u);
        case 3:  return Lerp(Color(1, 0.35f, 0.1f), Color(1, 0.75f, 0.2f), u);
        default: return Lerp(Color(1, 0.3f, 0.55f), Color(0.5f, 0.3f, 1), u);
        }
    }
    void Reseed() override {
        clouds.resize(14 + (int)(Amt() * 26));
        for (auto& c : clouds) c = { RandF(-1.2f, 1.2f), RandF(-1, 1), RandF(0.3f, 1), RandF(0.3f, 0.9f), RandF(0, 6.28f), RandF(-0.02f, 0.02f), RandI(0, 3), Pal(RandF(0, 1)) };
        stars.resize(500);
        for (auto& s : stars) s = { RandF(-1.5f, 1.5f), RandF(-1, 1), RandF(0.2f, 1), RandF(0.3f, 1) };
    }
    void Draw(Renderer& r, float dt) override {
        float W = (float)width * 0.5f, H = (float)height * 0.5f, m = MinDim();
        { Canvas2D bg; bg.Begin(width, height); Color st[3] = { Pal(0) * 0.05f, Pal(0.5f) * 0.1f, Pal(1) * 0.05f }; aero::VerticalGradient(bg, width, height, st, 3); bg.Draw(r, BLEND_OPAQUE); }
        float drift = dt * 0.015f * Spd();
        Canvas2D sc; sc.Begin(width, height);
        for (auto& s : stars) {
            s.x -= drift * s.z * 2; if (s.x < -1.5f) s.x += 3;
            sc.Dot(s.x * W, s.y * H, m * 0.003f * (0.5f + s.z), Color(1, 1, 1) * s.b);
        }
        sc.Draw(r, BLEND_ADD, dot);
        Canvas2D layer[4]; for (auto& l : layer) l.Begin(width, height);
        for (auto& c : clouds) {
            c.x -= drift * c.z; c.rot += c.spin * dt;
            if (c.x < -1.6f) c.x += 3.2f;
            float s = c.size * m * (0.6f + c.z);
            float pulse = 0.85f + 0.15f * sinf(t * 0.2f + c.rot * 5);
            aero::Sprite(layer[c.tex], c.x * W, c.y * H, s * 2, s * 1.4f, c.rot, (c.c * (0.5f * pulse)).WithAlpha(1));
        }
        for (int i = 0; i < 4; i++) layer[i].Draw(r, BLEND_ADD, tex[i]);
    }

};

Scene* CreateScene() { return new SaverScene; }
