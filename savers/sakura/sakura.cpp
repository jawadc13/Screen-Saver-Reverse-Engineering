// sakura.cpp - "Sakura Petals": cherry blossom petals drifting over soft spring hills
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Sakura Petals Settings",
    2, { { L"&Season:", L"Season", 0, L"Spring pink|Morning white|Evening gold|Night pink" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Petals", L"Petals", 50, L"Few", L"Blizzard" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullSakura"; }
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

    struct Petal { float x, y, z, vx, vy, rot, spin, flip, ph; };
    std::vector<Petal> petals;
    float hill[3][4] = {};
    Texture *petalTex = nullptr, *dot = nullptr;
    ~SaverScene() { delete petalTex; delete dot; }
    void Setup(Renderer& r) override {
        petalTex = aero::MakeTexture(r, 128, [](float x, float y) {
            float ex = x / 0.55f, ey = y / 0.95f;
            float d = sqrtf(ex * ex + ey * ey);
            float notch = expf(-((x * x) * 60 + (y + 0.95f) * (y + 0.95f) * 20));   // the little notch at the tip
            float a = (1 - aero::Smooth(0.85f, 1.0f, d)) * (1 - notch);
            float shade = 0.85f + 0.15f * (1 - d);
            return aero::Pack(shade, shade, shade, a);
        });
        dot = aero::SoftDot(r);
    }
    void Reseed() override {
        petals.resize(40 + (int)(Amt() * 260));
        for (auto& p : petals) p = { RandF(-0.6f, 0.6f), RandF(-0.6f, 0.6f), RandF(0.3f, 1), RandF(0.02f, 0.06f), RandF(0.03f, 0.08f), RandF(0, 6.28f), RandF(-2, 2), RandF(0, 6.28f), RandF(0, 6.28f) };
        for (auto& h : hill) for (auto& v : h) v = RandF(0, 6.28f);
    }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, petal, hillC;
        switch (Style()) {
        case 1:  top = Color(0.75f, 0.85f, 0.95f); hor = Color(0.98f, 0.97f, 0.95f); petal = Color(1, 0.92f, 0.95f); hillC = Color(0.6f, 0.75f, 0.6f); break;
        case 2:  top = Color(0.55f, 0.5f, 0.7f); hor = Color(1, 0.78f, 0.55f); petal = Color(1, 0.8f, 0.75f); hillC = Color(0.5f, 0.45f, 0.4f); break;
        case 3:  top = Color(0.03f, 0.03f, 0.1f); hor = Color(0.2f, 0.15f, 0.3f); petal = Color(1, 0.6f, 0.8f); hillC = Color(0.06f, 0.06f, 0.1f); break;
        default: top = Color(0.6f, 0.75f, 0.95f); hor = Color(1, 0.88f, 0.92f); petal = Color(1, 0.72f, 0.82f); hillC = Color(0.55f, 0.75f, 0.5f); break;
        }
        float W = (float)width, H = (float)height, m = MinDim();
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.6f), hor }; aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }
        Canvas2D hl; hl.Begin(width, height);
        for (int L = 0; L < 3; L++) {
            Color c = Lerp(hor, hillC, 0.35f + 0.3f * L);
            for (int i = 0; i < 80; i++) {
                auto y = [&](float u) { float v = 0; for (int k = 0; k < 4; k++) v += sinf((u + t * 0.004f * (L + 1)) * (k + 1) * 4.1f + hill[L][k]) / (k + 1); return -H * (0.25f + 0.1f * L) + H * 0.05f * v; };
                float u0 = (float)i / 80, u1 = (float)(i + 1) / 80, x0 = -W / 2 + W * u0, x1 = -W / 2 + W * u1;
                PushQuad(hl.v, hl.P(x0, -H / 2), hl.P(x1, -H / 2), hl.P(x1, y(u1)), hl.P(x0, y(u0)), Vec3(0, 0, 1), c * 0.85f, c * 0.85f, c, c);
            }
        }
        hl.Draw(r, BLEND_OPAQUE);
        Canvas2D pc; pc.Begin(width, height);
        for (auto& p : petals) {
            p.x += (p.vx + 0.02f * sinf(t + p.ph)) * dt * Spd() * p.z;
            p.y -= p.vy * dt * Spd() * p.z;
            p.rot += p.spin * dt; p.flip += dt * 2;
            if (p.y < -0.65f) { p.y = 0.65f; p.x = RandF(-0.7f, 0.5f); }
            if (p.x > 0.7f) p.x -= 1.4f;
            float s = m * 0.034f * p.z;
            aero::Sprite(pc, p.x * W, p.y * H, s * fabsf(cosf(p.flip)) + s * 0.2f, s * 1.5f, p.rot, Lerp(hor, petal, 0.5f + 0.5f * p.z).WithAlpha(0.9f));
        }
        pc.Draw(r, BLEND_ALPHA, petalTex);
    }

};

Scene* CreateScene() { return new SaverScene; }
