// scenekit.h - small helpers shared by the screensaver scenes: colors,
// vertex/quad builders, and 2D drawing in pixel coordinates.
#pragma once
#include "saver.h"
#include "simplecfg.h"

struct Color {
    float r, g, b, a;
    Color(float r_ = 1, float g_ = 1, float b_ = 1, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
    Color operator*(float s) const { return Color(r * s, g * s, b * s, a); }
    Color WithAlpha(float na) const { return Color(r, g, b, na); }
};

// h, s, v in [0,1] (h wraps).
inline Color Hsv(float h, float s, float v, float a = 1) {
    h = h - floorf(h);
    float hh = h * 6; int i = (int)hh; float f = hh - i;
    float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i % 6) {
    case 0: return Color(v, t, p, a);
    case 1: return Color(q, v, p, a);
    case 2: return Color(p, v, t, a);
    case 3: return Color(p, q, v, a);
    case 4: return Color(t, p, v, a);
    default: return Color(v, p, q, a);
    }
}

inline Color Lerp(const Color& a, const Color& b, float t) {
    return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

inline unsigned char ToByte(float x) { return (unsigned char)(x <= 0 ? 0 : x >= 1 ? 255 : x * 255 + 0.5f); }

inline void PushVertex(std::vector<Vertex>& out, const Vec3& p, const Vec3& n, float u, float v, const Color& c) {
    Vertex vx = { p.x, p.y, p.z, n.x, n.y, n.z, u, v, ToByte(c.r), ToByte(c.g), ToByte(c.b), ToByte(c.a) };
    out.push_back(vx);
}

// Two triangles a-b-c, a-c-d with per-corner colors.
inline void PushQuad(std::vector<Vertex>& out, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                     const Vec3& n, const Color& ca, const Color& cb, const Color& cc, const Color& cd,
                     float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
    PushVertex(out, a, n, u0, v0, ca); PushVertex(out, b, n, u1, v0, cb); PushVertex(out, c, n, u1, v1, cc);
    PushVertex(out, a, n, u0, v0, ca); PushVertex(out, c, n, u1, v1, cc); PushVertex(out, d, n, u0, v1, cd);
}

// 2D drawing in pixels, origin at the screen centre, +y up.
struct Canvas2D {
    std::vector<Vertex> v;
    float sx = 1, sy = 1;
    void Begin(int width, int height) { v.clear(); sx = 2.0f / width; sy = 2.0f / height; }
    Vec3 P(float x, float y) const { return Vec3(x * sx, y * sy, 0.5f); }

    // Axis-aligned rectangle, optional texture rectangle.
    void Rect(float x0, float y0, float x1, float y1, const Color& c,
              float u0 = 0, float v0 = 0, float u1 = 1, float v1 = 1) {
        PushQuad(v, P(x0, y0), P(x1, y0), P(x1, y1), P(x0, y1), Vec3(0, 0, 1), c, c, c, c, u0, v1, u1, v0);
    }
    // Thick line segment with a color per end (smooth gradient).
    void Line(float ax, float ay, float bx, float by, float halfWidth, const Color& ca, const Color& cb) {
        float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-3f) return;
        float nx = -dy / len * halfWidth, ny = dx / len * halfWidth;
        PushQuad(v, P(ax + nx, ay + ny), P(bx + nx, by + ny), P(bx - nx, by - ny), P(ax - nx, ay - ny),
                 Vec3(0, 0, 1), ca, cb, cb, ca);
    }
    // Soft round-ish dot (a square; additive blending makes it glow).
    void Dot(float x, float y, float r, const Color& c) { Rect(x - r, y - r, x + r, y + r, c); }

    void Draw(Renderer& r, BlendMode blend, Texture* tex = nullptr) {
        r.SetCamera(Mat4::Identity(), Mat4::Identity());
        DrawParams p;
        p.lit = false;
        p.depth = false;
        p.blend = blend;
        p.texture = tex;
        r.Draw(v.data(), v.size(), p);
    }
};

// Frame-rate independent "keep this fraction per second" fade factor.
inline float FadeAlpha(float keepPerSecond, float dt) { return 1.0f - powf(keepPerSecond, dt); }
