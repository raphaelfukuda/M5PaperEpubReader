#pragma once

#include <FS.h>
#include <string>
#include <vector>
#include "FileEntry.h"
#include "hal/SpiBusGuard.h"

enum class WorkResult { Idle, MoreWork, Completed, Failed };

class DirectoryScanner {
 public:
  explicit DirectoryScanner(SpiBusGuard& busGuard) : busGuard_(busGuard) {}
  bool start(const std::string& path);
  WorkResult processNextBatch();
  const std::vector<FileEntry>& entries() const { return entries_; }
  const std::string& path() const { return path_; }
  const std::string& error() const { return error_; }
  bool isRunning() const { return running_; }
  bool wasTruncated() const { return truncated_; }

 private:
  void finish();
  SpiBusGuard& busGuard_;
  fs::File directory_;
  std::vector<FileEntry> entries_;
  std::string path_ = "/";
  std::string error_;
  bool running_ = false;
  bool truncated_ = false;
};

