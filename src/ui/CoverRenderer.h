#pragma once

#include <M5Unified.h>
#include <string>

bool drawCoverImage(M5Canvas& canvas, const std::string& data,
                    const std::string& mediaType, int32_t x, int32_t y,
                    int32_t maximumWidth, int32_t maximumHeight);

bool createCoverThumbnail4(const std::string& data, const std::string& mediaType,
                           uint16_t maximumWidth, uint16_t maximumHeight,
                           uint16_t& width, uint16_t& height,
                           std::string& packedPixels);
bool drawCoverThumbnail4(M5Canvas& canvas, const std::string& packedPixels,
                         uint16_t width, uint16_t height, int32_t x, int32_t y,
                         int32_t maximumWidth, int32_t maximumHeight);
