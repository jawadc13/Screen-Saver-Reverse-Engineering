// mesh.h - tiny vector/matrix math and a CPU-side triangle list builder.
#pragma once
#include <math.h>
#include <string.h>
#include <vector>

static const float kPi = 3.14159265358979f;

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
};
inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3  Cross(const Vec3& a, const Vec3& b) { return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float Length(const Vec3& a) { return sqrtf(Dot(a, a)); }
inline Vec3  Normalize(const Vec3& a) { float l = Length(a); return l > 0 ? a * (1.0f / l) : a; }
// Any unit vector perpendicular to `a`.
inline Vec3 Perpendicular(const Vec3& a) {
    Vec3 t = fabsf(a.x) < 0.9f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
    return Normalize(Cross(a, t));
}

// 4x4 matrix, m[row][col], for column vectors (p' = M * p) - the same
// conventions as classic OpenGL (right-handed, camera looks down -Z).
struct Mat4 {
    float m[4][4];
    static Mat4 Identity() {
        Mat4 r; memset(r.m, 0, sizeof(r.m));
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1;
        return r;
    }
    static Mat4 Translate(float x, float y, float z) {
        Mat4 r = Identity(); r.m[0][3] = x; r.m[1][3] = y; r.m[2][3] = z; return r;
    }
    static Mat4 Scale(float s) {
        Mat4 r = Identity(); r.m[0][0] = r.m[1][1] = r.m[2][2] = s; return r;
    }
    // Rotation of `deg` degrees about axis (x,y,z), like glRotatef.
    static Mat4 Rotate(float deg, float x, float y, float z) {
        Vec3 a = Normalize(Vec3(x, y, z));
        float c = cosf(deg * kPi / 180), s = sinf(deg * kPi / 180), t = 1 - c;
        Mat4 r = Identity();
        r.m[0][0] = t * a.x * a.x + c;       r.m[0][1] = t * a.x * a.y - s * a.z; r.m[0][2] = t * a.x * a.z + s * a.y;
        r.m[1][0] = t * a.x * a.y + s * a.z; r.m[1][1] = t * a.y * a.y + c;       r.m[1][2] = t * a.y * a.z - s * a.x;
        r.m[2][0] = t * a.x * a.z - s * a.y; r.m[2][1] = t * a.y * a.z + s * a.x; r.m[2][2] = t * a.z * a.z + c;
        return r;
    }
    // Camera at `eye` looking at `target` (like gluLookAt).
    static Mat4 LookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = Normalize(target - eye), sd = Normalize(Cross(f, up)), u = Cross(sd, f);
        Mat4 r = Identity();
        r.m[0][0] = sd.x; r.m[0][1] = sd.y; r.m[0][2] = sd.z; r.m[0][3] = -Dot(sd, eye);
        r.m[1][0] = u.x;  r.m[1][1] = u.y;  r.m[1][2] = u.z;  r.m[1][3] = -Dot(u, eye);
        r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z; r.m[2][3] = Dot(f, eye);
        return r;
    }
    // Perspective projection with Direct3D's [0,1] depth range.
    static Mat4 Perspective(float fovyDeg, float aspect, float zn, float zf) {
        float f = 1.0f / tanf(fovyDeg * kPi / 360);
        Mat4 r; memset(r.m, 0, sizeof(r.m));
        r.m[0][0] = f / aspect;
        r.m[1][1] = f;
        r.m[2][2] = zf / (zn - zf);
        r.m[2][3] = zn * zf / (zn - zf);
        r.m[3][2] = -1;
        return r;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r.m[i][j] = m[i][0] * o.m[0][j] + m[i][1] * o.m[1][j] + m[i][2] * o.m[2][j] + m[i][3] * o.m[3][j];
        return r;
    }
};

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    unsigned char r, g, b, a;
};

struct Mesh {
    std::vector<Vertex> verts;

    void Clear() { verts.clear(); }

    void Add(const Vec3& p, const Vec3& n, float u, float v, const unsigned char* rgba) {
        Vertex vx = { p.x, p.y, p.z, n.x, n.y, n.z, u, v, rgba[0], rgba[1], rgba[2], rgba[3] };
        verts.push_back(vx);
    }

    // Quad strip between two rings of `slices`+1 points each.
    void AddRingStrip(const Vec3* p0, const Vec3* n0, float v0,
                      const Vec3* p1, const Vec3* n1, float v1,
                      int slices, const unsigned char* rgba) {
        for (int i = 0; i < slices; i++) {
            float u0 = (float)i / slices, u1 = (float)(i + 1) / slices;
            Add(p0[i], n0[i], u0, v0, rgba);     Add(p1[i], n1[i], u0, v1, rgba);         Add(p1[i + 1], n1[i + 1], u1, v1, rgba);
            Add(p0[i], n0[i], u0, v0, rgba);     Add(p1[i + 1], n1[i + 1], u1, v1, rgba); Add(p0[i + 1], n0[i + 1], u1, v0, rgba);
        }
    }

    void AddSphere(const Vec3& c, float r, int slices, const unsigned char* rgba) {
        int stacks = slices / 2;
        std::vector<Vec3> pa(slices + 1), na(slices + 1), pb(slices + 1), nb(slices + 1);
        for (int s = 0; s < stacks; s++) {
            float t0 = kPi * s / stacks, t1 = kPi * (s + 1) / stacks;
            for (int i = 0; i <= slices; i++) {
                float ph = 2 * kPi * i / slices;
                na[i] = Vec3(sinf(t0) * cosf(ph), cosf(t0), sinf(t0) * sinf(ph));
                nb[i] = Vec3(sinf(t1) * cosf(ph), cosf(t1), sinf(t1) * sinf(ph));
                pa[i] = c + na[i] * r;
                pb[i] = c + nb[i] * r;
            }
            AddRingStrip(pa.data(), na.data(), (float)s / stacks, pb.data(), nb.data(), (float)(s + 1) / stacks, slices, rgba);
        }
    }
};
