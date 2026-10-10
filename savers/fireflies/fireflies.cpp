// fireflies.cpp - "OLED Fireflies": fireflies drifting through the dark, blinking softly
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Fireflies Settings",
    1, { OLED_COLORS_CHOICE_DEF(2) },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Fireflies", L"Count", 40, L"Few", L"Many" } },
};

const wchar_t* RegistryName() { return L"OledFireflies"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct F { Vec3 p, v; float ph, rate, hue; };
    std::vector<F> fs;
    void Setup() override {
        int n = 20 + (int)(Amt() * 130);
        for (int i = 0; i < n; i++) fs.push_back({ Vec3(RandF(-6, 6), RandF(-3, 3), RandF(-4, 4)), Vec3(), RandF(0, 6.28f), RandF(0.7f, 1.6f), RandF(0, 1) });
    }
    void Tick(float dt) override {
        float aspect = (float)width / height;
        Vec3 box(6 * fmaxf(1, aspect), 4 * fmaxf(1, 1 / aspect), 4);
        cam.Set(Vec3(sinf(t * 0.02f) * 1.5f, cosf(t * 0.017f) * 0.8f, 10), Drift(t, 0.8f), 60, width, height);
        pt.fogNear = 7; pt.fogFar = 18;
        for (auto& f : fs) {
            Vec3 want(sinf(t * 0.3f + f.ph * 3), sinf(t * 0.23f + f.ph * 5) * 0.6f, cosf(t * 0.27f + f.ph * 7));
            f.v = f.v + (want * 0.5f - f.v) * fminf(1, dt * 0.6f);
            f.p = f.p + f.v * (dt * Spd());
            if (fabsf(f.p.x) > box.x) f.v.x -= f.p.x * 0.05f;
            if (fabsf(f.p.y) > box.y) f.v.y -= f.p.y * 0.05f;
            if (fabsf(f.p.z) > box.z) f.v.z -= f.p.z * 0.05f;
            float glow = powf(fmaxf(0, sinf(t * f.rate + f.ph)), 6);   // soft blink
            pt.Point3(cam, f.p, Col(f.hue) * (0.06f + glow), 3 + 9 * glow);
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
