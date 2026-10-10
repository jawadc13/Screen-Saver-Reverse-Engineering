// fountain.cpp - "OLED Particle Fountain": a fountain of glowing sparks arcing and bouncing
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Particle Fountain Settings",
    1, { OLED_COLORS_CHOICE },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Particles", L"Rate", 50, L"Few", L"Many" } },
};

const wchar_t* RegistryName() { return L"OledFountain"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct P { Vec3 p, v; float life, hue; };
    std::vector<P> ps;
    float emit = 0;
    void Tick(float dt) override {
        Vec3 c = Drift(t, 0.6f);
        cam.Orbit(c + Vec3(0, 2, 0), 10, t * 0.07f, 0.25f + 0.1f * sinf(t * 0.05f), 55, width, height);
        pt.fogNear = 7; pt.fogFar = 18;
        float k = Spd() * 0.6f;
        emit += dt * (60 + Amt() * 500);
        while (emit >= 1) {
            emit -= 1;
            float th = RandF(0, 0.3f), ph = RandF(0, 6.28f), sp = RandF(5.5f, 7.0f);
            ps.push_back({ Vec3(0, 0, 0), Vec3(sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph)) * sp, 3.5f, fmodf(t * 0.05f, 1) + RandF(0, 0.15f) });
        }
        for (auto& q : ps) {
            q.v.y -= 9.8f * dt * k;
            q.p = q.p + q.v * (dt * k);
            if (q.p.y < 0) { q.p.y = 0; q.v.y *= -0.35f; q.v.x *= 0.7f; q.v.z *= 0.7f; }
            q.life -= dt * k;
            float a = fminf(1, q.life) * fminf(1, (3.5f - q.life) * 10);
            if (a <= 0) continue;
            Color col = Col(fmodf(q.hue, 1)) * a;
            pt.Line3(cam, q.p - q.v * 0.04f, q.p, col * 0.2f, col, 1.2f);
            pt.Point3(cam, q.p, col, 2.2f);
        }
        ps.erase(std::remove_if(ps.begin(), ps.end(), [](const P& q) { return q.life <= 0; }), ps.end());
    }

};

Scene* CreateScene() { return new SaverScene; }
