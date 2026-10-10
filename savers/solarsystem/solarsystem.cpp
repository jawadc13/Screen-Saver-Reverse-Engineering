// solarsystem.cpp - "OLED Solar System": planets and moons wheeling round a softly glowing sun
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Solar System Settings",
    1, { OLED_COLORS_CHOICE_DEF(5) },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Planets", L"Planets", 50, L"Three", L"Nine" } },
};

const wchar_t* RegistryName() { return L"OledSolarSystem"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct Pl { float a, e, period, size, ph, tilt; bool moon; float moonPh; };
    std::vector<Pl> pls;
    void Setup() override {
        int n = 3 + (int)(Amt() * 6);
        for (int i = 0; i < n; i++) {
            float a = 1.3f + i * 0.8f + RandF(-0.1f, 0.1f);
            pls.push_back({ a, RandF(0, 0.12f), powf(a, 1.5f) * 2.5f, RandF(2.5f, 5.5f), RandF(0, 6.28f), RandF(-0.06f, 0.06f), i % 3 == 2, RandF(0, 6.28f) });
        }
    }
    void Tick(float dt) override {
        float rmax = pls.back().a;
        // The sun drifts around the screen (via the camera target) instead of
        // sitting on the same pixels.
        cam.Orbit(Drift(t, rmax * 0.25f), rmax * 1.7f, t * 0.03f, 0.45f + 0.12f * sinf(t * 0.03f), 50, width, height);
        pt.Point3(cam, Vec3(0, 0, 0), Color(1, 0.8f, 0.45f) * 0.35f, 35);
        pt.Point3(cam, Vec3(0, 0, 0), Color(1, 0.95f, 0.8f), 9);
        for (size_t i = 0; i < pls.size(); i++) {
            Pl& p = pls[i];
            p.ph += dt * Spd() * 2 * kPi / p.period;
            float b = p.a * sqrtf(1 - p.e * p.e);
            Vec3 u(1, p.tilt, 0), v(0, -p.tilt, 1);
            Circle3(pt, cam, Vec3(-p.a * p.e, 0, 0), u, v, p.a, b, 120, [&](float) { return Col((float)i / pls.size()) * 0.08f; }, 0.8f);
            Vec3 pos = Vec3(-p.a * p.e, 0, 0) + u * (cosf(p.ph) * p.a) + v * (sinf(p.ph) * b);
            pt.Point3(cam, pos, Col((float)i / pls.size()), p.size);
            if (p.moon) {
                p.moonPh += dt * Spd() * 3;
                pt.Point3(cam, pos + Vec3(cosf(p.moonPh), 0.2f * sinf(p.moonPh), sinf(p.moonPh)) * 0.35f, Color(0.8f, 0.8f, 0.85f) * 0.7f, 1.8f);
            }
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
