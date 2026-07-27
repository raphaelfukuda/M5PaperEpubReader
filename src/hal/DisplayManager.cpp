#include "DisplayManager.h"

#include "AppConfig.h"
#include "display/CanvasMemoryPolicy.h"
#include <esp_heap_caps.h>

namespace {
uint32_t elapsedUs(uint32_t since, uint32_t now) { return now - since; }
}

void DisplayManager::beginPageTurnMetric(PageTurnKind kind,
                                         uint32_t eventTimestampUs,
                                         uint32_t eventToHandlingUs) {
  activeTiming_ = {};
  activeTiming_.pageTurnKind = kind;
  activeTiming_.eventTimestampUs = eventTimestampUs;
  activeTiming_.eventToHandlingUs = eventToHandlingUs;
  activeTiming_.totalPixels = static_cast<uint32_t>(width() * height());
  timingActive_ = true;
}

void DisplayManager::markPageReady(uint32_t timestampUs) {
  if (!timingActive_) return;
  activeTiming_.eventToPageReadyUs =
      elapsedUs(activeTiming_.eventTimestampUs, timestampUs);
}

void DisplayManager::pollPanelTiming(uint32_t cpuWorkDuringBusyUs) {
  if (!waitingForPanelIdle_) return;
  activeTiming_.cpuWorkDuringBusyUs += cpuWorkDuringBusyUs;
  if (M5.Display.displayBusy()) return;
  const uint32_t now = micros();
  activeTiming_.panelBusyUs = elapsedUs(displayCommandCompletedUs_, now);
  activeTiming_.eventToIdleUs = elapsedUs(activeTiming_.eventTimestampUs, now);
  lastTiming_ = activeTiming_;
  Serial.printf(
      "M5EPUB_METRIC,type=page_turn,kind=%s,mode=%s,region=%s,event_to_handling_us=%lu,event_to_page_ready_us=%lu,canvas_render_us=%lu,display_wait_us=%lu,sprite_upload_us=%lu,display_command_us=%lu,panel_busy_us=%lu,cpu_work_during_busy_us=%lu,event_to_submit_us=%lu,event_to_idle_us=%lu,updated_pixels=%lu,total_pixels=%lu,total_us=%lu\n",
      pageTurnKindName(lastTiming_.pageTurnKind), activeModeName_, activeRegionName_,
      static_cast<unsigned long>(lastTiming_.eventToHandlingUs),
      static_cast<unsigned long>(lastTiming_.eventToPageReadyUs),
      static_cast<unsigned long>(lastTiming_.canvasRenderUs),
      static_cast<unsigned long>(lastTiming_.displayWaitUs),
      static_cast<unsigned long>(lastTiming_.spriteUploadUs),
      static_cast<unsigned long>(lastTiming_.displayCommandUs),
      static_cast<unsigned long>(lastTiming_.panelBusyUs),
      static_cast<unsigned long>(lastTiming_.cpuWorkDuringBusyUs),
      static_cast<unsigned long>(lastTiming_.eventToSubmitUs),
      static_cast<unsigned long>(lastTiming_.eventToIdleUs),
      static_cast<unsigned long>(lastTiming_.updatedPixels),
      static_cast<unsigned long>(lastTiming_.totalPixels),
      static_cast<unsigned long>(lastTiming_.eventToIdleUs));
  waitingForPanelIdle_ = false;
  timingActive_ = false;
}

void DisplayManager::drawBatteryIndicator(M5Canvas& target) {
  const int32_t level = M5.Power.getBatteryLevel();
  if (level < 0) return;
  const int32_t right = width() - 18;
  const int32_t top = 14;
  const int32_t iconWidth = 28;
  const int32_t iconHeight = 14;
  const int32_t iconLeft = right - iconWidth;
  target.fillRect(right - app_config::kBatteryIndicatorWidth, 8,
                   app_config::kBatteryIndicatorWidth, 34, TFT_WHITE);
  target.drawRect(iconLeft, top, iconWidth - 3, iconHeight, TFT_BLACK);
  target.fillRect(right - 3, top + 4, 3, 6, TFT_BLACK);
  const int32_t fill = ((iconWidth - 7) * min<int32_t>(100, level)) / 100;
  if (fill > 0) target.fillRect(iconLeft + 2, top + 2, fill, iconHeight - 4, TFT_BLACK);
  char label[8];
  snprintf(label, sizeof(label), "%ld%%", static_cast<long>(level));
  target.setFont(&fonts::Font0);
  target.setTextSize(1);
  target.setTextColor(TFT_BLACK, TFT_WHITE);
  target.setTextDatum(top_right);
  target.drawString(label, iconLeft - 5, top + 1);
}

bool DisplayManager::begin() {
  M5.Display.setRotation(app_config::kPortraitRotation);
  canvas_.setColorDepth(1);
  const size_t requested = static_cast<size_t>((M5.Display.width() + 7) / 8) *
                           static_cast<size_t>(M5.Display.height());
  const CanvasMemoryStats memory{heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                                 ESP.getFreePsram()};
#if M5EPUB_CANVAS_MEMORY_INTERNAL
  constexpr CanvasMemoryPreference preference = CanvasMemoryPreference::InternalRam;
#elif M5EPUB_CANVAS_MEMORY_PSRAM
  constexpr CanvasMemoryPreference preference = CanvasMemoryPreference::Psram;
#else
  constexpr CanvasMemoryPreference preference = CanvasMemoryPreference::Auto;
#endif
  const CanvasMemoryKind selected = chooseCanvasMemory(
      preference, requested, app_config::kInternalHeapSafetyMargin, memory);
  canvas_.setPsram(selected == CanvasMemoryKind::Psram);
  canvasReady_ = canvas_.createSprite(M5.Display.width(), M5.Display.height()) != nullptr;
  Serial.printf(
      "M5EPUB_MEMORY,canvas=front,requested=%u,selected=%s,success=%u,internal_free=%u,internal_largest=%u,psram_free=%u\n",
      static_cast<unsigned>(requested), canvasMemoryKindName(selected),
      canvasReady_ ? 1U : 0U, static_cast<unsigned>(memory.internalFree),
      static_cast<unsigned>(memory.internalLargestBlock),
      static_cast<unsigned>(memory.psramFree));
  return canvasReady_;
}

void DisplayManager::setRefreshProfile(RefreshProfile profile) {
  switch (profile) {
    case RefreshProfile::Quality: M5.Display.setEpdMode(epd_mode_t::epd_quality); break;
    case RefreshProfile::Text: M5.Display.setEpdMode(epd_mode_t::epd_text); break;
    case RefreshProfile::Fast: M5.Display.setEpdMode(epd_mode_t::epd_fast); break;
    case RefreshProfile::Fastest: M5.Display.setEpdMode(epd_mode_t::epd_fastest); break;
    case RefreshProfile::Adaptive: M5.Display.setEpdMode(epd_mode_t::epd_text); break;
  }
}

void DisplayManager::markTouch(int32_t x, int32_t y) {
  if (!canvasReady_) return;
  const int32_t r = app_config::kTouchMarkerRadius;
  const int32_t left = max<int32_t>(0, x - r);
  const int32_t top = max<int32_t>(0, y - r);
  const int32_t right = min<int32_t>(width() - 1, x + r);
  const int32_t bottom = min<int32_t>(height() - 1, y + r);
  canvas_.fillCircle(x, y, r, TFT_BLACK);
  ScopedSpiBus bus(busGuard_, SpiBusOwner::Display);
  if (!bus) return;
  M5.Display.waitDisplay();
  setRefreshProfile(RefreshProfile::Fast);
  M5.Display.fillCircle(x, y, r, TFT_BLACK);
  M5.Display.display(left, top, right - left + 1, bottom - top + 1);
  ++refreshMetrics_.fastRefreshes;
}

void DisplayManager::waitUntilIdle() {
  ScopedSpiBus bus(busGuard_, SpiBusOwner::Display);
  if (bus) M5.Display.waitDisplay();
}

bool DisplayManager::submitFull(RefreshIntent intent) {
  return submitCanvas(canvas_, intent);
}

bool DisplayManager::submitCanvas(M5Canvas& source, RefreshIntent intent) {
  if (!canvasReady_) return false;
  drawBatteryIndicator(source);
  DisplayRegion selectedRegion;
  DirtyRegionResult dirty;
  uint32_t diffScanUs = 0;
  if (intent == RefreshIntent::ReadingPage && lastSubmittedSource_ &&
      lastSubmittedSource_ != &source &&
      lastSubmittedSource_->width() == source.width() &&
      lastSubmittedSource_->height() == source.height() &&
      static_cast<uint8_t>(lastSubmittedSource_->getColorDepth()) == 1 &&
      static_cast<uint8_t>(source.getColorDepth()) == 1) {
    const uint32_t diffStarted = micros();
    const size_t stride = static_cast<size_t>((source.width() + 7) / 8);
    dirty = compareMonoBuffers(
        static_cast<const uint8_t*>(lastSubmittedSource_->getBuffer()),
        static_cast<const uint8_t*>(source.getBuffer()), source.width(),
        source.height(), stride);
    diffScanUs = elapsedUs(diffStarted, micros());
    if (dirty.valid && !dirty.bounds.empty()) {
      selectedRegion = dirty.bounds.expanded(app_config::kDirtyRegionExpansionPixels)
                           .aligned(app_config::kDisplayRegionAlignmentPixels,
                                    width(), height());
      if (selectedRegion.ratio(static_cast<uint32_t>(width() * height())) >
          app_config::kPartialRefreshMaximumRegionRatio)
        selectedRegion = {};
    }
  }
  ScopedSpiBus bus(busGuard_, SpiBusOwner::Display);
  if (!bus) return false;
  uint32_t started = micros();
  M5.Display.waitDisplay();
  if (timingActive_) activeTiming_.displayWaitUs = elapsedUs(started, micros());
  const uint32_t nowMs = millis();
  RefreshRequest request;
  request.fullScreen = selectedRegion.empty();
  request.changedAreaRatio = dirty.valid ? dirty.changedRatio : 1.0f;
  request.nowMs = nowMs;
  if (intent == RefreshIntent::ManualCleanup) {
    request.reason = RefreshReason::ManualCleanup;
  } else if (intent == RefreshIntent::FullQuality) {
    request.reason = RefreshReason::Boot;
  } else if (intent == RefreshIntent::InteractiveFeedback ||
             intent == RefreshIntent::SmallRegion) {
    request.reason = RefreshReason::MenuOpen;
  } else {
    const uint32_t sincePrevious = nowMs - lastReadingSubmitMs_;
    if (lastReadingSubmitMs_ != 0 &&
        sincePrevious <= app_config::kRapidPageTurnWindowMs) {
      if (consecutiveRapidTurns_ < UINT8_MAX) ++consecutiveRapidTurns_;
    } else {
      consecutiveRapidTurns_ = 1;
    }
    request.millisecondsSincePreviousTurn = sincePrevious;
    request.consecutiveRapidTurns = consecutiveRapidTurns_;
    request.reason = consecutiveRapidTurns_ >= app_config::kRapidTurnsBeforeFastest
                         ? RefreshReason::RapidPageTurn
                         : RefreshReason::NormalPageTurn;
    if (timingActive_ && activeTiming_.pageTurnKind == PageTurnKind::CachedPrevious)
      request.reason = RefreshReason::PreviousPage;
    else if (timingActive_ && activeTiming_.pageTurnKind == PageTurnKind::FontReflow)
      request.reason = RefreshReason::FontReflow;
    else if (timingActive_ && activeTiming_.pageTurnKind == PageTurnKind::TocNavigation)
      request.reason = RefreshReason::TocNavigation;
    lastReadingSubmitMs_ = nowMs;
  }
  const RefreshDecision decision = refreshPolicy_.decide(request);
  if (decision.effective == RefreshProfile::Quality) selectedRegion = {};
  activeDirtyFlags_ = selectedRegion.empty()
                          ? ReaderDirtyFlags::All
                          : classifyReaderDirtyRegion(selectedRegion, width(), height());
  setRefreshProfile(decision.effective);
  activeModeName_ = refreshProfileName(decision.effective);
  activeRegionName_ = selectedRegion.empty() ? "full" : "partial";
  Serial.printf(
      "M5EPUB_REFRESH,requested=%s,effective=%s,reason=%s,cleanup=%s,fast_count=%u,fastest_count=%u,reading_count=%u\n",
      refreshProfileName(decision.requested), refreshProfileName(decision.effective),
      refreshReasonName(decision.reason), decision.cleanupCause,
      refreshPolicy_.budget().fastRefreshes,
      refreshPolicy_.budget().fastestRefreshes,
      refreshPolicy_.budget().readingRefreshes);
  // Give the Arduino/ESP-IDF scheduler a safe point after changing the IT8951
  // waveform and before the long framebuffer transfer. This avoids an
  // intermittent first-refresh stall observed after RTS reset without adding a
  // blocking delay.
  yield();
  started = micros();
  if (selectedRegion.empty()) {
    source.pushSprite(0, 0);
  } else {
    M5.Display.setClipRect(selectedRegion.x, selectedRegion.y,
                           selectedRegion.width, selectedRegion.height);
    source.pushSprite(0, 0);
    M5.Display.clearClipRect();
  }
  if (timingActive_) activeTiming_.spriteUploadUs = elapsedUs(started, micros());
  started = micros();
  if (selectedRegion.empty())
    M5.Display.display();
  else
    M5.Display.display(selectedRegion.x, selectedRegion.y,
                       selectedRegion.width, selectedRegion.height);
  displayCommandCompletedUs_ = micros();
  if (timingActive_) {
    activeTiming_.displayCommandUs = elapsedUs(started, displayCommandCompletedUs_);
    activeTiming_.eventToSubmitUs = elapsedUs(activeTiming_.eventTimestampUs,
                                              displayCommandCompletedUs_);
    if (activeTiming_.eventToPageReadyUs == 0)
      activeTiming_.eventToPageReadyUs = activeTiming_.eventToSubmitUs;
    activeTiming_.updatedPixels = selectedRegion.empty()
                                      ? activeTiming_.totalPixels
                                      : selectedRegion.area();
    waitingForPanelIdle_ = true;
  }
  if (dirty.valid) {
    const uint32_t selectedPixels = selectedRegion.empty()
                                        ? static_cast<uint32_t>(width() * height())
                                        : selectedRegion.area();
    Serial.printf(
        "M5EPUB_DIRTY,diff_scan_us=%lu,changed_pixels=%lu,changed_ratio=%.6f,selected_pixels=%lu,selected_ratio=%.6f,region=%s,dirty_flags=%u\n",
        static_cast<unsigned long>(diffScanUs),
        static_cast<unsigned long>(dirty.changedPixels), dirty.changedRatio,
        static_cast<unsigned long>(selectedPixels),
        static_cast<double>(selectedPixels) / (width() * height()),
        selectedRegion.empty() ? "full" : "partial",
        static_cast<unsigned>(activeDirtyFlags_));
  }
  lastSubmittedSource_ = &source;
  refreshPolicy_.recordSubmitted(decision);
  if (decision.effective == RefreshProfile::Quality) {
    ++refreshMetrics_.qualityRefreshes;
    refreshMetrics_.readingPagesSinceQuality = 0;
    if (decision.cleanupForced) ++refreshMetrics_.periodicQualityRefreshes;
  } else if (decision.effective == RefreshProfile::Text) {
    ++refreshMetrics_.textRefreshes;
    ++refreshMetrics_.readingPagesSinceQuality;
  } else if (decision.effective == RefreshProfile::Fastest) {
    ++refreshMetrics_.fastestRefreshes;
    ++refreshMetrics_.readingPagesSinceQuality;
  } else if (decision.effective == RefreshProfile::Fast) {
    ++refreshMetrics_.fastRefreshes;
    ++refreshMetrics_.readingPagesSinceQuality;
  }
  return true;
}

bool DisplayManager::highlightRegion(int32_t x, int32_t y, int32_t w, int32_t h) {
  if (w <= 0 || h <= 0) return false;
  ScopedSpiBus bus(busGuard_, SpiBusOwner::Display);
  if (!bus) return false;
  M5.Display.waitDisplay();
  setRefreshProfile(RefreshProfile::Fast);
  M5.Display.drawRect(x + 2, y + 2, w - 4, h - 4, TFT_BLACK);
  M5.Display.display(x, y, w, h);
  ++refreshMetrics_.fastRefreshes;
  return true;
}
