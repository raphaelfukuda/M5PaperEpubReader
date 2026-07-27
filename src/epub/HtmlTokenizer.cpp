#include "HtmlTokenizer.h"
#include <algorithm>
#include <cctype>
#include "HtmlEntityDecoder.h"
#include "layout/TextStyle.h"

namespace {
void normalizeLayoutSpaces(std::string& text) {
  // EPUB files frequently use NBSP for visual spacing. The compact layout
  // engine needs an emergency break opportunity there; otherwise a whole
  // phrase becomes one oversized word and is split at arbitrary characters.
  const std::string nbsp{"\xC2\xA0", 2};
  size_t position = 0;
  while ((position = text.find(nbsp, position)) != std::string::npos) {
    text.replace(position, nbsp.size(), " ");
    ++position;
  }
}

size_t incompleteUtf8SuffixLength(const std::string& text) {
  if (text.empty()) return 0;
  size_t lead = text.size() - 1;
  while (lead > 0 &&
         (static_cast<uint8_t>(text[lead]) & 0xC0) == 0x80) {
    --lead;
  }
  const uint8_t first = static_cast<uint8_t>(text[lead]);
  size_t expected = 1;
  if ((first & 0xE0) == 0xC0) expected = 2;
  else if ((first & 0xF0) == 0xE0) expected = 3;
  else if ((first & 0xF8) == 0xF0) expected = 4;
  const size_t available = text.size() - lead;
  return expected > available ? available : 0;
}
}  // namespace

void HtmlTokenizer::reset() { inTag_ = false; inEntity_ = false; skipping_ = false; tag_.clear(); entity_.clear(); utf8Pending_.clear(); layoutBoundary_ = 0; }
void HtmlTokenizer::finishTag(std::string& out) {
  std::string tag = tag_; std::transform(tag.begin(), tag.end(), tag.begin(), [](char c){ return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  const size_t start = tag.find_first_not_of(" /\t\r\n"); const size_t end = start == std::string::npos ? start : tag.find_first_of(" /\t\r\n>", start); const std::string name = start == std::string::npos ? "" : tag.substr(start, end - start); const bool closing = tag.find('/') == tag.find_first_not_of(" \t\r\n");
  if (name == "script" || name == "style" || name == "head") skipping_ = !closing;
  if (!skipping_) {
    uint8_t style = 0;
    if (name == "p" && !closing) style = text_style_control::Paragraph;
    else if (name == "blockquote" && !closing) style = text_style_control::Blockquote;
    else if ((name == "div" || name == "li") && !closing) style = text_style_control::PlainBlock;
    else if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && !closing) style = static_cast<uint8_t>(text_style_control::Heading1) + (name[1] - '1');
    else if (name == "strong" || name == "b") style = closing ? text_style_control::BoldOff : text_style_control::BoldOn;
    else if (name == "em" || name == "i") style = closing ? text_style_control::ItalicOff : text_style_control::ItalicOn;
    const bool structural = name == "p" || name == "br" || name == "div" ||
                            name == "li" || name == "blockquote" ||
                            name == "pre" || name == "hr" ||
                            (name.size() == 2 && name[0] == 'h' &&
                             name[1] >= '1' && name[1] <= '6');
    if (structural && (out.empty() || out.back() != '\n')) out += '\n';
    if (style != 0) {
      out += text_style_control::kEscape;
      out += static_cast<char>(style);
    }
    if (name == "li" && !closing) out += "• ";
  }
  tag_.clear();
}
void HtmlTokenizer::finishEntity(std::string& out) { out += html_entities::decode("&" + entity_ + ";"); entity_.clear(); }
std::string HtmlTokenizer::feed(const uint8_t* data, size_t length, bool finalChunk) {
  std::string out;
  const bool seededBoundary = utf8Pending_.empty() &&
                              (layoutBoundary_ == ' ' || layoutBoundary_ == '\n');
  if (seededBoundary) out += layoutBoundary_;
  out += utf8Pending_;
  utf8Pending_.clear();
  out.reserve(out.size() + length);
  for (size_t i = 0; i < length; ++i) { const char c = static_cast<char>(data[i]); if (inTag_) { if (c == '>') { inTag_ = false; finishTag(out); } else if (tag_.size() < 256) tag_ += c; continue; } if (inEntity_) { if (c == ';') { inEntity_ = false; if (!skipping_) finishEntity(out); else entity_.clear(); } else if (entity_.size() < 16) entity_ += c; else { inEntity_ = false; entity_.clear(); } continue; } if (c == '<') { inTag_ = true; tag_.clear(); } else if (c == '&') { inEntity_ = true; entity_.clear(); } else if (!skipping_) { if (std::isspace(static_cast<unsigned char>(c))) { if (out.empty() || (out.back() != ' ' && out.back() != '\n')) out += ' '; } else out += c; } }
  if (finalChunk && inEntity_) { out += '&' + entity_; inEntity_ = false; entity_.clear(); }
  if (!finalChunk) {
    const size_t pendingLength = incompleteUtf8SuffixLength(out);
    if (pendingLength != 0) {
      utf8Pending_.assign(out, out.size() - pendingLength, pendingLength);
      out.resize(out.size() - pendingLength);
    }
  } else if (!utf8Pending_.empty()) {
    out += utf8Pending_;
    utf8Pending_.clear();
  }
  normalizeLayoutSpaces(out);
  if (seededBoundary) out.erase(0, 1);
  char boundary = layoutBoundary_;
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] == text_style_control::kEscape && i + 1 < out.size()) {
      ++i;
      continue;
    }
    boundary = out[i];
  }
  layoutBoundary_ = boundary;
  return out;
}
