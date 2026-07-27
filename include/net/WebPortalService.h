#pragma once

#include "AppConfig.h"

#if M5EPUB_ENABLE_WEB_PORTAL

#include <FS.h>
#include <WebServer.h>
#include <functional>
#include <stdint.h>
#include <string>

enum class PortalActivity : uint8_t {
  Started,
  Request,
  UploadStarted,
  UploadProgress,
  UploadCompleted,
  UploadFailed,
  FilesChanged,
  Stopped
};

struct PortalStatus {
  PortalActivity activity = PortalActivity::Stopped;
  std::string path;
  std::string fileName;
  std::string message;
  uint64_t completedBytes = 0;
  uint64_t totalBytes = 0;
};

class WebPortalService {
 public:
  struct Hooks {
    std::function<bool()> acquireSd;
    std::function<void()> releaseSd;
    // Invoked while the SD guard is held. The receiver must only copy status
    // and mark UI state dirty; it must never access SD or the display here.
    std::function<void(const PortalStatus&)> onActivity;
  };

  explicit WebPortalService(fs::FS& filesystem);
  ~WebPortalService();
  bool begin(const Hooks& hooks, const std::string& root = "/",
             bool portuguese = false);
  void poll();
  void end();
  bool running() const { return running_; }
  bool uploadActive() const { return uploadGuardHeld_; }
  uint32_t idleMs(uint32_t nowMs) const {
    const uint32_t elapsed = nowMs - lastRequestMs_;
    // A request handled in poll() can update lastRequestMs_ a few milliseconds
    // after the AppController captured its tick timestamp. Treat that ordering
    // as zero idle time instead of unsigned underflow (~49 days).
    return elapsed > 0x7FFFFFFFUL ? 0 : elapsed;
  }
  const PortalStatus& status() const { return status_; }

 private:
  bool acquire();
  void release();
  void notify(PortalActivity activity, const std::string& message = {});
  bool resolveRequestPath(const String& value, std::string& result);
  void sendJsonError(int code, const std::string& message);
  void routeRoot();
  void routeList();
  void routeStatus();
  void routeMkdir();
  void routeDelete();
  void routeUploadComplete();
  void routeUploadData();
  void abortUpload(const std::string& reason);

  fs::FS& filesystem_;
  WebServer server_{80};
  Hooks hooks_;
  std::string root_ = "/";
  std::string uploadFinalPath_;
  std::string uploadPartPath_;
  fs::File uploadFile_;
  PortalStatus status_;
  bool running_ = false;
  bool uploadGuardHeld_ = false;
  bool uploadFailed_ = false;
  bool uploadResponseSent_ = false;
  bool portuguese_ = false;
  uint64_t expectedUploadBytes_ = 0;
  uint64_t writtenUploadBytes_ = 0;
  uint64_t lastReportedBytes_ = 0;
  uint32_t lastRequestMs_ = 0;
};

#endif
