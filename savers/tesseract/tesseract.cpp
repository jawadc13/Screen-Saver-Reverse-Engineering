// tesseract.cpp - "OLED Tesseract": a 4D hypercube turning through the fourth dimension
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Tesseract Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Size", L"Size", 50, L"Small", L"Large" } },
};

const wchar_t* RegistryName() { return L"OledTesseract"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    float a[4] = {};
    void Tick(float dt) override {
        float s = dt * Spd();
        a[0] += s * 0.31f; a[1] += s * 0.23f; a[2] += s * 0.17f; a[3] += s * 0.11f;
        cam.Orbit(Drift(t, 0.4f), 7, t * 0.05f, 0.35f * sinf(t * 0.04f), 50, width, height);
        float size = 0.8f + Amt() * 0.9f;
        Vec3 p3[16]; float wv[16];
        for (int i = 0; i < 16; i++) {
            float v[4] = { i & 1 ? 1.f : -1.f, i & 2 ? 1.f : -1.f, i & 4 ? 1.f : -1.f, i & 8 ? 1.f : -1.f };
            auto rot = [&](int p, int q, float ang) {
                float c = cosf(ang), sn = sinf(ang), x = v[p], y = v[q];
                v[p] = x * c - y * sn; v[q] = x * sn + y * c;
            };
            rot(0, 3, a[0]); rot(1, 3, a[1]); rot(2, 3, a[2]); rot(0, 1, a[3]);
            float k = 2.5f / (3.2f - v[3]);          // 4D -> 3D perspective
            p3[i] = Vec3(v[0], v[1], v[2]) * (k * size);
            wv[i] = (v[3] + 1.7f) / 3.4f;
        }
        for (int i = 0; i < 16; i++)
            for (int b = 0; b < 4; b++) {
                int j = i ^ (1 << b);
                if (j > i) pt.Line3(cam, p3[i], p3[j], Col(wv[i]), Col(wv[j]), 1.7f);
            }
        for (int i = 0; i < 16; i++) pt.Point3(cam, p3[i], Col(wv[i]) * 1.2f, 4);
    }

};

Scene* CreateScene() { return new SaverScene; }
