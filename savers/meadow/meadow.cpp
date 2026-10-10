// meadow.cpp - "Aero Sky Meadow": glossy rolling green hills under a bright
// sky, fluffy clouds drifting by, slowly turning sun rays and a lens flare.
#include "../../common/aerokit.h"
#include "resource.h"

static SimpleConfig g_cfg = {
    L"Aero Sky Meadow Settings",
    2, {
        { L"&Time of day:", L"TimeOfDay", 1, L"Morning|Midday|Golden hour|Dusk" },
        { L"&Lens flare:", L"Flare", 1, L"Off|On" },
    },
    2, {
        { L"Wind speed", L"Speed", 35, L"Calm", L"Breezy" },
        { L"Clouds", L"Clouds", 50, L"Clear", L"Cloudy" },
    },
};

const wchar_t* RegistryName() { return L"AeroMeadow"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

struct CloudPuff { float x, y, w, speed, alpha; int tex; };
struct Hill { float base, amp[3], freq[3], phase[3]; Color top, bottom; };

class MeadowScene : public Scene {
    Canvas2D sky, rays, sunGlow, clouds[3], hills, flare;
    Texture *dot = nullptr, *cloudTex[3] = {}, *bokeh = nullptr;
    std::vector<CloudPuff> puffs;
    std::vector<Hill> hillLayers;
    int width = 1, height = 1;
    float t = 0, wind = 1;
    float sunX = 0, sunY = 0;

    struct Look { Color skyTop, skyMid, skyLow, sun, hillTint; float sunHeight; };
    Look GetLook() const {
        switch (g_cfg.choice[0]) {
        case 0:  return { {0.30f, 0.60f, 0.95f}, {0.60f, 0.82f, 1.0f}, {0.95f, 0.95f, 0.88f}, {1, 0.97f, 0.85f}, {1, 1, 1}, 0.18f };
        case 2:  return { {0.25f, 0.45f, 0.85f}, {0.85f, 0.75f, 0.70f}, {1.0f, 0.75f, 0.40f}, {1, 0.80f, 0.45f}, {1.05f, 0.9f, 0.6f}, 0.05f };
        case 3:  return { {0.08f, 0.10f, 0.35f}, {0.45f, 0.30f, 0.55f}, {0.95f, 0.55f, 0.40f}, {1, 0.65f, 0.45f}, {0.6f, 0.55f, 0.75f}, -0.02f };
        default: return { {0.10f, 0.45f, 0.95f}, {0.45f, 0.75f, 1.0f}, {0.85f, 0.95f, 1.0f}, {1, 1, 0.95f}, {1, 1, 1}, 0.32f };
        }
    }

    void SpawnCloud(CloudPuff& c, bool anywhere) {
        float m = (float)(width < height ? width : height);
        int layer = RandI(0, 2);            // 0 = far (small, slow), 2 = near
        c.w = m * (0.18f + 0.14f * layer) * RandF(0.8f, 1.4f);
        c.speed = m * (0.01f + 0.012f * layer) * RandF(0.8f, 1.2f);
        c.x = anywhere ? RandF(-0.6f, 0.6f) * width : -width * 0.5f - c.w;
        // Clouds live in the sky above the hills.
        c.y = height * RandF(0.0f, 0.45f) - layer * height * 0.04f;
        c.alpha = 0.75f + 0.08f * layer;
        c.tex = RandI(0, 2);
    }

    float HillY(const Hill& h, float x) const {
        float m = (float)(width < height ? width : height);
        float u = x / m;
        float y = h.base;
        for (int i = 0; i < 3; i++) y += h.amp[i] * sinf(u * h.freq[i] + h.phase[i]);
        return -height * 0.5f + height * y;
    }

public:
    ~MeadowScene() { delete dot; delete bokeh; for (auto* c : cloudTex) delete c; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        dot = aero::SoftDot(r);
        bokeh = aero::Bokeh(r);
        for (int i = 0; i < 3; i++) cloudTex[i] = aero::Cloud(r, (unsigned)RandI(1, 100000));
        wind = 0.3f + g_cfg.slider[0] / 100.0f * 3.0f;
        puffs.resize(g_cfg.slider[1] * 18 / 100);
        for (auto& c : puffs) SpawnCloud(c, true);
        // Three hill layers, far (pale, higher) to near (rich green, glossy).
        Color tops[3] = { {0.55f, 0.80f, 0.45f}, {0.40f, 0.78f, 0.20f}, {0.35f, 0.82f, 0.10f} };
        Color bots[3] = { {0.35f, 0.62f, 0.35f}, {0.18f, 0.52f, 0.10f}, {0.10f, 0.40f, 0.05f} };
        float bases[3] = { 0.36f, 0.27f, 0.16f };
        for (int i = 0; i < 3; i++) {
            Hill hl;
            hl.base = bases[i];
            for (int k = 0; k < 3; k++) {
                hl.amp[k] = RandF(0.015f, 0.05f) / (k + 1);
                hl.freq[k] = RandF(0.8f, 2.0f) * (k + 1);
                hl.phase[k] = RandF(0, 6.28f);
            }
            hl.top = tops[i]; hl.bottom = bots[i];
            hillLayers.push_back(hl);
        }
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        t += dt;
        Look lk = GetLook();
        float m = (float)(width < height ? width : height);
        sunX = -width * 0.22f;
        sunY = height * lk.sunHeight + height * 0.12f;

        sky.Begin(width, height);
        Color s[4] = { lk.skyTop, lk.skyMid, lk.skyLow, lk.skyLow };
        aero::VerticalGradient(sky, width, height, s, 4);

        // Slowly turning rays: thin triangles fanning out from the sun.
        rays.Begin(width, height);
        float reach = (float)(width + height);
        for (int i = 0; i < 14; i++) {
            float a = t * 0.02f + i * (2 * kPi / 14);
            float spread = 0.06f + 0.03f * sinf(i * 1.7f);
            Color c0 = lk.sun.WithAlpha(0.16f), c1 = lk.sun.WithAlpha(0);
            Vec3 p0 = rays.P(sunX, sunY);
            Vec3 p1 = rays.P(sunX + cosf(a - spread) * reach, sunY + sinf(a - spread) * reach);
            Vec3 p2 = rays.P(sunX + cosf(a + spread) * reach, sunY + sinf(a + spread) * reach);
            PushVertex(rays.v, p0, Vec3(0, 0, 1), 0, 0, c0);
            PushVertex(rays.v, p1, Vec3(0, 0, 1), 0, 0, c1);
            PushVertex(rays.v, p2, Vec3(0, 0, 1), 0, 0, c1);
        }
        sunGlow.Begin(width, height);
        sunGlow.Dot(sunX, sunY, m * 0.55f, lk.sun.WithAlpha(0.55f));
        sunGlow.Dot(sunX, sunY, m * 0.12f, Color(1, 1, 1, 1));

        for (auto& c : clouds) c.Begin(width, height);
        for (auto& p : puffs) {
            p.x += p.speed * wind * dt;
            if (p.x - p.w > width * 0.5f) SpawnCloud(p, false);
            Color tint = Lerp(Color(1, 1, 1), lk.sun, 0.25f).WithAlpha(p.alpha);
            clouds[p.tex].Rect(p.x - p.w, p.y - p.w * 0.5f, p.x + p.w, p.y + p.w * 0.5f, tint, 0, 0.1f, 1, 0.9f);
        }

        // Hills as filled strips from their silhouette down to the bottom
        // edge, with a glossy highlight band along each crest.
        // Hill fill and its crest highlight go into one alpha-blended layer,
        // hill by hill, so a far hill's shine never paints over a nearer one.
        hills.Begin(width, height);
        const int seg = 120;
        float left = -width * 0.5f, bottom = -height * 0.5f;
        for (auto& hl : hillLayers) {
            Color top = Color(hl.top.r * lk.hillTint.r, hl.top.g * lk.hillTint.g, hl.top.b * lk.hillTint.b);
            Color bot = Color(hl.bottom.r * lk.hillTint.r, hl.bottom.g * lk.hillTint.g, hl.bottom.b * lk.hillTint.b);
            struct SheenQuad { float x0, y0, x1, y1, g; };
            std::vector<SheenQuad> sheenQuads;
            for (int i = 0; i < seg; i++) {
                float x0 = left + width * (float)i / seg, x1 = left + width * (float)(i + 1) / seg;
                float y0 = HillY(hl, x0), y1 = HillY(hl, x1);
                PushQuad(hills.v, hills.P(x0, bottom), hills.P(x1, bottom), hills.P(x1, y1), hills.P(x0, y0), Vec3(0, 0, 1), bot, bot, top, top);
                float g = m * 0.025f;
                sheenQuads.push_back({ x0, y0, x1, y1, g });
            }
            Color hiA(1, 1, 1, 0.35f), hiB(1, 1, 1, 0);
            for (auto& q : sheenQuads)
                PushQuad(hills.v, hills.P(q.x0, q.y0 - q.g), hills.P(q.x1, q.y1 - q.g), hills.P(q.x1, q.y1), hills.P(q.x0, q.y0),
                         Vec3(0, 0, 1), hiB, hiB, hiA, hiA);
        }

        // Lens flare: ghosts along the line from the sun through the centre.
        flare.Begin(width, height);
        if (g_cfg.choice[1]) {
            const float pos[6] = { 0.35f, 0.6f, 0.9f, 1.15f, 1.45f, 1.8f };
            const float size[6] = { 0.05f, 0.09f, 0.03f, 0.12f, 0.06f, 0.16f };
            const Color cols[6] = { {0.6f, 0.9f, 1, 0.20f}, {0.6f, 1, 0.7f, 0.12f}, {1, 1, 1, 0.25f}, {0.5f, 0.7f, 1, 0.10f}, {1, 0.8f, 0.6f, 0.14f}, {0.6f, 1, 0.9f, 0.07f} };
            for (int i = 0; i < 6; i++) {
                float fx = sunX * (1 - pos[i] * 2), fy = sunY * (1 - pos[i] * 2);
                flare.Dot(fx, fy, m * size[i], cols[i]);
            }
        }

        r.BeginFrame(true);
        sky.Draw(r, BLEND_OPAQUE);
        rays.Draw(r, BLEND_ADD);
        sunGlow.Draw(r, BLEND_ADD, dot);
        for (int i = 0; i < 3; i++) clouds[i].Draw(r, BLEND_ALPHA, cloudTex[i]);
        hills.Draw(r, BLEND_ALPHA);
        flare.Draw(r, BLEND_ADD, bokeh);
    }
};

Scene* CreateScene() { return new MeadowScene; }
