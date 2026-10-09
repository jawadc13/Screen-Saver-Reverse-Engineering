// pipes.cpp - "3D Pipes" re-creation.
//
// Rules recovered from sspipes.scr (see docs/ANALYSIS.md):
//  * Pipes grow cell by cell through an invisible 3D grid, never crossing
//    an occupied cell, and turn at random or when blocked.
//  * A pipe that can't move any further ends with a ball cap, and a new
//    pipe starts in a free cell in a new color.
//  * Joints are Elbow, Ball, Mixed (random per joint) or Cycle (alternates
//    each time the screen is cleared).
//  * Single or Multiple pipes at once; Solid (colored) or Textured surface.
//  * When the grid is full enough, the screen is cleared and it starts again.
#include "../../common/mesh.h"
#include "resource.h"
#include <commctrl.h>
#include <commdlg.h>

// ---------------------------------------------------------------------------
// Settings (registry value names are the ones sspipes.scr uses)
// ---------------------------------------------------------------------------
enum { JOINT_ELBOW, JOINT_BALL, JOINT_MIXED, JOINT_CYCLE };

static DWORD   g_speed;        // "Speed"        0..100
static DWORD   g_jointType;    // "Joint Type"   JOINT_*
static DWORD   g_textured;     // "Textured"     0 = solid, 1 = textured
static DWORD   g_multiPipes;   // "MultiPipes"   0 = single, 1 = multiple
static DWORD   g_tessel;       // "Tessel Factor" 0..4
static wchar_t g_textureName[MAX_PATH];  // "Texture Name" (empty = default)

const wchar_t* RegistryName() { return L"Pipes"; }

void LoadSettings() {
    g_speed      = RegReadDword(L"Speed", 50);
    g_jointType  = RegReadDword(L"Joint Type", JOINT_CYCLE);
    g_textured   = RegReadDword(L"Textured", 0);
    g_multiPipes = RegReadDword(L"MultiPipes", 1);
    g_tessel     = RegReadDword(L"Tessel Factor", 2);
    RegReadString(L"Texture Name", g_textureName, MAX_PATH, L"");
    if (g_speed > 100) g_speed = 100;
    if (g_jointType > JOINT_CYCLE) g_jointType = JOINT_CYCLE;
    if (g_tessel > 4) g_tessel = 4;
}

static void SaveSettings() {
    RegWriteDword(L"Speed", g_speed);
    RegWriteDword(L"Joint Type", g_jointType);
    RegWriteDword(L"Textured", g_textured);
    RegWriteDword(L"MultiPipes", g_multiPipes);
    RegWriteDword(L"Tessel Factor", g_tessel);
    RegWriteDword(L"Default Texture", g_textureName[0] == 0);
    RegWriteString(L"Texture Name", g_textureName);
}

// ---------------------------------------------------------------------------
// Configuration dialog (layout follows the original "3D Pipes Settings")
// ---------------------------------------------------------------------------
static bool ChooseTexture(HWND dlg) {
    wchar_t filter[256], title[128], file[MAX_PATH];
    LoadStringW(g_hInst, IDS_TEXTURE_FILTER, filter, 256);
    LoadStringW(g_hInst, IDS_TEXTURE_TITLE, title, 128);
    for (wchar_t* p = filter; *p; p++) if (*p == L'#') *p = 0;   // '#' separators, as in the original
    lstrcpynW(file, g_textureName, MAX_PATH);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = dlg;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) return false;
    lstrcpynW(g_textureName, file, MAX_PATH);
    return true;
}

static void UpdateTextureButton(HWND dlg) {
    EnableWindow(GetDlgItem(dlg, IDC_CHOOSE_TEXTURE), IsDlgButtonChecked(dlg, IDC_TEXTURED) == BST_CHECKED);
}

static INT_PTR CALLBACK ConfigDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        CheckRadioButton(dlg, IDC_SINGLE, IDC_MULTIPLE, g_multiPipes ? IDC_MULTIPLE : IDC_SINGLE);
        CheckRadioButton(dlg, IDC_SOLID, IDC_TEXTURED, g_textured ? IDC_TEXTURED : IDC_SOLID);
        for (int i = 0; i < 4; i++) {
            wchar_t name[32]; LoadStringW(g_hInst, IDS_JOINT_ELBOW + i, name, 32);
            SendDlgItemMessageW(dlg, IDC_JOINT_TYPE, CB_ADDSTRING, 0, (LPARAM)name);
        }
        SendDlgItemMessageW(dlg, IDC_JOINT_TYPE, CB_SETCURSEL, g_jointType, 0);
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETTICFREQ, 10, 0);
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETPOS, TRUE, g_speed);
        UpdateTextureButton(dlg);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SOLID: case IDC_TEXTURED: UpdateTextureButton(dlg); return TRUE;
        case IDC_CHOOSE_TEXTURE: ChooseTexture(dlg); return TRUE;
        case IDC_DISPLAY_SETTINGS: ShowDisplaySettings(dlg); return TRUE;
        case IDOK:
            g_multiPipes = IsDlgButtonChecked(dlg, IDC_MULTIPLE) == BST_CHECKED;
            g_textured = IsDlgButtonChecked(dlg, IDC_TEXTURED) == BST_CHECKED;
            g_jointType = (DWORD)SendDlgItemMessageW(dlg, IDC_JOINT_TYPE, CB_GETCURSEL, 0, 0);
            g_speed = (DWORD)SendDlgItemMessageW(dlg, IDC_SPEED, TBM_GETPOS, 0, 0);
            SaveSettings();
            EndDialog(dlg, IDOK);
            return TRUE;
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
// Scene
// ---------------------------------------------------------------------------
static const int kDirs[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
static const unsigned char kColors[][4] = {
    {220, 40, 40,255}, { 40,200, 40,255}, { 50, 80,230,255}, {230,210, 40,255},
    {220, 60,220,255}, { 40,210,210,255}, {240,140, 30,255}, {230,230,230,255},
};
static const float kPipeR  = 0.2f;    // pipe radius (cell = 1 unit)
static const float kElbowR = 0.3f;    // bend radius of elbow joints
static const float kBallR  = 0.3f;    // ball joint / end cap radius

struct Pipe {
    int  x, y, z;
    int  dir;            // -1 before first move
    bool alive;
    unsigned char color[4];
};

class PipesScene : public Scene {
    int nx = 0, ny = 0, nz = 0;
    std::vector<unsigned char> occupied;
    std::vector<Pipe> pipes;
    Mesh mesh;
    int slices = 16;
    int roundJoint = JOINT_ELBOW;      // joint style for this round (Cycle advances it)
    int cycleIndex = 0;
    float stepTimer = 0, stepInterval = 0.05f;
    float fade = -1;                   // >= 0 while fading out
    float yaw = 0, pitch = 0;
    int width = 1, height = 1;
    int cellsFilled = 0, startsFailed = 0;
    GLuint texture = 0;
    bool textured = false;

    unsigned char& Cell(int x, int y, int z) { return occupied[(z * ny + y) * nx + x]; }
    bool Free(int x, int y, int z) {
        return x >= 0 && y >= 0 && z >= 0 && x < nx && y < ny && z < nz && !Cell(x, y, z);
    }
    Vec3 Center(int x, int y, int z) const {
        return Vec3(x - (nx - 1) * 0.5f, y - (ny - 1) * 0.5f, z - (nz - 1) * 0.5f);
    }
    static Vec3 DirVec(int d) { return Vec3((float)kDirs[d][0], (float)kDirs[d][1], (float)kDirs[d][2]); }

    // --- geometry -----------------------------------------------------------
    void Ring(const Vec3& c, const Vec3& u, const Vec3& v, Vec3* p, Vec3* n) {
        for (int i = 0; i <= slices; i++) {
            float a = 2 * kPi * i / slices;
            n[i] = u * cosf(a) + v * sinf(a);
            p[i] = c + n[i] * kPipeR;
        }
    }

    void AddCylinder(const Vec3& a, const Vec3& b, const unsigned char* rgba) {
        Vec3 axis = b - a;
        float len = Length(axis);
        if (len < 1e-4f) return;
        axis = axis * (1.0f / len);
        Vec3 u = Perpendicular(axis), v = Cross(axis, u);
        std::vector<Vec3> p0(slices + 1), n0(slices + 1), p1(slices + 1), n1(slices + 1);
        Ring(a, u, v, p0.data(), n0.data());
        Ring(b, u, v, p1.data(), n1.data());
        float vt = len / (2 * kPi * kPipeR);       // keep texels square
        mesh.AddRingStrip(p0.data(), n0.data(), 0, p1.data(), n1.data(), vt, slices, rgba);
    }

    // Quarter torus from P - din*R to P + dout*R (see docs/ANALYSIS.md).
    void AddElbow(const Vec3& P, const Vec3& din, const Vec3& dout, const unsigned char* rgba) {
        Vec3 O = P - din * kElbowR + dout * kElbowR;
        Vec3 u = Cross(din, dout);                 // normal of the bend plane
        const int steps = slices / 2 + 2;
        std::vector<Vec3> p0(slices + 1), n0(slices + 1), p1(slices + 1), n1(slices + 1);
        float arcStep = (kPi / 2) * kElbowR / steps / (2 * kPi * kPipeR);
        for (int s = 0; s <= steps; s++) {
            float t = (kPi / 2) * s / steps;
            Vec3 c = O - dout * (kElbowR * cosf(t)) + din * (kElbowR * sinf(t));
            Vec3 tangent = dout * sinf(t) + din * cosf(t);
            Vec3 v = Cross(tangent, u);
            Ring(c, u, v, p1.data(), n1.data());
            if (s > 0)
                mesh.AddRingStrip(p0.data(), n0.data(), (s - 1) * arcStep, p1.data(), n1.data(), s * arcStep, slices, rgba);
            p0.swap(p1); n0.swap(n1);
        }
    }

    void AddBall(const Vec3& c, const unsigned char* rgba) { mesh.AddSphere(c, kBallR, slices, rgba); }

    // --- simulation -----------------------------------------------------------
    void NewRound() {
        mesh.Clear();
        std::fill(occupied.begin(), occupied.end(), 0);
        pipes.clear();
        cellsFilled = 0;
        startsFailed = 0;
        fade = -1;
        if (g_jointType == JOINT_CYCLE) roundJoint = cycleIndex++ % 3;  // elbow, ball, mixed...
        else roundJoint = (int)g_jointType;
        yaw = RandF(-35, 35);
        pitch = RandF(-20, 20);
        StartPipe();
    }

    bool StartPipe() {
        for (int tries = 0; tries < 50; tries++) {
            int x = RandI(0, nx - 1), y = RandI(0, ny - 1), z = RandI(0, nz - 1);
            if (!Free(x, y, z)) continue;
            Pipe p;
            p.x = x; p.y = y; p.z = z; p.dir = -1; p.alive = true;
            const unsigned char* c = textured ? kColors[7] : kColors[RandI(0, 6)];
            memcpy(p.color, c, 4);
            Cell(x, y, z) = 1;
            cellsFilled++;
            AddBall(Center(x, y, z), p.color);     // start cap
            pipes.push_back(p);
            return true;
        }
        startsFailed++;
        return false;
    }

    void StepPipe(Pipe& p) {
        int cand[6], n = 0;
        for (int d = 0; d < 6; d++)
            if (Free(p.x + kDirs[d][0], p.y + kDirs[d][1], p.z + kDirs[d][2])) cand[n++] = d;

        Vec3 P = Center(p.x, p.y, p.z);
        if (n == 0) {
            // Dead end: finish the incoming half segment and cap with a ball.
            if (p.dir >= 0) AddCylinder(P - DirVec(p.dir) * 0.5f, P, p.color);
            AddBall(P, p.color);
            p.alive = false;
            return;
        }

        int out = cand[RandI(0, n - 1)];
        bool canGoStraight = false;
        for (int i = 0; i < n; i++) if (cand[i] == p.dir) canGoStraight = true;
        if (canGoStraight && RandF(0, 1) < 0.75f) out = p.dir;   // pipes prefer long runs

        Vec3 dout = DirVec(out);
        if (p.dir < 0) {
            AddCylinder(P, P + dout * 0.5f, p.color);
        } else if (out == p.dir) {
            AddCylinder(P - dout * 0.5f, P + dout * 0.5f, p.color);
        } else {
            Vec3 din = DirVec(p.dir);
            bool elbow = roundJoint == JOINT_ELBOW || (roundJoint == JOINT_MIXED && RandI(0, 1) == 0);
            if (elbow) {
                AddCylinder(P - din * 0.5f, P - din * kElbowR, p.color);
                AddElbow(P, din, dout, p.color);
                AddCylinder(P + dout * kElbowR, P + dout * 0.5f, p.color);
            } else {
                AddCylinder(P - din * 0.5f, P, p.color);
                AddBall(P, p.color);
                AddCylinder(P, P + dout * 0.5f, p.color);
            }
        }
        p.x += kDirs[out][0]; p.y += kDirs[out][1]; p.z += kDirs[out][2];
        p.dir = out;
        Cell(p.x, p.y, p.z) = 1;
        cellsFilled++;
    }

    void Step() {
        for (auto& p : pipes) if (p.alive) StepPipe(p);
        int alive = 0;
        for (auto& p : pipes) alive += p.alive;
        size_t maxAlive = g_multiPipes ? 5 : 1;
        int total = nx * ny * nz;
        bool full = cellsFilled > total * 2 / 5 || startsFailed > 3;
        if (!full)
            while ((size_t)alive < maxAlive && StartPipe()) alive++;
        if (full && alive == 0 && fade < 0) fade = 0;   // begin the clear
    }

public:
    ~PipesScene() { if (texture) glDeleteTextures(1, &texture); }

    bool Init(int w, int h, bool preview) override {
        slices = preview ? 8 : 8 + (int)g_tessel * 4;
        textured = g_textured != 0;
        if (textured) {
            texture = LoadTextureFromFile(g_textureName);
            if (!texture) {
                // Default texture: brushed-metal stripes.
                static unsigned px[64 * 64];
                for (int y = 0; y < 64; y++)
                    for (int x = 0; x < 64; x++) {
                        int v = 150 + (int)(60 * sinf(x * 0.2f)) + ((x * 7 + y * 13) % 17);
                        if (v > 255) v = 255;
                        int b = v > 200 ? 255 : v + 40;
                        px[y * 64 + x] = 0xFF000000u | (v << 16) | (v << 8) | b;
                    }
                texture = CreateTextureBGRA(px, 64, 64);
            }
        }
        // Stepping interval: 0.25 s (slow) .. 0.008 s (fast).
        stepInterval = 0.25f * powf(0.03f, g_speed / 100.0f);
        Resize(w, h);
        NewRound();
        return true;
    }

    void Resize(int w, int h) override {
        width = w; height = h > 0 ? h : 1;
        int newNx = (int)(14.0f * width / height + 0.5f);
        if (newNx < 6) newNx = 6;
        if (newNx > 40) newNx = 40;
        if (newNx != nx || ny != 14) {
            nx = newNx; ny = 14; nz = 14;
            occupied.assign(nx * ny * nz, 0);
            if (!mesh.verts.empty() || !pipes.empty()) NewRound();
        }
        glViewport(0, 0, width, height);
    }

    void Frame(float dt) override {
        if (fade >= 0) {
            fade += dt;
            if (fade > 1.0f) NewRound();
        } else {
            stepTimer += dt;
            int steps = 0;
            while (stepTimer >= stepInterval && steps < 8) { stepTimer -= stepInterval; Step(); steps++; }
            if (steps == 8) stepTimer = 0;
        }

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)width / height, 1.0, 200.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        float lightDir[4] = { -0.4f, 0.6f, 1.0f, 0 };
        float lightDir2[4] = { 0.6f, -0.3f, 0.5f, 0 };
        float white[4] = { 1, 1, 1, 1 }, dim[4] = { 0.35f, 0.35f, 0.4f, 1 }, amb[4] = { 0.15f, 0.15f, 0.15f, 1 };
        glLightfv(GL_LIGHT0, GL_POSITION, lightDir);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
        glLightfv(GL_LIGHT0, GL_SPECULAR, white);
        glLightfv(GL_LIGHT1, GL_POSITION, lightDir2);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, dim);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);

        float dist = ny * 0.5f / tanf(22.5f * kPi / 180) + nz * 0.5f + 1;
        glTranslatef(0, 0, -dist);
        glRotatef(pitch, 1, 0, 0);
        glRotatef(yaw, 0, 1, 0);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        glEnable(GL_NORMALIZE);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        float spec[4] = { 0.8f, 0.8f, 0.8f, 1 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50);
        glShadeModel(GL_SMOOTH);
        if (textured && texture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        }
        mesh.Draw(textured && texture);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);

        if (fade >= 0) {
            // Fade to black before the screen is cleared.
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glMatrixMode(GL_PROJECTION); glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);  glLoadIdentity();
            glColor4f(0, 0, 0, fade > 1 ? 1 : fade);
            glBegin(GL_QUADS);
            glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
            glEnd();
            glDisable(GL_BLEND);
        }
    }
};

Scene* CreateScene() { return new PipesScene; }
