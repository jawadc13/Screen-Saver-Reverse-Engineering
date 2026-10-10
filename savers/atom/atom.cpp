// atom.cpp - "OLED Atom": electrons racing round a nucleus, leaving glowing trails
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Atom Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Electrons", L"Count", 40, L"Two", L"Eight" } },
};

const wchar_t* RegistryName() { return L"OledAtom"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct E { Vec3 u, v; float a, b, speed, ph; Trail tr; };
    std::vector<E> es;
    void Setup() override {
        int n = 2 + (int)(Amt() * 6);
        for (int i = 0; i < n; i++) {
            E e;
            Vec3 nrm = Normalize(Vec3(RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)));
            e.u = Perpendicular(nrm); e.v = Cross(nrm, e.u);
            e.a = RandF(1.8f, 2.6f); e.b = e.a * RandF(0.45f, 1.0f);
            e.speed = RandF(1.8f, 3.2f) * (RandI(0, 1) ? 1 : -1);
            e.ph = RandF(0, 6.28f);
            e.tr.life = 0.9f;
            es.push_back(e);
        }
    }
    void Tick(float dt) override {
        Vec3 c = Drift(t, 0.5f);
        cam.Orbit(c, 6.2f, t * 0.06f, 0.4f * sinf(t * 0.05f), 50, width, height);
        pt.fogNear = 6; pt.fogFar = 11;
        // Nucleus: a small cluster of protons and neutrons, slowly tumbling.
        for (int i = 0; i < 14; i++) {
            float y = 1 - 2 * (i + 0.5f) / 14, rr = sqrtf(1 - y * y), ph = i * 2.39996f + t * 0.4f;
            pt.Point3(cam, Vec3(cosf(ph) * rr, y, sinf(ph) * rr) * 0.28f, Col(i % 2 ? 0.0f : 1.0f) * 0.7f, 4);
        }
        for (auto& e : es) {
            Circle3(pt, cam, Vec3(0, 0, 0), e.u, e.v, e.a, e.b, 72, [&](float) { return Col(0.5f) * 0.14f; }, 0.9f);
            float ang = e.ph + t * e.speed * Spd();
            Vec3 p = e.u * (cosf(ang) * e.a) + e.v * (sinf(ang) * e.b);
            e.tr.Push(p, t);
            e.tr.Draw(pt, cam, t, [&](float x) { return Col(0.3f + 0.7f * x); }, 1.8f);
            pt.Point3(cam, p, Color(1, 1, 1), 4.5f);
        }
    }

};

Scene* CreateScene() { return new SaverScene; }
