#pragma once
#include <windows.h>
#include <string>
#include <cwchar>
#include <cerrno>
#include "render.hpp"

namespace crosshair {
constexpr int SlotCount = 3;
struct Settings {
  Appearance appearance;
  Appearance slots[SlotCount];
  bool saved[SlotCount] = {};
  std::wstring monitor;
  bool initialized = false;
};
inline std::wstring slotSection(int slot) { return L"Slot" + std::to_wstring(slot + 1); }
inline int readInteger(const std::wstring& path, const std::wstring& section, const wchar_t* key,
    int fallback, int minimum, int maximum) {
  wchar_t text[64]{};
  GetPrivateProfileStringW(section.c_str(), key, L"", text, 64, path.c_str());
  if (!*text) return fallback;
  wchar_t* tail = nullptr;
  errno = 0;
  const long value = wcstol(text, &tail, 10);
  if (errno || tail == text || *tail || value < minimum || value > maximum) return fallback;
  return static_cast<int>(value);
}
inline Appearance readAppearance(const std::wstring& path, const std::wstring& section) {
  Appearance a;
  for (const auto& f : fields)
    a.*f.member = readInteger(path, section, f.key, a.*f.member, f.minimum, f.maximum);
  return a;
}
inline Settings loadSettings(const std::wstring& path) {
  Settings s;
  s.appearance = readAppearance(path, L"Crosshair");
  s.initialized = readInteger(path, L"Crosshair", L"Initialized", 0, 0, 1) == 1;
  wchar_t monitor[512]{};
  GetPrivateProfileStringW(L"Crosshair", L"Monitor", L"", monitor, 512, path.c_str());
  s.monitor = monitor;
  for (int i = 0; i < SlotCount; ++i) {
    // Only saved slots are written, so a valid Thickness marks one.
    s.saved[i] = readInteger(path, slotSection(i), L"Thickness", 0, 1, 16) != 0;
    if (s.saved[i]) s.slots[i] = readAppearance(path, slotSection(i));
  }
  return s;
}
inline void appendAppearance(std::wstring& text, const std::wstring& section, const Appearance& a) {
  text += L"[" + section + L"]\r\n";
  for (const auto& f : fields) text += std::wstring(f.key) + L"=" + std::to_wstring(a.*f.member) + L"\r\n";
}
// Write a complete UTF-16 INI beside the destination, then atomically replace it.
inline bool saveSettings(const std::wstring& path, const Settings& s) {
  bool ok = valid(s.appearance);
  for (int i = 0; i < SlotCount; ++i) ok = ok && (!s.saved[i] || valid(s.slots[i]));
  if (!ok) { SetLastError(ERROR_INVALID_DATA); return false; }
  std::wstring text = L"\xFEFF";
  appendAppearance(text, L"Crosshair", s.appearance);
  text += L"Initialized=" + std::to_wstring(s.initialized ? 1 : 0) + L"\r\nMonitor=" + s.monitor + L"\r\n";
  for (int i = 0; i < SlotCount; ++i)
    if (s.saved[i]) appendAppearance(text, slotSection(i), s.slots[i]);
  const std::wstring temporary = path + L".tmp";
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const DWORD expected = static_cast<DWORD>(text.size() * sizeof(wchar_t));
  ok = WriteFile(file, text.data(), expected, &written, nullptr) && written == expected;
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
