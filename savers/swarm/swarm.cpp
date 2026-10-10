// swarm.cpp - "OLED Swarm": a flock of glowing birds swirling with short light trails
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Swarm Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Birds", L"Count", 40, L"Few", L"Many" } },
};

const wchar_t* RegistryName() { return L"OledSwarm"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct B { Vec3 p, v; Trail tr; float hue; };
    std::vector<B> bs;
    void Setup() override {
        int n = preview ? 30 : 30 + (int)(Amt() * 150);
        for (int i = 0; i < n; i++) {
            B b; b.p = Vec3(RandF(-3, 3), RandF(-2, 2), RandF(-3, 3)); b.v = Normalize(Vec3(RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)));
            b.tr.life = 0.45f; b.tr.rate = 40; b.hue = RandF(0, 1); bs.push_back(b);
        }
    }
    void Tick(float dt) override {
        cam.Orbit(Drift(t, 0.6f), 13, t * 0.05f, 0.25f * sinf(t * 0.04f), 55, width, height);
        pt.fogNear = 9; pt.fogFar = 21;
        float k = dt * Spd();
        for (auto& b : bs) {
            Vec3 coh, ali, sep; int cnt = 0;
            for (auto& o : bs) {
                if (&o == &b) continue;
                Vec3 d = o.p - b.p; float dd = Dot(d, d);
                if (dd > 2.25f) continue;
                coh = coh + o.p; ali = ali + o.v; cnt++;
                if (dd < 0.36f) sep = sep - d * (1 / (dd + 0.05f));
            }
            Vec3 acc = sep * 0.15f;
            if (cnt) acc = acc + (coh * (1.0f / cnt) - b.p) * 0.6f + (ali * (1.0f / cnt) - b.v) * 0.8f;
            float r = Length(b.p);
            if (r > 5) acc = acc - b.p * ((r - 5) * 0.5f);   // stay in view
            acc = acc + Vec3(sinf(t * 0.3f), 0, cosf(t * 0.25f)) * 0.3f;
            b.v = b.v + acc * k;
            float sp = Length(b.v);
            b.v = b.v * (fminf(2.4f, fmaxf(1.3f, sp)) / (sp + 1e-4f));
        }
        for (auto& b : bs) {
            b.p = b.p + b.v * k;
            b.tr.Push(b.p, t);
            b.tr.Draw(pt, cam, t, [&](float) { return Col(b.hue); }, 1.3f);
            pt.Point3(cam, b.p, Lerp(Col(b.hue), Color(1, 1, 1), 0.4f), 2.5f);
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
