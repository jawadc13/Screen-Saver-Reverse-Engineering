// harmony.cpp - "Harmony Waves": glossy bands of colour flowing in harmony
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Harmony Waves Settings",
    2, { { L"&Palette:", L"Palette", 0, L"Aqua|Sunrise|Spectrum|Ocean" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Bands", L"Bands", 50, L"Few", L"Many" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullHarmony"; }
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

    struct Band { float base, amp, k, w, ph, thick, hue; };
    std::vector<Band> bands;
    float hue0 = 0;
    void Reseed() override {
        bands.resize(4 + (int)(Amt() * 8));
        for (size_t i = 0; i < bands.size(); i++) bands[i] = { -0.4f + 0.8f * i / bands.size(), RandF(0.04f, 0.12f), RandF(1, 3), RandF(0.1f, 0.3f), RandF(0, 6.28f), RandF(0.04f, 0.12f), (float)i / bands.size() };
        hue0 = RandF(0, 1);
    }
    Color Pal(float u) const {
        switch (Style()) {
        case 1:  return Lerp(Color(1, 0.45f, 0.3f), Color(1, 0.85f, 0.4f), u);
        case 2:  return Hsv(u + hue0, 0.75f, 1);
        case 3:  return Lerp(Color(0.05f, 0.3f, 0.7f), Color(0.3f, 0.9f, 0.9f), u);
        default: return Lerp(Color(0.1f, 0.6f, 0.9f), Color(0.5f, 1, 0.7f), u);
        }
    }
    void Draw(Renderer& r, float dt) override {
        float W = (float)width, H = (float)height, m = MinDim();
        { Canvas2D bg; bg.Begin(width, height); Color st[3] = { Pal(0) * 0.25f, Pal(0.5f) * 0.12f, Pal(1) * 0.25f }; aero::VerticalGradient(bg, width, height, st, 3); bg.Draw(r, BLEND_OPAQUE); }
        Canvas2D c; c.Begin(width, height);
        const int seg = 120;
        for (auto& b : bands) {
            auto y = [&](float u, float off) { return H * (b.base + b.amp * sinf(u * b.k * 6.28f + t * b.w * Spd() + b.ph + off)); };
            Color col = Pal(b.hue + 0.1f * sinf(t * 0.05f + b.ph));
            for (int i = 0; i < seg; i++) {
                float u0 = (float)i / seg, u1 = (float)(i + 1) / seg, x0 = -W / 2 + W * u0, x1 = -W / 2 + W * u1;
                float th = b.thick * m;
                float a0 = y(u0, 0), a1 = y(u1, 0), c0 = y(u0, 0.6f) - th, c1 = y(u1, 0.6f) - th;
                PushQuad(c.v, c.P(x0, c0), c.P(x1, c1), c.P(x1, a1), c.P(x0, a0), Vec3(0, 0, 1), col.WithAlpha(0.05f), col.WithAlpha(0.05f), col.WithAlpha(0.5f), col.WithAlpha(0.5f));
                // glossy top edge
                PushQuad(c.v, c.P(x0, a0 - m * 0.004f), c.P(x1, a1 - m * 0.004f), c.P(x1, a1 + m * 0.002f), c.P(x0, a0 + m * 0.002f), Vec3(0, 0, 1),
                         Color(1, 1, 1, 0.1f), Color(1, 1, 1, 0.1f), Color(1, 1, 1, 0.6f), Color(1, 1, 1, 0.6f));
            }
        }
        c.Draw(r, BLEND_ALPHA);
    }

};

Scene* CreateScene() { return new SaverScene; }
