#include "PathUtils.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace {
char asciiLower(char value) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
}
}

namespace path_utils {
bool isHiddenName(const std::string& name) { return !name.empty() && name[0] == '.'; }

bool hasEpubExtension(const std::string& name) {
  if (name.size() < 5) return false;
  const std::string suffix = name.substr(name.size() - 5);
  return asciiLower(suffix[0]) == '.' && asciiLower(suffix[1]) == 'e' &&
         asciiLower(suffix[2]) == 'p' && asciiLower(suffix[3]) == 'u' &&
         asciiLower(suffix[4]) == 'b';
}

std::string join(const std::string& base, const std::string& child) {
  if (child.empty()) return base.empty() ? "/" : base;
  if (child[0] == '/') return child;
  if (base.empty() || base == "/") return "/" + child;
  return base.back() == '/' ? base + child : base + "/" + child;
}

std::string parent(const std::string& path) {
  if (path.empty() || path == "/") return "/";
  const size_t end = path.find_last_not_of('/');
  if (end == std::string::npos) return "/";
  const size_t slash = path.find_last_of('/', end);
  return slash == 0 || slash == std::string::npos ? "/" : path.substr(0, slash);
}

std::string fileName(const std::string& path) {
  const size_t end = path.find_last_not_of('/');
  if (end == std::string::npos) return {};
  const size_t slash = path.find_last_of('/', end);
  return path.substr(slash == std::string::npos ? 0 : slash + 1, end - (slash == std::string::npos ? 0 : slash + 1) + 1);
}

int compareCaseInsensitive(const std::string& left, const std::string& right) {
  const size_t count = std::min(left.size(), right.size());
  for (size_t i = 0; i < count; ++i) {
    const char a = asciiLower(left[i]);
    const char b = asciiLower(right[i]);
    if (a != b) return a < b ? -1 : 1;
  }
  if (left.size() == right.size()) return 0;
  return left.size() < right.size() ? -1 : 1;
}

bool isSafeZipPath(const std::string& path) {
  if (path.empty() || path[0] == '/' || path.find('\\') != std::string::npos || path.find(':') != std::string::npos) return false;
  size_t start = 0;
  while (start <= path.size()) { const size_t end = path.find('/', start); const std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start); if (part.empty() || part == "." || part == "..") return false; if (end == std::string::npos) break; start = end + 1; }
  return true;
}

std::string resolveRelative(const std::string& baseFile, const std::string& relative) {
  if (relative.empty() || relative[0] == '/' || relative.find('\\') != std::string::npos || relative.find(':') != std::string::npos) return {};
  std::vector<std::string> parts; const size_t slash = baseFile.find_last_of('/'); std::string combined = (slash == std::string::npos ? "" : baseFile.substr(0, slash + 1)) + relative; size_t start = 0;
  while (start <= combined.size()) { const size_t end = combined.find('/', start); const std::string part = combined.substr(start, end == std::string::npos ? std::string::npos : end - start); if (part == "..") { if (parts.empty()) return {}; parts.pop_back(); } else if (!part.empty() && part != ".") parts.push_back(part); if (end == std::string::npos) break; start = end + 1; }
  std::string result; for (size_t i = 0; i < parts.size(); ++i) { if (i) result += '/'; result += parts[i]; } return isSafeZipPath(result) ? result : std::string{};
}
}  // namespace path_utils
