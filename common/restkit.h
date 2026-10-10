// restkit.h - full-screen scenes made safe for OLED panels.
//
// These savers may fill the whole screen with colour. Instead of keeping
// most pixels black, they protect the panel by making sure every pixel gets
// to rest and nothing stays lit and still:
//
//  1. Burn-in guard: a few times a second the GPU shrinks the frame to a
//     tiny (<= 64x64) image that the CPU reads back (without stalling).
//     For every region we track its average brightness and how much it
//     changes. A region that stays bright AND static for ~45 s is gently
//     dimmed in place until it starts changing again; if a large part of
//     the screen goes static, a full rest starts early.
//  2. Scheduled rest: every few minutes the scene fades to complete black,
//     rests, and fades back in as a new variation (new colours, camera,
//     layout), so pixels never repeat the same image.
//  3. Rolling rest band (optional): a soft dark band sweeps slowly across
//     the screen in changing directions, so every pixel gets regular dark
//     moments even mid-scene.
//  4. Pixel orbit: the whole image shifts a few pixels in a slow circle (as
//     OLED TVs do), so even hard edges never sit on exactly the same pixels.
#pragma once
#include "aerokit.h"

namespace rest {

// Shared settings rows. Savers use: choices {their palette, REST_BAND_CHOICE}
// and sliders {Speed, their own, REST_EVERY_SLIDER, REST_LENGTH_SLIDER}.
#define REST_BAND_CHOICE    { L"Rolling rest &band:", L"RestBand", 0, L"Off|Subtle|Strong" }
#define REST_SPEED_SLIDER   { L"Speed", L"Speed", 40, L"Slow", L"Fast" }
#define REST_EVERY_SLIDER   { L"Rest to black every (1 - 15 minutes)", L"RestEvery", 29, L"1 min", L"15 min" }
#define REST_LENGTH_SLIDER  { L"Rest length (5 - 60 seconds)", L"RestLength", 18, L"5 s", L"60 s" }

inline float Smooth(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return x * x * (3 - 2 * x); }

// Full-screen colour field computed on a coarse grid with colours blended
// between grid points (cheap even at 175 Hz on 4K). f(x, y) gets canvas
// pixel coordinates (origin centre, +y up).
template <class F>
inline void DrawField(Renderer& r, Canvas2D& c, int w, int h, float cellPx, F f) {
    int gx = (int)(w / cellPx) + 2, gy = (int)(h / cellPx) + 2;
    std::vector<Color> grid((size_t)gx * gy);
    for (int j = 0; j < gy; j++)
        for (int i = 0; i < gx; i++)
            grid[(size_t)j * gx + i] = f(-w * 0.5f + w * (float)i / (gx - 1), -h * 0.5f + h * (float)j / (gy - 1));
    c.Begin(w, h);
    for (int j = 0; j + 1 < gy; j++)
        for (int i = 0; i + 1 < gx; i++) {
            float x0 = -w * 0.5f + w * (float)i / (gx - 1), x1 = -w * 0.5f + w * (float)(i + 1) / (gx - 1);
            float y0 = -h * 0.5f + h * (float)j / (gy - 1), y1 = -h * 0.5f + h * (float)(j + 1) / (gy - 1);
            PushQuad(c.v, c.P(x0, y0), c.P(x1, y0), c.P(x1, y1), c.P(x0, y1), Vec3(0, 0, 1),
                     grid[(size_t)j * gx + i], grid[(size_t)j * gx + i + 1], grid[(size_t)(j + 1) * gx + i + 1], grid[(size_t)(j + 1) * gx + i]);
        }
    c.Draw(r, BLEND_OPAQUE);
}

// Height-field terrain as lit triangles. height(x, z) and colour(x, z, y).
// Smooth per-vertex normals unless `flat` (low-poly look).
template <class H, class C>
inline void BuildTerrain(std::vector<Vertex>& out, int nx, int nz, float x0, float z0, float step, H height, C colour, bool flat = false) {
    std::vector<Vec3> pts((size_t)(nx + 1) * (nz + 1)), nrm(pts.size());
    for (int j = 0; j <= nz; j++)
        for (int i = 0; i <= nx; i++) {
            float x = x0 + i * step, z = z0 + j * step;
            pts[(size_t)j * (nx + 1) + i] = Vec3(x, height(x, z), z);
            float e = step * 0.5f;
            nrm[(size_t)j * (nx + 1) + i] = Normalize(Vec3(height(x - e, z) - height(x + e, z), 2 * e, height(x, z - e) - height(x, z + e)));
        }
    for (int j = 0; j < nz; j++)
        for (int i = 0; i < nx; i++) {
            size_t a = (size_t)j * (nx + 1) + i, b = a + 1, c = a + nx + 1, d = c + 1;
            const Vec3* tri[2][3] = { { &pts[a], &pts[c], &pts[b] }, { &pts[b], &pts[c], &pts[d] } };
            const Vec3* nr[2][3] = { { &nrm[a], &nrm[c], &nrm[b] }, { &nrm[b], &nrm[c], &nrm[d] } };
            for (int k = 0; k < 2; k++) {
                Vec3 fn = Normalize(Cross(*tri[k][1] - *tri[k][0], *tri[k][2] - *tri[k][0]));
                if (fn.y < 0) fn = -fn;
                Vec3 centre = (*tri[k][0] + *tri[k][1] + *tri[k][2]) * (1.0f / 3);
                for (int v = 0; v < 3; v++) {
                    const Vec3& p = *tri[k][v];
                    Color col = flat ? colour(centre.x, centre.z, centre.y) : colour(p.x, p.z, p.y);
                    PushVertex(out, p, flat ? fn : *nr[k][v], 0, 0, col);
                }
            }
        }
}

// Direction from world space into view space (for SetLight).
inline Vec3 ToView(const Mat4& v, const Vec3& d) {
    return Normalize(Vec3(v.m[0][0] * d.x + v.m[0][1] * d.y + v.m[0][2] * d.z,
                          v.m[1][0] * d.x + v.m[1][1] * d.y + v.m[1][2] * d.z,
                          v.m[2][0] * d.x + v.m[2][1] * d.y + v.m[2][2] * d.z));
}

// World point -> canvas pixels (origin centre, +y up). False if behind the camera.
inline bool ToScreen(const Mat4& viewProj, const Vec3& p, int w, int h, float& sx, float& sy, float& depth) {
    const float(*m)[4] = viewProj.m;
    float cx = m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3];
    float cy = m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3];
    float cw = m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3];
    if (cw < 0.05f) return false;
    sx = cx / cw * w * 0.5f; sy = cy / cw * h * 0.5f; depth = cw;
    return true;
}

// Vertical field of view that keeps a given view of the short side on
// both landscape and portrait screens.
inline float FitFov(float fovShortDeg, int w, int h) {
    float aspect = (float)w / (h > 0 ? h : 1);
    if (aspect >= 1) return fovShortDeg;
    return 2 * atanf(tanf(fovShortDeg * kPi / 360) / aspect) * 180 / kPi;
}

// Base class for full-screen OLED-safe scenes: runs the rest cycle, the
// burn-in guard, the rolling band and the pixel orbit around the scene.
class RestScene : public Scene {
protected:
    int width = 1, height = 1;
    bool preview = false;
    float t = 0;            // scene time
    int variation = 0;      // increments after every rest

    virtual void Setup(Renderer& r) {}
    virtual void Reseed() {}                          // new look after each rest
    virtual void Draw(Renderer& r, float dt) = 0;     // draw the scene (frame already cleared to black)
    virtual void RestSettings(float& everySec, float& restSec, int& band) const = 0;
    // Scenes that build up their image over time (trails) keep the previous
    // frame instead of starting from black.
    virtual bool Persistent() const { return false; }

private:
    enum Phase { SHOW, FADE_OUT, RESTING, FADE_IN };
    Phase phase = FADE_IN;
    float phaseTime = 0;
    const float kFade = 2.5f;

    // Burn-in guard state, per grid cell.
    std::vector<float> lum, mean, change, staticTime, dim;
    int gw = 0, gh = 0;
    float sampleTimer = 0, lastSample = 0, hotTime = 0;
    Canvas2D overlay;
    bool burnInTest = false;   // registry "BurnInTest" = 1: draw a static block to see the guard work

    void UpdateGuard(Renderer& r, float dt) {
        sampleTimer -= dt;
        if (sampleTimer <= 0) { r.RequestLuminance(); sampleTimer = 0.2f; }
        int w = 0, h = 0;
        if (r.PollLuminance(lum, w, h)) {
            if (w != gw || h != gh) {
                gw = w; gh = h;
                mean.assign(lum.begin(), lum.end()); change.assign(lum.size(), 0.015f);
                staticTime.assign(lum.size(), 0); dim.assign(lum.size(), 0);
                lastSample = t;
                return;
            }
            float dts = fmaxf(0.05f, t - lastSample);
            lastSample = t;
            float k = 1 - expf(-dts / 20.0f);    // brightness: ~20 s average
            float kc = 1 - expf(-dts / 10.0f);   // change: ~10 s average
            for (size_t i = 0; i < lum.size(); i++) {
                float d = fabsf(lum[i] - mean[i]);
                mean[i] += (lum[i] - mean[i]) * k;
                change[i] += (d - change[i]) * kc;
                // Bright and barely changing = the kind of content that burns in.
                bool still = mean[i] > 0.22f && change[i] < 0.012f;
                staticTime[i] = still ? staticTime[i] + dts : fmaxf(0, staticTime[i] - 3 * dts);
            }
        }
        // Dim hot regions smoothly (in ~5 s), release slowly (~10 s).
        int hot = 0;
        for (size_t i = 0; i < dim.size(); i++) {
            float target = staticTime[i] > 45 ? 0.8f : 0;
            dim[i] += (target - dim[i]) * fminf(1, dt / (target > dim[i] ? 5.0f : 10.0f));
            hot += dim[i] > 0.4f;
        }
        // If a fifth of the screen has gone static for 20 s, rest early.
        hotTime = (!dim.empty() && hot > (int)dim.size() / 5) ? hotTime + dt : 0;
    }

    void DrawGuard(Renderer& r) {
        if (dim.empty()) return;
        overlay.Begin(width, height);
        // Corner values = average of the touching cells, so the dimming is
        // a smooth gradient rather than visible blocks.
        auto cellDim = [&](int x, int y) {
            x = x < 0 ? 0 : x >= gw ? gw - 1 : x; y = y < 0 ? 0 : y >= gh ? gh - 1 : y;
            return dim[(size_t)(gh - 1 - y) * gw + x];   // readback rows are top-down
        };
        auto corner = [&](int x, int y) { return 0.25f * (cellDim(x - 1, y - 1) + cellDim(x, y - 1) + cellDim(x - 1, y) + cellDim(x, y)); };
        bool any = false;
        for (float d : dim) if (d > 0.01f) { any = true; break; }
        if (!any) return;
        float cw = (float)width / gw, ch = (float)height / gh;
        for (int y = 0; y < gh; y++)
            for (int x = 0; x < gw; x++) {
                float a = corner(x, y), b = corner(x + 1, y), c = corner(x + 1, y + 1), d = corner(x, y + 1);
                if (a + b + c + d < 0.01f) continue;
                float x0 = -width * 0.5f + x * cw, y0 = -height * 0.5f + y * ch;
                PushQuad(overlay.v, overlay.P(x0, y0), overlay.P(x0 + cw, y0), overlay.P(x0 + cw, y0 + ch), overlay.P(x0, y0 + ch), Vec3(0, 0, 1),
                         Color(0, 0, 0, a), Color(0, 0, 0, b), Color(0, 0, 0, c), Color(0, 0, 0, d));
            }
        overlay.Draw(r, BLEND_ALPHA);
    }

    void DrawBand(Renderer& r, int band) {
        if (!band) return;
        float strength = band == 2 ? 0.7f : 0.35f;
        // Direction turns slowly; the band crosses the screen about every 40 s.
        float ang = t * 2 * kPi / 300;
        Vec3 n(cosf(ang), sinf(ang), 0), tg(-n.y, n.x, 0);
        float diag = sqrtf((float)width * width + (float)height * height);
        float pos = (fmodf(t / 40.0f, 1.0f) * 2 - 1) * diag * 0.75f;
        float bw = diag * 0.22f;
        overlay.Begin(width, height);
        // Soft bell-shaped profile across the band.
        float off[6] = { -bw, -bw * 0.55f, -bw * 0.2f, bw * 0.2f, bw * 0.55f, bw };
        float al[6] = { 0, strength * 0.45f, strength, strength, strength * 0.45f, 0 };
        for (int k = 0; k < 5; k++) {
            Vec3 a0 = n * (pos + off[k]) - tg * diag, a1 = n * (pos + off[k]) + tg * diag;
            Vec3 b0 = n * (pos + off[k + 1]) - tg * diag, b1 = n * (pos + off[k + 1]) + tg * diag;
            PushQuad(overlay.v, overlay.P(a0.x, a0.y), overlay.P(b0.x, b0.y), overlay.P(b1.x, b1.y), overlay.P(a1.x, a1.y), Vec3(0, 0, 1),
                     Color(0, 0, 0, al[k]), Color(0, 0, 0, al[k + 1]), Color(0, 0, 0, al[k + 1]), Color(0, 0, 0, al[k]));
        }
        overlay.Draw(r, BLEND_ALPHA);
    }

public:
    bool Init(Renderer& r, int w, int h, bool pv) override {
        width = w; height = h > 0 ? h : 1; preview = pv;
        burnInTest = RegReadDword(L"BurnInTest", 0) != 0;
        Setup(r);
        Reseed();
        phase = FADE_IN; phaseTime = 0;
        return true;
    }
    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        float every, restLen; int band;
        RestSettings(every, restLen, band);
        phaseTime += dt;
        if (preview) { phase = SHOW; band = 0; }
        switch (phase) {
        case SHOW:     if (!preview && (phaseTime > every || hotTime > 20)) { phase = FADE_OUT; phaseTime = 0; } break;
        case FADE_OUT: if (phaseTime > kFade) { phase = RESTING; phaseTime = 0; } break;
        case RESTING:
            if (phaseTime > restLen) {
                // Come back as a new variation, with the guard's history cleared.
                phase = FADE_IN; phaseTime = 0; variation++; hotTime = 0;
                Reseed();
                for (auto& s : staticTime) s = 0;
                for (auto& d : dim) d = 0;
            }
            break;
        case FADE_IN:  if (phaseTime > kFade) { phase = SHOW; phaseTime = 0; } break;
        }

        // Pixel orbit: a few pixels round a slow circle (3 px at 1080p).
        float rad = 3.0f * (float)(width < height ? width : height) / 1080.0f;
        r.SetPixelShift(cosf(t * 2 * kPi / 180) * rad, sinf(t * 2 * kPi / 180) * rad, 1 + 2.5f * rad / (width < height ? width : height));

        if (phase == RESTING) { r.BeginFrame(true); return; }   // every pixel fully off
        r.BeginFrame(!Persistent());
        t += dt;
        Draw(r, dt);
        if (burnInTest) {   // deliberately static, bright content
            Canvas2D tb; tb.Begin(width, height);
            tb.Rect(-width * 0.3f, height * 0.1f, -width * 0.05f, height * 0.35f, Color(1, 1, 1));
            tb.Draw(r, BLEND_OPAQUE);
        }
        if (!preview) UpdateGuard(r, dt);
        // Overlays go on top of the finished frame, never into a kept image.
        r.BeginOverlay();
        DrawGuard(r);
        DrawBand(r, band);
        float fade = phase == FADE_OUT ? Smooth(phaseTime / kFade) : phase == FADE_IN ? 1 - Smooth(phaseTime / kFade) : 0;
        if (fade > 0) r.FullscreenQuad(0, 0, 0, fade);
    }
};

}  // namespace rest
