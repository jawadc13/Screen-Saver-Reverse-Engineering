// wavegrid.cpp - "OLED Wave Grid": a field of glowing dots rippling with interfering waves
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Wave Grid Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Grid density", L"Density", 40, L"Coarse", L"Fine" } },
};

const wchar_t* RegistryName() { return L"OledWaveGrid"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    void Tick(float dt) override {
        cam.Orbit(Drift(t, 0.8f), 12, t * 0.05f, 0.55f + 0.1f * sinf(t * 0.04f), 50, width, height);
        pt.fogNear = 9; pt.fogFar = 20;
        int n = 24 + (int)(Amt() * 36);
        float half = 6, step = 2 * half / (n - 1), tt = t * Spd();
        Vec3 src[3] = { Vec3(sinf(tt * 0.31f) * 4, 0, cosf(tt * 0.23f) * 4), Vec3(cosf(tt * 0.19f) * 4, 0, sinf(tt * 0.27f) * 4), Vec3(sinf(tt * 0.13f + 2) * 3, 0, cosf(tt * 0.17f + 1) * 3) };
        std::vector<Vec3> row(n), prevRow(n);
        for (int j = 0; j < n; j++) {
            for (int i = 0; i < n; i++) {
                float x = -half + i * step, z = -half + j * step, y = 0;
                for (auto& s : src) {
                    float d = sqrtf((x - s.x) * (x - s.x) + (z - s.z) * (z - s.z));
                    y += 0.45f * sinf(d * 2.2f - tt * 2.5f) / (1 + 0.35f * d);
                }
                row[i] = Vec3(x, y, z);
                Color c = Col(0.5f + y * 0.7f) * (0.55f + 0.45f * fminf(1, fabsf(y) * 2 + 0.2f));
                pt.Point3(cam, row[i], c, 2.2f, 0.8f);
                if (i > 0) pt.Line3(cam, row[i - 1], row[i], c * 0.3f, c * 0.3f, 0.9f);
            }
            prevRow.swap(row);
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
