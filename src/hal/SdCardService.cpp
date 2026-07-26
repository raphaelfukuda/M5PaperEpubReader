#include "SdCardService.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "AppConfig.h"

SdCardStatus SdCardService::begin() {
  const uint32_t started = millis();
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard);
  status_ = {};
  if (bus) {
    // M5Paper routes both IT8951 and SD to VSPI on SCK14/MISO13/MOSI12.
    SPI.begin(app_config::kSharedSpiClock, app_config::kSharedSpiMiso,
              app_config::kSharedSpiMosi, app_config::kSdChipSelect);
    status_.mounted = SD.begin(app_config::kSdChipSelect, SPI,
                               app_config::kSdFrequencyHz);
    if (status_.mounted) status_.sizeBytes = SD.cardSize();
  }
  status_.mountMs = millis() - started;
  return status_;
}

bool SdCardService::directoryExists(const char* path) {
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard);
  if (!bus || !status_.mounted) return false;
  fs::File file = SD.open(path);
  const bool result = file && file.isDirectory();
  file.close();
  return result;
}
