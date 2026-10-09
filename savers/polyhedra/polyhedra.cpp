// polyhedra.cpp - "3D Polyhedra": shiny Platonic solids tumbling and
// bouncing inside an invisible box. Same framework/rules as 3D Pipes.
#include "../../common/mesh.h"
#include "resource.h"
#include <commctrl.h>

enum { SHAPE_TETRA, SHAPE_CUBE, SHAPE_OCTA, SHAPE_ICOSA, SHAPE_MIXED };

static DWORD g_speed;   // "Speed" 0..100
static DWORD g_count;   // "Count" 1..20
static DWORD g_shape;   // "Shape" SHAPE_*
static DWORD g_trails;  // "Trails" 0/1 (motion blur)

const wchar_t* RegistryName() { return L"Polyhedra"; }

void LoadSettings() {
    g_speed  = RegReadDword(L"Speed", 50);
    g_count  = RegReadDword(L"Count", 8);
    g_shape  = RegReadDword(L"Shape", SHAPE_MIXED);
    g_trails = RegReadDword(L"Trails", 0);
    if (g_speed > 100) g_speed = 100;
    if (g_count < 1) g_count = 1;
    if (g_count > 20) g_count = 20;
    if (g_shape > SHAPE_MIXED) g_shape = SHAPE_MIXED;
}

static void SaveSettings() {
    RegWriteDword(L"Speed", g_speed);
    RegWriteDword(L"Count", g_count);
    RegWriteDword(L"Shape", g_shape);
    RegWriteDword(L"Trails", g_trails);
}

static INT_PTR CALLBACK ConfigDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        for (int i = 0; i < 5; i++) {
            wchar_t name[32]; LoadStringW(g_hInst, IDS_SHAPE_TETRA + i, name, 32);
            SendDlgItemMessageW(dlg, IDC_SHAPE, CB_ADDSTRING, 0, (LPARAM)name);
        }
        SendDlgItemMessageW(dlg, IDC_SHAPE, CB_SETCURSEL, g_shape, 0);
        SendDlgItemMessageW(dlg, IDC_COUNT_SPIN, UDM_SETRANGE32, 1, 20);
        SendDlgItemMessageW(dlg, IDC_COUNT_SPIN, UDM_SETPOS32, 0, g_count);
        CheckDlgButton(dlg, IDC_TRAILS, g_trails ? BST_CHECKED : BST_UNCHECKED);
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETTICFREQ, 10, 0);
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETPOS, TRUE, g_speed);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_DISPLAY_SETTINGS: ShowDisplaySettings(dlg); return TRUE;
        case IDOK: {
            g_shape = (DWORD)SendDlgItemMessageW(dlg, IDC_SHAPE, CB_GETCURSEL, 0, 0);
            BOOL ok;
            UINT n = GetDlgItemInt(dlg, IDC_COUNT, &ok, FALSE);
            g_count = ok ? (n < 1 ? 1 : n > 20 ? 20 : n) : g_count;
            g_trails = IsDlgButtonChecked(dlg, IDC_TRAILS) == BST_CHECKED;
            g_speed = (DWORD)SendDlgItemMessageW(dlg, IDC_SPEED, TBM_GETPOS, 0, 0);
            SaveSettings();
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowConfigDialog(HWND parent) {
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_CONFIG), parent, ConfigDlgProc, 0);
}

// ---------------------------------------------------------------------------
// Geometry: flat-shaded unit solids (circumradius 1).
// ---------------------------------------------------------------------------
static void AddFace(Mesh& m, Vec3 a, Vec3 b, Vec3 c) {
    static const unsigned char white[4] = { 255, 255, 255, 255 };
    Vec3 n = Normalize(Cross(b - a, c - a));
    if (Dot(n, a + b + c) < 0) { Vec3 t = b; b = c; c = t; n = -n; }  // face outward
    m.Add(a, n, 0, 0, white); m.Add(b, n, 1, 0, white); m.Add(c, n, 0, 1, white);
}

static void BuildSolid(Mesh& m, int shape) {
    m.Clear();
    switch (shape) {
    case SHAPE_TETRA: {
        Vec3 v[4] = { {1,1,1}, {-1,-1,1}, {-1,1,-1}, {1,-1,-1} };
        int f[4][3] = { {0,1,2}, {0,3,1}, {0,2,3}, {1,3,2} };
        for (auto& t : f) AddFace(m, Normalize(v[t[0]]), Normalize(v[t[1]]), Normalize(v[t[2]]));
        break;
    }
    case SHAPE_CUBE: {
        float s = 1 / sqrtf(3);
        for (int axis = 0; axis < 3; axis++)
            for (int sign = -1; sign <= 1; sign += 2) {
                Vec3 c[4];
                for (int i = 0; i < 4; i++) {
                    float a = (i == 1 || i == 2) ? s : -s, b = (i >= 2) ? s : -s;
                    float p[3]; p[axis] = sign * s; p[(axis + 1) % 3] = a; p[(axis + 2) % 3] = b;
                    c[i] = Vec3(p[0], p[1], p[2]);
                }
                AddFace(m, c[0], c[1], c[2]);
                AddFace(m, c[0], c[2], c[3]);
            }
        break;
    }
    case SHAPE_OCTA: {
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    AddFace(m, Vec3((float)sx, 0, 0), Vec3(0, (float)sy, 0), Vec3(0, 0, (float)sz));
        break;
    }
    default: {   // icosahedron
        float g = (1 + sqrtf(5)) / 2;
        Vec3 v[12] = { {-1,g,0}, {1,g,0}, {-1,-g,0}, {1,-g,0}, {0,-1,g}, {0,1,g},
                       {0,-1,-g}, {0,1,-g}, {g,0,-1}, {g,0,1}, {-g,0,-1}, {-g,0,1} };
        int f[20][3] = { {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
                         {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1} };
        for (auto& t : f) AddFace(m, Normalize(v[t[0]]), Normalize(v[t[1]]), Normalize(v[t[2]]));
        break;
    }
    }
}

struct Body {
    Vec3 pos, vel, axis;
    float angle, spin, size;
    int shape;
    float color[4];
};

class PolyhedraScene : public Scene {
    Mesh solids[4];
    std::vector<Body> bodies;
    int width = 1, height = 1;
    float boxX = 10, boxY = 7, boxZ = 6, speedScale = 1;
    bool firstFrame = true;

public:
    bool Init(int w, int h, bool preview) override {
        for (int i = 0; i < 4; i++) BuildSolid(solids[i], i);
        speedScale = 0.3f + g_speed / 40.0f;
        Resize(w, h);
        bodies.resize(g_count);
        for (auto& b : bodies) {
            b.shape = g_shape == SHAPE_MIXED ? RandI(0, 3) : (int)g_shape;
            b.size = RandF(0.9f, 1.6f);
            b.pos = Vec3(RandF(-boxX, boxX) * 0.7f, RandF(-boxY, boxY) * 0.7f, RandF(-boxZ, boxZ) * 0.7f);
            b.vel = Normalize(Vec3(RandF(-1, 1), RandF(-1, 1), RandF(-1, 1))) * RandF(2, 5);
            b.axis = Normalize(Vec3(RandF(-1, 1), RandF(-1, 1), RandF(-1, 1)));
            b.angle = RandF(0, 360);
            b.spin = RandF(30, 120) * (RandI(0, 1) ? 1 : -1);
            float hue = RandF(0, 6); int i = (int)hue; float f = hue - i;
            float c[6][3] = {{1,f,0},{1-f,1,0},{0,1,f},{0,1-f,1},{f,0,1},{1,0,1-f}};
            for (int k = 0; k < 3; k++) b.color[k] = 0.25f + 0.75f * c[i % 6][k];
            b.color[3] = 1;
        }
        return true;
    }

    void Resize(int w, int h) override {
        width = w; height = h > 0 ? h : 1;
        boxY = 7;
        boxX = boxY * width / height;
        glViewport(0, 0, width, height);
        firstFrame = true;
    }

    void Frame(float dt) override {
        float sdt = dt * speedScale;
        for (auto& b : bodies) {
            b.pos = b.pos + b.vel * sdt;
            b.angle += b.spin * sdt;
            float lim[3] = { boxX - b.size, boxY - b.size, boxZ - b.size };
            float* p[3] = { &b.pos.x, &b.pos.y, &b.pos.z };
            float* v[3] = { &b.vel.x, &b.vel.y, &b.vel.z };
            for (int k = 0; k < 3; k++) {
                if (*p[k] > lim[k])  { *p[k] = lim[k];  *v[k] = -fabsf(*v[k]); }
                if (*p[k] < -lim[k]) { *p[k] = -lim[k]; *v[k] = fabsf(*v[k]); }
            }
        }

        if (g_trails && !firstFrame) {
            // Darken the previous frame instead of clearing it -> trails.
            // (The back buffer keeps its content on most drivers; if it
            // doesn't, this degrades to a plain clear.)
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
            glColor4f(0, 0, 0, 0.25f);
            glBegin(GL_QUADS);
            glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
            glEnd();
            glDisable(GL_BLEND);
            glClear(GL_DEPTH_BUFFER_BIT);
        } else {
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        firstFrame = false;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)width / height, 1.0, 100.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        float dist = boxY / tanf(22.5f * kPi / 180) + boxZ;
        glTranslatef(0, 0, -dist);

        float l0[4] = { -0.5f, 0.8f, 1.0f, 0 }, l1[4] = { 0.7f, -0.4f, 0.6f, 0 };
        float white[4] = { 1, 1, 1, 1 }, blue[4] = { 0.25f, 0.3f, 0.5f, 1 }, amb[4] = { 0.12f, 0.12f, 0.12f, 1 };
        glLightfv(GL_LIGHT0, GL_POSITION, l0);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
        glLightfv(GL_LIGHT0, GL_SPECULAR, white);
        glLightfv(GL_LIGHT1, GL_POSITION, l1);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, blue);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_NORMALIZE);
        glShadeModel(GL_FLAT);
        float spec[4] = { 0.9f, 0.9f, 0.9f, 1 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 70);
        glDisable(GL_COLOR_MATERIAL);   // per-body material below; vertex colors are white

        for (auto& b : bodies) {
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, b.color);
            glPushMatrix();
            glTranslatef(b.pos.x, b.pos.y, b.pos.z);
            glRotatef(b.angle, b.axis.x, b.axis.y, b.axis.z);
            glScalef(b.size, b.size, b.size);
            // Draw with the color array off so the material color applies.
            const Mesh& m = solids[b.shape];
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_NORMAL_ARRAY);
            glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &m.verts[0].px);
            glNormalPointer(GL_FLOAT, sizeof(Vertex), &m.verts[0].nx);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m.verts.size());
            glDisableClientState(GL_NORMAL_ARRAY);
            glDisableClientState(GL_VERTEX_ARRAY);
            glPopMatrix();
        }
        glDisable(GL_LIGHTING);
    }
};

Scene* CreateScene() { return new PolyhedraScene; }
