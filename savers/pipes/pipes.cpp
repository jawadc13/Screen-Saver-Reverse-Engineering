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
#include "../../common/saver.h"
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
    Texture* texture = nullptr;
    GpuMesh gpu;                       // pipe geometry on the GPU, appended as it grows
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
        gpu.Reset();
        std::fill(occupied.begin(), occupied.end(), 0);
        pipes.clear();
        cellsFilled = 0;
        startsFailed = 0;
        fade = -1;
        if (g_jointType == JOINT_CYCLE) roundJoint = cycleIndex++ % 3;  // elbow, ball, mixed...
        else roundJoint = (int)g_jointType;
        // Gentler angles on stretched grids so the pipes stay large.
        float stretch = (float)(nx > ny ? nx : ny) / 14.0f;
        yaw = RandF(-35, 35) / stretch;
        pitch = RandF(-20, 20) / stretch;
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
    ~PipesScene() { delete texture; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        slices = preview ? 8 : 8 + (int)g_tessel * 4;
        textured = g_textured != 0;
        if (textured) {
            std::vector<unsigned> px;
            int tw = 0, th = 0;
            if (LoadImageFile(g_textureName, px, tw, th)) texture = r.CreateTexture(px.data(), tw, th);
            if (!texture) {
                // Default texture: brushed-metal stripes.
                px.resize(64 * 64);
                for (int y = 0; y < 64; y++)
                    for (int x = 0; x < 64; x++) {
                        int v = 150 + (int)(60 * sinf(x * 0.2f)) + ((x * 7 + y * 13) % 17);
                        if (v > 255) v = 255;
                        int b = v > 200 ? 255 : v + 40;
                        px[y * 64 + x] = 0xFF000000u | (v << 16) | (v << 8) | b;
                    }
                texture = r.CreateTexture(px.data(), 64, 64);
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
        // Grid follows the screen's shape: wide for 3440x1440, tall for a
        // portrait 2160x3840, so pipes fill any monitor.
        float aspect = (float)width / height;
        int newNx = 14, newNy = 14;
        if (aspect >= 1) newNx = (int)(14.0f * aspect + 0.5f);
        else             newNy = (int)(14.0f / aspect + 0.5f);
        if (newNx > 40) newNx = 40;
        if (newNy > 40) newNy = 40;
        if (newNx != nx || newNy != ny) {
            nx = newNx; ny = newNy; nz = 14;
            occupied.assign(nx * ny * nz, 0);
            if (!mesh.verts.empty() || !pipes.empty()) NewRound();
        }
    }

    void Frame(Renderer& r, float dt) override {
        if (fade >= 0) {
            fade += dt;
            if (fade > 1.0f) NewRound();
        } else {
            stepTimer += dt;
            int steps = 0;
            while (stepTimer >= stepInterval && steps < 8) { stepTimer -= stepInterval; Step(); steps++; }
            if (steps == 8) stepTimer = 0;
        }

        r.BeginFrame(true);
        // Pull the camera back until all 8 corners of the (rotated) grid are
        // on screen - works for ultrawide, portrait and any camera angle.
        float aspect = (float)width / height;
        float t = tanf(22.5f * kPi / 180);
        Mat4 rot = Mat4::Rotate(pitch, 1, 0, 0) * Mat4::Rotate(yaw, 0, 1, 0);
        float dist = 0;
        for (int c = 0; c < 8; c++) {
            float x = (c & 1 ? 0.5f : -0.5f) * nx, y = (c & 2 ? 0.5f : -0.5f) * ny, z = (c & 4 ? 0.5f : -0.5f) * nz;
            float rx = rot.m[0][0] * x + rot.m[0][1] * y + rot.m[0][2] * z;
            float ry = rot.m[1][0] * x + rot.m[1][1] * y + rot.m[1][2] * z;
            float rz = rot.m[2][0] * x + rot.m[2][1] * y + rot.m[2][2] * z;
            float need = rz + fmaxf(fabsf(rx) / (t * aspect), fabsf(ry) / t);
            if (need > dist) dist = need;
        }
        dist *= 1.03f;   // small margin
        Mat4 view = Mat4::Translate(0, 0, -dist) * rot;
        r.SetCamera(view, Mat4::Perspective(45.0f, aspect, 1.0f, dist + nx + ny + nz));
        r.SetLight(0, Vec3(-0.4f, 0.6f, 1.0f), 1, 1, 1);
        r.SetLight(1, Vec3(0.6f, -0.3f, 0.5f), 0.35f, 0.35f, 0.4f);
        r.SetAmbient(0.15f, 0.15f, 0.15f);

        r.Upload(gpu, mesh);   // only the newly grown pieces are sent
        DrawParams p;
        p.specular[0] = p.specular[1] = p.specular[2] = 0.8f;
        p.shininess = 50;
        p.texture = textured ? texture : nullptr;
        r.Draw(gpu, p);

        if (fade >= 0) r.FullscreenQuad(0, 0, 0, fade > 1 ? 1 : fade);   // fade out before clearing
    }
};

Scene* CreateScene() { return new PipesScene; }
