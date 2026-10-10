// wormhole.cpp - "Wormhole": falling through a swirling wormhole of light
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Wormhole Settings",
    2, { { L"&Colours:", L"Colours", 0, L"Cosmic|Fire|Ice|Rainbow" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Swirl", L"Swirl", 50, L"Gentle", L"Wild" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullWormhole"; }
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
    float z = 0, p1 = 0, p2 = 0;
    void Reseed() override { p1 = RandF(0, 6.28f); p2 = RandF(0, 6.28f); }
    Color Pal(float u) const {
        switch (Style()) {
        case 1:  return Hsv(0.0f + 0.12f * u, 0.9f, 1);
        case 2:  return Lerp(Color(0.2f, 0.5f, 1), Color(0.85f, 0.95f, 1), u);
        case 3:  return Hsv(u, 0.8f, 1);
        default: return Lerp(Color(0.5f, 0.2f, 1), Color(0.2f, 0.9f, 1), u);
        }
    }
    Vec3 Path(float s) const { return Vec3(4 * sinf(s * 0.031f + p1) + 2 * sinf(s * 0.07f), 3 * cosf(s * 0.027f + p2), -s); }
    void Draw(Renderer& r, float dt) override {
        z += dt * Spd() * 14;
        Vec3 eye = Path(z), at = Path(z + 3);
        float roll = t * 0.3f * (0.3f + Amt());
        Mat4 view = Mat4::LookAt(eye, at, Vec3(sinf(roll), cosf(roll), 0));
        Mat4 proj = Mat4::Perspective(FitFov(75, width, height), (float)width / height, 0.1f, 120);
        verts.clear();
        const int sides = 40;
        float swirl = 0.5f + Amt() * 2.5f;
        float s0 = floorf(z) - 1;
        std::vector<Vec3> ring0(sides + 1), ring1(sides + 1);
        auto ring = [&](float s, std::vector<Vec3>& out) {
            Vec3 c = Path(s), f = Normalize(Path(s + 0.1f) - c), side = Normalize(Cross(f, Vec3(0, 1, 0))), up = Cross(side, f);
            float rad = 3 + 0.6f * sinf(s * 0.2f + t);
            for (int i = 0; i <= sides; i++) { float a = 2 * kPi * i / sides; out[i] = c + (side * cosf(a) + up * sinf(a)) * rad; }
        };
        ring(s0, ring0);
        for (float s = s0; s < z + 100; s += 1) {
            ring(s + 1, ring1);
            float fog = 1 - (s - z) / 100; fog = fog < 0 ? 0 : fog;
            for (int i = 0; i < sides; i++) {
                // Spiral stripes that turn as we fall.
                float u = (float)i / sides + s * 0.02f * swirl - t * 0.1f * Spd();
                float stripe = 0.5f + 0.5f * sinf(u * 6.28f * 3);
                Color c = Pal(fmodf(u + 10, 1)) * ((0.15f + 0.85f * stripe * stripe) * fog);
                PushQuad(verts, ring0[i], ring0[i + 1], ring1[i + 1], ring1[i], Vec3(0, 0, 1), c, c, c, c);
            }
            ring0.swap(ring1);
        }
        r.SetCamera(view, proj);
        DrawParams p; p.lit = false;
        r.Draw(verts.data(), verts.size(), p);
    }

};

Scene* CreateScene() { return new SaverScene; }
