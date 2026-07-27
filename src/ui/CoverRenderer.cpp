#include "CoverRenderer.h"
#include <algorithm>
#include "epub/EpubContentDiscovery.h"

namespace {
bool decodeCover(M5Canvas& target, const std::string& data,
                 const std::string& mediaType, float scale) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.data());
  if (mediaType == "image/jpeg")
    return target.drawJpg(bytes, static_cast<uint32_t>(data.size()), 0, 0,
                          target.width(), target.height(), 0, 0, scale,
                          scale, top_left);
  if (mediaType == "image/png")
    return target.drawPng(bytes, static_cast<uint32_t>(data.size()), 0, 0,
                          target.width(), target.height(), 0, 0, scale,
                          scale, top_left);
  return false;
}
}  // namespace

bool drawCoverImage(M5Canvas& canvas, const std::string& data,
                    const std::string& mediaType, int32_t x, int32_t y,
                    int32_t maximumWidth, int32_t maximumHeight) {
  if (data.empty() || maximumWidth <= 0 || maximumHeight <= 0) return false;
  uint32_t width = 0, height = 0;
  if (!epub_content::imageDimensions(
          reinterpret_cast<const uint8_t*>(data.data()), data.size(), mediaType,
          width, height))
    return false;
  const float scale = std::min(1.0f, std::min(
      static_cast<float>(maximumWidth) / width,
      static_cast<float>(maximumHeight) / height));
  const int32_t renderedWidth = std::max<int32_t>(1, static_cast<int32_t>(width * scale));
  const int32_t renderedHeight = std::max<int32_t>(1, static_cast<int32_t>(height * scale));
  const int32_t left = x + (maximumWidth - renderedWidth) / 2;
  const int32_t top = y + (maximumHeight - renderedHeight) / 2;
  if (static_cast<uint8_t>(canvas.getColorDepth()) > 1) {
    canvas.fillRect(x, y, maximumWidth, maximumHeight, TFT_WHITE);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.data());
    if (mediaType == "image/jpeg")
      return canvas.drawJpg(bytes, static_cast<uint32_t>(data.size()), left, top,
                            renderedWidth, renderedHeight, 0, 0, scale,
                            scale, top_left);
    if (mediaType == "image/png")
      return canvas.drawPng(bytes, static_cast<uint32_t>(data.size()), left, top,
                            renderedWidth, renderedHeight, 0, 0, scale,
                            scale, top_left);
    return false;
  }
  M5Canvas grayscale(&M5.Display);
  grayscale.setColorDepth(16);
  grayscale.setPsram(true);
  if (!grayscale.createSprite(renderedWidth, renderedHeight)) return false;
  grayscale.fillScreen(TFT_WHITE);
  if (!decodeCover(grayscale, data, mediaType, scale)) return false;

  canvas.fillRect(x, y, maximumWidth, maximumHeight, TFT_WHITE);
  // An 8x8 ordered halftone produces 64 perceived gray levels while keeping
  // the reader's proven 1-bit framebuffer and partial-refresh path.
  static constexpr uint8_t bayer8[8][8] = {
      {0,48,12,60,3,51,15,63}, {32,16,44,28,35,19,47,31},
      {8,56,4,52,11,59,7,55}, {40,24,36,20,43,27,39,23},
      {2,50,14,62,1,49,13,61}, {34,18,46,30,33,17,45,29},
      {10,58,6,54,9,57,5,53}, {42,26,38,22,41,25,37,21}};
  for (int32_t py = 0; py < renderedHeight; ++py) {
    for (int32_t px = 0; px < renderedWidth; ++px) {
      const uint16_t color = grayscale.readPixel(px, py);
      const uint32_t red = ((color >> 11) & 0x1F) * 255U / 31U;
      const uint32_t green = ((color >> 5) & 0x3F) * 255U / 63U;
      const uint32_t blue = (color & 0x1F) * 255U / 31U;
      const uint32_t luminance = (red * 30U + green * 59U + blue * 11U) / 100U;
      const uint32_t threshold = 2U + bayer8[py & 7][px & 7] * 4U;
      canvas.drawPixel(left + px, top + py,
                       luminance < threshold ? TFT_BLACK : TFT_WHITE);
    }
    if ((py & 15) == 15) yield();
  }
  return true;
}
