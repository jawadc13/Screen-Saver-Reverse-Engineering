// lissajous.cpp - "OLED Lissajous Knots": 3D Lissajous curves that morph smoothly from one knot to the next
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Lissajous Knots Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Complexity", L"Complexity", 50, L"Simple", L"Intricate" } },
};

const wchar_t* RegistryName() { return L"OledLissajous"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    float f[3] = { 1, 2, 3 }, g[3] = { 1, 2, 3 };   // current and target frequencies
    float timer = 0, delta = 0;
    void Tick(float dt) override {
        timer -= dt;
        if (timer <= 0) {
            int maxF = 2 + (int)(Amt() * 6);
            for (int i = 0; i < 3; i++) g[i] = (float)RandI(1, maxF);
            timer = 14;
        }
        for (int i = 0; i < 3; i++) f[i] += (g[i] - f[i]) * (1 - expf(-dt * 0.35f));   // smooth morph
        delta += dt * 0.25f * Spd();
        cam.Orbit(Drift(t, 0.4f), 7.5f, t * 0.06f, 0.3f * sinf(t * 0.045f), 50, width, height);
        const int n = 900;
        Vec3 prev;
        for (int i = 0; i <= n; i++) {
            float s = 2 * kPi * i / n;
            Vec3 p(sinf(f[0] * s + delta), sinf(f[1] * s), sinf(f[2] * s + delta * 0.5f));
            p = p * 2.3f;
            if (i > 0) pt.Line3(cam, prev, p, Col((float)(i - 1) / n), Col((float)i / n), 1.4f);
            prev = p;
        }
        float s = fmodf(t * Spd(), 2 * kPi);
        pt.Point3(cam, Vec3(sinf(f[0] * s + delta), sinf(f[1] * s), sinf(f[2] * s + delta * 0.5f)) * 2.3f, Color(1, 1, 1), 5);
    }

};

Scene* CreateScene() { return new SaverScene; }
