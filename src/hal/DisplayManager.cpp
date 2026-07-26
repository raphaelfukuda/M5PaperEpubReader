#include "DisplayManager.h"

#include "AppConfig.h"

bool DisplayManager::begin() {
  M5.Display.setRotation(app_config::kPortraitRotation);
  canvas_.setColorDepth(1);
  canvas_.setPsram(true);
  canvasReady_ = canvas_.createSprite(M5.Display.width(), M5.Display.height()) != nullptr;
  return canvasReady_;
}

void DisplayManager::setRefreshIntent(RefreshIntent intent) {
  switch (intent) {
    case RefreshIntent::FullQuality: M5.Display.setEpdMode(epd_mode_t::epd_quality); break;
    case RefreshIntent::ReadingPage: M5.Display.setEpdMode(epd_mode_t::epd_text); break;
    case RefreshIntent::InteractiveFeedback:
    case RefreshIntent::SmallRegion: M5.Display.setEpdMode(epd_mode_t::epd_fast); break;
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
  setRefreshIntent(RefreshIntent::SmallRegion);
  M5.Display.fillCircle(x, y, r, TFT_BLACK);
  M5.Display.display(left, top, right - left + 1, bottom - top + 1);
  ++refreshMetrics_.fastRefreshes;
}

void DisplayManager::waitUntilIdle() {
  ScopedSpiBus bus(busGuard_, SpiBusOwner::Display);
  if (bus) M5.Display.waitDisplay();
}

bool DisplayManager::submitFull(RefreshIntent intent) {
  if (!canvasReady_) return false;
  ScopedSpiBus bus(busGuard_, SpiBusOwner::Display);
  if (!bus) return false;
  M5.Display.waitDisplay();
  RefreshIntent effectiveIntent = intent;
  if (intent == RefreshIntent::ReadingPage &&
      refreshMetrics_.readingPagesSinceQuality + 1 >=
          app_config::kReadingPagesBeforeQualityRefresh) {
    effectiveIntent = RefreshIntent::FullQuality;
    ++refreshMetrics_.periodicQualityRefreshes;
  }
  setRefreshIntent(effectiveIntent);
  canvas_.pushSprite(0, 0);
  M5.Display.display();
  if (effectiveIntent == RefreshIntent::FullQuality) {
    ++refreshMetrics_.qualityRefreshes;
    refreshMetrics_.readingPagesSinceQuality = 0;
  } else if (effectiveIntent == RefreshIntent::ReadingPage) {
    ++refreshMetrics_.textRefreshes;
    ++refreshMetrics_.readingPagesSinceQuality;
  } else {
    ++refreshMetrics_.fastRefreshes;
  }
  return true;
}

bool DisplayManager::highlightRegion(int32_t x, int32_t y, int32_t w, int32_t h) {
  if (w <= 0 || h <= 0) return false;
  ScopedSpiBus bus(busGuard_, SpiBusOwner::Display);
  if (!bus) return false;
  M5.Display.waitDisplay();
  setRefreshIntent(RefreshIntent::InteractiveFeedback);
  M5.Display.drawRect(x + 2, y + 2, w - 4, h - 4, TFT_BLACK);
  M5.Display.display(x, y, w, h);
  ++refreshMetrics_.fastRefreshes;
  return true;
}
