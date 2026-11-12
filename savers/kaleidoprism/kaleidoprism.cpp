// kaleidoprism.cpp - "Kaleidoscope Prism": a kaleidoscope computed per pixel
// on the GPU (shader.h), with 1 to 15 cuts, six mirror layouts, thirteen
// patterns, twelve palettes and a settings dialog built from one table.
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include "shader.h"
#include <commctrl.h>
#include <map>
#include <string>

// ---------------------------------------------------------------------------
// Options. One table drives the registry, the dialog, presets and Randomize.
// ---------------------------------------------------------------------------
enum {
    O_DIV, O_SYM, O_LAYOUT, O_MORPH, O_MORPHTO, O_TWIST, O_RINGS, O_ROT, O_DRIFT, O_ZOOM, O_BREATH,
    O_PATTERN, O_SPEED, O_DETAIL, O_WARP, O_FLOW, O_SEAM, O_SEAMW, O_SPARK, O_JEWEL, O_QUALITY,
    O_PALETTE, O_CYCLE, O_BANDS, O_VIB, O_BRIGHT, O_GLOW, O_FRINGE, O_VIGNETTE, O_SURPRISE, O_BAND, O_EVERY, O_LENGTH,
    O_COUNT
};
enum Kind { COMBO, SLIDER, CHECK };
enum { PATTERNS = 13, PALETTES = 12 };

struct Opt {
    Kind kind;
    int col;                  // dialog column
    const wchar_t* label;
    const wchar_t* reg;       // registry value name
    int lo, hi, def;
    const wchar_t* names;     // combo items separated by '|'
    bool random;              // touched by Randomize / Surprise
};

static const Opt kOpts[O_COUNT] = {
    { SLIDER, 0, L"Cuts (divisions)", L"Cuts", 1, 15, 12, nullptr, true },
    { COMBO,  0, L"Symmetry:", L"Symmetry", 0, 1, 0, L"Mirrored|Pinwheel (no mirror)", true },
    { COMBO,  0, L"Layout:", L"Layout", 0, 5, 0, L"Classic wedges|Triangle mirror box|Square mirror box|Hexagon rosettes|Infinite tunnel|Nested rings", true },
    { CHECK,  0, L"Morph the number of cuts over time", L"Morph", 0, 1, 0, nullptr, true },
    { SLIDER, 0, L"Morph between Cuts and", L"CutsMorphTo", 1, 15, 6, nullptr, true },
    { SLIDER, 0, L"Spiral twist", L"Twist", -100, 100, 0, nullptr, true },
    { SLIDER, 0, L"Rings / tile density", L"Rings", 1, 12, 4, nullptr, true },
    { SLIDER, 0, L"Rotation (negative = anticlockwise)", L"Rotation", -100, 100, 20, nullptr, true },
    { SLIDER, 0, L"Centre drift", L"Drift", 0, 100, 30, nullptr, true },
    { SLIDER, 0, L"Zoom", L"Zoom", 0, 100, 50, nullptr, true },
    { SLIDER, 0, L"Zoom breathing", L"Breath", 0, 100, 20, nullptr, true },

    { COMBO,  1, L"Pattern:", L"PatternV2", 0, 13, 0, L"Silk|Stained glass|Gem facets|Petals|Ripples|Marble|Woven arcs|Glass beads|Ribbons|Solid stripes|Solid rings|Solid tiles|Solid triangles|Cycle through all", true },
    { SLIDER, 1, L"Speed", L"Speed", 0, 100, 40, nullptr, false },
    { SLIDER, 1, L"Detail", L"Detail", 0, 100, 50, nullptr, true },
    { SLIDER, 1, L"Liquid warp", L"Warp", 0, 100, 30, nullptr, true },
    { SLIDER, 1, L"Flow (pattern sliding past the mirrors)", L"Flow", 0, 100, 50, nullptr, true },
    { COMBO,  1, L"Mirror seams:", L"Seams", 0, 3, 0, L"Invisible|Glowing|Dark lead|Gold wire", true },
    { SLIDER, 1, L"Seam width", L"SeamWidth", 0, 100, 30, nullptr, true },
    { SLIDER, 1, L"Sparkles", L"Sparkles", 0, 100, 30, nullptr, true },
    { CHECK,  1, L"Jewel in the centre", L"Jewel", 0, 1, 1, nullptr, true },
    { COMBO,  1, L"Quality:", L"Quality", 0, 3, 0, L"Automatic|Standard (fastest)|High (2x)|Ultra (4x)", false },

    { COMBO,  2, L"Palette:", L"Palette", 0, 12, 0, L"Rainbow|Jewel|Neon|Sunset|Ocean|Aurora|Candy|Fire|Ice|Peacock|Royal|Ultraviolet|Cycle through all", true },
    { SLIDER, 2, L"Colour cycling", L"ColourCycle", 0, 100, 30, nullptr, true },
    { SLIDER, 2, L"Colour bands", L"Bands", 0, 100, 50, nullptr, true },
    { SLIDER, 2, L"Vibrance", L"Vibrance", 0, 100, 70, nullptr, true },
    { SLIDER, 2, L"Brightness", L"Brightness", 10, 100, 85, nullptr, false },
    { SLIDER, 2, L"Glow", L"Glow", 0, 100, 40, nullptr, true },
    { SLIDER, 2, L"Prism fringe", L"Fringe", 0, 100, 20, nullptr, true },
    { CHECK,  2, L"Vignette (darker corners)", L"Vignette", 0, 1, 1, nullptr, true },
    { CHECK,  2, L"Surprise me: new look after each rest", L"Surprise", 0, 1, 0, nullptr, false },
    { COMBO,  2, L"Rest band:", L"RestBand", 0, 2, 0, L"Off|Subtle|Strong", false },
    { SLIDER, 2, L"Rest to black every (minutes)", L"RestMinutes", 1, 15, 5, nullptr, false },
    { SLIDER, 2, L"Rest length (seconds)", L"RestSeconds", 5, 60, 15, nullptr, false },
};

static int g_opt[O_COUNT];

struct Preset { const wchar_t* name; int n; int kv[12][2]; };
static const Preset kPresets[] = {
    { L"Classic", 0, {} },
    { L"Cathedral window", 6, { { O_PATTERN, 1 }, { O_DIV, 8 }, { O_SEAM, 2 }, { O_PALETTE, 1 }, { O_GLOW, 30 }, { O_SEAMW, 25 } } },
    { L"Neon tunnel", 7, { { O_LAYOUT, 4 }, { O_PATTERN, 6 }, { O_PALETTE, 2 }, { O_DIV, 10 }, { O_TWIST, 35 }, { O_GLOW, 70 }, { O_RINGS, 6 } } },
    { L"Jewel box", 7, { { O_LAYOUT, 1 }, { O_PATTERN, 2 }, { O_PALETTE, 1 }, { O_RINGS, 5 }, { O_SPARK, 70 }, { O_SEAM, 1 }, { O_SEAMW, 15 } } },
    { L"Liquid opal", 6, { { O_PATTERN, 5 }, { O_PALETTE, 6 }, { O_DIV, 6 }, { O_WARP, 60 }, { O_FRINGE, 60 }, { O_BANDS, 65 } } },
    { L"Hex garden", 9, { { O_LAYOUT, 3 }, { O_PATTERN, 3 }, { O_PALETTE, 9 }, { O_DIV, 6 }, { O_RINGS, 3 }, { O_BANDS, 90 }, { O_DETAIL, 75 }, { O_SEAM, 1 }, { O_SEAMW, 8 } } },
    { L"Hypnotic spiral", 6, { { O_TWIST, 80 }, { O_PATTERN, 8 }, { O_PALETTE, 0 }, { O_DIV, 15 }, { O_ROT, 50 }, { O_CYCLE, 60 } } },
    { L"Mandala rings", 7, { { O_LAYOUT, 5 }, { O_PATTERN, 3 }, { O_PALETTE, 10 }, { O_DIV, 12 }, { O_RINGS, 5 }, { O_SEAM, 3 }, { O_SEAMW, 20 } } },
    { L"Deep ocean", 6, { { O_PATTERN, 4 }, { O_PALETTE, 4 }, { O_DIV, 10 }, { O_SPEED, 25 }, { O_SPARK, 10 }, { O_GLOW, 55 } } },
    { L"Bead curtain", 5, { { O_LAYOUT, 2 }, { O_PATTERN, 7 }, { O_PALETTE, 3 }, { O_RINGS, 4 }, { O_GLOW, 60 } } },
    { L"Snowflake", 8, { { O_DIV, 6 }, { O_PATTERN, 2 }, { O_PALETTE, 8 }, { O_SEAM, 1 }, { O_SEAMW, 10 }, { O_SPARK, 80 }, { O_ZOOM, 40 }, { O_DETAIL, 65 } } },
    { L"Paper cut-outs", 6, { { O_PATTERN, 11 }, { O_PALETTE, 6 }, { O_DIV, 8 }, { O_SPARK, 0 }, { O_FRINGE, 0 }, { O_GLOW, 15 } } },
    { L"Solid pinwheel", 7, { { O_PATTERN, 9 }, { O_PALETTE, 0 }, { O_DIV, 10 }, { O_TWIST, 45 }, { O_SPARK, 0 }, { O_FRINGE, 0 }, { O_WARP, 0 } } },
    { L"Solid target", 6, { { O_PATTERN, 10 }, { O_PALETTE, 2 }, { O_DIV, 5 }, { O_SPARK, 0 }, { O_FRINGE, 0 }, { O_WARP, 15 } } },
    { L"Everything changes", 5, { { O_PATTERN, 13 }, { O_PALETTE, 12 }, { O_MORPH, 1 }, { O_DIV, 3 }, { O_MORPHTO, 15 } } },
};
enum { PRESET_COUNT = sizeof(kPresets) / sizeof(kPresets[0]) };

static void Defaults(int* v) { for (int i = 0; i < O_COUNT; i++) v[i] = kOpts[i].def; }
static void ApplyPreset(int* v, int index) {
    // Keeps speed, brightness, quality and the rest settings as they are.
    for (int i = 0; i < O_COUNT; i++) if (kOpts[i].random) v[i] = kOpts[i].def;
    for (int k = 0; k < kPresets[index].n; k++) v[kPresets[index].kv[k][0]] = kPresets[index].kv[k][1];
}
static void Randomize(int* v) {
    for (int i = 0; i < O_COUNT; i++) if (kOpts[i].random) v[i] = RandI(kOpts[i].lo, kOpts[i].hi);
    // Keep the result good-looking: favour the classic layout and mirrored
    // symmetry, and keep the extremes of a few sliders out.
    if (RandI(0, 2) == 0) v[O_LAYOUT] = 0;
    if (RandI(0, 3) != 0) v[O_SYM] = 0;
    v[O_DIV] = RandI(2, 15);
    v[O_MORPHTO] = RandI(2, 15);
    v[O_TWIST] = RandI(0, 2) == 0 ? RandI(-70, 70) : 0;
    v[O_ZOOM] = RandI(30, 75);
    v[O_DETAIL] = RandI(30, 75);
    v[O_VIB] = RandI(55, 100);
    v[O_SEAMW] = RandI(5, 50);
    v[O_FRINGE] = RandI(0, 60);
    v[O_RINGS] = RandI(2, 7);
}

const wchar_t* RegistryName() { return L"KaleidoPrism"; }
void LoadSettings() {
    for (int i = 0; i < O_COUNT; i++) {
        const Opt& o = kOpts[i];
        // Stored as an offset from the lowest value, so negatives fit a DWORD.
        DWORD raw = RegReadDword(o.reg, (DWORD)(o.def - o.lo));
        g_opt[i] = raw > (DWORD)(o.hi - o.lo) ? o.def : (int)raw + o.lo;
    }
}
static void SaveSettings() {
    for (int i = 0; i < O_COUNT; i++) RegWriteDword(kOpts[i].reg, (DWORD)(g_opt[i] - kOpts[i].lo));
}

// ---------------------------------------------------------------------------
// Settings dialog. The controls are created from kOpts, three columns.
// ---------------------------------------------------------------------------
enum { IDD_PRISM = 400, IDC_LABEL = 2000, IDC_CTRL = 2100, IDC_PRESET = 1900, IDC_RANDOM, IDC_DEFAULTS, IDC_DISPLAY, IDC_STYLE };

static HWND Make(HWND dlg, const wchar_t* cls, const wchar_t* text, DWORD style, int id, int x, int y, int w, int h) {
    RECT r = { x, y, x + w, y + h };
    MapDialogRect(dlg, &r);
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left, r.bottom - r.top,
                             dlg, (HMENU)(INT_PTR)id, g_hInst, nullptr);
    SendMessageW(c, WM_SETFONT, SendMessageW(dlg, WM_GETFONT, 0, 0), FALSE);
    return c;
}

static void ShowValue(HWND dlg, int i, int value) {
    wchar_t buf[128];
    wsprintfW(buf, L"%s: %d", kOpts[i].label, value);
    SetDlgItemTextW(dlg, IDC_LABEL + i, buf);
}

static void SetControls(HWND dlg, const int* v) {
    for (int i = 0; i < O_COUNT; i++) {
        HWND c = GetDlgItem(dlg, IDC_CTRL + i);
        switch (kOpts[i].kind) {
        case COMBO:  SendMessageW(c, CB_SETCURSEL, v[i], 0); break;
        case CHECK:  SendMessageW(c, BM_SETCHECK, v[i] ? BST_CHECKED : BST_UNCHECKED, 0); break;
        case SLIDER:
            // Move the thumb away first so it is always repositioned, even
            // when the value equals the position it was created with.
            SendMessageW(c, TBM_SETPOS, FALSE, v[i] == kOpts[i].lo ? kOpts[i].hi : kOpts[i].lo);
            SendMessageW(c, TBM_SETPOS, TRUE, v[i]);
            ShowValue(dlg, i, v[i]);
            break;
        }
    }
}

static void GetControls(HWND dlg, int* v) {
    for (int i = 0; i < O_COUNT; i++) {
        HWND c = GetDlgItem(dlg, IDC_CTRL + i);
        switch (kOpts[i].kind) {
        case COMBO:  v[i] = (int)SendMessageW(c, CB_GETCURSEL, 0, 0); break;
        case CHECK:  v[i] = SendMessageW(c, BM_GETCHECK, 0, 0) == BST_CHECKED; break;
        case SLIDER: v[i] = (int)SendMessageW(c, TBM_GETPOS, 0, 0); break;
        }
        if (v[i] < kOpts[i].lo || v[i] > kOpts[i].hi) v[i] = kOpts[i].def;
    }
}

static void AddItems(HWND combo, const wchar_t* names) {
    while (*names) {
        wchar_t item[64]; int n = 0;
        while (*names && *names != L'|' && n < 63) item[n++] = *names++;
        item[n] = 0;
        if (*names == L'|') names++;
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)item);
    }
}

static void BuildDialog(HWND dlg) {
    const int colW = 156, gap = 6, left = 7, top = 26;
    const wchar_t* titles[3] = { L"Mirrors and motion", L"Pattern", L"Colour, finish and screen rest" };

    Make(dlg, L"STATIC", L"&Preset:", 0, -1, left, 9, 28, 8);
    HWND preset = Make(dlg, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_PRESET, left + 30, 7, 110, 200);
    for (const Preset& p : kPresets) SendMessageW(preset, CB_ADDSTRING, 0, (LPARAM)p.name);
    Make(dlg, L"BUTTON", L"&Randomize", BS_PUSHBUTTON | WS_TABSTOP, IDC_RANDOM, left + 146, 6, 56, 14);
    Make(dlg, L"BUTTON", L"D&efaults", BS_PUSHBUTTON | WS_TABSTOP, IDC_DEFAULTS, left + 206, 6, 50, 14);
    Make(dlg, L"STATIC", L"Nothing is saved until you press OK.", 0, -1, left + 264, 9, 220, 8);

    int y[3] = { top + 12, top + 12, top + 12 };
    HWND group[3];
    for (int c = 0; c < 3; c++) group[c] = Make(dlg, L"BUTTON", titles[c], BS_GROUPBOX, -1, left + c * (colW + gap), top, colW, 10);
    for (int i = 0; i < O_COUNT; i++) {
        const Opt& o = kOpts[i];
        int x = left + o.col * (colW + gap) + 6, w = colW - 12, &cy = y[o.col];
        switch (o.kind) {
        case COMBO:
            Make(dlg, L"STATIC", o.label, 0, IDC_LABEL + i, x, cy + 2, 46, 8);
            AddItems(Make(dlg, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_CTRL + i, x + 48, cy, w - 48, 220), o.names);
            cy += 17;
            break;
        case CHECK:
            Make(dlg, L"BUTTON", o.label, BS_AUTOCHECKBOX | WS_TABSTOP, IDC_CTRL + i, x, cy, w, 10);
            cy += 14;
            break;
        case SLIDER: {
            Make(dlg, L"STATIC", o.label, 0, IDC_LABEL + i, x, cy, w, 8);
            HWND tb = Make(dlg, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, IDC_CTRL + i, x - 4, cy + 9, w + 8, 13);
            SendMessageW(tb, TBM_SETRANGEMIN, FALSE, o.lo);
            SendMessageW(tb, TBM_SETRANGEMAX, TRUE, o.hi);
            SendMessageW(tb, TBM_SETPAGESIZE, 0, (o.hi - o.lo) / 10 > 0 ? (o.hi - o.lo) / 10 : 1);
            cy += 25;
            break; }
        }
    }
    int bottom = y[0] > y[1] ? y[0] : y[1];
    if (y[2] > bottom) bottom = y[2];
    bottom += 4;
    for (int c = 0; c < 3; c++) {
        RECT r = { left + c * (colW + gap), top, left + c * (colW + gap) + colW, bottom };
        MapDialogRect(dlg, &r);
        SetWindowPos(group[c], nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    int width = left * 2 + 3 * colW + 2 * gap, by = bottom + 7;
    Make(dlg, L"BUTTON", L"&Display Settings...", BS_PUSHBUTTON | WS_TABSTOP, IDC_DISPLAY, left, by, 76, 14);
    Make(dlg, L"BUTTON", L"&Themes && Effects...", BS_PUSHBUTTON | WS_TABSTOP, IDC_STYLE, left + 80, by, 80, 14);
    Make(dlg, L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK, width - left - 104, by, 50, 14);
    Make(dlg, L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL, width - left - 50, by, 50, 14);

    RECT rc = { 0, 0, width, by + 21 };
    MapDialogRect(dlg, &rc);
    AdjustWindowRectEx(&rc, GetWindowLongW(dlg, GWL_STYLE), FALSE, GetWindowLongW(dlg, GWL_EXSTYLE));
    RECT cur; GetWindowRect(dlg, &cur);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    SetWindowPos(dlg, nullptr, (cur.left + cur.right - w) / 2, (cur.top + cur.bottom - h) / 2, w, h, SWP_NOZORDER);
    SetControls(dlg, g_opt);
}

static INT_PTR CALLBACK PrismDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG:
        BuildDialog(dlg);
        return TRUE;
    case WM_HSCROLL: {
        int i = GetDlgCtrlID((HWND)lp) - IDC_CTRL;
        if (i >= 0 && i < O_COUNT && kOpts[i].kind == SLIDER) ShowValue(dlg, i, (int)SendMessageW((HWND)lp, TBM_GETPOS, 0, 0));
        return TRUE; }
    case WM_COMMAND: {
        int v[O_COUNT];
        switch (LOWORD(wp)) {
        case IDC_PRESET:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendDlgItemMessageW(dlg, IDC_PRESET, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < PRESET_COUNT) { GetControls(dlg, v); ApplyPreset(v, sel); SetControls(dlg, v); }
            }
            return TRUE;
        case IDC_RANDOM:
            GetControls(dlg, v); Randomize(v); SetControls(dlg, v);
            SendDlgItemMessageW(dlg, IDC_PRESET, CB_SETCURSEL, (WPARAM)-1, 0);
            return TRUE;
        case IDC_DEFAULTS:
            Defaults(v); SetControls(dlg, v);
            SendDlgItemMessageW(dlg, IDC_PRESET, CB_SETCURSEL, 0, 0);
            return TRUE;
        case IDC_DISPLAY: ShowDisplaySettings(dlg); return TRUE;
        case IDC_STYLE:   ShowStyleSettings(dlg); return TRUE;
        case IDOK:
            GetControls(dlg, g_opt);
            SaveSettings();
            EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break; }
    }
    return FALSE;
}

void ShowConfigDialog(HWND parent) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_PRISM), parent, PrismDlgProc, 0);
}

// ---------------------------------------------------------------------------
// Palettes: six colours each, looping.
// ---------------------------------------------------------------------------
static const unsigned kPalettes[PALETTES][6] = {
    { 0xFF0040, 0xFF8A00, 0xFFE600, 0x00E676, 0x00B0FF, 0xB040FF },   // Rainbow
    { 0xE0115F, 0x0F52BA, 0x50C878, 0x9966CC, 0xFFC87C, 0x00A8A8 },   // Jewel
    { 0xFF00E5, 0x00F0FF, 0x7CFF00, 0xFFF200, 0xFF3D00, 0x8A2BFF },   // Neon
    { 0xFF5E3A, 0xFF2A68, 0xC644FC, 0x5856D6, 0xFF9500, 0xFFCC00 },   // Sunset
    { 0x001F54, 0x0077B6, 0x00B4D8, 0x90E0EF, 0x00F5D4, 0x3A0CA3 },   // Ocean
    { 0x00FF9F, 0x00B8FF, 0x001EFF, 0xBD00FF, 0xD600FF, 0x12FFD1 },   // Aurora
    { 0xFF6FB5, 0xFFD166, 0x06D6A0, 0x4CC9F0, 0xB388FF, 0xFF9E80 },   // Candy
    { 0x200000, 0xC1121F, 0xFF5400, 0xFFBD00, 0xFFF3B0, 0x7A0010 },   // Fire
    { 0x03045E, 0x0096C7, 0xADE8F4, 0xFFFFFF, 0x7B8CFF, 0x48CAE4 },   // Ice
    { 0x005F73, 0x0A9396, 0x94D2BD, 0xEE9B00, 0x6A00F4, 0x00E5A0 },   // Peacock
    { 0x7B0828, 0xD4AF37, 0xFFE9A8, 0xB5179E, 0x3C096C, 0xFF9F1C },   // Royal
    { 0x10002B, 0x5A189A, 0x9D4EDD, 0xE0AAFF, 0xFF4D9E, 0x3A86FF },   // Ultraviolet
};

using namespace rest;

// Holds a value, then cross-fades to the next one: hold, fade, hold, ...
struct Cycler {
    int a = 0, b = 0;
    float mix = 0, timer = 0;
    void Set(int v) { a = b = v; mix = 0; timer = 0; }
    template <class Next> void Step(float dt, float hold, float fade, Next next) {
        timer += dt;
        if (a == b) {
            if (timer > hold) { b = next(a); timer = 0; }
        } else {
            mix = Smooth(timer / fade);
            if (timer >= fade) { a = b; mix = 0; timer = 0; }
        }
    }
};

// One compiled variant of the shader. Variants needed later (the next
// pattern in a cycle, a lower quality level) compile on a worker thread, so
// no frame ever waits for the compiler.
struct Variant {
    Renderer* r = nullptr;
    std::string src;
    ScreenShader* shader = nullptr;
    HANDLE thread = nullptr;
    volatile LONG done = 0;
    static DWORD WINAPI Run(LPVOID p) {
        Variant* v = (Variant*)p;
        v->shader = v->r->CreateScreenShader(v->src.c_str());
        InterlockedExchange(&v->done, 1);
        return 0;
    }
    void Compile(bool wait) {
        if (!wait) thread = CreateThread(nullptr, 0, Run, this, 0, nullptr);
        if (!thread) Run(this);
    }
    bool Ready() const { return done != 0; }
    ~Variant() {
        if (thread) { WaitForSingleObject(thread, INFINITE); CloseHandle(thread); }
        delete shader;
    }
};

class SaverScene : public RestScene {
    int v[O_COUNT];                 // this monitor's settings (Surprise changes them)
    std::map<int, Variant*> variants;   // key: quality level and pattern
    int level = 1;                  // 0, 1, 2 = 1, 2, 4 samples per pixel
    float slowTime = 0, frameAvg = 0, settle = 0;

    float pt = 0;                   // pattern time (scaled by Speed)
    float rot = 0, chamber = 0, flowX = 0, flowY = 0, flowDir = 0, phase = 0;
    float seed[4] = {};
    Cycler divs, pattern, palette;
    int nextPattern = 0;

    float F(int i) const { return (v[i] - kOpts[i].lo) / (float)(kOpts[i].hi - kOpts[i].lo); }

    void RestSettings(float& every, float& len, int& band) const override {
        every = 60.0f * v[O_EVERY];
        len = (float)v[O_LENGTH];
        band = v[O_BAND];
    }

    void ClearVariants() {
        for (auto& kv : variants) delete kv.second;
        variants.clear();
    }
    ~SaverScene() { ClearVariants(); }

    void Setup(Renderer& r) override {
        memcpy(v, g_opt, sizeof(v));
        r.UseSingleSample();        // the shader does its own anti-aliasing
    }

    // The variant for a quality level and pattern; everything else in the
    // key is fixed between rests. Null while it is still compiling.
    ScreenShader* Shader(Renderer& r, int lvl, int pat, bool wait) {
        Variant*& var = variants[lvl * 100 + pat];
        if (!var) {
            char defs[256];
            wsprintfA(defs, "#define SAMPLES %d\n#define LAYOUT %d\n#define PATTERN %d\n#define SYM %d\n#define SEAM %d\n"
                            "#define SPARK %d\n#define FRINGE %d\n#define JEWEL %d\n",
                      1 << lvl, v[O_LAYOUT], pat, v[O_SYM], v[O_SEAM], v[O_SPARK] > 0, v[O_FRINGE] > 0, Centred() && v[O_JEWEL]);
            var = new Variant;
            var->r = &r;
            var->src = std::string(defs) + kPrismShader;
            var->Compile(wait);
        }
        if (wait && !var->Ready()) WaitForSingleObject(var->thread, INFINITE);
        return var->Ready() ? var->shader : nullptr;
    }
    bool Centred() const { return v[O_LAYOUT] == 0 || v[O_LAYOUT] == 4 || v[O_LAYOUT] == 5; }
    int OtherPattern(int cur) const { return (cur + RandI(1, PATTERNS - 1)) % PATTERNS; }

    void Reseed() override {
        if (variation > 0 && v[O_SURPRISE]) { Randomize(v); ClearVariants(); }
        for (float& s : seed) s = RandF(0, 100);
        pt = RandF(0, 500);
        rot = RandF(0, 2 * kPi); chamber = RandF(0, 2 * kPi); flowDir = RandF(0, 2 * kPi);
        flowX = RandF(-20, 20); flowY = RandF(-20, 20);
        phase = RandF(0, 1);
        divs.Set(v[O_DIV]);
        pattern.Set(v[O_PATTERN] >= PATTERNS ? RandI(0, PATTERNS - 1) : v[O_PATTERN]);
        nextPattern = OtherPattern(pattern.a);
        palette.Set(v[O_PALETTE] >= PALETTES ? RandI(0, PALETTES - 1) : v[O_PALETTE]);
        // Automatic starts at 2 samples per pixel and gets a fresh chance
        // after every rest.
        int q = v[O_QUALITY];
        level = q == 0 ? 1 : q - 1;
        slowTime = 0; settle = 0;
    }

    // Automatic quality: when frames take clearly longer than the monitor's
    // refresh, drop to one sample per pixel.
    void AutoQuality(Renderer& r, float dt, bool fading) {
        if (v[O_QUALITY] != 0 || preview || level == 0) return;
        settle += dt;
        if (settle < 3) { frameAvg = (float)r.RefreshPeriod(); return; }   // ignore start-up
        if (fading) { slowTime = 0; return; }                              // a cross-fade costs double; don't judge by it
        frameAvg += (fminf(dt, 0.25f) - frameAvg) * 0.05f;
        slowTime = frameAvg > r.RefreshPeriod() * 1.2 ? slowTime + dt : 0;
        if (slowTime > 1.0f && Shader(r, level - 1, pattern.a, false)) { level--; slowTime = 0; settle = 0; }
    }

    void Draw(Renderer& r, float dt) override {
        bool fading = pattern.a != pattern.b || divs.a != divs.b;
        AutoQuality(r, dt, fading);
        ScreenShader* s = Shader(r, level, pattern.a, true);
        if (!s && level > 0) { level = 0; s = Shader(r, 0, pattern.a, true); }
        if (!s) return;
        // Have the cheaper variant ready before it is needed.
        if (v[O_QUALITY] == 0 && level > 0 && !preview) Shader(r, level - 1, pattern.a, false);

        float speed = 0.15f + F(O_SPEED) * 1.85f, sdt = dt * speed;
        pt += sdt;
        rot += sdt * (v[O_ROT] / 100.0f) * 0.5f;
        chamber += sdt * 0.05f;
        flowDir += sdt * 0.07f * sinf(pt * 0.05f + seed[0]);
        float flow = F(O_FLOW) * 0.45f;
        flowX += cosf(flowDir) * flow * sdt; flowY += sinf(flowDir) * flow * sdt;
        phase += sdt * F(O_CYCLE) * 0.25f;
        phase -= floorf(phase);

        // Only one of cuts and pattern cross-fades at a time (one extra pass).
        if (v[O_MORPH] && pattern.a == pattern.b) {
            int lo = v[O_DIV] < v[O_MORPHTO] ? v[O_DIV] : v[O_MORPHTO], hi = v[O_DIV] + v[O_MORPHTO] - lo;
            divs.Step(sdt, 14, 5, [&](int cur) { int n = RandI(lo, hi); return n == cur && hi > lo ? (cur < hi ? cur + 1 : cur - 1) : n; });
        }
        if (v[O_PATTERN] >= PATTERNS && divs.a == divs.b) {
            // The next pattern compiles in the background during the hold.
            bool ready = Shader(r, level, nextPattern, false) != nullptr;
            if (pattern.a != pattern.b || ready) {
                int before = pattern.a;
                pattern.Step(sdt, 22, 6, [&](int) { return nextPattern; });
                if (pattern.a != before) nextPattern = OtherPattern(pattern.a);
            }
        }
        if (v[O_PALETTE] >= PALETTES) palette.Step(sdt, 16, 8, [](int cur) { return (cur + RandI(1, PALETTES - 1)) % PALETTES; });

        float minDim = (float)(width < height ? width : height);
        float drift = F(O_DRIFT) * 0.28f;
        float zoom = expf((F(O_ZOOM) - 0.5f) * 2.0f + sinf(pt * 0.23f + seed[1]) * F(O_BREATH) * 0.5f);
        float sw = 0.002f + F(O_SEAMW) * 0.02f;

        float c[60] = {
            (float)width, (float)height, 2.0f / minDim, pt,
            (float)divs.a, 0, 0, 0,
            0, v[O_TWIST] / 100.0f * 1.5f, (float)v[O_RINGS], rot,
            sinf(pt * 0.11f + seed[2]) * drift, cosf(pt * 0.087f + seed[3]) * drift, 1 / zoom, 2.0f * expf((F(O_DETAIL) - 0.5f) * 2.4f),
            0, 0, 0, F(O_WARP) * 0.6f,
            flowX, flowY, chamber, F(O_SPARK) * 1.6f,
            0, sw / zoom, 0, v[O_VIGNETTE] ? 0.5f : 0.0f,
            phase, 0.3f + F(O_BANDS) * 2.2f, 0.6f + F(O_VIB) * 1.0f, v[O_BRIGHT] / 100.0f,
            F(O_GLOW) * 0.9f, F(O_FRINGE) * 0.07f, 1, 0,
        };
        for (int k = 0; k < 6; k++) {
            unsigned a = kPalettes[palette.a][k], b = kPalettes[palette.b][k];
            for (int ch = 0; ch < 3; ch++) {
                float ca = ((a >> (16 - 8 * ch)) & 255) / 255.0f, cb = ((b >> (16 - 8 * ch)) & 255) / 255.0f;
                c[36 + k * 4 + ch] = ca + (cb - ca) * palette.mix;
            }
        }
        r.DrawScreenShader(*s, c, 60);
        // Cross-fade: the incoming look is drawn over the top, blended in.
        float mix = pattern.mix > divs.mix ? pattern.mix : divs.mix;
        if (mix > 0.002f) {
            ScreenShader* s2 = Shader(r, level, pattern.b, true);
            if (s2) {
                c[4] = (float)divs.b;
                c[34] = mix;
                r.DrawScreenShader(*s2, c, 60, BLEND_ALPHA);
            }
        }
    }
};

Scene* CreateScene() { return new SaverScene; }
