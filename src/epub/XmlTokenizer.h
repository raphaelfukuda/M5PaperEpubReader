#pragma once
#include <map>
#include <string>

enum class XmlTokenType { StartElement, EndElement, Text, End, Error };
struct XmlToken {
  XmlToken(XmlTokenType tokenType = XmlTokenType::End) : type(tokenType) {}
  XmlTokenType type = XmlTokenType::End;
  std::string name;
  std::string text;
  std::map<std::string, std::string> attributes;
  bool selfClosing = false;
};

class XmlTokenizer {
 public:
  explicit XmlTokenizer(const std::string& input) : input_(input) {}
  XmlToken next();
  const std::string& error() const { return error_; }
  static std::string localName(const std::string& qualified);
 private:
  void skipWhitespace();
  std::string parseName();
  std::string parseAttributeValue();
  const std::string& input_;
  size_t position_ = 0;
  std::string error_;
};
