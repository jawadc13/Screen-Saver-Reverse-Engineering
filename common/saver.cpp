// saver.cpp - screensaver framework (WinMain, command line, windows, input
// rules, OpenGL setup, settings helpers, shared Display Settings dialog).
//
// Behaviour mirrors the D3DSaver framework used by the XP 3D savers; every
// rule implemented here is documented in docs/ANALYSIS.md.
#include "saver.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// Random numbers. Each monitor window has its own generator state so that,
// with "same image on all monitors", every screen evolves identically.
// ---------------------------------------------------------------------------
static uint32_t  g_defaultRng = 0x12345678u;
static uint32_t* g_rng = &g_defaultRng;

static uint32_t NextRand() {
    // xorshift32
    uint32_t x = *g_rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *g_rng = x ? x : 0x9E3779B9u;
    return *g_rng;
}
float RandF(float lo, float hi) { return lo + (hi - lo) * (NextRand() >> 8) * (1.0f / 16777216.0f); }
int   RandI(int lo, int hi)     { return lo + (int)(NextRand() % (uint32_t)(hi - lo + 1)); }

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------
static void KeyPath(wchar_t* out, size_t cch, const wchar_t* subkey) {
    if (subkey) _snwprintf(out, cch, L"Software\\ScreenSaverRE\\%s\\%s", RegistryName(), subkey);
    else        _snwprintf(out, cch, L"Software\\ScreenSaverRE\\%s", RegistryName());
    out[cch - 1] = 0;
}

DWORD RegReadDword(const wchar_t* value, DWORD def, const wchar_t* subkey) {
    wchar_t path[260]; KeyPath(path, 260, subkey);
    HKEY k; DWORD v = def, cb = sizeof(v), type;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, value, nullptr, &type, (BYTE*)&v, &cb) != ERROR_SUCCESS || type != REG_DWORD)
            v = def;
        RegCloseKey(k);
    }
    return v;
}

void RegWriteDword(const wchar_t* value, DWORD data, const wchar_t* subkey) {
    wchar_t path[260]; KeyPath(path, 260, subkey);
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, value, 0, REG_DWORD, (const BYTE*)&data, sizeof(data));
        RegCloseKey(k);
    }
}

void RegReadString(const wchar_t* value, wchar_t* buf, DWORD cch, const wchar_t* def) {
    wchar_t path[260]; KeyPath(path, 260, nullptr);
    lstrcpynW(buf, def, cch);
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        DWORD cb = cch * sizeof(wchar_t), type;
        if (RegQueryValueExW(k, value, nullptr, &type, (BYTE*)buf, &cb) != ERROR_SUCCESS || type != REG_SZ)
            lstrcpynW(buf, def, cch);
        buf[cch - 1] = 0;
        RegCloseKey(k);
    }
}

void RegWriteString(const wchar_t* value, const wchar_t* data) {
    wchar_t path[260]; KeyPath(path, 260, nullptr);
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, value, 0, REG_SZ, (const BYTE*)data, (lstrlenW(data) + 1) * sizeof(wchar_t));
        RegCloseKey(k);
    }
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
GLuint CreateTextureBGRA(const unsigned* pixels, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // gluBuild2DMipmaps rescales non-power-of-two images for GL 1.1 drivers.
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels);
    return tex;
}

// Minimal GDI+ flat API declarations (gdiplus.dll ships with XP and later).
namespace gdip {
struct StartupInput { UINT32 version; void* debugCb; BOOL noBgThread; BOOL noCodecs; };
struct Rect { INT x, y, w, h; };
struct BitmapData { UINT w, h; INT stride; INT format; void* scan0; UINT_PTR reserved; };
}
extern "C" {
int  WINAPI GdiplusStartup(ULONG_PTR*, const gdip::StartupInput*, void*);
void WINAPI GdiplusShutdown(ULONG_PTR);
int  WINAPI GdipCreateBitmapFromFile(const WCHAR*, void**);
int  WINAPI GdipGetImageWidth(void*, UINT*);
int  WINAPI GdipGetImageHeight(void*, UINT*);
int  WINAPI GdipBitmapLockBits(void*, const gdip::Rect*, UINT, INT, gdip::BitmapData*);
int  WINAPI GdipBitmapUnlockBits(void*, gdip::BitmapData*);
int  WINAPI GdipDisposeImage(void*);
}

GLuint LoadTextureFromFile(const wchar_t* path) {
    if (!path || !*path) return 0;
    gdip::StartupInput in = { 1, nullptr, FALSE, FALSE };
    ULONG_PTR token;
    if (GdiplusStartup(&token, &in, nullptr) != 0) return 0;
    GLuint tex = 0;
    void* bmp = nullptr;
    if (GdipCreateBitmapFromFile(path, &bmp) == 0 && bmp) {
        UINT w = 0, h = 0;
        GdipGetImageWidth(bmp, &w);
        GdipGetImageHeight(bmp, &h);
        gdip::Rect r = { 0, 0, (INT)w, (INT)h };
        gdip::BitmapData bd = {};
        const INT PixelFormat32bppARGB = 0x26200A, ImageLockModeRead = 1;
        if (w && h && GdipBitmapLockBits(bmp, &r, ImageLockModeRead, PixelFormat32bppARGB, &bd) == 0) {
            std::vector<unsigned> px((size_t)w * h);
            for (UINT y = 0; y < h; y++)
                memcpy(&px[(size_t)y * w], (BYTE*)bd.scan0 + (INT_PTR)y * bd.stride, w * 4);
            GdipBitmapUnlockBits(bmp, &bd);
            tex = CreateTextureBGRA(px.data(), (int)w, (int)h);
        }
        GdipDisposeImage(bmp);
    }
    GdiplusShutdown(token);
    return tex;
}

// ---------------------------------------------------------------------------
// Monitors
// ---------------------------------------------------------------------------
struct MonitorInfo { HMONITOR hmon; RECT rc; wchar_t device[CCHDEVICENAME]; bool primary; };

static BOOL CALLBACK EnumMonProc(HMONITOR hm, HDC, LPRECT, LPARAM lp) {
    auto* v = (std::vector<MonitorInfo>*)lp;
    MONITORINFOEXW mi = {}; mi.cbSize = sizeof(mi);
    GetMonitorInfoW(hm, &mi);
    MonitorInfo m = {};
    m.hmon = hm; m.rc = mi.rcMonitor; m.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    lstrcpynW(m.device, mi.szDevice, CCHDEVICENAME);
    v->push_back(m);
    return TRUE;
}

static std::vector<MonitorInfo> EnumMonitors() {
    std::vector<MonitorInfo> v;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonProc, (LPARAM)&v);
    // Keep the primary monitor first, like D3DSaver's "Screen 1".
    for (size_t i = 1; i < v.size(); i++)
        if (v[i].primary) { MonitorInfo t = v[0]; v[0] = v[i]; v[i] = t; }
    if (v.empty()) {
        MonitorInfo m = {};
        SetRect(&m.rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        m.primary = true;
        lstrcpyW(m.device, L"DISPLAY");
        v.push_back(m);
    }
    return v;
}

static void ScreenKey(wchar_t* out, int index) { wsprintfW(out, L"Screen %d", index + 1); }

// ---------------------------------------------------------------------------
// Shared "Display Settings" dialog (the original has per-adapter tabs with
// "Display screen saver / Display nothing on this monitor").
// ---------------------------------------------------------------------------
struct DisplayDlgState { std::vector<MonitorInfo> mons; std::vector<DWORD> black; DWORD same; int sel; };

static void DisplayDlgShow(HWND dlg, DisplayDlgState* s) {
    int i = s->sel;
    const MonitorInfo& m = s->mons[i];
    wchar_t info[256];
    wsprintfW(info, L"Name: %s%s\r\nMode: %d by %d\r\nRendering: OpenGL",
              m.device, m.primary ? L" (primary)" : L"",
              m.rc.right - m.rc.left, m.rc.bottom - m.rc.top);
    SetDlgItemTextW(dlg, IDC_MONITOR_INFO, info);
    CheckRadioButton(dlg, IDC_SHOW_SAVER, IDC_LEAVE_BLACK, s->black[i] ? IDC_LEAVE_BLACK : IDC_SHOW_SAVER);
}

static INT_PTR CALLBACK DisplayDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* s = (DisplayDlgState*)GetWindowLongPtrW(dlg, DWLP_USER);
    switch (msg) {
    case WM_INITDIALOG: {
        s = (DisplayDlgState*)lp;
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        for (size_t i = 0; i < s->mons.size(); i++) {
            wchar_t name[64]; wsprintfW(name, L"Monitor %d", (int)i + 1);
            SendDlgItemMessageW(dlg, IDC_MONITOR_LIST, LB_ADDSTRING, 0, (LPARAM)name);
        }
        SendDlgItemMessageW(dlg, IDC_MONITOR_LIST, LB_SETCURSEL, 0, 0);
        CheckDlgButton(dlg, IDC_SAME_ON_ALL, s->same ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(dlg, IDC_SAME_ON_ALL), s->mons.size() > 1);
        DisplayDlgShow(dlg, s);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_MONITOR_LIST:
            if (HIWORD(wp) == LBN_SELCHANGE) {
                int sel = (int)SendDlgItemMessageW(dlg, IDC_MONITOR_LIST, LB_GETCURSEL, 0, 0);
                if (sel >= 0) { s->sel = sel; DisplayDlgShow(dlg, s); }
            }
            return TRUE;
        case IDC_SHOW_SAVER:  s->black[s->sel] = 0; return TRUE;
        case IDC_LEAVE_BLACK: s->black[s->sel] = 1; return TRUE;
        case IDOK: {
            s->same = IsDlgButtonChecked(dlg, IDC_SAME_ON_ALL) == BST_CHECKED;
            RegWriteDword(L"AllScreensSame", s->same);
            for (size_t i = 0; i < s->mons.size(); i++) {
                wchar_t key[32]; ScreenKey(key, (int)i);
                RegWriteDword(L"Leave Black", s->black[i], key);
            }
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

void ShowDisplaySettings(HWND parent) {
    DisplayDlgState s;
    s.mons = EnumMonitors();
    s.sel = 0;
    s.same = RegReadDword(L"AllScreensSame", 0);
    for (size_t i = 0; i < s.mons.size(); i++) {
        wchar_t key[32]; ScreenKey(key, (int)i);
        s.black.push_back(RegReadDword(L"Leave Black", 0, key));
    }
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_DISPLAY_SETTINGS), parent, DisplayDlgProc, (LPARAM)&s);
}

void ErrorBox(HWND parent, UINT id) {
    wchar_t title[128], text[512];
    LoadStringW(g_hInst, IDS_DESCRIPTION, title, 128);
    if (!LoadStringW(g_hInst, id, text, 512)) LoadStringW(g_hInst, IDS_ERR_GENERIC, text, 512);
    MessageBoxW(parent, text, title, MB_OK | MB_ICONERROR);
}

// ---------------------------------------------------------------------------
// Saver runtime
// ---------------------------------------------------------------------------
enum SaverMode { SM_CONFIG, SM_FULL, SM_PREVIEW, SM_PASSWORD, SM_TEST };

struct SaverWindow {
    HWND     hwnd = nullptr;
    HDC      hdc = nullptr;
    HGLRC    hglrc = nullptr;
    Scene*   scene = nullptr;   // null => black window / failed preview
    uint32_t rng = 0;
    int      width = 0, height = 0;
    bool     failed = false;
};

static SaverMode                 g_mode;
static HWND                      g_parent;
static std::vector<SaverWindow*> g_windows;
static bool                      g_quitting;
static bool                      g_checkingPassword;
static int                       g_mouseMoves;
static POINT                     g_lastMouse = { -1, -1 };
static bool                      g_isWin9x;

static bool Win9xPasswordOk(HWND hwnd) {
    // Windows 9x/Me only: NT-based Windows enforces the password itself
    // through the "On resume, password protect" setting.
    if (!g_isWin9x) return true;
    HKEY k; DWORD use = 0, cb = sizeof(use);
    if (RegOpenKeyW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, L"ScreenSaveUsePassword", nullptr, nullptr, (BYTE*)&use, &cb);
        RegCloseKey(k);
    }
    if (!use) return true;
    HMODULE cpl = LoadLibraryW(L"PASSWORD.CPL");
    if (!cpl) return true;
    typedef BOOL (WINAPI *VERIFYPWDPROC)(HWND);
    auto verify = (VERIFYPWDPROC)GetProcAddress(cpl, "VerifyScreenSavePwd");
    BOOL ok = TRUE;
    if (verify) {
        g_checkingPassword = true;
        ShowCursor(TRUE);
        ok = verify(hwnd);
        ShowCursor(FALSE);
        g_checkingPassword = false;
        g_mouseMoves = 0;
    }
    FreeLibrary(cpl);
    return ok != FALSE;
}

// Any user input ends the saver (InterruptSaver in D3DSaver).
static void InterruptSaver(HWND hwnd) {
    if (g_quitting || g_checkingPassword) return;
    if (g_mode != SM_FULL && g_mode != SM_TEST) return;
    if (!Win9xPasswordOk(hwnd)) return;
    g_quitting = true;
    for (auto* w : g_windows) PostMessageW(w->hwnd, WM_CLOSE, 0, 0);
}

static void PaintNoPreview(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(dc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    wchar_t text[128];
    LoadStringW(g_hInst, IDS_NO_PREVIEW, text, 128);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 0));
    DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK SaverWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* w = (SaverWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    bool full = (g_mode == SM_FULL || g_mode == SM_TEST);

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)lp)->lpCreateParams);
        return 0;

    case WM_SETCURSOR:
        if (full && !g_checkingPassword) { SetCursor(nullptr); return TRUE; }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        if (!w || !w->scene) {
            if (g_mode == SM_PREVIEW) { PaintNoPreview(hwnd); return 0; }
            PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
            FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
            EndPaint(hwnd, &ps);
            return 0;
        }
        ValidateRect(hwnd, nullptr);
        return 0;

    case WM_SIZE:
        if (w) { w->width = LOWORD(lp); w->height = HIWORD(lp); }
        if (w && w->scene && w->width > 0 && w->height > 0) {
            wglMakeCurrent(w->hdc, w->hglrc);
            w->scene->Resize(w->width, w->height);
        }
        return 0;

    // --- Input rules: anything ends a full-screen saver ------------------
    case WM_MOUSEMOVE:
        if (full) {
            POINT p = { (short)LOWORD(lp), (short)HIWORD(lp) };
            ClientToScreen(hwnd, &p);
            if (p.x != g_lastMouse.x || p.y != g_lastMouse.y) {
                g_lastMouse = p;
                // D3DSaver tolerates a few move messages: windows receive
                // synthetic WM_MOUSEMOVEs when they appear and the mouse may
                // jitter. More than 5 real moves means the user is back.
                if (++g_mouseMoves > 5) InterruptSaver(hwnd);
            }
        }
        break;

    case WM_KEYDOWN: case WM_SYSKEYDOWN:
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        if (full) { InterruptSaver(hwnd); return 0; }
        break;

    case WM_ACTIVATEAPP:
        // Losing activation (e.g. Alt+Tab, another app grabbing focus) ends it.
        if (full && !wp && !g_checkingPassword) InterruptSaver(hwnd);
        break;

    case WM_POWERBROADCAST:
        if (full && wp == PBT_APMSUSPEND) InterruptSaver(hwnd);
        break;

    case WM_SYSCOMMAND:
        if (full) {
            switch (wp & 0xFFF0) {
            case SC_SCREENSAVE:   // already running - don't start another
            case SC_NEXTWINDOW:
            case SC_PREVWINDOW:
            case SC_KEYMENU:
            case SC_CLOSE:
                return 0;
            // SC_MONITORPOWER falls through to DefWindowProc so the system
            // can still power the monitor down.
            }
        }
        break;

    case WM_CLOSE:
        if (full && !g_quitting) {
            // Closing one monitor's window closes them all.
            InterruptSaver(hwnd);
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        // Preview: the Display Properties dialog destroys our parent (and so
        // us) when the user picks another saver or closes the dialog.
        g_quitting = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool SetupGL(SaverWindow* w) {
    w->hdc = GetDC(w->hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(w->hdc, &pfd);
    if (!pf || !SetPixelFormat(w->hdc, pf, &pfd)) { ErrorBox(nullptr, IDS_ERR_NO_PIXEL_FORMAT); return false; }
    w->hglrc = wglCreateContext(w->hdc);
    if (!w->hglrc || !wglMakeCurrent(w->hdc, w->hglrc)) { ErrorBox(nullptr, IDS_ERR_CREATE_DEVICE); return false; }

    // Sync to vertical retrace when available (D3DPRESENT_INTERVAL_ONE).
    typedef BOOL (WINAPI *SWAPPROC)(int);
    auto swapInterval = (SWAPPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInterval) swapInterval(1);
    return true;
}

static void InitScene(SaverWindow* w) {
    RECT rc; GetClientRect(w->hwnd, &rc);
    w->width = rc.right; w->height = rc.bottom;
    if (!SetupGL(w)) { w->failed = true; return; }
    g_rng = &w->rng;
    w->scene = CreateScene();
    if (!w->scene || !w->scene->Init(w->width, w->height, g_mode == SM_PREVIEW)) {
        delete w->scene; w->scene = nullptr;
        w->failed = true;
        if (g_mode != SM_PREVIEW) ErrorBox(nullptr, IDS_ERR_INIT);
        return;
    }
    w->scene->Resize(w->width, w->height);
}

static void RegisterSaverClass() {
    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = SaverWndProc;
    wc.hInstance = g_hInst;
    wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_MAIN));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"D3DSaverWndClass";  // same class name as the original
    RegisterClassW(&wc);
}

static int RunSaver() {
    RegisterSaverClass();
    LoadSettings();

    uint32_t seed = (uint32_t)GetTickCount() ^ (uint32_t)GetCurrentProcessId() * 2654435761u;
    if (!seed) seed = 1;
    bool same = RegReadDword(L"AllScreensSame", 0) != 0;

    wchar_t title[128];
    LoadStringW(g_hInst, IDS_DESCRIPTION, title, 128);

    if (g_mode == SM_PREVIEW) {
        RECT rc; GetClientRect(g_parent, &rc);
        auto* w = new SaverWindow;
        w->rng = seed;
        g_windows.push_back(w);
        w->hwnd = CreateWindowExW(0, L"D3DSaverWndClass", title, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                  0, 0, rc.right, rc.bottom, g_parent, nullptr, g_hInst, w);
        if (!w->hwnd) return 1;
        // The preview shouldn't steal CPU from the Display Properties dialog.
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        InitScene(w);
    } else {
        // Windows 9x: tell the system a saver runs so Ctrl+Alt+Del and
        // Alt+Tab are disabled while the password dialog is in effect.
        if (g_isWin9x) { BOOL old; SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, TRUE, &old, 0); }

        auto mons = EnumMonitors();
        for (size_t i = 0; i < mons.size(); i++) {
            auto* w = new SaverWindow;
            w->rng = same ? seed : seed + (uint32_t)i * 0x9E3779B9u;
            if (!w->rng) w->rng = 1;
            const RECT& r = mons[i].rc;
            w->hwnd = CreateWindowExW(WS_EX_TOPMOST | (g_mode == SM_FULL ? WS_EX_TOOLWINDOW : 0),
                                      L"D3DSaverWndClass", title, WS_POPUP | WS_VISIBLE,
                                      r.left, r.top, r.right - r.left, r.bottom - r.top,
                                      nullptr, nullptr, g_hInst, w);
            if (!w->hwnd) { delete w; continue; }
            g_windows.push_back(w);
            wchar_t key[32]; ScreenKey(key, (int)i);
            if (!RegReadDword(L"Leave Black", 0, key)) InitScene(w);  // else stays black
        }
        if (g_windows.empty()) return 1;
        SetForegroundWindow(g_windows[0]->hwnd);
        SetFocus(g_windows[0]->hwnd);
        // Hide the cursor for the whole session.
        ShowCursor(FALSE);
    }

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    MSG msg;
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto done;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (g_quitting) { Sleep(1); continue; }

        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        last = now;
        if (dt > 0.1f) dt = 0.1f;   // avoid big jumps after stalls

        bool drew = false;
        for (auto* w : g_windows) {
            if (!w->scene || w->width <= 0 || w->height <= 0) continue;
            wglMakeCurrent(w->hdc, w->hglrc);
            g_rng = &w->rng;
            w->scene->Frame(dt);
            SwapBuffers(w->hdc);
            drew = true;
        }
        if (!drew) WaitMessage();
        else if (g_mode == SM_PREVIEW) Sleep(15);   // ~ 30-60 fps is plenty for the preview
    }
done:
    for (auto* w : g_windows) {
        if (w->hglrc) {
            wglMakeCurrent(w->hdc, w->hglrc);
            delete w->scene;
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(w->hglrc);
        }
        delete w;
    }
    g_windows.clear();
    if (g_mode != SM_PREVIEW) {
        ShowCursor(TRUE);
        if (g_isWin9x) { BOOL old; SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, FALSE, &old, 0); }
    }
    return 0;
}

// /a <hwnd> - Windows 9x "Change password" button.
static void ChangePassword(HWND parent) {
    HMODULE mpr = LoadLibraryW(L"MPR.DLL");
    if (!mpr) return;
    typedef DWORD (WINAPI *PWCHGPROC)(LPCSTR, HWND, DWORD, LPVOID);
    auto change = (PWCHGPROC)GetProcAddress(mpr, "PwdChangePasswordA");
    if (change) change("SCRSAVE", parent, 0, nullptr);
    FreeLibrary(mpr);
}

// ---------------------------------------------------------------------------
// Command line, exactly as Windows invokes screensavers:
//   (none)           -> configuration dialog, no parent
//   /c  or /c:HWND   -> configuration dialog (parent = HWND or foreground)
//   /s               -> run full screen
//   /p HWND, /l HWND -> preview in the given child window
//   /a HWND          -> change password (Windows 9x only)
// Switches may start with '/' or '-', are case-insensitive, and the HWND may
// follow a ':' or whitespace.
// ---------------------------------------------------------------------------
static HWND ParseHwnd(const wchar_t* p) {
    while (*p == L' ' || *p == L'\t' || *p == L':') p++;
    return (HWND)(UINT_PTR)_wcstoui64(p, nullptr, 10);
}

static SaverMode ParseCommandLine(HWND* parent) {
    const wchar_t* p = GetCommandLineW();
    // Skip the program name (possibly quoted).
    if (*p == L'"') { p++; while (*p && *p != L'"') p++; if (*p) p++; }
    else            { while (*p && *p != L' ' && *p != L'\t') p++; }
    while (*p == L' ' || *p == L'\t') p++;

    *parent = nullptr;
    if (*p != L'/' && *p != L'-') return SM_CONFIG;
    wchar_t c = p[1];
    if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    p += 2;
    switch (c) {
    case L's': return SM_FULL;
    case L't': return SM_TEST;     // D3DSaver's windowed debug mode
    case L'p': case L'l':
        *parent = ParseHwnd(p);
        return *parent && IsWindow(*parent) ? SM_PREVIEW : SM_CONFIG;
    case L'a':
        *parent = ParseHwnd(p);
        return SM_PASSWORD;
    case L'c': default:
        *parent = ParseHwnd(p);
        if (!*parent) *parent = GetForegroundWindow();
        return SM_CONFIG;
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g_hInst = hInst;
    OSVERSIONINFOW ov = {}; ov.dwOSVersionInfoSize = sizeof(ov);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    GetVersionExW(&ov);
#pragma GCC diagnostic pop
    g_isWin9x = ov.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS;

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    g_mode = ParseCommandLine(&g_parent);
    switch (g_mode) {
    case SM_CONFIG:
        LoadSettings();
        ShowConfigDialog(g_parent);
        return 0;
    case SM_PASSWORD:
        ChangePassword(g_parent);
        return 0;
    default:
        return RunSaver();
    }
}
