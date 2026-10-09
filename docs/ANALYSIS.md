# 3D Pipes (`sspipes.scr`): reverse-engineering notes

These notes cover the uploaded `3D_Pipes.scr`: what it is, how Windows talks
to it, and the rules it follows as a Windows screensaver. The recreations in
`savers/` follow the same rules. Their shared runtime is in `common/saver.cpp`.

Run `python3 tools/pe_inspect.py <file.scr>` to reproduce these findings.

## 1. Identity

| Field | Value |
|---|---|
| Format | PE32, Intel 386, GUI subsystem (2), 4 sections (`.text .data .data1 .rsrc`) |
| Version resource | *Direct3D Pipes Screen Saver*, 5.1.2600.5512 (`xpsp.080413-2105`), so Windows XP SP3 |
| Internal / PDB name | `Pipes` / `sspipes.pdb`, shipped as `sspipes.scr` |
| Manifest | `Microsoft.Windows.Pipes`, depends on Common Controls 6 |
| Graphics API | `d3d8.dll!Direct3DCreate8`, so **Direct3D 8** |
| Framework | Microsoft's DirectX 8 SDK **D3DSaver** framework. It has the window class `D3DSaverWndClass`, the `(hw vp)`, `(D24S8)`, `%.02f fps (%dx%dx%d)` device-description strings, and the per-monitor "Display Settings" dialog. |

The other XP 3D savers (`ssflwbox`, `sstext3d`, `ssmaze`) are built on the same framework.

## 2. Imports and what they tell us

* **ADVAPI32** `RegCreateKeyExW/RegSetValueExW/RegQueryValueExW/RegOpenKey*`: settings live in the registry.
* **KERNEL32** `GetPrivateProfileIntW/StringW/SectionW` with `control.ini` and `Screen Saver.3DPipes`: on first run it migrates settings from the old Windows 3.x / 9x `control.ini` section.
* **USER32** `SystemParametersInfoA/W`: `SPI_SETSCREENSAVERRUNNING`, plus the mouse and power checks.
* **USER32** `DialogBoxParamW`, `CheckRadioButton`, `SendDlgItemMessageW`, `COMCTL32 #17` (`InitCommonControls`): the settings dialog with a trackbar.
* **comdlg32** `GetOpenFileNameW`: "Choose Texture...".
* **WINMM** `timeGetTime` and **KERNEL32** `QueryPerformanceCounter`: frame timing.
* Strings `PASSWORD.CPL`/`VerifyScreenSavePwd`, `MPR.DLL`/`PwdChangePasswordA`, and `ScreenSaveUsePassword` under `Control Panel\Desktop`: Windows 9x password support.
* Embedded zlib/libpng error strings (`Invalid color type/bit depth combination in IHDR`): D3DX is statically linked to load `*.bmp;*.jpg;*.tga;*.png` textures.

## 3. Resources

| Type | ID | Content |
|---|---|---|
| STRING | 1 | `3D Pipes`. **String #1 is the name Windows shows in the screensaver list.** |
| STRING | 2100-2112 | D3DSaver error messages ("Could not create the Direct3D device." ...) |
| STRING | 2200-2212 | Hardware/software rendering status texts for the Display Settings dialog |
| STRING | 3015-3017 | `Texture Files (*.bmp;*.jpg;*.tga;*.png)#...#` (`#` is converted to NUL), `Choose Texture File` |
| STRING | 4001-4004 | Joint types: `Elbow`, `Ball`, `Mixed`, `Cycle` |
| STRING | 9000-9006 | Legacy `control.ini` keys: `JointType`, `SurfStyle`, `TextureQuality`, `MultiPipes`, `Screen Saver.3DPipes`, `control.ini`, `Texture` |
| DIALOG | 106 | "3D Pipes Settings": Pipes (Single/Multiple), Pipe Style (Joint Type combo), Surface Style (Solid/Textured + Choose Texture...), Speed trackbar (Slow..Fast), Display Settings..., OK/Cancel |
| DIALOG | 200 / 201 | D3DSaver "Display Settings" (single-monitor / multi-monitor tab variants) |
| ICON group | 101 | Icon shown in Display Properties |
| VERSION, MANIFEST | 1 | see above |

## 4. Settings: `HKCU\Software\Microsoft\ScreenSavers\Pipes`

| Value | Meaning |
|---|---|
| `Speed` | trackbar position |
| `Joint Type` | 0 Elbow, 1 Ball, 2 Mixed, 3 Cycle |
| `Textured` / `Texture Name` / `Default Texture` | surface style and chosen file |
| `MultiPipes` | single vs. multiple pipes |
| `Tessel Factor` | mesh detail |
| `AllScreensSame`, `Screen N\Adapter ID`, `Disable Hardware`, `Leave Black`, `Width`, `Height`, `Format` | D3DSaver per-monitor display settings |

The recreations use the same value names. They store them under
`HKCU\Software\ScreenSaverRE\<Name>` so they never overwrite a real Microsoft saver's settings.

## 5. The screensaver rules (D3DSaver behaviour)

These rules come from the imports and strings above, and from the published D3DSaver sources this binary is compiled from. Every rule below is implemented in `common/saver.cpp`.

### File and registration
1. The file is a normal GUI `.exe` renamed to **`.scr`**. Windows lists it when it is in `System32`/`SysWOW64` (or is installed via right-click → **Install**).
2. **String table entry 1** is the display name. Icon resource 101 is the icon.

### Command line
| Arguments | Mode |
|---|---|
| *(none)* | Show the settings dialog (no parent) |
| `/c` or `/c:HWND` | Settings dialog, owned by `HWND`. With no HWND, owned by the foreground window |
| `/s` | Run full screen |
| `/p HWND` (also `/l HWND`) | Preview: create a **child window** inside `HWND` (the little monitor in Display Properties) |
| `/a HWND` | Windows 9x "Change password": `MPR.DLL!PwdChangePasswordA("SCRSAVE", HWND, 0, 0)` |

Switches may begin with `/` or `-` and are case-insensitive. The HWND may follow a `:` or a space.

### Full-screen mode
3. Create **one top-most popup window per monitor**. The class is `D3DSaverWndClass`. The primary monitor is "Screen 1".
4. A monitor marked **"Display nothing"** (`Leave Black`) gets a black window and no renderer.
5. **Hide the cursor** (`WM_SETCURSOR`, then `SetCursor(NULL)`).
6. **Exit on any input**: a key press, any mouse button, the mouse wheel, or **more than 5 mouse moves that actually change position**. Windows send fake `WM_MOUSEMOVE`s when they appear, and mice jitter, so a single move is not enough.
7. **Exit when the app is deactivated** (`WM_ACTIVATEAPP` with FALSE) and when the system suspends (`PBT_APMSUSPEND`).
8. **Swallow `SC_SCREENSAVE`** (so the system doesn't start another saver), plus `SC_CLOSE`, `SC_KEYMENU`, `SC_NEXTWINDOW` and `SC_PREVWINDOW`. **Let `SC_MONITORPOWER` through** so the monitor can still power off.
9. Closing any one monitor's window ends the whole saver.
10. **Windows 9x only:** set `SPI_SETSCREENSAVERRUNNING` while running. Before exiting, if `ScreenSaveUsePassword` is set, call `PASSWORD.CPL!VerifyScreenSavePwd`, and keep running if it fails. On NT-based Windows the OS handles the password, so the saver just exits.

### Preview mode
11. Draw into a `WS_CHILD` window that fills the parent's client area. Ignore input (the user is working in the dialog).
12. Exit when the parent destroys the child window (`WM_DESTROY`, then `PostQuitMessage`).
13. If the device can't be created, paint **"No preview available"** (string 2112) instead of showing an error box.
14. Lower the priority so the Control Panel stays responsive.

### Rendering
15. Sync to the vertical refresh, and time frames with `QueryPerformanceCounter`.
    The remake goes further for mixed setups (for example a 175 Hz and a 60 Hz monitor on different GPUs). **Each monitor gets its own render thread and OpenGL context.** Each thread uses its own clock and that monitor's vsync, so a slow monitor never holds back a fast one. If vsync is off, the thread paces itself to the monitor's refresh rate (`EnumDisplaySettings`). All animation is driven by elapsed time, so speed is the same at any refresh rate or resolution.
16. Show errors from the 2100-range string table, with string 1 as the caption.

## 6. The Pipes animation

* The pipes live in an invisible **3D grid of cells**. Every pipe starts in a random free cell with a ball cap and a random color. Textured pipes all use the texture.
* Each step, a pipe moves one cell along an axis into a **free** cell. It usually keeps going straight and otherwise turns at random. It never goes back or crosses another pipe.
* A pipe that can't move gets a ball end cap. A new pipe then starts.
* **Joints:** *Elbow* is a quarter-torus bend. *Ball* is a sphere at the corner. *Mixed* picks one or the other per corner. *Cycle* switches style each time the screen resets.
* **Single** means one pipe grows at a time. **Multiple** means several grow at once.
* When the grid is about 40% full (or no free start cell is found), the scene fades out. It then restarts with a new camera angle.
* **Speed** sets the step rate. **Tessel Factor** sets the number of sides on each tube.

### Elbow geometry used in the remake
A pipe arrives at cell centre **P** moving along **din** and leaves along **dout**. The straight runs stop **R** short of P. A quarter torus around
`O = P − din·R + dout·R` joins them. Its points are
`O − dout·R·cosθ + din·R·sinθ` for θ ∈ [0, π/2]. The tangent goes from din to dout, so the bend blends smoothly into both straight runs.

## 7. Differences in the remake

* **OpenGL 1.1** replaces Direct3D 8. It works on every Windows version from XP up and needs no DirectX runtime.
* Textures load through **GDI+** (BMP/JPG/PNG/GIF). TGA is not supported. D3DX is not used.
* The Display Settings dialog has no "Disable hardware 3D rendering" or display-mode options. It keeps the per-monitor "Display nothing" choice and "same on all monitors" (`AllScreensSame`).
* There is no `control.ini` migration.
