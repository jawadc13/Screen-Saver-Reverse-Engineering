// spirograph.cpp - "OLED Spirograph 3D": a pen tracing ever-changing 3D spirograph loops that fade behind it
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Spirograph 3D Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Trail length", L"Trail", 50, L"Short", L"Long" } },
};

const wchar_t* RegistryName() { return L"OledSpirograph"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    Trail tr;
    float s = 0;
    void Setup() override { tr.life = 4 + Amt() * 12; }
    Vec3 Pen(float u) const {
        float R = 5, r = 1.6f + 1.1f * sinf(t * 0.011f), d = 1.3f + 0.8f * sinf(t * 0.017f + 1);
        float k = (R - r) / r;
        return Vec3((R - r) * cosf(u) + d * cosf(k * u), 1.3f * sinf(u * 3.0f + t * 0.05f), (R - r) * sinf(u) - d * sinf(k * u)) * 0.45f;
    }
    void Tick(float dt) override {
        cam.Orbit(Drift(t, 0.4f), 7.5f, t * 0.04f, 0.6f + 0.2f * sinf(t * 0.03f), 55, width, height);
        s += dt * Spd() * 2.5f;
        tr.Push(Pen(s), t);
        tr.Draw(pt, cam, t, [&](float x) { return Col(fmodf(s * 0.02f + x * 0.5f, 1)); }, 1.5f);
        pt.Point3(cam, Pen(s), Color(1, 1, 1), 5);
    }

};

Scene* CreateScene() { return new SaverScene; }
