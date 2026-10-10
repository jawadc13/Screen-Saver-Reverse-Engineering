// inkswirls.cpp - "Ink Swirls": clouds of coloured ink curling through clear water
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Ink Swirls Settings",
    2, { { L"&Water:", L"Water", 0, L"Milk white|Deep blue|Paper|Black" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Ink amount", L"Ink", 50, L"Little", L"Lots" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullInk"; }
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

    struct P { float x, y, vx, vy, life, hue; };
    std::vector<P> ps;
    float hue0 = 0, emit = 0, ex = 0, ey = 0;
    bool cleared = false;
    bool Persistent() const override { return true; }
    void Setup(Renderer& r) override { r.SetPersistent(true); }
    void Reseed() override { ps.clear(); hue0 = RandF(0, 1); cleared = false; }
    Color Water() const {
        switch (Style()) { case 1: return Color(0.02f, 0.06f, 0.15f); case 2: return Color(0.93f, 0.9f, 0.82f); case 3: return Color(0, 0, 0); default: return Color(0.95f, 0.96f, 0.97f); }
    }
    void Draw(Renderer& r, float dt) override {
        Color water = Water();
        bool light = water.r > 0.5f;
        if (!cleared) { r.FullscreenQuad(water.r, water.g, water.b, 1); cleared = true; }
        r.FullscreenQuad(water.r, water.g, water.b, FadeAlpha(0.8f, dt));   // ink slowly dissolves
        // Emitter wanders around, so ink never pools in one place.
        ex = 0.7f * sinf(t * 0.11f * Spd() + hue0 * 5); ey = 0.6f * sinf(t * 0.07f * Spd() + 1);
        emit += dt * (80 + Amt() * 600);
        while (emit >= 1) {
            emit -= 1;
            float a = RandF(0, 6.28f), s = RandF(0.05f, 0.25f);
            ps.push_back({ ex, ey, cosf(a) * s, sinf(a) * s, RandF(2, 5), fmodf(hue0 + t * 0.02f + RandF(0, 0.15f), 1) });
        }
        float aspect = (float)width / height, W = (float)width * 0.5f, H = (float)height * 0.5f;
        Canvas2D c; c.Begin(width, height);
        float lw = MinDim() / 500.0f;
        for (auto& p : ps) {
            float x = p.x * aspect, y = p.y;
            // Curl-like swirl field.
            float vx = sinf(y * 3.1f + t * 0.3f) + 0.5f * cosf(x * 2.3f - t * 0.2f), vy = cosf(x * 2.7f - t * 0.25f) + 0.5f * sinf(y * 1.9f + t * 0.35f);
            p.vx += (vx * 0.3f - p.vx) * dt; p.vy += (vy * 0.3f - p.vy) * dt;
            float nx = p.x + p.vx * dt * Spd() / aspect, ny = p.y + p.vy * dt * Spd();
            p.life -= dt;
            float a = Smooth(p.life) * 0.35f;
            Color ink = light ? Hsv(p.hue, 0.85f, 0.55f) : Hsv(p.hue, 0.75f, 1);
            c.Line(p.x * W, p.y * H, nx * W, ny * H, lw * (1 + (5 - p.life)), ink.WithAlpha(a), ink.WithAlpha(a));
            p.x = nx; p.y = ny;
        }
        ps.erase(std::remove_if(ps.begin(), ps.end(), [](const P& p) { return p.life <= 0; }), ps.end());
        c.Draw(r, BLEND_ALPHA);
    }

};

Scene* CreateScene() { return new SaverScene; }
