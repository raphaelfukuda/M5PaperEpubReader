#include "net/WifiService.h"

#if M5EPUB_ENABLE_WEB_PORTAL

#include <WiFi.h>
#include <algorithm>
#include <cstring>

namespace {
constexpr char kPreferencesNamespace[] = "m5epub-wifi";
constexpr char kCredentialsKey[] = "credentials";
constexpr size_t kMaximumSavedNetworks = 8;

#pragma pack(push, 1)
struct StoredCredentials {
  uint8_t version = 1;
  uint8_t count = 0;
  struct Entry {
    char ssid[33] = {};
    char password[65] = {};
  } entries[kMaximumSavedNetworks];
};
#pragma pack(pop)
}

WifiService::~WifiService() { stop(); }

void WifiService::loadSaved() {
  if (savedLoaded_) return;
  savedLoaded_ = true;
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, true)) return;
  StoredCredentials blob;
  if (preferences.getBytesLength(kCredentialsKey) == sizeof(blob) &&
      preferences.getBytes(kCredentialsKey, &blob, sizeof(blob)) == sizeof(blob) &&
      blob.version == 1) {
    const size_t count = std::min<size_t>(blob.count, kMaximumSavedNetworks);
    for (size_t index = 0; index < count; ++index) {
      blob.entries[index].ssid[sizeof(blob.entries[index].ssid) - 1] = '\0';
      blob.entries[index].password[sizeof(blob.entries[index].password) - 1] = '\0';
      if (blob.entries[index].ssid[0])
        saved_.push_back({blob.entries[index].ssid, blob.entries[index].password});
    }
  }
  preferences.end();
}

void WifiService::saveAll() {
  StoredCredentials blob;
  blob.count = static_cast<uint8_t>(std::min(saved_.size(), kMaximumSavedNetworks));
  for (size_t index = 0; index < blob.count; ++index) {
    std::strncpy(blob.entries[index].ssid, saved_[index].ssid.c_str(),
                 sizeof(blob.entries[index].ssid) - 1);
    std::strncpy(blob.entries[index].password, saved_[index].password.c_str(),
                 sizeof(blob.entries[index].password) - 1);
  }
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, false)) {
    preferences.putBytes(kCredentialsKey, &blob, sizeof(blob));
    preferences.end();
  }
}

const WifiService::SavedNetwork* WifiService::findSaved(
    const std::string& ssid) const {
  for (const auto& item : saved_)
    if (item.ssid == ssid) return &item;
  return nullptr;
}

bool WifiService::hasSaved() const {
  const_cast<WifiService*>(this)->loadSaved();
  return !saved_.empty();
}

bool WifiService::hasSaved(const std::string& ssid) const {
  const_cast<WifiService*>(this)->loadSaved();
  return findSaved(ssid) != nullptr;
}

bool WifiService::beginScan() {
  loadSaved();
  if (state_ == WifiState::Scanning || state_ == WifiState::Connecting) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  WiFi.scanDelete();
  networks_.clear();
  lastError_.clear();
  stateStartedMs_ = millis();
  scanLaunched_ = false;
  scanPhase_ = 1;
  scanRetries_ = 0;
  state_ = WifiState::Scanning;
  Serial.println("M5EPUB_WIFI,event=scan_requested");
  return true;
}

void WifiService::finishScan() {
  const int count = WiFi.scanComplete();
  networks_.clear();
  for (int index = 0; index < count; ++index) {
    const std::string name = WiFi.SSID(index).c_str();
    if (name.empty()) continue;
    auto existing = std::find_if(networks_.begin(), networks_.end(),
        [&name](const WifiNetwork& item) { return item.ssid == name; });
    const int32_t signal = WiFi.RSSI(index);
    if (existing != networks_.end()) {
      if (signal > existing->rssi) existing->rssi = signal;
      continue;
    }
    WifiNetwork network;
    network.ssid = name;
    network.rssi = signal;
    network.secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    network.saved = hasSaved(name);
    networks_.push_back(network);
  }
  std::sort(networks_.begin(), networks_.end(),
      [](const WifiNetwork& left, const WifiNetwork& right) {
        if (left.saved != right.saved) return left.saved > right.saved;
        return left.rssi > right.rssi;
      });
  state_ = WifiState::ScanDone;
  Serial.printf("M5EPUB_WIFI,event=results_ready,visible_count=%u\n",
                static_cast<unsigned>(networks_.size()));
}

bool WifiService::connect(const std::string& ssid,
                          const std::string& password, bool rememberNetwork) {
  if (ssid.empty() || ssid.size() > 32 || password.size() > 64) return false;
  WiFi.scanDelete();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  activeSsid_ = ssid;
  pendingPassword_ = password;
  rememberPending_ = rememberNetwork;
  lastError_.clear();
  stateStartedMs_ = millis();
  state_ = WifiState::Connecting;
  return true;
}

bool WifiService::connectSaved(const std::string& ssid) {
  loadSaved();
  const SavedNetwork* saved = findSaved(ssid);
  return saved && connect(saved->ssid, saved->password, false);
}

void WifiService::remember(const std::string& ssid,
                           const std::string& password) {
  auto found = std::find_if(saved_.begin(), saved_.end(),
      [&ssid](const SavedNetwork& item) { return item.ssid == ssid; });
  if (found != saved_.end()) found->password = password;
  else {
    if (saved_.size() >= kMaximumSavedNetworks) saved_.erase(saved_.begin());
    saved_.push_back({ssid, password});
  }
  saveAll();
}

void WifiService::poll(uint32_t nowMs) {
  if (state_ == WifiState::Scanning) {
    // The ESP32 radio needs a short cooperative settling period after WIFI_OFF
    // -> WIFI_STA. Starting the asynchronous scan in the same call can return
    // WIFI_SCAN_FAILED on the original M5Paper.
    if (!scanLaunched_) {
      if (nowMs - stateStartedMs_ < 1000) return;
      // Passive scanning avoids the intermittent zero-result active scan seen
      // on the original ESP32-D0WDQ6 after M5Unified initializes the board.
      // It remains asynchronous; 500 ms/channel is bounded by our outer timeout.
      const int started = WiFi.scanNetworks(true, false, true, 500);
      if (started == WIFI_SCAN_FAILED) {
        if (scanRetries_++ < 2) {
          WiFi.scanDelete();
          stateStartedMs_ = nowMs;
          scanPhase_ = 1;
          Serial.printf("M5EPUB_WIFI,event=scan_retry,attempt=%u\n",
                        static_cast<unsigned>(scanRetries_));
          return;
        }
        state_ = WifiState::Failed;
        lastError_ = "Wi-Fi scan could not start";
        Serial.println("M5EPUB_WIFI,event=scan_failed,phase=start");
        return;
      }
      scanLaunched_ = true;
      stateStartedMs_ = nowMs;
      Serial.println("M5EPUB_WIFI,event=scan_started,mode=passive");
      return;
    }
    const int result = WiFi.scanComplete();
    if (result > 0) {
      Serial.printf("M5EPUB_WIFI,event=scan_completed,count=%d\n", result);
      finishScan();
    } else if (result == 0) {
      if (scanRetries_++ < 2) {
        WiFi.scanDelete();
        scanLaunched_ = false;
        scanPhase_ = 1;
        stateStartedMs_ = nowMs;
        Serial.printf("M5EPUB_WIFI,event=scan_retry,attempt=%u,reason=empty\n",
                      static_cast<unsigned>(scanRetries_));
      } else {
        state_ = WifiState::Failed;
        lastError_ = "No Wi-Fi networks found";
        Serial.println("M5EPUB_WIFI,event=scan_failed,phase=empty");
      }
    } else if (result == WIFI_SCAN_FAILED) {
      if (scanRetries_++ < 2) {
        WiFi.scanDelete();
        scanLaunched_ = false;
        scanPhase_ = 1;
        stateStartedMs_ = nowMs;
        Serial.printf("M5EPUB_WIFI,event=scan_retry,attempt=%u\n",
                      static_cast<unsigned>(scanRetries_));
      } else {
        state_ = WifiState::Failed;
        lastError_ = "Wi-Fi scan failed";
        Serial.println("M5EPUB_WIFI,event=scan_failed,phase=poll");
      }
    } else if (nowMs - stateStartedMs_ >= app_config::kWifiScanTimeoutMs) {
      WiFi.scanDelete();
      state_ = WifiState::Failed;
      lastError_ = "Wi-Fi scan timed out";
      Serial.println("M5EPUB_WIFI,event=scan_failed,phase=timeout");
    }
  } else if (state_ == WifiState::Connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      state_ = WifiState::Connected;
      connectionLostSinceMs_ = 0;
      if (rememberPending_) remember(activeSsid_, pendingPassword_);
      pendingPassword_.clear();
      rememberPending_ = false;
    } else if (nowMs - stateStartedMs_ >= app_config::kWifiConnectTimeoutMs) {
      lastError_ = "Connection timed out";
      state_ = WifiState::Failed;
      WiFi.disconnect(true, false);
    }
  } else if (state_ == WifiState::Connected) {
    const wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
      if (connectionLostSinceMs_)
        Serial.printf("M5EPUB_WIFI,event=connection_recovered,duration_ms=%lu\n",
                      static_cast<unsigned long>(nowMs - connectionLostSinceMs_));
      connectionLostSinceMs_ = 0;
    } else if (!connectionLostSinceMs_) {
      connectionLostSinceMs_ = nowMs ? nowMs : 1;
      Serial.printf("M5EPUB_WIFI,event=connection_unstable,status=%u\n",
                    static_cast<unsigned>(status));
    } else if (nowMs - connectionLostSinceMs_ >= 10000) {
      state_ = WifiState::Failed;
      lastError_ = "Wi-Fi connection lost";
      Serial.printf("M5EPUB_WIFI,event=connection_lost,status=%u,duration_ms=%lu\n",
                    static_cast<unsigned>(status),
                    static_cast<unsigned long>(nowMs - connectionLostSinceMs_));
    }
  }
}

bool WifiService::forget(const std::string& ssid) {
  loadSaved();
  const auto oldSize = saved_.size();
  saved_.erase(std::remove_if(saved_.begin(), saved_.end(),
      [&ssid](const SavedNetwork& item) { return item.ssid == ssid; }), saved_.end());
  if (saved_.size() == oldSize) return false;
  saveAll();
  return true;
}

void WifiService::forgetAll() {
  loadSaved();
  saved_.clear();
  Preferences preferences;
  if (preferences.begin(kPreferencesNamespace, false)) {
    preferences.remove(kCredentialsKey);
    preferences.end();
  }
}

void WifiService::stop() {
  WiFi.scanDelete();
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  state_ = WifiState::Off;
  scanLaunched_ = false;
  scanPhase_ = 0;
  networks_.clear();
  activeSsid_.clear();
  pendingPassword_.clear();
  rememberPending_ = false;
  connectionLostSinceMs_ = 0;
}

std::string WifiService::ip() const {
  return state_ == WifiState::Connected ? WiFi.localIP().toString().c_str() : "";
}

int32_t WifiService::rssi() const {
  return state_ == WifiState::Connected ? WiFi.RSSI() : 0;
}

#endif
