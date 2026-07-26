#pragma once

#include <stdint.h>
#include <string>

enum class BasicTextAlign { Unspecified, Left, Center, Right, Justify };

struct BasicCssStyle {
  BasicTextAlign textAlign = BasicTextAlign::Unspecified;
  bool bold = false;
  bool italic = false;
  bool hasTextIndent = false;
  int16_t textIndentPx = 0;
  bool hasLineHeightPercent = false;
  uint16_t lineHeightPercent = 100;
};

namespace basic_css {
BasicCssStyle parseDeclarations(const std::string& declarations);
}  // namespace basic_css
