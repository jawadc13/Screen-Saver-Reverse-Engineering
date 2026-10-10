// studio.cpp - Screensaver Studio: browse every screensaver in this folder,
// watch it live, and try themes, patterns, effects, motion and speed without
// installing anything.
//
// The live view is the screensaver itself, started the way the Control
// Panel starts its preview ("Saver.scr /p <window>"), drawing into a child
// window here. Style changes are written to the same registry key the saver
// reads (theme.h), then the preview restarts to pick them up.
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <algorithm>
#include "../common/theme.h"
#include "resource.h"

enum {
    ID_LIST = 100, ID_PREVIEW_FRAME, ID_THEME, ID_THEME_NAME, ID_PREV, ID_NEXT, ID_RANDOM, ID_PATTERN, ID_EFFECT,
    ID_MOTION, ID_SPEED, ID_SPEED_LABEL, ID_STRENGTH, ID_STRENGTH_LABEL, ID_SETTINGS, ID_FULLSCREEN, ID_INSTALL,
    ID_ALL, ID_SURPRISE, ID_RESET, ID_INFO, ID_FILTER,
};
static const UINT_PTR kRestartTimer = 1;

struct SaverEntry { std::wstring path, file, name; int group; };
static const wchar_t* kGroupNames[] = { L"Classic", L"Frutiger Aero", L"OLED (black background)", L"Full-screen OLED-safe" };

static HINSTANCE g_inst;
static HWND g_main, g_list, g_preview, g_frame, g_filter;
static std::vector<SaverEntry> g_savers;
static std::vector<int> g_rows;   // list row -> saver index, -1 for group headers
static int g_cur = -1;
static PROCESS_INFORMATION g_proc;
static bool g_loading;
static HFONT g_font, g_bold;
static int g_dpi = 96;

static int S(int px) { return MulDiv(px, g_dpi, 96); }
static HWND Item(int id) { return GetDlgItem(g_main, id); }

// ---------------------------------------------------------------------------
// Saver discovery
// ---------------------------------------------------------------------------
static int GroupOf(const std::wstring& file) {
    if (file.compare(0, 4, L"Aero") == 0) return 1;
    if (file.compare(0, 5, L"OLED_") == 0) return 2;
    if (file.compare(0, 5, L"Safe_") == 0) return 3;
    return 0;
}

static void FindSavers() {
    wchar_t dir[MAX_PATH];
    GetModuleFileNameW(nullptr, dir, MAX_PATH);
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (slash) slash[1] = 0;
    std::wstring pattern = std::wstring(dir) + L"*.scr";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        SaverEntry e;
        e.path = std::wstring(dir) + fd.cFileName;
        e.file = fd.cFileName;
        e.file = e.file.substr(0, e.file.size() - 4);
        e.name = e.file;
        // Display name: string resource 1, as the Control Panel reads it.
        HMODULE m = LoadLibraryExW(e.path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (m) {
            wchar_t buf[256];
            if (LoadStringW(m, 1, buf, 256) > 0) e.name = buf;
            FreeLibrary(m);
        }
        e.group = GroupOf(e.file);
        g_savers.push_back(e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(g_savers.begin(), g_savers.end(), [](const SaverEntry& a, const SaverEntry& b) {
        return a.group != b.group ? a.group < b.group : lstrcmpiW(a.name.c_str(), b.name.c_str()) < 0;
    });
}

static void FillList() {
    int filter = (int)SendMessageW(g_filter, CB_GETCURSEL, 0, 0) - 1;   // -1 = all
    SendMessageW(g_list, LB_RESETCONTENT, 0, 0);
    g_rows.clear();
    int lastGroup = -1, select = -1;
    for (size_t i = 0; i < g_savers.size(); i++) {
        const SaverEntry& e = g_savers[i];
        if (filter >= 0 && e.group != filter) continue;
        if (e.group != lastGroup) {
            std::wstring head = L"--- " + std::wstring(kGroupNames[e.group]) + L" ---";
            SendMessageW(g_list, LB_ADDSTRING, 0, (LPARAM)head.c_str());
            g_rows.push_back(-1);
            lastGroup = e.group;
        }
        std::wstring row = L"    " + e.name;
        SendMessageW(g_list, LB_ADDSTRING, 0, (LPARAM)row.c_str());
        if ((int)i == g_cur) select = (int)g_rows.size();
        g_rows.push_back((int)i);
    }
    if (select >= 0) SendMessageW(g_list, LB_SETCURSEL, select, 0);
}

// ---------------------------------------------------------------------------
// Live preview
// ---------------------------------------------------------------------------
static void StopPreview() {
    if (g_preview) { DestroyWindow(g_preview); g_preview = nullptr; }   // the saver exits with its parent
    if (g_proc.hProcess) {
        if (WaitForSingleObject(g_proc.hProcess, 1500) == WAIT_TIMEOUT) TerminateProcess(g_proc.hProcess, 0);
        CloseHandle(g_proc.hProcess);
        CloseHandle(g_proc.hThread);
        ZeroMemory(&g_proc, sizeof(g_proc));
    }
}

static bool Launch(const std::wstring& path, const std::wstring& args, PROCESS_INFORMATION* keep) {
    std::wstring cmd = L"\"" + path + L"\" " + args;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    STARTUPINFOW si = {}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(path.c_str(), buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    if (keep) *keep = pi;
    else { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    return true;
}

static void StartPreview() {
    StopPreview();
    if (g_cur < 0) return;
    RECT rc; GetWindowRect(g_frame, &rc);
    MapWindowPoints(nullptr, g_main, (POINT*)&rc, 2);
    InflateRect(&rc, -1, -1);
    g_preview = CreateWindowExW(0, L"StudioPreview", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, g_main, nullptr, g_inst, nullptr);
    wchar_t args[32]; wsprintfW(args, L"/p %u", (unsigned)(UINT_PTR)g_preview);
    Launch(g_savers[g_cur].path, args, &g_proc);
}

// ---------------------------------------------------------------------------
// Style controls
// ---------------------------------------------------------------------------
static void ShowThemeName() {
    BOOL ok; UINT id = GetDlgItemInt(g_main, ID_THEME, &ok, FALSE);
    wchar_t name[96]; style::ThemeName(ok ? (int)id : 0, name, 96);
    SetDlgItemTextW(g_main, ID_THEME_NAME, name);
}

static void ShowSliderLabels() {
    int sp = (int)SendMessageW(Item(ID_SPEED), TBM_GETPOS, 0, 0);
    int st = (int)SendMessageW(Item(ID_STRENGTH), TBM_GETPOS, 0, 0);
    wchar_t b[64];
    int x100 = (int)(style::SpeedMultiplier(sp) * 100 + 0.5f);
    wsprintfW(b, L"Speed: %d.%02dx", x100 / 100, x100 % 100);
    SetDlgItemTextW(g_main, ID_SPEED_LABEL, b);
    wsprintfW(b, L"Theme strength: %d%%", st);
    SetDlgItemTextW(g_main, ID_STRENGTH_LABEL, b);
}

static void LoadControls(const style::Settings& st) {
    g_loading = true;
    SetDlgItemInt(g_main, ID_THEME, st.theme, FALSE);
    SendMessageW(Item(ID_PATTERN), CB_SETCURSEL, st.pattern, 0);
    SendMessageW(Item(ID_EFFECT), CB_SETCURSEL, st.effect, 0);
    SendMessageW(Item(ID_MOTION), CB_SETCURSEL, st.motion, 0);
    SendMessageW(Item(ID_SPEED), TBM_SETPOS, TRUE, st.speed);
    SendMessageW(Item(ID_STRENGTH), TBM_SETPOS, TRUE, st.strength);
    ShowThemeName();
    ShowSliderLabels();
    g_loading = false;
}

static style::Settings ReadControls() {
    style::Settings st;
    BOOL ok; UINT id = GetDlgItemInt(g_main, ID_THEME, &ok, FALSE);
    st.theme = ok ? id : 0;
    st.pattern = (DWORD)SendMessageW(Item(ID_PATTERN), CB_GETCURSEL, 0, 0);
    st.effect = (DWORD)SendMessageW(Item(ID_EFFECT), CB_GETCURSEL, 0, 0);
    st.motion = (DWORD)SendMessageW(Item(ID_MOTION), CB_GETCURSEL, 0, 0);
    st.speed = (DWORD)SendMessageW(Item(ID_SPEED), TBM_GETPOS, 0, 0);
    st.strength = (DWORD)SendMessageW(Item(ID_STRENGTH), TBM_GETPOS, 0, 0);
    style::Clamp(st);
    return st;
}

// Save now, restart the preview shortly (several quick clicks = one restart).
static void StyleChanged() {
    if (g_loading || g_cur < 0) return;
    style::Save(g_savers[g_cur].file.c_str(), ReadControls());
    ShowThemeName();
    ShowSliderLabels();
    SetTimer(g_main, kRestartTimer, 300, nullptr);
}

static void SelectSaver(int idx) {
    if (idx < 0 || idx >= (int)g_savers.size()) return;
    g_cur = idx;
    LoadControls(style::Load(g_savers[idx].file.c_str()));
    std::wstring info = g_savers[idx].name + L"\n" + g_savers[idx].file + L".scr  \x2022  " + kGroupNames[g_savers[idx].group];
    SetDlgItemTextW(g_main, ID_INFO, info.c_str());
    StartPreview();
}

static DWORD Rand() {
    static DWORD x = GetTickCount() | 1;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}

static void SetAsScreensaver() {
    if (g_cur < 0) return;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        const std::wstring& p = g_savers[g_cur].path;
        RegSetValueExW(k, L"SCRNSAVE.EXE", 0, REG_SZ, (const BYTE*)p.c_str(), (DWORD)((p.size() + 1) * sizeof(wchar_t)));
        RegSetValueExW(k, L"ScreenSaveActive", 0, REG_SZ, (const BYTE*)L"1", 2 * sizeof(wchar_t));
        RegCloseKey(k);
    }
    SystemParametersInfoW(SPI_SETSCREENSAVEACTIVE, TRUE, nullptr, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    std::wstring msg = g_savers[g_cur].name + L" is now your screen saver.\n\nIt runs from:\n" + g_savers[g_cur].path +
                       L"\n\nKeep this folder where it is (or install the .scr) so Windows can find it. The wait time is set in Windows' Screen Saver Settings.";
    MessageBoxW(g_main, msg.c_str(), L"Screensaver Studio", MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------
static HWND Make(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, DWORD ex = 0) {
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), g_main,
                             (HMENU)(INT_PTR)id, g_inst, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, FALSE);
    return c;
}

static void CreateControls() {
    // Left: saver list.
    Make(L"STATIC", L"Show:", 0, 12, 14, 40, 18, -1);
    g_filter = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 52, 10, 218, 300, ID_FILTER);
    SendMessageW(g_filter, CB_ADDSTRING, 0, (LPARAM)L"All screensavers");
    for (auto* g : kGroupNames) SendMessageW(g_filter, CB_ADDSTRING, 0, (LPARAM)g);
    SendMessageW(g_filter, CB_SETCURSEL, 0, 0);
    g_list = Make(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT, 12, 40, 258, 598, ID_LIST, WS_EX_CLIENTEDGE);

    // Right: preview and its info.
    g_frame = Make(L"STATIC", L"", SS_BLACKRECT, 284, 10, 640, 360, ID_PREVIEW_FRAME);
    HWND info = Make(L"STATIC", L"", 0, 284, 376, 640, 36, ID_INFO);
    SendMessageW(info, WM_SETFONT, (WPARAM)g_bold, FALSE);

    // Style.
    int x = 284, y = 418;
    Make(L"BUTTON", L"Colour theme", BS_GROUPBOX, x, y, 640, 60, -1);
    Make(L"STATIC", L"Theme #", 0, x + 12, y + 26, 54, 18, -1);
    Make(L"EDIT", L"0", ES_NUMBER | WS_TABSTOP, x + 66, y + 23, 62, 22, ID_THEME, WS_EX_CLIENTEDGE);
    Make(L"BUTTON", L"< Prev", WS_TABSTOP, x + 136, y + 22, 64, 24, ID_PREV);
    Make(L"BUTTON", L"Next >", WS_TABSTOP, x + 204, y + 22, 64, 24, ID_NEXT);
    Make(L"BUTTON", L"Random", WS_TABSTOP, x + 272, y + 22, 64, 24, ID_RANDOM);
    HWND tn = Make(L"STATIC", L"", 0, x + 348, y + 26, 284, 18, ID_THEME_NAME);
    SendMessageW(tn, WM_SETFONT, (WPARAM)g_bold, FALSE);

    y += 70;
    Make(L"STATIC", L"Pattern", 0, x, y + 4, 60, 18, -1);
    HWND pat = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x + 60, y, 250, 400, ID_PATTERN);
    Make(L"STATIC", L"Effect", 0, x + 330, y + 4, 50, 18, -1);
    HWND fx = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x + 390, y, 250, 400, ID_EFFECT);
    y += 32;
    Make(L"STATIC", L"Motion", 0, x, y + 4, 60, 18, -1);
    HWND mo = Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x + 60, y, 250, 300, ID_MOTION);
    for (int i = 0; i < style::kPatternCount; i++) SendMessageW(pat, CB_ADDSTRING, 0, (LPARAM)style::kPatternNames[i]);
    for (int i = 0; i < style::kEffectCount; i++)  SendMessageW(fx, CB_ADDSTRING, 0, (LPARAM)style::kEffectNames[i]);
    for (int i = 0; i < style::kMotionCount; i++)  SendMessageW(mo, CB_ADDSTRING, 0, (LPARAM)style::kMotionNames[i]);
    y += 34;
    Make(L"STATIC", L"", 0, x, y + 4, 150, 18, ID_SPEED_LABEL);
    HWND sp = Make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, x + 150, y, 160, 28, ID_SPEED);
    Make(L"STATIC", L"", 0, x + 330, y + 4, 150, 18, ID_STRENGTH_LABEL);
    HWND st = Make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, x + 480, y, 160, 28, ID_STRENGTH);
    for (HWND t : { sp, st }) { SendMessageW(t, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100)); SendMessageW(t, TBM_SETTICFREQ, 10, 0); }

    // Actions.
    y += 42;
    Make(L"BUTTON", L"Surprise me!", WS_TABSTOP, x, y, 116, 30, ID_SURPRISE);
    Make(L"BUTTON", L"Reset style", WS_TABSTOP, x + 122, y, 90, 30, ID_RESET);
    Make(L"BUTTON", L"Apply style to all", WS_TABSTOP, x + 218, y, 124, 30, ID_ALL);
    Make(L"BUTTON", L"Saver settings...", WS_TABSTOP, x + 348, y, 116, 30, ID_SETTINGS);
    Make(L"BUTTON", L"Full screen", WS_TABSTOP, x + 470, y, 82, 30, ID_FULLSCREEN);
    Make(L"BUTTON", L"Use as my saver", WS_TABSTOP | BS_DEFPUSHBUTTON, x + 556, y, 84, 30, ID_INSTALL);
}

static LRESULT CALLBACK PreviewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_ERASEBKGND) { RECT rc; GetClientRect(h, &rc); FillRect((HDC)w, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); return 1; }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_main = h;
        CreateControls();
        FillList();
        if (g_savers.empty()) {
            SetDlgItemTextW(h, ID_INFO, L"No .scr files found. Put ScreensaverStudio.exe in the folder with the screensavers.");
        } else {
            SelectSaver(0);
            FillList();
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        BOOL ok; int theme = (int)GetDlgItemInt(h, ID_THEME, &ok, FALSE);
        if (!ok) theme = 0;
        switch (id) {
        case ID_FILTER: if (code == CBN_SELCHANGE) FillList(); return 0;
        case ID_LIST:
            if (code == LBN_SELCHANGE) {
                int row = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (row >= 0 && row < (int)g_rows.size() && g_rows[row] >= 0 && g_rows[row] != g_cur) SelectSaver(g_rows[row]);
            } else if (code == LBN_DBLCLK && g_cur >= 0) Launch(g_savers[g_cur].path, L"/s", nullptr);
            return 0;
        case ID_THEME: if (code == EN_CHANGE) StyleChanged(); return 0;
        case ID_PREV:   SetDlgItemInt(h, ID_THEME, theme > 0 ? theme - 1 : style::kThemeCount, FALSE); return 0;
        case ID_NEXT:   SetDlgItemInt(h, ID_THEME, theme < style::kThemeCount ? theme + 1 : 0, FALSE); return 0;
        case ID_RANDOM: SetDlgItemInt(h, ID_THEME, 1 + Rand() % style::kThemeCount, FALSE); return 0;
        case ID_PATTERN: case ID_EFFECT: case ID_MOTION: if (code == CBN_SELCHANGE) StyleChanged(); return 0;
        case ID_SURPRISE: {
            style::Settings st;
            st.theme = 1 + Rand() % style::kThemeCount;
            st.pattern = Rand() % 3 == 0 ? 0 : Rand() % style::kPatternCount;
            st.effect = Rand() % 3 == 0 ? 0 : Rand() % style::kEffectCount;
            st.motion = Rand() % style::kMotionCount;
            st.speed = 35 + Rand() % 40;
            st.strength = 70 + Rand() % 31;
            LoadControls(st);
            StyleChanged();
            return 0;
        }
        case ID_RESET: LoadControls(style::Settings()); StyleChanged(); return 0;
        case ID_ALL:
            style::Save(L"_All", ReadControls());
            for (const SaverEntry& e : g_savers) style::Save(e.file.c_str(), ReadControls());
            MessageBoxW(h, L"This style is now set on every screensaver.", L"Screensaver Studio", MB_ICONINFORMATION);
            return 0;
        case ID_SETTINGS:
            if (g_cur >= 0) {
                wchar_t args[32]; wsprintfW(args, L"/c:%u", (unsigned)(UINT_PTR)h);
                PROCESS_INFORMATION pi;
                if (Launch(g_savers[g_cur].path, args, &pi)) {
                    // Modal like the Control Panel; pump messages while it runs.
                    EnableWindow(h, FALSE);
                    while (MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) {
                        MSG msg;
                        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                    }
                    EnableWindow(h, TRUE);
                    SetForegroundWindow(h);
                    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
                    SelectSaver(g_cur);   // settings (and maybe style) changed
                }
            }
            return 0;
        case ID_FULLSCREEN: if (g_cur >= 0) Launch(g_savers[g_cur].path, L"/s", nullptr); return 0;
        case ID_INSTALL: SetAsScreensaver(); return 0;
        }
        break;
    }
    case WM_HSCROLL:
        if ((HWND)l == Item(ID_SPEED) || (HWND)l == Item(ID_STRENGTH)) {
            ShowSliderLabels();
            if (LOWORD(w) == TB_ENDTRACK || LOWORD(w) == TB_THUMBPOSITION) StyleChanged();
        }
        return 0;
    case WM_TIMER:
        if (w == kRestartTimer) { KillTimer(h, kRestartTimer); StartPreview(); }
        return 0;
    case WM_CTLCOLORSTATIC:
        if ((HWND)l == g_frame) return (LRESULT)GetStockObject(BLACK_BRUSH);
        break;
    case WM_GETMINMAXINFO: {
        auto* mm = (MINMAXINFO*)l;
        RECT rc = { 0, 0, S(936), S(650) };
        AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
        mm->ptMinTrackSize.x = rc.right - rc.left;
        mm->ptMinTrackSize.y = rc.bottom - rc.top;
        return 0;
    }
    case WM_DESTROY:
        StopPreview();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    g_inst = inst;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);
    HDC dc = GetDC(nullptr);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    NONCLIENTMETRICSW ncm = {}; ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    ncm.lfMessageFont.lfWeight = FW_BOLD;
    g_bold = CreateFontIndirectW(&ncm.lfMessageFont);

    FindSavers();

    WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PreviewProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"StudioPreview";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = L"ScreensaverStudio";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_STUDIO));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, S(936), S(650) };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME), FALSE, 0);
    HWND h = CreateWindowExW(0, L"ScreensaverStudio", L"Screensaver Studio", WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME),
                             CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, inst, nullptr);
    ShowWindow(h, show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return 0;
}
