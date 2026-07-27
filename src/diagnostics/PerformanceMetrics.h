#pragma once

#include <stdint.h>

enum class PageTurnKind : uint8_t {
  CachedNext,
  CachedPrevious,
  PrefetchedNext,
  GeneratedNext,
  RebuiltPrevious,
  ChapterTransition,
  FontReflow,
  TocNavigation,
  MenuReturn
};

inline const char* pageTurnKindName(PageTurnKind kind) {
  switch (kind) {
    case PageTurnKind::CachedNext: return "cached_next";
    case PageTurnKind::CachedPrevious: return "cached_previous";
    case PageTurnKind::PrefetchedNext: return "prefetched_next";
    case PageTurnKind::GeneratedNext: return "generated_next";
    case PageTurnKind::RebuiltPrevious: return "rebuilt_previous";
    case PageTurnKind::ChapterTransition: return "chapter_transition";
    case PageTurnKind::FontReflow: return "font_reflow";
    case PageTurnKind::TocNavigation: return "toc_navigation";
    case PageTurnKind::MenuReturn: return "menu_return";
  }
  return "generated_next";
}

struct DisplayTimingMetrics {
  uint32_t eventTimestampUs = 0;
  uint32_t eventToHandlingUs = 0;
  uint32_t eventToPageReadyUs = 0;
  uint32_t canvasRenderUs = 0;
  uint32_t displayWaitUs = 0;
  uint32_t spriteUploadUs = 0;
  uint32_t displayCommandUs = 0;
  uint32_t panelBusyUs = 0;
  uint32_t cpuWorkDuringBusyUs = 0;
  uint32_t eventToSubmitUs = 0;
  uint32_t eventToIdleUs = 0;
  uint32_t updatedPixels = 0;
  uint32_t totalPixels = 0;
  PageTurnKind pageTurnKind = PageTurnKind::GeneratedNext;
};

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
  uint32_t qualityRefreshes = 0, textRefreshes = 0, fastRefreshes = 0,
           fastestRefreshes = 0;
  uint32_t periodicQualityRefreshes = 0;
  uint8_t readingPagesSinceQuality = 0;
};
