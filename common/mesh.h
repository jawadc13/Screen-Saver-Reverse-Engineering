// mesh.h - tiny vector math + an interleaved triangle list drawn with
// OpenGL 1.1 client-side vertex arrays (works on every Windows GL driver,
// including the built-in "GDI Generic" software renderer).
#pragma once
#include "saver.h"
#include <math.h>

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
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

static const float kPi = 3.14159265358979f;

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

    void Draw(bool textured) const { DrawRange(0, verts.size(), textured); }

    void DrawRange(size_t first, size_t count, bool textured) const {
        if (!count) return;
        const Vertex* v = &verts[first];
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &v->px);
        glNormalPointer(GL_FLOAT, sizeof(Vertex), &v->nx);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &v->r);
        if (textured) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &v->u);
        }
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)count);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    // Quad strip between two rings of `slices`+1 points each.
    void AddRingStrip(const Vec3* p0, const Vec3* n0, float v0,
                      const Vec3* p1, const Vec3* n1, float v1,
                      int slices, const unsigned char* rgba) {
        for (int i = 0; i < slices; i++) {
            float u0 = (float)i / slices, u1 = (float)(i + 1) / slices;
            Add(p0[i], n0[i], u0, v0, rgba);     Add(p1[i], n1[i], u0, v1, rgba);     Add(p1[i + 1], n1[i + 1], u1, v1, rgba);
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
