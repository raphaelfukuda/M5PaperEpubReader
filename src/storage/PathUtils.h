#pragma once

#include <string>

namespace path_utils {
bool isHiddenName(const std::string& name);
bool hasEpubExtension(const std::string& name);
std::string join(const std::string& base, const std::string& child);
std::string parent(const std::string& path);
std::string fileName(const std::string& path);
int compareCaseInsensitive(const std::string& left, const std::string& right);
bool isSafeZipPath(const std::string& path);
std::string resolveRelative(const std::string& baseFile, const std::string& relative);
}  // namespace path_utils
