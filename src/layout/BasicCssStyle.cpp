#include "BasicCssStyle.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {
std::string trimLower(std::string value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
  std::transform(value.begin(), value.end(), value.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  return value;
}

bool parseBoundedInteger(const std::string& value, const char* suffix,
                         long minimum, long maximum, long& result) {
  char* end = nullptr;
  const long parsed = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || parsed < minimum || parsed > maximum) return false;
  while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
  if (std::string(end) != suffix) return false;
  result = parsed;
  return true;
}
}  // namespace

BasicCssStyle basic_css::parseDeclarations(const std::string& declarations) {
  BasicCssStyle style;
  size_t start = 0;
  while (start < declarations.size()) {
    const size_t end = declarations.find(';', start);
    const std::string declaration = declarations.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    const size_t colon = declaration.find(':');
    if (colon != std::string::npos) {
      const std::string property = trimLower(declaration.substr(0, colon));
      const std::string value = trimLower(declaration.substr(colon + 1));
      if (property == "text-align") {
        if (value == "left") style.textAlign = BasicTextAlign::Left;
        else if (value == "center") style.textAlign = BasicTextAlign::Center;
        else if (value == "right") style.textAlign = BasicTextAlign::Right;
        else if (value == "justify") style.textAlign = BasicTextAlign::Justify;
      } else if (property == "font-weight") {
        style.bold = value == "bold" || value == "bolder" || value == "600" ||
                     value == "700" || value == "800" || value == "900";
      } else if (property == "font-style") {
        style.italic = value == "italic" || value == "oblique";
      } else if (property == "text-indent") {
        long parsed = 0;
        if (parseBoundedInteger(value, "px", -120, 240, parsed)) {
          style.hasTextIndent = true;
          style.textIndentPx = static_cast<int16_t>(parsed);
        }
      } else if (property == "line-height") {
        long parsed = 0;
        if (parseBoundedInteger(value, "%", 80, 250, parsed)) {
          style.hasLineHeightPercent = true;
          style.lineHeightPercent = static_cast<uint16_t>(parsed);
        }
      }
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return style;
}
