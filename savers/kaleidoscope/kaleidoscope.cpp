// kaleidoscope.cpp - "Kaleidoscope": ever-turning mirrored patterns of light and colour
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Kaleidoscope Settings",
    2, { { L"&Mirrors:", L"Mirrors", 1, L"Six|Eight|Twelve|Sixteen" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Shapes", L"Shapes", 50, L"Few", L"Many" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullKaleido"; }
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

    struct Shape { float r, a, vr, va, size, hue, spin; int kind; };
    std::vector<Shape> shapes;
    Texture* dot = nullptr;
    float hue0 = 0;
    ~SaverScene() { delete dot; }
    void Setup(Renderer& r) override { dot = aero::SoftDot(r); }
    void Reseed() override {
        shapes.resize(8 + (int)(Amt() * 24));
        for (auto& s : shapes) s = { RandF(0.05f, 0.7f), RandF(0, 0.5f), RandF(-0.03f, 0.03f), RandF(-0.1f, 0.1f), RandF(0.03f, 0.12f), RandF(0, 1), RandF(-1, 1), RandI(0, 2) };
        hue0 = RandF(0, 1);
    }
    void Draw(Renderer& r, float dt) override {
        int folds = Style() == 0 ? 6 : Style() == 1 ? 8 : Style() == 2 ? 12 : 16;
        float m = MinDim(), R = sqrtf((float)width * width + (float)height * height) * 0.5f;
        // The centre wanders a little so the middle never sits still.
        float cx = sinf(t * 0.05f) * width * 0.06f, cy = cosf(t * 0.04f) * height * 0.06f;
        { Canvas2D bg; bg.Begin(width, height);
          Color st[3] = { Hsv(hue0 + t * 0.01f, 0.7f, 0.25f), Hsv(hue0 + 0.3f + t * 0.01f, 0.7f, 0.12f), Hsv(hue0 + 0.6f + t * 0.01f, 0.7f, 0.25f) };
          aero::VerticalGradient(bg, width, height, st, 3); bg.Draw(r, BLEND_OPAQUE); }
        Canvas2D c; c.Begin(width, height);
        float wedge = 2 * kPi / folds, spin = t * 0.05f * Spd();
        for (auto& s : shapes) {
            s.r += s.vr * dt * Spd(); s.a += s.va * dt * Spd();
            if (s.r < 0.03f || s.r > 0.75f) s.vr = -s.vr;
            float a = fmodf(fabsf(s.a), wedge * 0.5f);   // keep within half a wedge, then mirror
            Color col = Hsv(s.hue + t * 0.03f, 0.8f, 1).WithAlpha(0.7f);
            for (int f = 0; f < folds; f++)
                for (int mirror = 0; mirror < 2; mirror++) {
                    float ang = spin + f * wedge + (mirror ? -a : a);
                    float x = cx + cosf(ang) * s.r * R, y = cy + sinf(ang) * s.r * R, sz = s.size * m;
                    if (s.kind == 0) c.Dot(x, y, sz, col);
                    else {
                        float a2 = ang + s.spin * t;
                        c.Line(x - cosf(a2) * sz, y - sinf(a2) * sz, x + cosf(a2) * sz, y + sinf(a2) * sz, sz * 0.12f, col, col.WithAlpha(0.1f));
                    }
                }
        }
        c.Draw(r, BLEND_ADD, dot);
    }

};

Scene* CreateScene() { return new SaverScene; }
