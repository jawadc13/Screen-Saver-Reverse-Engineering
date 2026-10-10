// plasma.cpp - "Plasma": the classic demoscene color plasma, a field of
// overlapping sine waves mapped through a color palette.
#include "../../common/scenekit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"Plasma Settings",
    1, {
        { L"&Palette:", L"Palette", 0, L"Psychedelic|Fire|Ocean|Aurora|Grayscale" },
    },
    2, {
        { L"Speed", L"Speed", 40, L"Slow", L"Fast" },
        { L"Pattern size", L"Scale", 50, L"Fine", L"Large" },
    },
};

const wchar_t* RegistryName() { return L"Plasma"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

class PlasmaScene : public Scene {
    std::vector<Vertex> verts;
    std::vector<Color> grid;
    int width = 1, height = 1, gx = 2, gy = 2;
    float t = 0, speed = 1, scale = 1;
    Color lut[256];

    void BuildPalette() {
        for (int i = 0; i < 256; i++) {
            float x = i / 255.0f;
            switch (g_cfg.choice[0]) {
            case 1: lut[i] = Color(fminf(1, x * 2.2f), fmaxf(0, fminf(1, x * 2 - 0.6f)), fmaxf(0, x * 3 - 2.2f)); break;
            case 2: lut[i] = Hsv(0.5f + 0.15f * sinf(x * 6.28f), 0.85f, 0.25f + 0.75f * x); break;
            case 3: lut[i] = Hsv(0.33f + 0.25f * sinf(x * 6.28f * 1.5f), 0.8f, 0.15f + 0.85f * x * x); break;
            case 4: lut[i] = Color(x, x, x); break;
            default: lut[i] = Hsv(x, 0.9f, 1); break;
            }
        }
    }

    void Layout() {
        // The plasma is smooth, so a grid of ~45 cells across the short side,
        // with colors interpolated between points, looks identical to
        // per-pixel at a fraction of the cost - even at 175 Hz.
        float cellPx = (float)(width < height ? width : height) / 45.0f;
        if (cellPx < 8) cellPx = 8;
        gx = (int)(width / cellPx) + 2;
        gy = (int)(height / cellPx) + 2;
        grid.resize((size_t)gx * gy);
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        speed = 0.2f + g_cfg.slider[0] / 100.0f * 1.6f;
        scale = 0.4f + g_cfg.slider[1] / 100.0f * 2.0f;
        BuildPalette();
        Layout();
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; Layout(); }

    void Frame(Renderer& r, float dt) override {
        t += dt * speed;
        // Coordinates in "pattern units" relative to the short screen side,
        // so the pattern keeps its proportions on ultrawide and portrait.
        float unit = (float)(width < height ? width : height) * 0.25f * scale;
        float cx = 1.5f * sinf(t * 0.31f), cy = 1.5f * cosf(t * 0.23f);
        for (int j = 0; j < gy; j++) {
            for (int i = 0; i < gx; i++) {
                float x = ((float)i / (gx - 1) - 0.5f) * width / unit;
                float y = ((float)j / (gy - 1) - 0.5f) * height / unit;
                float v = sinf(x + t)
                        + sinf((y + t) * 0.7f)
                        + sinf((x + y + t) * 0.6f)
                        + sinf(sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) * 1.4f - t * 1.3f);
                float k = (v + 4) / 8;                         // 0..1
                k = k * 2 + t * 0.05f;                         // cycle the palette slowly
                int idx = (int)((k - floorf(k)) * 255);
                grid[(size_t)j * gx + i] = lut[idx];
            }
        }
        verts.clear();
        for (int j = 0; j + 1 < gy; j++) {
            float y0 = (float)j / (gy - 1) * 2 - 1, y1 = (float)(j + 1) / (gy - 1) * 2 - 1;
            for (int i = 0; i + 1 < gx; i++) {
                float x0 = (float)i / (gx - 1) * 2 - 1, x1 = (float)(i + 1) / (gx - 1) * 2 - 1;
                PushQuad(verts, Vec3(x0, y0, 0.5f), Vec3(x1, y0, 0.5f), Vec3(x1, y1, 0.5f), Vec3(x0, y1, 0.5f), Vec3(0, 0, 1),
                         grid[(size_t)j * gx + i], grid[(size_t)j * gx + i + 1],
                         grid[(size_t)(j + 1) * gx + i + 1], grid[(size_t)(j + 1) * gx + i]);
            }
        }
        r.BeginFrame(false);
        r.SetCamera(Mat4::Identity(), Mat4::Identity());
        DrawParams p;
        p.lit = false;
        p.depth = false;
        r.Draw(verts.data(), verts.size(), p);
    }
};

Scene* CreateScene() { return new PlasmaScene; }
