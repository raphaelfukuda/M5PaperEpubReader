#include "net/PortalPath.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <vector>

namespace portal_path {
namespace {
void setError(std::string* error, const char* message) {
  if (error) *error = message;
}

bool decodeUtf8(const std::string& value, size_t& offset, uint32_t& codepoint) {
  const uint8_t first = static_cast<uint8_t>(value[offset++]);
  if (first < 0x80) { codepoint = first; return true; }
  size_t count = 0;
  uint32_t result = 0;
  if ((first & 0xE0) == 0xC0) { count = 1; result = first & 0x1F; }
  else if ((first & 0xF0) == 0xE0) { count = 2; result = first & 0x0F; }
  else if ((first & 0xF8) == 0xF0) { count = 3; result = first & 0x07; }
  else { codepoint = '_'; return false; }
  if (offset + count > value.size()) { offset = value.size(); codepoint = '_'; return false; }
  for (size_t index = 0; index < count; ++index) {
    const uint8_t next = static_cast<uint8_t>(value[offset++]);
    if ((next & 0xC0) != 0x80) { codepoint = '_'; return false; }
    result = (result << 6) | (next & 0x3F);
  }
  codepoint = result;
  return true;
}

char latin1Ascii(uint32_t cp) {
  if (cp >= 0xC0 && cp <= 0xC5) return 'A';
  if (cp == 0xC7) return 'C';
  if (cp >= 0xC8 && cp <= 0xCB) return 'E';
  if (cp >= 0xCC && cp <= 0xCF) return 'I';
  if (cp == 0xD0) return 'D';
  if (cp == 0xD1) return 'N';
  if (cp >= 0xD2 && cp <= 0xD6) return 'O';
  if (cp == 0xD8) return 'O';
  if (cp >= 0xD9 && cp <= 0xDC) return 'U';
  if (cp == 0xDD) return 'Y';
  if (cp == 0xDE) return 'T';
  if (cp == 0xDF) return 's';
  if (cp >= 0xE0 && cp <= 0xE5) return 'a';
  if (cp == 0xE7) return 'c';
  if (cp >= 0xE8 && cp <= 0xEB) return 'e';
  if (cp >= 0xEC && cp <= 0xEF) return 'i';
  if (cp == 0xF0) return 'd';
  if (cp == 0xF1) return 'n';
  if (cp >= 0xF2 && cp <= 0xF6) return 'o';
  if (cp == 0xF8) return 'o';
  if (cp >= 0xF9 && cp <= 0xFC) return 'u';
  if (cp == 0xFD || cp == 0xFF) return 'y';
  if (cp == 0xFE) return 't';
  return '_';
}

std::vector<std::string> segments(const std::string& path, bool& traversal) {
  std::vector<std::string> result;
  traversal = false;
  size_t begin = 0;
  while (begin <= path.size()) {
    size_t end = path.find_first_of("/\\", begin);
    if (end == std::string::npos) end = path.size();
    const std::string part = path.substr(begin, end - begin);
    if (part == "..") { traversal = true; return {}; }
    if (!part.empty() && part != ".") result.push_back(part);
    if (end == path.size()) break;
    begin = end + 1;
  }
  return result;
}
}

bool resolve(const std::string& root, const std::string& requested,
             std::string& resolved, std::string* error) {
  resolved.clear();
  if (requested.size() > kMaximumPathLength || root.size() > kMaximumPathLength) {
    setError(error, "path is too long");
    return false;
  }
  bool rootTraversal = false;
  bool requestedTraversal = false;
  std::vector<std::string> all = segments(root, rootTraversal);
  const std::vector<std::string> suffix = segments(requested, requestedTraversal);
  if (rootTraversal || requestedTraversal) {
    setError(error, "parent traversal is not allowed");
    return false;
  }
  all.insert(all.end(), suffix.begin(), suffix.end());
  resolved = "/";
  for (size_t index = 0; index < all.size(); ++index) {
    if (index) resolved += '/';
    resolved += all[index];
  }
  if (resolved.size() > kMaximumPathLength) {
    resolved.clear();
    setError(error, "resolved path is too long");
    return false;
  }
  if (error) error->clear();
  return true;
}

std::string sanitizeName(const std::string& name, size_t maximumLength) {
  std::string output;
  output.reserve(std::min(name.size(), maximumLength));
  bool separator = false;
  for (size_t offset = 0; offset < name.size();) {
    uint32_t cp = 0;
    decodeUtf8(name, offset, cp);
    char value = '_';
    if (cp < 128 && (std::isalnum(static_cast<unsigned char>(cp)) ||
                     cp == '.' || cp == '-' || cp == '_')) value = static_cast<char>(cp);
    else if (cp == ' ' || cp == 0xA0) value = '_';
    else if (cp >= 0xC0 && cp <= 0xFF) value = latin1Ascii(cp);
    if (value == '_' || value == '-' || value == '.') {
      if (separator && value != '.') continue;
      separator = true;
    } else separator = false;
    output += value;
  }
  while (!output.empty() && (output.front() == '.' || output.front() == '_'))
    output.erase(output.begin());
  while (!output.empty() && (output.back() == '.' || output.back() == '_'))
    output.pop_back();
  if (output.empty()) output = "file";
  if (output.size() <= maximumLength) return output;
  const size_t dot = output.find_last_of('.');
  const std::string extension = dot != std::string::npos && output.size() - dot <= 16
                                    ? output.substr(dot) : std::string();
  const size_t stemLength = maximumLength > extension.size()
                                ? maximumLength - extension.size() : maximumLength;
  return output.substr(0, stemLength) + (stemLength + extension.size() <= maximumLength
                                             ? extension : std::string());
}

}  // namespace portal_path
