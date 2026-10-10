// cloudflight.cpp - "Cloud Flight": flying through soft, sunlit clouds
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Cloud Flight Settings",
    2, { { L"&Sky:", L"Sky", 0, L"Blue sky|Golden hour|Pink dusk|Storm" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Clouds", L"Clouds", 50, L"Few", L"Many" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullClouds"; }
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

    struct Puff { Vec3 p; float size; int tex; float alpha; };
    std::vector<Puff> puffs;
    Texture* clouds[4] = {};
    Texture* dot = nullptr;
    ~SaverScene() { for (auto* c : clouds) delete c; delete dot; }
    void Setup(Renderer& r) override { for (int i = 0; i < 4; i++) clouds[i] = aero::Cloud(r, (unsigned)RandI(1, 99999)); dot = aero::SoftDot(r); }
    void Spawn(Puff& p, bool anywhere) {
        p.p = Vec3(RandF(-40, 40), RandF(-12, 10), anywhere ? RandF(-160, -5) : -160);
        p.size = RandF(8, 20); p.tex = RandI(0, 3); p.alpha = anywhere ? 1 : 0;
    }
    void Reseed() override { puffs.resize(30 + (int)(Amt() * 90)); for (auto& p : puffs) Spawn(p, true); }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, tint, sun;
        switch (Style()) {
        case 1:  top = Color(0.25f, 0.4f, 0.75f); hor = Color(1, 0.75f, 0.45f); tint = Color(1, 0.88f, 0.75f); sun = Color(1, 0.8f, 0.5f); break;
        case 2:  top = Color(0.3f, 0.25f, 0.55f); hor = Color(1, 0.6f, 0.7f); tint = Color(1, 0.8f, 0.85f); sun = Color(1, 0.7f, 0.75f); break;
        case 3:  top = Color(0.15f, 0.17f, 0.22f); hor = Color(0.45f, 0.48f, 0.52f); tint = Color(0.6f, 0.62f, 0.66f); sun = Color(0.5f, 0.55f, 0.6f); break;
        default: top = Color(0.1f, 0.4f, 0.9f); hor = Color(0.7f, 0.88f, 1.0f); tint = Color(1, 1, 1); sun = Color(1, 1, 0.9f); break;
        }
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.6f), hor };
          aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }

        Mat4 proj = Mat4::Perspective(FitFov(60, width, height), (float)width / height, 0.5f, 400);
        Mat4 view = Mat4::LookAt(Vec3(sinf(t * 0.05f) * 3, sinf(t * 0.07f) * 1.5f, 0), Vec3(sinf(t * 0.03f) * 6, sinf(t * 0.04f) * 2, -50), Vec3(sinf(t * 0.02f) * 0.15f, 1, 0));
        Canvas2D glow; glow.Begin(width, height);
        glow.Dot(width * (0.25f * sinf(t * 0.01f)), height * 0.3f, MinDim() * 0.6f, sun * 0.35f);
        glow.Draw(r, BLEND_ADD, dot);
        for (auto& p : puffs) { p.p.z += dt * Spd() * 12; p.alpha = fminf(1, p.alpha + dt * 0.5f); if (p.p.z > 2) Spawn(p, false); }
        std::sort(puffs.begin(), puffs.end(), [](const Puff& a, const Puff& b) { return a.p.z < b.p.z; });
        Canvas2D layer[4]; for (auto& l : layer) l.Begin(width, height);
        for (auto& p : puffs) {
            float sx, sy, d;
            if (!ToScreen(proj * view, p.p, width, height, sx, sy, d)) continue;
            float s = p.size / d * height * 0.9f;
            float fadeNear = fminf(1, (-p.p.z) / 12);   // dissolve as we fly through
            float fadeFar = fminf(1, (160 + p.p.z) / 40);
            Color c = Lerp(tint, hor, fminf(1, d / 160)).WithAlpha(0.85f * p.alpha * fadeNear * fadeFar);
            layer[p.tex].Rect(sx - s, sy - s * 0.5f, sx + s, sy + s * 0.5f, c, 0, 0.1f, 1, 0.9f);
        }
        for (int i = 0; i < 4; i++) layer[i].Draw(r, BLEND_ALPHA, clouds[i]);
    }

};

Scene* CreateScene() { return new SaverScene; }
