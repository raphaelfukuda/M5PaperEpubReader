#include "DisplayRegion.h"

#include <algorithm>
#include <limits>

namespace {
int64_t rightEdge(const DisplayRegion& region) {
  return static_cast<int64_t>(region.x) + region.width;
}
int64_t bottomEdge(const DisplayRegion& region) {
  return static_cast<int64_t>(region.y) + region.height;
}
int32_t safeInt32(int64_t value) {
  if (value < std::numeric_limits<int32_t>::min()) return std::numeric_limits<int32_t>::min();
  if (value > std::numeric_limits<int32_t>::max()) return std::numeric_limits<int32_t>::max();
  return static_cast<int32_t>(value);
}
}

uint32_t DisplayRegion::area() const {
  if (empty()) return 0;
  const uint64_t value = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
  return value > std::numeric_limits<uint32_t>::max()
             ? std::numeric_limits<uint32_t>::max()
             : static_cast<uint32_t>(value);
}

float DisplayRegion::ratio(uint32_t totalPixels) const {
  return totalPixels == 0 ? 0.0f : static_cast<float>(area()) / totalPixels;
}

DisplayRegion DisplayRegion::clamped(int32_t displayWidth, int32_t displayHeight) const {
  if (empty() || displayWidth <= 0 || displayHeight <= 0) return {};
  const int64_t left = std::max<int64_t>(0, x);
  const int64_t top = std::max<int64_t>(0, y);
  const int64_t right = std::min<int64_t>(displayWidth, rightEdge(*this));
  const int64_t bottom = std::min<int64_t>(displayHeight, bottomEdge(*this));
  if (right <= left || bottom <= top) return {};
  return DisplayRegion(static_cast<int32_t>(left), static_cast<int32_t>(top),
                       static_cast<int32_t>(right - left),
                       static_cast<int32_t>(bottom - top));
}

DisplayRegion DisplayRegion::expanded(int32_t pixels) const {
  if (empty() || pixels <= 0) return *this;
  return DisplayRegion(safeInt32(static_cast<int64_t>(x) - pixels),
                       safeInt32(static_cast<int64_t>(y) - pixels),
                       safeInt32(static_cast<int64_t>(width) + pixels * 2LL),
                       safeInt32(static_cast<int64_t>(height) + pixels * 2LL));
}

DisplayRegion DisplayRegion::aligned(int32_t multiple, int32_t displayWidth,
                                     int32_t displayHeight) const {
  DisplayRegion result = clamped(displayWidth, displayHeight);
  if (result.empty() || multiple <= 1) return result;
  const int64_t left = (result.x / multiple) * multiple;
  const int64_t right = ((rightEdge(result) + multiple - 1) / multiple) * multiple;
  result.x = static_cast<int32_t>(left);
  result.width = safeInt32(right - left);
  return result.clamped(displayWidth, displayHeight);
}

DisplayRegion DisplayRegion::unite(const DisplayRegion& first,
                                   const DisplayRegion& second) {
  if (first.empty()) return second;
  if (second.empty()) return first;
  const int64_t left = std::min<int64_t>(first.x, second.x);
  const int64_t top = std::min<int64_t>(first.y, second.y);
  const int64_t right = std::max(rightEdge(first), rightEdge(second));
  const int64_t bottom = std::max(bottomEdge(first), bottomEdge(second));
  return DisplayRegion(safeInt32(left), safeInt32(top), safeInt32(right - left),
                       safeInt32(bottom - top));
}

DisplayRegion DisplayRegion::intersect(const DisplayRegion& first,
                                       const DisplayRegion& second) {
  if (first.empty() || second.empty()) return {};
  const int64_t left = std::max<int64_t>(first.x, second.x);
  const int64_t top = std::max<int64_t>(first.y, second.y);
  const int64_t right = std::min(rightEdge(first), rightEdge(second));
  const int64_t bottom = std::min(bottomEdge(first), bottomEdge(second));
  if (right <= left || bottom <= top) return {};
  return DisplayRegion(safeInt32(left), safeInt32(top), safeInt32(right - left),
                       safeInt32(bottom - top));
}
