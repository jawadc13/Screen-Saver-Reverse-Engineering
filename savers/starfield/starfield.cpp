// starfield.cpp - "3D Starfield": flying through a field of stars, built on
// the same framework (and so the same screensaver rules) as 3D Pipes.
#include "../../common/mesh.h"
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
    int width = 1, height = 1;
    float speed = 20, roll = 0, rollRate = 0, t = 0;
    static constexpr float kNear = 1.0f, kFar = 100.0f, kSpread = 40.0f;

    void Respawn(Star& s, bool anywhere) {
        s.x = RandF(-kSpread, kSpread);
        s.y = RandF(-kSpread, kSpread);
        s.z = anywhere ? RandF(-kFar, -kNear) : -kFar;
        switch (g_colorMode) {
        case COLOR_WHITE: s.r = s.g = s.b = 1; break;
        case COLOR_TINTED: { float k = RandF(0, 1); s.r = 0.8f + 0.2f * k; s.g = 0.85f; s.b = 1.0f - 0.2f * k; break; }
        default: { float h = RandF(0, 6); int i = (int)h; float f = h - i;
            float c[6][3] = {{1,f,0},{1-f,1,0},{0,1,f},{0,1-f,1},{f,0,1},{1,0,1-f}};
            s.r = c[i % 6][0]; s.g = c[i % 6][1]; s.b = c[i % 6][2]; break; }
        }
    }

public:
    bool Init(int w, int h, bool preview) override {
        int count = 300 + (int)g_density * 30;
        if (preview) count /= 4;
        stars.resize(count);
        for (auto& s : stars) Respawn(s, true);
        speed = 5 + g_speed * 0.6f;
        rollRate = RandF(-8, 8);
        Resize(w, h);
        return true;
    }

    void Resize(int w, int h) override {
        width = w; height = h > 0 ? h : 1;
        glViewport(0, 0, width, height);
    }

    void Frame(float dt) override {
        t += dt;
        roll += rollRate * dt;
        if (RandF(0, 1) < dt * 0.1f) rollRate = RandF(-8, 8);   // drift the roll now and then

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(60.0, (double)width / height, 0.5, 200.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(roll, 0, 0, 1);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // additive glow
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_POINT_SMOOTH);

        float move = speed * dt;
        float trail = g_warp ? speed * 0.06f : 0.0f;
        float pointSize = height / 400.0f;
        if (pointSize < 1) pointSize = 1;

        glLineWidth(pointSize);
        glBegin(GL_LINES);
        for (auto& s : stars) {
            s.z += move;
            if (s.z > -kNear) Respawn(s, false);
            float bright = 1.0f - (-s.z / kFar);     // fade in from the distance
            bright *= bright;
            glColor4f(s.r, s.g, s.b, 0);
            glVertex3f(s.x, s.y, s.z - trail - 0.05f);
            glColor4f(s.r, s.g, s.b, bright);
            glVertex3f(s.x, s.y, s.z);
        }
        glEnd();

        glPointSize(pointSize * 1.5f);
        glBegin(GL_POINTS);
        for (auto& s : stars) {
            float bright = 1.0f - (-s.z / kFar);
            glColor4f(s.r, s.g, s.b, bright * bright);
            glVertex3f(s.x, s.y, s.z);
        }
        glEnd();
        glDisable(GL_BLEND);
    }
};

Scene* CreateScene() { return new StarfieldScene; }
