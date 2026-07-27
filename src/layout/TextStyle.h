#pragma once

#include <stdint.h>
#include <string>

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
                      BoldOn, BoldOff, ItalicOn, ItalicOff, InlineImage };

inline void appendInlineImage(std::string& output, const std::string& source,
                              const std::string& alternative) {
  const size_t rawLength = source.size() + 1U + alternative.size();
  const uint16_t length = static_cast<uint16_t>(
      rawLength > 0xFFFFU ? 0xFFFFU : rawLength);
  output += kEscape;
  output += static_cast<char>(InlineImage);
  output += static_cast<char>(length & 0xFFU);
  output += static_cast<char>((length >> 8) & 0xFFU);
  const size_t sourceBytes = source.size() > length ? length : source.size();
  output.append(source, 0, sourceBytes);
  if (sourceBytes < length) {
    output += '\n';
    output.append(alternative, 0, length - sourceBytes - 1U);
  }
}

inline bool decodeInlineImage(const std::string& input, size_t offset,
                              std::string& source, std::string& alternative,
                              size_t& markerLength) {
  source.clear(); alternative.clear(); markerLength = 0;
  if (offset + 4U > input.size() || input[offset] != kEscape ||
      static_cast<uint8_t>(input[offset + 1]) != InlineImage) return false;
  const uint16_t payload = static_cast<uint8_t>(input[offset + 2]) |
                           (static_cast<uint16_t>(static_cast<uint8_t>(input[offset + 3])) << 8);
  markerLength = 4U + payload;
  if (offset + markerLength > input.size()) { markerLength = 0; return false; }
  const std::string data = input.substr(offset + 4U, payload);
  const size_t separator = data.find('\n');
  source = data.substr(0, separator);
  if (separator != std::string::npos) alternative = data.substr(separator + 1U);
  return !source.empty();
}
}
