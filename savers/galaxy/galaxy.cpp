// galaxy.cpp - "OLED Spiral Galaxy": thousands of stars wheeling in spiral arms
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Spiral Galaxy Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Stars", L"Stars", 50, L"Few", L"Many" } },
};

const wchar_t* RegistryName() { return L"OledGalaxy"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct Star { float r, a, y, speed, size; Color c; };
    std::vector<Star> stars;
    void Setup() override {
        int n = preview ? 900 : 1500 + (int)(Amt() * 6500);
        int arms = RandI(2, 4);
        for (int i = 0; i < n; i++) {
            Star s;
            s.r = powf(RandF(0, 1), 0.7f) * 6 + 0.15f;
            int arm = RandI(0, arms - 1);
            s.a = arm * 2 * kPi / arms + s.r * 0.9f + RandF(-0.4f, 0.4f) * (1.3f - s.r / 6);
            s.y = RandF(-1, 1) * RandF(0, 1) * 0.35f * (1.2f - s.r / 7);
            s.speed = 0.5f / sqrtf(s.r + 0.3f);
            s.size = RandF(0.5f, 1.4f);
            float core = 1 - fminf(1, s.r / 2.5f);
            s.c = Lerp(Col(RandF(0, 1)), Color(1, 0.9f, 0.7f), core * core) * RandF(0.5f, 1.0f);
            stars.push_back(s);
        }
    }
    void Tick(float dt) override {
        cam.Orbit(Drift(t, 0.5f), 9, t * 0.04f, 0.55f + 0.18f * sinf(t * 0.03f), 58, width, height);
        pt.fogNear = 8; pt.fogFar = 17;
        for (auto& s : stars) {
            s.a += s.speed * dt * Spd() * 0.25f;
            pt.Point3(cam, Vec3(cosf(s.a) * s.r, s.y, sinf(s.a) * s.r), s.c, s.size * 1.3f, 0.7f);
        }
        pt.Point3(cam, Vec3(0, 0, 0), Color(1, 0.85f, 0.6f) * 0.25f, 50);   // soft core glow
    }

};

Scene* CreateScene() { return new SaverScene; }
