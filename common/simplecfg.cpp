// simplecfg.cpp - generic settings dialog (see simplecfg.h).
#include "saver.h"
#include "simplecfg.h"
#include <commctrl.h>

// Control IDs inside IDD_SIMPLE_CONFIG (common.rc).
enum {
    IDC_SC_GROUP = 1360, IDC_SC_DISPLAY = 1361, IDC_SC_STYLE = 1362,
    IDC_SC_CLABEL = 1300, IDC_SC_COMBO = 1310,
    IDC_SC_SLABEL = 1320, IDC_SC_SLIDER = 1330, IDC_SC_LOW = 1340, IDC_SC_HIGH = 1350,
};

void SimpleLoad(SimpleConfig& c) {
    for (int i = 0; i < c.nChoices; i++) {
        int count = 1;
        for (const wchar_t* p = c.choices[i].names; *p; p++) count += *p == L'|';
        c.choice[i] = RegReadDword(c.choices[i].regName, c.choices[i].def);
        if (c.choice[i] >= (DWORD)count) c.choice[i] = c.choices[i].def;
    }
    for (int i = 0; i < c.nSliders; i++) {
        c.slider[i] = RegReadDword(c.sliders[i].regName, c.sliders[i].def);
        if (c.slider[i] > 100) c.slider[i] = c.sliders[i].def;
    }
}

// Moves a control to a rectangle given in dialog units.
static void Place(HWND dlg, int id, int x, int y, int w, int h, bool show = true) {
    HWND c = GetDlgItem(dlg, id);
    if (!c) return;
    RECT r = { x, y, x + w, y + h };
    MapDialogRect(dlg, &r);
    SetWindowPos(c, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(c, show ? SW_SHOW : SW_HIDE);
}

static void Layout(HWND dlg, SimpleConfig& c) {
    SetWindowTextW(dlg, c.title);
    int y = 20;
    for (int i = 0; i < 3; i++) {
        bool used = i < c.nChoices;
        if (used) {
            SetDlgItemTextW(dlg, IDC_SC_CLABEL + i, c.choices[i].label);
            HWND combo = GetDlgItem(dlg, IDC_SC_COMBO + i);
            const wchar_t* p = c.choices[i].names;
            while (*p) {
                wchar_t item[64]; int n = 0;
                while (*p && *p != L'|' && n < 63) item[n++] = *p++;
                item[n] = 0;
                if (*p == L'|') p++;
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)item);
            }
            SendMessageW(combo, CB_SETCURSEL, c.choice[i], 0);
        }
        Place(dlg, IDC_SC_CLABEL + i, 14, y + 2, 72, 8, used);
        Place(dlg, IDC_SC_COMBO + i, 90, y, 177, 100, used);
        if (used) y += 18;
    }
    if (c.nChoices) y += 4;
    for (int i = 0; i < 4; i++) {
        bool used = i < c.nSliders;
        if (used) {
            SetDlgItemTextW(dlg, IDC_SC_SLABEL + i, c.sliders[i].label);
            SetDlgItemTextW(dlg, IDC_SC_LOW + i, c.sliders[i].low);
            SetDlgItemTextW(dlg, IDC_SC_HIGH + i, c.sliders[i].high);
            HWND tb = GetDlgItem(dlg, IDC_SC_SLIDER + i);
            SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessageW(tb, TBM_SETTICFREQ, 10, 0);
            SendMessageW(tb, TBM_SETPOS, TRUE, c.slider[i]);
        }
        Place(dlg, IDC_SC_SLABEL + i, 14, y, 190, 8, used);
        Place(dlg, IDC_SC_LOW + i, 14, y + 14, 30, 8, used);
        Place(dlg, IDC_SC_SLIDER + i, 46, y + 11, 194, 16, used);
        Place(dlg, IDC_SC_HIGH + i, 244, y + 14, 28, 8, used);
        if (used) y += 30;
    }
    Place(dlg, IDC_SC_GROUP, 7, 7, 267, y - 4);
    int by = y + 9;
    Place(dlg, IDC_SC_DISPLAY, 7, by, 72, 14);
    Place(dlg, IDC_SC_STYLE, 83, by, 80, 14);
    Place(dlg, IDOK, 170, by, 50, 14);
    Place(dlg, IDCANCEL, 224, by, 50, 14);
    // Resize the dialog to fit, keeping it centred.
    RECT rc = { 0, 0, 281, by + 21 };
    MapDialogRect(dlg, &rc);
    AdjustWindowRectEx(&rc, GetWindowLongW(dlg, GWL_STYLE), FALSE, GetWindowLongW(dlg, GWL_EXSTYLE));
    RECT cur; GetWindowRect(dlg, &cur);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int cx = (cur.left + cur.right) / 2, cy = (cur.top + cur.bottom) / 2;
    SetWindowPos(dlg, nullptr, cx - w / 2, cy - h / 2, w, h, SWP_NOZORDER);
}

static INT_PTR CALLBACK SimpleDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* c = (SimpleConfig*)GetWindowLongPtrW(dlg, DWLP_USER);
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(dlg, DWLP_USER, lp);
        Layout(dlg, *(SimpleConfig*)lp);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_SC_DISPLAY: ShowDisplaySettings(dlg); return TRUE;
        case IDC_SC_STYLE: ShowStyleSettings(dlg); return TRUE;
        case IDOK:
            for (int i = 0; i < c->nChoices; i++) {
                c->choice[i] = (DWORD)SendDlgItemMessageW(dlg, IDC_SC_COMBO + i, CB_GETCURSEL, 0, 0);
                RegWriteDword(c->choices[i].regName, c->choice[i]);
            }
            for (int i = 0; i < c->nSliders; i++) {
                c->slider[i] = (DWORD)SendDlgItemMessageW(dlg, IDC_SC_SLIDER + i, TBM_GETPOS, 0, 0);
                RegWriteDword(c->sliders[i].regName, c->slider[i]);
            }
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

void SimpleShowDialog(HWND parent, SimpleConfig& cfg) {
    DialogBoxParamW(g_hInst, MAKEINTRESOURCEW(IDD_SIMPLE_CONFIG), parent, SimpleDlgProc, (LPARAM)&cfg);
}
