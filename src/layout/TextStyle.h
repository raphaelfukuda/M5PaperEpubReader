#pragma once

#include <stdint.h>

enum class TextAlignment : uint8_t { Left, Center, Right, Justify };

struct TextStyle {
  float fontScale = 1.0f;
  float lineSpacingScale = 1.0f;
  uint16_t firstLineIndent = 0;
  uint16_t blockIndent = 0;
  TextAlignment alignment = TextAlignment::Left;
  bool bold = false;
  bool italic = false;
};

namespace text_style_control {
constexpr char kEscape = 0x1D;
enum Code : uint8_t { Paragraph = 1, Heading1, Heading2, Heading3, Heading4,
                      Heading5, Heading6, Blockquote, PlainBlock,
                      BoldOn, BoldOff, ItalicOn, ItalicOff };
}
