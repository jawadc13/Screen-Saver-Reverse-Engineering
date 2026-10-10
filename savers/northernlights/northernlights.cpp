// northernlights.cpp - "Northern Lights": aurora curtains rippling over snowy mountains under the stars
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Northern Lights Settings",
    2, { { L"&Aurora:", L"Aurora", 0, L"Green|Teal and violet|Pink|Rainbow" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Activity", L"Activity", 50, L"Calm", L"Storm" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullAurora"; }
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

    struct Star { float x, y, b, ph; };
    struct Curtain { float base, amp, k, w, ph, h; };
    std::vector<Star> stars;
    std::vector<Curtain> curtains;
    float ridge[3][8];
    Texture* dot = nullptr;
    ~SaverScene() { delete dot; }
    void Setup(Renderer& r) override { dot = aero::SoftDot(r); }
    void Reseed() override {
        stars.resize(300);
        for (auto& s : stars) s = { RandF(-0.5f, 0.5f), RandF(-0.1f, 0.5f), RandF(0.2f, 1), RandF(0, 6.28f) };
        curtains.resize(3 + (int)(Amt() * 4));
        for (auto& c : curtains) c = { RandF(0.0f, 0.25f), RandF(0.03f, 0.08f), RandF(1.5f, 4), RandF(0.1f, 0.35f), RandF(0, 6.28f), RandF(0.15f, 0.35f) };
        for (auto& rg : ridge) for (auto& v : rg) v = RandF(0, 6.28f);
    }
    Color AuroraCol(float u) const {
        switch (Style()) {
        case 1:  return Lerp(Color(0.1f, 0.9f, 0.8f), Color(0.6f, 0.3f, 1.0f), u);
        case 2:  return Lerp(Color(1.0f, 0.3f, 0.6f), Color(0.6f, 0.4f, 1.0f), u);
        case 3:  return Hsv(u * 0.8f + t * 0.01f, 0.75f, 1);
        default: return Lerp(Color(0.2f, 1.0f, 0.4f), Color(0.3f, 0.8f, 0.7f), u);
        }
    }
    void Draw(Renderer& r, float dt) override {
        float W = (float)width, H = (float)height, m = MinDim(), drift = t * 0.004f * Spd();
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { Color(0.0f, 0.01f, 0.05f), Color(0.02f, 0.04f, 0.12f), Color(0.05f, 0.08f, 0.18f) };
          aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }
        Canvas2D st; st.Begin(width, height);
        for (auto& s : stars) {
            float x = fmodf(s.x + drift + 10.5f, 1.0f) - 0.5f;   // the sky turns slowly
            st.Dot(x * W, s.y * H, m * 0.004f, Color(1, 1, 1) * (s.b * (0.6f + 0.4f * sinf(t * 2 + s.ph))));
        }
        st.Draw(r, BLEND_ADD, dot);
        // Curtains: columns of light hanging from a wavy line, fading upward.
        Canvas2D cur; cur.Begin(width, height);
        int seg = 120;
        for (size_t ci = 0; ci < curtains.size(); ci++) {
            auto& c = curtains[ci];
            for (int i = 0; i < seg; i++) {
                float u0 = (float)i / seg, u1 = (float)(i + 1) / seg;
                auto base = [&](float u) { return H * (c.base + c.amp * sinf(u * c.k * 6.28f + t * c.w * Spd() + c.ph) + 0.02f * sinf(u * 23 + t)); };
                auto bright = [&](float u) { float b = 0.5f + 0.5f * sinf(u * 40 + t * 1.3f * Spd() + c.ph * 3); return (0.25f + 0.75f * b * b) * Smooth(u * 6) * Smooth((1 - u) * 6); };
                float x0 = -W / 2 + W * u0, x1 = -W / 2 + W * u1, b0 = base(u0), b1 = base(u1), h = H * c.h;
                Color col0 = AuroraCol((float)ci / curtains.size() + u0 * 0.2f), col1 = AuroraCol((float)ci / curtains.size() + u1 * 0.2f);
                Color a0 = col0.WithAlpha(0.55f * bright(u0)), a1 = col1.WithAlpha(0.55f * bright(u1));
                PushQuad(cur.v, cur.P(x0, b0), cur.P(x1, b1), cur.P(x1, b1 + h), cur.P(x0, b0 + h), Vec3(0, 0, 1), a0, a1, a1.WithAlpha(0), a0.WithAlpha(0));
                PushQuad(cur.v, cur.P(x0, b0 - h * 0.08f), cur.P(x1, b1 - h * 0.08f), cur.P(x1, b1), cur.P(x0, b0), Vec3(0, 0, 1), a0.WithAlpha(0), a1.WithAlpha(0), a1, a0);
            }
        }
        cur.Draw(r, BLEND_ADD);
        // Snowy mountains in three layers drifting sideways (parallax).
        Canvas2D mt; mt.Begin(width, height);
        for (int L = 0; L < 3; L++) {
            float off = t * 0.006f * (L + 1) * Spd(), baseY = -H * (0.12f + 0.12f * L);
            Color top = Lerp(Color(0.35f, 0.42f, 0.55f), Color(0.04f, 0.05f, 0.08f), L / 2.0f), bot = top * 0.4f;
            for (int i = 0; i < 80; i++) {
                auto y = [&](float u) { float v = 0; for (int k = 0; k < 4; k++) v += sinf((u + off) * (k + 1) * 5.3f + ridge[L][k]) / (k + 1); return baseY + H * 0.06f * (v + 1.5f); };
                float u0 = (float)i / 80, u1 = (float)(i + 1) / 80, x0 = -W / 2 + W * u0, x1 = -W / 2 + W * u1;
                PushQuad(mt.v, mt.P(x0, -H / 2), mt.P(x1, -H / 2), mt.P(x1, y(u1)), mt.P(x0, y(u0)), Vec3(0, 0, 1), bot, bot, top, top);
            }
        }
        mt.Draw(r, BLEND_OPAQUE);
    }

};

Scene* CreateScene() { return new SaverScene; }
