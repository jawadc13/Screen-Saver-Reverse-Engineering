// dnahelix.cpp - "OLED DNA Helix": a slowly turning double helix of glowing bases
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED DNA Helix Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Twist", L"Twist", 50, L"Loose", L"Tight" } },
};

const wchar_t* RegistryName() { return L"OledDNA"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    float phase = 0;
    // The helix runs along the screen's long side.
    Vec3 AxisPoint(float u, float c, float s) const {
        const float r = 1.0f;
        return width >= height ? Vec3(u, c * r, s * r) : Vec3(c * r, u, s * r);
    }
    void Tick(float dt) override {
        phase += dt * Spd() * 1.2f;
        float aspect = (float)width / height;
        float len = (aspect >= 1 ? aspect : 1 / aspect) * 6.0f;
        Vec3 target = Drift(t, 0.4f);
        float sway = 0.5f * sinf(t * 0.05f);
        Vec3 eye = width >= height ? target + Vec3(sinf(sway) * 7, 1.5f * sinf(t * 0.07f), cosf(sway) * 7)
                                   : target + Vec3(sinf(sway) * 7, 0, cosf(sway) * 7);
        cam.Set(eye, target, 52, width, height);
        pt.fogNear = 6; pt.fogFar = 9.5f;
        const int n = 140;
        float twist = 0.5f + Amt() * 1.2f;
        Vec3 pa, pb;
        for (int i = 0; i <= n; i++) {
            float u = -len / 2 + len * i / n;
            float a = u * twist + phase;
            float e = 1 - powf(fabsf(u) / (len / 2), 6);   // fade out at the ends
            Vec3 a1 = AxisPoint(u, cosf(a), sinf(a)), b1 = AxisPoint(u, cosf(a + kPi), sinf(a + kPi));
            if (i > 0) {
                pt.Line3(cam, pa, a1, Col(0.1f) * e, Col(0.1f) * e, 1.4f);
                pt.Line3(cam, pb, b1, Col(0.9f) * e, Col(0.9f) * e, 1.4f);
            }
            if (i % 4 == 0) {   // base pairs
                Vec3 mid = (a1 + b1) * 0.5f;
                pt.Line3(cam, a1, mid, Col(0.1f) * (0.45f * e), Col(0.5f) * (0.45f * e), 1.0f);
                pt.Line3(cam, mid, b1, Col(0.5f) * (0.45f * e), Col(0.9f) * (0.45f * e), 1.0f);
                pt.Point3(cam, a1, Col(0.1f) * e, 3);
                pt.Point3(cam, b1, Col(0.9f) * e, 3);
            }
            pa = a1; pb = b1;
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
