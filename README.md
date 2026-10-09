# Screen Saver Reverse Engineering

This repo has two parts:

* An analysis of the Windows XP **3D Pipes** screensaver (`sspipes.scr`, v5.1.2600.5512).
* New screensavers that follow the **same Windows screensaver rules** it does.

The full findings are in **[docs/ANALYSIS.md](docs/ANALYSIS.md)**: identity, imports, resources, registry settings, command line, input and exit rules, multi-monitor, preview and password handling, and how the pipes animation works.

| 3D Pipes (Remake) | Ball joints + textured |
|---|---|
| ![](docs/screenshots/pipes.png) | ![](docs/screenshots/pipes_textured_ball.png) |
| **3D Starfield** | **3D Polyhedra** |
| ![](docs/screenshots/starfield.png) | ![](docs/screenshots/polyhedra.png) |

## The screensavers

| File | Description | Settings |
|---|---|---|
| `Pipes.scr` | Recreation of 3D Pipes: pipes grow through a 3D grid. | Single/Multiple; Elbow/Ball/Mixed/Cycle joints; Solid/Textured (BMP/JPG/PNG/GIF); Speed |
| `Starfield.scr` | Flying through stars with warp trails. | Colors (White/Tinted/Rainbow); Warp trails; Speed; Density |
| `Polyhedra.scr` | Shiny Platonic solids tumbling and bouncing. | Shape (or Mixed); Number; Motion trails; Speed |

Every saver has a **Display Settings...** button. It lets you pick, per monitor, whether to show the saver or nothing, and whether every monitor shows the same animation.

## Rules every saver follows (same as `sspipes.scr`)

* `/s` runs full screen with one top-most window per monitor and the cursor hidden.
* `/p HWND` (or `/l`) draws a live preview inside the Display Properties dialog. It shows "No preview available" on failure.
* No arguments, or `/c[:HWND]`, opens the settings dialog. `/a HWND` changes the password on Windows 9x.
* Any key, mouse button or wheel input ends the saver. So does real mouse movement: more than 5 moves, so jitter doesn't end it. It also ends on losing focus or system suspend.
* `SC_SCREENSAVE` is swallowed so a second saver doesn't start. Monitor power-off still works.
* On Windows 9x it checks the password (`PASSWORD.CPL`) and sets `SPI_SETSCREENSAVERRUNNING`.
* String #1 holds the name shown in Windows. Icon #101, a version resource, and a Common Controls 6 manifest are included.
* Settings are saved per user in the registry under `HKCU\Software\ScreenSaverRE\<Name>`. The value names are the originals' (`Speed`, `Joint Type`, `MultiPipes`, `Textured`, `Texture Name`, `Screen N\Leave Black`, `AllScreensSame`, ...).

The savers render with **OpenGL 1.1** instead of Direct3D 8, so they need no DirectX runtime. They run on Windows XP through Windows 11.

## Building

On Linux with MinGW-w64 (`apt install g++-mingw-w64-i686`), or with MSYS2 on Windows:

```sh
make            # -> build/pipes.scr, build/starfield.scr, build/polyhedra.scr (32-bit)
make ARCH=x64   # 64-bit build
```

## Installing

Prebuilt 32-bit binaries are in `bin/`.

Copy a `.scr` to `C:\Windows\System32` (64-bit build) or `C:\Windows\SysWOW64` (32-bit build on 64-bit Windows). Then choose it in **Screen saver settings**. You can also right-click the `.scr` file and choose **Install**, or **Test** to run it right away.

## Layout

```
common/           shared framework: WinMain, command line, windows, input rules,
                  OpenGL setup, registry, Display Settings dialog, common resources
savers/<name>/    each saver: scene + settings dialog (.cpp), resources (.rc, .ico)
tools/            pe_inspect.py (analysis), make_icons.py (icon generator)
docs/             ANALYSIS.md + screenshots
```

To add a new saver, create `savers/<name>/` and implement `RegistryName()`, `LoadSettings()`, `ShowConfigDialog()` and `CreateScene()` (see `common/saver.h`). Then add the name to `SAVERS` in the Makefile.
