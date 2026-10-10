// matrix.cpp - "Matrix Rain": columns of glyphs streaming down the screen.
// Glyphs are drawn once with GDI into a texture atlas, then rendered as
// textured quads, so any resolution (including portrait 4K) stays crisp.
#include "../../common/scenekit.h"
#include "resource.h"
#include <string>

static SimpleConfig g_cfg = {
    L"Matrix Rain Settings",
    2, {
        { L"&Colors:", L"Colors", 0, L"Green|Cyan|Amber|Red|Rainbow" },
        { L"&Glyphs:", L"Glyphs", 0, L"Katakana|Binary|Hex|Latin" },
    },
    3, {
        { L"Speed", L"Speed", 50, L"Slow", L"Fast" },
        { L"Density", L"Density", 60, L"Sparse", L"Dense" },
        { L"Glyph size", L"Size", 40, L"Small", L"Large" },
    },
};

const wchar_t* RegistryName() { return L"Matrix"; }
void LoadSettings() { SimpleLoad(g_cfg); }
void ShowConfigDialog(HWND parent) { SimpleShowDialog(parent, g_cfg); }

static const int kAtlasCols = 16, kAtlasRows = 8, kCell = 64;

// True if `font` really has a glyph for `ch` (no fallback box).
static bool FontHasGlyph(HDC dc, HFONT font, wchar_t ch) {
    HGDIOBJ old = SelectObject(dc, font);
    WORD idx = 0xFFFF;
    bool ok = GetGlyphIndicesW(dc, &ch, 1, &idx, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR && idx != 0xFFFF;
    SelectObject(dc, old);
    return ok;
}

// Renders the glyph set with GDI into BGRA pixels (white on black).
static int BuildAtlas(std::vector<unsigned>& px) {
    std::wstring glyphs;
    int set = (int)g_cfg.choice[1];
    // Katakana needs a Japanese font; use the first one installed, or fall
    // back to Latin glyphs if the PC has none.
    HDC probe = CreateCompatibleDC(nullptr);
    const wchar_t* faces[] = { L"MS Gothic", L"Yu Gothic", L"Meiryo", L"MS Mincho", L"Arial Unicode MS" };
    const wchar_t* face = L"Consolas";
    if (set == 0) {
        bool found = false;
        for (const wchar_t* f : faces) {
            HFONT test = CreateFontW(-32, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH, f);
            bool has = FontHasGlyph(probe, test, 0xFF71);
            DeleteObject(test);
            if (has) { face = f; found = true; break; }
        }
        if (!found) set = 3;
    }
    DeleteDC(probe);
    switch (set) {
    case 1: glyphs = L"01"; break;
    case 2: glyphs = L"0123456789ABCDEF"; break;
    case 3: glyphs = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$+-*/=%\"'#&_(),.;:?!|{}<>[]^~"; break;
    default:
        for (wchar_t c = 0xFF66; c <= 0xFF9D; c++) glyphs += c;   // half-width katakana
        glyphs += L"0123456789Z:.=*+-<>|";
        break;
    }
    int count = (int)glyphs.size();
    if (count > kAtlasCols * kAtlasRows) count = kAtlasCols * kAtlasRows;

    int w = kAtlasCols * kCell, h = kAtlasRows * kCell;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    HFONT font = CreateFontW(-kCell * 4 / 5, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH, face);
    HGDIOBJ oldFont = SelectObject(dc, font);
    RECT all = { 0, 0, w, h };
    FillRect(dc, &all, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    for (int i = 0; i < count; i++) {
        RECT rc = { (i % kAtlasCols) * kCell, (i / kAtlasCols) * kCell, 0, 0 };
        rc.right = rc.left + kCell; rc.bottom = rc.top + kCell;
        DrawTextW(dc, &glyphs[i], 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    GdiFlush();
    px.assign((unsigned*)bits, (unsigned*)bits + (size_t)w * h);
    for (auto& p : px) {   // brightness -> grey, opaque
        unsigned v = p & 0xFF;
        p = 0xFF000000u | (v << 16) | (v << 8) | v;
    }
    SelectObject(dc, oldFont); DeleteObject(font);
    SelectObject(dc, oldBmp); DeleteObject(bmp);
    DeleteDC(dc);
    return count;
}

struct Column {
    float head;     // row of the leading glyph (can be above the screen)
    float speed;    // rows per second
    int   length;   // trail length in rows
    float hue;
};

class MatrixScene : public Scene {
    Texture* atlas = nullptr;
    int glyphCount = 1;
    std::vector<Column> cols;
    std::vector<unsigned char> grid;   // glyph per cell
    int nCols = 1, nRows = 1;
    float cell = 20, flicker = 0;
    int width = 1, height = 1;
    Canvas2D canvas;

    void ResetColumn(Column& c, bool anywhere) {
        float rowsPerSec = 8 + g_cfg.slider[0] / 100.0f * 30;
        c.speed = rowsPerSec * RandF(0.6f, 1.4f);
        c.length = RandI(nRows / 4 + 4, nRows);
        c.head = anywhere ? RandF(-(float)nRows, (float)nRows) : -RandF(0, nRows * (1.2f - g_cfg.slider[1] / 100.0f));
        c.hue = RandF(0, 1);
    }

    void Layout() {
        float minDim = (float)(width < height ? width : height);
        cell = minDim / (60 - g_cfg.slider[2] * 0.4f);   // 60 .. 20 rows on the short side
        if (cell < 8) cell = 8;
        nCols = (int)(width / cell) + 1;
        nRows = (int)(height / cell) + 1;
        cols.resize(nCols);
        for (auto& c : cols) ResetColumn(c, true);
        grid.resize((size_t)nCols * nRows);
        for (auto& g : grid) g = (unsigned char)RandI(0, glyphCount - 1);
    }

    Color Tint(const Column& c, float bright) const {
        switch (g_cfg.choice[0]) {
        case 1:  return Color(0.2f, 0.9f, 1.0f) * bright;
        case 2:  return Color(1.0f, 0.7f, 0.15f) * bright;
        case 3:  return Color(1.0f, 0.15f, 0.1f) * bright;
        case 4:  return Hsv(c.hue, 0.8f, 1) * bright;
        default: return Color(0.15f, 1.0f, 0.3f) * bright;
        }
    }

public:
    ~MatrixScene() { delete atlas; }

    bool Init(Renderer& r, int w, int h, bool preview) override {
        std::vector<unsigned> px;
        glyphCount = BuildAtlas(px);
        atlas = r.CreateTexture(px.data(), kAtlasCols * kCell, kAtlasRows * kCell);
        if (!atlas) return false;
        width = w; height = h > 0 ? h : 1;
        Layout();
        return true;
    }

    void Resize(int w, int h) override {
        if (w == width && h == height) return;
        width = w; height = h > 0 ? h : 1;
        Layout();
    }

    void Frame(Renderer& r, float dt) override {
        // Random glyphs change now and then, like the film.
        flicker += dt * nCols * 3;
        while (flicker >= 1) { flicker -= 1; grid[RandI(0, (int)grid.size() - 1)] = (unsigned char)RandI(0, glyphCount - 1); }

        r.BeginFrame(true);
        canvas.Begin(width, height);
        const float du = 1.0f / kAtlasCols, dv = 1.0f / kAtlasRows;
        const float left = -width * 0.5f, top = height * 0.5f;
        for (int x = 0; x < nCols; x++) {
            Column& c = cols[x];
            c.head += c.speed * dt;
            if (c.head - c.length > nRows) ResetColumn(c, false);
            int head = (int)floorf(c.head);
            for (int k = 0; k < c.length; k++) {
                int y = head - k;
                if (y < 0 || y >= nRows) continue;
                float t = 1.0f - (float)k / c.length;
                Color col = k == 0 ? Color(0.9f, 1, 0.95f) : Tint(c, powf(t, 1.5f) * 1.1f + 0.08f);
                int g = grid[(size_t)y * nCols + x];
                float u0 = (g % kAtlasCols) * du, v0 = (g / kAtlasCols) * dv;
                float x0 = left + x * cell, y1 = top - y * cell;
                canvas.Rect(x0, y1 - cell, x0 + cell, y1, col, u0, v0, u0 + du, v0 + dv);
            }
        }
        canvas.Draw(r, BLEND_ADD, atlas);
    }
};

Scene* CreateScene() { return new MatrixScene; }
