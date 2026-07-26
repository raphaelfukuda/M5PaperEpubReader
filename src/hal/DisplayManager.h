#pragma once

#include <M5Unified.h>
#include "diagnostics/PerformanceMetrics.h"
#include "SpiBusGuard.h"

enum class RefreshIntent { FullQuality, ReadingPage, InteractiveFeedback, SmallRegion };

class DisplayManager {
 public:
  explicit DisplayManager(SpiBusGuard& busGuard) : busGuard_(busGuard) {}
  bool begin();
  void markTouch(int32_t x, int32_t y);
  void waitUntilIdle();
  M5Canvas& canvas() { return canvas_; }
  bool submitFull(RefreshIntent intent);
  bool highlightRegion(int32_t x, int32_t y, int32_t width, int32_t height);
  int32_t width() const { return M5.Display.width(); }
  int32_t height() const { return M5.Display.height(); }
  const DisplayRefreshMetrics& refreshMetrics() const { return refreshMetrics_; }

 private:
  void drawBatteryIndicator();
  void setRefreshIntent(RefreshIntent intent);
  SpiBusGuard& busGuard_;
  M5Canvas canvas_{&M5.Display};
  bool canvasReady_ = false;
  DisplayRefreshMetrics refreshMetrics_;
};
