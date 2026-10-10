// ripples.cpp - "OLED Rain Ripples": raindrops landing on dark water, rings spreading and fading
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Rain Ripples Settings",
    1, { OLED_COLORS_CHOICE_DEF(4) },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Rain", L"Rain", 40, L"Drizzle", L"Downpour" } },
};

const wchar_t* RegistryName() { return L"OledRipples"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct Ring { Vec3 c; float age, speed; };
    std::vector<Ring> rings;
    float spawn = 0;
    void Tick(float dt) override {
        Vec3 c = Drift(t, 1.5f); c.y = 0;
        cam.Set(c + Vec3(sinf(t * 0.03f) * 2, 9, 9 + cosf(t * 0.02f) * 2), c, 55, width, height);
        pt.fogNear = 9; pt.fogFar = 24;
        float aspect = (float)width / height;
        float ex = 9 * fmaxf(1, aspect), ez = 9 * fmaxf(1, 1 / aspect);
        spawn -= dt * (1 + Amt() * 9);
        while (spawn <= 0) {
            Vec3 p(c.x + RandF(-ex, ex), 0, c.z + RandF(-ez, ez) * 0.7f);
            float sp = RandF(0.8f, 1.2f);
            rings.push_back({ p, 0, sp });
            rings.push_back({ p, -0.28f, sp * 0.8f });   // second, slower ring
            spawn += RandF(0.3f, 1.0f);
        }
        const float life = 3.2f;
        for (auto& rg : rings) {
            rg.age += dt * Spd();
            if (rg.age <= 0) continue;
            float a = 1 - rg.age / life;
            if (a <= 0) continue;
            float r = rg.age * rg.speed;
            if (rg.age < 0.25f) pt.Point3(cam, rg.c, Color(1, 1, 1) * (1 - rg.age / 0.25f), 3);   // splash
            Circle3(pt, cam, rg.c, Vec3(1, 0, 0), Vec3(0, 0, 1), r, r, 48, [&](float) { return Col(0.5f) * (a * a * 0.8f); }, 1.1f);
        }
        rings.erase(std::remove_if(rings.begin(), rings.end(), [&](const Ring& g) { return g.age >= life; }), rings.end());
    }

};

Scene* CreateScene() { return new SaverScene; }
