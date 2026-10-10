// aquarium.cpp - "Aero Aqua Depths": sunlit tropical water - shafts of
// light from the surface, glossy fish, rising bubbles and swaying seaweed.
#include "../../common/aerokit.h"
#include "resource.h"
#include <algorithm>

static SimpleConfig g_cfg = {
    L"Aero Aqua Depths Settings",
    1, {
        { L"&Water:", L"Water", 0, L"Tropical lagoon|Deep blue|Green reef" },
    },
    3, {
        { L"Swim speed", L"Speed", 40, L"Lazy", L"Darting" },
        { L"Number of fish", L"Fish", 50, L"Few", L"Many" },
        { L"Bubbles", L"Bubbles", 50, L"Few", L"Lots" },
    },
};

const wchar_t* RegistryName() { return L"AeroAquarium"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct Fish { float x, y, size, speed, dir, phase, depth, wiggle; Color body, fin; };
struct Bubble { float x, y, r, speed, phase; };
struct Weed { float x, height, phase, width; Color col; };

class AquariumScene : public Scene {
    Canvas2D water, beams, plants, fishFins, fishLayer, bubbleLayer;
    Texture *atlas = nullptr;
    std::vector<Fish> fish;
    std::vector<Bubble> bubbles;
    std::vector<Weed> weeds;
    int width = 1, height = 1;
    float t = 0, speed = 1, unit = 1;

    void SpawnFish(Fish& f, bool anywhere) {
        static const Color bodies[] = { {1, 0.55f, 0.1f}, {0.2f, 0.6f, 1}, {1, 0.9f, 0.2f}, {0.3f, 0.95f, 0.6f}, {1, 0.35f, 0.5f}, {0.6f, 0.45f, 1} };
        f.depth = RandF(0.4f, 1.0f);
        f.size = unit * RandF(0.05f, 0.1f) * f.depth;
        f.dir = RandI(0, 1) ? 1.0f : -1.0f;
        f.speed = unit * RandF(0.04f, 0.1f) * f.depth;
        f.x = anywhere ? RandF(-0.5f, 0.5f) * width : -f.dir * (width * 0.5f + f.size * 3);
        f.y = RandF(-0.3f, 0.35f) * height;
        f.phase = RandF(0, 6.28f);
        f.wiggle = RandF(4, 7);
        f.body = bodies[RandI(0, 5)];
        f.fin = Lerp(f.body, Color(1, 1, 1), 0.35f);
    }

    void SpawnBubble(Bubble& b, bool anywhere) {
        b.r = unit * RandF(0.004f, 0.016f);
        b.x = RandF(-0.5f, 0.5f) * width;
        b.y = anywhere ? RandF(-0.5f, 0.5f) * height : -height * 0.5f - b.r;
        b.speed = unit * RandF(0.06f, 0.14f);
        b.phase = RandF(0, 6.28f);
    }

public:
    ~AquariumScene() { delete atlas; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        unit = (float)(width < height ? width : height);
        atlas = aero::OrbAtlas(r);
        speed = 0.4f + g_cfg.slider[0] / 100.0f * 1.8f;
        fish.resize(3 + g_cfg.slider[1] * 22 / 100);
        for (auto& f : fish) SpawnFish(f, true);
        bubbles.resize(10 + g_cfg.slider[2] * 90 / 100);
        for (auto& b : bubbles) SpawnBubble(b, true);
        int nWeeds = (int)(width / (unit * 0.06f));
        for (int i = 0; i < nWeeds; i++) {
            Weed wd;
            wd.x = RandF(-0.5f, 0.5f) * width;
            wd.height = unit * RandF(0.12f, 0.35f);
            wd.phase = RandF(0, 6.28f);
            wd.width = unit * RandF(0.008f, 0.016f);
            wd.col = Hsv(RandF(0.25f, 0.38f), 0.75f, RandF(0.45f, 0.8f));
            weeds.push_back(wd);
        }
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; unit = (float)(width < height ? width : height); }

    void Frame(Renderer& r, float dt) override {
        t += dt;
        float top = height * 0.5f, bottom = -height * 0.5f;

        water.Begin(width, height);
        switch (g_cfg.choice[0]) {
        case 1: { Color s[4] = { {0.15f, 0.55f, 0.85f}, {0.03f, 0.30f, 0.60f}, {0.01f, 0.12f, 0.35f}, {0.02f, 0.08f, 0.20f} }; aero::VerticalGradient(water, width, height, s, 4); break; }
        case 2: { Color s[4] = { {0.45f, 0.85f, 0.70f}, {0.10f, 0.60f, 0.50f}, {0.05f, 0.35f, 0.30f}, {0.55f, 0.55f, 0.35f} }; aero::VerticalGradient(water, width, height, s, 4); break; }
        default: { Color s[4] = { {0.55f, 0.95f, 1.0f}, {0.10f, 0.70f, 0.85f}, {0.02f, 0.40f, 0.65f}, {0.75f, 0.70f, 0.50f} }; aero::VerticalGradient(water, width, height, s, 4); break; }
        }

        // Light shafts from the surface, swaying gently.
        beams.Begin(width, height);
        for (int i = 0; i < 9; i++) {
            float x = (i / 8.0f - 0.5f) * width * 1.2f + sinf(t * 0.15f + i) * unit * 0.08f;
            float w0 = unit * (0.02f + 0.02f * sinf(i * 2.3f + t * 0.3f) + 0.02f);
            float slant = unit * 0.25f;
            Color c0(1, 1, 1, 0.16f + 0.08f * sinf(t * 0.5f + i * 1.3f)), c1(1, 1, 1, 0);
            PushQuad(beams.v, beams.P(x + slant - w0 * 3, bottom + height * 0.1f), beams.P(x + slant + w0 * 3, bottom + height * 0.1f),
                     beams.P(x + w0, top), beams.P(x - w0, top), Vec3(0, 0, 1), c1, c1, c0, c0);
        }

        // Seaweed: a chain of segments bending more towards the tip.
        plants.Begin(width, height);
        for (auto& wd : weeds) {
            const int seg = 10;
            float px = wd.x, py = bottom;
            for (int k = 0; k < seg; k++) {
                float f = (float)(k + 1) / seg;
                float nx = wd.x + sinf(t * 0.8f + wd.phase + f * 2) * wd.height * 0.15f * f * f;
                float ny = bottom + wd.height * f;
                float w0 = wd.width * (1 - (float)k / seg), w1 = wd.width * (1 - f);
                Color c0 = wd.col * (0.7f + 0.3f * (float)k / seg), c1 = wd.col * (0.7f + 0.3f * f);
                PushQuad(plants.v, plants.P(px - w0, py), plants.P(px + w0, py), plants.P(nx + w1, ny), plants.P(nx - w1, ny), Vec3(0, 0, 1), c0, c0, c1, c1);
                px = nx; py = ny;
            }
        }

        // Fish: swim across, bob, turn around at the edges.
        std::sort(fish.begin(), fish.end(), [](const Fish& a, const Fish& b) { return a.depth < b.depth; });
        fishFins.Begin(width, height);
        fishLayer.Begin(width, height);
        for (auto& f : fish) {
            f.x += f.dir * f.speed * speed * dt;
            if (f.x * f.dir > width * 0.5f + f.size * 4) SpawnFish(f, false);
            float y = f.y + sinf(t * 0.7f + f.phase) * f.size * 0.8f;
            float fade = 0.55f + 0.45f * f.depth;
            // Tail: a fan that flaps.
            float flap = sinf(t * f.wiggle * speed + f.phase) * f.size * 0.45f;
            float tx = f.x - f.dir * f.size * 1.6f;
            Color fin = f.fin.WithAlpha(0.9f * fade);
            PushVertex(fishFins.v, fishFins.P(f.x - f.dir * f.size * 0.9f, y), Vec3(0, 0, 1), 0, 0, fin);
            PushVertex(fishFins.v, fishFins.P(tx - f.dir * f.size * 0.5f, y + f.size * 0.7f + flap), Vec3(0, 0, 1), 0, 0, fin);
            PushVertex(fishFins.v, fishFins.P(tx - f.dir * f.size * 0.5f, y - f.size * 0.7f + flap), Vec3(0, 0, 1), 0, 0, fin);
            // Top fin.
            PushVertex(fishFins.v, fishFins.P(f.x - f.dir * f.size * 0.3f, y + f.size * 0.6f), Vec3(0, 0, 1), 0, 0, fin);
            PushVertex(fishFins.v, fishFins.P(f.x + f.dir * f.size * 0.4f, y + f.size * 0.6f), Vec3(0, 0, 1), 0, 0, fin);
            PushVertex(fishFins.v, fishFins.P(f.x - f.dir * f.size * 0.6f, y + f.size * 1.05f), Vec3(0, 0, 1), 0, 0, fin);
            // Glossy body: stretched orb + gloss, then a bright eye.
            aero::OrbCell(fishLayer, aero::ORB_BODY, f.x, y, f.size * 0.75f, Lerp(f.body, Color(1, 1, 1), 0.1f).WithAlpha(0.95f * fade), 1.6f);
            aero::OrbCell(fishLayer, aero::ORB_BODY, f.x, y, f.size * 0.72f, f.body.WithAlpha(0.8f * fade), 1.55f);
            aero::OrbCell(fishLayer, aero::ORB_GLOSS, f.x, y, f.size * 0.75f, Color(1, 1, 1, 0.8f * fade), 1.6f);
            aero::OrbCell(fishLayer, aero::ORB_DOT, f.x + f.dir * f.size * 0.7f, y + f.size * 0.12f, f.size * 0.14f, Color(0.05f, 0.05f, 0.1f, fade));
        }

        bubbleLayer.Begin(width, height);
        for (auto& b : bubbles) {
            b.y += b.speed * speed * dt;
            b.x += sinf(t * 2 + b.phase) * b.r * 2 * dt;
            if (b.y - b.r > top) SpawnBubble(b, false);
            aero::OrbCell(bubbleLayer, aero::ORB_BODY, b.x, b.y, b.r, Color(0.85f, 0.97f, 1, 0.7f));
            aero::OrbCell(bubbleLayer, aero::ORB_GLOSS, b.x, b.y, b.r, Color(1, 1, 1, 0.9f));
        }

        r.BeginFrame(true);
        water.Draw(r, BLEND_OPAQUE);
        beams.Draw(r, BLEND_ADD);
        plants.Draw(r, BLEND_ALPHA);
        fishFins.Draw(r, BLEND_ALPHA);
        fishLayer.Draw(r, BLEND_ALPHA, atlas);
        bubbleLayer.Draw(r, BLEND_ALPHA, atlas);
    }
};

Scene* CreateScene() { return new AquariumScene; }
