// simplecfg.h - a ready-made settings dialog for screensavers that only need
// a couple of drop-down lists and sliders (plus "Display Settings...").
// The dialog lays itself out for however many rows are used.
#pragma once
#include <windows.h>

struct CfgChoice {
    const wchar_t* label;     // e.g. L"&Colors:"
    const wchar_t* regName;   // registry value name
    DWORD          def;
    const wchar_t* names;     // items separated by '|', e.g. L"Green|Blue|Red"
};

struct CfgSlider {
    const wchar_t* label;     // e.g. L"Speed"
    const wchar_t* regName;
    DWORD          def;       // 0..100
    const wchar_t* low;       // e.g. L"Slow"
    const wchar_t* high;      // e.g. L"Fast"
};

struct SimpleConfig {
    const wchar_t* title;     // dialog caption
    int            nChoices;
    CfgChoice      choices[3];
    int            nSliders;
    CfgSlider      sliders[4];
    DWORD          choice[3] = {}; // current values (filled by SimpleLoad)
    DWORD          slider[4] = {}; // 0..100
};

void SimpleLoad(SimpleConfig& cfg);
void SimpleShowDialog(HWND parent, SimpleConfig& cfg);
