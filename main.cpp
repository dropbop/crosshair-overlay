#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wtsapi32.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>
#include "settings.hpp"

namespace {
constexpr wchar_t AppName[] = L"Crosshair Overlay";
constexpr wchar_t MainClass[] = L"CrosshairOverlay.Settings.1";
constexpr wchar_t OverlayClass[] = L"CrosshairOverlay.Overlay.1";
constexpr wchar_t PreviewClass[] = L"CrosshairOverlay.Preview.1";
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t RunValue[] = L"CrosshairOverlay";
constexpr UINT TrayMessage = WM_APP + 1, OpenSettingsMessage = WM_APP + 2;
constexpr UINT TrayRetry = 1, DisplayRefresh = 2;
constexpr int FirstField = 100, MonitorControl = 200, StartupControl = 201;
constexpr int ToggleCommand = 301, SettingsCommand = 302, StartupCommand = 303, ExitCommand = 304;
const GUID TrayGuid = {0x998ab17c, 0x1e07, 0x44bf, {0x90,0x62,0x6d,0x2e,0xa5,0x7c,0xf7,0x91}};
struct Field {
  const wchar_t* label;
  int crosshair::Appearance::* member;
  int minimum, maximum;
};
const Field fields[] = {
  {L"&Red (0-255)", &crosshair::Appearance::red, 0, 255},
  {L"&Green (0-255)", &crosshair::Appearance::green, 0, 255},
  {L"&Blue (0-255)", &crosshair::Appearance::blue, 0, 255},
  {L"&Opacity (%)", &crosshair::Appearance::opacity, 0, 100},
  {L"&Thickness (px)", &crosshair::Appearance::thickness, 1, 16},
  {L"&Length per arm (px)", &crosshair::Appearance::length, 1, 64},
  {L"Center &gap (px)", &crosshair::Appearance::gap, 0, 64},
  {L"Black o&utline (px)", &crosshair::Appearance::outline, 0, 8}
};
struct Monitor {
  HMONITOR handle;
  RECT rect;
  bool primary;
  std::wstring identity, label;
};
struct Control {
  HWND window;
  int x, y, width, height;
  bool heading;
};
HINSTANCE instance;
HWND mainWindow, overlayWindow, previewWindow, statusWindow;
HFONT bodyFont, headingFont;
HICON trayIcon;
HANDLE instanceMutex;
HWINEVENTHOOK foregroundHook;
UINT taskbarCreated;
UINT dpi = 96;
bool visible = true, initializing = true, hotkeyRegistered = false, trayAdded = false;
bool startupEnabled = false, startupMode = false, quitting = false;
int trayRetries = 0;
std::wstring configPath, executablePath, saveError, startupError, inputError, displayError;
crosshair::Settings settings;
crosshair::Raster raster;
std::vector<Monitor> monitors;
std::vector<Control> controls;

std::wstring systemError(DWORD error) {
  wchar_t* text = nullptr;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
  std::wstring result = text ? text : L"Windows error " + std::to_wstring(error);
  if (text) LocalFree(text);
  while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) result.pop_back();
  return result;
}
int px(int logical) { return MulDiv(logical, static_cast<int>(dpi), 96); }
void updateStatus();
void showSettings();
void refreshDisplay();
void updateOverlay();
void refreshStartup();
void updateTray();

void persist() {
  if (configPath.empty()) {
    saveError = L"Settings cannot be saved: the local settings folder is unavailable.";
  } else if (!crosshair::saveSettings(configPath, settings)) {
    saveError = L"Settings were not saved: " + systemError(GetLastError());
  } else {
    saveError.clear();
  }
  updateStatus();
}
std::wstring startupCommand() { return L"\"" + executablePath + L"\" --startup"; }
void refreshStartup() {
  wchar_t value[32768]{};
  DWORD bytes = sizeof(value);
  const LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, RunKey, RunValue,
    RRF_RT_REG_SZ, nullptr, value, &bytes);
  startupEnabled = result == ERROR_SUCCESS && startupCommand() == value;
  if (mainWindow) CheckDlgButton(mainWindow, StartupControl, startupEnabled ? BST_CHECKED : BST_UNCHECKED);
}
void setStartup(bool enabled) {
  if (enabled && startupCommand().size() > 260) {
    startupError = L"Startup path is too long. Move the app to a shorter path.";
  } else {
    HKEY key = nullptr;
    LSTATUS result = RegCreateKeyExW(HKEY_CURRENT_USER, RunKey, 0, nullptr, 0,
      KEY_SET_VALUE, nullptr, &key, nullptr);
    if (result == ERROR_SUCCESS) {
      if (enabled) {
        const auto command = startupCommand();
        result = RegSetValueExW(key, RunValue, 0, REG_SZ,
          reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
      } else {
        result = RegDeleteValueW(key, RunValue);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
      }
      RegCloseKey(key);
    }
    startupError = result == ERROR_SUCCESS ? L"" : L"Startup could not be changed: " + systemError(result);
  }
  refreshStartup();
  updateStatus();
}
void updateStatus() {
  if (!statusWindow) return;
  std::wstring message;
  if (!inputError.empty()) message = inputError;
  else if (!saveError.empty()) message = saveError;
  else if (!startupError.empty()) message = startupError;
  else if (!displayError.empty()) message = displayError;
  else if (!hotkeyRegistered) message = L"Shift + Alt + Z is unavailable. Another app may use it. Use the tray to toggle.";
  else message = visible ? L"Crosshair shown  |  Shift + Alt + Z to hide" :
    L"Crosshair hidden  |  Shift + Alt + Z to show";
  SetWindowTextW(statusWindow, message.c_str());
}
void notifyError(const wchar_t* message) {
  NOTIFYICONDATAW data{sizeof(data)};
  data.hWnd = mainWindow;
  data.uFlags = NIF_GUID | NIF_INFO;
  data.guidItem = TrayGuid;
  wcscpy_s(data.szInfoTitle, AppName);
  wcsncpy_s(data.szInfo, message, _TRUNCATE);
  data.dwInfoFlags = NIIF_WARNING;
  Shell_NotifyIconW(NIM_MODIFY, &data);
}
void registerHotkey() {
  if (hotkeyRegistered) return;
  hotkeyRegistered = RegisterHotKey(mainWindow, 1, MOD_SHIFT | MOD_ALT | MOD_NOREPEAT, 'Z') != FALSE;
  updateStatus();
}
void updateTray() {
  NOTIFYICONDATAW data{sizeof(data)};
  data.hWnd = mainWindow;
  data.uFlags = NIF_GUID | NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  data.guidItem = TrayGuid;
  data.uCallbackMessage = TrayMessage;
  data.hIcon = trayIcon;
  wcscpy_s(data.szTip, visible ? L"Crosshair Overlay - shown" : L"Crosshair Overlay - hidden");
  if (Shell_NotifyIconW(trayAdded ? NIM_MODIFY : NIM_ADD, &data)) {
    trayAdded = true;
    trayRetries = 0;
    KillTimer(mainWindow, TrayRetry);
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
  } else if (++trayRetries <= 5) {
    SetTimer(mainWindow, TrayRetry, 2000, nullptr);
  } else {
    KillTimer(mainWindow, TrayRetry);
    if (!IsWindowVisible(mainWindow)) showSettings();
    SetWindowTextW(statusWindow, L"The tray icon could not be added. Settings remain available.");
  }
}
void toggle() {
  visible = !visible;
  updateOverlay();
  updateTray();
  updateStatus();
}

std::wstring monitorIdentity(const wchar_t* sourceName, std::wstring& friendlyName) {
  UINT32 pathCount = 0, modeCount = 0;
  const UINT32 flags = QDC_ONLY_ACTIVE_PATHS;
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (GetDisplayConfigBufferSizes(flags, &pathCount, &modeCount) != ERROR_SUCCESS) break;
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    const LONG result = QueryDisplayConfig(flags, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
    if (result == ERROR_INSUFFICIENT_BUFFER) continue;
    if (result != ERROR_SUCCESS) break;
    for (UINT32 i = 0; i < pathCount; ++i) {
      DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
      source.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source),
        paths[i].sourceInfo.adapterId, paths[i].sourceInfo.id};
      if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
          wcscmp(source.viewGdiDeviceName, sourceName) != 0) continue;
      DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
      target.header = {DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME, sizeof(target),
        paths[i].targetInfo.adapterId, paths[i].targetInfo.id};
      if (DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS) {
        friendlyName = target.monitorFriendlyDeviceName;
        return target.monitorDevicePath;
      }
    }
    break;
  }
  return sourceName;
}
BOOL CALLBACK enumerateMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM) {
  MONITORINFOEXW info{};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(monitor, &info)) return TRUE;
  std::wstring friendly;
  Monitor entry{monitor, info.rcMonitor, (info.dwFlags & MONITORINFOF_PRIMARY) != 0, L"", L""};
  entry.identity = monitorIdentity(info.szDevice, friendly);
  if (friendly.empty()) friendly = info.szDevice;
  entry.label = friendly + L"  (" + std::to_wstring(info.rcMonitor.right - info.rcMonitor.left) +
    L" x " + std::to_wstring(info.rcMonitor.bottom - info.rcMonitor.top) + L")";
  if (entry.primary) entry.label += L" - main";
  monitors.push_back(entry);
  return TRUE;
}
const Monitor* chosenMonitor() {
  for (const auto& monitor : monitors)
    if (monitor.identity == settings.monitor) return &monitor;
  for (const auto& monitor : monitors)
    if (monitor.primary) return &monitor;
  return monitors.empty() ? nullptr : &monitors.front();
}
void refreshDisplay() {
  monitors.clear();
  EnumDisplayMonitors(nullptr, nullptr, enumerateMonitor, 0);
  if (settings.monitor.empty()) {
    if (const auto* monitor = chosenMonitor()) {
      settings.monitor = monitor->identity;
      if (!initializing) persist();
    }
  }
  const HWND combo = GetDlgItem(mainWindow, MonitorControl);
  if (combo) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selected = -1;
    for (size_t i = 0; i < monitors.size(); ++i) {
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(monitors[i].label.c_str()));
      if (monitors[i].identity == settings.monitor) selected = static_cast<int>(i);
    }
    if (selected < 0 && !settings.monitor.empty()) {
      selected = static_cast<int>(monitors.size());
      SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Saved monitor disconnected - using main display"));
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
  }
  updateOverlay();
}
BITMAPINFO bitmapInfo(int width, int height) {
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  return info;
}
void updateOverlay() {
  if (!overlayWindow) return;
  const Monitor* monitor = chosenMonitor();
  if (!visible || !monitor) { ShowWindow(overlayWindow, SW_HIDE); return; }
  if (raster.pixels.empty()) raster = crosshair::render(settings.appearance);
  const int side = raster.side;
  HDC screen = GetDC(nullptr);
  HDC memory = CreateCompatibleDC(screen);
  auto info = bitmapInfo(side, side);
  void* bits = nullptr;
  HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!screen || !memory || !bitmap) {
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    if (screen) ReleaseDC(nullptr, screen);
    displayError = L"Could not allocate the crosshair bitmap.";
    updateStatus();
    return;
  }
  memcpy(bits, raster.pixels.data(), raster.pixels.size() * sizeof(uint32_t));
  HGDIOBJ old = SelectObject(memory, bitmap);
  POINT destination{
    crosshair::centeredOrigin(monitor->rect.left, monitor->rect.right - monitor->rect.left, side),
    crosshair::centeredOrigin(monitor->rect.top, monitor->rect.bottom - monitor->rect.top, side)};
  SIZE size{side, side};
  POINT source{0, 0};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  if (!UpdateLayeredWindow(overlayWindow, screen, &destination, &size, memory, &source, 0, &blend, ULW_ALPHA))
    displayError = L"Crosshair could not be displayed: " + systemError(GetLastError());
  else displayError.clear();
  SelectObject(memory, old);
  DeleteObject(bitmap);
  DeleteDC(memory);
  ReleaseDC(nullptr, screen);
  SetWindowPos(overlayWindow, HWND_TOPMOST, 0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  updateStatus();
}
void appearanceChanged() {
  raster = crosshair::render(settings.appearance);
  updateOverlay();
  if (previewWindow) InvalidateRect(previewWindow, nullptr, FALSE);
  persist();
}
HICON makeIcon() {
  crosshair::Appearance appearance;
  appearance.length = 7;
  appearance.thickness = 2;
  appearance.gap = 4;
  appearance.outline = 1;
  const auto iconRaster = crosshair::render(appearance);
  const int size = 32;
  auto info = bitmapInfo(size, size);
  void* bits = nullptr;
  HDC dc = GetDC(nullptr);
  HBITMAP color = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, dc);
  if (!color) return nullptr;
  auto* pixels = static_cast<uint32_t*>(bits);
  std::fill(pixels, pixels + size * size, 0);
  const int offset = (size - iconRaster.side) / 2;
  for (int y = 0; y < iconRaster.side; ++y)
    for (int x = 0; x < iconRaster.side; ++x)
      pixels[(y + offset) * size + x + offset] = iconRaster.pixels[y * iconRaster.side + x];
  BYTE maskBits[128]{};
  HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits);
  ICONINFO ii{TRUE, 0, 0, mask, color};
  HICON icon = CreateIconIndirect(&ii);
  DeleteObject(mask);
  DeleteObject(color);
  return icon;
}

HWND control(const wchar_t* type, const wchar_t* text, DWORD style, int id,
    int x, int y, int width, int height, DWORD extra = 0, bool heading = false) {
  HWND window = CreateWindowExW(extra, type, text, WS_CHILD | WS_VISIBLE | style,
    px(x), px(y), px(width), px(height), mainWindow,
    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
  controls.push_back({window, x, y, width, height, heading});
  return window;
}
void layout() {
  HFONT previousBody = bodyFont, previousHeading = headingFont;
  bodyFont = CreateFontW(-MulDiv(10, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  headingFont = CreateFontW(-MulDiv(17, static_cast<int>(dpi), 72), 0, 0, 0, FW_SEMIBOLD,
    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
  for (const auto& c : controls) {
    SetWindowPos(c.window, nullptr, px(c.x), px(c.y), px(c.width), px(c.height),
      SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(c.window, WM_SETFONT, reinterpret_cast<WPARAM>(c.heading ? headingFont : bodyFont), TRUE);
  }
  if (previousBody) DeleteObject(previousBody);
  if (previousHeading) DeleteObject(previousHeading);
  InvalidateRect(mainWindow, nullptr, TRUE);
}
void createControls() {
  control(L"STATIC", L"Crosshair", 0, -1, 20, 14, 280, 32, 0, true);
  control(L"STATIC", L"Changes are saved automatically.", 0, -1, 20, 50, 440, 24);
  for (int i = 0; i < 8; ++i) {
    const int y = 86 + i * 30;
    control(L"STATIC", fields[i].label, 0, -1, 20, y + 3, 154, 24);
    HWND edit = control(L"EDIT", L"", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, FirstField + i,
      180, y, 68, 26, WS_EX_CLIENTEDGE);
    SendMessageW(edit, EM_SETLIMITTEXT, 3, 0);
    HWND spin = control(UPDOWN_CLASSW, L"", UDS_ARROWKEYS | UDS_SETBUDDYINT,
      400 + i, 248, y, 18, 26);
    SendMessageW(spin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(edit), 0);
    SendMessageW(spin, UDM_SETRANGE32, fields[i].minimum, fields[i].maximum);
    SendMessageW(spin, UDM_SETPOS32, 0, settings.appearance.*(fields[i].member));
  }
  control(L"STATIC", L"Live preview", 0, -1, 292, 86, 156, 24);
  previewWindow = control(PreviewClass, L"Crosshair preview", 0, -1, 286, 116, 174, 170, WS_EX_CLIENTEDGE);
  control(L"STATIC", L"Actual screen pixels.\r\nOutline 0 turns it off.", 0, -1, 286, 292, 174, 42);
  control(L"STATIC", L"&Monitor", 0, -1, 20, 332, 440, 24);
  control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
    MonitorControl, 20, 356, 440, 200);
  control(L"BUTTON", L"Launch at &sign-in", WS_TABSTOP | BS_AUTOCHECKBOX,
    StartupControl, 20, 398, 240, 26);
  statusWindow = control(L"STATIC", L"", 0, -1, 20, 432, 440, 42);
  control(L"BUTTON", L"&Close", WS_TABSTOP | BS_DEFPUSHBUTTON, IDOK, 360, 484, 100, 28);
  layout();
}
void showSettings() {
  refreshStartup();
  registerHotkey();
  updateStatus();
  ShowWindow(mainWindow, SW_RESTORE);
  SetForegroundWindow(mainWindow);
}
void trayMenu(POINT point) {
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, ToggleCommand, visible ? L"Hide crosshair" : L"Show crosshair");
  AppendMenuW(menu, MF_STRING, SettingsCommand, L"Settings");
  refreshStartup();
  AppendMenuW(menu, MF_STRING | (startupEnabled ? MF_CHECKED : 0), StartupCommand, L"Launch at sign-in");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ExitCommand, L"Exit");
  SetForegroundWindow(mainWindow);
  const UINT chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
    point.x, point.y, 0, mainWindow, nullptr);
  DestroyMenu(menu);
  PostMessageW(mainWindow, WM_NULL, 0, 0);
  if (chosen) SendMessageW(mainWindow, WM_COMMAND, chosen, 0);
}
void editChanged(int fieldIndex, int notification) {
  if (initializing) return;
  const auto& field = fields[fieldIndex];
  BOOL converted = FALSE;
  const UINT value = GetDlgItemInt(mainWindow, FirstField + fieldIndex, &converted, FALSE);
  if (!converted || value < static_cast<UINT>(field.minimum) || value > static_cast<UINT>(field.maximum)) {
    if (notification == EN_KILLFOCUS) {
      SetDlgItemInt(mainWindow, FirstField + fieldIndex, settings.appearance.*(field.member), FALSE);
      inputError.clear();
    } else {
      inputError = L"Enter a value from " + std::to_wstring(field.minimum) + L" to " +
        std::to_wstring(field.maximum) + L".";
    }
    updateStatus();
    return;
  }
  inputError.clear();
  if (settings.appearance.*(field.member) != static_cast<int>(value)) {
    settings.appearance.*(field.member) = static_cast<int>(value);
    appearanceChanged();
  }
  updateStatus();
}
LRESULT CALLBACK previewProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_ERASEBKGND) return 1;
  if (message == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(window, &ps);
    RECT rect;
    GetClientRect(window, &rect);
    const int width = rect.right, height = rect.bottom;
    if (width > 0 && height > 0) {
      std::vector<uint32_t> pixels(width * height);
      const int originX = (width - raster.side) / 2, originY = (height - raster.side) / 2;
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const uint32_t background = ((x / 10 + y / 10) % 2) ? 222 : 244;
          uint32_t pixel = background * 0x010101;
          const int rx = x - originX, ry = y - originY;
          if (rx >= 0 && ry >= 0 && rx < raster.side && ry < raster.side) {
            const uint32_t source = raster.pixels[ry * raster.side + rx];
            const uint32_t remaining = (background * (255 - (source >> 24)) + 127) / 255;
            pixel = (((source >> 16) & 255) + remaining) << 16 |
              (((source >> 8) & 255) + remaining) << 8 | ((source & 255) + remaining);
          }
          pixels[y * width + x] = pixel;
        }
      }
      auto info = bitmapInfo(width, height);
      SetDIBitsToDevice(dc, 0, 0, width, height, 0, 0, 0, height, pixels.data(), &info, DIB_RGB_COLORS);
    }
    EndPaint(window, &ps);
    return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}
LRESULT CALLBACK overlayProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
  if (message == WM_NCHITTEST) return HTTRANSPARENT;
  return DefWindowProcW(window, message, wParam, lParam);
}
void CALLBACK foregroundChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
  // Out-of-context notification only: never reads or injects into another process.
  if (!quitting && visible && overlayWindow && IsWindowVisible(overlayWindow))
    SetWindowPos(overlayWindow, HWND_TOPMOST, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
LRESULT CALLBACK mainProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (taskbarCreated && message == taskbarCreated) {
    trayAdded = false;
    trayRetries = 0;
    updateTray();
    return 0;
  }
  switch (message) {
    case WM_HOTKEY:
      if (wParam == 1) toggle();
      return 0;
    case OpenSettingsMessage:
      showSettings();
      return 0;
    case TrayMessage:
      switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case NIN_KEYSELECT: showSettings(); break;
        case WM_CONTEXTMENU: {
          POINT point{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};
          if (point.x == -1 && point.y == -1) GetCursorPos(&point);
          trayMenu(point);
          break;
        }
        case NIN_BALLOONUSERCLICK: showSettings(); break;
      }
      return 0;
    case WM_COMMAND: {
      const int id = LOWORD(wParam), notification = HIWORD(wParam);
      if (id >= FirstField && id < FirstField + 8 &&
          (notification == EN_CHANGE || notification == EN_KILLFOCUS)) {
        editChanged(id - FirstField, notification);
        return 0;
      }
      if (id == MonitorControl && notification == CBN_SELCHANGE) {
        const LRESULT selected = SendDlgItemMessageW(window, MonitorControl, CB_GETCURSEL, 0, 0);
        if (selected >= 0 && static_cast<size_t>(selected) < monitors.size()) {
          settings.monitor = monitors[selected].identity;
          updateOverlay();
          persist();
        }
      } else if (id == StartupControl && notification == BN_CLICKED) {
        setStartup(IsDlgButtonChecked(window, StartupControl) == BST_CHECKED);
      } else if (id == StartupCommand) setStartup(!startupEnabled);
      else if (id == ToggleCommand) toggle();
      else if (id == SettingsCommand) showSettings();
      else if (id == ExitCommand) DestroyWindow(window);
      else if (id == IDOK || id == IDCANCEL) ShowWindow(window, SW_HIDE);
      return 0;
    }
    case WM_CLOSE:
      ShowWindow(window, SW_HIDE);
      return 0;
    case WM_DPICHANGED: {
      dpi = HIWORD(wParam);
      const auto* suggested = reinterpret_cast<const RECT*>(lParam);
      SetWindowPos(window, nullptr, suggested->left, suggested->top,
        suggested->right - suggested->left, suggested->bottom - suggested->top,
        SWP_NOZORDER | SWP_NOACTIVATE);
      layout();
      SetTimer(window, DisplayRefresh, 150, nullptr);
      return 0;
    }
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
    case WM_DEVICECHANGE:
    case WM_WTSSESSION_CHANGE:
      SetTimer(window, DisplayRefresh, 150, nullptr);
      return 0;
    case WM_POWERBROADCAST:
      if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND)
        SetTimer(window, DisplayRefresh, 150, nullptr);
      return TRUE;
    case WM_TIMER:
      if (wParam == DisplayRefresh) { KillTimer(window, DisplayRefresh); refreshDisplay(); }
      else if (wParam == TrayRetry) updateTray();
      return 0;
    case WM_QUERYENDSESSION:
      return TRUE;
    case WM_ENDSESSION:
      if (wParam) DestroyWindow(window);
      return 0;
    case WM_DESTROY: {
      quitting = true;
      if (foregroundHook) UnhookWinEvent(foregroundHook);
      UnregisterHotKey(window, 1);
      WTSUnRegisterSessionNotification(window);
      NOTIFYICONDATAW data{sizeof(data)};
      data.hWnd = window;
      data.uFlags = NIF_GUID;
      data.guidItem = TrayGuid;
      Shell_NotifyIconW(NIM_DELETE, &data);
      if (overlayWindow) DestroyWindow(overlayWindow);
      PostQuitMessage(0);
      return 0;
    }
  }
  return DefWindowProcW(window, message, wParam, lParam);
}
bool registerClass(const wchar_t* name, WNDPROC procedure, HBRUSH brush) {
  WNDCLASSEXW type{sizeof(type)};
  type.lpfnWndProc = procedure;
  type.hInstance = instance;
  type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  type.hbrBackground = brush;
  type.lpszClassName = name;
  type.hIcon = trayIcon;
  type.hIconSm = trayIcon;
  return RegisterClassExW(&type) != 0;
}
bool initializePaths() {
  wchar_t path[32768]{};
  const DWORD length = GetModuleFileNameW(nullptr, path, 32768);
  if (!length || length >= 32768) return false;
  executablePath.assign(path, length);
  PWSTR local = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) return false;
  const std::wstring directory = std::wstring(local) + L"\\CrosshairOverlay";
  CoTaskMemFree(local);
  if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
  configPath = directory + L"\\settings.ini";
  return true;
}
} // namespace

int WINAPI wWinMain(HINSTANCE appInstance, HINSTANCE, PWSTR, int) {
  instance = appInstance;
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (int i = 1; argv && i < argc; ++i) {
    if (wcscmp(argv[i], L"--startup") == 0) startupMode = true;
    else {
      MessageBoxW(nullptr, L"Usage: CrosshairOverlay.exe [--startup]", AppName, MB_OK | MB_ICONINFORMATION);
      LocalFree(argv);
      return 1;
    }
  }
  if (argv) LocalFree(argv);
  instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\CrosshairOverlay.998ab17c-1e07-44bf-9062-6d2ea57cf791");
  if (!instanceMutex) return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (!startupMode) {
      HWND existing = nullptr;
      for (int attempt = 0; attempt < 20 && !existing; ++attempt) {
        existing = FindWindowW(MainClass, AppName);
        if (!existing) Sleep(50);
      }
      if (existing) {
        DWORD process = 0;
        GetWindowThreadProcessId(existing, &process);
        AllowSetForegroundWindow(process);
        PostMessageW(existing, OpenSettingsMessage, 0, 0);
      }
    }
    CloseHandle(instanceMutex);
    return 0;
  }
  const bool pathsReady = initializePaths();
  if (pathsReady) settings = crosshair::loadSettings(configPath);
  else saveError = L"Settings folder is unavailable. Changes cannot be saved.";
  const bool firstLaunch = !settings.initialized;
  raster = crosshair::render(settings.appearance);
  INITCOMMONCONTROLSEX common{sizeof(common), ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&common);
  trayIcon = makeIcon();
  taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
  if (!registerClass(MainClass, mainProc, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1)) ||
      !registerClass(OverlayClass, overlayProc, nullptr) ||
      !registerClass(PreviewClass, previewProc, nullptr)) return 1;
  dpi = GetDpiForSystem();
  RECT bounds{0, 0, px(480), px(526)};
  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  AdjustWindowRectExForDpi(&bounds, style, FALSE, WS_EX_CONTROLPARENT, dpi);
  mainWindow = CreateWindowExW(WS_EX_CONTROLPARENT, MainClass, AppName, style,
    CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
    nullptr, nullptr, instance, nullptr);
  if (!mainWindow) return 1;
  dpi = GetDpiForWindow(mainWindow);
  createControls();
  overlayWindow = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
    WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, OverlayClass, L"Crosshair", WS_POPUP,
    0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
  if (!overlayWindow) {
    MessageBoxW(mainWindow, L"Could not create the overlay window.", AppName, MB_OK | MB_ICONERROR);
    DestroyWindow(mainWindow);
    return 1;
  }
  refreshDisplay();
  initializing = false;
  registerHotkey();
  refreshStartup();
  if (firstLaunch && !startupMode) {
    setStartup(true);
    settings.initialized = true;
    persist();
  }
  updateTray();
  WTSRegisterSessionNotification(mainWindow, NOTIFY_FOR_THIS_SESSION);
  foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
    nullptr, foregroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
  if (!startupMode) showSettings();
  else if (!saveError.empty() || !hotkeyRegistered) {
    notifyError(!saveError.empty() ? saveError.c_str() :
      L"Shift + Alt + Z is unavailable. Open Settings from the tray.");
  }
  MSG message{};
  int result = 0;
  while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    if (IsWindowVisible(mainWindow) && IsDialogMessageW(mainWindow, &message)) continue;
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  if (bodyFont) DeleteObject(bodyFont);
  if (headingFont) DeleteObject(headingFont);
  if (trayIcon) DestroyIcon(trayIcon);
  CloseHandle(instanceMutex);
  return result == -1 ? 1 : 0;
}
