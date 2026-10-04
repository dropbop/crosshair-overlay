#pragma once
#include <windows.h>
#include <string>
#include <cwchar>
#include <cerrno>
#include "render.hpp"

namespace crosshair {
struct Settings {
  Appearance appearance;
  std::wstring monitor;
  bool initialized = false;
};
inline int readInteger(const std::wstring& path, const wchar_t* key, int fallback,
    int minimum, int maximum) {
  wchar_t text[64]{};
  GetPrivateProfileStringW(L"Crosshair", key, L"", text, 64, path.c_str());
  if (!*text) return fallback;
  wchar_t* tail = nullptr;
  errno = 0;
  const long value = wcstol(text, &tail, 10);
  if (errno || tail == text || *tail || value < minimum || value > maximum) return fallback;
  return static_cast<int>(value);
}
inline Settings loadSettings(const std::wstring& path) {
  Settings s;
  auto& a = s.appearance;
  a.red = readInteger(path, L"Red", a.red, 0, 255);
  a.green = readInteger(path, L"Green", a.green, 0, 255);
  a.blue = readInteger(path, L"Blue", a.blue, 0, 255);
  a.opacity = readInteger(path, L"Opacity", a.opacity, 0, 100);
  a.thickness = readInteger(path, L"Thickness", a.thickness, 1, 16);
  a.length = readInteger(path, L"Length", a.length, 1, 64);
  a.gap = readInteger(path, L"Gap", a.gap, 0, 64);
  a.outline = readInteger(path, L"Outline", a.outline, 0, 8);
  s.initialized = readInteger(path, L"Initialized", 0, 0, 1) == 1;
  wchar_t monitor[512]{};
  GetPrivateProfileStringW(L"Crosshair", L"Monitor", L"", monitor, 512, path.c_str());
  s.monitor = monitor;
  return s;
}
// Write a complete UTF-16 INI beside the destination, then atomically replace it.
inline bool saveSettings(const std::wstring& path, const Settings& s) {
  if (!valid(s.appearance)) { SetLastError(ERROR_INVALID_DATA); return false; }
  const auto& a = s.appearance;
  std::wstring text = L"\xFEFF[Crosshair]\r\nInitialized=" + std::to_wstring(s.initialized ? 1 : 0);
  const wchar_t* keys[] = {L"Red", L"Green", L"Blue", L"Opacity", L"Thickness",
    L"Length", L"Gap", L"Outline"};
  const int values[] = {a.red, a.green, a.blue, a.opacity, a.thickness, a.length, a.gap, a.outline};
  for (int i = 0; i < 8; ++i) text += L"\r\n" + std::wstring(keys[i]) + L"=" + std::to_wstring(values[i]);
  text += L"\r\nMonitor=" + s.monitor + L"\r\n";
  const std::wstring temporary = path + L".tmp";
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const DWORD expected = static_cast<DWORD>(text.size() * sizeof(wchar_t));
  bool ok = WriteFile(file, text.data(), expected, &written, nullptr) && written == expected;
  if (ok) ok = FlushFileBuffers(file) != FALSE;
  DWORD error = GetLastError();
  CloseHandle(file);
  if (ok) {
    ok = MoveFileExW(temporary.c_str(), path.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    error = GetLastError();
  }
  if (!ok) DeleteFileW(temporary.c_str());
  SetLastError(error);
  return ok;
}
} // namespace crosshair
