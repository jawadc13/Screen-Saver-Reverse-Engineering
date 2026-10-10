// hexpulse.cpp - "Hex Pulse": a field of hexagonal pillars rising and falling in waves of light
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Hex Pulse Settings",
    2, { { L"&Palette:", L"Palette", 0, L"Electric|Candy|Lava|Glacier" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Wave height", L"Height", 50, L"Low", L"High" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullHexPulse"; }
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
    Vec3 src[3];
    void Reseed() override { for (auto& s : src) s = Vec3(RandF(-8, 8), 0, RandF(-8, 8)); }
    Color Pal(float u) const {
        switch (Style()) {
        case 1:  return Hsv(0.85f + u * 0.35f, 0.55f, 1);
        case 2:  return Hsv(0.0f + u * 0.12f, 0.9f, 0.4f + 0.6f * u);
        case 3:  return Lerp(Color(0.2f, 0.35f, 0.6f), Color(0.85f, 0.95f, 1), u);
        default: return Lerp(Color(0.1f, 0.15f, 0.6f), Color(0.2f, 1, 0.9f), u);
        }
    }
    void Draw(Renderer& r, float dt) override {
        { Canvas2D bg; bg.Begin(width, height); Color st[2] = { Pal(0) * 0.15f, Pal(0.3f) * 0.35f }; aero::VerticalGradient(bg, width, height, st, 2); bg.Draw(r, BLEND_OPAQUE); }
        float tt = t * Spd();
        Vec3 c = Vec3(sinf(t * 0.03f) * 3, 0, cosf(t * 0.025f) * 3);
        Mat4 view = Mat4::LookAt(c + Vec3(sinf(t * 0.04f) * 16, 13 + 2 * sinf(t * 0.05f), cosf(t * 0.04f) * 16), c, Vec3(0, 1, 0));
        Mat4 proj = Mat4::Perspective(FitFov(55, width, height), (float)width / height, 0.5f, 200);
        verts.clear();
        const float R = 0.6f, dx = R * 1.7320508f, dz = R * 1.5f;
        int n = preview ? 10 : 18;
        for (int j = -n; j <= n; j++)
            for (int i = -n; i <= n; i++) {
                float x = i * dx + (j & 1) * dx * 0.5f, z = j * dz;
                float h = 0;
                for (auto& s : src) { float d = sqrtf((x - s.x) * (x - s.x) + (z - s.z) * (z - s.z)); h += sinf(d * 0.7f - tt * 1.8f) / (1 + d * 0.1f); }
                h = 0.4f + (h + 1.5f) * (0.3f + Amt() * 1.2f);
                Color top = Pal(fminf(1, fmaxf(0, h / (3 * (0.3f + Amt() * 1.2f) + 0.4f)))), side = top * 0.55f;
                Vec3 cen(x, h, z);
                for (int k = 0; k < 6; k++) {
                    float a0 = kPi / 6 + k * kPi / 3, a1 = a0 + kPi / 3;
                    Vec3 p0(x + cosf(a0) * R * 0.94f, h, z + sinf(a0) * R * 0.94f), p1(x + cosf(a1) * R * 0.94f, h, z + sinf(a1) * R * 0.94f);
                    PushVertex(verts, cen, Vec3(0, 1, 0), 0, 0, top); PushVertex(verts, p1, Vec3(0, 1, 0), 0, 0, top); PushVertex(verts, p0, Vec3(0, 1, 0), 0, 0, top);
                    Vec3 nrm = Normalize(Vec3(cosf((a0 + a1) / 2), 0, sinf((a0 + a1) / 2)));
                    PushQuad(verts, Vec3(p0.x, 0, p0.z), Vec3(p1.x, 0, p1.z), p1, p0, nrm, side * 0.5f, side * 0.5f, side, side);
                }
            }
        r.SetCamera(view, proj);
        r.SetLight(0, ToView(view, Normalize(Vec3(0.4f, 1, 0.3f))), 1, 1, 1);
        r.SetLight(1, ToView(view, Normalize(Vec3(-0.5f, 0.3f, -0.6f))), 0.3f, 0.3f, 0.4f);
        r.SetAmbient(0.25f, 0.25f, 0.3f);
        DrawParams p;
        p.specular[0] = p.specular[1] = p.specular[2] = 0.5f; p.shininess = 40;
        p.fogColor[0] = Pal(0.3f).r * 0.35f; p.fogColor[1] = Pal(0.3f).g * 0.35f; p.fogColor[2] = Pal(0.3f).b * 0.35f; p.fogDensity = 0.03f;
        r.Draw(verts.data(), verts.size(), p);
    }

};

Scene* CreateScene() { return new SaverScene; }
