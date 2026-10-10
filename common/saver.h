// saver.h - shared screensaver framework.
//
// Re-creates the behaviour of the DirectX 8 "D3DSaver" framework that the
// Windows XP 3D screensavers (sspipes.scr, ssflwbox.scr, sstext3d.scr, ...)
// are built on, using Direct3D 11 instead of Direct3D 8. See docs/ANALYSIS.md.
//
// Each screensaver implements the functions declared at the bottom of this
// file and links against saver.cpp, which owns WinMain.
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <vector>

#include "render.h"
#include "common_res.h"

// ---------------------------------------------------------------------------
// Registry helpers. All settings live under
//   HKEY_CURRENT_USER\Software\ScreenSaverRE\<RegistryName()>
// (the originals use HKCU\Software\Microsoft\ScreenSavers\<name>; we use our
// own root so we never clobber the real Microsoft savers' settings).
// ---------------------------------------------------------------------------
DWORD RegReadDword(const wchar_t* value, DWORD def, const wchar_t* subkey = nullptr);
void  RegWriteDword(const wchar_t* value, DWORD data, const wchar_t* subkey = nullptr);
void  RegReadString(const wchar_t* value, wchar_t* buf, DWORD cch, const wchar_t* def);
void  RegWriteString(const wchar_t* value, const wchar_t* data);

// ---------------------------------------------------------------------------
// Utilities available to scenes.
// ---------------------------------------------------------------------------
float RandF(float lo, float hi);          // uniform float in [lo, hi)
int   RandI(int lo, int hi);              // uniform int in [lo, hi]
// Loads a .bmp/.jpg/.png/.gif/.tif (via GDI+) as 32-bit BGRA pixels.
bool  LoadImageFile(const wchar_t* path, std::vector<unsigned>& bgra, int& w, int& h);
// Shows the shared "Display Settings" dialog (per-monitor options).
void  ShowDisplaySettings(HWND parent);
void  ShowStyleSettings(HWND parent);     // Themes & Effects (theme.h)
const wchar_t* SaverFileName();
float ScreenCareFadeSeconds();             // fade to/from black around a rest (style setting)           // this .scr's name without extension
// Shows a string-table message box with the saver's name as caption.
void  ErrorBox(HWND parent, UINT stringId);

extern HINSTANCE g_hInst;

// ---------------------------------------------------------------------------
// What each screensaver implements.
// ---------------------------------------------------------------------------
class Scene {
public:
    virtual ~Scene() {}
    // Called once on the monitor's render thread. `preview` is true when
    // drawing into the little monitor in the Display Properties dialog.
    virtual bool Init(Renderer& r, int width, int height, bool preview) = 0;
    virtual void Resize(int width, int height) = 0;
    // Advance by `dt` seconds and draw one frame (the framework presents).
    virtual void Frame(Renderer& r, float dt) = 0;
    // True for scenes that run their own rest cycle and burn-in guard
    // (restkit.h); the framework's screen care then stays out of the way.
    virtual bool HasOwnScreenCare() const { return false; }
};

const wchar_t* RegistryName();             // e.g. L"Pipes"
void   LoadSettings();                     // read registry into globals
void   ShowConfigDialog(HWND parent);      // modal settings dialog (/c)
Scene* CreateScene();                      // one per monitor window
