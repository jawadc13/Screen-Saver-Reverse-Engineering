// bubbles.cpp - "Bubbles": glassy soap bubbles drifting up the screen with
// shimmering highlights (after the Windows 7 Bubbles screensaver).
#include "../../common/scenekit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"Bubbles Settings",
    1, {
        { L"&Tint:", L"Tint", 1, L"Clear|Rainbow film|Blue|Green|Gold" },
    },
    3, {
        { L"Speed", L"Speed", 40, L"Slow", L"Fast" },
        { L"Number of bubbles", L"Count", 40, L"Few", L"Many" },
        { L"Bubble size", L"Size", 50, L"Small", L"Large" },
    },
};

const wchar_t* RegistryName() { return L"Bubbles"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Bubble {
    Vec3 pos;
    float radius, rise, wobble, phase, hue, spin;
};

class BubblesScene : public Scene {
    GpuMesh sphere;
    std::vector<Bubble> bubbles;
    int width = 1, height = 1;
    float boxX = 10, boxY = 7, speed = 1, t = 0;

    void Spawn(Bubble& b, bool anywhere) {
        float size = 0.4f + g_cfg.slider[2] / 100.0f * 1.4f;
        b.radius = size * RandF(0.4f, 1.0f);
        b.pos = Vec3(RandF(-boxX, boxX), anywhere ? RandF(-boxY, boxY) : -boxY - b.radius - RandF(0, 3), RandF(-4, 4));
        b.rise = RandF(0.6f, 1.4f) / sqrtf(b.radius);   // small bubbles rise faster
        b.wobble = RandF(0.2f, 0.7f);
        b.phase = RandF(0, 6.28f);
        b.hue = RandF(0, 1);
        b.spin = RandF(-40, 40);
    }

    Color Tint(const Bubble& b) const {
        switch (g_cfg.choice[0]) {
        case 0:  return Color(0.10f, 0.11f, 0.13f);
        case 2:  return Color(0.03f, 0.08f, 0.22f);
        case 3:  return Color(0.03f, 0.18f, 0.07f);
        case 4:  return Color(0.22f, 0.15f, 0.02f);
        default: return Hsv(b.hue + t * 0.05f, 0.7f, 0.22f);
        }
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        Mesh m;
        unsigned char white[4] = { 255, 255, 255, 255 };
        m.AddSphere(Vec3(0, 0, 0), 1.0f, preview ? 16 : 40, white);
        r.Upload(sphere, m);
        Resize(w, h);
        speed = 0.3f + g_cfg.slider[0] / 100.0f * 2.0f;
        bubbles.resize(6 + g_cfg.slider[1] * 54 / 100);
        for (auto& b : bubbles) Spawn(b, true);
        return true;
    }

    void Resize(int w, int h) override {
        width = w; height = h > 0 ? h : 1;
        float aspect = (float)width / height;
        boxY = 7; boxX = 7 * aspect;   // the visible area at z = 0
    }

    void Frame(Renderer& r, float dt) override {
        t += dt;
        for (auto& b : bubbles) {
            b.pos.y += b.rise * speed * dt;
            b.pos.x += sinf(t * b.wobble * 2 + b.phase) * 0.3f * speed * dt;
            if (b.pos.y - b.radius > boxY + 1) Spawn(b, false);   // "pop" at the top, reappear below
        }

        r.BeginFrame(true);
        float aspect = (float)width / height;
        float dist = boxY / tanf(22.5f * kPi / 180);
        r.SetCamera(Mat4::Translate(0, 0, -dist), Mat4::Perspective(45.0f, aspect, 1.0f, dist + 20));
        r.SetLight(0, Vec3(-0.5f, 0.7f, 0.8f), 1, 1, 1);
        r.SetLight(1, Vec3(0.6f, -0.2f, 0.5f), 0.5f, 0.6f, 0.9f);
        r.SetAmbient(0.05f, 0.05f, 0.06f);
        // Additive blending: a dim body plus bright specular highlights reads
        // as thin, transparent glass, with no need to sort the bubbles.
        for (auto& b : bubbles) {
            DrawParams p;
            p.world = Mat4::Translate(b.pos.x, b.pos.y, b.pos.z) * Mat4::Rotate(b.spin * t, 0, 1, 0) * Mat4::Scale(b.radius);
            p.vertexColor = false;
            Color c = Tint(b);
            p.color[0] = c.r; p.color[1] = c.g; p.color[2] = c.b; p.color[3] = 1;
            p.specular[0] = p.specular[1] = p.specular[2] = 1.0f;
            p.shininess = 60;
            p.rim = 1.0f;
            p.depth = false;
            p.blend = BLEND_ADD;
            r.Draw(sphere, p);
        }
    }
};

Scene* CreateScene() { return new BubblesScene; }
