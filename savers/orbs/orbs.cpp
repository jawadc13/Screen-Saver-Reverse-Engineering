// orbs.cpp - "Aero Glass Orbs": glossy gel orbs, like Aero buttons set
// free, floating up through a bright sky and gently nudging each other.
#include "../../common/aerokit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Aero Glass Orbs Settings",
    2, {
        { L"&Orb colors:", L"Colors", 2, L"Aqua|Lime|Aqua and lime|Rainbow|Clear glass" },
        { L"&Background:", L"Background", 0, L"Bright sky|Deep ocean|Spring green|Pearl white" },
    },
    3, {
        { L"Speed", L"Speed", 35, L"Slow", L"Fast" },
        { L"Number of orbs", L"Count", 45, L"Few", L"Many" },
        { L"Orb size", L"Size", 50, L"Small", L"Large" },
    },
};

const wchar_t* RegistryName() { return L"AeroOrbs"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Orb { float x, y, vx, vy, r, depth, phase, hue; Color tint; };

class OrbsScene : public Scene {
    Canvas2D bg, shine, orbLayer;
    Texture *atlas = nullptr, *dotTex = nullptr;
    std::vector<Orb> orbs;
    int width = 1, height = 1;
    float t = 0, speed = 1;

    Color Tint(float hue) const {
        switch (g_cfg.choice[0]) {
        case 1:  return Color(0.45f, 0.95f, 0.1f);
        case 2:  return hue < 0.5f ? Color(0.0f, 0.6f, 1.0f) : Color(0.45f, 0.95f, 0.1f);
        case 3:  return Hsv(hue, 0.6f, 1);
        case 4:  return Color(0.92f, 0.97f, 1.0f);
        default: return Color(0.0f, 0.6f, 1.0f);
        }
    }

    void Spawn(Orb& o, bool anywhere) {
        float m = (float)(width < height ? width : height);
        o.depth = RandF(0.35f, 1.0f);                         // 1 = near: bigger, faster, clearer
        o.r = m * (0.05f + g_cfg.slider[2] / 100.0f * 0.12f) * o.depth * RandF(0.7f, 1.3f);
        o.x = RandF(-0.5f, 0.5f) * width;
        o.y = anywhere ? RandF(-0.5f, 0.5f) * height : -height * 0.5f - o.r * 1.2f;
        o.vx = 0;
        o.vy = m * 0.05f * o.depth;
        o.phase = RandF(0, 6.28f);
        o.hue = RandF(0, 1);
        o.tint = Tint(o.hue);
    }

public:
    ~OrbsScene() { delete atlas; delete dotTex; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        atlas = aero::OrbAtlas(r);
        dotTex = aero::SoftDot(r);
        speed = 0.3f + g_cfg.slider[0] / 100.0f * 2.0f;
        orbs.resize(6 + g_cfg.slider[1] * 40 / 100);
        for (auto& o : orbs) Spawn(o, true);
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        t += dt;
        // Rise, sway, and push apart softly where orbs overlap.
        for (size_t i = 0; i < orbs.size(); i++) {
            Orb& a = orbs[i];
            for (size_t j = i + 1; j < orbs.size(); j++) {
                Orb& b = orbs[j];
                if (fabsf(a.depth - b.depth) > 0.25f) continue;   // different depth layers pass by
                float dx = b.x - a.x, dy = b.y - a.y, d = sqrtf(dx * dx + dy * dy) + 1e-3f, min = a.r + b.r;
                if (d < min) {
                    float push = (min - d) * 2.0f;
                    a.vx -= dx / d * push * dt; b.vx += dx / d * push * dt;
                }
            }
            a.vx *= powf(0.3f, dt);
            a.x += (a.vx + sinf(t * 0.6f + a.phase) * a.r * 0.4f) * dt * speed;
            a.y += a.vy * dt * speed;
            if (a.y - a.r * 1.2f > height * 0.5f) Spawn(a, false);
        }
        // Far orbs first.
        std::sort(orbs.begin(), orbs.end(), [](const Orb& a, const Orb& b) { return a.depth < b.depth; });

        bg.Begin(width, height);
        switch (g_cfg.choice[1]) {
        case 1: { Color s[3] = { {0.0f, 0.15f, 0.35f}, {0.0f, 0.35f, 0.6f}, {0.0f, 0.55f, 0.7f} }; aero::VerticalGradient(bg, width, height, s, 3); break; }
        case 2: { Color s[3] = { {0.55f, 0.85f, 1.0f}, {0.85f, 1.0f, 0.85f}, {0.35f, 0.75f, 0.2f} }; aero::VerticalGradient(bg, width, height, s, 3); break; }
        case 3: { Color s[3] = { {0.85f, 0.92f, 0.98f}, {0.97f, 0.99f, 1.0f}, {0.8f, 0.9f, 0.95f} }; aero::VerticalGradient(bg, width, height, s, 3); break; }
        default: { Color s[3] = { {0.15f, 0.5f, 0.95f}, {0.5f, 0.8f, 1.0f}, {0.9f, 0.97f, 1.0f} }; aero::VerticalGradient(bg, width, height, s, 3); break; }
        }
        // A soft sunburst from the upper left, very Aero.
        shine.Begin(width, height);
        float m = (float)(width > height ? width : height);
        shine.Rect(-width * 0.5f - m * 0.4f, height * 0.5f - m * 0.4f, -width * 0.5f + m * 0.4f, height * 0.5f + m * 0.4f, Color(1, 1, 1, 0.35f));

        // Each orb is drawn whole (body, highlight, twinkle), far to near.
        orbLayer.Begin(width, height);
        for (auto& o : orbs) {
            float a = 0.6f + 0.4f * o.depth;
            aero::OrbCell(orbLayer, aero::ORB_BODY, o.x, o.y, o.r, o.tint.WithAlpha(a));
            aero::OrbCell(orbLayer, aero::ORB_GLOSS, o.x, o.y, o.r, Color(1, 1, 1, 0.85f * a));
            float tw = 0.5f + 0.5f * sinf(t * 2 + o.phase * 3);
            aero::OrbCell(orbLayer, aero::ORB_DOT, o.x + o.r * 0.55f, o.y - o.r * 0.5f, o.r * 0.18f, Color(1, 1, 1, 0.6f * tw * a));
        }

        r.BeginFrame(true);
        bg.Draw(r, BLEND_OPAQUE);
        shine.Draw(r, BLEND_ADD, dotTex);
        orbLayer.Draw(r, BLEND_ALPHA, atlas);
    }
};

Scene* CreateScene() { return new OrbsScene; }
