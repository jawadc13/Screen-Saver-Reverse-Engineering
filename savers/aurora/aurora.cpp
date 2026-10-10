// aurora.cpp - "Aero Aurora": soft flowing wisps of light over a deep
// blue-green glow, with drifting bokeh - the Windows Vista "Aurora" look.
#include "../../common/aerokit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"Aero Aurora Settings",
    1, {
        { L"&Palette:", L"Palette", 0, L"Vista blue-green|Aqua|Lime|Sunrise|Violet" },
    },
    3, {
        { L"Speed", L"Speed", 40, L"Slow", L"Fast" },
        { L"Light wisps", L"Wisps", 50, L"Few", L"Many" },
        { L"Bokeh", L"Bokeh", 50, L"None", L"Lots" },
    },
};

const wchar_t* RegistryName() { return L"AeroAurora"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Wisp { float base, amp, k1, k2, w1, w2, ph, thick, hueOff, bright; };
struct Light { float x, y, vx, vy, r, a, hueOff; };

class AuroraScene : public Scene {
    Canvas2D bg, glow, dots;
    Texture* bokeh = nullptr;
    std::vector<Wisp> wisps;
    std::vector<Light> lights;
    int width = 1, height = 1;
    float t = 0, speed = 1;

    // Palette: background top, background bottom, wisp base color.
    void Palette(Color& top, Color& bottom, Color& wisp, Color& wisp2) const {
        switch (g_cfg.choice[0]) {
        case 1: top = Color(0.00f, 0.10f, 0.25f); bottom = Color(0.0f, 0.45f, 0.60f); wisp = Color(0.4f, 0.9f, 1.0f); wisp2 = Color(0.7f, 1.0f, 1.0f); break;
        case 2: top = Color(0.02f, 0.15f, 0.05f); bottom = Color(0.25f, 0.55f, 0.10f); wisp = Color(0.7f, 1.0f, 0.3f); wisp2 = Color(1.0f, 1.0f, 0.6f); break;
        case 3: top = Color(0.10f, 0.05f, 0.25f); bottom = Color(0.85f, 0.45f, 0.30f); wisp = Color(1.0f, 0.8f, 0.4f); wisp2 = Color(1.0f, 0.6f, 0.7f); break;
        case 4: top = Color(0.05f, 0.02f, 0.20f); bottom = Color(0.35f, 0.15f, 0.55f); wisp = Color(0.8f, 0.6f, 1.0f); wisp2 = Color(0.5f, 0.8f, 1.0f); break;
        default: top = Color(0.00f, 0.08f, 0.22f); bottom = Color(0.0f, 0.38f, 0.42f); wisp = Color(0.3f, 1.0f, 0.6f); wisp2 = Color(0.4f, 0.8f, 1.0f); break;
        }
    }

    void Spawn(Light& l, bool anywhere) {
        float m = (float)(width < height ? width : height);
        l.r = m * RandF(0.01f, 0.06f);
        l.x = RandF(-0.5f, 0.5f) * width;
        l.y = anywhere ? RandF(-0.5f, 0.5f) * height : -height * 0.5f - l.r;
        l.vx = RandF(-0.02f, 0.02f) * m;
        l.vy = RandF(0.01f, 0.04f) * m;
        l.a = RandF(0.05f, 0.25f);
        l.hueOff = RandF(0, 1);
    }

public:
    ~AuroraScene() { delete bokeh; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        bokeh = aero::Bokeh(r);
        speed = 0.2f + g_cfg.slider[0] / 100.0f * 1.2f;
        wisps.resize(3 + g_cfg.slider[1] * 9 / 100);
        for (auto& wp : wisps) {
            wp.base = RandF(-0.25f, 0.3f);
            wp.amp = RandF(0.05f, 0.22f);
            wp.k1 = RandF(1.0f, 2.5f); wp.k2 = RandF(2.5f, 5.0f);
            wp.w1 = RandF(0.15f, 0.4f); wp.w2 = RandF(-0.5f, -0.2f);
            wp.ph = RandF(0, 6.28f);
            wp.thick = RandF(0.04f, 0.16f);
            wp.hueOff = RandF(0, 1);
            wp.bright = RandF(0.25f, 0.6f);
        }
        lights.resize(g_cfg.slider[2] * 60 / 100);
        for (auto& l : lights) Spawn(l, true);
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        t += dt * speed;
        Color top, bottom, wc, wc2;
        Palette(top, bottom, wc, wc2);

        bg.Begin(width, height);
        Color stops[3] = { top, Lerp(top, bottom, 0.6f), bottom };
        aero::VerticalGradient(bg, width, height, stops, 3);

        // Wisps: the screen's short side sets their scale so they look the
        // same on ultrawide and portrait monitors.
        glow.Begin(width, height);
        float m = (float)(width < height ? width : height);
        for (auto& wp : wisps) {
            auto yAt = [&](float x) {
                float u = x / m;
                return height * wp.base + m * wp.amp * (sinf(u * wp.k1 + t * wp.w1 + wp.ph) + 0.4f * sinf(u * wp.k2 + t * wp.w2 + wp.ph * 2));
            };
            float pulse = 0.75f + 0.25f * sinf(t * 0.7f + wp.ph);
            Color c = Lerp(wc, wc2, 0.5f + 0.5f * sinf(wp.hueOff * 6.28f + t * 0.2f)).WithAlpha(wp.bright * pulse);
            aero::LightBand(glow, width, 96, yAt, m * wp.thick, c);
            aero::LightBand(glow, width, 96, yAt, m * wp.thick * 0.18f, Color(1, 1, 1, wp.bright * 0.5f * pulse));   // bright core
        }

        dots.Begin(width, height);
        for (auto& l : lights) {
            l.x += l.vx * dt * speed; l.y += l.vy * dt * speed;
            if (l.y - l.r > height * 0.5f) Spawn(l, false);
            Color c = Lerp(wc, Color(1, 1, 1), 0.4f + 0.3f * sinf(l.hueOff * 6.28f)).WithAlpha(l.a);
            dots.Rect(l.x - l.r, l.y - l.r, l.x + l.r, l.y + l.r, c);
        }

        r.BeginFrame(true);
        bg.Draw(r, BLEND_OPAQUE);
        glow.Draw(r, BLEND_ADD);
        dots.Draw(r, BLEND_ADD, bokeh);
    }
};

Scene* CreateScene() { return new AuroraScene; }
