#pragma once

#include <Arduino.h>

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
constexpr size_t kMaxVisitedPageTextBytes = 96 * 1024;
constexpr size_t kMaxPageHistoryEntries = 8192;
constexpr uint8_t kReadingPagesBeforeQualityRefresh = 8;
}  // namespace app_config
