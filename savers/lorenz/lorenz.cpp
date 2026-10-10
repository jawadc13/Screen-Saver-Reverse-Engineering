// lorenz.cpp - "OLED Lorenz Attractor": glowing tracers drawing the Lorenz butterfly
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Lorenz Attractor Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Tracers", L"Count", 30, L"One", L"Six" } },
};

const wchar_t* RegistryName() { return L"OledLorenz"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct Tracer { float x, y, z, hue; Trail trail; };
    std::vector<Tracer> tr;
    static void StepL(Tracer& a, float h) {
        const float s = 10, rr = 28, b = 8.0f / 3;
        float dx = s * (a.y - a.x), dy = a.x * (rr - a.z) - a.y, dz = a.x * a.y - b * a.z;
        a.x += dx * h; a.y += dy * h; a.z += dz * h;
    }
    static Vec3 P(const Tracer& a) { return Vec3(a.x, a.z - 25, a.y) * 0.12f; }
    void Setup() override {
        int n = 1 + (int)(Amt() * 5);
        for (int i = 0; i < n; i++) {
            Tracer a; a.x = 0.1f + i * 0.3f; a.y = 0; a.z = 20 + i; a.hue = (float)i / n;
            a.trail.life = 7;
            for (int k = 0; k < 3000; k++) StepL(a, 0.005f);   // settle onto the attractor
            tr.push_back(a);
        }
    }
    void Tick(float dt) override {
        cam.Orbit(Drift(t, 0.4f), 9, t * 0.08f, 0.25f * sinf(t * 0.05f), 50, width, height);
        float sim = dt * Spd() * 0.5f;
        int steps = (int)(sim / 0.002f) + 1;
        for (auto& a : tr) {
            for (int k = 0; k < steps; k++) StepL(a, sim / steps);
            a.trail.Push(P(a), t);
            a.trail.Draw(pt, cam, t, [&](float x) { return Col(fmodf(a.hue + x * 0.4f, 1)); }, 1.6f);
            pt.Point3(cam, P(a), Color(1, 1, 1), 5);
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
