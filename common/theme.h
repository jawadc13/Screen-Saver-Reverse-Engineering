// theme.h - the style layer shared by every screensaver and by Screensaver
// Studio: 10,240 named colour themes, 17 patterns, 12 effects, 5 motion
// styles and a speed multiplier. Header-only, so the Studio app can use it
// without the saver framework.
//
// Settings live per saver under
//   HKCU\Software\ScreenSaverRE\Styles\<saver file name without .scr>
// falling back to HKCU\Software\ScreenSaverRE\Styles\_All (set by
// "Apply to all" in the Studio), then to the defaults (no change).
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <string>

namespace style {

// ---------------------------------------------------------------------------
// Palettes: 64 hand-made gradients, 5 stops each (dark -> light).
// ---------------------------------------------------------------------------
struct Palette { const wchar_t* name; unsigned stops[5]; };

static const Palette kPalettes[] = {
    { L"Sunset",      { 0x1a0a2e, 0x6b1d5c, 0xd1495b, 0xf79d5c, 0xffe29a } },
    { L"Ocean",       { 0x021526, 0x03346e, 0x1f6fb2, 0x4fc3d9, 0xc9f4ff } },
    { L"Vaporwave",   { 0x1b0033, 0x6a0dad, 0xff6ec7, 0x7df9ff, 0xfff0ff } },
    { L"Forest",      { 0x0b1a0f, 0x1f4d2b, 0x3f8f45, 0x9bd16b, 0xeaf7c6 } },
    { L"Ember",       { 0x120000, 0x5c0a00, 0xc2410c, 0xf59e0b, 0xfff1b8 } },
    { L"Glacier",     { 0x06141f, 0x1e3a5f, 0x5b8db8, 0xa9d6e5, 0xf2fbff } },
    { L"Gold",        { 0x140d00, 0x5a3d00, 0xb8860b, 0xf3c74f, 0xfff6d5 } },
    { L"Rose",        { 0x1f0a14, 0x6d2041, 0xc44569, 0xf29bb5, 0xffe8f0 } },
    { L"Cyberpunk",   { 0x0a0014, 0x3d0066, 0xff007f, 0x00f0ff, 0xf5ff7a } },
    { L"Mint",        { 0x041a14, 0x0f5c4a, 0x2bb38f, 0x8ff0c8, 0xf0fff8 } },
    { L"Lavender",    { 0x140f24, 0x3e2f6b, 0x7e6bc4, 0xc3b6f2, 0xf6f2ff } },
    { L"Citrus",      { 0x141400, 0x5c5c00, 0xb8c400, 0xffd23f, 0xfff9d6 } },
    { L"Aurora",      { 0x020a14, 0x0b3d4a, 0x1fd18c, 0x8a5cf6, 0xe8fff6 } },
    { L"Desert",      { 0x1f120a, 0x6b3e1f, 0xc27c3e, 0xe8b77d, 0xfff1dc } },
    { L"Neon Night",  { 0x05000f, 0x2a0a5e, 0x00b3ff, 0xff2bd6, 0xffffff } },
    { L"Coral Reef",  { 0x04121f, 0x0b4f6c, 0x01baef, 0xff7f6b, 0xfff3e6 } },
    { L"Autumn",      { 0x1a0b05, 0x5e2a0e, 0xb5541c, 0xe3a33b, 0xfbe7b0 } },
    { L"Cherry",      { 0x14000a, 0x5c0022, 0xb0003a, 0xff4f79, 0xffe0e8 } },
    { L"Electric",    { 0x000814, 0x001d3d, 0x003566, 0x00b4d8, 0xffd60a } },
    { L"Moss",        { 0x0d0f05, 0x2f3a12, 0x5f7a24, 0xa7c957, 0xf2f7d9 } },
    { L"Peach",       { 0x1f0f0a, 0x6b3524, 0xe07a5f, 0xf2b8a0, 0xfff1eb } },
    { L"Arctic",      { 0x0a0f14, 0x2e3e4f, 0x6b8299, 0xbfd3e6, 0xffffff } },
    { L"Lagoon",      { 0x00141a, 0x005f73, 0x0a9396, 0x94d2bd, 0xe9fff7 } },
    { L"Magma",       { 0x000004, 0x3b0f70, 0x8c2981, 0xde4968, 0xfcfdbf } },
    { L"Viridis",     { 0x440154, 0x3b528b, 0x21918c, 0x5ec962, 0xfde725 } },
    { L"Inferno",     { 0x000004, 0x420a68, 0x932667, 0xdd513a, 0xfcffa4 } },
    { L"Plasma",      { 0x0d0887, 0x6a00a8, 0xb12a90, 0xe16462, 0xf0f921 } },
    { L"Twilight",    { 0x0f0a1e, 0x2d1e4f, 0x5c4b8a, 0xe2a3c7, 0xfde2e4 } },
    { L"Retro Teal",  { 0x061a1a, 0x0f4c4c, 0x2a9d8f, 0xe9c46a, 0xf4a261 } },
    { L"Candy",       { 0x1a0a1a, 0xff5ea2, 0xffb4d6, 0x8ae1fc, 0xfffbd6 } },
    { L"Bubblegum",   { 0x1a0614, 0x9b2c77, 0xff7eb6, 0x7ee8fa, 0xfdfcdc } },
    { L"Steel",       { 0x0a0c0f, 0x2b3038, 0x58606b, 0xa3acb8, 0xeef1f5 } },
    { L"Jade",        { 0x02140c, 0x0a4d34, 0x16a085, 0x76d7c4, 0xe8fff8 } },
    { L"Royal",       { 0x050314, 0x1b1464, 0x3c2fb8, 0xd4af37, 0xfff4cf } },
    { L"Volcano",     { 0x0a0000, 0x330000, 0x990000, 0xff4500, 0xffd700 } },
    { L"Spring",      { 0x0a140a, 0x3a7d44, 0x9ad24c, 0xffd6e8, 0xfffdf5 } },
    { L"Midnight",    { 0x000005, 0x0a0f3d, 0x1c2a80, 0x5f7dd6, 0xd6e0ff } },
    { L"Sepia",       { 0x140d06, 0x4a3520, 0x8c6a45, 0xc9a77c, 0xf5e6cf } },
    { L"Toxic",       { 0x050a00, 0x1f3d00, 0x5cb800, 0xb6ff00, 0xf5ffd6 } },
    { L"Flamingo",    { 0x1f0a10, 0x8a2b4a, 0xf45b8a, 0xfca5c4, 0xfff0f5 } },
    { L"Deep Sea",    { 0x00030a, 0x001a33, 0x00395c, 0x007a99, 0x7ae7ff } },
    { L"Sahara",      { 0x1a0f05, 0x8c4a1a, 0xd98c3e, 0xf2c57c, 0xfff5e0 } },
    { L"Berry",       { 0x0f0514, 0x4a0f5c, 0x8e2c8a, 0xd65db1, 0xffc6ff } },
    { L"Matcha",      { 0x0a0f05, 0x3a4d1f, 0x7a9a3e, 0xc3d991, 0xf7fbe8 } },
    { L"Nordic",      { 0x2e3440, 0x3b4252, 0x5e81ac, 0x88c0d0, 0xeceff4 } },
    { L"Dracula",     { 0x14141f, 0x44475a, 0xbd93f9, 0xff79c6, 0xf8f8f2 } },
    { L"Solarized",   { 0x002b36, 0x268bd2, 0x2aa198, 0xb58900, 0xfdf6e3 } },
    { L"Monokai",     { 0x272822, 0xf92672, 0xa6e22e, 0x66d9ef, 0xf8f8f2 } },
    { L"Synthwave",   { 0x120024, 0x2b0f54, 0xab1f65, 0xff4f69, 0xfff7f8 } },
    { L"Miami",       { 0x0f0524, 0x3a0ca3, 0xf72585, 0x4cc9f0, 0xffffff } },
    { L"Tropical",    { 0x00140f, 0x007f5f, 0x2b9348, 0xffb703, 0xfb8500 } },
    { L"Copper",      { 0x0f0703, 0x4a2310, 0x9c5221, 0xd98e4b, 0xffe2c4 } },
    { L"Ice Cream",   { 0x1f141a, 0xf8b4c4, 0xfde2b8, 0xb8e8d4, 0xfffaf2 } },
    { L"Galaxy",      { 0x02000a, 0x1a0b4d, 0x5b2a86, 0x9d4edd, 0xe0aaff } },
    { L"Firefly",     { 0x050a05, 0x0f1f0f, 0x3d5c1f, 0xd4e157, 0xfffde0 } },
    { L"Storm",       { 0x05070a, 0x1f2633, 0x3d4b61, 0x7d8ea8, 0xd6dee8 } },
    { L"Poppy",       { 0x140303, 0x6b0f0f, 0xd62828, 0xf77f00, 0xfcbf49 } },
    { L"Sea Glass",   { 0x0a1414, 0x2f5d5d, 0x6fa5a0, 0xb8dbd0, 0xf2fbf8 } },
    { L"Amethyst",    { 0x0a0514, 0x2d1659, 0x6a3fb5, 0xa98be0, 0xefe6ff } },
    { L"Sunflower",   { 0x140f00, 0x5c4100, 0xd99700, 0xffd000, 0xfff7c2 } },
    { L"Blueprint",   { 0x001024, 0x003a75, 0x0066cc, 0x6cb4ff, 0xe6f2ff } },
    { L"Terminal",    { 0x000500, 0x003300, 0x00800f, 0x33ff33, 0xccffcc } },
    { L"Sakura",      { 0x1a0a10, 0x7a3b52, 0xe08aa8, 0xf7c8d8, 0xfff5f8 } },
    { L"Opal",        { 0x0f141a, 0x5d7b9a, 0xa7c4d9, 0xf2d4e0, 0xfffaf5 } },
};
static const int kPaletteCount = (int)(sizeof(kPalettes) / sizeof(kPalettes[0]));   // 64

enum ThemeMode { MODE_GRADIENT, MODE_DUOTONE, MODE_TINT, MODE_HUE, MODE_PASTEL, MODE_NEON, MODE_MONO, MODE_SPLIT, MODE_COUNT };
static const wchar_t* kModeNames[MODE_COUNT] = { L"Gradient", L"Duotone", L"Tint", L"Hue Shift", L"Pastel", L"Neon", L"Mono", L"Split-tone" };
static const int kVariations = 20;
static const int kThemeCount = 64 * MODE_COUNT * kVariations;   // 10,240 (theme 0 = Original colours)

static const wchar_t* kPatternNames[] = {
    L"None", L"Mirror (left/right)", L"Mirror (top/bottom)", L"Quad mirror", L"Kaleidoscope 4", L"Kaleidoscope 6",
    L"Kaleidoscope 8", L"Kaleidoscope 12", L"Tile 2 x 2", L"Tile 3 x 3", L"Swirl", L"Ripple", L"Fisheye",
    L"Slow rotate", L"Zoom pulse", L"Pixelate (retro)", L"Polar tunnel" };
static const int kPatternCount = (int)(sizeof(kPatternNames) / sizeof(kPatternNames[0]));

static const wchar_t* kEffectNames[] = {
    L"None", L"Vignette", L"CRT scanlines", L"Film grain", L"Chromatic aberration", L"Posterize", L"Dreamy glow",
    L"Glitch", L"Night vision", L"Old film (sepia)", L"Bloom", L"Comic (posterize + outline)" };
static const int kEffectCount = (int)(sizeof(kEffectNames) / sizeof(kEffectNames[0]));

static const wchar_t* kMotionNames[] = { L"Normal", L"Breathing (speed pulses)", L"Tidal (long slow waves)", L"Bursts", L"Time warp (drifting speed)" };
static const int kMotionCount = (int)(sizeof(kMotionNames) / sizeof(kMotionNames[0]));

// What the GPU post pass needs.
struct ThemeParams {
    int   mode = -1;          // -1 = original colours
    float stops[5][3] = {};
    float hueShift = 0;       // radians
    float saturation = 1, contrast = 1, brightness = 1;
};

inline void Unpack(unsigned rgb, float* out) {
    out[0] = ((rgb >> 16) & 255) / 255.0f; out[1] = ((rgb >> 8) & 255) / 255.0f; out[2] = (rgb & 255) / 255.0f;
}

// Theme 0 = original; 1..kThemeCount = palette x mode x variation.
inline ThemeParams GetTheme(int id) {
    ThemeParams tp;
    if (id <= 0 || id > kThemeCount) return tp;
    int i = id - 1;
    int pal = i % kPaletteCount, mode = (i / kPaletteCount) % MODE_COUNT, var = i / (kPaletteCount * MODE_COUNT);
    tp.mode = mode;
    const Palette& p = kPalettes[pal];
    bool reverse = (var % 4) == 3 && mode != MODE_GRADIENT;   // some variations flip the gradient
    for (int k = 0; k < 5; k++) Unpack(p.stops[reverse ? 4 - k : k], tp.stops[k]);
    // Variations: a hue nudge, contrast and saturation steps.
    tp.hueShift = (var % 5) * 0.35f - 0.7f + (mode == MODE_HUE ? (1 + var) * 0.31f : 0);
    tp.contrast = 0.9f + (var % 3) * 0.12f;
    tp.saturation = 0.85f + ((var / 3) % 3) * 0.2f;
    tp.brightness = 0.95f + ((var / 9) % 2) * 0.1f;
    return tp;
}

inline void ThemeName(int id, wchar_t* out, size_t cch) {
    if (id <= 0 || id > kThemeCount) { _snwprintf(out, cch, L"Original colours"); out[cch - 1] = 0; return; }
    int i = id - 1;
    _snwprintf(out, cch, L"%s %s %d", kPalettes[i % kPaletteCount].name, kModeNames[(i / kPaletteCount) % MODE_COUNT], i / (kPaletteCount * MODE_COUNT) + 1);
    out[cch - 1] = 0;
}

// ---------------------------------------------------------------------------
// Per-saver style settings
// ---------------------------------------------------------------------------
struct Settings {
    DWORD theme = 0;      // 0 = original, 1..kThemeCount
    DWORD pattern = 0;
    DWORD effect = 0;
    DWORD motion = 0;
    DWORD speed = 50;     // 0..100 -> 0.1x .. 4x (50 = 1x)
    DWORD strength = 100; // theme strength %
    DWORD shuffle = 0;    // RandomWhat bits: options picked at random every time the saver starts
    // OLED screen care (savers with their own rest cycle use theirs instead)
    DWORD restEvery = 0;  // minutes between rests to black, 0 = off
    DWORD restLength = 10;// seconds of black
    DWORD orbit = 1;      // pixel orbit
    DWORD guard = 1;      // burn-in guard (dim static bright areas)
};

// 0 = 0.1x, 50 = 1x, 100 = 4x (log scale on each side).
inline float SpeedMultiplier(DWORD s) { return s <= 50 ? powf(10.0f, (s - 50.0f) / 50.0f) : powf(4.0f, (s - 50.0f) / 50.0f); }

inline void KeyFor(const wchar_t* saverFile, wchar_t* out, size_t cch) {
    _snwprintf(out, cch, L"Software\\ScreenSaverRE\\Styles\\%s", saverFile);
    out[cch - 1] = 0;
}

inline bool ReadKey(const wchar_t* path, Settings& s) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) != ERROR_SUCCESS) return false;
    auto rd = [&](const wchar_t* name, DWORD& v) { DWORD d, cb = sizeof(d), type; if (RegQueryValueExW(k, name, nullptr, &type, (BYTE*)&d, &cb) == ERROR_SUCCESS && type == REG_DWORD) v = d; };
    rd(L"Theme", s.theme); rd(L"Pattern", s.pattern); rd(L"Effect", s.effect);
    rd(L"Motion", s.motion); rd(L"Speed", s.speed); rd(L"Strength", s.strength);
    DWORD legacy = 0;   // older builds: one "Randomize" switch for everything
    rd(L"Randomize", legacy);
    if (legacy == 1) s.shuffle = 31;
    rd(L"Shuffle", s.shuffle);
    rd(L"RestEvery", s.restEvery); rd(L"RestLength", s.restLength); rd(L"PixelOrbit", s.orbit); rd(L"BurnInGuard", s.guard);
    RegCloseKey(k);
    return true;
}

inline void Clamp(Settings& s) {
    if (s.theme > (DWORD)kThemeCount) s.theme = 0;
    if (s.pattern >= (DWORD)kPatternCount) s.pattern = 0;
    if (s.effect >= (DWORD)kEffectCount) s.effect = 0;
    if (s.motion >= (DWORD)kMotionCount) s.motion = 0;
    if (s.speed > 100) s.speed = 50;
    if (s.strength > 100) s.strength = 100;
    s.shuffle &= 31;
    if (s.restEvery > 240) s.restEvery = 0;
    if (s.restLength < 3 || s.restLength > 600) s.restLength = 10;
    if (s.orbit > 1) s.orbit = 1;
    if (s.guard > 1) s.guard = 1;
}

// saverFile: e.g. L"OLED_Lorenz" (the .scr name without extension).
// draft: prefer unsaved changes being previewed (Styles\_Draft\<file>).
inline Settings Load(const wchar_t* saverFile, bool draft = false) {
    Settings s;
    wchar_t path[300];
    if (draft) {
        _snwprintf(path, 300, L"Software\\ScreenSaverRE\\Styles\\_Draft\\%s", saverFile); path[299] = 0;
        if (ReadKey(path, s)) { Clamp(s); return s; }
    }
    KeyFor(saverFile, path, 300);
    if (!ReadKey(path, s)) ReadKey(L"Software\\ScreenSaverRE\\Styles\\_All", s);
    Clamp(s);
    return s;
}

inline void Save(const wchar_t* saverFile, const Settings& s) {
    wchar_t path[300];
    KeyFor(saverFile, path, 300);
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    auto wr = [&](const wchar_t* name, DWORD v) { RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); };
    wr(L"Theme", s.theme); wr(L"Pattern", s.pattern); wr(L"Effect", s.effect);
    wr(L"Motion", s.motion); wr(L"Speed", s.speed); wr(L"Strength", s.strength);
    wr(L"Shuffle", s.shuffle);
    RegDeleteValueW(k, L"Randomize");
    wr(L"RestEvery", s.restEvery); wr(L"RestLength", s.restLength); wr(L"PixelOrbit", s.orbit); wr(L"BurnInGuard", s.guard);
    RegCloseKey(k);
}

// Choices offered for the rest cycle.
static const DWORD kRestEveryMinutes[] = { 0, 1, 2, 3, 5, 10, 15, 20, 30, 45, 60 };
static const DWORD kRestLengthSeconds[] = { 5, 10, 15, 20, 30, 45, 60, 120 };

// Time multiplier for a motion style at time t (seconds).
inline float MotionFactor(DWORD motion, float t) {
    switch (motion) {
    case 1: return 0.55f + 0.45f * sinf(t * 2 * 3.14159f / 6);                     // breathing, 6 s
    case 2: return 0.35f + 0.65f * (0.5f + 0.5f * sinf(t * 2 * 3.14159f / 40));     // tidal, 40 s
    case 3: { float ph = fmodf(t, 9.0f); return ph < 1.5f ? 1 + 2.0f * sinf(ph / 1.5f * 3.14159f) : 0.6f; }   // bursts
    case 4: return 0.4f + 1.2f * (0.5f + 0.5f * sinf(t * 0.21f) * sinf(t * 0.077f + 1));                   // time warp
    default: return 1;
    }
}

// Subkey names of Styles\<sub> (sub = nullptr: Styles itself).
inline std::vector<std::wstring> ListKeys(const wchar_t* sub) {
    std::vector<std::wstring> names;
    std::wstring path = L"Software\\ScreenSaverRE\\Styles";
    if (sub) path += std::wstring(L"\\") + sub;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return names;
    wchar_t name[256];
    for (DWORD i = 0;; i++) {
        DWORD cch = 256;
        if (RegEnumKeyExW(k, i, name, &cch, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        names.push_back(name);
    }
    RegCloseKey(k);
    return names;
}

inline void DeleteKey(const std::wstring& rel) {
    RegDeleteKeyW(HKEY_CURRENT_USER, (L"Software\\ScreenSaverRE\\Styles\\" + rel).c_str());
}

// Unsaved changes shown by a live preview.
inline void SaveDraft(const wchar_t* saverFile, const Settings& s) { Save((L"_Draft\\" + std::wstring(saverFile)).c_str(), s); }
inline void DeleteDraft(const wchar_t* saverFile) { DeleteKey(L"_Draft\\" + std::wstring(saverFile)); }

// Named presets: Styles\_Presets\<name>.
inline std::vector<std::wstring> ListPresets() { return ListKeys(L"_Presets"); }
inline void SavePreset(const std::wstring& name, const Settings& s) { Save((L"_Presets\\" + name).c_str(), s); }
inline void DeletePreset(const std::wstring& name) { DeleteKey(L"_Presets\\" + name); }
inline Settings LoadPreset(const std::wstring& name) {
    Settings s;
    ReadKey((L"Software\\ScreenSaverRE\\Styles\\_Presets\\" + name).c_str(), s);
    Clamp(s);
    return s;
}

// "Apply to all": the _All fallback plus every saver that has its own key.
inline void SaveToAll(const Settings& s) {
    Save(L"_All", s);
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\ScreenSaverRE\\Styles", 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    std::vector<std::wstring> names;
    wchar_t name[256];
    for (DWORD i = 0;; i++) {
        DWORD cch = 256;
        if (RegEnumKeyExW(k, i, name, &cch, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        if (name[0] != L'_') names.push_back(name);
    }
    RegCloseKey(k);
    for (const std::wstring& n : names) Save(n.c_str(), s);
}

// ---------------------------------------------------------------------------
// Randomizer: which options may be picked (shared by every saver), and
// favourite themes. Stored in HKCU\Software\ScreenSaverRE\Styles\_Random.
// ---------------------------------------------------------------------------
enum RandomWhat { RW_THEME = 1, RW_PATTERN = 2, RW_EFFECT = 4, RW_MOTION = 8, RW_SPEED = 16, RW_ALL = 31 };

struct RandomPool {
    DWORD pal0 = 0xFFFFFFFF, pal1 = 0xFFFFFFFF;   // palettes 0-31, 32-63
    DWORD modes = (1u << MODE_COUNT) - 1;
    DWORD patterns = (1u << kPatternCount) - 1;
    DWORD effects = (1u << kEffectCount) - 1;
    DWORD motions = (1u << kMotionCount) - 1;
    DWORD favOnly = 0;        // themes only from the favourites
    DWORD original = 1;       // original colours may come up too
    DWORD speedMin = 35, speedMax = 70;
    bool Palette(int i) const { return ((i < 32 ? pal0 : pal1) >> (i & 31)) & 1; }
    void SetPalette(int i, bool on) { DWORD& m = i < 32 ? pal0 : pal1; m = on ? m | (1u << (i & 31)) : m & ~(1u << (i & 31)); }
};

static const wchar_t kRandomKey[] = L"Software\\ScreenSaverRE\\Styles\\_Random";

inline RandomPool LoadPool() {
    RandomPool p;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRandomKey, 0, KEY_READ, &k) != ERROR_SUCCESS) return p;
    auto rd = [&](const wchar_t* name, DWORD& v) { DWORD d, cb = sizeof(d), type; if (RegQueryValueExW(k, name, nullptr, &type, (BYTE*)&d, &cb) == ERROR_SUCCESS && type == REG_DWORD) v = d; };
    rd(L"Palettes1", p.pal0); rd(L"Palettes2", p.pal1); rd(L"Modes", p.modes); rd(L"Patterns", p.patterns);
    rd(L"Effects", p.effects); rd(L"Motions", p.motions); rd(L"FavouritesOnly", p.favOnly);
    rd(L"Original", p.original); rd(L"SpeedMin", p.speedMin); rd(L"SpeedMax", p.speedMax);
    RegCloseKey(k);
    if (p.speedMin > 100) p.speedMin = 0;
    if (p.speedMax > 100) p.speedMax = 100;
    if (p.speedMax < p.speedMin) p.speedMax = p.speedMin;
    return p;
}

inline void SavePool(const RandomPool& p) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRandomKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    auto wr = [&](const wchar_t* name, DWORD v) { RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v)); };
    wr(L"Palettes1", p.pal0); wr(L"Palettes2", p.pal1); wr(L"Modes", p.modes); wr(L"Patterns", p.patterns);
    wr(L"Effects", p.effects); wr(L"Motions", p.motions); wr(L"FavouritesOnly", p.favOnly);
    wr(L"Original", p.original); wr(L"SpeedMin", p.speedMin); wr(L"SpeedMax", p.speedMax);
    RegCloseKey(k);
}

// Favourite theme numbers, kept as text ("12,345,9001") in _Random\Favourites.
inline std::vector<int> LoadFavourites() {
    std::vector<int> v;
    wchar_t buf[8192]; DWORD cb = sizeof(buf) - sizeof(wchar_t), type;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRandomKey, 0, KEY_READ, &k) != ERROR_SUCCESS) return v;
    if (RegQueryValueExW(k, L"Favourites", nullptr, &type, (BYTE*)buf, &cb) == ERROR_SUCCESS && type == REG_SZ) {
        buf[cb / sizeof(wchar_t)] = 0;
        int n = 0; bool any = false;
        for (const wchar_t* q = buf;; q++) {
            if (*q >= L'0' && *q <= L'9') { n = n * 10 + (*q - L'0'); any = true; if (n > kThemeCount) n = kThemeCount + 1; }
            else { if (any && n >= 1 && n <= kThemeCount) v.push_back(n); n = 0; any = false; if (!*q) break; }
        }
    }
    RegCloseKey(k);
    return v;
}

inline void SaveFavourites(const std::vector<int>& v) {
    std::wstring s;
    for (size_t i = 0; i < v.size() && i < 1000; i++) { if (i) s += L','; s += std::to_wstring(v[i]); }
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRandomKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, L"Favourites", 0, REG_SZ, (const BYTE*)s.c_str(), (DWORD)((s.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}

inline bool IsFavourite(int id) {
    for (int f : LoadFavourites()) if (f == id) return true;
    return false;
}

inline void SetFavourite(int id, bool on) {
    if (id < 1 || id > kThemeCount) return;
    std::vector<int> v = LoadFavourites(), out;
    for (int f : v) if (f != id) out.push_back(f);
    if (on) out.push_back(id);
    SaveFavourites(out);
}

struct Rng {
    unsigned s;
    explicit Rng(unsigned seed) : s(seed ? seed : 0x9E3779B9u) {}
    unsigned Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int Below(int n) { return n > 0 ? (int)(Next() % (unsigned)n) : 0; }
};

inline unsigned TimeSeed() {
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    return (unsigned)q.QuadPart ^ (unsigned)(q.QuadPart >> 32) ^ GetCurrentProcessId() * 2654435761u;
}

// A random set bit of `mask` (bits 0..count-1), or -1 if none are set.
inline int PickBit(DWORD mask, int count, Rng& r) {
    int n = 0;
    for (int i = 0; i < count; i++) n += (mask >> i) & 1;
    if (!n) return -1;
    int k = r.Below(n);
    for (int i = 0; i < count; i++) if (((mask >> i) & 1) && k-- == 0) return i;
    return -1;
}

// A theme the pool allows (0 = original colours).
inline int RandomTheme(const RandomPool& p, Rng& r) {
    if (p.favOnly) {
        std::vector<int> f = LoadFavourites();
        if (!f.empty()) return f[r.Below((int)f.size())];
    }
    if (p.original && r.Below(12) == 0) return 0;
    int pals[64], np = 0;
    for (int i = 0; i < kPaletteCount; i++) if (p.Palette(i)) pals[np++] = i;
    int mode = PickBit(p.modes, MODE_COUNT, r);
    if (!np || mode < 0) return p.original ? 0 : 1 + r.Below(kThemeCount);
    return 1 + pals[r.Below(np)] + kPaletteCount * mode + kPaletteCount * MODE_COUNT * r.Below(kVariations);
}

// `s` with the options in `what` (RandomWhat bits) replaced by picks the
// pool allows.
inline Settings Randomize(Settings s, const RandomPool& p, Rng& r, DWORD what) {
    if (what & RW_THEME) s.theme = (DWORD)RandomTheme(p, r);
    int v;
    if ((what & RW_PATTERN) && (v = PickBit(p.patterns, kPatternCount, r)) >= 0) s.pattern = v;
    if ((what & RW_EFFECT) && (v = PickBit(p.effects, kEffectCount, r)) >= 0) s.effect = v;
    if ((what & RW_MOTION) && (v = PickBit(p.motions, kMotionCount, r)) >= 0) s.motion = v;
    if (what & RW_SPEED) s.speed = p.speedMin + r.Below((int)(p.speedMax - p.speedMin) + 1);
    return s;
}

}  // namespace style
