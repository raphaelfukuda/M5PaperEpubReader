#pragma once

#include <cstddef>
#include <cstdint>

enum class CanvasMemoryPreference : uint8_t { Auto, InternalRam, Psram };
enum class CanvasMemoryKind : uint8_t { InternalRam, Psram };

struct CanvasMemoryStats {
  CanvasMemoryStats() = default;
  CanvasMemoryStats(size_t freeBytes, size_t largestBlock, size_t freePsram)
      : internalFree(freeBytes), internalLargestBlock(largestBlock),
        psramFree(freePsram) {}
  size_t internalFree = 0;
  size_t internalLargestBlock = 0;
  size_t psramFree = 0;
};

inline CanvasMemoryKind chooseCanvasMemory(CanvasMemoryPreference preference,
                                           size_t requestedBytes,
                                           size_t safetyMargin,
                                           const CanvasMemoryStats& stats) {
  if (preference == CanvasMemoryPreference::Psram) return CanvasMemoryKind::Psram;
  const bool internalSafe = stats.internalFree >= requestedBytes + safetyMargin &&
                            stats.internalLargestBlock >= requestedBytes;
  if (preference == CanvasMemoryPreference::InternalRam)
    return internalSafe ? CanvasMemoryKind::InternalRam : CanvasMemoryKind::Psram;
  // Auto remains conservative until an internal-RAM canvas passes prolonged
  // physical IT8951 testing. The explicit InternalRam override still checks
  // contiguous space and the safety margin before attempting it.
  return CanvasMemoryKind::Psram;
}

inline const char* canvasMemoryKindName(CanvasMemoryKind kind) {
  return kind == CanvasMemoryKind::InternalRam ? "internal" : "psram";
}
