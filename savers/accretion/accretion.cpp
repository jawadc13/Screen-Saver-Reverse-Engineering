// accretion.cpp - "OLED Accretion Disk": a black hole swallowing a glowing, swirling disk of matter
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Accretion Disk Settings",
    1, { OLED_COLORS_CHOICE_DEF(2) },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Particles", L"Particles", 50, L"Few", L"Many" } },
};

const wchar_t* RegistryName() { return L"OledAccretion"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct M { float r, a, y, age; };
    std::vector<M> ms;
    void Spawn(M& m, bool anywhere) {
        m.r = anywhere ? RandF(1.15f, 6) : RandF(5.5f, 6);
        m.a = RandF(0, 6.28f); m.y = RandF(-1, 1) * 0.06f * m.r; m.age = anywhere ? 2 : 0;
    }
    void Setup() override {
        ms.resize(preview ? 800 : 1500 + (int)(Amt() * 6000));
        for (auto& m : ms) Spawn(m, true);
    }
    void Tick(float dt) override {
        cam.Orbit(Drift(t, 0.6f), 7.5f, t * 0.04f, 0.3f + 0.12f * sinf(t * 0.03f), 50, width, height);
        float k = dt * Spd();
        for (auto& m : ms) {
            m.a += k * 1.6f / powf(m.r, 1.5f);           // Keplerian: inner matter orbits faster
            m.r -= k * 0.06f * (6.5f / m.r);              // slowly spirals in
            m.y *= 1 - k * 0.05f;
            m.age += dt;
            if (m.r < 1.05f) Spawn(m, false);
            float fadeIn = fminf(1, m.age), fadeOut = fminf(1, (m.r - 1.05f) / 0.35f);
            float heat = 1 - fminf(1, (m.r - 1.05f) / 3.0f);   // hotter nearer the hole
            Color c = Lerp(Col(0.3f) * 0.6f, Color(1, 0.95f, 0.85f), heat * heat) * (fadeIn * fadeOut * (0.35f + 0.65f * heat));
            pt.Point3(cam, Vec3(cosf(m.a) * m.r, m.y, sinf(m.a) * m.r), c * 1.3f, 1.4f, 0.8f);
        }
        // Faint photon ring round the black centre.
        Circle3(pt, cam, Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 0, 1), 1.0f, 1.0f, 96, [&](float) { return Col(0.7f) * 0.35f; }, 1.2f);
    }

};

Scene* CreateScene() { return new SaverScene; }
