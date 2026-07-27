#pragma once

#include <M5Unified.h>
#include "diagnostics/PerformanceMetrics.h"
#include "refresh/RefreshPolicy.h"
#include "display/DirtyRegion.h"
#include "display/ReaderDirtyFlags.h"
#include "SpiBusGuard.h"

enum class RefreshIntent {
  FullQuality, ManualCleanup, ReadingPage, InteractiveFeedback, SmallRegion
};

class DisplayManager {
 public:
  explicit DisplayManager(SpiBusGuard& busGuard) : busGuard_(busGuard) {}
  bool begin();
  void markTouch(int32_t x, int32_t y);
  void waitUntilIdle();
  M5Canvas& canvas() { return canvas_; }
  bool submitFull(RefreshIntent intent);
  bool submitCanvas(M5Canvas& source, RefreshIntent intent);
  void beginPageTurnMetric(PageTurnKind kind, uint32_t eventTimestampUs,
                           uint32_t eventToHandlingUs = 0);
  void markPageReady(uint32_t timestampUs);
  void pollPanelTiming(uint32_t cpuWorkDuringBusyUs = 0);
  bool highlightRegion(int32_t x, int32_t y, int32_t width, int32_t height);
  int32_t width() const { return M5.Display.width(); }
  int32_t height() const { return M5.Display.height(); }
  const DisplayRefreshMetrics& refreshMetrics() const { return refreshMetrics_; }
  const DisplayTimingMetrics& lastDisplayTiming() const { return lastTiming_; }

 private:
  void drawBatteryIndicator(M5Canvas& target);
  void setRefreshProfile(RefreshProfile profile);
  SpiBusGuard& busGuard_;
  M5Canvas canvas_{&M5.Display};
  bool canvasReady_ = false;
  DisplayRefreshMetrics refreshMetrics_;
  RefreshPolicy refreshPolicy_;
  DisplayTimingMetrics activeTiming_;
  DisplayTimingMetrics lastTiming_;
  const char* activeModeName_ = "unknown";
  const char* activeRegionName_ = "full";
  bool timingActive_ = false;
  bool waitingForPanelIdle_ = false;
  uint32_t displayCommandCompletedUs_ = 0;
  uint32_t lastReadingSubmitMs_ = 0;
  uint8_t consecutiveRapidTurns_ = 0;
  M5Canvas* lastSubmittedSource_ = nullptr;
  ReaderDirtyFlags activeDirtyFlags_ = ReaderDirtyFlags::All;
};
