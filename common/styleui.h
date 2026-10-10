// styleui.h - the style editor shared by every screensaver and Screensaver
// Studio (header-only, plain Win32, controls created in code so it needs no
// resources):
//   * Editor            - the theme / pattern / effect / motion / speed panel
//   * ShowStyleDialog   - "Themes & Effects" window with a live preview
//   * ShowRandomizer    - which options the randomizer may pick, favourites
#pragma once
#include "theme.h"
#include <commctrl.h>
#include <string>

namespace styleui {

using namespace style;

inline int WindowDpi(HWND h) {
    typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn fn = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn && h) { UINT d = fn(h); if (d) return (int)d; }
    HDC dc = GetDC(nullptr);
    int d = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    return d;
}

// The Windows message font at `dpi`.
inline HFONT MakeFont(int dpi, bool bold) {
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HDC dc = GetDC(nullptr);
    int sys = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    ncm.lfMessageFont.lfHeight = MulDiv(ncm.lfMessageFont.lfHeight, dpi, sys);
    if (bold) ncm.lfMessageFont.lfWeight = FW_BOLD;
    return CreateFontIndirectW(&ncm.lfMessageFont);
}

// Child controls laid out in 96-dpi pixels, scaled to the window's DPI.
struct Ui {
    HWND w = nullptr;
    int dpi = 96, ox = 0, oy = 0;
    HFONT font = nullptr, bold = nullptr;
    int S(int v) const { return MulDiv(v, dpi, 96); }
    HWND Make(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int cx, int cy, int id, DWORD ex = 0, bool isBold = false) const {
        HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, S(ox + x), S(oy + y), S(cx), S(cy), w,
                                 (HMENU)(INT_PTR)id, (HINSTANCE)GetWindowLongPtrW(w, GWLP_HINSTANCE), nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)(isBold ? bold : font), FALSE);
        return c;
    }
    HWND I(int id) const { return GetDlgItem(w, id); }
};

// Sizes a window's client area to cx x cy (96-dpi pixels), keeping it centred.
inline void FitClient(HWND w, int dpi, int cx, int cy) {
    RECT rc = { 0, 0, MulDiv(cx, dpi, 96), MulDiv(cy, dpi, 96) };
    AdjustWindowRectEx(&rc, (DWORD)GetWindowLongW(w, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(w, GWL_EXSTYLE));
    int W = rc.right - rc.left, H = rc.bottom - rc.top;
    RECT cur; GetWindowRect(w, &cur);
    HWND owner = GetWindow(w, GW_OWNER);
    RECT ref = cur;
    if (owner) GetWindowRect(owner, &ref);
    int x = (ref.left + ref.right) / 2 - W / 2, y = (ref.top + ref.bottom) / 2 - H / 2;
    MONITORINFO mi = {}; mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromRect(&ref, MONITOR_DEFAULTTONEAREST), &mi)) {
        if (x + W > mi.rcWork.right) x = mi.rcWork.right - W;
        if (y + H > mi.rcWork.bottom) y = mi.rcWork.bottom - H;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top) y = mi.rcWork.top;
    }
    SetWindowPos(w, nullptr, x, y, W, H, SWP_NOZORDER);
}

// A modal dialog with no controls; the procedure creates them in WM_INITDIALOG.
inline INT_PTR RunDialog(HWND owner, const wchar_t* title, DLGPROC proc, LPARAM lp) {
    std::vector<WORD> t;
    auto dw = [&](DWORD v) { t.push_back(LOWORD(v)); t.push_back(HIWORD(v)); };
    auto str = [&](const wchar_t* s) { while (*s) t.push_back(*s++); t.push_back(0); };
    dw(DS_SETFONT | DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU);
    dw(0);
    t.push_back(0);                                    // no items
    t.push_back(0); t.push_back(0); t.push_back(200); t.push_back(100);
    t.push_back(0); t.push_back(0);                    // no menu, default class
    str(title);
    t.push_back(9); str(L"Segoe UI");
    return DialogBoxIndirectParamW((HINSTANCE)GetModuleHandleW(nullptr), (LPCDLGTEMPLATEW)t.data(), owner, proc, lp);
}

void ShowRandomizer(HWND owner);

// ---------------------------------------------------------------------------
// Editor panel (640 x 272 at 96 dpi)
// ---------------------------------------------------------------------------
enum {
    E_THEME = 3000, E_NAME, E_PREV, E_NEXT, E_RANDOM, E_FAV, E_PATTERN, E_EFFECT, E_MOTION, E_SPEED, E_SPEEDL,
    E_STRENGTH, E_STRENGTHL, E_RANDOMIZE, E_POOL, E_SURPRISE, E_RESET, E_ALL, E_REST_EVERY, E_REST_LEN, E_ORBIT, E_GUARD,
};

struct Editor {
    static const int kWidth = 640, kHeight = 272;
    Ui ui;
    bool loading = false;
    Rng rng{ TimeSeed() };

    void Create(HWND parent, int x, int y, int dpi, HFONT font, HFONT bold) {
        ui.w = parent; ui.ox = x; ui.oy = y; ui.dpi = dpi; ui.font = font; ui.bold = bold;
        ui.Make(L"BUTTON", L"Colour theme", BS_GROUPBOX, 0, 0, 640, 60, -1);
        ui.Make(L"STATIC", L"Theme #", 0, 12, 26, 54, 18, -1);
        ui.Make(L"EDIT", L"0", ES_NUMBER | WS_TABSTOP, 66, 23, 62, 22, E_THEME, WS_EX_CLIENTEDGE);
        ui.Make(L"BUTTON", L"< Prev", WS_TABSTOP, 136, 22, 60, 24, E_PREV);
        ui.Make(L"BUTTON", L"Next >", WS_TABSTOP, 200, 22, 60, 24, E_NEXT);
        ui.Make(L"BUTTON", L"Random", WS_TABSTOP, 264, 22, 60, 24, E_RANDOM);
        ui.Make(L"BUTTON", L"Favourite", BS_AUTOCHECKBOX | WS_TABSTOP, 334, 25, 80, 20, E_FAV);
        ui.Make(L"STATIC", L"", SS_ENDELLIPSIS, 420, 26, 212, 18, E_NAME, 0, true);

        ui.Make(L"STATIC", L"Pattern", 0, 0, 74, 60, 18, -1);
        HWND pat = ui.Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 60, 70, 250, 400, E_PATTERN);
        ui.Make(L"STATIC", L"Effect", 0, 330, 74, 50, 18, -1);
        HWND fx = ui.Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 390, 70, 250, 400, E_EFFECT);
        ui.Make(L"STATIC", L"Motion", 0, 0, 106, 60, 18, -1);
        HWND mo = ui.Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 60, 102, 250, 300, E_MOTION);
        for (int i = 0; i < kPatternCount; i++) SendMessageW(pat, CB_ADDSTRING, 0, (LPARAM)kPatternNames[i]);
        for (int i = 0; i < kEffectCount; i++)  SendMessageW(fx, CB_ADDSTRING, 0, (LPARAM)kEffectNames[i]);
        for (int i = 0; i < kMotionCount; i++)  SendMessageW(mo, CB_ADDSTRING, 0, (LPARAM)kMotionNames[i]);
        ui.Make(L"BUTTON", L"Randomize the style every time it starts", BS_AUTOCHECKBOX | WS_TABSTOP, 330, 104, 310, 20, E_RANDOMIZE);

        ui.Make(L"STATIC", L"", 0, 0, 142, 150, 18, E_SPEEDL);
        HWND sp = ui.Make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, 150, 138, 160, 28, E_SPEED);
        ui.Make(L"STATIC", L"", 0, 330, 142, 150, 18, E_STRENGTHL);
        HWND st = ui.Make(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_AUTOTICKS | WS_TABSTOP, 480, 138, 160, 28, E_STRENGTH);
        for (HWND tb : { sp, st }) { SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100)); SendMessageW(tb, TBM_SETTICFREQ, 10, 0); }

        ui.Make(L"BUTTON", L"Surprise me!", WS_TABSTOP, 0, 180, 116, 30, E_SURPRISE);
        ui.Make(L"BUTTON", L"Reset style", WS_TABSTOP, 122, 180, 96, 30, E_RESET);
        ui.Make(L"BUTTON", L"Apply style to all savers", WS_TABSTOP, 224, 180, 170, 30, E_ALL);
        ui.Make(L"BUTTON", L"Randomizer && favourites...", WS_TABSTOP, 400, 180, 240, 30, E_POOL);

        ui.Make(L"BUTTON", L"OLED screen care  (full-screen OLED-safe savers use the rest settings in their own Settings)", BS_GROUPBOX, 0, 218, 640, 54, -1);
        ui.Make(L"STATIC", L"Rest to black every", 0, 12, 244, 112, 18, -1);
        HWND ev = ui.Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 124, 240, 84, 300, E_REST_EVERY);
        ui.Make(L"STATIC", L"for", 0, 216, 244, 22, 18, -1);
        HWND len = ui.Make(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 240, 240, 70, 300, E_REST_LEN);
        for (DWORD m : kRestEveryMinutes) {
            wchar_t b[32];
            if (m) wsprintfW(b, L"%u min", m); else lstrcpyW(b, L"Never");
            SendMessageW(ev, CB_ADDSTRING, 0, (LPARAM)b);
        }
        for (DWORD sec : kRestLengthSeconds) { wchar_t b[32]; wsprintfW(b, L"%u s", sec); SendMessageW(len, CB_ADDSTRING, 0, (LPARAM)b); }
        ui.Make(L"BUTTON", L"Pixel orbit", BS_AUTOCHECKBOX | WS_TABSTOP, 330, 242, 100, 20, E_ORBIT);
        ui.Make(L"BUTTON", L"Burn-in guard (dims static areas)", BS_AUTOCHECKBOX | WS_TABSTOP, 434, 242, 200, 20, E_GUARD);
    }

    // Index of the closest choice in a list.
    template <size_t N>
    static int Closest(const DWORD (&list)[N], DWORD v) {
        int best = 0;
        for (size_t i = 0; i < N; i++) if ((list[i] > v ? list[i] - v : v - list[i]) < (list[best] > v ? list[best] - v : v - list[best])) best = (int)i;
        return best;
    }

    int Theme() const {
        BOOL ok; UINT id = GetDlgItemInt(ui.w, E_THEME, &ok, FALSE);
        return ok && id <= (UINT)kThemeCount ? (int)id : 0;
    }

    void Refresh() {
        int id = Theme();
        wchar_t name[96]; ThemeName(id, name, 96);
        SetDlgItemTextW(ui.w, E_NAME, name);
        CheckDlgButton(ui.w, E_FAV, id > 0 && IsFavourite(id) ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(ui.I(E_FAV), id > 0);
        int sp = (int)SendMessageW(ui.I(E_SPEED), TBM_GETPOS, 0, 0);
        int st = (int)SendMessageW(ui.I(E_STRENGTH), TBM_GETPOS, 0, 0);
        int x100 = (int)(SpeedMultiplier(sp) * 100 + 0.5f);
        wchar_t b[64];
        wsprintfW(b, L"Speed: %d.%02dx", x100 / 100, x100 % 100);
        SetDlgItemTextW(ui.w, E_SPEEDL, b);
        wsprintfW(b, L"Theme strength: %d%%", st);
        SetDlgItemTextW(ui.w, E_STRENGTHL, b);
    }

    void Load(const Settings& s) {
        loading = true;
        SetDlgItemInt(ui.w, E_THEME, s.theme, FALSE);
        SendMessageW(ui.I(E_PATTERN), CB_SETCURSEL, s.pattern, 0);
        SendMessageW(ui.I(E_EFFECT), CB_SETCURSEL, s.effect, 0);
        SendMessageW(ui.I(E_MOTION), CB_SETCURSEL, s.motion, 0);
        SendMessageW(ui.I(E_SPEED), TBM_SETPOS, TRUE, s.speed);
        SendMessageW(ui.I(E_STRENGTH), TBM_SETPOS, TRUE, s.strength);
        CheckDlgButton(ui.w, E_RANDOMIZE, s.randomize ? BST_CHECKED : BST_UNCHECKED);
        SendMessageW(ui.I(E_REST_EVERY), CB_SETCURSEL, Closest(kRestEveryMinutes, s.restEvery), 0);
        SendMessageW(ui.I(E_REST_LEN), CB_SETCURSEL, Closest(kRestLengthSeconds, s.restLength), 0);
        CheckDlgButton(ui.w, E_ORBIT, s.orbit ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(ui.w, E_GUARD, s.guard ? BST_CHECKED : BST_UNCHECKED);
        Refresh();
        loading = false;
    }

    Settings Read() const {
        Settings s;
        s.theme = (DWORD)Theme();
        s.pattern = (DWORD)SendMessageW(ui.I(E_PATTERN), CB_GETCURSEL, 0, 0);
        s.effect = (DWORD)SendMessageW(ui.I(E_EFFECT), CB_GETCURSEL, 0, 0);
        s.motion = (DWORD)SendMessageW(ui.I(E_MOTION), CB_GETCURSEL, 0, 0);
        s.speed = (DWORD)SendMessageW(ui.I(E_SPEED), TBM_GETPOS, 0, 0);
        s.strength = (DWORD)SendMessageW(ui.I(E_STRENGTH), TBM_GETPOS, 0, 0);
        s.randomize = IsDlgButtonChecked(ui.w, E_RANDOMIZE) == BST_CHECKED;
        int ev = (int)SendMessageW(ui.I(E_REST_EVERY), CB_GETCURSEL, 0, 0), len = (int)SendMessageW(ui.I(E_REST_LEN), CB_GETCURSEL, 0, 0);
        if (ev >= 0 && ev < (int)(sizeof(kRestEveryMinutes) / sizeof(DWORD))) s.restEvery = kRestEveryMinutes[ev];
        if (len >= 0 && len < (int)(sizeof(kRestLengthSeconds) / sizeof(DWORD))) s.restLength = kRestLengthSeconds[len];
        s.orbit = IsDlgButtonChecked(ui.w, E_ORBIT) == BST_CHECKED;
        s.guard = IsDlgButtonChecked(ui.w, E_GUARD) == BST_CHECKED;
        Clamp(s);
        return s;
    }

    // WM_COMMAND. Returns true when the style changed: the caller saves
    // Read() and refreshes its preview.
    bool OnCommand(WPARAM wp) {
        int id = LOWORD(wp), code = HIWORD(wp), theme = Theme();
        switch (id) {
        case E_THEME:  if (code == EN_CHANGE) { Refresh(); return !loading; } return false;
        case E_PREV:   SetDlgItemInt(ui.w, E_THEME, theme > 0 ? theme - 1 : kThemeCount, FALSE); return false;
        case E_NEXT:   SetDlgItemInt(ui.w, E_THEME, theme < kThemeCount ? theme + 1 : 0, FALSE); return false;
        case E_RANDOM: SetDlgItemInt(ui.w, E_THEME, RandomTheme(LoadPool(), rng), FALSE); return false;
        case E_FAV:    SetFavourite(theme, IsDlgButtonChecked(ui.w, E_FAV) == BST_CHECKED); return false;
        case E_PATTERN: case E_EFFECT: case E_MOTION: case E_REST_EVERY: case E_REST_LEN: return code == CBN_SELCHANGE;
        case E_RANDOMIZE: case E_ORBIT: case E_GUARD: return true;
        case E_POOL:   ShowRandomizer(GetAncestor(ui.w, GA_ROOT)); Refresh(); return false;
        case E_SURPRISE: {
            RandomPool pool = LoadPool();
            pool.what = RW_THEME | RW_PATTERN | RW_EFFECT | RW_MOTION | RW_SPEED;
            Load(Randomize(Read(), pool, rng));
            return true;
        }
        case E_RESET: {   // style back to defaults; screen care is kept
            Settings cur = Read(), d;
            d.restEvery = cur.restEvery; d.restLength = cur.restLength; d.orbit = cur.orbit; d.guard = cur.guard;
            Load(d);
            return true;
        }
        case E_ALL:
            SaveToAll(Read());
            MessageBoxW(GetAncestor(ui.w, GA_ROOT), L"This style (including the randomize setting) is now used by every screensaver.",
                        L"Themes & Effects", MB_ICONINFORMATION);
            return true;
        }
        return false;
    }

    // WM_HSCROLL. Returns true when a slider was released at a new value.
    bool OnHScroll(WPARAM wp, LPARAM lp) {
        if ((HWND)lp != ui.I(E_SPEED) && (HWND)lp != ui.I(E_STRENGTH)) return false;
        Refresh();
        return LOWORD(wp) == TB_ENDTRACK || LOWORD(wp) == TB_THUMBPOSITION;
    }
};

// ---------------------------------------------------------------------------
// Randomizer & favourites dialog
// ---------------------------------------------------------------------------
enum {
    R_PAL = 3100, R_MODE, R_PATTERN, R_EFFECT, R_MOTION, R_FAVS, R_FAV_REMOVE, R_FAV_CLEAR,
    R_W_THEME, R_W_PATTERN, R_W_EFFECT, R_W_MOTION, R_W_SPEED, R_FAVONLY, R_ORIGINAL,
    R_SMIN, R_SMAX, R_SMINL, R_SMAXL, R_ALL0, R_NONE0 = R_ALL0 + 8,
};

struct RandomizerState {
    Ui ui;
    RandomPool pool;
    std::vector<int> favs;
};

inline HWND MakeCheckList(const Ui& ui, int x, int y, int cx, int cy, int id) {
    HWND lv = ui.Make(WC_LISTVIEWW, L"", LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
                      x, y, cx, cy, id, WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    col.cx = ui.S(cx - 26);
    ListView_InsertColumn(lv, 0, &col);
    return lv;
}

inline void FillCheckList(HWND lv, const wchar_t* const* names, int count, DWORD mask) {
    for (int i = 0; i < count; i++) {
        LVITEMW it = {};
        it.mask = LVIF_TEXT;
        it.iItem = i;
        it.pszText = (LPWSTR)names[i];
        ListView_InsertItem(lv, &it);
        ListView_SetCheckState(lv, i, (mask >> i) & 1);
    }
}

inline DWORD ReadCheckList(HWND lv) {
    DWORD m = 0;
    int n = ListView_GetItemCount(lv);
    for (int i = 0; i < n && i < 32; i++) if (ListView_GetCheckState(lv, i)) m |= 1u << i;
    return m;
}

inline void FillFavourites(RandomizerState* s) {
    HWND lb = s->ui.I(R_FAVS);
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    for (int id : s->favs) {
        wchar_t name[96], row[128];
        ThemeName(id, name, 96);
        wsprintfW(row, L"%d  %s", id, name);
        SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)row);
    }
    if (s->favs.empty()) SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)L"(tick Favourite next to a theme)");
}

inline void ShowSpeedRange(RandomizerState* s) {
    int lo = (int)SendMessageW(s->ui.I(R_SMIN), TBM_GETPOS, 0, 0), hi = (int)SendMessageW(s->ui.I(R_SMAX), TBM_GETPOS, 0, 0);
    wchar_t b[64];
    int a = (int)(SpeedMultiplier(lo) * 100 + 0.5f), c = (int)(SpeedMultiplier(hi) * 100 + 0.5f);
    wsprintfW(b, L"Slowest: %d.%02dx", a / 100, a % 100); SetDlgItemTextW(s->ui.w, R_SMINL, b);
    wsprintfW(b, L"Fastest: %d.%02dx", c / 100, c % 100); SetDlgItemTextW(s->ui.w, R_SMAXL, b);
}

inline INT_PTR CALLBACK RandomizerProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* s = (RandomizerState*)GetWindowLongPtrW(dlg, DWLP_USER);
    static const int kLists[5] = { R_PAL, R_MODE, R_PATTERN, R_EFFECT, R_MOTION };
    switch (msg) {
    case WM_INITDIALOG: {
        s = (RandomizerState*)lp;
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        Ui& ui = s->ui;
        ui.w = dlg;
        ui.dpi = WindowDpi(dlg);
        ui.font = MakeFont(ui.dpi, false);
        ui.bold = MakeFont(ui.dpi, true);
        ui.Make(L"STATIC", L"Untick anything you never want the randomizer (Random, Surprise me! and \"Randomize every time\") to pick.",
                0, 12, 10, 900, 18, -1);
        const wchar_t* pal[64];
        for (int i = 0; i < kPaletteCount; i++) pal[i] = kPalettes[i].name;
        DWORD palMask[2] = { s->pool.pal0, s->pool.pal1 };
        struct Col { const wchar_t* title; int id, x, y, cx, cy; };
        const Col cols[5] = {
            { L"Palettes", R_PAL, 12, 36, 170, 300 },
            { L"Theme styles", R_MODE, 194, 36, 150, 156 },
            { L"Patterns", R_PATTERN, 356, 36, 170, 300 },
            { L"Effects", R_EFFECT, 538, 36, 170, 300 },
            { L"Motion", R_MOTION, 194, 216, 150, 120 },
        };
        for (int c = 0; c < 5; c++) {
            ui.Make(L"STATIC", cols[c].title, 0, cols[c].x, cols[c].y, cols[c].cx, 18, -1, 0, true);
            HWND lv = MakeCheckList(ui, cols[c].x, cols[c].y + 20, cols[c].cx, cols[c].cy - 20, cols[c].id);
            switch (cols[c].id) {
            case R_PAL:
                for (int i = 0; i < kPaletteCount; i++) {
                    LVITEMW it = {}; it.mask = LVIF_TEXT; it.iItem = i; it.pszText = (LPWSTR)pal[i];
                    ListView_InsertItem(lv, &it);
                    ListView_SetCheckState(lv, i, (palMask[i / 32] >> (i & 31)) & 1);
                }
                break;
            case R_MODE:    FillCheckList(lv, kModeNames, MODE_COUNT, s->pool.modes); break;
            case R_PATTERN: FillCheckList(lv, kPatternNames, kPatternCount, s->pool.patterns); break;
            case R_EFFECT:  FillCheckList(lv, kEffectNames, kEffectCount, s->pool.effects); break;
            case R_MOTION:  FillCheckList(lv, kMotionNames, kMotionCount, s->pool.motions); break;
            }
            ui.Make(L"BUTTON", L"All", WS_TABSTOP, cols[c].x, cols[c].y + cols[c].cy + 4, 50, 22, R_ALL0 + c);
            ui.Make(L"BUTTON", L"None", WS_TABSTOP, cols[c].x + 54, cols[c].y + cols[c].cy + 4, 50, 22, R_NONE0 + c);
        }
        ui.Make(L"STATIC", L"Favourite themes", 0, 720, 36, 190, 18, -1, 0, true);
        ui.Make(L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT, 720, 56, 190, 280, R_FAVS, WS_EX_CLIENTEDGE);
        ui.Make(L"BUTTON", L"Remove", WS_TABSTOP, 720, 340, 70, 22, R_FAV_REMOVE);
        ui.Make(L"BUTTON", L"Clear all", WS_TABSTOP, 794, 340, 70, 22, R_FAV_CLEAR);
        FillFavourites(s);

        ui.Make(L"BUTTON", L"Randomize these", BS_GROUPBOX, 12, 376, 440, 86, -1);
        const wchar_t* what[5] = { L"Theme", L"Pattern", L"Effect", L"Motion", L"Speed" };
        for (int i = 0; i < 5; i++) {
            ui.Make(L"BUTTON", what[i], BS_AUTOCHECKBOX | WS_TABSTOP, 24 + i * 84, 398, 80, 20, R_W_THEME + i);
            CheckDlgButton(dlg, R_W_THEME + i, (s->pool.what >> i) & 1 ? BST_CHECKED : BST_UNCHECKED);
        }
        ui.Make(L"BUTTON", L"Themes only from my favourites", BS_AUTOCHECKBOX | WS_TABSTOP, 24, 428, 210, 20, R_FAVONLY);
        ui.Make(L"BUTTON", L"Original colours can come up", BS_AUTOCHECKBOX | WS_TABSTOP, 240, 428, 205, 20, R_ORIGINAL);
        CheckDlgButton(dlg, R_FAVONLY, s->pool.favOnly ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dlg, R_ORIGINAL, s->pool.original ? BST_CHECKED : BST_UNCHECKED);

        ui.Make(L"BUTTON", L"Random speed range", BS_GROUPBOX, 464, 376, 446, 86, -1);
        ui.Make(L"STATIC", L"", 0, 476, 402, 120, 18, R_SMINL);
        ui.Make(TRACKBAR_CLASSW, L"", TBS_HORZ | WS_TABSTOP, 596, 396, 300, 26, R_SMIN);
        ui.Make(L"STATIC", L"", 0, 476, 432, 120, 18, R_SMAXL);
        ui.Make(TRACKBAR_CLASSW, L"", TBS_HORZ | WS_TABSTOP, 596, 426, 300, 26, R_SMAX);
        for (int id : { R_SMIN, R_SMAX }) SendMessageW(ui.I(id), TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(ui.I(R_SMIN), TBM_SETPOS, TRUE, s->pool.speedMin);
        SendMessageW(ui.I(R_SMAX), TBM_SETPOS, TRUE, s->pool.speedMax);
        ShowSpeedRange(s);

        ui.Make(L"BUTTON", L"Reset to defaults", WS_TABSTOP, 12, 476, 130, 28, IDRETRY);
        ui.Make(L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, 736, 476, 84, 28, IDOK);
        ui.Make(L"BUTTON", L"Cancel", WS_TABSTOP, 826, 476, 84, 28, IDCANCEL);
        FitClient(dlg, ui.dpi, 922, 516);
        return TRUE;
    }
    case WM_HSCROLL: {
        if (!s) break;
        int lo = (int)SendMessageW(s->ui.I(R_SMIN), TBM_GETPOS, 0, 0), hi = (int)SendMessageW(s->ui.I(R_SMAX), TBM_GETPOS, 0, 0);
        if (lo > hi) {   // keep slowest <= fastest
            if ((HWND)lp == s->ui.I(R_SMIN)) SendMessageW(s->ui.I(R_SMAX), TBM_SETPOS, TRUE, lo);
            else SendMessageW(s->ui.I(R_SMIN), TBM_SETPOS, TRUE, hi);
        }
        ShowSpeedRange(s);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if ((id >= R_ALL0 && id < R_ALL0 + 5) || (id >= R_NONE0 && id < R_NONE0 + 5)) {
            bool on = id < R_NONE0;
            HWND lv = s->ui.I(kLists[on ? id - R_ALL0 : id - R_NONE0]);
            for (int i = 0, n = ListView_GetItemCount(lv); i < n; i++) ListView_SetCheckState(lv, i, on);
            return TRUE;
        }
        switch (id) {
        case R_FAV_REMOVE: {
            int sel = (int)SendMessageW(s->ui.I(R_FAVS), LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)s->favs.size()) { s->favs.erase(s->favs.begin() + sel); FillFavourites(s); }
            return TRUE;
        }
        case R_FAV_CLEAR: s->favs.clear(); FillFavourites(s); return TRUE;
        case IDRETRY: {   // defaults: rebuild the dialog's ticks
            RandomPool d;
            DWORD masks[5] = { 0, d.modes, d.patterns, d.effects, d.motions };
            for (int c = 0; c < 5; c++) {
                HWND lv = s->ui.I(kLists[c]);
                for (int i = 0, n = ListView_GetItemCount(lv); i < n; i++) ListView_SetCheckState(lv, i, c == 0 ? true : ((masks[c] >> i) & 1) != 0);
            }
            for (int i = 0; i < 5; i++) CheckDlgButton(dlg, R_W_THEME + i, (d.what >> i) & 1 ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dlg, R_FAVONLY, d.favOnly ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(dlg, R_ORIGINAL, d.original ? BST_CHECKED : BST_UNCHECKED);
            SendMessageW(s->ui.I(R_SMIN), TBM_SETPOS, TRUE, d.speedMin);
            SendMessageW(s->ui.I(R_SMAX), TBM_SETPOS, TRUE, d.speedMax);
            ShowSpeedRange(s);
            return TRUE;
        }
        case IDOK: {
            RandomPool& p = s->pool;
            HWND pl = s->ui.I(R_PAL);
            for (int i = 0; i < kPaletteCount; i++) p.SetPalette(i, ListView_GetCheckState(pl, i) != 0);
            p.modes = ReadCheckList(s->ui.I(R_MODE));
            p.patterns = ReadCheckList(s->ui.I(R_PATTERN));
            p.effects = ReadCheckList(s->ui.I(R_EFFECT));
            p.motions = ReadCheckList(s->ui.I(R_MOTION));
            p.what = 0;
            for (int i = 0; i < 5; i++) if (IsDlgButtonChecked(dlg, R_W_THEME + i) == BST_CHECKED) p.what |= 1u << i;
            p.favOnly = IsDlgButtonChecked(dlg, R_FAVONLY) == BST_CHECKED;
            p.original = IsDlgButtonChecked(dlg, R_ORIGINAL) == BST_CHECKED;
            p.speedMin = (DWORD)SendMessageW(s->ui.I(R_SMIN), TBM_GETPOS, 0, 0);
            p.speedMax = (DWORD)SendMessageW(s->ui.I(R_SMAX), TBM_GETPOS, 0, 0);
            SavePool(p);
            SaveFavourites(s->favs);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL: EndDialog(dlg, IDCANCEL); return TRUE;
        }
        break;
    }
    case WM_DESTROY:
        if (s) { DeleteObject(s->ui.font); DeleteObject(s->ui.bold); }
        break;
    }
    return FALSE;
}

inline void ShowRandomizer(HWND owner) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    RandomizerState s;
    s.pool = LoadPool();
    s.favs = LoadFavourites();
    RunDialog(owner, L"Randomizer & Favourites", RandomizerProc, (LPARAM)&s);
}

// ---------------------------------------------------------------------------
// Live preview: the saver itself, started as "<scr> /p <window>"
// ---------------------------------------------------------------------------
struct Preview {
    HWND child = nullptr;
    PROCESS_INFORMATION pi = {};

    static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_ERASEBKGND) { RECT rc; GetClientRect(h, &rc); FillRect((HDC)w, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH)); return 1; }
        return DefWindowProcW(h, m, w, l);
    }

    void Stop() {
        if (child) { DestroyWindow(child); child = nullptr; }   // the saver ends with its parent window
        if (pi.hProcess) {
            if (WaitForSingleObject(pi.hProcess, 1500) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            ZeroMemory(&pi, sizeof(pi));
        }
    }

    // `rc`: client rectangle of `parent` to fill.
    void Start(HWND parent, const RECT& rc, const wchar_t* saverPath) {
        Stop();
        HINSTANCE inst = (HINSTANCE)GetModuleHandleW(nullptr);
        WNDCLASSW wc = {};
        if (!GetClassInfoW(inst, L"StyleUiPreview", &wc)) {
            wc.lpfnWndProc = Proc;
            wc.hInstance = inst;
            wc.lpszClassName = L"StyleUiPreview";
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            RegisterClassW(&wc);
        }
        child = CreateWindowExW(0, L"StyleUiPreview", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, parent, nullptr, inst, nullptr);
        std::wstring cmd = L"\"" + std::wstring(saverPath) + L"\" /p " + std::to_wstring((unsigned long long)(UINT_PTR)child);
        std::vector<wchar_t> buf(cmd.begin(), cmd.end());
        buf.push_back(0);
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        if (!CreateProcessW(saverPath, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) ZeroMemory(&pi, sizeof(pi));
    }
};

// ---------------------------------------------------------------------------
// "Themes & Effects" window: preview + editor, saved as you go.
// ---------------------------------------------------------------------------
struct StyleDlgState {
    Ui ui;
    Editor ed;
    Preview preview;
    std::wstring path, file;
    RECT previewRc = {};
};

static const UINT_PTR kPreviewTimer = 7;

inline INT_PTR CALLBACK StyleDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* s = (StyleDlgState*)GetWindowLongPtrW(dlg, DWLP_USER);
    switch (msg) {
    case WM_INITDIALOG: {
        s = (StyleDlgState*)lp;
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        Ui& ui = s->ui;
        ui.w = dlg;
        ui.dpi = WindowDpi(dlg);
        ui.font = MakeFont(ui.dpi, false);
        ui.bold = MakeFont(ui.dpi, true);
        // 16:9 preview, centred above the editor.
        HWND frame = ui.Make(L"STATIC", L"", SS_BLACKRECT, 92, 12, 480, 270, -1);
        GetWindowRect(frame, &s->previewRc);
        MapWindowPoints(nullptr, dlg, (POINT*)&s->previewRc, 2);
        s->ed.Create(dlg, 12, 294, ui.dpi, ui.font, ui.bold);
        s->ed.Load(Load(s->file.c_str()));
        ui.Make(L"STATIC", L"Changes are saved straight away; the preview restarts to show them.", 0, 12, 586, 440, 18, -1);
        ui.Make(L"BUTTON", L"Close", BS_DEFPUSHBUTTON | WS_TABSTOP, 562, 580, 90, 28, IDOK);
        FitClient(dlg, ui.dpi, 664, 620);
        s->preview.Start(dlg, s->previewRc, s->path.c_str());
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) { EndDialog(dlg, IDOK); return TRUE; }
        if (s && s->ed.OnCommand(wp)) {
            Save(s->file.c_str(), s->ed.Read());
            SetTimer(dlg, kPreviewTimer, 300, nullptr);
        }
        return TRUE;
    case WM_HSCROLL:
        if (s && s->ed.OnHScroll(wp, lp)) {
            Save(s->file.c_str(), s->ed.Read());
            SetTimer(dlg, kPreviewTimer, 300, nullptr);
        }
        return TRUE;
    case WM_TIMER:
        if (wp == kPreviewTimer) { KillTimer(dlg, kPreviewTimer); s->preview.Start(dlg, s->previewRc, s->path.c_str()); }
        return TRUE;
    case WM_DESTROY:
        if (s) { s->preview.Stop(); DeleteObject(s->ui.font); DeleteObject(s->ui.bold); }
        break;
    }
    return FALSE;
}

// saverPath: the .scr to preview; saverFile: its name without extension.
inline void ShowStyleDialog(HWND owner, const wchar_t* saverPath, const wchar_t* saverFile) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    StyleDlgState s;
    s.path = saverPath;
    s.file = saverFile;
    std::wstring title = std::wstring(L"Themes & Effects - ") + saverFile;
    RunDialog(owner, title.c_str(), StyleDlgProc, (LPARAM)&s);
}

}  // namespace styleui
