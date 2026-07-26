#pragma once
#include <M5Unified.h>
#include <string>
#include "LayoutStyle.h"
#include "PageModel.h"
#include "TextStyle.h"

class TextLayoutEngine {
 public:
  void begin(M5Canvas& canvas, const ReaderSettings& settings);
  size_t processText(const std::string& text);
  bool finish();
  bool pageFull() const { return full_; }
 private:
  bool flushWord();
  bool newLine(bool paragraph = false);
  void applyStyle(uint8_t code);
  void updateFontMetrics();
  M5Canvas* canvas_ = nullptr;
  ReaderSettings settings_;
  std::string word_;
  int32_t x_ = 0, y_ = 0, lineHeight_ = 24;
  bool pendingSpace_ = false, full_ = false;
  TextStyle style_;
  uint16_t baseFontSize_ = 16;
};
