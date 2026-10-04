# Crosshair Overlay

A tiny Windows 11 x64 tray app that draws one crosshair on top of your games.
Built for Windows 11 only. If it works on your machine, great; if not, tough luck.

## Use

Run **dist\CrosshairOverlay.exe**. The first manual launch turns on launch at sign-in and opens Settings;
sign-in launches stay quietly in the tray. The crosshair is visible on every fresh launch.

- **Shift + Alt + Z** shows or hides the crosshair.
- Click the tray icon for Settings; right-click for Show/Hide, Settings, Launch at sign-in, and Exit.
- Closing Settings keeps the app running. Use **Exit** in the tray to stop it.
- Edits apply instantly and are kept after restarts; there is nothing to save.

## Settings

Every value has a slider, a number box, and up/down arrows. Quick colors set the color in one click.
Sizes are actual screen pixels. Length is per arm; gap is the full empty center opening.
The black outline never fills the gap, except around a center dot. Set outline or dot to 0 to turn it off.
**Zoom 8x** magnifies the preview: each checker square is one screen pixel, and red ticks mark the monitor's true center.

**Pixel-exact centering.** A part sits exactly on the monitor's center only when its size has the same parity as the
monitor (even sizes on 2560 x 1440). Otherwise it lands half a pixel up/left. Settings doesn't hide this: it names each
off-center part and says when parts are 1 px lopsided relative to each other (e.g. even thickness with an odd gap).

**Saved crosshairs.** Three optional slots hold snapshots to switch between. Settings shows whether the selected slot
matches the live crosshair (green) or differs (orange); Load and Save grey out when they would change nothing.

**Monitor.** The crosshair centers on the full selected monitor (not a small game window). If that monitor
disconnects, the main monitor is used until it returns.

**Files.** Settings and slots live in **%LOCALAPPDATA%\CrosshairOverlay\settings.ini**, not beside the exe.
Delete it to reset to defaults. Launch at sign-in uses the **CrosshairOverlay** value under
**HKCU\Software\Microsoft\Windows\CurrentVersion\Run**; if you move the exe, re-enable it from the new copy.

## Compatibility

Works over windowed and borderless games; exclusive fullscreen may cover it.
It is an ordinary click-through window: no injection, game-memory access, driver, input hook, or network access.

## Build

Needs MSYS2 UCRT64 (g++, windres, objdump). From PowerShell:

~~~powershell
.\build.ps1
~~~

Pass **-Compiler C:\path\to\g++.exe** to pick a compiler. The build links everything statically, fails if the exe
imports anything but Windows system DLLs, and keeps it under 1 MB. Exit the running app before rebuilding.

## Remove

Uncheck **Launch at sign-in**, choose **Exit**, delete the exe, and optionally delete **%LOCALAPPDATA%\CrosshairOverlay**.
