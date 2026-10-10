// oledkit.h - building blocks for OLED-friendly 3D screensavers.
//
// OLED rules every saver built on this follows:
//  * Pure black background (RGB 0,0,0) - unused pixels are fully off.
//  * Sparse light: thin glowing lines and small points, never large bright
//    areas, so average screen brightness stays very low.
//  * Nothing stands still: the camera always orbits/drifts slowly, so no
//    bright element sits on the same pixels for long (no burn-in).
//  * A Brightness setting caps peak brightness.
//  * Smooth, time-based fades (no popping), as in Matrix.
//
// Drawing is done in 3D -> projected on the CPU -> drawn as constant-pixel-
// width glowing lines and soft points, so lines are equally crisp on a
// 1080p, an ultrawide or a 4K portrait screen.
#pragma once
#include "aerokit.h"

namespace oled {

// Common settings rows, so all OLED savers share the same look and feel.
#define OLED_COLOR_LIST L"Cyan|Magenta|Amber|Green|Ice blue|Rainbow|White"
#define OLED_COLORS_CHOICE  { L"&Colors:", L"Colors", 0, OLED_COLOR_LIST }
#define OLED_COLORS_CHOICE_DEF(n) { L"&Colors:", L"Colors", n, OLED_COLOR_LIST }
#define OLED_SPEED_SLIDER   { L"Speed", L"Speed", 40, L"Slow", L"Fast" }
#define OLED_BRIGHT_SLIDER  { L"Brightness", L"Brightness", 70, L"Dim", L"Bright" }

// Palette for the shared Colors list; t in [0,1] varies the shade a little
// (or sweeps the hue for Rainbow).
inline Color Palette(DWORD choice, float t) {
    switch (choice) {
    case 1:  return Lerp(Color(1.0f, 0.15f, 0.85f), Color(0.6f, 0.3f, 1.0f), t);
    case 2:  return Lerp(Color(1.0f, 0.55f, 0.08f), Color(1.0f, 0.85f, 0.3f), t);
    case 3:  return Lerp(Color(0.15f, 1.0f, 0.35f), Color(0.6f, 1.0f, 0.3f), t);
    case 4:  return Lerp(Color(0.45f, 0.7f, 1.0f), Color(0.85f, 0.95f, 1.0f), t);
    case 5:  return Hsv(t, 0.8f, 1);
    case 6:  return Lerp(Color(0.85f, 0.88f, 1.0f), Color(1, 1, 1), t);
    default: return Lerp(Color(0.05f, 0.85f, 1.0f), Color(0.4f, 1.0f, 0.95f), t);
    }
}

// Perspective camera that projects to canvas pixels (origin at the centre,
// +y up). The field of view applies to the screen's SHORT side, so scenes
// look the same on landscape, ultrawide and portrait monitors.
struct Camera {
    Mat4 view = Mat4::Identity();
    float focal = 1;   // pixels per unit at distance 1
    int w = 1, h = 1;
    float nearZ = 0.1f;

    void Set(const Vec3& eye, const Vec3& target, float fovShortDeg, int width, int height, const Vec3& up = Vec3(0, 1, 0)) {
        w = width; h = height > 0 ? height : 1;
        view = Mat4::LookAt(eye, target, up);
        float shortSide = (float)(w < h ? w : h);
        focal = shortSide * 0.5f / tanf(fovShortDeg * kPi / 360);
    }
    // Returns false if the point is behind the camera.
    bool Project(const Vec3& p, float& sx, float& sy, float& depth) const {
        float x = view.m[0][0] * p.x + view.m[0][1] * p.y + view.m[0][2] * p.z + view.m[0][3];
        float y = view.m[1][0] * p.x + view.m[1][1] * p.y + view.m[1][2] * p.z + view.m[1][3];
        float z = view.m[2][0] * p.x + view.m[2][1] * p.y + view.m[2][2] * p.z + view.m[2][3];
        depth = -z;
        if (depth < nearZ) return false;
        sx = x / depth * focal;
        sy = y / depth * focal;
        return true;
    }
    // Camera circling `target` at `radius`, slowly, with a gentle bob: the
    // standard anti-burn-in motion used by most OLED savers.
    void Orbit(const Vec3& target, float radius, float angle, float elevation, float fov, int width, int height) {
        Vec3 eye = target + Vec3(sinf(angle) * cosf(elevation), sinf(elevation), cosf(angle) * cosf(elevation)) * radius;
        Set(eye, target, fov, width, height);
    }
};

// Accumulates glowing lines/points for one frame and draws them additively
// on pure black.
struct Painter {
    Canvas2D lines, dots;
    Texture* dot = nullptr;
    float brightness = 0.7f;   // global cap from the Brightness slider
    float px = 1;              // pixel scale: 1 at 1080p, 2 at 4K
    float fogNear = 1e9f, fogFar = 2e9f;   // depth fade to black
    int w = 1, h = 1;

    void Create(Renderer& r) { dot = aero::SoftDot(r); }
    void Release() { delete dot; dot = nullptr; }

    void Begin(int width, int height, DWORD brightSlider) {
        w = width; h = height > 0 ? height : 1;
        lines.Begin(w, h);
        dots.Begin(w, h);
        brightness = 0.25f + 0.75f * brightSlider / 100.0f;
        px = (float)(w < h ? w : h) / 1080.0f;
        if (px < 0.4f) px = 0.4f;
    }

    float Fog(float depth) const {
        if (depth <= fogNear) return 1;
        if (depth >= fogFar) return 0;
        float f = 1 - (depth - fogNear) / (fogFar - fogNear);
        return f * f;
    }

    // 2D glowing line in canvas pixels: a faint wide halo plus a bright core.
    void Line2(float ax, float ay, float bx, float by, Color ca, Color cb, float width) {
        float k = brightness;
        lines.Line(ax, ay, bx, by, width * 2.6f * px, ca * (0.18f * k), cb * (0.18f * k));
        lines.Line(ax, ay, bx, by, width * 0.6f * px, ca * k, cb * k);
    }

    void Line3(const Camera& cam, const Vec3& a, const Vec3& b, const Color& ca, const Color& cb, float width = 1.5f) {
        float ax, ay, ad, bx, by, bd;
        if (!cam.Project(a, ax, ay, ad) || !cam.Project(b, bx, by, bd)) return;
        float fa = Fog(ad), fb = Fog(bd);
        if (fa <= 0 && fb <= 0) return;
        Line2(ax, ay, bx, by, ca * fa, cb * fb, width);
    }

    // Soft glowing point. `size` is in 1080p pixels at distance 1 and shrinks
    // with distance (perspective); minSize keeps far points visible.
    void Point3(const Camera& cam, const Vec3& p, const Color& c, float size, float minSize = 1.0f) {
        float sx, sy, d;
        if (!cam.Project(p, sx, sy, d)) return;
        float f = Fog(d);
        if (f <= 0) return;
        float s = fmaxf(minSize * 1.6f, size / d * 22) * px;
        dots.Dot(sx, sy, s, c * (f * brightness));
    }

    void Point2(float x, float y, const Color& c, float size) { dots.Dot(x, y, size * px, c * brightness); }

    void Draw(Renderer& r) {
        lines.Draw(r, BLEND_ADD);
        dots.Draw(r, BLEND_ADD, dot);
    }
};

// A fading trail: positions with timestamps, drawn with brightness by age
// (continuous, so nothing pops) plus a live head.
struct Trail {
    std::vector<Vec3> pts;
    std::vector<float> times;
    float life = 2.0f;   // seconds a point stays visible
    float rate = 120;    // samples kept per second (the newest is always live)

    void Push(const Vec3& p, float now) {
        if (times.size() >= 2 && now - times[times.size() - 2] < 1.0f / rate) { pts.back() = p; times.back() = now; return; }
        pts.push_back(p);
        times.push_back(now);
        size_t drop = 0;
        while (drop < times.size() && now - times[drop] > life) drop++;
        if (drop) { pts.erase(pts.begin(), pts.begin() + drop); times.erase(times.begin(), times.begin() + drop); }
    }
    void Clear() { pts.clear(); times.clear(); }

    template <class ColorFn>
    void Draw(Painter& pt, const Camera& cam, float now, ColorFn colorAt, float width = 1.5f) const {
        for (size_t i = 1; i < pts.size(); i++) {
            float a0 = 1 - (now - times[i - 1]) / life, a1 = 1 - (now - times[i]) / life;
            if (a1 <= 0) continue;
            a0 = fmaxf(0, a0);
            pt.Line3(cam, pts[i - 1], pts[i], colorAt(a0) * (a0 * a0), colorAt(a1) * (a1 * a1), width);
        }
    }
};

// Slow wander for the camera's target: keeps bright centres (a sun, a
// nucleus, a galaxy core) from sitting on the same pixels.
inline Vec3 Drift(float t, float amp) {
    return Vec3(sinf(t * 0.051f), sinf(t * 0.037f) * 0.6f, cosf(t * 0.043f)) * amp;
}

// Circle (or ellipse) in the plane spanned by u, v around c.
template <class ColorFn>
inline void Circle3(Painter& pt, const Camera& cam, const Vec3& c, const Vec3& u, const Vec3& v,
                    float ru, float rv, int segs, ColorFn colorAt, float width = 1.2f) {
    Vec3 prev = c + u * ru;
    for (int i = 1; i <= segs; i++) {
        float a = 2 * kPi * i / segs;
        Vec3 p = c + u * (cosf(a) * ru) + v * (sinf(a) * rv);
        pt.Line3(cam, prev, p, colorAt((float)(i - 1) / segs), colorAt((float)i / segs), width);
        prev = p;
    }
}

// Base class: handles size, time, painter and the draw call; savers
// implement Setup() and Tick().
class Scene3D : public Scene {
protected:
    int width = 1, height = 1;
    float t = 0;
    bool preview = false;
    Camera cam;
    Painter pt;
    virtual void Setup() {}
    virtual DWORD BrightnessSlider() const = 0;
    virtual void Tick(float dt) = 0;   // update and paint into pt using cam
public:
    ~Scene3D() { pt.Release(); }
    bool Init(Renderer& r, int w, int h, bool pv) override {
        width = w; height = h > 0 ? h : 1; preview = pv;
        pt.Create(r);
        Setup();
        return true;
    }
    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }
    void Frame(Renderer& r, float dt) override {
        t += dt;
        pt.Begin(width, height, BrightnessSlider());
        Tick(dt);
        r.BeginFrame(true);   // pure black
        pt.Draw(r);
    }
};

}  // namespace oled
