// lavalamp.cpp - "Lava Lamp": glowing blobs of wax rising, merging and sinking
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Lava Lamp Settings",
    2, { { L"&Colours:", L"Colours", 0, L"Orange on red|Pink on purple|Green on teal|Blue on navy" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Blobs", L"Blobs", 50, L"Few", L"Many" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullLava"; }
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

    struct Blob { float x, y, r, vy, ph, wob; };
    std::vector<Blob> blobs;
    Texture *dot = nullptr, *atlas = nullptr;
    ~SaverScene() { delete dot; delete atlas; }
    void Setup(Renderer& r) override { dot = aero::SoftDot(r); atlas = aero::OrbAtlas(r); }
    void Reseed() override {
        blobs.resize(5 + (int)(Amt() * 12));
        for (auto& b : blobs) b = { RandF(-0.4f, 0.4f), RandF(-0.5f, 0.5f), RandF(0.06f, 0.16f), RandF(0.02f, 0.06f) * (RandI(0, 1) ? 1 : -1), RandF(0, 6.28f), RandF(0.3f, 0.8f) };
    }
    void Draw(Renderer& r, float dt) override {
        Color bgTop, bgBot, wax, hot;
        switch (Style()) {
        case 1:  bgTop = Color(0.12f, 0.02f, 0.2f); bgBot = Color(0.45f, 0.08f, 0.4f); wax = Color(1, 0.35f, 0.7f); hot = Color(1, 0.75f, 0.9f); break;
        case 2:  bgTop = Color(0.0f, 0.12f, 0.12f); bgBot = Color(0.05f, 0.35f, 0.3f); wax = Color(0.45f, 1, 0.35f); hot = Color(0.85f, 1, 0.6f); break;
        case 3:  bgTop = Color(0.01f, 0.02f, 0.12f); bgBot = Color(0.05f, 0.12f, 0.4f); wax = Color(0.3f, 0.6f, 1); hot = Color(0.75f, 0.9f, 1); break;
        default: bgTop = Color(0.15f, 0.02f, 0.0f); bgBot = Color(0.55f, 0.08f, 0.02f); wax = Color(1, 0.5f, 0.1f); hot = Color(1, 0.85f, 0.4f); break;
        }
        { Canvas2D bg; bg.Begin(width, height); Color st[3] = { bgTop, Lerp(bgTop, bgBot, 0.5f), bgBot }; aero::VerticalGradient(bg, width, height, st, 3); bg.Draw(r, BLEND_OPAQUE); }
        float W = (float)width, Hh = (float)height, m = MinDim();
        Canvas2D glow, body; glow.Begin(width, height); body.Begin(width, height);
        for (auto& b : blobs) {
            // Heated at the bottom, rise; cool at the top, sink.
            b.vy += (b.y < -0.4f ? 0.03f : b.y > 0.4f ? -0.03f : 0) * dt;
            b.vy = fmaxf(-0.07f, fminf(0.07f, b.vy));
            b.y += b.vy * dt * Spd();
            float x = (b.x + 0.05f * sinf(t * b.wob + b.ph)) * W, y = b.y * Hh;
            float rr = b.r * m * (1 + 0.12f * sinf(t * b.wob * 1.7f + b.ph));
            glow.Dot(x, y, rr * 3.2f, wax * 0.35f);
            aero::OrbCell(body, aero::ORB_BODY, x, y, rr, Lerp(wax, hot, 0.2f + 0.2f * sinf(t + b.ph)), 1 + 0.15f * sinf(t * 0.9f + b.ph));
            aero::OrbCell(body, aero::ORB_GLOSS, x, y, rr, Color(1, 1, 1, 0.35f));
        }
        glow.Draw(r, BLEND_ADD, dot);
        body.Draw(r, BLEND_ADD, atlas);
    }

};

Scene* CreateScene() { return new SaverScene; }
