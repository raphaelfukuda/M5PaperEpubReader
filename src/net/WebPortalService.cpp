#include "net/WebPortalService.h"

#if M5EPUB_ENABLE_WEB_PORTAL

#include "net/PortalPath.h"
#include "net/PortalPage.h"
#include <Arduino.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cctype>

namespace {
constexpr uint64_t kFreeSpaceMargin = 64ULL * 1024ULL;
constexpr uint64_t kProgressInterval = 128ULL * 1024ULL;

uint64_t sdFreeBytes() {
  const uint64_t total = SD.totalBytes();
  const uint64_t used = SD.usedBytes();
  return used < total ? total - used : 0;
}

String jsonEscape(const char* value) {
  String result;
  for (const char* current = value; current && *current; ++current) {
    const char ch = *current;
    if (ch == '"' || ch == '\\') { result += '\\'; result += ch; }
    else if (static_cast<uint8_t>(ch) >= 0x20) result += ch;
  }
  return result;
}
}

WebPortalService::WebPortalService(fs::FS& filesystem) : filesystem_(filesystem) {}
WebPortalService::~WebPortalService() { end(); }

bool WebPortalService::begin(const Hooks& hooks, const std::string& root) {
  if (running_ || !hooks.acquireSd || !hooks.releaseSd) return false;
  std::string resolved;
  if (!portal_path::resolve("/", root, resolved)) return false;
  hooks_ = hooks;
  root_ = resolved;
  const char* headers[] = {"X-Dest-Path", "X-File-Size"};
  server_.collectHeaders(headers, 2);
  server_.on("/", HTTP_GET, [this]() { routeRoot(); });
  server_.on("/api/list", HTTP_GET, [this]() { routeList(); });
  server_.on("/api/status", HTTP_GET, [this]() { routeStatus(); });
  server_.on("/api/mkdir", HTTP_POST, [this]() { routeMkdir(); });
  server_.on("/api/delete", HTTP_POST, [this]() { routeDelete(); });
  server_.on("/api/upload", HTTP_POST,
             [this]() { routeUploadComplete(); },
             [this]() { routeUploadData(); });
  server_.onNotFound([this]() { sendJsonError(404, "not found"); });
  server_.begin();
  running_ = true;
  lastRequestMs_ = millis();
  status_ = {};
  status_.activity = PortalActivity::Started;
  return true;
}

void WebPortalService::poll() {
  if (running_) server_.handleClient();
}

void WebPortalService::end() {
  if (!running_) return;
  if (uploadGuardHeld_) abortUpload("portal stopped");
  server_.stop();
  running_ = false;
  status_.activity = PortalActivity::Stopped;
}

bool WebPortalService::acquire() {
  if (uploadGuardHeld_) return true;
  if (!hooks_.acquireSd()) return false;
  uploadGuardHeld_ = true;
  return true;
}

void WebPortalService::release() {
  if (!uploadGuardHeld_) return;
  uploadGuardHeld_ = false;
  hooks_.releaseSd();
}

void WebPortalService::notify(PortalActivity activity,
                              const std::string& message) {
  status_.activity = activity;
  status_.message = message;
  if (hooks_.onActivity) hooks_.onActivity(status_);
}

bool WebPortalService::resolveRequestPath(const String& value,
                                          std::string& result) {
  std::string error;
  if (portal_path::resolve(root_, value.c_str(), result, &error)) return true;
  sendJsonError(400, error);
  return false;
}

void WebPortalService::sendJsonError(int code, const std::string& message) {
  server_.send(code, "application/json",
               String("{\"error\":\"") + jsonEscape(message.c_str()) + "\"}");
}

void WebPortalService::routeRoot() {
  lastRequestMs_ = millis();
  server_.sendHeader("Cache-Control", "no-store");
  server_.send_P(200, "text/html; charset=utf-8", kPortalPage);
}

void WebPortalService::routeList() {
  lastRequestMs_ = millis();
  std::string path;
  if (!resolveRequestPath(server_.arg("path"), path)) return;
  if (!acquire()) { sendJsonError(503, "SD card is busy"); return; }
  fs::File directory = filesystem_.open(path.c_str());
  if (!directory || !directory.isDirectory()) {
    directory.close(); release(); sendJsonError(404, "folder not found"); return;
  }
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "application/json", "");
  server_.sendContent(String("{\"path\":\"") + jsonEscape(path.c_str()) +
      "\",\"freeKiB\":" + String(sdFreeBytes() / 1024ULL) +
      ",\"totalKiB\":" + String(SD.totalBytes() / 1024ULL) + ",\"entries\":[");
  bool first = true;
  fs::File entry;
  while ((entry = directory.openNextFile())) {
    const char* rawName = entry.name();
    const char* name = strrchr(rawName, '/');
    name = name ? name + 1 : rawName;
    if (name[0] != '.') {
      String chunk = first ? "" : ",";
      first = false;
      chunk += String("{\"name\":\"") + jsonEscape(name) +
          "\",\"directory\":" + (entry.isDirectory() ? "true" : "false") +
          ",\"size\":" + String(static_cast<uint64_t>(entry.size())) + "}";
      server_.sendContent(chunk);
    }
    entry.close();
    yield();
  }
  directory.close();
  notify(PortalActivity::Request);
  release();
  server_.sendContent("]}");
  server_.sendContent("");
}

void WebPortalService::routeStatus() {
  lastRequestMs_ = millis();
  if (!acquire()) { sendJsonError(503, "SD card is busy"); return; }
  String body = String("{\"root\":\"") + jsonEscape(root_.c_str()) +
      "\",\"ssid\":\"" + jsonEscape(WiFi.SSID().c_str()) +
      "\",\"ip\":\"" + WiFi.localIP().toString() +
      "\",\"rssi\":" + String(WiFi.RSSI()) +
      ",\"freeKiB\":" + String(sdFreeBytes() / 1024ULL) +
      ",\"totalKiB\":" + String(SD.totalBytes() / 1024ULL) +
      ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
  notify(PortalActivity::Request);
  release();
  server_.send(200, "application/json", body);
}

void WebPortalService::routeMkdir() {
  lastRequestMs_ = millis();
  std::string parent;
  if (!resolveRequestPath(server_.arg("path"), parent)) return;
  const std::string name = portal_path::sanitizeName(server_.arg("name").c_str());
  std::string target;
  if (!portal_path::resolve(parent, name, target)) { sendJsonError(400, "invalid folder name"); return; }
  if (!acquire()) { sendJsonError(503, "SD card is busy"); return; }
  const bool ok = filesystem_.exists(target.c_str()) || filesystem_.mkdir(target.c_str());
  status_.path = parent;
  notify(ok ? PortalActivity::FilesChanged : PortalActivity::UploadFailed,
         ok ? "folder ready" : "cannot create folder");
  release();
  if (ok) server_.send(200, "application/json", "{\"ok\":true}");
  else sendJsonError(500, "cannot create folder");
}

void WebPortalService::routeDelete() {
  lastRequestMs_ = millis();
  std::string target;
  if (!resolveRequestPath(server_.arg("path"), target)) return;
  if (target == root_) { sendJsonError(400, "cannot delete root"); return; }
  if (!acquire()) { sendJsonError(503, "SD card is busy"); return; }
  fs::File item = filesystem_.open(target.c_str());
  bool ok = false;
  if (item) {
    if (item.isDirectory()) {
      fs::File child = item.openNextFile();
      const bool empty = !child;
      child.close(); item.close();
      if (empty) ok = filesystem_.rmdir(target.c_str());
    } else { item.close(); ok = filesystem_.remove(target.c_str()); }
  }
  status_.path = target;
  notify(ok ? PortalActivity::FilesChanged : PortalActivity::UploadFailed,
         ok ? "item deleted" : "folder is not empty or item is missing");
  release();
  if (ok) server_.send(200, "application/json", "{\"ok\":true}");
  else sendJsonError(409, "folder is not empty or item is missing");
}

void WebPortalService::abortUpload(const std::string& reason) {
  uploadFailed_ = true;
  if (uploadFile_) uploadFile_.close();
  if (!uploadPartPath_.empty()) filesystem_.remove(uploadPartPath_.c_str());
  notify(PortalActivity::UploadFailed, reason);
  release();
}

void WebPortalService::routeUploadData() {
  HTTPUpload& upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    lastRequestMs_ = millis();
    uploadFailed_ = false;
    uploadResponseSent_ = false;
    writtenUploadBytes_ = lastReportedBytes_ = 0;
    expectedUploadBytes_ = strtoull(server_.header("X-File-Size").c_str(), nullptr, 10);
    std::string destination;
    if (!resolveRequestPath(server_.header("X-Dest-Path"), destination)) {
      uploadFailed_ = true; uploadResponseSent_ = true; return;
    }
    if (!acquire()) { uploadFailed_ = true; return; }
    if (expectedUploadBytes_ == 0 || expectedUploadBytes_ + kFreeSpaceMargin > sdFreeBytes()) {
      abortUpload("not enough free space"); return;
    }
    const std::string name = portal_path::sanitizeName(upload.filename.c_str());
    std::string extension = name.size() >= 5 ? name.substr(name.size() - 5) : "";
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    if (extension != ".epub") {
      abortUpload("only .epub files are accepted"); return;
    }
    if (!portal_path::resolve(destination, name, uploadFinalPath_)) {
      abortUpload("invalid destination"); return;
    }
    uploadPartPath_ = uploadFinalPath_ + ".part";
    filesystem_.remove(uploadPartPath_.c_str());
    uploadFile_ = filesystem_.open(uploadPartPath_.c_str(), FILE_WRITE);
    if (!uploadFile_) { abortUpload("cannot create temporary file"); return; }
    status_.path = destination;
    status_.fileName = name;
    status_.completedBytes = 0;
    status_.totalBytes = expectedUploadBytes_;
    notify(PortalActivity::UploadStarted);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFailed_ || !uploadGuardHeld_) return;
    const size_t written = uploadFile_.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) { abortUpload("SD write failed"); return; }
    writtenUploadBytes_ += written;
    if (writtenUploadBytes_ - lastReportedBytes_ >= kProgressInterval) {
      lastReportedBytes_ = writtenUploadBytes_;
      status_.completedBytes = writtenUploadBytes_;
      notify(PortalActivity::UploadProgress);
    }
    yield();
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFailed_ || !uploadGuardHeld_) return;
    uploadFile_.close();
    if (writtenUploadBytes_ != expectedUploadBytes_) {
      abortUpload("uploaded size does not match header"); return;
    }
    if (filesystem_.exists(uploadFinalPath_.c_str())) filesystem_.remove(uploadFinalPath_.c_str());
    if (!filesystem_.rename(uploadPartPath_.c_str(), uploadFinalPath_.c_str())) {
      abortUpload("cannot finalize upload"); return;
    }
    status_.completedBytes = writtenUploadBytes_;
    notify(PortalActivity::UploadCompleted);
    release();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadGuardHeld_) abortUpload("upload aborted");
    else uploadFailed_ = true;
  }
}

void WebPortalService::routeUploadComplete() {
  lastRequestMs_ = millis();
  if (uploadResponseSent_) return;
  uploadResponseSent_ = true;
  if (uploadFailed_) sendJsonError(400, status_.message.empty() ? "upload failed" : status_.message);
  else server_.send(201, "application/json", "{\"ok\":true}");
}

#endif
