#pragma once

#include <stdint.h>

enum class AppEventType {
  None, TouchPressed, TouchReleased, Tap, SwipeLeft, SwipeRight, SwipeUp,
  SwipeDown, OpenDirectory, OpenBook, Back, NextPage, PreviousPage, OpenMenu,
  CloseMenu, SleepRequested, SdRemoved, Error
};

struct AppEvent {
  AppEvent(AppEventType eventType = AppEventType::None, int32_t eventX = 0,
           int32_t eventY = 0) : type(eventType), x(eventX), y(eventY) {}
  AppEventType type;
  int32_t x;
  int32_t y;
};
