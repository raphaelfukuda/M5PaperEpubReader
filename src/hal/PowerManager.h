#pragma once

#include <stdint.h>

class PowerManager {
 public:
  void begin();
  void enableRadio();
  void disableRadio();
  bool radioEnabled() const { return radioEnabled_; }
  void enterLowPowerSleep();

 private:
  bool radioEnabled_ = false;
};
