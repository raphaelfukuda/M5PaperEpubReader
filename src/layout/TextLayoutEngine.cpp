#include "TextLayoutEngine.h"
#include <cctype>
#include "PortugueseTextRenderer.h"
#include "ReaderFont.h"

void TextLayoutEngine::begin(M5Canvas& canvas, const ReaderSettings& settings) {
  canvas_ = &canvas; settings_ = settings; word_.clear(); pendingSpace_ = false; full_ = false; style_ = {}; baseFontSize_ = settings_.fontSize;
  canvas_->fillScreen(TFT_WHITE); canvas_->setTextColor(TFT_BLACK, TFT_WHITE); canvas_->setTextDatum(top_left); updateFontMetrics(); x_ = settings_.marginLeft; y_ = settings_.marginTop;
}

void TextLayoutEngine::updateFontMetrics() {
  uint16_t styledSize = static_cast<uint16_t>(baseFontSize_ * style_.fontScale);
  if (styledSize < 16) styledSize = 16;
  if (styledSize > 60) styledSize = 60;
  canvas_->setFont(reader_font::forSize(styledSize, settings_.fontFamily)); canvas_->setTextSize(reader_font::scaleForSize(styledSize, settings_.fontFamily));
  lineHeight_ = static_cast<int32_t>(canvas_->fontHeight() * settings_.lineSpacing * style_.lineSpacingScale);
}

void TextLayoutEngine::applyStyle(uint8_t code) {
  using namespace text_style_control;
  if (code == BoldOn) style_.bold = true;
  else if (code == BoldOff) style_.bold = false;
  else if (code == ItalicOn) style_.italic = true;
  else if (code == ItalicOff) style_.italic = false;
  else {
    const bool bold = style_.bold, italic = style_.italic; style_ = {}; style_.bold = bold; style_.italic = italic;
    if (code == Paragraph) style_.firstLineIndent = baseFontSize_ * 3 / 2;
    else if (code == Blockquote) { style_.blockIndent = baseFontSize_ * 2; style_.lineSpacingScale = 1.05f; }
    else if (code >= Heading1 && code <= Heading6) {
      static const float scales[] = {1.50f, 1.35f, 1.25f, 1.15f, 1.10f, 1.05f};
      style_.fontScale = scales[code - Heading1]; style_.lineSpacingScale = 1.08f; style_.bold = true;
    }
    x_ = settings_.marginLeft + style_.blockIndent + style_.firstLineIndent;
  }
  updateFontMetrics();
}

bool TextLayoutEngine::newLine(bool paragraph) { x_ = settings_.marginLeft + style_.blockIndent; y_ += lineHeight_ + (paragraph ? lineHeight_ / 2 : 0); pendingSpace_ = false; if (y_ + lineHeight_ > canvas_->height() - settings_.marginBottom) full_ = true; return !full_; }

bool TextLayoutEngine::flushWord() {
  if (word_.empty() || full_) return !full_; const int32_t maxX = canvas_->width() - settings_.marginRight; const int32_t space = pendingSpace_ ? canvas_->textWidth(" ") : 0; const int32_t width = portuguese_text::width(*canvas_, word_);
  if (x_ + space + width > maxX && x_ > settings_.marginLeft) if (!newLine()) return false;
  if (width <= maxX - settings_.marginLeft) { if (pendingSpace_ && x_ > settings_.marginLeft) { canvas_->drawString(" ", x_, y_); x_ += canvas_->textWidth(" "); } portuguese_text::draw(*canvas_, word_, x_, y_); x_ += width; }
  else { std::string segment; size_t i = 0; while (i < word_.size() && !full_) { const size_t bytes = (static_cast<unsigned char>(word_[i]) & 0x80) == 0 ? 1 : ((static_cast<unsigned char>(word_[i]) & 0xE0) == 0xC0 ? 2 : ((static_cast<unsigned char>(word_[i]) & 0xF0) == 0xE0 ? 3 : 4)); const std::string candidate = segment + word_.substr(i, bytes); if (!segment.empty() && portuguese_text::width(*canvas_, candidate) > maxX - settings_.marginLeft) { portuguese_text::draw(*canvas_, segment, x_, y_); if (!newLine()) break; segment.clear(); } segment += word_.substr(i, bytes); i += bytes; } if (!full_ && !segment.empty()) { portuguese_text::draw(*canvas_, segment, x_, y_); x_ += portuguese_text::width(*canvas_, segment); } }
  word_.clear(); pendingSpace_ = false; return !full_;
}

size_t TextLayoutEngine::processText(const std::string& text) {
  size_t consumed = 0, wordStart = 0;
  for (size_t i = 0; i < text.size() && !full_; ++i) { const unsigned char c = text[i]; if (c == static_cast<unsigned char>(text_style_control::kEscape) && i + 1 < text.size()) { if (!flushWord()) return wordStart; applyStyle(static_cast<uint8_t>(text[++i])); consumed = i + 1; wordStart = i + 1; } else if (c == '\n') { if (!flushWord()) return wordStart; consumed = i + 1; if (!newLine(true)) return consumed; wordStart = i + 1; } else if (std::isspace(c)) { if (!flushWord()) return wordStart; pendingSpace_ = true; consumed = i + 1; wordStart = i + 1; } else { if (word_.empty()) wordStart = i; word_ += static_cast<char>(c); } }
  if (!full_ && !word_.empty()) { if (!flushWord()) return wordStart; consumed = text.size(); }
  return consumed;
}
bool TextLayoutEngine::finish() { return flushWord(); }
