// torusknot.cpp - "OLED Torus Knot": glowing torus knots that cross-fade into new knots
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Torus Knot Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Line thickness", L"Thickness", 40, L"Fine", L"Bold" } },
};

const wchar_t* RegistryName() { return L"OledTorusKnot"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    int p = 2, q = 3, op = 2, oq = 3;
    float blend = 1, timer = 16, spin = 0;
    static int Gcd(int a, int b) { return b ? Gcd(b, a % b) : a; }
    void Knot(int kp, int kq, float alpha) {
        const int n = 700;
        Mat4 R = Mat4::Rotate(spin, 0.3f, 1, 0.2f);
        Vec3 prev;
        for (int i = 0; i <= n; i++) {
            float s = 2 * kPi * i / n, r = cosf(kq * s) + 2.2f;
            Vec3 l(r * cosf(kp * s), r * sinf(kp * s), -sinf(kq * s));
            Vec3 w(R.m[0][0] * l.x + R.m[0][1] * l.y + R.m[0][2] * l.z, R.m[1][0] * l.x + R.m[1][1] * l.y + R.m[1][2] * l.z,
                   R.m[2][0] * l.x + R.m[2][1] * l.y + R.m[2][2] * l.z);
            w = w * 0.85f;
            if (i > 0) pt.Line3(cam, prev, w, Col((float)(i - 1) / n) * alpha, Col((float)i / n) * alpha, 1.0f + Amt() * 3);
            prev = w;
        }
    }
    void Tick(float dt) override {
        spin += dt * Spd() * 12;
        cam.Orbit(Drift(t, 0.4f), 8, t * 0.05f, 0.3f * sinf(t * 0.04f), 50, width, height);
        timer -= dt;
        if (timer <= 0) {
            op = p; oq = q;
            do { p = RandI(2, 5); q = RandI(3, 8); } while (p == q || Gcd(p, q) != 1 || (p == op && q == oq));
            blend = 0; timer = 16;
        }
        blend = fminf(1, blend + dt / 3);
        float e = blend * blend * (3 - 2 * blend);
        if (e < 1) Knot(op, oq, 1 - e);
        Knot(p, q, e);
    }

};

Scene* CreateScene() { return new SaverScene; }
