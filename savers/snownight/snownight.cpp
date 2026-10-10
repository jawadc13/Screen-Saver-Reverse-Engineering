// snownight.cpp - "Snowy Night": snow falling over moonlit pine-covered hills
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Snowy Night Settings",
    2, { { L"&Sky:", L"Sky", 0, L"Moonlit blue|Starry violet|Dawn|Northern glow" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Snowfall", L"Snow", 50, L"Light", L"Blizzard" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullSnowNight"; }
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

    struct Flake { float x, y, z, ph; };
    struct Tree { float x, h; int layer; };
    std::vector<Flake> flakes;
    std::vector<Tree> trees;
    float hillPh[3][3] = {};
    Texture* dot = nullptr;
    ~SaverScene() { delete dot; }
    void Setup(Renderer& r) override { dot = aero::SoftDot(r); }
    void Reseed() override {
        flakes.resize(150 + (int)(Amt() * 900));
        for (auto& f : flakes) f = { RandF(-0.6f, 0.6f), RandF(-0.6f, 0.6f), RandF(0.2f, 1), RandF(0, 6.28f) };
        trees.clear();
        for (int L = 0; L < 3; L++) for (int i = 0; i < 26; i++) trees.push_back({ RandF(-1, 1), RandF(0.05f, 0.12f) * (0.6f + 0.25f * L), L });
        for (auto& h : hillPh) for (auto& v : h) v = RandF(0, 6.28f);
    }
    void Draw(Renderer& r, float dt) override {
        Color top, hor, moon;
        switch (Style()) {
        case 1:  top = Color(0.04f, 0.02f, 0.12f); hor = Color(0.25f, 0.18f, 0.4f); moon = Color(0.95f, 0.9f, 1); break;
        case 2:  top = Color(0.2f, 0.25f, 0.45f); hor = Color(0.95f, 0.7f, 0.6f); moon = Color(1, 0.9f, 0.8f); break;
        case 3:  top = Color(0.01f, 0.05f, 0.06f); hor = Color(0.1f, 0.4f, 0.3f); moon = Color(0.85f, 1, 0.9f); break;
        default: top = Color(0.02f, 0.04f, 0.12f); hor = Color(0.18f, 0.28f, 0.45f); moon = Color(0.95f, 0.97f, 1); break;
        }
        float W = (float)width, H = (float)height, m = MinDim();
        { Canvas2D sky; sky.Begin(width, height); Color st[3] = { top, Lerp(top, hor, 0.5f), hor }; aero::VerticalGradient(sky, width, height, st, 3); sky.Draw(r, BLEND_OPAQUE); }
        // The moon arcs slowly across the sky.
        { Canvas2D mn; mn.Begin(width, height); float ma = t * 0.004f; float mx = W * 0.4f * sinf(ma), my = H * (0.2f + 0.15f * cosf(ma));
          mn.Dot(mx, my, m * 0.35f, moon * 0.25f); mn.Dot(mx, my, m * 0.05f, moon); mn.Draw(r, BLEND_ADD, dot); }
        Canvas2D land; land.Begin(width, height);
        for (int L = 0; L < 3; L++) {
            float off = t * 0.005f * (L + 1) * Spd();
            auto y = [&](float x) { float u = x / W; float v = 0; for (int k = 0; k < 3; k++) v += sinf((u + off) * (k + 1) * 4.3f + hillPh[L][k]) / (k + 1); return -H * (0.12f + 0.13f * L) + H * 0.05f * v; };
            Color snow = Lerp(hor, Color(0.85f, 0.9f, 1), 0.3f + 0.25f * L) * (0.55f + 0.2f * L), pine = Lerp(top, Color(0.02f, 0.05f, 0.04f), 0.5f + 0.2f * L);
            for (auto& tr : trees) {
                if (tr.layer != L) continue;
                float x = (fmodf(tr.x - off * 2 + 11, 2.0f) - 1) * W * 0.6f, base = y(x), h = tr.h * H;
                for (int k = 0; k < 3; k++) {
                    float yb = base + h * 0.25f * k, w = h * (0.35f - 0.08f * k);
                    PushVertex(land.v, land.P(x - w, yb), Vec3(0, 0, 1), 0, 0, pine);
                    PushVertex(land.v, land.P(x + w, yb), Vec3(0, 0, 1), 0, 0, pine);
                    PushVertex(land.v, land.P(x, yb + h * 0.45f), Vec3(0, 0, 1), 0, 0, Lerp(pine, snow, 0.4f));
                }
            }
            for (int i = 0; i < 80; i++) {
                float x0 = -W / 2 + W * i / 80, x1 = -W / 2 + W * (i + 1) / 80;
                PushQuad(land.v, land.P(x0, -H / 2), land.P(x1, -H / 2), land.P(x1, y(x1)), land.P(x0, y(x0)), Vec3(0, 0, 1), snow * 0.7f, snow * 0.7f, snow, snow);
            }
        }
        land.Draw(r, BLEND_OPAQUE);
        Canvas2D fl; fl.Begin(width, height);
        for (auto& f : flakes) {
            f.y -= dt * 0.06f * (0.3f + f.z) * Spd();
            f.x += dt * 0.02f * sinf(t * 0.8f + f.ph) * f.z;
            if (f.y < -0.62f) { f.y = 0.62f; f.x = RandF(-0.6f, 0.6f); }
            fl.Dot(f.x * W, f.y * H, m * 0.004f * (0.4f + f.z * 1.4f), Color(1, 1, 1) * (0.4f + 0.6f * f.z));
        }
        fl.Draw(r, BLEND_ADD, dot);
    }

};

Scene* CreateScene() { return new SaverScene; }
