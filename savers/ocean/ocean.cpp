// ocean.cpp - "Ocean Waves": gliding low over rolling, sunlit ocean swells
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Ocean Waves Settings",
    2, { { L"&Time of day:", L"TimeOfDay", 1, L"Morning|Midday|Sunset|Moonlit night" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Wave height", L"Waves", 45, L"Calm", L"Rough" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullOcean"; }
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

    struct Wave { float kx, kz, amp, speed, ph; } waves[6];
    std::vector<Vertex> verts;
    Texture* dot = nullptr;
    float camZ = 0, sunAz = 0;
    ~SaverScene() { delete dot; }
    void Setup(Renderer& r) override { dot = aero::SoftDot(r); }
    void Reseed() override {
        for (auto& w : waves) { float a = RandF(0, 6.28f), k = RandF(0.12f, 0.55f); w = { cosf(a) * k, sinf(a) * k, RandF(0.15f, 0.45f) / (k * 4), RandF(0.8f, 1.4f), RandF(0, 6.28f) }; }
        sunAz = RandF(-0.5f, 0.5f);
    }
    float H(float x, float z) const {
        float y = 0;
        for (auto& w : waves) y += w.amp * sinf(w.kx * x + w.kz * z + w.ph + t * w.speed * 1.2f);
        return y * (0.4f + Amt() * 1.4f);
    }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, sun, water; float sunH;
        switch (Style()) {
        case 0:  top = Color(0.35f, 0.55f, 0.85f); hor = Color(1.0f, 0.82f, 0.68f); sun = Color(1, 0.9f, 0.7f); water = Color(0.1f, 0.3f, 0.45f); sunH = 0.1f; break;
        case 2:  top = Color(0.2f, 0.15f, 0.42f); hor = Color(1.0f, 0.55f, 0.3f); sun = Color(1, 0.6f, 0.3f); water = Color(0.15f, 0.1f, 0.22f); sunH = 0.04f; break;
        case 3:  top = Color(0.01f, 0.02f, 0.07f); hor = Color(0.07f, 0.1f, 0.22f); sun = Color(0.75f, 0.8f, 1.0f); water = Color(0.02f, 0.04f, 0.09f); sunH = 0.2f; break;
        default: top = Color(0.12f, 0.42f, 0.88f); hor = Color(0.75f, 0.9f, 1.0f); sun = Color(1, 1, 0.95f); water = Color(0.04f, 0.28f, 0.48f); sunH = 0.35f; break;
        }
        camZ += dt * Spd() * 3;
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.6f), hor };
          aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }

        float aspect = (float)width / height, yaw = sinf(t * 0.02f) * 0.3f;
        Vec3 eye(sinf(t * 0.013f) * 6, 2.4f + 0.25f * sinf(t * 0.3f), -camZ);
        Mat4 view = Mat4::LookAt(eye, eye + Vec3(sinf(yaw), -0.1f, -cosf(yaw)), Vec3(0, 1, 0));
        Mat4 proj = Mat4::Perspective(FitFov(50, width, height), aspect, 0.3f, 500);
        Vec3 sunDir = Normalize(Vec3(sinf(sunAz + t * 0.004f), sunH, -cosf(sunAz + t * 0.004f)));
        // Sun (or moon) glow, slowly crossing the sky.
        float sx, sy, sd;
        if (ToScreen(proj * view, eye + sunDir * 300, width, height, sx, sy, sd)) {
            Canvas2D g; g.Begin(width, height);
            g.Dot(sx, sy, MinDim() * 0.45f, sun * 0.45f);
            g.Dot(sx, sy, MinDim() * 0.06f, Color(1, 1, 1));
            g.Draw(r, BLEND_ADD, dot);
        }
        verts.clear();
        int n = preview ? 40 : 96;
        float step = 1.4f;
        float x0 = floorf((eye.x - n * step / 2) / step) * step, z0 = floorf((eye.z - n * step + 6) / step) * step;
        BuildTerrain(verts, n, n, x0, z0, step, [&](float x, float z) { return H(x, z); },
                     [&](float, float, float y) { return Lerp(water, hor, fmaxf(0, 0.12f + y * 0.25f)); });
        r.SetCamera(view, proj);
        r.SetLight(0, ToView(view, sunDir), sun.r, sun.g, sun.b);
        r.SetLight(1, ToView(view, Vec3(0, 1, 0)), top.r * 0.4f, top.g * 0.4f, top.b * 0.4f);
        r.SetAmbient(hor.r * 0.25f, hor.g * 0.25f, hor.b * 0.25f);
        DrawParams p;
        p.specular[0] = sun.r; p.specular[1] = sun.g; p.specular[2] = sun.b;
        p.shininess = 90;
        p.fogColor[0] = hor.r; p.fogColor[1] = hor.g; p.fogColor[2] = hor.b; p.fogDensity = 0.025f;
        r.Draw(verts.data(), verts.size(), p);
    }

};

Scene* CreateScene() { return new SaverScene; }
