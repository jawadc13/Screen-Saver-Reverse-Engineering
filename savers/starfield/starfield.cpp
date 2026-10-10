// starfield.cpp - "3D Starfield": flying through a field of stars, built on
// the same framework (and so the same screensaver rules) as 3D Pipes.
#include "../../common/saver.h"
#include "resource.h"
#include <commctrl.h>

enum { COLOR_WHITE, COLOR_TINTED, COLOR_RAINBOW };

static DWORD g_speed;     // "Speed"      0..100
static DWORD g_density;   // "Density"    0..100
static DWORD g_colorMode; // "Color Mode" COLOR_*
static DWORD g_warp;      // "Warp Trails" 0/1

const wchar_t* RegistryName() { return L"Starfield"; }

void LoadSettings() {
    g_speed     = RegReadDword(L"Speed", 40);
    g_density   = RegReadDword(L"Density", 50);
    g_colorMode = RegReadDword(L"Color Mode", COLOR_TINTED);
    g_warp      = RegReadDword(L"Warp Trails", 1);
    if (g_speed > 100) g_speed = 100;
    if (g_density > 100) g_density = 100;
    if (g_colorMode > COLOR_RAINBOW) g_colorMode = COLOR_TINTED;
}

static void SaveSettings() {
    RegWriteDword(L"Speed", g_speed);
    RegWriteDword(L"Density", g_density);
    RegWriteDword(L"Color Mode", g_colorMode);
    RegWriteDword(L"Warp Trails", g_warp);
}

static INT_PTR CALLBACK ConfigDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        for (int i = 0; i < 3; i++) {
            wchar_t name[32]; LoadStringW(g_hInst, IDS_COLOR_WHITE + i, name, 32);
            SendDlgItemMessageW(dlg, IDC_COLOR_MODE, CB_ADDSTRING, 0, (LPARAM)name);
        }
        SendDlgItemMessageW(dlg, IDC_COLOR_MODE, CB_SETCURSEL, g_colorMode, 0);
        CheckDlgButton(dlg, IDC_WARP, g_warp ? BST_CHECKED : BST_UNCHECKED);
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETTICFREQ, 10, 0);
        SendDlgItemMessageW(dlg, IDC_SPEED, TBM_SETPOS, TRUE, g_speed);
        SendDlgItemMessageW(dlg, IDC_DENSITY, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        SendDlgItemMessageW(dlg, IDC_DENSITY, TBM_SETTICFREQ, 10, 0);
        SendDlgItemMessageW(dlg, IDC_DENSITY, TBM_SETPOS, TRUE, g_density);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_DISPLAY_SETTINGS: ShowDisplaySettings(dlg); return TRUE;
        case IDOK:
            g_colorMode = (DWORD)SendDlgItemMessageW(dlg, IDC_COLOR_MODE, CB_GETCURSEL, 0, 0);
            g_warp = IsDlgButtonChecked(dlg, IDC_WARP) == BST_CHECKED;
            g_speed = (DWORD)SendDlgItemMessageW(dlg, IDC_SPEED, TBM_GETPOS, 0, 0);
            g_density = (DWORD)SendDlgItemMessageW(dlg, IDC_DENSITY, TBM_GETPOS, 0, 0);
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

struct Star { float x, y, z; float r, g, b; };

class StarfieldScene : public Scene {
    std::vector<Star> stars;
    std::vector<Vertex> verts;
    int width = 1, height = 1;
    float speed = 20, roll = 0, rollRate = 0;
    static constexpr float kNear = 1.0f, kFar = 100.0f, kSpread = 40.0f;

    void Respawn(Star& s, bool anywhere) {
        // Cover the screen's shape: wider for ultrawide, taller for portrait.
        // The longer side is used both ways so rolling never shows empty corners.
        float aspect = (float)width / height;
        float spread = kSpread * (aspect > 1 ? aspect : 1 / aspect);
        s.x = RandF(-spread, spread);
        s.y = RandF(-spread, spread);
        s.z = anywhere ? RandF(-kFar, -kNear) : -kFar;
        switch (g_colorMode) {
        case COLOR_WHITE: s.r = s.g = s.b = 1; break;
        case COLOR_TINTED: { float k = RandF(0, 1); s.r = 0.8f + 0.2f * k; s.g = 0.85f; s.b = 1.0f - 0.2f * k; break; }
        default: { float h = RandF(0, 6); int i = (int)h; float f = h - i;
            float c[6][3] = {{1,f,0},{1-f,1,0},{0,1,f},{0,1-f,1},{f,0,1},{1,0,1-f}};
            s.r = c[i % 6][0]; s.g = c[i % 6][1]; s.b = c[i % 6][2]; break; }
        }
    }

    void Quad(const float* xy, const float* alpha, const Star& s) {
        static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
        for (int k : idx) {
            Vertex v = { xy[k * 2], xy[k * 2 + 1], 0.5f, 0, 0, 1, 0, 0,
                         (unsigned char)(s.r * 255), (unsigned char)(s.g * 255), (unsigned char)(s.b * 255),
                         (unsigned char)(alpha[k] * 255) };
            verts.push_back(v);
        }
    }

public:
    bool Init(Renderer& r, int w, int h, bool preview) override {
        width = w; height = h > 0 ? h : 1;
        // Star count scales with screen area so a 4K portrait screen looks
        // as dense as a 1080p one.
        float area = (float)width * height / (1920.0f * 1080.0f);
        if (area < 0.25f) area = 0.25f;
        if (area > 4) area = 4;
        int count = (int)((300 + g_density * 30) * area);
        if (preview) count = 300;
        stars.resize(count);
        for (auto& s : stars) Respawn(s, true);
        speed = 5 + g_speed * 0.6f;
        rollRate = RandF(-8, 8);
        return true;
    }

    void Resize(int w, int h) override { width = w; height = h > 0 ? h : 1; }

    void Frame(Renderer& r, float dt) override {
        roll += rollRate * dt;
        if (RandF(0, 1) < dt * 0.1f) rollRate = RandF(-8, 8);   // drift the roll now and then

        // Stars are projected on the CPU and drawn as screen-space quads, so
        // streak thickness is in pixels and identical on every resolution.
        const float f = 1.0f / tanf(30.0f * kPi / 180);          // 60 degree vertical FOV
        const float aspect = (float)width / height;
        const float px = height / 540.0f > 1 ? height / 540.0f : 1;   // line width in pixels
        const float sx = 2.0f / width, sy = 2.0f / height;         // pixels -> clip space
        const float cr = cosf(roll * kPi / 180), sr = sinf(roll * kPi / 180);
        const float trail = g_warp ? speed * 0.06f : 0.0f;

        verts.clear();
        for (auto& s : stars) {
            s.z += speed * dt;
            if (s.z > -kNear) Respawn(s, false);
            float x = s.x * cr - s.y * sr, y = s.x * sr + s.y * cr;
            float bright = 1.0f - (-s.z / kFar);
            bright = bright * (2 - bright);         // ease-out: visible from further away
            // Fade out over the last few units before passing the camera, so
            // stars glide out of view instead of winking off.
            float nearFade = (-s.z - kNear) / 6.0f;
            if (nearFade < 1) bright *= nearFade < 0 ? 0 : nearFade * nearFade * (3 - 2 * nearFade);
            // Head and tail positions in pixels.
            float hz = -s.z, tz = -(s.z - trail - 0.05f);
            float hx = f / aspect * x / hz / sx, hy = f * y / hz / sy;
            float tx = f / aspect * x / tz / sx, ty = f * y / tz / sy;
            if (fabsf(hx) > width || fabsf(hy) > height) continue;
            float dx = hx - tx, dy = hy - ty, len = sqrtf(dx * dx + dy * dy);
            float w = px * (0.6f + 1.4f * bright);   // half-width in pixels
            if (len > 0.5f) {
                float nx = -dy / len * w, ny = dx / len * w;
                float q[8] = { (tx + nx) * sx, (ty + ny) * sy, (hx + nx) * sx, (hy + ny) * sy,
                               (hx - nx) * sx, (hy - ny) * sy, (tx - nx) * sx, (ty - ny) * sy };
                float a[4] = { 0, bright, bright, 0 };
                Quad(q, a, s);
            }
            float d = w * 1.5f;   // bright dot at the head
            float q[8] = { (hx - d) * sx, (hy - d) * sy, (hx + d) * sx, (hy - d) * sy,
                           (hx + d) * sx, (hy + d) * sy, (hx - d) * sx, (hy + d) * sy };
            float a[4] = { bright, bright, bright, bright };
            Quad(q, a, s);
        }

        r.BeginFrame(true);
        r.SetCamera(Mat4::Identity(), Mat4::Identity());
        DrawParams p;
        p.lit = false;
        p.depth = false;
        p.blend = BLEND_ADD;   // overlapping stars glow
        r.Draw(verts.data(), verts.size(), p);
    }
};

Scene* CreateScene() { return new StarfieldScene; }
