#include "XmlTokenizer.h"
#include <cctype>

namespace { bool isNameChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == ':' || c == '_' || c == '-' || c == '.'; } }

void XmlTokenizer::skipWhitespace() { while (position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_; }
std::string XmlTokenizer::parseName() { const size_t start = position_; while (position_ < input_.size() && isNameChar(input_[position_])) ++position_; return input_.substr(start, position_ - start); }
std::string XmlTokenizer::parseAttributeValue() {
  skipWhitespace();
  if (position_ >= input_.size() || (input_[position_] != '\'' && input_[position_] != '"')) return {};
  const char quote = input_[position_++]; const size_t start = position_;
  while (position_ < input_.size() && input_[position_] != quote) ++position_;
  const std::string value = input_.substr(start, position_ - start);
  if (position_ < input_.size()) ++position_;
  return value;
}

std::string XmlTokenizer::localName(const std::string& qualified) { const size_t colon = qualified.find(':'); return colon == std::string::npos ? qualified : qualified.substr(colon + 1); }

XmlToken XmlTokenizer::next() {
  if (!error_.empty()) return {XmlTokenType::Error};
  if (position_ >= input_.size()) return {XmlTokenType::End};
  if (input_[position_] != '<') { const size_t start = position_; while (position_ < input_.size() && input_[position_] != '<') ++position_; XmlToken t; t.type = XmlTokenType::Text; t.text = input_.substr(start, position_ - start); return t; }
  if (input_.compare(position_, 4, "<!--") == 0) { const size_t end = input_.find("-->", position_ + 4); if (end == std::string::npos) { error_ = "Comentario XML sem fechamento"; return {XmlTokenType::Error}; } position_ = end + 3; return next(); }
  if (input_.compare(position_, 2, "<?") == 0) { const size_t end = input_.find("?>", position_ + 2); if (end == std::string::npos) { error_ = "Declaracao XML sem fechamento"; return {XmlTokenType::Error}; } position_ = end + 2; return next(); }
  if (input_.compare(position_, 2, "<!") == 0) { const size_t end = input_.find('>', position_ + 2); if (end == std::string::npos) { error_ = "Declaracao XML invalida"; return {XmlTokenType::Error}; } position_ = end + 1; return next(); }
  ++position_; XmlToken token;
  if (position_ < input_.size() && input_[position_] == '/') { ++position_; token.type = XmlTokenType::EndElement; token.name = parseName(); const size_t end = input_.find('>', position_); if (end == std::string::npos) { error_ = "Tag final invalida"; return {XmlTokenType::Error}; } position_ = end + 1; return token; }
  token.type = XmlTokenType::StartElement; token.name = parseName();
  if (token.name.empty()) { error_ = "Nome de elemento vazio"; return {XmlTokenType::Error}; }
  while (position_ < input_.size()) {
    skipWhitespace();
    if (input_[position_] == '>') { ++position_; return token; }
    if (input_[position_] == '/' && position_ + 1 < input_.size() && input_[position_ + 1] == '>') { position_ += 2; token.selfClosing = true; return token; }
    const std::string name = parseName(); skipWhitespace();
    if (name.empty() || position_ >= input_.size() || input_[position_] != '=') { error_ = "Atributo XML invalido"; return {XmlTokenType::Error}; }
    ++position_; const std::string value = parseAttributeValue(); token.attributes[name] = value;
  }
  error_ = "Elemento XML sem fechamento"; return {XmlTokenType::Error};
}
