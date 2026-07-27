#pragma once

#include <M5Unified.h>
#include <stdint.h>
#include "LayoutStyle.h"

namespace reader_font {

inline const lgfx::IFont* forSize(uint16_t size,
                                  ReaderFontFamily family = ReaderFontFamily::Book) {
  if (family == ReaderFontFamily::Sans) {
    if (size >= 36) return &fonts::FreeSans18pt7b;
    if (size >= 24) return &fonts::FreeSans12pt7b;
    return &fonts::FreeSans9pt7b;
  }
  if (family == ReaderFontFamily::Compact) {
    if (size >= 24) return &fonts::efontCN_24;
    return &fonts::efontCN_16;
  }
  if (size >= 36) return &fonts::FreeSerif18pt7b;
  if (size >= 24) return &fonts::FreeSerif12pt7b;
  return &fonts::FreeSerif9pt7b;
}

inline float scaleForSize(uint16_t size,
                          ReaderFontFamily family = ReaderFontFamily::Book) {
  // M5GFX 0.2.26 has efontCN at 16 and 24 px only. Scale the 24 px
  // bitmap for the 32, 36, and 40 px accessibility levels.
  if (family == ReaderFontFamily::Compact)
    return size >= 24 ? static_cast<float>(size) / 24.0f : 1.0f;
  if (size >= 36) return static_cast<float>(size) / 36.0f;
  if (size >= 24) return static_cast<float>(size) / 24.0f;
  return static_cast<float>(size) / 18.0f;
}

inline const char* familyName(ReaderFontFamily family) {
  switch (family) {
    case ReaderFontFamily::Book: return "Book";
    case ReaderFontFamily::Sans: return "Sans";
    case ReaderFontFamily::Compact: return "Compact";
  }
  return "Book";
}

}  // namespace reader_font
