#pragma once

#include <M5Unified.h>
#include <stdint.h>

namespace reader_font {

inline const lgfx::IFont* forSize(uint16_t size) {
  if (size >= 24) return &fonts::efontCN_24;
  return &fonts::efontCN_16;
}

inline float scaleForSize(uint16_t size) {
  // M5GFX 0.2.26 has efontCN at 16 and 24 px only. Scale the 24 px
  // bitmap for the 32, 36, and 40 px accessibility levels.
  return size >= 24 ? static_cast<float>(size) / 24.0f : 1.0f;
}

}  // namespace reader_font
