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
  while (gpio_get_level(increase) == 0 || gpio_get_level(decrease) == 0)
    delay(10);
  M5.Display.sleep();
  M5.Display.waitDisplay();
  gpio_wakeup_enable(increase, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(decrease, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  gpio_wakeup_disable(increase);
  gpio_wakeup_disable(decrease);
  M5.Display.wakeup();
}
