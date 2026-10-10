// caustics.cpp - "Caustics": rippling sunlight dancing across a sandy sea floor
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Caustics Settings",
    2, { { L"&Water:", L"Water", 0, L"Tropical|Pool|Deep|Lagoon green" }, REST_BAND_CHOICE },
    4, { REST_SPEED_SLIDER, { L"Ripple size", L"Scale", 50, L"Fine", L"Large" }, REST_EVERY_SLIDER, REST_LENGTH_SLIDER },
};

const wchar_t* RegistryName() { return L"FullCaustics"; }
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

    float s1 = 0, s2 = 0;
    Canvas2D field;
    void Reseed() override { s1 = RandF(0, 50); s2 = RandF(0, 50); }
    void Draw(Renderer& r, float dt) override {
        Color deep, light;
        switch (Style()) {
        case 1:  deep = Color(0.05f, 0.35f, 0.6f); light = Color(0.75f, 0.95f, 1); break;
        case 2:  deep = Color(0.01f, 0.06f, 0.18f); light = Color(0.3f, 0.6f, 0.9f); break;
        case 3:  deep = Color(0.05f, 0.3f, 0.25f); light = Color(0.7f, 1, 0.75f); break;
        default: deep = Color(0.08f, 0.35f, 0.45f); light = Color(1, 0.97f, 0.8f); break;
        }
        float scale = (0.6f + Amt() * 1.6f) * MinDim() * 0.12f, tt = t * 0.6f * Spd();
        DrawField(r, field, width, height, MinDim() / 70, [&](float x, float y) {
            float u = x / scale, v = y / scale;
            // Two layers of warped interference make the bright caustic net.
            float a = sinf(u + sinf(v * 0.8f + tt + s1)) + sinf(v * 1.1f + sinf(u * 0.7f - tt * 0.8f + s2));
            float b = sinf((u + v) * 0.7f + tt * 1.3f) + sinf((u - v) * 0.9f - tt * 0.9f);
            float c = powf(fmaxf(0, 1 - fabsf(a + b * 0.5f) * 0.6f), 4);
            float sand = 0.55f + 0.1f * sinf(u * 3.1f + v * 2.3f);
            return Lerp(deep * sand * 1.6f, light, c * 0.85f);
        });
    }

};

Scene* CreateScene() { return new SaverScene; }
