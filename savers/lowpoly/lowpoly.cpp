// lowpoly.cpp - "Low-Poly Mountains": flying over flat-shaded pastel mountains and valleys
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Low-Poly Mountains Settings",
    2, { { L"&Palette:", L"Palette", 0, L"Spring|Autumn|Winter|Alien" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Mountain height", L"Height", 50, L"Hills", L"Alps" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullLowPoly"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

using namespace rest;

class SaverScene : public RestScene {
    void RestSettings(float& every, float& len, int& band) const override {
        every = 60 * (1 + g_cfg.slider[2] * 14 / 100.0f);
        len = 5 + g_cfg.slider[3] * 55 / 100.0f;
        band = (int)g_cfg.choice[1];
    }
    float Spd() const { return 0.2f + g_cfg.slider[0] / 100.0f * 1.8f; }
    float Amt() const { return g_cfg.slider[1] / 100.0f; }
    DWORD Style() const { return g_cfg.choice[0]; }
    float MinDim() const { return (float)(width < height ? width : height); }

    std::vector<Vertex> verts;
    float camZ = 0, ph[5] = {};
    void Reseed() override { for (auto& p : ph) p = RandF(0, 6.28f); }
    float H(float x, float z) const {
        float k = 0.5f + Amt() * 2;
        return k * (4 * sinf(x * 0.05f + ph[0]) * sinf(z * 0.04f + ph[1]) + 2.5f * sinf(x * 0.13f + z * 0.07f + ph[2]) + 1.2f * sinf(z * 0.21f + ph[3]) * cosf(x * 0.17f + ph[4])) + 2;
    }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, low, mid, high;
        switch (Style()) {
        case 1:  top = Color(0.4f, 0.55f, 0.75f); hor = Color(1, 0.85f, 0.7f); low = Color(0.55f, 0.35f, 0.15f); mid = Color(0.85f, 0.45f, 0.15f); high = Color(0.95f, 0.8f, 0.5f); break;
        case 2:  top = Color(0.45f, 0.6f, 0.8f); hor = Color(0.85f, 0.9f, 0.98f); low = Color(0.5f, 0.6f, 0.7f); mid = Color(0.75f, 0.82f, 0.9f); high = Color(1, 1, 1); break;
        case 3:  top = Color(0.1f, 0.05f, 0.25f); hor = Color(0.7f, 0.3f, 0.6f); low = Color(0.2f, 0.1f, 0.45f); mid = Color(0.1f, 0.6f, 0.6f); high = Color(0.9f, 0.9f, 0.4f); break;
        default: top = Color(0.35f, 0.6f, 0.95f); hor = Color(0.85f, 0.95f, 1); low = Color(0.3f, 0.6f, 0.3f); mid = Color(0.55f, 0.75f, 0.35f); high = Color(0.95f, 0.95f, 0.9f); break;
        }
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.6f), hor };
          aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }

        camZ += dt * Spd() * 6;
        Vec3 eye(sinf(t * 0.03f) * 15, 0, -camZ);
        eye.y = fmaxf(H(eye.x, eye.z), H(eye.x, eye.z - 10)) + 7;
        Mat4 view = Mat4::LookAt(eye, eye + Vec3(sinf(t * 0.02f) * 0.5f, -0.22f, -1), Vec3(0, 1, 0));
        Mat4 proj = Mat4::Perspective(FitFov(60, width, height), (float)width / height, 0.5f, 400);
        verts.clear();
        int n = preview ? 30 : 64;
        float step = 3;
        float x0 = floorf((eye.x - n * step / 2) / step) * step, z0 = floorf((eye.z - n * step + 6) / step) * step;
        float k = 0.5f + Amt() * 2;
        BuildTerrain(verts, n, n, x0, z0, step, [&](float x, float z) { return H(x, z); }, [&](float, float, float y) {
            float u = (y + 2 * k) / (11 * k);
            return u < 0.5f ? Lerp(low, mid, Smooth(u * 2)) : Lerp(mid, high, Smooth(u * 2 - 1));
        }, true);
        r.SetCamera(view, proj);
        r.SetLight(0, ToView(view, Normalize(Vec3(0.5f + 0.3f * sinf(t * 0.01f), 0.8f, 0.3f))), 1, 0.97f, 0.9f);
        r.SetLight(1, ToView(view, Normalize(Vec3(-0.6f, 0.3f, -0.4f))), top.r * 0.35f, top.g * 0.35f, top.b * 0.35f);
        r.SetAmbient(0.25f, 0.25f, 0.28f);
        DrawParams p;
        p.fogColor[0] = hor.r; p.fogColor[1] = hor.g; p.fogColor[2] = hor.b; p.fogDensity = 0.012f;
        r.Draw(verts.data(), verts.size(), p);
    }

};

Scene* CreateScene() { return new SaverScene; }
