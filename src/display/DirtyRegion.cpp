#include "DirtyRegion.h"

#include <algorithm>

namespace {
uint8_t countBits(uint8_t value) {
  value = static_cast<uint8_t>(value - ((value >> 1) & 0x55));
  value = static_cast<uint8_t>((value & 0x33) + ((value >> 2) & 0x33));
  return static_cast<uint8_t>((value + (value >> 4)) & 0x0F);
}
}

DirtyRegionResult compareMonoBuffers(const uint8_t* previous, const uint8_t* next,
                                     int32_t width, int32_t height,
                                     size_t strideBytes) {
  DirtyRegionResult result;
  if (!previous || !next || width <= 0 || height <= 0 ||
      strideBytes < static_cast<size_t>((width + 7) / 8))
    return result;
  int32_t minX = width, minY = height, maxX = -1, maxY = -1;
  const size_t visibleBytes = static_cast<size_t>((width + 7) / 8);
  for (int32_t y = 0; y < height; ++y) {
    for (size_t byteX = 0; byteX < visibleBytes; ++byteX) {
      uint8_t changed = previous[y * strideBytes + byteX] ^ next[y * strideBytes + byteX];
      if (byteX + 1 == visibleBytes && (width & 7))
        changed &= static_cast<uint8_t>(0xFFu << (8 - (width & 7)));
      if (!changed) continue;
      result.changedPixels += countBits(changed);
      for (uint8_t bit = 0; bit < 8; ++bit) {
        if (!(changed & (0x80u >> bit))) continue;
        const int32_t x = static_cast<int32_t>(byteX * 8 + bit);
        if (x >= width) continue;
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
      }
    }
  }
  result.comparedPixels = static_cast<uint32_t>(width) * height;
  result.changedRatio = result.comparedPixels
                            ? static_cast<float>(result.changedPixels) / result.comparedPixels
                            : 0.0f;
  result.valid = true;
  if (maxX >= minX && maxY >= minY)
    result.bounds = DisplayRegion(minX, minY, maxX - minX + 1, maxY - minY + 1);
  return result;
}
