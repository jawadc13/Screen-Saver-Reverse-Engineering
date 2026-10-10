// synthgrid.cpp - "OLED Synthwave Grid": gliding over endless neon wireframe mountains
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Synthwave Grid Settings",
    1, { OLED_COLORS_CHOICE_DEF(1) },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Mountain height", L"Height", 50, L"Flat", L"Peaks" } },
};

const wchar_t* RegistryName() { return L"OledSynthGrid"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    float z0 = 0;
    float H(float x, float z) const {
        float m = 0.4f + Amt() * 3.0f;
        float valley = fminf(1, fabsf(x) / 5); valley *= valley;   // flat road down the middle
        float h = 0.55f * sinf(x * 0.35f + z * 0.21f) + 0.35f * sinf(x * 0.8f - z * 0.5f) + 0.25f * sinf(z * 0.9f + x * 0.13f);
        return m * valley * (h + 0.9f);
    }
    void Tick(float dt) override {
        z0 += dt * Spd() * 5;
        cam.Set(Vec3(sinf(t * 0.11f) * 0.9f, 2.3f + 0.3f * sinf(t * 0.13f), 0), Vec3(sinf(t * 0.07f) * 2, 1.2f, -20), 60, width, height);
        pt.fogNear = 6; pt.fogFar = 46;
        float base = floorf(z0), frac = z0 - base;
        const int rows = 48, half = 24;
        auto P = [&](int x, int j) { return Vec3((float)x, H((float)x, base + j), -(j - frac)); };
        auto C = [&](float h) { return Col(fminf(1, h / 3.5f)); };
        for (int j = 0; j < rows; j++)
            for (int x = -half; x < half; x++) {
                Vec3 a = P(x, j), b = P(x + 1, j), c = P(x, j + 1);
                float k = fminf(1, (j - frac) / 2.0f);   // new rows fade in under the camera
                k = k < 0 ? 0 : k;
                pt.Line3(cam, a, b, C(a.y) * (0.8f * k), C(b.y) * (0.8f * k), 1.1f);
                pt.Line3(cam, a, c, C(a.y) * (0.8f * k), C(c.y) * (0.8f * k), 1.1f);
            }
    }

};

Scene* CreateScene() { return new SaverScene; }
