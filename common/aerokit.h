// aerokit.h - building blocks for the "Frutiger Aero" screensavers: soft
// procedural textures (glow, bokeh, glass orb, gloss, cloud), gradient
// skies and rotated sprites. Everything is generated at startup, so the
// savers need no image files and look sharp at any resolution.
#pragma once
#include "scenekit.h"

namespace aero {

inline float Smooth(float e0, float e1, float x) {
    float t = (x - e0) / (e1 - e0);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3 - 2 * t);
}

inline unsigned Pack(float r, float g, float b, float a) {
    return ((unsigned)ToByte(a) << 24) | ((unsigned)ToByte(r) << 16) | ((unsigned)ToByte(g) << 8) | ToByte(b);
}

// Fills a size x size texture from f(x, y) with x, y in [-1, 1] (y down).
template <class F>
inline Texture* MakeTexture(Renderer& r, int size, F f) {
    std::vector<unsigned> px((size_t)size * size);
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) {
            float x = (i + 0.5f) / size * 2 - 1, y = (j + 0.5f) / size * 2 - 1;
            px[(size_t)j * size + i] = f(x, y);
        }
    return r.CreateTexture(px.data(), size, size);
}

// Soft round glow: bright centre fading smoothly to nothing.
inline unsigned SoftDotPx(float x, float y);
inline Texture* SoftDot(Renderer& r) { return MakeTexture(r, 128, SoftDotPx); }

// Out-of-focus "bokeh" disc: flat fill, slightly brighter rim, soft edge.
inline Texture* Bokeh(Renderer& r) {
    return MakeTexture(r, 128, [](float x, float y) {
        float d = sqrtf(x * x + y * y);
        float edge = 1 - Smooth(0.86f, 0.96f, d);
        float a = (0.45f + 0.45f * Smooth(0.6f, 0.9f, d)) * edge;
        return Pack(1, 1, 1, a);
    });
}

// Glossy glass orb body (tinted by vertex color): clear centre, bright
// rim, and a warm glow pooling at the bottom like light through glass.
inline unsigned OrbBodyPx(float x, float y) {
    float d = sqrtf(x * x + y * y);
    float edge = 1 - Smooth(0.94f, 1.0f, d);
    float rim = powf(d, 4);
    float pool = expf(-(x * x * 2.5f + (y - 0.62f) * (y - 0.62f) * 9));
    float v = fminf(1, 0.45f + 0.55f * rim + 0.6f * pool);
    float a = (0.35f + 0.6f * rim + 0.35f * pool) * edge;
    return Pack(v, v, v, fminf(1, a));
}

// The glossy highlight on the top half of an Aero button or orb.
inline unsigned GlossPx(float x, float y) {
    float ex = x / 0.72f, ey = (y + 0.42f) / 0.44f;
    float inside = 1 - Smooth(0.85f, 1.0f, sqrtf(ex * ex + ey * ey));
    float fade = 1 - Smooth(-0.9f, 0.05f, y);      // strong at the top, gone by the middle
    return Pack(1, 1, 1, 0.9f * inside * (0.25f + 0.75f * fade));
}

inline unsigned SoftDotPx(float x, float y) {
    float a = 1 - Smooth(0, 1, sqrtf(x * x + y * y));
    return Pack(1, 1, 1, a * a);
}

inline Texture* OrbBody(Renderer& r) { return MakeTexture(r, 256, OrbBodyPx); }
inline Texture* Gloss(Renderer& r) { return MakeTexture(r, 256, GlossPx); }

// One texture holding [orb body | gloss | soft dot] side by side, so a
// glossy object can be drawn as a unit (body, then its own highlight) in a
// single back-to-front pass. Use OrbCell() for the u range of each cell.
enum OrbCellId { ORB_BODY = 0, ORB_GLOSS = 1, ORB_DOT = 2 };
inline Texture* OrbAtlas(Renderer& r) {
    const int n = 256;
    std::vector<unsigned> px((size_t)n * 3 * n);
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n * 3; i++) {
            float x = ((i % n) + 0.5f) / n * 2 - 1, y = (j + 0.5f) / n * 2 - 1;
            int cell = i / n;
            px[(size_t)j * n * 3 + i] = cell == 0 ? OrbBodyPx(x, y) : cell == 1 ? GlossPx(x, y) : SoftDotPx(x, y);
        }
    return r.CreateTexture(px.data(), n * 3, n);
}
// Textured square of cell `id` from OrbAtlas, centred at (x, y), radius rad.
inline void OrbCell(Canvas2D& c, OrbCellId id, float x, float y, float rad, const Color& col, float squashX = 1) {
    // inset by half a texel so neighbouring cells never bleed in
    float u0 = id / 3.0f + 0.5f / 768, u1 = (id + 1) / 3.0f - 0.5f / 768;
    c.Rect(x - rad * squashX, y - rad, x + rad * squashX, y + rad, col, u0, 0, u1, 1);
}

// Fluffy cumulus puff from a fixed set of overlapping soft blobs.
inline Texture* Cloud(Renderer& r, unsigned seed) {
    struct Blob { float x, y, r; };
    std::vector<Blob> blobs;
    unsigned s = seed * 2654435761u + 12345;
    auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0f; };
    for (int i = 0; i < 14; i++) {
        float bx = (rnd() * 2 - 1) * 0.6f;
        float by = (rnd() * 0.7f - 0.15f) * (1 - fabsf(bx));   // dome-shaped cluster, flat bottom
        blobs.push_back({ bx, by, 0.22f + rnd() * 0.25f });
    }
    return MakeTexture(r, 256, [&blobs](float x, float y) {
        float a = 0, shade = 0;
        for (const Blob& b : blobs) {
            float d = sqrtf((x - b.x) * (x - b.x) + (y - b.y) * (y - b.y)) / b.r;
            float k = 1 - Smooth(0.55f, 1.0f, d);
            a = fmaxf(a, k);
            shade = fmaxf(shade, k * (0.5f - 0.5f * (y - b.y) / b.r));   // lit from above
        }
        a *= 1 - Smooth(0.25f, 0.45f, y);                               // flat underside
        float v = 0.82f + 0.18f * fminf(1, shade * 1.4f);
        return Pack(v, v, fminf(1, v + 0.03f), a);
    });
}

// Full-screen vertical gradient through `n` evenly spaced color stops
// (stops[0] at the top).
inline void VerticalGradient(Canvas2D& c, int width, int height, const Color* stops, int n) {
    float w = width * 0.5f, top = height * 0.5f, band = (float)height / (n - 1);
    for (int i = 0; i + 1 < n; i++) {
        float y0 = top - band * i, y1 = top - band * (i + 1);
        PushQuad(c.v, c.P(-w, y1), c.P(w, y1), c.P(w, y0), c.P(-w, y0), Vec3(0, 0, 1),
                 stops[i + 1], stops[i + 1], stops[i], stops[i]);
    }
}

// Rotated, textured rectangle centred on (x, y) in canvas pixels.
inline void Sprite(Canvas2D& c, float x, float y, float w, float h, float angle, const Color& col) {
    float ca = cosf(angle), sa = sinf(angle);
    float hx = w * 0.5f, hy = h * 0.5f;
    auto corner = [&](float px, float py) { return c.P(x + px * ca - py * sa, y + px * sa + py * ca); };
    PushQuad(c.v, corner(-hx, -hy), corner(hx, -hy), corner(hx, hy), corner(-hx, hy), Vec3(0, 0, 1),
             col, col, col, col, 0, 1, 1, 0);
}

// Soft horizontal band of light whose centre follows `yAt(x)`: used for
// aurora wisps and light streaks. Alpha fades to zero at both edges.
template <class F>
inline void LightBand(Canvas2D& c, int width, int segments, F yAt, float halfWidth, const Color& col) {
    float w = width * 0.5f;
    for (int i = 0; i < segments; i++) {
        float x0 = -w + width * (float)i / segments, x1 = -w + width * (float)(i + 1) / segments;
        float ya = yAt(x0), yb = yAt(x1);
        // fade the band's ends near the screen edges
        float fa = Smooth(0, 0.15f, (float)i / segments) * Smooth(0, 0.15f, 1 - (float)i / segments);
        float fb = Smooth(0, 0.15f, (float)(i + 1) / segments) * Smooth(0, 0.15f, 1 - (float)(i + 1) / segments);
        Color ca = col.WithAlpha(col.a * fa), cb = col.WithAlpha(col.a * fb);
        Color za = col.WithAlpha(0), zb = col.WithAlpha(0);
        PushQuad(c.v, c.P(x0, ya), c.P(x1, yb), c.P(x1, yb + halfWidth), c.P(x0, ya + halfWidth), Vec3(0, 0, 1), ca, cb, zb, za);
        PushQuad(c.v, c.P(x0, ya - halfWidth), c.P(x1, yb - halfWidth), c.P(x1, yb), c.P(x0, ya), Vec3(0, 0, 1), za, zb, cb, ca);
    }
}

}  // namespace aero
