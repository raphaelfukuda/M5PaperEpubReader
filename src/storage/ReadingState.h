#pragma once

#include <stdint.h>
#include <string>

struct ReadingState {
  static constexpr uint32_t kCurrentVersion = 1;

  uint32_t version = kCurrentVersion;
  std::string bookPath;
  uint32_t spineIndex = 0;
  uint64_t textOffset = 0;
  uint32_t parserCheckpoint = 0;
  uint16_t fontSize = 24;
  uint16_t lineSpacing = 0;
  uint16_t horizontalMargin = 24;
};
