#pragma once

#include "AppConfig.h"

#if M5EPUB_ENABLE_WEB_PORTAL

#include <Preferences.h>
#include <stdint.h>
#include <string>
#include <vector>

enum class WifiState : uint8_t {
  Off,
  Scanning,
  ScanDone,
  Connecting,
  Connected,
  Failed
};

struct WifiNetwork {
  std::string ssid;
  int32_t rssi = 0;
  bool secured = false;
  bool saved = false;
};

class WifiService {
 public:
  WifiService() = default;
  ~WifiService();

  bool beginScan();
  bool connect(const std::string& ssid, const std::string& password,
               bool remember);
  bool connectSaved(const std::string& ssid);
  void poll(uint32_t nowMs);
  void stop();

  bool hasSaved() const;
  bool hasSaved(const std::string& ssid) const;
  bool forget(const std::string& ssid);
  void forgetAll();

  WifiState state() const { return state_; }
  const std::vector<WifiNetwork>& networks() const { return networks_; }
  std::string ip() const;
  const std::string& ssid() const { return activeSsid_; }
  int32_t rssi() const;
  const std::string& lastError() const { return lastError_; }

 private:
  struct SavedNetwork {
    std::string ssid;
    std::string password;
  };

  void loadSaved();
  void saveAll();
  void remember(const std::string& ssid, const std::string& password);
  const SavedNetwork* findSaved(const std::string& ssid) const;
  void finishScan();

  WifiState state_ = WifiState::Off;
  std::vector<WifiNetwork> networks_;
  std::vector<SavedNetwork> saved_;
  std::string activeSsid_;
  std::string pendingPassword_;
  std::string lastError_;
  bool rememberPending_ = false;
  bool savedLoaded_ = false;
  uint32_t stateStartedMs_ = 0;
};

#endif
