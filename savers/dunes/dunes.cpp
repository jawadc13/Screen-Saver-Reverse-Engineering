// dunes.cpp - "Desert Dunes": gliding over golden sand dunes in low sunlight
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Desert Dunes Settings",
    2, { { L"&Light:", L"Light", 0, L"Golden hour|Midday|Blue hour|Red planet" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Dune height", L"Height", 50, L"Gentle", L"Tall" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullDunes"; }
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
    float camZ = 0, ph[4] = {};
    Texture* dot = nullptr;
    ~SaverScene() { delete dot; }
    void Setup(Renderer& r) override { dot = aero::SoftDot(r); }
    void Reseed() override { for (auto& p : ph) p = RandF(0, 6.28f); }
    float H(float x, float z) const {
        float k = 0.6f + Amt() * 1.8f;
        float ridge = 1 - fabsf(sinf(x * 0.08f + z * 0.05f + ph[0] + sinf(z * 0.03f + ph[1]) * 1.5f));
        return k * (2.5f * ridge * ridge + 0.8f * sinf(x * 0.21f + ph[2]) * sinf(z * 0.17f + ph[3]) + 0.15f * sinf(x * 1.3f + z * 0.9f));
    }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, sand, sun;
        switch (Style()) {
        case 1:  top = Color(0.2f, 0.45f, 0.85f); hor = Color(0.85f, 0.85f, 0.8f); sand = Color(0.95f, 0.8f, 0.55f); sun = Color(1, 1, 0.95f); break;
        case 2:  top = Color(0.05f, 0.08f, 0.25f); hor = Color(0.4f, 0.4f, 0.6f); sand = Color(0.55f, 0.5f, 0.6f); sun = Color(0.6f, 0.7f, 1); break;
        case 3:  top = Color(0.3f, 0.2f, 0.18f); hor = Color(0.85f, 0.6f, 0.45f); sand = Color(0.8f, 0.4f, 0.22f); sun = Color(0.9f, 0.85f, 0.95f); break;
        default: top = Color(0.3f, 0.35f, 0.7f); hor = Color(1, 0.75f, 0.5f); sand = Color(1, 0.72f, 0.4f); sun = Color(1, 0.75f, 0.45f); break;
        }
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.6f), hor };
          aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }

        camZ += dt * Spd() * 4;
        Vec3 eye(sinf(t * 0.02f) * 20, 0, -camZ);
        eye.y = fmaxf(H(eye.x, eye.z), H(eye.x, eye.z - 4)) + 3.5f;
        Mat4 view = Mat4::LookAt(eye, eye + Vec3(sinf(t * 0.015f) * 0.4f, -0.12f, -1), Vec3(0, 1, 0));
        Mat4 proj = Mat4::Perspective(FitFov(55, width, height), (float)width / height, 0.3f, 400);
        Vec3 sunDir = Normalize(Vec3(cosf(t * 0.003f + 1), Style() == 1 ? 0.8f : 0.18f, -0.6f));
        float sx, sy, sd;
        if (ToScreen(proj * view, eye + sunDir * 300, width, height, sx, sy, sd)) {
            Canvas2D g; g.Begin(width, height); g.Dot(sx, sy, MinDim() * 0.5f, sun * 0.4f); g.Dot(sx, sy, MinDim() * 0.05f, Color(1, 1, 1)); g.Draw(r, BLEND_ADD, dot);
        }
        verts.clear();
        int n = preview ? 40 : 100;
        float step = 1.6f;
        float x0 = floorf((eye.x - n * step / 2) / step) * step, z0 = floorf((eye.z - n * step + 4) / step) * step;
        BuildTerrain(verts, n, n, x0, z0, step, [&](float x, float z) { return H(x, z); }, [&](float, float, float) { return sand; });
        r.SetCamera(view, proj);
        r.SetLight(0, ToView(view, sunDir), sun.r, sun.g, sun.b);
        r.SetLight(1, ToView(view, Vec3(0, 1, 0)), top.r * 0.3f, top.g * 0.3f, top.b * 0.3f);
        r.SetAmbient(top.r * 0.25f, top.g * 0.25f, top.b * 0.25f);
        DrawParams p;
        p.fogColor[0] = hor.r; p.fogColor[1] = hor.g; p.fogColor[2] = hor.b; p.fogDensity = 0.02f;
        r.Draw(verts.data(), verts.size(), p);
    }

};

Scene* CreateScene() { return new SaverScene; }
