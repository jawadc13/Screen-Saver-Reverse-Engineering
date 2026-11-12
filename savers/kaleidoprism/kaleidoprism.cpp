// kaleidoprism.cpp - "Kaleidoscope Prism": a kaleidoscope computed per pixel
// on the GPU (shader.h), with any number of divisions, six mirror layouts,
// nine patterns, twelve palettes and a settings dialog built from one table.
// Full-screen scene made OLED safe by rest::RestScene (burn-in guard,
// scheduled rests, rolling rest band, pixel orbit) - see common/restkit.h.
#include "../../common/restkit.h"
#include "resource.h"
#include "shader.h"
#include <commctrl.h>
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
enum { PATTERNS = 9, PALETTES = 12 };

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
    { SLIDER, 0, L"Divisions", L"Divisions", 2, 64, 12, nullptr, true },
    { COMBO,  0, L"Symmetry:", L"Symmetry", 0, 1, 0, L"Mirrored|Pinwheel (no mirror)", true },
    { COMBO,  0, L"Layout:", L"Layout", 0, 5, 0, L"Classic wedges|Triangle mirror box|Square mirror box|Hexagon rosettes|Infinite tunnel|Nested rings", true },
    { CHECK,  0, L"Morph the number of divisions over time", L"Morph", 0, 1, 0, nullptr, true },
    { SLIDER, 0, L"Morph between Divisions and", L"MorphTo", 2, 64, 24, nullptr, true },
    { SLIDER, 0, L"Spiral twist", L"Twist", -100, 100, 0, nullptr, true },
    { SLIDER, 0, L"Rings / tile density", L"Rings", 1, 12, 4, nullptr, true },
    { SLIDER, 0, L"Rotation (negative = anticlockwise)", L"Rotation", -100, 100, 20, nullptr, true },
    { SLIDER, 0, L"Centre drift", L"Drift", 0, 100, 30, nullptr, true },
    { SLIDER, 0, L"Zoom", L"Zoom", 0, 100, 50, nullptr, true },
    { SLIDER, 0, L"Zoom breathing", L"Breath", 0, 100, 20, nullptr, true },

    { COMBO,  1, L"Pattern:", L"Pattern", 0, 9, 0, L"Silk|Stained glass|Gem facets|Petals|Ripples|Marble|Woven arcs|Glass beads|Ribbons|Cycle through all", true },
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
    { L"Hypnotic spiral", 6, { { O_TWIST, 80 }, { O_PATTERN, 8 }, { O_PALETTE, 0 }, { O_DIV, 16 }, { O_ROT, 50 }, { O_CYCLE, 60 } } },
    { L"Mandala rings", 7, { { O_LAYOUT, 5 }, { O_PATTERN, 3 }, { O_PALETTE, 10 }, { O_DIV, 12 }, { O_RINGS, 5 }, { O_SEAM, 3 }, { O_SEAMW, 20 } } },
    { L"Deep ocean", 6, { { O_PATTERN, 4 }, { O_PALETTE, 4 }, { O_DIV, 10 }, { O_SPEED, 25 }, { O_SPARK, 10 }, { O_GLOW, 55 } } },
    { L"Bead curtain", 5, { { O_LAYOUT, 2 }, { O_PATTERN, 7 }, { O_PALETTE, 3 }, { O_RINGS, 4 }, { O_GLOW, 60 } } },
    { L"Snowflake", 8, { { O_DIV, 6 }, { O_PATTERN, 2 }, { O_PALETTE, 8 }, { O_SEAM, 1 }, { O_SEAMW, 10 }, { O_SPARK, 80 }, { O_ZOOM, 40 }, { O_DETAIL, 65 } } },
    { L"Everything changes", 5, { { O_PATTERN, 9 }, { O_PALETTE, 12 }, { O_MORPH, 1 }, { O_DIV, 4 }, { O_MORPHTO, 32 } } },
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
    v[O_DIV] = RandI(3, 24);
    v[O_MORPHTO] = RandI(3, 32);
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

class SaverScene : public RestScene {
    int v[O_COUNT];                 // this monitor's settings (Surprise changes them)
    ScreenShader* shader[3] = {};   // 1, 2 and 4 samples per pixel, compiled when first used
    bool tried[3] = {};
    int level = 2;                  // index into shader[]
    float slowTime = 0, frameAvg = 0, settle = 0;

    float pt = 0;                   // pattern time (scaled by Speed)
    float rot = 0, chamber = 0, flowX = 0, flowY = 0, flowDir = 0, phase = 0;
    float seed[4] = {};
    Cycler divs, pattern, palette;

    float F(int i) const { return (v[i] - kOpts[i].lo) / (float)(kOpts[i].hi - kOpts[i].lo); }

    void RestSettings(float& every, float& len, int& band) const override {
        every = 60.0f * v[O_EVERY];
        len = (float)v[O_LENGTH];
        band = v[O_BAND];
    }

    ~SaverScene() { for (auto* s : shader) delete s; }

    void Setup(Renderer& r) override {
        memcpy(v, g_opt, sizeof(v));
        int q = v[O_QUALITY];
        level = q == 0 || q == 3 ? 2 : q - 1;
    }

    ScreenShader* Shader(Renderer& r) {
        // Fall back to fewer samples if a variant will not compile.
        for (int l = level; l >= 0; l--) {
            if (!tried[l]) {
                tried[l] = true;
                std::string src = std::string("#define SAMPLES ") + (l == 2 ? "4" : l == 1 ? "2" : "1") + "\n" + kPrismShader;
                shader[l] = r.CreateScreenShader(src.c_str());
            }
            if (shader[l]) return shader[l];
        }
        return nullptr;
    }

    void Reseed() override {
        if (variation > 0 && v[O_SURPRISE]) Randomize(v);
        for (float& s : seed) s = RandF(0, 100);
        pt = RandF(0, 500);
        rot = RandF(0, 2 * kPi); chamber = RandF(0, 2 * kPi); flowDir = RandF(0, 2 * kPi);
        flowX = RandF(-20, 20); flowY = RandF(-20, 20);
        phase = RandF(0, 1);
        divs.Set(v[O_DIV]);
        pattern.Set(v[O_PATTERN] >= PATTERNS ? RandI(0, PATTERNS - 1) : v[O_PATTERN]);
        palette.Set(v[O_PALETTE] >= PALETTES ? RandI(0, PALETTES - 1) : v[O_PALETTE]);
        // Automatic quality gets a fresh chance after every rest.
        if (v[O_QUALITY] == 0) { level = 2; slowTime = 0; settle = 0; }
    }

    // Automatic quality: when frames take clearly longer than the monitor's
    // refresh, drop one supersampling level.
    void AutoQuality(Renderer& r, float dt) {
        if (v[O_QUALITY] != 0 || preview || level == 0) return;
        settle += dt;
        if (settle < 3) { frameAvg = (float)r.RefreshPeriod(); return; }   // ignore start-up and shader compiles
        frameAvg += (fminf(dt, 0.25f) - frameAvg) * 0.05f;
        slowTime = frameAvg > r.RefreshPeriod() * 1.3 ? slowTime + dt : 0;
        if (slowTime > 1.5f) { level--; slowTime = 0; settle = 0; }
    }

    void Draw(Renderer& r, float dt) override {
        AutoQuality(r, dt);
        ScreenShader* s = Shader(r);
        if (!s) return;

        float speed = 0.15f + F(O_SPEED) * 1.85f, sdt = dt * speed;
        pt += sdt;
        rot += sdt * (v[O_ROT] / 100.0f) * 0.5f;
        chamber += sdt * 0.05f;
        flowDir += sdt * 0.07f * sinf(pt * 0.05f + seed[0]);
        float flow = F(O_FLOW) * 0.45f;
        flowX += cosf(flowDir) * flow * sdt; flowY += sinf(flowDir) * flow * sdt;
        phase += sdt * F(O_CYCLE) * 0.25f;
        phase -= floorf(phase);

        if (v[O_MORPH]) {
            int lo = v[O_DIV] < v[O_MORPHTO] ? v[O_DIV] : v[O_MORPHTO], hi = v[O_DIV] + v[O_MORPHTO] - lo;
            divs.Step(sdt, 14, 5, [&](int cur) { int n = RandI(lo, hi); return n == cur && hi > lo ? (cur < hi ? cur + 1 : cur - 1) : n; });
        }
        if (v[O_PATTERN] >= PATTERNS) pattern.Step(sdt, 22, 6, [](int cur) { return (cur + RandI(1, PATTERNS - 1)) % PATTERNS; });
        if (v[O_PALETTE] >= PALETTES) palette.Step(sdt, 16, 8, [](int cur) { return (cur + RandI(1, PALETTES - 1)) % PALETTES; });

        int layout = v[O_LAYOUT];
        float minDim = (float)(width < height ? width : height);
        float drift = F(O_DRIFT) * 0.28f;
        float zoom = expf((F(O_ZOOM) - 0.5f) * 2.0f + sinf(pt * 0.23f + seed[1]) * F(O_BREATH) * 0.5f);
        float sw = 0.002f + F(O_SEAMW) * 0.02f;
        bool centred = layout == 0 || layout == 4 || layout == 5;

        float c[60] = {
            (float)width, (float)height, 2.0f / minDim, pt,
            (float)divs.a, (float)divs.b, divs.mix, (float)v[O_SYM],
            (float)layout, v[O_TWIST] / 100.0f * 1.5f, (float)v[O_RINGS], rot,
            sinf(pt * 0.11f + seed[2]) * drift, cosf(pt * 0.087f + seed[3]) * drift, 1 / zoom, 2.0f * expf((F(O_DETAIL) - 0.5f) * 2.4f),
            (float)pattern.a, (float)pattern.b, pattern.mix, F(O_WARP) * 0.6f,
            flowX, flowY, chamber, F(O_SPARK) * 1.6f,
            (float)v[O_SEAM], sw / zoom, centred && v[O_JEWEL] ? 1.0f : 0.0f, v[O_VIGNETTE] ? 0.5f : 0.0f,
            phase, 0.3f + F(O_BANDS) * 2.2f, 0.6f + F(O_VIB) * 1.0f, v[O_BRIGHT] / 100.0f,
            F(O_GLOW) * 0.9f, F(O_FRINGE) * 0.07f, 0, 0,
        };
        for (int k = 0; k < 6; k++) {
            unsigned a = kPalettes[palette.a][k], b = kPalettes[palette.b][k];
            for (int ch = 0; ch < 3; ch++) {
                float ca = ((a >> (16 - 8 * ch)) & 255) / 255.0f, cb = ((b >> (16 - 8 * ch)) & 255) / 255.0f;
                c[36 + k * 4 + ch] = ca + (cb - ca) * palette.mix;
            }
        }
        r.DrawScreenShader(*s, c, 60);
    }
};

Scene* CreateScene() { return new SaverScene; }
