// bokeh.cpp - "Aero Bokeh": soft out-of-focus lights drifting at different
// depths over a fresh gradient, with the odd sparkle - pure Frutiger Aero.
#include "../../common/aerokit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Aero Bokeh Settings",
    1, {
        { L"&Palette:", L"Palette", 0, L"Aqua and lime|Ocean|Spring|Sunset|Rainbow" },
    },
    3, {
        { L"Speed", L"Speed", 30, L"Slow", L"Fast" },
        { L"Number of lights", L"Count", 50, L"Few", L"Many" },
        { L"Light size", L"Size", 50, L"Small", L"Large" },
    },
};

const wchar_t* RegistryName() { return L"AeroBokeh"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Light { float x, y, vx, vy, r, depth, a, phase, pick; };
struct Sparkle { float x, y, life, size; };

class BokehScene : public Scene {
    Canvas2D bg, lights, sparkles;
    Texture *disc = nullptr, *dot = nullptr;
    std::vector<Light> pool;
    std::vector<Sparkle> sparks;
    int width = 1, height = 1;
    float t = 0, speed = 1, sparkTimer = 0;

    void Colors(Color& top, Color& bottom, Color& a, Color& b) const {
        switch (g_cfg.choice[0]) {
        case 1:  top = Color(0.0f, 0.1f, 0.3f); bottom = Color(0.0f, 0.4f, 0.6f); a = Color(0.3f, 0.8f, 1); b = Color(0.6f, 1, 1); break;
        case 2:  top = Color(0.35f, 0.65f, 0.25f); bottom = Color(0.85f, 0.95f, 0.6f); a = Color(1, 1, 0.7f); b = Color(0.7f, 1, 0.5f); break;
        case 3:  top = Color(0.25f, 0.1f, 0.35f); bottom = Color(0.95f, 0.5f, 0.3f); a = Color(1, 0.8f, 0.4f); b = Color(1, 0.5f, 0.6f); break;
        case 4:  top = Color(0.05f, 0.1f, 0.25f); bottom = Color(0.1f, 0.35f, 0.5f); a = Color(1, 1, 1); b = Color(1, 1, 1); break;
        default: top = Color(0.0f, 0.25f, 0.45f); bottom = Color(0.35f, 0.75f, 0.35f); a = Color(0.4f, 0.95f, 1); b = Color(0.75f, 1, 0.35f); break;
        }
    }

    void Spawn(Light& l, bool anywhere) {
        float m = (float)(width < height ? width : height);
        l.depth = RandF(0, 1);   // 0 = far: small, faint, slow; 1 = near: big, soft, fast
        l.r = m * (0.015f + g_cfg.slider[2] / 100.0f * 0.06f) * (0.4f + 1.6f * l.depth * l.depth);
        float ang = RandF(0, 6.28f), sp = m * 0.02f * (0.3f + l.depth);
        l.vx = cosf(ang) * sp * 0.6f;
        l.vy = fabsf(sinf(ang)) * sp + m * 0.005f;   // overall gentle upward drift
        l.x = RandF(-0.5f, 0.5f) * width;
        l.y = anywhere ? RandF(-0.5f, 0.5f) * height : -height * 0.5f - l.r;
        l.a = (0.12f + 0.25f * (1 - l.depth)) * RandF(0.6f, 1.0f);   // big near ones are fainter
        l.phase = RandF(0, 6.28f);
        l.pick = RandF(0, 1);
    }

public:
    ~BokehScene() { delete disc; delete dot; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        disc = aero::Bokeh(r);
        dot = aero::SoftDot(r);
        speed = 0.3f + g_cfg.slider[0] / 100.0f * 2.5f;
        pool.resize(20 + g_cfg.slider[1] * 120 / 100);
        for (auto& l : pool) Spawn(l, true);
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        t += dt;
        Color top, bottom, ca, cb;
        Colors(top, bottom, ca, cb);
        bg.Begin(width, height);
        Color s[3] = { top, Lerp(top, bottom, 0.55f), bottom };
        aero::VerticalGradient(bg, width, height, s, 3);

        // Far lights first; respawning changes depth, so sort every frame.
        std::sort(pool.begin(), pool.end(), [](const Light& a, const Light& b) { return a.depth < b.depth; });
        lights.Begin(width, height);
        for (auto& l : pool) {
            l.x += (l.vx + sinf(t * 0.4f + l.phase) * l.r * 0.3f) * dt * speed;
            l.y += l.vy * dt * speed;
            if (l.y - l.r > height * 0.5f || fabsf(l.x) > width * 0.5f + l.r * 2) Spawn(l, false);
            Color c = g_cfg.choice[0] == 4 ? Hsv(l.pick + t * 0.03f, 0.55f, 1) : Lerp(ca, cb, l.pick);
            float breathe = 0.8f + 0.2f * sinf(t * 0.8f + l.phase);
            lights.Rect(l.x - l.r, l.y - l.r, l.x + l.r, l.y + l.r, c.WithAlpha(l.a * breathe));
        }

        // Occasional four-pointed sparkles.
        sparkTimer -= dt;
        if (sparkTimer <= 0) {
            float m = (float)(width < height ? width : height);
            sparks.push_back({ RandF(-0.5f, 0.5f) * width, RandF(-0.5f, 0.5f) * height, 1.2f, m * RandF(0.02f, 0.05f) });
            sparkTimer = RandF(0.2f, 0.8f);
        }
        sparkles.Begin(width, height);
        for (auto& sp : sparks) {
            sp.life -= dt;
            float k = sp.life > 0.6f ? (1.2f - sp.life) / 0.6f : sp.life / 0.6f;   // fade in, fade out
            k = fmaxf(0, k);
            Color c(1, 1, 1, 0.9f * k);
            float len = sp.size * k, th = sp.size * 0.06f;
            sparkles.Rect(sp.x - len, sp.y - th, sp.x + len, sp.y + th, c);
            sparkles.Rect(sp.x - th, sp.y - len, sp.x + th, sp.y + len, c);
            sparkles.Rect(sp.x - th * 4, sp.y - th * 4, sp.x + th * 4, sp.y + th * 4, c);
        }
        sparks.erase(std::remove_if(sparks.begin(), sparks.end(), [](const Sparkle& s) { return s.life <= 0; }), sparks.end());

        r.BeginFrame(true);
        bg.Draw(r, BLEND_OPAQUE);
        lights.Draw(r, BLEND_ADD, disc);
        sparkles.Draw(r, BLEND_ADD, dot);
    }
};

Scene* CreateScene() { return new BokehScene; }
