// lightning.cpp - "OLED Lightning": branching lightning bolts flashing out of the darkness
// OLED friendly: pure black background, sparse light that keeps moving (see
// common/oledkit.h for the rules every OLED saver follows).
#include "../../common/oledkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"OLED Lightning Settings",
    1, { OLED_COLORS_CHOICE_DEF(4) },
    3, { OLED_SPEED_SLIDER, OLED_BRIGHT_SLIDER, { L"Storm intensity", L"Intensity", 40, L"Calm", L"Wild" } },
};

const wchar_t* RegistryName() { return L"OledLightning"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace oled;

class SaverScene : public Scene3D {
    DWORD BrightnessSlider() const override { return g_cfg.slider[1]; }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }   // speed factor
    float Amt() const { return g_cfg.slider[2] / 100.0f; }                  // saver-specific 0..1
    Color Col(float x) const { return Palette(g_cfg.choice[0], x); }

    struct Bolt { std::vector<Vec3> a, b; std::vector<float> w; float age; };
    std::vector<Bolt> bolts;
    float next = 1;
    void Branch(Bolt& bl, Vec3 p, Vec3 q, float disp, int depth, float width) {
        if (depth == 0) { bl.a.push_back(p); bl.b.push_back(q); bl.w.push_back(width); return; }
        Vec3 mid = (p + q) * 0.5f + Vec3(RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)) * disp;
        Branch(bl, p, mid, disp * 0.5f, depth - 1, width);
        Branch(bl, mid, q, disp * 0.5f, depth - 1, width);
        if (depth >= 3 && RandF(0, 1) < 0.35f) {   // side branch
            Vec3 dir = (q - p) * 0.7f + Vec3(RandF(-1, 1), RandF(-0.5f, 0), RandF(-1, 1)) * Length(q - p) * 0.6f;
            Branch(bl, mid, mid + dir, disp * 0.5f, depth - 2, width * 0.55f);
        }
    }
    void Tick(float dt) override {
        cam.Set(Vec3(sinf(t * 0.05f) * 2, sinf(t * 0.03f) * 0.5f, 11), Drift(t, 1.0f), 60, width, height);
        float aspect = (float)width / height;
        float ex = 4 * fmaxf(1, aspect), ey = 4 * fmaxf(1, 1 / aspect);
        next -= dt * Spd();
        if (next <= 0) {
            Bolt bl; bl.age = 0;
            Vec3 top(RandF(-ex, ex), ey, RandF(-2, 2)), bottom(top.x + RandF(-3, 3), -ey, RandF(-2, 2));
            Branch(bl, top, bottom, Length(bottom - top) * 0.18f, 7, 2.2f);
            bolts.push_back(bl);
            next = RandF(0.4f, 2.6f) * (1.5f - Amt() * 1.2f);
        }
        for (auto& bl : bolts) {
            bl.age += dt;
            // Flash, then fade, with a couple of flickering re-strikes.
            float f = expf(-bl.age * 5) * (0.65f + 0.35f * powf(fabsf(sinf(bl.age * 38)), 2)) + 0.5f * expf(-fabsf(bl.age - 0.22f) * 40);
            Color c = Lerp(Col(0.5f), Color(1, 1, 1), 0.65f) * f;
            for (size_t i = 0; i < bl.a.size(); i++) pt.Line3(cam, bl.a[i], bl.b[i], c, c, bl.w[i]);
        }
        bolts.erase(std::remove_if(bolts.begin(), bolts.end(), [](const Bolt& b) { return b.age > 1.4f; }), bolts.end());
    }

};

Scene* CreateScene() { return new SaverScene; }
