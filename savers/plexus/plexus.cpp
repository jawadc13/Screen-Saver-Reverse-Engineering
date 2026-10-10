// plexus.cpp - "OLED Plexus": drifting points that link up with glowing lines when close
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Plexus Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Points", L"Points", 40, L"Few", L"Many" } },
};

const wchar_t* RegistryName() { return L"OledPlexus"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct N { Vec3 p, v; };
    std::vector<N> ns;
    Vec3 box;
    void Setup() override {
        int n = preview ? 40 : 40 + (int)(Amt() * 160);
        for (int i = 0; i < n; i++) ns.push_back({ Vec3(RandF(-3, 3), RandF(-2, 2), RandF(-2, 2)), Normalize(Vec3(RandF(-1, 1), RandF(-1, 1), RandF(-1, 1))) * RandF(0.15f, 0.4f) });
    }
    void Tick(float dt) override {
        float aspect = (float)width / height;
        box = Vec3(3.5f * fmaxf(1, aspect), 3.5f * fmaxf(1, 1 / aspect), 2.5f);
        cam.Orbit(Drift(t, 0.4f), 10, t * 0.04f, 0.2f * sinf(t * 0.05f), 55, width, height);
        pt.fogNear = 7; pt.fogFar = 15;
        for (auto& n : ns) {
            n.p = n.p + n.v * (dt * Spd());
            float* c[3] = { &n.p.x, &n.p.y, &n.p.z }; float* v[3] = { &n.v.x, &n.v.y, &n.v.z }; float lim[3] = { box.x, box.y, box.z };
            for (int k = 0; k < 3; k++) { if (*c[k] > lim[k]) *v[k] = -fabsf(*v[k]); if (*c[k] < -lim[k]) *v[k] = fabsf(*v[k]); }
        }
        const float d0 = 1.8f;
        for (size_t i = 0; i < ns.size(); i++) {
            for (size_t j = i + 1; j < ns.size(); j++) {
                Vec3 d = ns[i].p - ns[j].p;
                float dd = Dot(d, d);
                if (dd > d0 * d0) continue;
                float a = 1 - sqrtf(dd) / d0;
                a = a * a;   // fades smoothly to nothing at the threshold
                pt.Line3(cam, ns[i].p, ns[j].p, Col((float)i / ns.size()) * a, Col((float)j / ns.size()) * a, 1.2f);
            }
            pt.Point3(cam, ns[i].p, Col((float)i / ns.size()), 2.5f);
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
