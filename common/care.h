// care.h - OLED screen care shared by every saver (see restkit.h for the
// full description):
//   * BurnGuard: finds regions that stay bright and static (from a tiny GPU
//     luminance snapshot read back without stalling) and dims them in place.
//   * Care: the framework-level rest cycle (fade to black, rest, fade back),
//     pixel orbit and guard, wrapped around any scene. Settings come from
//     the saver's style key (theme.h: RestEvery, RestLength, Orbit, Guard).
#pragma once
#include "scenekit.h"
#include <math.h>

namespace care {

inline float Smooth(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return x * x * (3 - 2 * x); }

struct BurnGuard {
    std::vector<float> lum, mean, change, staticTime, dim;
    int gw = 0, gh = 0;
    float t = 0, sampleTimer = 0, lastSample = 0, hotTime = 0;
    Canvas2D overlay;

    // Seconds a fifth or more of the screen has been static (rest early).
    float HotTime() const { return hotTime; }

    void Reset() {
        hotTime = 0;
        for (auto& s : staticTime) s = 0;
        for (auto& d : dim) d = 0;
    }

    void Update(Renderer& r, float dt) {
        t += dt;
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
        hotTime = (!dim.empty() && hot > (int)dim.size() / 5) ? hotTime + dt : 0;
    }

    // Call after BeginOverlay.
    void Draw(Renderer& r, int width, int height) {
        if (dim.empty()) return;
        bool any = false;
        for (float d : dim) if (d > 0.01f) { any = true; break; }
        if (!any) return;
        overlay.Begin(width, height);
        // Corner values = average of the touching cells, so the dimming is
        // a smooth gradient rather than visible blocks.
        auto cellDim = [&](int x, int y) {
            x = x < 0 ? 0 : x >= gw ? gw - 1 : x; y = y < 0 ? 0 : y >= gh ? gh - 1 : y;
            return dim[(size_t)(gh - 1 - y) * gw + x];   // readback rows are top-down
        };
        auto corner = [&](int x, int y) { return 0.25f * (cellDim(x - 1, y - 1) + cellDim(x, y - 1) + cellDim(x - 1, y) + cellDim(x, y)); };
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
};

// Pixel orbit: a few pixels round a slow circle (3 px at 1080p, every 3 min).
inline void Orbit(Renderer& r, float t, int width, int height) {
    int s = width < height ? width : height;
    float rad = 3.0f * s / 1080.0f;
    const float k = 2 * 3.14159265f / 180;
    r.SetPixelShift(cosf(t * k) * rad, sinf(t * k) * rad, 1 + 2.5f * rad / (s > 0 ? s : 1));
}

// Framework rest cycle around any scene.
struct Care {
    enum Phase { SHOW, FADE_OUT, RESTING, FADE_IN };
    Phase phase = SHOW;
    float phaseTime = 0, t = 0;
    BurnGuard guard;
    float kFade = 5;   // seconds, from the style's FadeSeconds

    // Before the scene: returns false while resting (skip the scene; the
    // screen is black), true when the scene should draw this frame.
    bool Before(Renderer& r, float dt, int width, int height, float everySec, float restSec, float fadeSec, bool orbit, bool guardOn, bool preview) {
        kFade = fadeSec > 0.1f ? fadeSec : 0.1f;
        t += dt;
        phaseTime += dt;
        if (preview) phase = SHOW;
        switch (phase) {
        case SHOW:
            if (!preview && ((everySec > 0 && phaseTime > everySec) || (guardOn && guard.HotTime() > 20))) { phase = FADE_OUT; phaseTime = 0; }
            break;
        case FADE_OUT: if (phaseTime > kFade) { phase = RESTING; phaseTime = 0; } break;
        case RESTING:  if (phaseTime > restSec) { phase = FADE_IN; phaseTime = 0; guard.Reset(); } break;
        case FADE_IN:  if (phaseTime > kFade) { phase = SHOW; phaseTime = 0; } break;
        }
        if (orbit) Orbit(r, t, width, height);
        else r.SetPixelShift(0, 0, 1);
        if (phase == RESTING) {
            // Every pixel fully off; the scene's own image is left untouched.
            r.BeginFrame(false, false);   // keeps the scene's image; only the screen goes black
            r.BeginOverlay();
            r.FullscreenQuad(0, 0, 0, 1, BLEND_OPAQUE);
            return false;
        }
        return true;
    }

    // After the scene has drawn its frame.
    void After(Renderer& r, float dt, int width, int height, bool guardOn, bool preview) {
        if (guardOn && !preview) guard.Update(r, dt);
        float fade = phase == FADE_OUT ? Smooth(phaseTime / kFade) : phase == FADE_IN ? 1 - Smooth(phaseTime / kFade) : 0;
        if (!(guardOn && !preview) && fade <= 0) return;
        r.BeginOverlay();
        if (guardOn && !preview) guard.Draw(r, width, height);
        if (fade > 0) r.FullscreenQuad(0, 0, 0, fade);
    }
};

}  // namespace care
