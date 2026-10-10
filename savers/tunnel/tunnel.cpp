// tunnel.cpp - "3D Tunnel": flying down an endless, winding tube.
#include "../../common/scenekit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"3D Tunnel Settings",
    2, {
        { L"&Style:", L"Style", 0, L"Checkerboard|Neon rings|Stripes|Hex plates" },
        { L"&Colors:", L"Colors", 0, L"Rainbow|Ocean|Lava|Monochrome" },
    },
    2, {
        { L"Speed", L"Speed", 50, L"Slow", L"Fast" },
        { L"Twistiness", L"Twist", 50, L"Straight", L"Winding" },
    },
};

const wchar_t* RegistryName() { return L"Tunnel"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

class TunnelScene : public Scene {
    std::vector<Vertex> verts;
    int width = 1, height = 1;
    float z = 0, speed = 10, twist = 1, roll = 0, hueShift = 0;
    float f1, f2, f3, p1, p2;
    int sides = 32;
    static constexpr float kRadius = 3.0f, kStep = 0.5f, kAhead = 70.0f;

    // Centre line of the tube: a smooth curve made of a few sines.
    Vec3 Center(float t) const {
        return Vec3(twist * (6 * sinf(t * f1 + p1) + 2.5f * sinf(t * f3)),
                    twist * (5 * cosf(t * f2 + p2) + 2 * sinf(t * f3 * 1.3f)), -t);
    }

    Color Palette(float x) const {
        switch (g_cfg.choice[1]) {
        case 1:  return Hsv(0.5f + 0.12f * sinf(x * 6.28f), 0.8f, 1);
        case 2:  return Hsv(0.0f + 0.1f * (0.5f + 0.5f * sinf(x * 6.28f)), 0.95f, 1);
        case 3:  { float v = 0.6f + 0.4f * sinf(x * 6.28f); return Color(v, v, v); }
        default: return Hsv(x, 0.85f, 1);
        }
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        speed = 4 + g_cfg.slider[0] / 100.0f * 26;
        twist = g_cfg.slider[1] / 50.0f;
        f1 = RandF(0.02f, 0.035f); f2 = RandF(0.018f, 0.03f); f3 = RandF(0.05f, 0.08f);
        p1 = RandF(0, 6.28f); p2 = RandF(0, 6.28f);
        if (preview) sides = 16;
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        z += speed * dt;
        roll += dt * 10 * twist;
        hueShift += dt * 0.05f;

        Vec3 eye = Center(z), target = Center(z + 4);
        Vec3 up(sinf(roll * kPi / 180), cosf(roll * kPi / 180), 0);
        r.BeginFrame(true);
        float aspect = (float)width / height;
        // Wider field of view on tall (portrait) screens so the tube fills them.
        float fov = aspect < 1 ? 90.0f : 70.0f;
        r.SetCamera(Mat4::LookAt(eye, target, up), Mat4::Perspective(fov, aspect, 0.1f, kAhead + 5));

        // Build rings from just behind the camera to the far distance.
        verts.clear();
        float t0 = floorf(z / kStep) * kStep - kStep;
        int style = (int)g_cfg.choice[0];
        std::vector<Vec3> ringA(sides + 1), ringB(sides + 1);
        std::vector<Vec3> nA(sides + 1), nB(sides + 1);
        auto makeRing = [&](float t, std::vector<Vec3>& ring, std::vector<Vec3>& nrm) {
            Vec3 c = Center(t), fwd = Normalize(Center(t + 0.1f) - c);
            Vec3 side = Normalize(Cross(fwd, Vec3(0, 1, 0))), upv = Cross(side, fwd);
            for (int i = 0; i <= sides; i++) {
                float a = 2 * kPi * i / sides;
                Vec3 out = side * cosf(a) + upv * sinf(a);
                ring[i] = c + out * kRadius;
                nrm[i] = -out;
            }
        };
        makeRing(t0, ringA, nA);
        for (float t = t0; t < z + kAhead; t += kStep) {
            makeRing(t + kStep, ringB, nB);
            int ringIndex = (int)floorf(t / kStep + 0.5f);
            float dist = t - z;
            float fog = 1.0f - dist / kAhead;            // fade into darkness
            fog = fog < 0 ? 0 : fog * fog;
            for (int i = 0; i < sides; i++) {
                float b;
                Color base;
                switch (style) {
                case 1:   // neon rings: dark walls, every 8th ring glows
                    b = (ringIndex % 8 == 0) ? 1.0f : 0.06f;
                    base = Palette(hueShift + ringIndex * 0.01f);
                    break;
                case 2:   // spiral stripes
                    b = ((i + ringIndex / 2) % 8 < 4) ? 0.9f : 0.25f;
                    base = Palette(hueShift + i / (float)sides);
                    break;
                case 3:   // hex-ish plates with dark seams
                    b = ((i % 4 == 0) || (ringIndex % 4 == 0)) ? 0.08f : 0.7f + 0.3f * ((i / 4 + ringIndex / 4) % 2);
                    base = Palette(hueShift + ((i / 4) * 7 + (ringIndex / 4) * 3) % 16 / 16.0f);
                    break;
                default:  // checkerboard
                    b = ((i / 2 + ringIndex) % 2) ? 1.0f : 0.35f;
                    base = Palette(hueShift + ringIndex * 0.004f);
                    break;
                }
                // Fake lighting: brighter on the "ceiling" and towards the camera.
                float shade = 0.65f + 0.35f * sinf(2 * kPi * (i + 0.5f) / sides);
                Color c = base * (b * shade * fog);
                PushQuad(verts, ringA[i], ringA[i + 1], ringB[i + 1], ringB[i], nA[i], c, c, c, c);
            }
            ringA.swap(ringB); nA.swap(nB);
        }
        DrawParams p;
        p.lit = false;
        r.Draw(verts.data(), verts.size(), p);
    }
};

Scene* CreateScene() { return new TunnelScene; }
