#include "Logger.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include "BuildConfig.h"

void Logger::begin() {
  Serial.begin(115200);
  Serial.printf("\n%s %s\n", M5EPUB_APP_NAME, M5EPUB_VERSION);
}

void Logger::hardwareReport(bool sdMounted, unsigned long long sdSize,
                            unsigned long sdMountMs) {
  Serial.printf("Board enum: %d (expected %d)\n", static_cast<int>(M5.getBoard()),
                static_cast<int>(m5::board_t::board_M5Paper));
  Serial.printf("Display: %ld x %ld, rotation=%u\n", M5.Display.width(),
                M5.Display.height(), M5.Display.getRotation());
  Serial.printf("Touch enabled: %s\n", M5.Touch.isEnabled() ? "yes" : "no");
  Serial.printf("Flash: %u bytes\n", ESP.getFlashChipSize());
  Serial.printf("PSRAM total/free: %u/%u bytes\n", ESP.getPsramSize(), ESP.getFreePsram());
  Serial.printf("Internal heap free/largest: %u/%u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.printf("SD mounted: %s; size=%llu bytes; mount=%lu ms\n",
                sdMounted ? "yes" : "no", sdSize, sdMountMs);
  Serial.printf("CPU: %u MHz\n", ESP.getCpuFreqMHz());
}

