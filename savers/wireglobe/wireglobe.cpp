// wireglobe.cpp - "OLED Wire Globe": a rotating wireframe Earth with glowing arcs between cities
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Wire Globe Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Arcs", L"Arcs", 50, L"Few", L"Many" } },
};

const wchar_t* RegistryName() { return L"OledGlobe"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct City { Vec3 p; float phase; };
    struct Arc { int a, b; float age, dur; };
    std::vector<City> cities;
    std::vector<Arc> arcs;
    float spawn = 0, rot = 0;
    Mat4 R;
    Vec3 W(const Vec3& p) const {
        return Vec3(R.m[0][0] * p.x + R.m[0][1] * p.y + R.m[0][2] * p.z,
                    R.m[1][0] * p.x + R.m[1][1] * p.y + R.m[1][2] * p.z,
                    R.m[2][0] * p.x + R.m[2][1] * p.y + R.m[2][2] * p.z) * 2.0f;
    }
    static Vec3 Sph(float lat, float lon) { return Vec3(cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon)); }
    void Setup() override {
        for (int i = 0; i < 40; i++) cities.push_back({ Normalize(Vec3(RandF(-1, 1), RandF(-0.8f, 0.8f), RandF(-1, 1))), RandF(0, 6.28f) });
    }
    void Tick(float dt) override {
        rot += dt * Spd() * 8;
        R = Mat4::Rotate(23.5f, 0, 0, 1) * Mat4::Rotate(rot, 0, 1, 0);
        cam.Orbit(Drift(t, 0.5f), 6.5f, t * 0.03f, 0.3f * sinf(t * 0.04f), 45, width, height);
        pt.fogNear = 5.0f; pt.fogFar = 9.0f;   // the far side of the globe dims
        auto grid = [&](float) { return Col(0.2f) * 0.3f; };
        for (int la = -75; la <= 75; la += 15) {
            Vec3 prev = W(Sph(la * kPi / 180, 0));
            for (int i = 1; i <= 72; i++) { Vec3 p = W(Sph(la * kPi / 180, i * kPi / 36)); pt.Line3(cam, prev, p, grid(0), grid(0), 0.9f); prev = p; }
        }
        for (int lo = 0; lo < 180; lo += 15) {
            Vec3 prev = W(Sph(-kPi / 2, lo * kPi / 180));
            for (int i = 1; i <= 72; i++) { Vec3 p = W(Sph(-kPi / 2 + i * kPi / 36, lo * kPi / 180)); pt.Line3(cam, prev, p, grid(0), grid(0), 0.9f); prev = p; }
        }
        for (auto& c : cities) pt.Point3(cam, W(c.p), Col(0.8f) * (0.4f + 0.6f * powf(0.5f + 0.5f * sinf(t * 2 + c.phase), 3)), 3);
        // Arcs between cities: the head races ahead, the tail follows and fades.
        spawn -= dt * Spd();
        if (spawn <= 0) {
            int a = RandI(0, (int)cities.size() - 1), b = RandI(0, (int)cities.size() - 1);
            if (a != b && Dot(cities[a].p, cities[b].p) > -0.6f) arcs.push_back({ a, b, 0, RandF(1.5f, 2.5f) });
            spawn = RandF(0.3f, 1.2f) * (1.6f - Amt() * 1.4f);
        }
        for (auto& ar : arcs) {
            ar.age += dt * Spd();
            float head = ar.age / ar.dur * 1.6f, tail = head - 0.6f;
            const int segs = 40;
            Vec3 prev; bool first = true;
            for (int i = 0; i <= segs; i++) {
                float s = (float)i / segs;
                if (s > head) break;
                Vec3 p = W(Normalize(cities[ar.a].p * (1 - s) + cities[ar.b].p * s) * (1 + 0.3f * sinf(kPi * s)));
                if (!first && s >= tail) {
                    float a = fminf(1, (s - tail) / 0.6f);
                    pt.Line3(cam, prev, p, Col(1 - s) * a, Col(1 - s) * a, 1.6f);
                }
                prev = p; first = false;
            }
        }
        arcs.erase(std::remove_if(arcs.begin(), arcs.end(), [](const Arc& a) { return a.age / a.dur * 1.6f - 0.6f > 1; }), arcs.end());
    }

};

Scene* CreateScene() { return new SaverScene; }
