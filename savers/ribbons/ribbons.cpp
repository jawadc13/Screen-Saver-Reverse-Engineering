// ribbons.cpp - "Ribbons": glowing ribbons sweeping through 3D space and
// twisting as they go (after the Windows Vista/7 Ribbons screensaver).
#include "../../common/scenekit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"Ribbons Settings",
    1, {
        { L"&Colors:", L"Colors", 0, L"Rainbow|Warm|Cool|Pastel|White" },
    },
    3, {
        { L"Speed", L"Speed", 45, L"Slow", L"Fast" },
        { L"Number of ribbons", L"Count", 40, L"Few", L"Many" },
        { L"Ribbon width", L"Width", 50, L"Thin", L"Wide" },
    },
};

const wchar_t* RegistryName() { return L"Ribbons"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

static const int kTrail = 160;      // samples along each ribbon
static const float kSample = 0.02f; // path time between samples

struct Ribbon {
    float f[6], ph[6];           // path frequencies/phases
    float hue, twist, life, t;
    Vec3 pos[kTrail], side[kTrail];
    int head = 0, filled = 0;
    float sampleTimer = 0;
};

class RibbonsScene : public Scene {
    std::vector<Ribbon> ribbons;
    std::vector<Vertex> verts;
    int width = 1, height = 1;
    float speed = 1, halfWidth = 0.3f, orbit = 0, boxX = 8, boxY = 5;

    void Reset(Ribbon& rb) {
        for (int i = 0; i < 6; i++) { rb.f[i] = RandF(0.15f, 0.5f); rb.ph[i] = RandF(0, 6.28f); }
        rb.hue = RandF(0, 1);
        rb.twist = RandF(-3, 3);
        rb.life = RandF(10, 25);
        rb.t = RandF(0, 100);
        rb.head = rb.filled = 0;
    }

    // Position and (twisting) half-width vector of the ribbon at path time st.
    void Sample(const Ribbon& rb, float st, Vec3& pos, Vec3& side) const {
        Vec3 d = Normalize(Path(rb, st + 0.01f) - Path(rb, st));
        float a = st * rb.twist;
        // A fixed reference axis keeps the ribbon from flipping.
        Vec3 ref = Normalize(Cross(d, Vec3(0.3f, 1, 0.2f)));
        Vec3 ref2 = Cross(d, ref);
        pos = Path(rb, st);
        side = (ref * cosf(a) + ref2 * sinf(a)) * halfWidth;
    }

    Vec3 Path(const Ribbon& rb, float t) const {
        return Vec3(boxX * (0.7f * sinf(rb.f[0] * t + rb.ph[0]) + 0.3f * sinf(rb.f[3] * t * 1.7f + rb.ph[3])),
                    boxY * (0.7f * sinf(rb.f[1] * t + rb.ph[1]) + 0.3f * cosf(rb.f[4] * t * 1.3f + rb.ph[4])),
                    4.0f * sinf(rb.f[2] * t + rb.ph[2]) + 1.5f * cosf(rb.f[5] * t * 2.1f + rb.ph[5]));
    }

    Color RibbonColor(const Ribbon& rb, float along) const {
        switch (g_cfg.choice[0]) {
        case 1:  return Hsv(0.0f + 0.12f * (0.5f + 0.5f * sinf(rb.hue * 6.28f + along * 3)), 0.85f, 1);
        case 2:  return Hsv(0.5f + 0.15f * (0.5f + 0.5f * sinf(rb.hue * 6.28f + along * 3)), 0.8f, 1);
        case 3:  return Hsv(rb.hue + along * 0.3f, 0.35f, 1);
        case 4:  return Color(0.9f, 0.95f, 1);
        default: return Hsv(rb.hue + along * 0.4f, 0.9f, 1);
        }
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        Resize(w, h);
        ribbons.resize(2 + g_cfg.slider[1] * 14 / 100);
        speed = 0.4f + g_cfg.slider[0] / 100.0f * 2.0f;
        halfWidth = 0.15f + g_cfg.slider[2] / 100.0f * 0.9f;
        for (auto& rb : ribbons) Reset(rb);
        return true;
    }

    void Resize(int w, int h) override {
        width = w; height = h > 0 ? h : 1;
        float aspect = (float)width / height;
        boxX = aspect >= 1 ? 6 * aspect : 6;
        boxY = aspect >= 1 ? 6 : 6 / aspect;
    }

    void Frame(Renderer& r, float dt) override {
        orbit += dt * 4;
        verts.clear();
        for (auto& rb : ribbons) {
            rb.t += dt * speed;
            rb.life -= dt;
            if (rb.life < -3) Reset(rb);
            // Sample the path at a fixed rate in path-time so the ribbon's
            // shape doesn't depend on the frame rate.
            rb.sampleTimer += dt * speed;
            while (rb.sampleTimer >= kSample) {
                rb.sampleTimer -= kSample;
                rb.head = (rb.head + 1) % kTrail;
                Sample(rb, rb.t - rb.sampleTimer, rb.pos[rb.head], rb.side[rb.head]);
                if (rb.filled < kTrail) rb.filled++;
            }
            // Live tip at the exact current position, so the front of the
            // ribbon glides instead of advancing in sample-sized jumps.
            Vec3 tipPos, tipSide;
            Sample(rb, rb.t, tipPos, tipSide);
            const float frac = rb.sampleTimer / kSample;   // how far past the newest sample
            // Fade the whole ribbon in at birth and out at end of life.
            float vis = rb.life > 0 ? 1.0f : 1.0f + rb.life / 3;
            float age = 25 - rb.life;
            if (age < 2) vis *= age / 2;
            // Brightness by continuous age along the trail (including the
            // fraction of a sample since the last one), so the tail fades
            // smoothly instead of shifting a notch per sample.
            auto ageAlpha = [&](float k) { return fmaxf(0, 1.0f - (k + frac) / kTrail); };
            if (rb.filled > 0) {
                float a0 = 1.0f, a1 = ageAlpha(0);
                Color c0 = RibbonColor(rb, a0) * (a0 * vis * 0.8f), c1 = RibbonColor(rb, a1) * (a1 * vis * 0.8f);
                const Vec3& hp = rb.pos[rb.head]; const Vec3& hs = rb.side[rb.head];
                PushQuad(verts, tipPos + tipSide, hp + hs, hp - hs, tipPos - tipSide, Vec3(0, 0, 1), c0, c1, c1, c0);
            }
            for (int k = 0; k + 1 < rb.filled; k++) {
                int i0 = (rb.head - k + kTrail) % kTrail, i1 = (rb.head - k - 1 + kTrail) % kTrail;
                float a0 = ageAlpha((float)k), a1 = ageAlpha((float)(k + 1));
                Color c0 = RibbonColor(rb, a0) * (a0 * vis * 0.8f), c1 = RibbonColor(rb, a1) * (a1 * vis * 0.8f);
                PushQuad(verts, rb.pos[i0] + rb.side[i0], rb.pos[i1] + rb.side[i1],
                         rb.pos[i1] - rb.side[i1], rb.pos[i0] - rb.side[i0], Vec3(0, 0, 1), c0, c1, c1, c0);
            }
        }
        r.BeginFrame(true);
        float aspect = (float)width / height;
        float t = tanf(30.0f * kPi / 180);
        float dist = fmaxf(boxY / t, boxX / (t * aspect)) + 6;   // fit wide and tall screens
        Mat4 view = Mat4::Translate(0, 0, -dist) * Mat4::Rotate(sinf(orbit * 0.02f) * 25, 0, 1, 0);
        r.SetCamera(view, Mat4::Perspective(60.0f, aspect, 0.5f, dist + 20));
        DrawParams p;
        p.lit = false;
        p.depth = false;
        p.blend = BLEND_ADD;   // overlapping ribbons glow
        r.Draw(verts.data(), verts.size(), p);
    }
};

Scene* CreateScene() { return new RibbonsScene; }
