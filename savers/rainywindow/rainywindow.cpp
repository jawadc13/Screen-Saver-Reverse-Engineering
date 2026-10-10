// rainywindow.cpp - "Rainy Window": raindrops trickling down a window over blurred city lights
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Rainy Window Settings",
    2, { { L"&City:", L"City", 0, L"Warm night|Neon|Blue hour|Autumn" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Rain", L"Rain", 50, L"Light", L"Heavy" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullRainyWindow"; }
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

    struct Light { float x, y, r, hue, ph, vx; };
    struct Drop { float x, y, r, vy, stick; };
    std::vector<Light> lights;
    std::vector<Drop> drops;
    Texture *bokeh = nullptr, *atlas = nullptr;
    float spawn = 0;
    ~SaverScene() { delete bokeh; delete atlas; }
    void Setup(Renderer& r) override { bokeh = aero::Bokeh(r); atlas = aero::OrbAtlas(r); }
    Color Pal(float u) const {
        switch (Style()) {
        case 1:  return Hsv(0.75f + u * 0.4f, 0.8f, 1);
        case 2:  return Lerp(Color(0.3f, 0.6f, 1), Color(1, 0.85f, 0.6f), u);
        case 3:  return Lerp(Color(1, 0.5f, 0.15f), Color(1, 0.85f, 0.4f), u);
        default: return Lerp(Color(1, 0.65f, 0.3f), Color(1, 0.9f, 0.7f), u);
        }
    }
    void Reseed() override {
        lights.resize(70);
        for (auto& l : lights) l = { RandF(-0.55f, 0.55f), RandF(-0.5f, 0.5f), RandF(0.02f, 0.09f), RandF(0, 1), RandF(0, 6.28f), RandF(-0.004f, 0.004f) };
        drops.clear();
    }
    void Draw(Renderer& r, float dt) override {
        float W = (float)width, H = (float)height, m = MinDim();
        { Canvas2D bg; bg.Begin(width, height); Color st[3] = { Pal(0.2f) * 0.06f, Pal(0.5f) * 0.12f, Pal(0.8f) * 0.08f }; aero::VerticalGradient(bg, width, height, st, 3); bg.Draw(r, BLEND_OPAQUE); }
        Canvas2D bl; bl.Begin(width, height);
        for (auto& l : lights) {
            l.x += l.vx * dt * Spd(); if (l.x > 0.6f) l.x -= 1.2f; if (l.x < -0.6f) l.x += 1.2f;   // traffic and swaying lights
            bl.Dot(l.x * W, l.y * H, l.r * m, Pal(l.hue).WithAlpha(0.35f + 0.2f * sinf(t * 0.5f + l.ph)));
        }
        bl.Draw(r, BLEND_ADD, bokeh);
        spawn -= dt * (3 + Amt() * 25);
        while (spawn <= 0) { drops.push_back({ RandF(-0.5f, 0.5f), RandF(-0.45f, 0.5f), RandF(0.004f, 0.014f), 0, RandF(0.5f, 4) }); spawn += 1; }
        Canvas2D dl; dl.Begin(width, height);
        for (auto& d : drops) {
            d.stick -= dt;
            if (d.stick < 0 && d.r > 0.007f) d.vy = fminf(d.vy + dt * 0.2f, 0.25f * d.r / 0.01f);   // big drops run
            d.y -= d.vy * dt * Spd();
            d.x += sinf(t * 3 + d.y * 40) * d.vy * 0.02f * dt;
            float x = d.x * W, y = d.y * H, rr = d.r * m * (d.vy > 0 ? 1.0f : 0.9f);
            aero::OrbCell(dl, aero::ORB_BODY, x, y, rr, Color(0.8f, 0.85f, 0.95f, 0.55f), 0.85f);
            aero::OrbCell(dl, aero::ORB_GLOSS, x, y, rr, Color(1, 1, 1, 0.7f), 0.85f);
        }
        drops.erase(std::remove_if(drops.begin(), drops.end(), [](const Drop& d) { return d.y < -0.6f; }), drops.end());
        if (drops.size() > 600) drops.erase(drops.begin(), drops.begin() + (drops.size() - 600));
        dl.Draw(r, BLEND_ALPHA, atlas);
    }

};

Scene* CreateScene() { return new SaverScene; }
