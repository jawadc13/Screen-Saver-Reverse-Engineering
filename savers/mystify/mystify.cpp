// mystify.cpp - "Mystify": glowing polygons whose corners bounce around the
// screen, leaving fading trails (after the classic Windows Mystify).
#include "../../common/scenekit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"Mystify Settings",
    2, {
        { L"&Shapes:", L"Shapes", 1, L"One|Two|Three|Four" },
        { L"&Colors:", L"Colors", 0, L"Cycling|Rainbow|Ice|Fire" },
    },
    3, {
        { L"Speed", L"Speed", 50, L"Slow", L"Fast" },
        { L"Corners", L"Corners", 30, L"3", L"8" },
        { L"Echoes", L"Trail", 40, L"None", L"Many" },
    },
};

const wchar_t* RegistryName() { return L"Mystify"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Poly {
    std::vector<float> x, y, vx, vy;
    float hue, hueRate;
    // Echo history: snapshots of the corners, newest first. Like the
    // classic Mystify, the trail is a few discrete copies, not a smear.
    std::vector<std::vector<float>> hx, hy;
    std::vector<float> hhue;
};

class MystifyScene : public Scene {
    std::vector<Poly> polys;
    Canvas2D canvas;
    int width = 1, height = 1, echoes = 6;
    float speed = 1, sampleTimer = 0;

    void Bounce(float& p, float& v, float lim) {
        if (p > lim)  { p = lim;  v = -fabsf(v); }
        if (p < -lim) { p = -lim; v = fabsf(v); }
    }

    Color PolyColor(float hue, int corner) const {
        switch (g_cfg.choice[1]) {
        case 1:  return Hsv(hue + corner * 0.12f, 0.9f, 1);
        case 2:  return Hsv(0.5f + 0.08f * sinf(hue * 6.28f), 0.6f, 1);
        case 3:  return Hsv(0.02f + 0.1f * (0.5f + 0.5f * sinf(hue * 6.28f)), 0.95f, 1);
        default: return Hsv(hue, 0.85f, 1);
        }
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        int corners = 3 + (int)(g_cfg.slider[1] * 5 / 100);
        echoes = 1 + (int)(g_cfg.slider[2] * 15 / 100);
        polys.resize(g_cfg.choice[0] + 1);
        float scale = (float)(width < height ? width : height);
        speed = scale * (0.1f + g_cfg.slider[0] / 100.0f * 0.6f);   // pixels per second
        for (auto& p : polys) {
            for (int i = 0; i < corners; i++) {
                p.x.push_back(RandF(-0.5f, 0.5f) * width);
                p.y.push_back(RandF(-0.5f, 0.5f) * height);
                float a = RandF(0, 2 * kPi), sp = RandF(0.5f, 1.0f) * speed;
                p.vx.push_back(cosf(a) * sp);
                p.vy.push_back(sinf(a) * sp);
            }
            p.hue = RandF(0, 1);
            p.hueRate = RandF(0.03f, 0.08f);
        }
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        // Echo spacing is time-based (not per frame), so the trail looks the
        // same at 60 Hz and 175 Hz.
        const float spacing = 0.035f;
        sampleTimer += dt;
        bool sample = sampleTimer >= spacing;
        if (sample) sampleTimer = fmodf(sampleTimer, spacing);

        canvas.Begin(width, height);
        float lineW = height / 700.0f + 0.6f;
        for (auto& p : polys) {
            p.hue += p.hueRate * dt;
            size_t n = p.x.size();
            for (size_t i = 0; i < n; i++) {
                p.x[i] += p.vx[i] * dt;
                p.y[i] += p.vy[i] * dt;
                Bounce(p.x[i], p.vx[i], width * 0.5f);
                Bounce(p.y[i], p.vy[i], height * 0.5f);
            }
            if (sample) {
                p.hx.insert(p.hx.begin(), p.x);
                p.hy.insert(p.hy.begin(), p.y);
                p.hhue.insert(p.hhue.begin(), p.hue);
                if ((int)p.hx.size() > echoes) { p.hx.pop_back(); p.hy.pop_back(); p.hhue.pop_back(); }
            }
            // Live polygon at full brightness, then the echoes fading out.
            for (int e = -1; e < (int)p.hx.size(); e++) {
                const std::vector<float>& xs = e < 0 ? p.x : p.hx[e];
                const std::vector<float>& ys = e < 0 ? p.y : p.hy[e];
                float hue = e < 0 ? p.hue : p.hhue[e];
                // Fade by continuous age, not echo index, so the oldest echo has
                // already faded to nothing when it is dropped (no popping).
                float age = e < 0 ? 0 : sampleTimer + e * spacing;
                float fade = e < 0 ? 1.0f : 0.85f * fmaxf(0, 1.0f - age / (echoes * spacing));
                for (size_t i = 0; i < n; i++) {
                    size_t j = (i + 1) % n;
                    canvas.Line(xs[i], ys[i], xs[j], ys[j], lineW, PolyColor(hue, (int)i) * fade, PolyColor(hue, (int)j) * fade);
                }
            }
        }
        r.BeginFrame(true);
        canvas.Draw(r, BLEND_ADD);
    }
};

Scene* CreateScene() { return new MystifyScene; }
