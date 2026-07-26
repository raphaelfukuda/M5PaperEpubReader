#pragma once

#include "app/AppEvent.h"

class TouchController {
 public:
  bool begin();
  AppEvent poll();
  bool isAvailable() const { return available_; }

 private:
  bool available_ = false;
  bool pressed_ = false;
  bool gestureIsSwipe_ = false;
  int32_t startX_ = 0;
  int32_t startY_ = 0;
  int32_t lastX_ = 0;
  int32_t lastY_ = 0;
  uint32_t startedMs_ = 0;
};
