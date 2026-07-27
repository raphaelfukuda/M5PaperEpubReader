#include "PowerManager.h"

#include <WiFi.h>
#include <esp_bt.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <M5Unified.h>
#include "AppConfig.h"

void PowerManager::begin() {
  WiFi.mode(WIFI_OFF);
  btStop();
}

void PowerManager::enterLowPowerSleep() {
  const gpio_num_t increase = app_config::kIncreaseFontButtonPin;
  const gpio_num_t decrease = app_config::kDecreaseFontButtonPin;
  constexpr gpio_num_t touchInterrupt = GPIO_NUM_36;
  while (gpio_get_level(increase) == 0 || gpio_get_level(decrease) == 0)
    delay(10);
  // A menu tap can keep the GT911 interrupt asserted until its final release
  // packet is consumed. Entering light sleep with GPIO36 already low causes an
  // immediate wake, so drain touch updates and require a stable idle-high pin.
  const uint32_t touchReleaseStartedMs = millis();
  uint32_t touchHighSinceMs = 0;
  while (millis() - touchReleaseStartedMs < 1500) {
    M5.update();
    if (gpio_get_level(touchInterrupt) != 0) {
      if (touchHighSinceMs == 0) touchHighSinceMs = millis();
      if (millis() - touchHighSinceMs >= 80) break;
    } else {
      touchHighSinceMs = 0;
    }
    delay(10);
  }
  const bool touchWakeReady = gpio_get_level(touchInterrupt) != 0 &&
                              touchHighSinceMs != 0;
  M5.Display.sleep();
  M5.Display.waitDisplay();
  gpio_wakeup_enable(increase, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(decrease, GPIO_INTR_LOW_LEVEL);
  // The original M5Paper routes the GT911 interrupt to GPIO36. M5GFX's
  // Display.sleep() only sleeps the IT8951 panel, so the touch controller
  // remains active. GPIO wake therefore adds touch without changing the
  // existing ESP32 light-sleep mode.
  if (touchWakeReady)
    gpio_wakeup_enable(touchInterrupt, GPIO_INTR_LOW_LEVEL);
  else
    Serial.println("M5EPUB_SLEEP,touch_wake=disabled,pin_still_low=1");
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  Serial.printf("M5EPUB_SLEEP,wakeup_cause=%u,touch=%d,increase=%d,decrease=%d\n",
                static_cast<unsigned>(esp_sleep_get_wakeup_cause()),
                gpio_get_level(touchInterrupt), gpio_get_level(increase),
                gpio_get_level(decrease));
  gpio_wakeup_disable(increase);
  gpio_wakeup_disable(decrease);
  if (touchWakeReady) gpio_wakeup_disable(touchInterrupt);
  M5.Display.wakeup();
}
