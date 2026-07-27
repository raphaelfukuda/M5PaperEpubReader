#pragma once

#include <cstdint>

struct DisplayRegion {
  DisplayRegion() = default;
  DisplayRegion(int32_t left, int32_t top, int32_t regionWidth, int32_t regionHeight)
      : x(left), y(top), width(regionWidth), height(regionHeight) {}

  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;

  bool empty() const { return width <= 0 || height <= 0; }
  uint32_t area() const;
  float ratio(uint32_t totalPixels) const;
  DisplayRegion clamped(int32_t displayWidth, int32_t displayHeight) const;
  DisplayRegion expanded(int32_t pixels) const;
  DisplayRegion aligned(int32_t multiple, int32_t displayWidth,
                        int32_t displayHeight) const;
  static DisplayRegion unite(const DisplayRegion& first, const DisplayRegion& second);
  static DisplayRegion intersect(const DisplayRegion& first,
                                 const DisplayRegion& second);
};
