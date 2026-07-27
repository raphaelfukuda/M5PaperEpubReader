#pragma once

#include <stddef.h>
#include <string>

namespace portal_path {

constexpr size_t kMaximumPathLength = 200;
constexpr size_t kDefaultMaximumNameLength = 120;

bool resolve(const std::string& root, const std::string& requested,
             std::string& resolved, std::string* error = nullptr);
std::string sanitizeName(const std::string& name,
                         size_t maximumLength = kDefaultMaximumNameLength);

}  // namespace portal_path
