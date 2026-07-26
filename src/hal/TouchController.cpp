#include "TouchController.h"

#include <M5Unified.h>
#include <Arduino.h>
#include <cstdlib>
#include "AppConfig.h"

bool TouchController::begin() {
  available_ = M5.Touch.isEnabled();
  return available_;
}

AppEvent TouchController::poll() {
  if (!available_) return {};
  const auto count = M5.Touch.getCount();
  if (count > 0) {
    const auto detail = M5.Touch.getDetail(0);
    lastX_ = detail.x;
    lastY_ = detail.y;
    if (!pressed_) {
      pressed_ = true;
      gestureIsSwipe_ = false;
      startX_ = lastX_;
      startY_ = lastY_;
      startedMs_ = millis();
      return {AppEventType::TouchPressed, lastX_, lastY_};
    }
    if (abs(lastX_ - startX_) >= app_config::kSwipeThreshold ||
        abs(lastY_ - startY_) >= app_config::kSwipeThreshold) gestureIsSwipe_ = true;
    return {};
  }
  if (pressed_) {
    pressed_ = false;
    const int32_t dx = lastX_ - startX_;
    const int32_t dy = lastY_ - startY_;
    if (gestureIsSwipe_) {
      if (abs(dx) > abs(dy)) return {dx < 0 ? AppEventType::SwipeLeft : AppEventType::SwipeRight, lastX_, lastY_};
      return {dy < 0 ? AppEventType::SwipeUp : AppEventType::SwipeDown, lastX_, lastY_};
    }
    if (millis() - startedMs_ <= app_config::kTapMaxDurationMs)
      return {AppEventType::Tap, startX_, startY_};
    return {AppEventType::TouchReleased, lastX_, lastY_};
  }
  return {};
}
