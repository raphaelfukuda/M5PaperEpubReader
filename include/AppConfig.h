#pragma once

#include <Arduino.h>

#ifndef M5EPUB_CANVAS_MEMORY_INTERNAL
#define M5EPUB_CANVAS_MEMORY_INTERNAL 0
#endif
#ifndef M5EPUB_CANVAS_MEMORY_PSRAM
#define M5EPUB_CANVAS_MEMORY_PSRAM 0
#endif
#ifndef M5EPUB_ENABLE_LAYOUT_WORKER
#define M5EPUB_ENABLE_LAYOUT_WORKER 0
#endif

namespace app_config {
constexpr uint32_t kSerialBaud = 115200;
// Verified on the original M5Paper: rotation 0 yields 540 x 960 portrait.
constexpr uint8_t kPortraitRotation = 0;
constexpr int8_t kSdChipSelect = 4;
constexpr int8_t kSharedSpiClock = 14;
constexpr int8_t kSharedSpiMiso = 13;
constexpr int8_t kSharedSpiMosi = 12;
constexpr uint32_t kSdFrequencyHz = 25000000;
constexpr uint32_t kIdleYieldIntervalMs = 2;
constexpr uint32_t kSleepAfterInactivityMs = 10UL * 60UL * 1000UL;
constexpr gpio_num_t kIncreaseFontButtonPin = GPIO_NUM_37;
constexpr gpio_num_t kDecreaseFontButtonPin = GPIO_NUM_39;
constexpr uint16_t kTouchMarkerRadius = 18;
constexpr uint16_t kTouchMarkerSize = kTouchMarkerRadius * 2 + 2;
constexpr uint16_t kBrowserHeaderHeight = 92;
constexpr uint16_t kBrowserFooterHeight = 54;
constexpr uint16_t kBrowserRowHeight = 72;
constexpr uint8_t kDirectoryBatchSize = 8;
constexpr size_t kMaxDirectoryEntries = 256;
constexpr int32_t kSwipeThreshold = 70;
constexpr uint32_t kTapMaxDurationMs = 650;
constexpr size_t kMaxContainerXmlBytes = 64 * 1024;
constexpr size_t kMaxOpfBytes = 512 * 1024;
constexpr size_t kMaxManifestItems = 2048;
constexpr size_t kMaxSpineItems = 4096;
constexpr size_t kMaxTocDocumentBytes = 512 * 1024;
constexpr size_t kMaxTocEntries = 1024;
constexpr uint16_t kReaderTopActionHeight = 110;
constexpr int32_t kBatteryIndicatorWidth = 104;
constexpr size_t kMaxVisitedPageTextBytes = 96 * 1024;
constexpr size_t kMaxPageHistoryEntries = 8192;
constexpr uint8_t kReadingPagesBeforeQualityRefresh = 8;
constexpr uint32_t kRapidPageTurnWindowMs = 900;
constexpr uint8_t kRapidTurnsBeforeFastest = 2;
constexpr uint8_t kMaxConsecutiveFastestRefreshes = 4;
constexpr uint8_t kMaxFastRefreshesBeforeCleanup = 8;
constexpr uint8_t kMaxReadingRefreshesBeforeQuality = 10;
constexpr uint32_t kMaxMillisecondsWithoutQualityRefresh = 10UL * 60UL * 1000UL;
constexpr size_t kReaderInputChunkSize = 1024;
constexpr size_t kReaderInputBufferChunks = 1;
constexpr uint32_t kCpuWorkBudgetPerTickUs = 2000;
constexpr size_t kPersistedPreviousPageAnchors = 32;
constexpr float kPartialRefreshMaximumRegionRatio = 0.85f;
constexpr int32_t kDirtyRegionExpansionPixels = 4;
constexpr int32_t kDisplayRegionAlignmentPixels = 4;
constexpr uint32_t kReadingStateIdleSaveDelayMs = 15000;
constexpr uint32_t kReadingStateMaxSaveIntervalMs = 60000;
constexpr uint8_t kReadingStatePageSaveThreshold = 5;
constexpr size_t kInternalHeapSafetyMargin = 64 * 1024;
}  // namespace app_config
