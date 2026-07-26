#pragma once

#include <stdint.h>

struct BootMetrics {
  uint32_t totalMs = 0;
  uint32_t sdMountMs = 0;
};

struct PageTurnMetrics {
  uint32_t sdReadMs = 0, inflateMs = 0, parseMs = 0, layoutMs = 0;
  uint32_t renderMs = 0, displaySubmitMs = 0, totalMs = 0;
};

struct ReaderRuntimeMetrics {
  uint32_t lastBookOpenMs = 0, lastPageTurnMs = 0;
  uint32_t completedBookOpens = 0, completedPageTurns = 0;
};

struct DisplayRefreshMetrics {
  uint32_t qualityRefreshes = 0, textRefreshes = 0, fastRefreshes = 0;
  uint32_t periodicQualityRefreshes = 0;
  uint8_t readingPagesSinceQuality = 0;
};
