// fireworks.cpp - "Fireworks": rockets climb, burst into glowing sparks
// that fall under gravity and leave fading trails.
#include "../../common/scenekit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Fireworks Settings",
    1, {
        { L"&Colors:", L"Colors", 0, L"Festival|Gold|Patriotic|Pastel" },
    },
    3, {
        { L"Launches", L"Rate", 50, L"Few", L"Many" },
        { L"Burst size", L"Size", 50, L"Small", L"Big" },
        { L"Trail length", L"Trail", 50, L"Short", L"Long" },
    },
};

const wchar_t* RegistryName() { return L"Fireworks"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Spark {
    float x, y, vx, vy, life, maxLife, size;
    Color c;
    bool rocket;      // still climbing; bursts when it slows down
    int kind;         // burst shape
};

class FireworksScene : public Scene {
    std::vector<Spark> sparks;
    Canvas2D canvas;
    int width = 1, height = 1;
    float launchTimer = 0, keep = 0.05f, unit = 1;
    bool cleared = false;

    float Gravity() const { return 9.8f * 30 * unit; }   // pixels/s^2

    Color PickColor() const {
        switch (g_cfg.choice[0]) {
        case 1:  return Hsv(RandF(0.08f, 0.14f), RandF(0.5f, 0.9f), 1);
        case 2: { int k = RandI(0, 2); return k == 0 ? Color(1, 0.15f, 0.15f) : k == 1 ? Color(1, 1, 1) : Color(0.25f, 0.4f, 1); }
        case 3:  return Hsv(RandF(0, 1), 0.35f, 1);
        default: return Hsv(RandF(0, 1), 0.85f, 1);
        }
    }

    void Launch() {
        Spark s = {};
        s.x = RandF(-0.4f, 0.4f) * width;
        s.y = -height * 0.5f;
        // Burst height scales with the screen, so tall portrait screens
        // get fireworks all the way up.
        float apex = RandF(0.45f, 0.85f) * height;
        s.vy = sqrtf(2 * Gravity() * apex);   // just enough speed to reach the apex
        s.vx = RandF(-0.05f, 0.05f) * width;
        s.life = s.maxLife = 10;
        s.size = 2.0f * unit;
        s.c = Color(1, 0.85f, 0.6f);
        s.rocket = true;
        s.kind = RandI(0, 3);
        sparks.push_back(s);
    }

    void Burst(const Spark& rk) {
        int n = 60 + (int)(g_cfg.slider[1] * 1.6f);
        float power = (0.15f + g_cfg.slider[1] / 100.0f * 0.2f) * (float)(width < height ? width : height);
        Color c1 = PickColor(), c2 = PickColor();
        for (int i = 0; i < n; i++) {
            Spark s = {};
            float a = RandF(0, 2 * kPi), v;
            switch (rk.kind) {
            case 1:  v = power * (i % 2 ? 1.0f : 0.6f); a = 2 * kPi * i / n; break;   // double ring
            case 2:  v = power * RandF(0.2f, 1.0f) * (0.7f + 0.3f * cosf(5 * a)); break; // star
            default: v = power * sqrtf(RandF(0.05f, 1.0f)); break;                      // peony
            }
            s.x = rk.x; s.y = rk.y;
            s.vx = cosf(a) * v + rk.vx * 0.3f;
            s.vy = sinf(a) * v + rk.vy * 0.3f;
            s.maxLife = s.life = RandF(1.2f, 2.4f);
            s.size = RandF(1.2f, 2.0f) * unit;
            s.c = (rk.kind == 3 && i % 2) ? c2 : c1;
            sparks.push_back(s);
        }
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        r.SetPersistent(true);   // trails: keep and fade the previous image
        Resize(w, h);
        keep = 0.0005f + powf(g_cfg.slider[2] / 100.0f, 2) * 0.15f;
        return true;
    }

    void Resize(int w, int h) override {
        width = w; height = h > 0 ? h : 1;
        unit = (float)(width < height ? width : height) / 1080.0f;   // scale with resolution
        if (unit < 0.3f) unit = 0.3f;
        cleared = false;
    }

    void Frame(Renderer& r, float dt) override {
        float rate = 0.3f + g_cfg.slider[0] / 100.0f * 3.0f;   // launches per second
        launchTimer -= dt;
        if (launchTimer <= 0) { Launch(); launchTimer = RandF(0.3f, 1.7f) / rate; }

        const float g = Gravity();
        std::vector<Spark> born;
        for (auto& s : sparks) {
            s.vy -= g * dt * (s.rocket ? 1.0f : 0.35f);
            float drag = s.rocket ? 1.0f : powf(0.35f, dt);
            s.vx *= drag; s.vy *= drag;
            s.x += s.vx * dt; s.y += s.vy * dt;
            s.life -= dt;
            if (s.rocket && s.vy <= 0) { s.life = 0; born.push_back(s); }   // apex: burst
        }
        for (auto& b : born) Burst(b);
        sparks.erase(std::remove_if(sparks.begin(), sparks.end(), [](const Spark& s) { return s.life <= 0; }), sparks.end());

        r.BeginFrame(!cleared);
        cleared = true;
        r.FadeToBlack(FadeAlpha(keep, dt));
        canvas.Begin(width, height);
        for (auto& s : sparks) {
            float f = s.rocket ? 1.0f : s.life / s.maxLife;
            float twinkle = s.rocket ? 1.0f : 0.75f + 0.25f * sinf(s.life * 40 + s.x);
            Color c = s.c * (f * twinkle);
            // A short streak along the velocity looks like motion blur.
            float sx = s.x - s.vx * 0.016f, sy = s.y - s.vy * 0.016f;
            canvas.Line(sx, sy, s.x, s.y, s.size * 0.6f, c * 0.3f, c);
            canvas.Dot(s.x, s.y, s.size, c);
        }
        canvas.Draw(r, BLEND_ADD);
    }
};

Scene* CreateScene() { return new FireworksScene; }
