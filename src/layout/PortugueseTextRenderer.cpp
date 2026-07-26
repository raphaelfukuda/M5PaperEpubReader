#include "PortugueseTextRenderer.h"

namespace {

enum class Accent : uint8_t { None, Acute, Grave, Circumflex, Tilde, Diaeresis, Cedilla };

struct Glyph {
  char base;
  Accent accent;
};

uint32_t nextCodepoint(const std::string& text, size_t& offset) {
  const uint8_t first = static_cast<uint8_t>(text[offset++]);
  if (first < 0x80) return first;
  if ((first & 0xE0) == 0xC0 && offset < text.size()) {
    return ((first & 0x1F) << 6) |
           (static_cast<uint8_t>(text[offset++]) & 0x3F);
  }
  if ((first & 0xF0) == 0xE0 && offset + 1 < text.size()) {
    const uint32_t cp = ((first & 0x0F) << 12) |
                        ((static_cast<uint8_t>(text[offset]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(text[offset + 1]) & 0x3F);
    offset += 2;
    return cp;
  }
  return '?';
}

Glyph glyphFor(uint32_t cp) {
  switch (cp) {
    // Common editorial punctuation. The compact reader font does not include
    // these Unicode glyphs, so use visually clear ASCII equivalents.
    case 0x00AB: return {'"', Accent::None};  // left guillemet
    case 0x00BB: return {'"', Accent::None};  // right guillemet
    case 0x2010: return {'-', Accent::None};
    case 0x2011: return {'-', Accent::None};
    case 0x2012: return {'-', Accent::None};
    case 0x2013: return {'-', Accent::None};  // en dash
    case 0x2014: return {'-', Accent::None};  // em dash
    case 0x2015: return {'-', Accent::None};
    case 0x2018: return {'\'', Accent::None};
    case 0x2019: return {'\'', Accent::None};
    case 0x201A: return {'\'', Accent::None};
    case 0x201B: return {'\'', Accent::None};
    case 0x201C: return {'"', Accent::None};
    case 0x201D: return {'"', Accent::None};
    case 0x201E: return {'"', Accent::None};
    case 0x2022: return {'*', Accent::None};  // list bullet
    case 0x2026: return {'.', Accent::None};  // ellipsis, compact fallback
    case 0x2032: return {'\'', Accent::None};
    case 0x2033: return {'"', Accent::None};
    case 0x2044: return {'/', Accent::None};
    case 0x2212: return {'-', Accent::None};
    case 0xFEFF: return {' ', Accent::None};  // byte-order mark
    case 0x2002: return {' ', Accent::None};
    case 0x2003: return {' ', Accent::None};
    case 0x2009: return {' ', Accent::None};
    case 0x200B: return {' ', Accent::None};
    case 0x00AD: return {'-', Accent::None};  // soft hyphen
    case 0x00A9: return {'c', Accent::None};
    case 0x00AE: return {'R', Accent::None};
    case 0x00B0: return {'o', Accent::None};
    case 0x00AA: return {'a', Accent::None};
    case 0x00BA: return {'o', Accent::None};
    case 0x00C0: return {'A', Accent::Grave};
    case 0x00C1: return {'A', Accent::Acute};
    case 0x00C2: return {'A', Accent::Circumflex};
    case 0x00C3: return {'A', Accent::Tilde};
    case 0x00C4: return {'A', Accent::Diaeresis};
    case 0x00C7: return {'C', Accent::Cedilla};
    case 0x00C8: return {'E', Accent::Grave};
    case 0x00C9: return {'E', Accent::Acute};
    case 0x00CA: return {'E', Accent::Circumflex};
    case 0x00CB: return {'E', Accent::Diaeresis};
    case 0x00CC: return {'I', Accent::Grave};
    case 0x00CD: return {'I', Accent::Acute};
    case 0x00CE: return {'I', Accent::Circumflex};
    case 0x00CF: return {'I', Accent::Diaeresis};
    case 0x00D2: return {'O', Accent::Grave};
    case 0x00D3: return {'O', Accent::Acute};
    case 0x00D4: return {'O', Accent::Circumflex};
    case 0x00D5: return {'O', Accent::Tilde};
    case 0x00D6: return {'O', Accent::Diaeresis};
    case 0x00D9: return {'U', Accent::Grave};
    case 0x00DA: return {'U', Accent::Acute};
    case 0x00DB: return {'U', Accent::Circumflex};
    case 0x00DC: return {'U', Accent::Diaeresis};
    case 0x00D1: return {'N', Accent::Tilde};
    case 0x00E0: return {'a', Accent::Grave};
    case 0x00E1: return {'a', Accent::Acute};
    case 0x00E2: return {'a', Accent::Circumflex};
    case 0x00E3: return {'a', Accent::Tilde};
    case 0x00E4: return {'a', Accent::Diaeresis};
    case 0x00E7: return {'c', Accent::Cedilla};
    case 0x00E8: return {'e', Accent::Grave};
    case 0x00E9: return {'e', Accent::Acute};
    case 0x00EA: return {'e', Accent::Circumflex};
    case 0x00EB: return {'e', Accent::Diaeresis};
    case 0x00EC: return {'i', Accent::Grave};
    case 0x00ED: return {'i', Accent::Acute};
    case 0x00EE: return {'i', Accent::Circumflex};
    case 0x00EF: return {'i', Accent::Diaeresis};
    case 0x00F2: return {'o', Accent::Grave};
    case 0x00F3: return {'o', Accent::Acute};
    case 0x00F4: return {'o', Accent::Circumflex};
    case 0x00F5: return {'o', Accent::Tilde};
    case 0x00F6: return {'o', Accent::Diaeresis};
    case 0x00F9: return {'u', Accent::Grave};
    case 0x00FA: return {'u', Accent::Acute};
    case 0x00FB: return {'u', Accent::Circumflex};
    case 0x00FC: return {'u', Accent::Diaeresis};
    case 0x00F1: return {'n', Accent::Tilde};
    case 0x00DD: return {'Y', Accent::Acute};
    case 0x00FD: return {'y', Accent::Acute};
    case 0x00FF: return {'y', Accent::Diaeresis};
    case 0x00A0: return {' ', Accent::None};
    default:
      if (cp >= 0x80) {
        static uint32_t reported[16] = {};
        static size_t reportedCount = 0;
        bool alreadyReported = false;
        for (size_t i = 0; i < reportedCount; ++i) {
          if (reported[i] == cp) alreadyReported = true;
        }
        if (!alreadyReported && reportedCount < 16) {
          reported[reportedCount++] = cp;
          Serial.printf("[FONT] Unsupported Unicode U+%04lX\n",
                        static_cast<unsigned long>(cp));
        }
      }
      return {cp < 0x80 ? static_cast<char>(cp) : '?', Accent::None};
  }
}

void drawAccent(M5Canvas& canvas, Accent accent, int32_t x, int32_t y,
                int32_t glyphWidth) {
  const int32_t center = x + glyphWidth / 2;
  switch (accent) {
    case Accent::Acute:
      canvas.drawLine(center, y + 2, center + 2, y, TFT_BLACK);
      break;
    case Accent::Grave:
      canvas.drawLine(center - 2, y, center, y + 2, TFT_BLACK);
      break;
    case Accent::Circumflex:
      canvas.drawLine(center - 2, y + 2, center, y, TFT_BLACK);
      canvas.drawLine(center, y, center + 2, y + 2, TFT_BLACK);
      break;
    case Accent::Tilde:
      canvas.drawPixel(center - 3, y + 2, TFT_BLACK);
      canvas.drawLine(center - 2, y + 1, center, y + 1, TFT_BLACK);
      canvas.drawLine(center + 1, y + 2, center + 3, y + 2, TFT_BLACK);
      break;
    case Accent::Diaeresis:
      canvas.fillCircle(center - 2, y + 1, 1, TFT_BLACK);
      canvas.fillCircle(center + 2, y + 1, 1, TFT_BLACK);
      break;
    case Accent::Cedilla:
      canvas.drawLine(center, y + canvas.fontHeight() - 3, center - 1,
                      y + canvas.fontHeight() - 1, TFT_BLACK);
      break;
    case Accent::None:
      break;
  }
}

}  // namespace

int32_t portuguese_text::width(M5Canvas& canvas, const std::string& utf8) {
  int32_t result = 0;
  for (size_t offset = 0; offset < utf8.size();) {
    const Glyph glyph = glyphFor(nextCodepoint(utf8, offset));
    const char value[2] = {glyph.base, 0};
    result += canvas.textWidth(value);
  }
  return result;
}

void portuguese_text::draw(M5Canvas& canvas, const std::string& utf8,
                           int32_t x, int32_t y) {
  for (size_t offset = 0; offset < utf8.size();) {
    const Glyph glyph = glyphFor(nextCodepoint(utf8, offset));
    const char value[2] = {glyph.base, 0};
    const int32_t glyphWidth = canvas.textWidth(value);
    canvas.drawString(value, x, y);
    drawAccent(canvas, glyph.accent, x, y, glyphWidth);
    x += glyphWidth;
  }
}

std::string portuguese_text::truncateToWidth(M5Canvas& canvas,
                                             const std::string& utf8,
                                             int32_t maxWidth) {
  size_t offset = 0;
  size_t accepted = 0;
  int32_t used = 0;
  while (offset < utf8.size()) {
    const size_t start = offset;
    const Glyph glyph = glyphFor(nextCodepoint(utf8, offset));
    const char value[2] = {glyph.base, 0};
    const int32_t glyphWidth = canvas.textWidth(value);
    if (used + glyphWidth > maxWidth) break;
    used += glyphWidth;
    accepted = offset;
    if (offset <= start) break;
  }
  return utf8.substr(0, accepted);
}
