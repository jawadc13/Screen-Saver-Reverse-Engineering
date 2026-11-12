// shader.h - the Kaleidoscope Prism pixel shader. Every pixel is folded
// through the mirrors analytically, so the image is exact at any resolution.
//
// The scene prepends #defines that pick one layout, one pattern and the
// finishing touches, so each compiled variant holds only the code it runs:
//   SAMPLES 1|2|4   LAYOUT 0..5   PATTERN 0..12   SYM 0|1 (1 = pinwheel)
//   SEAM 0..3   SPARK 0|1   FRINGE 0|1   JEWEL 0|1
// Cross-fades (pattern cycling, morphing cuts) are two passes blended by
// uFx.z, never a branch in here.
#pragma once

static const char kPrismShader[] = R"(
cbuffer K : register(b0) {
    float4 uRes;    // width, height, units per pixel (short side = -1..1), pattern time
    float4 uSeg;    // cuts (divisions), unused
    float4 uGeo;    // layout, spiral twist, rings / tile density, rotation
    float4 uView;   // centre x, centre y, 1 / zoom, detail
    float4 uPat;    // unused xyz, liquid warp
    float4 uFlow;   // flow offset x, y, chamber angle, sparkle
    float4 uSeam;   // seam style, seam width, centre jewel, vignette
    float4 uCol;    // colour phase, colour bands, vibrance, brightness
    float4 uFx;     // glow, prism fringe, output alpha (cross-fade pass), unused
    float4 uStops[6];   // palette (loops)
};

// atan2 written out (some HLSL compilers, e.g. Wine's, lack it).
float Atan2(float y, float x) {
    float ax = abs(x), ay = abs(y);
    float a = min(ax, ay) / max(max(ax, ay), 1e-8);
    float s = a * a;
    float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
    if (ay > ax) r = 1.57079637 - r;
    if (x < 0) r = 3.14159274 - r;
    return y < 0 ? -r : r;
}
float H1(float2 p) {
    float3 q = frac(p.xyx * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return frac((q.x + q.y) * q.z);
}
float2 H2(float2 p) {
    float3 q = frac(p.xyx * float3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return frac((q.xx + q.yz) * q.zy);
}
float VNoise(float2 p) {
    float2 i = floor(p), f = p - i;
    f = f * f * (3 - 2 * f);
    return lerp(lerp(H1(i), H1(i + float2(1, 0)), f.x), lerp(H1(i + float2(0, 1)), H1(i + float2(1, 1)), f.x), f.y);
}
float Fbm(float2 p) {
    return (0.5 * VNoise(p) + 0.25 * VNoise(p * 2.03 + 7.1) + 0.125 * VNoise(p * 4.01 + 13.7)) * 1.142857;
}
float2 Rot(float2 p, float a) {
    float c = cos(a), s = sin(a);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float3 Pal(float x) {
    x = frac(x) * 6;
    float i = min(floor(x), 5);
    float f = x - i;
    f = f * f * (3 - 2 * f);
    int a = (int)i;
    int b = a < 5 ? a + 1 : 0;
    return lerp(uStops[a].rgb, uStops[b].rgb, f);
}
// Palette with the colour channels slightly out of step, like light split by a prism.
float3 PalF(float x) {
#if FRINGE
    return float3(Pal(x - uFx.y).r, Pal(x).g, Pal(x + uFx.y).b);
#else
    return Pal(x);
#endif
}

// Angle folded into one division: x = folded angle, y = angle to the nearest seam.
float2 Wedge(float a, float n) {
    float seg = 6.2831853 / n;
    float m = frac(a / seg);
#if SYM == 0
    float af = min(m, 1 - m) * seg;
    float e = min(af, 0.5 * seg - af);
#else
    float af = (m - 0.5) * seg;
    float e = 0.5 * seg - abs(af);
#endif
    return float2(af, min(e, 1.5707963));
}

// Distance from a point at radius r to the nearest wedge seam, given its
// angle e to that seam. Where the wedges are narrower than the seam line
// (many cuts, or near the centre) the distance is stretched so a seam never
// covers more than about a third of its wedge - otherwise the seams would
// merge and fill the screen with one flat colour.
float SeamDist(float e, float r, float n) {
#if SYM == 0
    float room = r * sin(min(1.5707963 / n, 1.5707963));
#else
    float room = r * sin(min(3.1415927 / n, 1.5707963));
#endif
    return r * sin(e) * max(1, uSeam.y / max(0.3 * room, 1e-6));
}

// Mirrors. Returns the folded point (xy), the distance to the nearest
// mirror seam (z) and how much the fold magnifies the pattern (w). In the
// tile layouts every tile shows the same amount of pattern however small
// the tiles are, so a tile is never filled by a single flat colour.
float4 Fold(float2 p, float n) {
    float rings = uGeo.z, r = length(p);
#if LAYOUT == 0                    // classic wedge
    float2 w = Wedge(Atan2(p.y, p.x) + uGeo.w + uGeo.y * log(r + 0.02), n);
    return float4(r * cos(w.x), r * sin(w.x), SeamDist(w.y, r, n), 1);
#elif LAYOUT == 1                  // three mirrors in a triangle: tiles the plane
    float k = rings * 0.5;
    float2 s = Rot(p, uGeo.w) * k;
    float sy = s.y * 1.1547005, sx = s.x - 0.5 * sy;
    float2 c = floor(float2(sx, sy));
    float2 f = float2(sx, sy) - c;
    float3 b = float3(1 - f.x - f.y, f.x, f.y);
    if (f.x + f.y > 1) b = float3(f.x + f.y - 1, 1 - f.y, 1 - f.x);
    // Each lattice corner has one of three colours and mirroring keeps
    // them, so the corner colours say which way round this triangle is.
    float col = c.x - c.y;
    col = col - 3 * floor(col / 3);
    float3 w = b;
    if (col > 1.5) w = b.yzx; else if (col > 0.5) w = b.zxy;
    float2 q = w.y * float2(1, 0) + w.z * float2(0.5, 0.8660254);
    return float4(q, min(w.x, min(w.y, w.z)) * 0.8660254 / k, k);
#elif LAYOUT == 2                  // four mirrors in a square
    float k = rings * 0.5;
    float2 s = Rot(p, uGeo.w) * k;
    float2 q = abs(frac(s * 0.5) * 2 - 1);
    return float4(q, min(min(q.x, q.y), min(1 - q.x, 1 - q.y)) / k, k);
#elif LAYOUT == 3                  // a kaleidoscope in every hexagon
    float k = rings * 0.5;
    float2 s = Rot(p, uGeo.w * 0.25) * k;
    float2 hs = float2(1, 1.7320508);
    float2 la = s - (floor(s / hs) + 0.5) * hs;
    float2 lb = s - (floor(s / hs - 0.5) + 1.0) * hs;
    float2 l = la;
    if (dot(lb, lb) < dot(la, la)) l = lb;
    float lr = length(l);
    float2 w = Wedge(Atan2(l.y, l.x) + uGeo.w + uGeo.y * log(lr + 0.02), n);
    float he = 0.5 - max(abs(l.x), dot(abs(l), float2(0.5, 0.8660254)));
    return float4(lr * cos(w.x) * 2, lr * sin(w.x) * 2, min(SeamDist(w.y, lr / k, n), he / k), 2 * k);
#elif LAYOUT == 4                  // infinite tunnel (log-polar)
    float2 w = Wedge(Atan2(p.y, p.x) + uGeo.w + uGeo.y * log(r + 0.02), n);
    float k = 0.25 + rings * 0.12;
    return float4((log(max(r, 1e-4)) - uRes.w * 0.12) * k, w.x * k, SeamDist(w.y, r, n), k / max(r, 0.002));
#else                              // nested rings turning against each other
    float ri = floor(r * rings);
    float dir = frac(ri * 0.5) * 4 - 1;
    float2 w = Wedge(Atan2(p.y, p.x) + uGeo.w * dir * (1 + 0.25 * ri) + uGeo.y * log(r + 0.02), n);
    float re = (0.5 - abs(frac(r * rings) - 0.5)) / rings;
    return float4(r * cos(w.x), r * sin(w.x), min(SeamDist(w.y, r, n), re), 1);
#endif
}

// The coloured "glass" seen through the mirrors: x = palette position,
// y = brightness. w = size of a pixel in pattern units, for smooth edges.
float2 Pattern(float2 u, float w, float t) {
#if PATTERN == 0                   // silk
    float2 k = u * 1.6;
    k += 0.35 * sin(k.yx * 1.9 + t * float2(0.7, -0.5));
    float v = sin(k.x * 1.7 + t) + sin(k.y * 2.3 - t * 0.8) + sin((k.x + k.y) * 1.3 + t * 0.6) + sin(length(k) * 2.9 - t * 1.1);
    return float2(v * 0.22, 0.72 + 0.28 * sin(v * 3.1 + k.x));
#elif PATTERN == 1                 // stained glass
    float2 k = u * 2.2;
    float2 c = floor(k), f = k - c;
    float d1 = 9, d2 = 9, hid = 0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            float2 g = float2(i, j);
            float2 h = H2(c + g);
            float2 o = g + 0.5 + 0.38 * sin(t * 0.6 + 6.2831 * h) - f;
            float d = dot(o, o);
            if (d < d1) { d2 = d1; d1 = d; hid = h.x + h.y * 0.37; }
            else if (d < d2) d2 = d;
        }
    d1 = sqrt(d1);
    float e = sqrt(d2) - d1;
    float lead = smoothstep(0.035, 0.035 + max(0.03, 5 * w), e);
    return float2(hid + 0.1 * d1, lead * (1 - 0.45 * d1));
#elif PATTERN == 2 || PATTERN == 12   // gem facets / solid triangles
    float2 k = u * 2.4;
    float sy = k.y * 1.1547005, sx = k.x - 0.5 * sy;
    float2 c = floor(float2(sx, sy));
    float2 f = float2(sx, sy) - c;
    float up = step(1, f.x + f.y);
    float3 b = lerp(float3(1 - f.x - f.y, f.x, f.y), float3(f.x + f.y - 1, 1 - f.y, 1 - f.x), up);
    float h = H1(c * 2 + up);
    float e = min(b.x, min(b.y, b.z));
    float edge = smoothstep(0, 0.02 + 6 * w, e);
#if PATTERN == 2
    float facet = 0.62 + 0.38 * sin(6.2831 * (h + b.x * 0.35) + t * (0.6 + h));
    return float2(h + 0.12 * b.y + 0.05 * sin(t * 0.5 + h * 20), edge * facet);
#else
    return float2(floor(h * 6) / 6, 0.1 + 0.9 * edge);
#endif
#elif PATTERN == 3                 // petals
    float rr = length(u), aa = Atan2(u.y, u.x);
    float pet = sin(aa * 5 + rr * 3 - t * 0.5);
    return float2(rr * 0.9 - t * 0.12 + 0.12 * pet, 0.55 + 0.45 * abs(sin(rr * 7 - t * 0.9 + 1.5 * pet)));
#elif PATTERN == 4                 // ripples from three wandering drops
    float2 c1 = 0.9 * float2(sin(t * 0.31), cos(t * 0.23));
    float2 c2 = 0.9 * float2(cos(t * 0.19 + 2), sin(t * 0.27 + 1));
    float2 c3 = 0.9 * float2(sin(t * 0.17 + 4), cos(t * 0.29 + 3));
    float a = sin(length(u - c1) * 7 - t * 1.6), b = sin(length(u - c2) * 9 - t * 1.3), c = sin(length(u - c3) * 5 - t);
    return float2((a + b + c) * 0.15 + 0.1 * length(u), 0.68 + 0.32 * a * b);
#elif PATTERN == 5                 // marble
    float2 k = u * 1.3;
    float2 wv = float2(Fbm(k + float2(0, t * 0.11)), Fbm(k + float2(5.2, 1.3 - t * 0.13)));
    float n = Fbm(k + 2.2 * wv + float2(1.7, 9.2));
    return float2(n * 1.6 + 0.3 * wv.x, 0.55 + 0.45 * smoothstep(0.2, 0.8, wv.y + 0.5 * n - 0.2));
#elif PATTERN == 6                 // woven arcs
    float2 k = u * 2.6;
    float2 c = floor(k), f = k - c;
    if (H1(c) > 0.5) f.x = 1 - f.x;
    float d = min(abs(length(f) - 0.5), abs(length(f - 1) - 0.5));
    float aa = max(0.02, 5.2 * w);
    float line = 1 - smoothstep(0.13, 0.13 + aa, d);
    return float2((k.x + k.y) * 0.08 + d * 1.2 - t * 0.15, 0.1 + 0.9 * line + 0.35 * exp(-d * 9) * (1 - line));
#elif PATTERN == 7                 // glass beads
    float2 k = u * 3.0;
    float2 c = floor(k), f = k - c - 0.5;
    float h = H1(c + 3.7);
    float R = 0.24 + 0.1 * (1 + sin(t * (0.5 + h) + h * 30));
    float d = length(f), aa = max(0.01, 4.5 * w);
    float disc = 1 - smoothstep(R - aa, R + aa, d);
    float2 hl = f - float2(-0.3, 0.3) * R;
    float gloss = 0.55 + 0.45 * (1 - d / R) + 0.6 * exp(-dot(hl, hl) * 14 / (R * R));
    return float2(h + t * 0.03, 0.07 + disc * gloss);
#elif PATTERN == 8                 // ribbons
    float y = u.y * 2.2 + 0.6 * sin(u.x * 1.7 + t * 0.7) + 0.3 * sin(u.x * 3.9 - t);
    return float2(0.18 * y + 0.1 * sin(u.x * 1.1 + t * 0.3), 0.3 + 0.7 * smoothstep(0, 0.7, abs(sin(y * 1.6))));
#elif PATTERN == 9 || PATTERN == 10   // solid stripes / solid rings
    // Flat bands of colour: the cheapest patterns. Each band sits on its own
    // palette position; only the last pixel before an edge blends.
#if PATTERN == 9
    // Gently waved, so the bands still cross a thin wedge that lies along them.
    float s = u.y * 2.5 + 0.9 * sin(u.x * 1.8 + t * 0.3) + t * 0.2;
    float aa = min(0.5, 4.7 * w);
#else
    float s = length(u) * 3 - t * 0.3;
    float aa = min(0.5, 4.5 * w);
#endif
    float fl = floor(s);
    return float2((fl + smoothstep(1 - aa, 1, s - fl)) / 6, 1);
#else                              // solid tiles
    float2 k = u * 2.5;
    float2 c = floor(k), f = k - c;
    float e = min(min(f.x, f.y), min(1 - f.x, 1 - f.y));
    return float2(floor(H1(c) * 6) / 6, 0.1 + 0.9 * smoothstep(0.02, 0.02 + max(0.01, 3.75 * w), e));
#endif
}

float3 Paint(float2 P) { return PalF(P.x * uCol.y + uCol.x) * P.y; }

// One kaleidoscope with n divisions.
float3 Kal(float2 p, float n) {
    float4 F = Fold(p, n);
    float px = uRes.z * uView.z;
    float t = uRes.w;
    float2 u = Rot(F.xy, uFlow.z) * uView.w;
    u += uPat.w * float2(sin(u.y * 1.3 + t * 0.4), cos(u.x * 1.1 - t * 0.33));
    u += uFlow.xy;
    float w = px * uView.w * F.w;
    float2 P = Pattern(u, w, t);
    float3 col = Paint(P);
#if SPARK
    float2 sk = u * 5 + 17.3;
    float2 sc = floor(sk);
    float2 sh = H2(sc);
    float tw = saturate(sin(t * 1.7 + sh.x * 40));
    tw *= tw; tw *= tw; tw *= tw;
    float rad = max(0.1, 15 * w);
    float sd = length(sk - sc - 0.25 - 0.5 * sh);
    col += uFlow.w * tw * (1 - smoothstep(0, rad, sd)) * (0.1 / rad) * (0.6 + 0.4 * Pal(sh.y));
#endif
#if SEAM                           // mirror seams
    float sw = uSeam.y, e = F.z;
    float line = 1 - smoothstep(sw, sw + 1.5 * px, e);
#if SEAM == 1
    col += (0.45 + 0.55 * Pal(P.x * uCol.y + uCol.x + 0.5)) * (0.8 * line + 0.45 * exp(-e / (3 * sw + px)));
#elif SEAM == 2
    col *= 1 - line;
#else
    col = lerp(col, float3(1, 0.8, 0.42) * (0.55 + 0.45 * cos(saturate(e / sw) * 3.14159)), line);
#endif
#endif
#if LAYOUT == 4
    col *= smoothstep(0.0, 0.07, length(p));   // tunnel vanishing point
#endif
    return col;
}

float3 Shade(float2 frag) {
    float2 ps = (frag - 0.5 * uRes.xy) * uRes.z - uView.xy;
    float2 p = ps * uView.z;
    float3 c = Kal(p, uSeg.x);
#if JEWEL
    float r = length(ps);
    float ring = (r - 0.055) * 70;
    c += (0.5 + 0.5 * Pal(uCol.x + 0.3)) * (exp(-r * r * 900) * 1.2 + 0.35 * exp(-ring * ring));
#endif
    return c;
}

float4 PS(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
#if SAMPLES == 4
    float3 c = (Shade(pos.xy + float2(0.125, 0.375)) + Shade(pos.xy + float2(-0.375, 0.125))
              + Shade(pos.xy + float2(0.375, -0.125)) + Shade(pos.xy + float2(-0.125, -0.375))) * 0.25;
#elif SAMPLES == 2
    float3 c = (Shade(pos.xy + float2(0.25, 0.25)) + Shade(pos.xy - float2(0.25, 0.25))) * 0.5;
#else
    float3 c = Shade(pos.xy);
#endif
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = max(lerp(float3(l, l, l), c, uCol.z), 0);
    c += c * c * uFx.x;
    c *= uCol.w;
    float2 sp = (pos.xy - 0.5 * uRes.xy) * uRes.z;
    c *= 1 - uSeam.w * smoothstep(0.7, 2.0, length(sp));
    // Over-bright colours keep their hue instead of clipping channel by channel.
    float m = max(c.r, max(c.g, c.b));
    if (m > 1) c = lerp(c / m, float3(1, 1, 1), saturate((m - 1) * 0.35));
    c += (H1(pos.xy) - 0.5) / 255;   // dither: no banding in smooth gradients
    return float4(c, uFx.z);
}
)";
