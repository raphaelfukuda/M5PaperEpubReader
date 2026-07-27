#pragma once

#include <M5Unified.h>
#include <string>

bool drawCoverImage(M5Canvas& canvas, const std::string& data,
                    const std::string& mediaType, int32_t x, int32_t y,
                    int32_t maximumWidth, int32_t maximumHeight);

