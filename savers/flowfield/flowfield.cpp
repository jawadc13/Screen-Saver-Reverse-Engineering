// flowfield.cpp - "Flow Field": thousands of particles painting silky currents of colour
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Flow Field Settings",
    2, { { L"&Palette:", L"Palette", 0, L"Neon|Ocean|Fire|Rainbow" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Particles", L"Particles", 50, L"Few", L"Many" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullFlowField"; }
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

    struct P { float x, y, life, hue; };
    std::vector<P> ps;
    float seedA = 0, seedB = 0;
    bool cleared = false;
    bool Persistent() const override { return true; }
    void Setup(Renderer& r) override { r.SetPersistent(true); }
    void Reseed() override {
        ps.resize(preview ? 400 : 1500 + (int)(Amt() * 5000));
        for (auto& p : ps) p = { RandF(-1, 1), RandF(-1, 1), RandF(0, 6), RandF(0, 1) };
        seedA = RandF(0, 100); seedB = RandF(0, 100); cleared = false;
    }
    Color Pal(float u) const {
        switch (Style()) {
        case 1:  return Hsv(0.48f + 0.12f * u, 0.8f, 1);
        case 2:  return Hsv(0.0f + 0.12f * u, 0.9f, 1);
        case 3:  return Hsv(u, 0.8f, 1);
        default: return Hsv(0.75f + 0.35f * u, 0.85f, 1);
        }
    }
    void Draw(Renderer& r, float dt) override {
        // The canvas keeps its image; it is darkened slowly so trails fade.
        if (!cleared) { r.FullscreenQuad(0, 0, 0, 1); cleared = true; }
        r.FadeToBlack(FadeAlpha(0.8f, dt));
        float aspect = (float)width / height, W = (float)width * 0.5f, H = (float)height * 0.5f;
        Canvas2D c; c.Begin(width, height);
        float lw = MinDim() / 600.0f;
        for (auto& p : ps) {
            float x = p.x * aspect, y = p.y;
            float ang = 2.2f * (sinf(x * 1.3f + seedA + t * 0.05f) + cosf(y * 1.7f + seedB - t * 0.04f) + sinf((x + y) * 0.7f + t * 0.03f));
            float sp = 0.12f * dt * Spd();
            float nx = p.x + cosf(ang) * sp / aspect, ny = p.y + sinf(ang) * sp;
            p.life -= dt;
            float a = Smooth(p.life) * Smooth(6 - p.life);
            c.Line(p.x * W, p.y * H, nx * W, ny * H, lw, Pal(p.hue).WithAlpha(0.9f * a), Pal(p.hue).WithAlpha(0.9f * a));
            p.x = nx; p.y = ny;
            if (p.life <= 0 || fabsf(p.x) > 1.05f || fabsf(p.y) > 1.05f) p = { RandF(-1, 1), RandF(-1, 1), RandF(3, 6), fmodf(RandF(0, 1) + t * 0.01f, 1) };
        }
        c.Draw(r, BLEND_ADD);
    }

};

Scene* CreateScene() { return new SaverScene; }
