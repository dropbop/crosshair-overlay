#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace crosshair {
struct Appearance {
  int red = 0, green = 255, blue = 255;
  int opacity = 100, thickness = 2, length = 4, gap = 4, outline = 1, dot = 0;
};
// One table drives validation, the INI keys, and the settings rows.
struct Field {
  const wchar_t* key;
  const wchar_t* label;
  int Appearance::* member;
  int minimum, maximum;
};
inline const Field fields[] = {
  {L"Red", L"&Red", &Appearance::red, 0, 255},
  {L"Green", L"&Green", &Appearance::green, 0, 255},
  {L"Blue", L"&Blue", &Appearance::blue, 0, 255},
  {L"Opacity", L"&Opacity (%)", &Appearance::opacity, 0, 100},
  {L"Thickness", L"&Thickness", &Appearance::thickness, 1, 16},
  {L"Length", L"&Length per arm", &Appearance::length, 1, 64},
  {L"Gap", L"Center ga&p", &Appearance::gap, 0, 64},
  {L"Dot", L"Center &dot (0 = off)", &Appearance::dot, 0, 16},
  {L"Outline", L"O&utline (0 = off)", &Appearance::outline, 0, 8}
};
constexpr int FieldCount = sizeof(fields) / sizeof(fields[0]);
struct Rect {
  int left, top, right, bottom;
  bool contains(int x, int y) const {
    return x >= left && x < right && y >= top && y < bottom;
  }
  Rect grown(int d) const { return {left - d, top - d, right + d, bottom + d}; }
};
struct Raster {
  int width = 0, height = 0;
  std::vector<uint32_t> pixels;
};
inline bool valid(const Appearance& a) {
  for (const auto& f : fields)
    if (a.*f.member < f.minimum || a.*f.member > f.maximum) return false;
  return true;
}
inline bool same(const Appearance& a, const Appearance& b) {
  for (const auto& f : fields)
    if (a.*f.member != b.*f.member) return false;
  return true;
}
// The raster's width/height parity matches the target monitor's, so centering the
// raster on the monitor is exact and each part is rounded only once: a part whose
// size parity differs from the monitor's lands exactly half a pixel up/left.
inline Raster render(const Appearance& a, bool oddWidth = false, bool oddHeight = false) {
  if (!valid(a)) return {};
  const int base = 2 * (a.length + a.outline + 2) + std::max({a.gap, a.thickness, a.dot});
  Raster result;
  result.width = base + ((base & 1) != oddWidth);
  result.height = base + ((base & 1) != oddHeight);
  const int w = result.width, h = result.height;
  const int sx = (w - a.thickness) / 2, sy = (h - a.thickness) / 2;
  const int gx = (w - a.gap) / 2, gy = (h - a.gap) / 2;
  const int dx = (w - a.dot) / 2, dy = (h - a.dot) / 2;
  const Rect shapes[] = {
    {sx, gy - a.length, sx + a.thickness, gy},
    {sx, gy + a.gap, sx + a.thickness, gy + a.gap + a.length},
    {gx - a.length, sy, gx, sy + a.thickness},
    {gx + a.gap, sy, gx + a.gap + a.length, sy + a.thickness},
    {dx, dy, dx + a.dot, dy + a.dot}
  };
  const int count = a.dot ? 5 : 4;
  const Rect hole{gx, gy, gx + a.gap, gy + a.gap};
  const uint32_t alpha = (a.opacity * 255 + 50) / 100;
  const auto premultiply = [alpha](int channel) { return (channel * alpha + 127) / 255; };
  const uint32_t color = (alpha << 24) | (premultiply(a.red) << 16) |
    (premultiply(a.green) << 8) | premultiply(a.blue);
  result.pixels.assign(w * h, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const bool open = a.gap > 0 && hole.contains(x, y);
      bool fill = false, border = false;
      for (int i = 0; i < count; ++i) {
        if (open && i < 4) continue;  // Arm outlines never fill the gap; the dot may.
        fill |= shapes[i].contains(x, y);
        border |= shapes[i].grown(a.outline).contains(x, y);
      }
      result.pixels[y * w + x] = fill ? color : (border ? alpha << 24 : 0);
    }
  }
  return result;
}
inline int centeredOrigin(int origin, int extent, int imageExtent) {
  return origin + (extent - imageExtent) / 2;
}
} // namespace crosshair
