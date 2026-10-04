#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace crosshair {
struct Appearance {
  int red = 0, green = 255, blue = 0;
  int opacity = 100, thickness = 2, length = 8, gap = 4, outline = 1;
};
struct Rect {
  int left, top, right, bottom;
  bool contains(int x, int y) const {
    return x >= left && x < right && y >= top && y < bottom;
  }
};
struct Raster {
  int side = 0;
  std::vector<uint32_t> pixels;
};
inline bool valid(const Appearance& a) {
  return a.red >= 0 && a.red <= 255 && a.green >= 0 && a.green <= 255 &&
    a.blue >= 0 && a.blue <= 255 && a.opacity >= 0 && a.opacity <= 100 &&
    a.thickness >= 1 && a.thickness <= 16 && a.length >= 1 && a.length <= 64 &&
    a.gap >= 0 && a.gap <= 64 && a.outline >= 0 && a.outline <= 8;
}
inline Raster render(const Appearance& a) {
  if (!valid(a)) return {};
  Raster result;
  result.side = 2 * (a.length + a.outline + 2) + std::max(a.gap, a.thickness);
  const int n = result.side;
  const int stripe = (n - a.thickness) / 2;
  const int start = (n - a.gap) / 2;
  const int end = start + a.gap;
  const Rect arms[] = {
    {stripe, start - a.length, stripe + a.thickness, start},
    {stripe, end, stripe + a.thickness, end + a.length},
    {start - a.length, stripe, start, stripe + a.thickness},
    {end, stripe, end + a.length, stripe + a.thickness}
  };
  const Rect hole{start, start, end, end};
  const uint32_t alpha = (a.opacity * 255 + 50) / 100;
  const auto premultiply = [alpha](int channel) { return (channel * alpha + 127) / 255; };
  const uint32_t color = (alpha << 24) | (premultiply(a.red) << 16) |
    (premultiply(a.green) << 8) | premultiply(a.blue);
  result.pixels.assign(n * n, 0);
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      if (a.gap > 0 && hole.contains(x, y)) continue;
      bool fill = false, border = false;
      for (const auto& arm : arms) {
        fill |= arm.contains(x, y);
        const Rect expanded{arm.left - a.outline, arm.top - a.outline,
          arm.right + a.outline, arm.bottom + a.outline};
        border |= expanded.contains(x, y);
      }
      result.pixels[y * n + x] = fill ? color : (border ? alpha << 24 : 0);
    }
  }
  return result;
}
inline int centeredOrigin(int origin, int extent, int imageExtent) {
  return origin + (extent - imageExtent) / 2;
}
} // namespace crosshair
