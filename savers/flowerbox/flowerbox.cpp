// flowerbox.cpp - "3D Flower Box": a spinning cube that morphs into a sphere
// and then blossoms into a spiky flower and back (after XP's ssflwbox.scr,
// another D3DSaver screensaver from the same family as 3D Pipes).
#include "../../common/scenekit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"3D Flower Box Settings",
    2, {
        { L"&Colors:", L"Colors", 0, L"Per face|Rainbow|Chrome|Cycling" },
        { L"&Shape:", L"Shape", 0, L"Flower|Star|Blob" },
    },
    3, {
        { L"Spin speed", L"Speed", 50, L"Slow", L"Fast" },
        { L"Morph speed", L"Morph", 50, L"Slow", L"Fast" },
        { L"Size", L"Size", 60, L"Small", L"Large" },
    },
};

const wchar_t* RegistryName() { return L"FlowerBox"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

class FlowerBoxScene : public Scene {
    std::vector<Vertex> verts;
    std::vector<Vec3> pts;   // morphed grid points of one face
    int width = 1, height = 1, n = 24;
    float t = 0, angle = 0, morph = 0;
    Vec3 axis;

    // Point on the shape: blend cube -> sphere -> flower depending on m.
    Vec3 Shape(const Vec3& cube, float m) const {
        Vec3 s = Normalize(cube);
        if (m <= 1) return cube * (1 - m) + s * m;           // cube -> sphere
        float k = m - 1;                                      // 0..1 sphere -> flower
        float th = atan2f(s.z, s.x), ph = acosf(fmaxf(-1, fminf(1, s.y)));
        float bump;
        switch (g_cfg.choice[1]) {
        case 1:  bump = 0.6f * powf(fabsf(sinf(4 * th) * sinf(4 * ph)), 3); break;          // star spikes
        case 2:  bump = 0.25f * sinf(3 * th + t) * sinf(2 * ph + t * 0.7f); break;          // wobbly blob
        default: bump = 0.45f * powf(fabsf(cosf(3 * th) * sinf(3 * ph)), 2); break;         // flower petals
        }
        return s * (1 + bump * k);
    }

    Color FaceColor(int face, const Vec3& p) const {
        static const Color faces[6] = { {1,0.2f,0.2f}, {0.2f,1,0.3f}, {0.3f,0.4f,1}, {1,1,0.25f}, {1,0.3f,1}, {0.2f,1,1} };
        switch (g_cfg.choice[0]) {
        case 1:  return Hsv(0.5f + 0.5f * atan2f(p.z, p.x) / kPi + t * 0.05f, 0.85f, 1);
        case 2:  return Color(0.75f, 0.78f, 0.82f);
        case 3:  return Hsv(face / 6.0f + t * 0.1f, 0.8f, 1);
        default: return faces[face];
        }
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        if (preview) n = 12;
        axis = Normalize(Vec3(RandF(-1, 1), 1, RandF(-1, 1)));
        morph = RandF(0, 2);   // start somewhere in the cube/sphere/flower cycle
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        t += dt;
        angle += dt * (10 + g_cfg.slider[0] * 0.8f);
        morph += dt * (0.05f + g_cfg.slider[1] / 100.0f * 0.35f);
        // 0 -> 2 -> 0 ping-pong, eased so it lingers on each shape.
        float ph = fmodf(morph, 2.0f);
        float m = ph < 1 ? ph : 2 - ph;
        m = m * m * (3 - 2 * m) * 2;

        verts.clear();
        pts.resize((size_t)(n + 1) * (n + 1));
        for (int face = 0; face < 6; face++) {
            int ax = face / 2;
            float sign = face % 2 ? -1.0f : 1.0f;
            for (int j = 0; j <= n; j++)
                for (int i = 0; i <= n; i++) {
                    float a = (float)i / n * 2 - 1, b = (float)j / n * 2 - 1;
                    float c[3];
                    c[ax] = sign; c[(ax + 1) % 3] = a * sign; c[(ax + 2) % 3] = b;
                    pts[(size_t)j * (n + 1) + i] = Shape(Vec3(c[0], c[1], c[2]), m);
                }
            for (int j = 0; j < n; j++)
                for (int i = 0; i < n; i++) {
                    const Vec3& p00 = pts[(size_t)j * (n + 1) + i];
                    const Vec3& p10 = pts[(size_t)j * (n + 1) + i + 1];
                    const Vec3& p11 = pts[(size_t)(j + 1) * (n + 1) + i + 1];
                    const Vec3& p01 = pts[(size_t)(j + 1) * (n + 1) + i];
                    // Flat facet normal, pointing outwards.
                    Vec3 nrm = Normalize(Cross(p10 - p00, p01 - p00));
                    if (Dot(nrm, p00) < 0) nrm = -nrm;
                    Color col = FaceColor(face, p00);
                    PushQuad(verts, p00, p10, p11, p01, nrm, col, col, col, col);
                }
        }

        r.BeginFrame(true);
        float aspect = (float)width / height;
        float size = 0.6f + g_cfg.slider[2] / 100.0f * 0.8f;
        // Keep the box the same apparent size on wide and tall screens.
        float fitDist = 2.4f / tanf(22.5f * kPi / 180) / (aspect < 1 ? aspect : 1);
        r.SetCamera(Mat4::Translate(0, 0, -fitDist / size), Mat4::Perspective(45.0f, aspect, 0.5f, 50));
        r.SetLight(0, Vec3(-0.5f, 0.7f, 1.0f), 1, 1, 1);
        r.SetLight(1, Vec3(0.6f, -0.4f, 0.4f), 0.3f, 0.3f, 0.45f);
        r.SetAmbient(0.15f, 0.15f, 0.15f);
        DrawParams p;
        p.world = Mat4::Rotate(angle, axis.x, axis.y, axis.z) * Mat4::Rotate(angle * 0.37f, 1, 0, 0);
        p.specular[0] = p.specular[1] = p.specular[2] = g_cfg.choice[0] == 2 ? 1.0f : 0.6f;
        p.shininess = g_cfg.choice[0] == 2 ? 90.0f : 40.0f;
        r.Draw(verts.data(), verts.size(), p);
    }
};

Scene* CreateScene() { return new FlowerBoxScene; }
