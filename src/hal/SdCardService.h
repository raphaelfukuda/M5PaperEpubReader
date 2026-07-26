#pragma once

#include <stdint.h>
#include "SpiBusGuard.h"

struct SdCardStatus {
  bool mounted = false;
  uint64_t sizeBytes = 0;
  uint32_t mountMs = 0;
};

class SdCardService {
 public:
  explicit SdCardService(SpiBusGuard& busGuard) : busGuard_(busGuard) {}
  SdCardStatus begin();
  const SdCardStatus& status() const { return status_; }
  bool directoryExists(const char* path);

 private:
  SpiBusGuard& busGuard_;
  SdCardStatus status_;
};
