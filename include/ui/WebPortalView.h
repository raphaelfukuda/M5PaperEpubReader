#pragma once

#include "AppConfig.h"

#if M5EPUB_ENABLE_WEB_PORTAL
#include "net/WifiService.h"
#include "hal/DisplayManager.h"
#include <string>

enum class WebPortalViewAction : uint8_t {
  None, Network, Rescan, ForgetAll, Cancel, Backspace, Connect, ToggleRemember, Key
};

struct WebPortalViewHit {
  WebPortalViewAction action = WebPortalViewAction::None;
  size_t networkIndex = 0;
  char key = 0;
};

class WebPortalView {
 public:
  explicit WebPortalView(DisplayManager& display) : display_(display) {}
  void renderNetworks(const WifiService& wifi);
  void renderPassword(const std::string& ssid, const std::string& password,
                      bool remember, const std::string& error = {});
  WebPortalViewHit hitNetworks(int32_t x, int32_t y,
                               size_t networkCount) const;
  WebPortalViewHit hitPassword(int32_t x, int32_t y) const;

 private:
  DisplayManager& display_;
};
#endif
