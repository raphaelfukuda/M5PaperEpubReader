#pragma once

#include <cstdint>
#include "DisplayRegion.h"

enum class ReaderDirtyFlags : uint8_t {
  None = 0,
  Body = 1 << 0,
  Header = 1 << 1,
  Footer = 1 << 2,
  Battery = 1 << 3,
  All = 0x0F
};

inline ReaderDirtyFlags operator|(ReaderDirtyFlags left, ReaderDirtyFlags right) {
  return static_cast<ReaderDirtyFlags>(static_cast<uint8_t>(left) |
                                       static_cast<uint8_t>(right));
}

inline bool hasDirtyFlag(ReaderDirtyFlags flags, ReaderDirtyFlags flag) {
  return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
}

inline ReaderDirtyFlags classifyReaderDirtyRegion(const DisplayRegion& dirty,
                                                   int32_t screenWidth,
                                                   int32_t screenHeight) {
  if (dirty.empty() || screenWidth <= 0 || screenHeight <= 0)
    return ReaderDirtyFlags::None;
  ReaderDirtyFlags flags = ReaderDirtyFlags::None;
  const DisplayRegion header{0, 0, screenWidth, 70};
  const DisplayRegion body{0, 70, screenWidth, screenHeight - 125};
  const DisplayRegion footer{0, screenHeight - 55, screenWidth, 55};
  const DisplayRegion battery{screenWidth - 122, 8, 104, 34};
  if (!DisplayRegion::intersect(dirty, header).empty()) flags = flags | ReaderDirtyFlags::Header;
  if (!DisplayRegion::intersect(dirty, body).empty()) flags = flags | ReaderDirtyFlags::Body;
  if (!DisplayRegion::intersect(dirty, footer).empty()) flags = flags | ReaderDirtyFlags::Footer;
  if (!DisplayRegion::intersect(dirty, battery).empty()) flags = flags | ReaderDirtyFlags::Battery;
  return flags;
}
