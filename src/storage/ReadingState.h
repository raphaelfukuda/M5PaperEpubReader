#pragma once

#include <stdint.h>
#include <string>
#include <vector>

struct ReadingHistoryEntry {
  uint32_t spineIndex = 0;
  uint64_t textOffset = 0;
  uint32_t parserCheckpoint = 0;
};

struct ReadingState {
  static constexpr uint32_t kCurrentVersion = 3;

  uint32_t version = kCurrentVersion;
  std::string bookPath;
  uint32_t spineIndex = 0;
  uint64_t textOffset = 0;
  uint32_t parserCheckpoint = 0;
  uint16_t fontSize = 24;
  uint8_t fontFamily = 2;
  uint16_t lineSpacing = 0;
  uint16_t horizontalMargin = 24;
  uint32_t pageNumber = 1;
  std::vector<ReadingHistoryEntry> previousPages;
};
