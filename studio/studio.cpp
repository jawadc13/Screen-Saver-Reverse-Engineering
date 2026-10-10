// studio.cpp - Screensaver Studio: browse every screensaver in this folder,
// watch it live, and try themes, patterns, effects, motion and speed without
// installing anything.
//
// The live view is the screensaver itself, started the way the Control
// Panel starts its preview ("Saver.scr /p <window>"), drawing into a child
// window here. Style changes are written to the same registry key the saver
// reads (theme.h), then the preview restarts to pick them up. The style
// panel is the same one every saver shows under "Themes & Effects"
// (common/styleui.h).
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <algorithm>
#include "../common/styleui.h"
#include "resource.h"

enum { ID_LIST = 100, ID_SETTINGS, ID_FULLSCREEN, ID_INSTALL, ID_INFO, ID_FILTER };
static const UINT_PTR kRestartTimer = 1;

struct SaverEntry { std::wstring path, file, name; int group; };
static const wchar_t* kGroupNames[] = { L"Classic", L"Frutiger Aero", L"OLED (black background)", L"Full-screen OLED-safe" };

static HWND g_main, g_list, g_filter;
static std::vector<SaverEntry> g_savers;
static std::vector<int> g_rows;   // list row -> saver index, -1 for group headers
static int g_cur = -1;
static styleui::Ui g_ui;
static styleui::Editor g_ed;
static styleui::Preview g_preview;
static RECT g_previewRc;

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
// Preview and actions
// ---------------------------------------------------------------------------
static void StartPreview() {
    if (g_cur >= 0) g_preview.Start(g_main, g_previewRc, g_savers[g_cur].path.c_str());
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

static void SelectSaver(int idx) {
    if (idx < 0 || idx >= (int)g_savers.size()) return;
    g_cur = idx;
    g_ed.Load(style::Load(g_savers[idx].file.c_str()));
    std::wstring info = g_savers[idx].name + L"\n" + g_savers[idx].file + L".scr  \x2022  " + kGroupNames[g_savers[idx].group];
    SetDlgItemTextW(g_main, ID_INFO, info.c_str());
    StartPreview();
}

// Save now, restart the preview shortly (several quick clicks = one restart).
static void StyleChanged() {
    if (g_cur < 0) return;
    style::Save(g_savers[g_cur].file.c_str(), g_ed.Read());
    SetTimer(g_main, kRestartTimer, 300, nullptr);
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
// Window (client 936 x 640 at 96 dpi)
// ---------------------------------------------------------------------------
static void CreateControls() {
    styleui::Ui& ui = g_ui;
    ui.Make(L"STATIC", L"Show:", 0, 12, 14, 40, 18, -1);
    g_filter = ui.Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 52, 10, 218, 300, ID_FILTER);
    SendMessageW(g_filter, CB_ADDSTRING, 0, (LPARAM)L"All screensavers");
    for (auto* g : kGroupNames) SendMessageW(g_filter, CB_ADDSTRING, 0, (LPARAM)g);
    SendMessageW(g_filter, CB_SETCURSEL, 0, 0);
    g_list = ui.Make(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT, 12, 40, 258, 588, ID_LIST, WS_EX_CLIENTEDGE);

    HWND frame = ui.Make(L"STATIC", L"", SS_BLACKRECT, 284, 10, 640, 360, -1);
    GetWindowRect(frame, &g_previewRc);
    MapWindowPoints(nullptr, g_main, (POINT*)&g_previewRc, 2);
    ui.Make(L"STATIC", L"", 0, 284, 378, 330, 36, ID_INFO, 0, true);
    ui.Make(L"BUTTON", L"Saver settings...", WS_TABSTOP, 618, 378, 108, 30, ID_SETTINGS);
    ui.Make(L"BUTTON", L"Full screen", WS_TABSTOP, 730, 378, 84, 30, ID_FULLSCREEN);
    ui.Make(L"BUTTON", L"Use as my saver", WS_TABSTOP | BS_DEFPUSHBUTTON, 818, 378, 106, 30, ID_INSTALL);
    g_ed.Create(g_main, 284, 418, ui.dpi, ui.font, ui.bold);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        g_main = h;
        g_ui.w = h;
        g_ui.dpi = styleui::WindowDpi(h);
        g_ui.font = styleui::MakeFont(g_ui.dpi, false);
        g_ui.bold = styleui::MakeFont(g_ui.dpi, true);
        CreateControls();
        if (g_savers.empty()) {
            SetDlgItemTextW(h, ID_INFO, L"No .scr files found. Put ScreensaverStudio.exe in the folder with the screensavers.");
        } else {
            SelectSaver(0);
        }
        FillList();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        switch (id) {
        case ID_FILTER: if (code == CBN_SELCHANGE) FillList(); return 0;
        case ID_LIST:
            if (code == LBN_SELCHANGE) {
                int row = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
                if (row >= 0 && row < (int)g_rows.size() && g_rows[row] >= 0 && g_rows[row] != g_cur) SelectSaver(g_rows[row]);
            } else if (code == LBN_DBLCLK && g_cur >= 0) Launch(g_savers[g_cur].path, L"/s", nullptr);
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
        if (g_ed.OnCommand(w)) StyleChanged();
        return 0;
    }
    case WM_HSCROLL:
        if (g_ed.OnHScroll(w, l)) StyleChanged();
        return 0;
    case WM_TIMER:
        if (w == kRestartTimer) { KillTimer(h, kRestartTimer); StartPreview(); }
        return 0;
    case WM_DESTROY:
        g_preview.Stop();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);
    FindSavers();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"ScreensaverStudio";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_STUDIO));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    const DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_MAXIMIZEBOX | WS_THICKFRAME);
    HWND h = CreateWindowExW(0, L"ScreensaverStudio", L"Screensaver Studio", style,
                             CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, nullptr, nullptr, inst, nullptr);
    styleui::FitClient(h, g_ui.dpi, 936, 640);
    ShowWindow(h, show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return 0;
}
