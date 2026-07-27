#pragma once

#include <cstddef>
#include <cstdint>
#include "DisplayRegion.h"

struct DirtyRegionResult {
  DisplayRegion bounds;
  uint32_t changedPixels = 0;
  uint32_t comparedPixels = 0;
  float changedRatio = 0.0f;
  bool valid = false;
};

DirtyRegionResult compareMonoBuffers(const uint8_t* previous, const uint8_t* next,
                                     int32_t width, int32_t height,
                                     size_t strideBytes);
