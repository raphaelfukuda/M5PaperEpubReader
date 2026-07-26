#include "PowerManager.h"

#include <WiFi.h>
#include <esp_bt.h>

void PowerManager::begin() {
  WiFi.mode(WIFI_OFF);
  btStop();
}

