#pragma once

#include <M5Unified.h>
#include <string>

namespace portuguese_text {

int32_t width(M5Canvas& canvas, const std::string& utf8);
void draw(M5Canvas& canvas, const std::string& utf8, int32_t x, int32_t y);
std::string truncateToWidth(M5Canvas& canvas, const std::string& utf8,
                            int32_t maxWidth);

}  // namespace portuguese_text
