// retrosunset.cpp - "Retro Sunset": an 80s synthwave sunset over a racing neon grid
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Retro Sunset Settings",
    2, { { L"&Palette:", L"Palette", 0, L"Synthwave|Vaporwave|Outrun red|Cyber teal" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Speed of the grid", L"GridSpeed", 50, L"Cruise", L"Turbo" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullRetroSunset"; }
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

    float z0 = 0, mph[6] = {};
    void Reseed() override { for (auto& p : mph) p = RandF(0, 6.28f); }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, sunA, sunB, grid, ground;
        switch (Style()) {
        case 1:  top = Color(0.35f, 0.6f, 0.9f); hor = Color(1, 0.6f, 0.85f); sunA = Color(1, 0.9f, 0.5f); sunB = Color(1, 0.4f, 0.75f); grid = Color(0.4f, 0.9f, 1); ground = Color(0.2f, 0.05f, 0.3f); break;
        case 2:  top = Color(0.1f, 0.0f, 0.05f); hor = Color(0.9f, 0.2f, 0.15f); sunA = Color(1, 0.85f, 0.3f); sunB = Color(1, 0.15f, 0.1f); grid = Color(1, 0.3f, 0.2f); ground = Color(0.1f, 0.0f, 0.02f); break;
        case 3:  top = Color(0.0f, 0.05f, 0.1f); hor = Color(0.1f, 0.6f, 0.65f); sunA = Color(0.7f, 1, 0.9f); sunB = Color(0.1f, 0.6f, 0.9f); grid = Color(0.2f, 1, 0.9f); ground = Color(0.0f, 0.05f, 0.07f); break;
        default: top = Color(0.08f, 0.02f, 0.2f); hor = Color(0.95f, 0.3f, 0.5f); sunA = Color(1, 0.9f, 0.3f); sunB = Color(1, 0.2f, 0.6f); grid = Color(1, 0.3f, 0.9f); ground = Color(0.06f, 0.0f, 0.12f); break;
        }
        float W = (float)width, H = (float)height, m = MinDim();
        float horizon = -H * 0.05f;
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.5f), hor }; aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }
        // Striped sun that slowly sets, rises and drifts side to side.
        Canvas2D sun; sun.Begin(width, height);
        float sx = W * 0.15f * sinf(t * 0.01f), sy = horizon + m * (0.12f + 0.08f * sinf(t * 0.015f)), R = m * 0.28f;
        for (int i = 0; i < 48; i++) {
            float y0 = sy - R + 2 * R * i / 48, y1 = sy - R + 2 * R * (i + 1) / 48;
            float u = (float)i / 48;
            float gap = u < 0.5f && fmodf(u * 12 + t * 0.2f, 1) < 0.35f * (1 - u * 2) ? 1 : 0;   // the classic stripes
            if (gap > 0 || y1 < horizon) continue;
            float w0 = sqrtf(fmaxf(0, R * R - (y0 - sy) * (y0 - sy))), w1 = sqrtf(fmaxf(0, R * R - (y1 - sy) * (y1 - sy)));
            Color c0 = Lerp(sunB, sunA, u), c1 = Lerp(sunB, sunA, (float)(i + 1) / 48);
            PushQuad(sun.v, sun.P(sx - w0, y0), sun.P(sx + w0, y0), sun.P(sx + w1, y1), sun.P(sx - w1, y1), Vec3(0, 0, 1), c0, c0, c1, c1);
        }
        sun.Draw(r, BLEND_OPAQUE);
        // Wireframe mountains on the horizon (drifting slowly).
        Canvas2D mt; mt.Begin(width, height);
        for (int i = 0; i < 90; i++) {
            auto y = [&](float u) { float v = 0; for (int k = 0; k < 3; k++) v += fabsf(sinf((u + t * 0.003f) * (k + 2) * 3.1f + mph[k])) / (k + 1); return horizon + m * 0.09f * v; };
            float u0 = (float)i / 90, u1 = (float)(i + 1) / 90, x0 = -W / 2 + W * u0, x1 = -W / 2 + W * u1;
            PushQuad(mt.v, mt.P(x0, horizon), mt.P(x1, horizon), mt.P(x1, y(u1)), mt.P(x0, y(u0)), Vec3(0, 0, 1), ground, ground, ground * 1.6f, ground * 1.6f);
            mt.Line(x0, y(u0), x1, y(u1), m * 0.0015f, grid * 0.8f, grid * 0.8f);
        }
        mt.Draw(r, BLEND_OPAQUE);
        // Ground and the racing grid.
        Canvas2D g; g.Begin(width, height);
        PushQuad(g.v, g.P(-W, -H), g.P(W, -H), g.P(W, horizon), g.P(-W, horizon), Vec3(0, 0, 1), ground, ground, ground, ground);
        z0 += dt * (0.3f + Amt() * 2.5f) * Spd();
        float camH = 1.0f, f = H * 1.2f;
        for (int i = 0; i < 40; i++) {
            float z = 1 + i - fmodf(z0, 1.0f);
            float y = horizon - camH / z * f;
            if (y < -H) continue;
            float a = fminf(1, 3 / z);
            g.Line(-W, y, W, y, m * 0.0012f * (1 + 2 / z), grid * a, grid * a);
        }
        for (int i = -30; i <= 30; i++) {
            float xw = i * 1.0f + sinf(t * 0.05f) * 0.5f;
            g.Line(xw / 40 * f, horizon, xw / 0.6f * f, horizon - camH / 0.6f * f, m * 0.0012f, grid * 0.3f, grid);
        }
        g.Draw(r, BLEND_ALPHA);
    }

};

Scene* CreateScene() { return new SaverScene; }
