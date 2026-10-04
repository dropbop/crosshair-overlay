# Crosshair Overlay

A small Windows 11 x64 tray app that draws one upright crosshair.

## Use

Open **dist\CrosshairOverlay.exe**. The first manual launch enables launch at sign-in.
Settings open on a manual launch; sign-in launches stay quietly in the tray.
Every fresh launch starts with the crosshair visible.

- **Shift + Alt + Z** shows or hides the crosshair. Either side's Shift/Alt works.
- Click the tray icon to open Settings. Right-click for Show/Hide, Settings, Launch at sign-in, and Exit.
- Closing Settings leaves the app running. Choose **Exit** in the tray to stop it.
- Settings save as you edit them. Invalid input is ignored and restored when you leave the field.
- If Windows puts the tray icon in the overflow menu, it is available under the taskbar's hidden-icons arrow.

The preview and overlay use actual screen pixels. Length is per arm; gap is the full empty center opening.
The black outline does not fill that opening. Set outline to zero to disable it.
Opacity applies to the colored cross and outline together.
When odd/even pixel dimensions cannot share an exact half-pixel center, edges snap consistently to the nearest pixel grid.

Select a monitor in Settings. Its device identity is remembered. If it disconnects, the app uses the main monitor until the selected monitor returns. The crosshair is centered on the full monitor, not the area above the taskbar. Its position does not follow a small game window.

Appearance and monitor settings live in **%LOCALAPPDATA%\CrosshairOverlay\settings.ini**.
A temporary file and atomic replacement protect existing settings if a write fails.
Sign-in startup uses this app's **CrosshairOverlay** value under
**HKCU\Software\Microsoft\Windows\CurrentVersion\Run**.
Keep the executable at its chosen path. If you move it, launch the new copy and enable Launch at sign-in again.

## Compatibility

Intended for desktop, windowed, and borderless fullscreen use. Exclusive fullscreen is not guaranteed.
This is an ordinary desktop overlay: no game-memory access, injection, driver, keyboard hook, or network access.
A Windows foreground-change notification keeps it above ordinary windows without taking focus.
Game-specific overlay restrictions and anti-cheat policies still apply.

Only one copy runs per user session. Launching it again opens the existing settings window.
A conflicting global shortcut produces a message in Settings; the tray toggle remains usable.
Opening Settings retries shortcut registration after a conflict has been removed.

## Build and verify

Requires a MinGW-w64 x64 C++17 compiler, windres, and objdump (MSYS2 UCRT64 is supported).
Run from PowerShell:

~~~powershell
.\build.ps1 -Test
~~~

Or specify the compiler with **-Compiler C:\path\to\g++.exe**.
The build statically links compiler support libraries, checks Windows-only DLL imports,
and enforces an executable below 1,000,000 bytes. No runtime install is required.
Tests cover rendering geometry, alpha, monitor-coordinate centering, settings round trips,
invalid settings, and failed saves.

## Remove

Uncheck **Launch at sign-in**, choose **Exit**, then delete the executable.
Delete **%LOCALAPPDATA%\CrosshairOverlay** if you also want to remove saved preferences.
Disabling startup removes only this app's named entry.

For manual acceptance results and remaining hardware/game checks, see **VERIFICATION.md**.
