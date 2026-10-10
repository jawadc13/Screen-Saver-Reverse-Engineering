// gyrorings.cpp - "OLED Gyro Rings": nested rings spinning like a gyroscope, beads racing round them
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Gyro Rings Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Rings", L"Rings", 40, L"Three", L"Eight" } },
};

const wchar_t* RegistryName() { return L"OledGyroRings"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    float ang[8] = {};
    void Tick(float dt) override {
        cam.Orbit(Drift(t, 0.5f), 9, t * 0.05f, 0.3f * sinf(t * 0.04f), 50, width, height);
        int n = 3 + (int)(Amt() * 5);
        Mat4 M = Mat4::Identity();
        for (int i = 0; i < n; i++) {
            ang[i] += dt * Spd() * (20 + i * 9) * (i % 2 ? -1 : 1);
            M = M * Mat4::Rotate(ang[i], i % 2 ? 1.0f : 0.0f, 0, i % 2 ? 0.0f : 1.0f);
            float r = 2.8f - i * 0.3f;
            Vec3 u(M.m[0][0], M.m[1][0], M.m[2][0]), v(M.m[0][2], M.m[1][2], M.m[2][2]);
            Circle3(pt, cam, Vec3(0, 0, 0), u, v, r, r, 96, [&](float) { return Col((float)i / n) * 0.6f; }, 1.4f);
            float b = t * Spd() * (1.5f + i * 0.4f);
            pt.Point3(cam, u * (cosf(b) * r) + v * (sinf(b) * r), Lerp(Col((float)i / n), Color(1, 1, 1), 0.5f), 4.5f);
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
