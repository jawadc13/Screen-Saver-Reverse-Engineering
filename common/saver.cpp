// saver.cpp - screensaver framework (WinMain, command line, windows, input
// rules, Direct3D setup, settings helpers, shared Display Settings dialog).
//
// Behaviour mirrors the D3DSaver framework used by the XP 3D savers; every
// rule implemented here is documented in docs/ANALYSIS.md.
#include "saver.h"
#include <commctrl.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <mmsystem.h>
#include "theme.h"
#include "styleui.h"
#include "care.h"

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// Random numbers. Each monitor window has its own generator state so that,
// with "same image on all monitors", every screen evolves identically.
// ---------------------------------------------------------------------------
static uint32_t  g_defaultRng = 0x12345678u;
// Thread-local because every monitor renders on its own thread.
static thread_local uint32_t* g_rng = &g_defaultRng;

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
// Images
// ---------------------------------------------------------------------------
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

bool LoadImageFile(const wchar_t* path, std::vector<unsigned>& px, int& outW, int& outH) {
    if (!path || !*path) return false;
    gdip::StartupInput in = { 1, nullptr, FALSE, FALSE };
    ULONG_PTR token;
    if (GdiplusStartup(&token, &in, nullptr) != 0) return false;
    bool ok = false;
    void* bmp = nullptr;
    if (GdipCreateBitmapFromFile(path, &bmp) == 0 && bmp) {
        UINT w = 0, h = 0;
        GdipGetImageWidth(bmp, &w);
        GdipGetImageHeight(bmp, &h);
        gdip::Rect r = { 0, 0, (INT)w, (INT)h };
        gdip::BitmapData bd = {};
        const INT PixelFormat32bppARGB = 0x26200A, ImageLockModeRead = 1;
        if (w && h && GdipBitmapLockBits(bmp, &r, ImageLockModeRead, PixelFormat32bppARGB, &bd) == 0) {
            px.resize((size_t)w * h);
            for (UINT y = 0; y < h; y++)
                memcpy(&px[(size_t)y * w], (BYTE*)bd.scan0 + (INT_PTR)y * bd.stride, w * 4);
            GdipBitmapUnlockBits(bmp, &bd);
            outW = (int)w; outH = (int)h;
            ok = true;
        }
        GdipDisposeImage(bmp);
    }
    GdiplusShutdown(token);
    return ok;
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
struct DisplayDlgState { std::vector<MonitorInfo> mons; std::vector<DWORD> black; DWORD same; DWORD graph; DWORD msaa; int sel; };

// Anti-aliasing choices: combo index <-> MSAA sample count.
static const int kMsaaCounts[4] = { 1, 2, 4, 8 };
static const wchar_t* kMsaaNames[4] = { L"Off", L"2x", L"4x (default)", L"8x" };

static void DisplayDlgShow(HWND dlg, DisplayDlgState* s) {
    int i = s->sel;
    const MonitorInfo& m = s->mons[i];
    wchar_t info[256];
    wsprintfW(info, L"Name: %s%s\r\nMode: %d by %d\r\nRendering: Direct3D 11 (this monitor's GPU)",
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
        CheckDlgButton(dlg, IDC_FRAME_GRAPH, s->graph ? BST_CHECKED : BST_UNCHECKED);
        for (int i = 0; i < 4; i++) {
            SendDlgItemMessageW(dlg, IDC_MSAA, CB_ADDSTRING, 0, (LPARAM)kMsaaNames[i]);
            if ((DWORD)kMsaaCounts[i] == s->msaa) SendDlgItemMessageW(dlg, IDC_MSAA, CB_SETCURSEL, i, 0);
        }
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
        case IDC_STYLE_BUTTON: ShowStyleSettings(dlg); return TRUE;
        case IDC_SHOW_SAVER:  s->black[s->sel] = 0; return TRUE;
        case IDC_LEAVE_BLACK: s->black[s->sel] = 1; return TRUE;
        case IDOK: {
            s->same = IsDlgButtonChecked(dlg, IDC_SAME_ON_ALL) == BST_CHECKED;
            RegWriteDword(L"AllScreensSame", s->same);
            RegWriteDword(L"Show Frame Graph", IsDlgButtonChecked(dlg, IDC_FRAME_GRAPH) == BST_CHECKED);
            {
                int sel = (int)SendDlgItemMessageW(dlg, IDC_MSAA, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 4) RegWriteDword(L"MSAA", kMsaaCounts[sel]);
            }
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

// ---------------------------------------------------------------------------
// "Themes & Effects" dialog (same settings Screensaver Studio edits).
// ---------------------------------------------------------------------------
const wchar_t* SaverFileName() {
    static wchar_t name[MAX_PATH];
    if (!name[0]) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        const wchar_t* b = path;
        for (const wchar_t* q = path; *q; q++) if (*q == L'\\' || *q == L'/') b = q + 1;
        lstrcpynW(name, b, MAX_PATH);
        for (wchar_t* q = name + lstrlenW(name); q > name; q--) if (*q == L'.') { *q = 0; break; }
    }
    return name;
}

void ShowStyleSettings(HWND parent) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    styleui::ShowStyleDialog(parent, path, SaverFileName());
}

void ShowDisplaySettings(HWND parent) {
    DisplayDlgState s;
    s.mons = EnumMonitors();
    s.sel = 0;
    s.same = RegReadDword(L"AllScreensSame", 0);
    s.graph = RegReadDword(L"Show Frame Graph", 0);
    s.msaa = RegReadDword(L"MSAA", 4);
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
    HMONITOR monitor = nullptr;
    Scene*   scene = nullptr;   // null => black window / failed preview
    uint32_t rng = 0;
    int      width = 0, height = 0;   // size the scene was last told about
    volatile LONG newW = 0, newH = 0; // latest size from WM_SIZE (main thread)
    bool     wantScene = false;       // false => "Display nothing" monitor
    volatile LONG failed = 0;
    HANDLE   thread = nullptr;        // this window's render thread
};

static SaverMode                 g_mode;
static HWND                      g_parent;
static std::vector<SaverWindow*> g_windows;
static volatile LONG             g_quitting;
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

// Tells every render thread to finish and waits for them, so no thread is
// still drawing into a window that is about to be destroyed.
static void StopRenderThreads() {
    static bool stopping = false;
    InterlockedExchange(&g_quitting, 1);
    if (stopping) return;          // re-entered from a message pumped below
    stopping = true;
    std::vector<HANDLE> threads;
    for (auto* w : g_windows) if (w->thread) threads.push_back(w->thread);
    // Keep pumping messages while waiting: a graphics driver may need this (window)
    // thread to answer a message before SwapBuffers can return.
    bool sawQuit = false;
    DWORD deadline = GetTickCount() + 5000;
    while (!threads.empty() && (int)(deadline - GetTickCount()) > 0) {
        DWORD r = MsgWaitForMultipleObjects((DWORD)threads.size(), threads.data(), FALSE,
                                            deadline - GetTickCount(), QS_ALLINPUT);
        if (r < WAIT_OBJECT_0 + threads.size()) {
            threads.erase(threads.begin() + (r - WAIT_OBJECT_0));
        } else if (r == WAIT_OBJECT_0 + threads.size()) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { sawQuit = true; continue; }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        } else {
            break;   // timeout or error
        }
    }
    for (auto* w : g_windows)
        if (w->thread) { CloseHandle(w->thread); w->thread = nullptr; }
    if (sawQuit) PostQuitMessage(0);
    stopping = false;
}

// Any user input ends the saver (InterruptSaver in D3DSaver).
static void InterruptSaver(HWND hwnd) {
    if (g_quitting || g_checkingPassword) return;
    if (g_mode != SM_FULL && g_mode != SM_TEST) return;
    if (!Win9xPasswordOk(hwnd)) return;
    StopRenderThreads();
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
        if (!w || !w->wantScene || w->failed) {
            if (g_mode == SM_PREVIEW) { PaintNoPreview(hwnd); return 0; }
            PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
            FillRect(dc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));
            EndPaint(hwnd, &ps);
            return 0;
        }
        ValidateRect(hwnd, nullptr);
        return 0;

    case WM_SIZE:
        // The render thread picks the new size up before its next frame.
        if (w) { InterlockedExchange(&w->newW, LOWORD(lp)); InterlockedExchange(&w->newH, HIWORD(lp)); }
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
        StopRenderThreads();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static double Now() {
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
}

// Waits until `until` (seconds, Now() clock). Uses a high-resolution
// waitable timer (Windows 10 1803+) - plain Sleep() can be ~15.6 ms late,
// which is a whole 60 Hz frame - then spins the last half millisecond.
static void WaitUntil(double until) {
    static thread_local HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x2 /* HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
    for (;;) {
        double left = until - Now();
        if (left <= 0) return;
        if (left > 0.001) {
            if (timer) {
                LARGE_INTEGER due; due.QuadPart = -(LONGLONG)((left - 0.0005) * 1e7);   // relative, 100 ns units
                SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
                WaitForSingleObject(timer, INFINITE);
            } else {
                Sleep((DWORD)((left - 0.0008) * 1000));   // timeBeginPeriod(1) is active
            }
        } else {
            YieldProcessor();
        }
    }
}

// Multimedia Class Scheduler: gives a render thread the steady, high-priority
// scheduling Windows uses for games and video (avrt.dll, Vista+).
static void JoinMmcss() {
    HMODULE avrt = LoadLibraryW(L"avrt.dll");
    if (!avrt) return;
    typedef HANDLE (WINAPI *AVSETPROC)(LPCWSTR, LPDWORD);
    auto set = (AVSETPROC)GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW");
    DWORD index = 0;
    if (set) set(L"Games", &index);   // released automatically when the thread ends
}

static bool g_showGraph;
static int  g_msaa = 4;   // anti-aliasing sample count (Display Settings)
static style::Settings g_style;   // theme / pattern / effect / motion / speed

// Diagnostics overlay: one bar per recent frame, height = time since the
// previous frame. The white line is one refresh; green bars are on time,
// red bars are hitches (a refresh was missed).
static void DrawFrameGraph(Renderer& r, const float* hist, int count, int head, double period) {
    std::vector<Vertex> v;
    const float sx = 2.0f / r.Width(), sy = 2.0f / r.Height();
    const float x0 = 20, y0 = 20, unit = 30;
    float barW = (r.Width() - 2 * x0) / count;   // fit narrow (portrait) screens
    if (barW > 3) barW = 3;
    auto quad = [&](float x, float y, float w, float h, unsigned char cr, unsigned char cg, unsigned char cb) {
        float ax = -1 + x * sx, ay = -1 + y * sy, bx = -1 + (x + w) * sx, by = -1 + (y + h) * sy;
        float p[6][2] = { {ax,ay}, {bx,ay}, {bx,by}, {ax,ay}, {bx,by}, {ax,by} };
        for (auto& q : p) { Vertex vx = { q[0], q[1], 0.5f, 0, 0, 1, 0, 0, cr, cg, cb, 220 }; v.push_back(vx); }
    };
    quad(x0 - 4, y0 - 4, count * barW + 8, unit * 3 + 8, 0, 0, 0);
    for (int i = 0; i < count; i++) {
        float d = hist[(head + i) % count];
        if (d <= 0) continue;
        float h = (float)(d / period) * unit;
        if (h > unit * 3) h = unit * 3;
        bool late = d > period * 1.5;
        quad(x0 + i * barW, y0, barW > 1.5f ? barW - 1 : barW, h, late ? 255 : 40, late ? 50 : 220, late ? 50 : 60);
    }
    quad(x0, y0 + unit, count * barW, 1, 255, 255, 255);
    r.BeginOverlay();   // on top of the final image, never into a persistent scene
    r.SetCamera(Mat4::Identity(), Mat4::Identity());
    DrawParams p;
    p.lit = false;
    p.depth = false;
    p.blend = BLEND_ALPHA;
    r.Draw(v.data(), v.size(), p);
}

// One render thread per monitor. Each has its own Direct3D 11 device -
// created on the GPU that drives that monitor - its own clock and its own
// vsync, so a 175 Hz and a 60 Hz monitor on different GPUs never wait on
// each other or copy frames between GPUs.
float ScreenCareFadeSeconds() { return (float)g_style.fade; }

static PostParams MakePost(const style::Settings& st, float time) {
    PostParams p;
    style::ThemeParams tp = style::GetTheme((int)st.theme);
    p.mode = tp.mode;
    memcpy(p.stops, tp.stops, sizeof(p.stops));
    p.hue = tp.hueShift; p.saturation = tp.saturation; p.contrast = tp.contrast; p.brightness = tp.brightness;
    p.strength = st.strength / 100.0f;
    p.pattern = (int)st.pattern; p.effect = (int)st.effect;
    p.time = time;
    return p;
}

static DWORD WINAPI RenderThread(LPVOID param) {
    auto* w = (SaverWindow*)param;
    g_rng = &w->rng;
    if (g_mode == SM_PREVIEW) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);  // keep Control Panel responsive
    } else {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);  // steady frame pacing
        JoinMmcss();
    }

    RECT rc; GetClientRect(w->hwnd, &rc);
    w->width = rc.right; w->height = rc.bottom;
    Renderer* r = new Renderer;
    r->SetMultisample(g_msaa);
    if (!r->Create(w->hwnd, w->monitor, w->width, w->height)) {
        delete r;
        InterlockedExchange(&w->failed, 1);
        InvalidateRect(w->hwnd, nullptr, FALSE);
        if (g_mode != SM_PREVIEW) ErrorBox(nullptr, IDS_ERR_CREATE_DEVICE);
        return 0;
    }
    w->scene = CreateScene();
    if (!w->scene || !w->scene->Init(*r, w->width, w->height, g_mode == SM_PREVIEW)) {
        delete w->scene; w->scene = nullptr;
        InterlockedExchange(&w->failed, 1);
        InvalidateRect(w->hwnd, nullptr, FALSE);
        if (g_mode != SM_PREVIEW) ErrorBox(nullptr, IDS_ERR_INIT);
    } else {
        w->scene->Resize(w->width, w->height);
        const double period = r->RefreshPeriod();
        const int kGraph = 240;
        float hist[kGraph] = {};
        int histHead = 0;
        double last = Now();
        double styleTime = 0;
        care::Care screenCare;   // OLED rest cycle, pixel orbit, burn-in guard
        const bool ownCare = w->scene->HasOwnScreenCare();
        const bool preview = g_mode == SM_PREVIEW;
        while (!g_quitting) {
            int nw = w->newW, nh = w->newH;
            if (nw > 0 && nh > 0 && (nw != w->width || nh != w->height)) {
                w->width = nw; w->height = nh;
                r->Resize(nw, nh);
                w->scene->Resize(nw, nh);
            }
            // Wait until this monitor can take a new frame, *then* sample the
            // clock, so each frame shows the moment it will actually appear.
            r->WaitForFrame();
            double now = Now();
            double dt = now - last;
            last = now;
            hist[histHead] = (float)dt;
            histHead = (histHead + 1) % kGraph;
            // Snap to whole refresh periods: sub-millisecond timer jitter
            // would otherwise show up as uneven motion. Variable frame rates
            // (G-Sync, or a frame that ran long) keep the measured dt.
            double frames = floor(dt / period + 0.5);
            if (frames >= 1 && fabs(dt - frames * period) < 0.15 * period) dt = frames * period;
            if (dt > 0.1) dt = 0.1;   // avoid big jumps after stalls
            // Style: speed and motion scale the scene's clock; the theme,
            // pattern and effect are applied by the renderer's post pass.
            styleTime += dt;
            const float realDt = (float)dt;
            dt *= style::SpeedMultiplier(g_style.speed) * style::MotionFactor(g_style.motion, (float)styleTime);
            if (dt > 0.25) dt = 0.25;
            r->SetPost(MakePost(g_style, (float)styleTime));
            if (w->width > 0 && w->height > 0) {
                bool draw = ownCare || screenCare.Before(*r, realDt, w->width, w->height, g_style.restEvery * 60.0f,
                                                         (float)g_style.restLength, (float)g_style.fade, g_style.orbit != 0, g_style.guard != 0, preview);
                if (draw) {
                    w->scene->Frame(*r, (float)dt);
                    if (!ownCare) screenCare.After(*r, realDt, w->width, w->height, g_style.guard != 0, preview);
                }
                if (g_showGraph && g_mode != SM_PREVIEW) DrawFrameGraph(*r, hist, kGraph, histHead, period);
                r->Present();
            }
            // Normally the frame-latency waitable object paces us to this
            // monitor's vsync. Only without it (old Windows / legacy swap
            // chain), or if frames come back far too fast because vsync is
            // forced off in the driver, pace manually.
            if (!r->HasFrameWait() && Now() - now < period * 0.5) WaitUntil(now + period);
        }
        delete w->scene;
        w->scene = nullptr;
    }
    delete r;
    return 0;
}

static void StartRenderThread(SaverWindow* w) {
    w->wantScene = true;
    w->thread = CreateThread(nullptr, 0, RenderThread, w, 0, nullptr);
    if (!w->thread) InterlockedExchange(&w->failed, 1);
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
    g_showGraph = RegReadDword(L"Show Frame Graph", 0) != 0;
    g_msaa = (int)RegReadDword(L"MSAA", 4);
    // The preview shows unsaved changes from the Themes & Effects editor.
    g_style = style::Load(SaverFileName(), g_mode == SM_PREVIEW);
    if (g_style.shuffle) {   // the "Shuffle" options get a fresh pick every time the saver starts
        style::Rng rng(style::TimeSeed());
        g_style = style::Randomize(g_style, style::LoadPool(), rng, g_style.shuffle);
    }
    timeBeginPeriod(1);   // 1 ms timer resolution while running

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
        w->monitor = MonitorFromWindow(w->hwnd, MONITOR_DEFAULTTONEAREST);
        StartRenderThread(w);
    } else {
        // Windows 9x: tell the system a saver runs so Ctrl+Alt+Del and
        // Alt+Tab are disabled while the password dialog is in effect.
        if (g_isWin9x) { BOOL old; SystemParametersInfoW(SPI_SETSCREENSAVERRUNNING, TRUE, &old, 0); }

        auto mons = EnumMonitors();
        for (size_t i = 0; i < mons.size(); i++) {
            auto* w = new SaverWindow;
            w->rng = same ? seed : seed + (uint32_t)i * 0x9E3779B9u;
            if (!w->rng) w->rng = 1;
            w->monitor = mons[i].hmon ? mons[i].hmon : MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
            const RECT& r = mons[i].rc;
            w->hwnd = CreateWindowExW(WS_EX_TOPMOST | (g_mode == SM_FULL ? WS_EX_TOOLWINDOW : 0),
                                      L"D3DSaverWndClass", title, WS_POPUP | WS_VISIBLE,
                                      r.left, r.top, r.right - r.left, r.bottom - r.top,
                                      nullptr, nullptr, g_hInst, w);
            if (!w->hwnd) { delete w; continue; }
            g_windows.push_back(w);
            wchar_t key[32]; ScreenKey(key, (int)i);
            if (!RegReadDword(L"Leave Black", 0, key)) StartRenderThread(w);  // else stays black
        }
        if (g_windows.empty()) return 1;
        SetForegroundWindow(g_windows[0]->hwnd);
        SetFocus(g_windows[0]->hwnd);
        // Hide the cursor for the whole session.
        ShowCursor(FALSE);
    }

    // The main thread only handles messages; rendering happens on the
    // per-monitor threads.
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    StopRenderThreads();
    for (auto* w : g_windows) delete w;
    g_windows.clear();
    timeEndPeriod(1);
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
    // Per-monitor DPI awareness (also declared in the manifest): render every
    // monitor at its true native resolution, whatever its scaling setting.
    typedef BOOL (WINAPI *SETDPICTX)(HANDLE);
    if (auto setCtx = (SETDPICTX)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"))
        setCtx((HANDLE)-4 /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */);
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
