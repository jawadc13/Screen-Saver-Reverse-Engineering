// glasspanes.cpp - "Aero Glass Panes": translucent panes of frosted Aero
// glass with glossy highlights and bright edges, drifting and turning
// slowly in 3D over a soft glowing backdrop (think Windows Vista Flip 3D).
#include "../../common/aerokit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Aero Glass Panes Settings",
    2, {
        { L"&Glass tint:", L"Tint", 0, L"Aqua|Emerald|Clear|Rainbow|Smoke" },
        { L"&Backdrop:", L"Backdrop", 0, L"Ocean glow|Spring|Twilight|Daylight" },
    },
    2, {
        { L"Speed", L"Speed", 35, L"Slow", L"Fast" },
        { L"Number of panes", L"Count", 45, L"Few", L"Many" },
    },
};

const wchar_t* RegistryName() { return L"AeroGlassPanes"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Pane { Vec3 pos, vel; float w, h, yaw, pitch, yawRate, phase, hue; };

class GlassPanesScene : public Scene {
    Canvas2D bg, glows;
    Texture* dot = nullptr;
    std::vector<Pane> panes;
    std::vector<Vertex> verts;
    struct Glow { float x, y, r, a, ph; Color c; };
    std::vector<Glow> glowSpots;
    int width = 1, height = 1;
    float t = 0, speed = 1, boxX = 8, boxY = 5;

    Color Tint(const Pane& p) const {
        switch (g_cfg.choice[0]) {
        case 1:  return Color(0.35f, 0.95f, 0.55f);
        case 2:  return Color(0.9f, 0.95f, 1.0f);
        case 3:  return Hsv(p.hue + t * 0.02f, 0.55f, 1);
        case 4:  return Color(0.35f, 0.4f, 0.45f);
        default: return Color(0.35f, 0.8f, 1.0f);
        }
    }

    void Spawn(Pane& p, bool anywhere) {
        p.w = RandF(2.6f, 5.0f);
        p.h = p.w * RandF(0.6f, 0.8f);
        float z = RandF(-8, 2);
        float dir = RandI(0, 1) ? 1.0f : -1.0f;
        p.vel = Vec3(dir * RandF(0.3f, 0.7f), RandF(-0.1f, 0.1f), 0);
        float edge = boxX + 6 - z * 0.4f;
        p.pos = Vec3(anywhere ? RandF(-boxX, boxX) : -dir * edge, RandF(-boxY, boxY) * 0.8f, z);
        p.yaw = RandF(-35, 35);
        p.pitch = RandF(-12, 12);
        p.yawRate = RandF(-8, 8);
        p.phase = RandF(0, 6.28f);
        p.hue = RandF(0, 1);
    }

    void AddPane(const Pane& p) {
        Mat4 m = Mat4::Translate(p.pos.x, p.pos.y + sinf(t * 0.5f + p.phase) * 0.3f, p.pos.z) *
                 Mat4::Rotate(p.yaw, 0, 1, 0) * Mat4::Rotate(p.pitch, 1, 0, 0);
        auto P = [&](float x, float y) {
            return Vec3(m.m[0][0] * x + m.m[0][1] * y + m.m[0][3], m.m[1][0] * x + m.m[1][1] * y + m.m[1][3], m.m[2][0] * x + m.m[2][1] * y + m.m[2][3]);
        };
        float hw = p.w * 0.5f, hh = p.h * 0.5f;
        Color tint = Tint(p);
        Vec3 n(0, 0, 1);
        // Frosted body: clearer at the bottom, whiter at the top.
        PushQuad(verts, P(-hw, -hh), P(hw, -hh), P(hw, hh), P(-hw, hh), n,
                 tint.WithAlpha(0.18f), tint.WithAlpha(0.18f), Lerp(tint, Color(1, 1, 1), 0.5f).WithAlpha(0.38f), Lerp(tint, Color(1, 1, 1), 0.5f).WithAlpha(0.38f));
        // Gloss: the classic Aero highlight across the top 45%, with a soft edge.
        float gy = hh - p.h * 0.45f;
        PushQuad(verts, P(-hw, gy), P(hw, gy + p.h * 0.08f), P(hw, hh), P(-hw, hh), n,
                 Color(1, 1, 1, 0.06f), Color(1, 1, 1, 0.06f), Color(1, 1, 1, 0.45f), Color(1, 1, 1, 0.45f));
        // Bright edges.
        float e = 0.035f;
        Color edge = Lerp(tint, Color(1, 1, 1), 0.6f).WithAlpha(0.75f);
        PushQuad(verts, P(-hw, hh - e), P(hw, hh - e), P(hw, hh), P(-hw, hh), n, edge, edge, edge, edge);
        PushQuad(verts, P(-hw, -hh), P(hw, -hh), P(hw, -hh + e), P(-hw, -hh + e), n, edge.WithAlpha(0.4f), edge.WithAlpha(0.4f), edge.WithAlpha(0.4f), edge.WithAlpha(0.4f));
        PushQuad(verts, P(-hw, -hh), P(-hw + e, -hh), P(-hw + e, hh), P(-hw, hh), n, edge.WithAlpha(0.4f), edge.WithAlpha(0.4f), edge, edge);
        PushQuad(verts, P(hw - e, -hh), P(hw, -hh), P(hw, hh), P(hw - e, hh), n, edge.WithAlpha(0.4f), edge.WithAlpha(0.4f), edge, edge);
    }

public:
    ~GlassPanesScene() { delete dot; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        dot = aero::SoftDot(r);
        Resize(w, h);
        speed = 0.3f + g_cfg.slider[0] / 100.0f * 2.0f;
        panes.resize(4 + g_cfg.slider[1] * 20 / 100);
        for (auto& p : panes) Spawn(p, true);
        for (int i = 0; i < 10; i++)
            glowSpots.push_back({ RandF(-0.5f, 0.5f), RandF(-0.5f, 0.5f), RandF(0.2f, 0.5f), RandF(0.15f, 0.35f), RandF(0, 6.28f), Color() });
        return true;
    }

    void Resize(int w, int h) override {
        width = w; height = h > 0 ? h : 1;
        float aspect = (float)width / height;
        boxY = aspect >= 1 ? 5 : 5 / aspect;
        boxX = aspect >= 1 ? 5 * aspect : 5;
    }

    void Frame(Renderer& r, float dt) override {
        t += dt * speed;
        Color top, bottom, spot;
        switch (g_cfg.choice[1]) {
        case 1:  top = Color(0.2f, 0.55f, 0.2f); bottom = Color(0.7f, 0.95f, 0.5f); spot = Color(0.9f, 1, 0.6f); break;
        case 2:  top = Color(0.05f, 0.05f, 0.25f); bottom = Color(0.45f, 0.25f, 0.55f); spot = Color(0.6f, 0.5f, 1); break;
        case 3:  top = Color(0.35f, 0.65f, 1.0f); bottom = Color(0.85f, 0.95f, 1.0f); spot = Color(1, 1, 1); break;
        default: top = Color(0.0f, 0.12f, 0.3f); bottom = Color(0.0f, 0.45f, 0.55f); spot = Color(0.3f, 0.9f, 1); break;
        }
        bg.Begin(width, height);
        Color s[2] = { top, bottom };
        aero::VerticalGradient(bg, width, height, s, 2);
        glows.Begin(width, height);
        float m = (float)(width > height ? width : height);
        for (auto& g : glowSpots)
            glows.Dot(g.x * width + sinf(t * 0.1f + g.ph) * m * 0.05f, g.y * height, g.r * m, spot.WithAlpha(g.a * (0.7f + 0.3f * sinf(t * 0.3f + g.ph))));

        for (auto& p : panes) {
            p.pos = p.pos + p.vel * (dt * speed);
            p.yaw += p.yawRate * dt * speed;
            if (fabsf(p.pos.x) > boxX + 8 - p.pos.z * 0.4f) Spawn(p, false);
        }
        std::sort(panes.begin(), panes.end(), [](const Pane& a, const Pane& b) { return a.pos.z < b.pos.z; });   // far first
        verts.clear();
        for (auto& p : panes) AddPane(p);

        r.BeginFrame(true);
        bg.Draw(r, BLEND_OPAQUE);
        glows.Draw(r, BLEND_ADD, dot);
        float aspect = (float)width / height;
        float dist = boxY / tanf(22.5f * kPi / 180);
        r.SetCamera(Mat4::Translate(0, 0, -dist), Mat4::Perspective(45.0f, aspect, 0.5f, dist + 30));
        DrawParams p;
        p.lit = false;
        p.depth = false;
        p.blend = BLEND_ALPHA;
        r.Draw(verts.data(), verts.size(), p);
    }
};

Scene* CreateScene() { return new GlassPanesScene; }
