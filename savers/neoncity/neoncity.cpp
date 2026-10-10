// neoncity.cpp - "Neon City Flyover": flying low over an endless night city of glowing windows
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Neon City Flyover Settings",
    2, { { L"&Mood:", L"Mood", 0, L"Synthwave|Cyber blue|Golden night|Matrix green" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Building height", L"Height", 50, L"Low", L"Towers" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullNeonCity"; }
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

    std::vector<Vertex> solid, lights;
    float camZ = 0;
    unsigned salt = 1;
    void Reseed() override { salt = (unsigned)RandI(1, 1 << 30); }
    float Hash(int x, int z, int k) const {
        unsigned h = (unsigned)x * 73856093u ^ (unsigned)z * 19349663u ^ (unsigned)k * 83492791u ^ salt;
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return (h & 0xFFFFFF) / 16777216.0f;
    }
    void Box(float x0, float z0, float x1, float z1, float h, Color c) {
        Vec3 a(x0, 0, z0), b(x1, 0, z0), cc(x1, 0, z1), d(x0, 0, z1), up(0, h, 0);
        PushQuad(solid, a + up, b + up, cc + up, d + up, Vec3(0, 1, 0), c, c, c, c);
        PushQuad(solid, d, cc, cc + up, d + up, Vec3(0, 0, 1), c, c, c, c);
        PushQuad(solid, b, a, a + up, b + up, Vec3(0, 0, -1), c, c, c, c);
        PushQuad(solid, a, d, d + up, a + up, Vec3(-1, 0, 0), c, c, c, c);
        PushQuad(solid, cc, b, b + up, cc + up, Vec3(1, 0, 0), c, c, c, c);
    }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, win1, win2;
        switch (Style()) {
        case 1:  top = Color(0.0f, 0.02f, 0.08f); hor = Color(0.05f, 0.25f, 0.45f); win1 = Color(0.3f, 0.8f, 1); win2 = Color(0.8f, 0.9f, 1); break;
        case 2:  top = Color(0.03f, 0.02f, 0.06f); hor = Color(0.4f, 0.22f, 0.1f); win1 = Color(1, 0.75f, 0.35f); win2 = Color(1, 0.9f, 0.6f); break;
        case 3:  top = Color(0.0f, 0.03f, 0.01f); hor = Color(0.03f, 0.25f, 0.1f); win1 = Color(0.3f, 1, 0.45f); win2 = Color(0.7f, 1, 0.7f); break;
        default: top = Color(0.05f, 0.0f, 0.12f); hor = Color(0.6f, 0.15f, 0.45f); win1 = Color(1, 0.3f, 0.8f); win2 = Color(0.3f, 0.9f, 1); break;
        }
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.6f), hor };
          aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }

        camZ += dt * Spd() * 8;
        Vec3 eye(sinf(t * 0.07f) * 1.5f, 9 + 2 * sinf(t * 0.05f), -camZ);
        Mat4 view = Mat4::LookAt(eye, eye + Vec3(sinf(t * 0.03f) * 0.3f, -0.35f, -1), Vec3(0, 1, 0));
        Mat4 proj = Mat4::Perspective(FitFov(60, width, height), (float)width / height, 0.5f, 300);
        solid.clear(); lights.clear();
        const float cell = 6;
        int cz0 = (int)floorf(-camZ / cell) - 34, cz1 = (int)floorf(-camZ / cell) + 2;
        for (int cz = cz0; cz <= cz1; cz++)
            for (int cx = -9; cx <= 9; cx++) {
                if (cx == 0) continue;   // the avenue we fly along
                float h = (2 + Hash(cx, cz, 1) * 14 * (0.4f + Amt() * 1.6f)) * (1 + 0.3f * (abs(cx) > 3));
                float x0 = cx * cell + 0.8f, z0 = cz * cell + 0.8f, x1 = x0 + cell - 1.6f, z1 = z0 + cell - 1.6f;
                Box(x0, z0, x1, z1, h, Color(0.08f, 0.08f, 0.12f));
                // Window lights on the face towards the avenue and the front face.
                for (int fl = 1; fl < (int)h; fl += 1)
                    for (int k = 0; k < 3; k++) {
                        if (Hash(cx * 7 + k, cz * 13 + fl, 2) < 0.55f) continue;
                        Color w = Lerp(win1, win2, Hash(cx, cz + fl, 3)) * (0.5f + 0.5f * sinf(t * 0.2f + Hash(cx, fl, 4) * 30));
                        float wx = x0 + 0.5f + k * (cell - 2.6f) / 3, y = fl + 0.15f;
                        Vec3 n(0, 0, 1);
                        PushQuad(lights, Vec3(wx, y, z1 + 0.02f), Vec3(wx + 0.6f, y, z1 + 0.02f), Vec3(wx + 0.6f, y + 0.55f, z1 + 0.02f), Vec3(wx, y + 0.55f, z1 + 0.02f), n, w, w, w, w);
                    }
            }
        r.SetCamera(view, proj);
        r.SetLight(0, ToView(view, Normalize(Vec3(0.3f, 1, 0.5f))), hor.r * 0.6f, hor.g * 0.6f, hor.b * 0.6f);
        r.SetLight(1, ToView(view, Vec3(0, 0, 1)), 0, 0, 0);
        r.SetAmbient(0.1f, 0.1f, 0.14f);
        DrawParams p;
        p.fogColor[0] = hor.r; p.fogColor[1] = hor.g; p.fogColor[2] = hor.b; p.fogDensity = 0.012f;
        r.Draw(solid.data(), solid.size(), p);
        p.lit = false;
        r.Draw(lights.data(), lights.size(), p);
    }

};

Scene* CreateScene() { return new SaverScene; }
