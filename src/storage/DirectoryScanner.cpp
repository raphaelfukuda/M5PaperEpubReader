#include "DirectoryScanner.h"

#include <SD.h>
#include <algorithm>
#include "AppConfig.h"
#include "PathUtils.h"

bool DirectoryScanner::start(const std::string& path) {
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard);
  if (!bus) return false;
  directory_.close();
  entries_.clear();
  entries_.reserve(32);
  error_.clear();
  truncated_ = false;
  path_ = path.empty() ? "/" : path;
  directory_ = SD.open(path_.c_str());
  if (!directory_ || !directory_.isDirectory()) {
    error_ = "Diretorio inacessivel: " + path_;
    directory_.close();
    running_ = false;
    return false;
  }
  running_ = true;
  return true;
}

WorkResult DirectoryScanner::processNextBatch() {
  if (!running_) return error_.empty() ? WorkResult::Idle : WorkResult::Failed;
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard);
  if (!bus) return WorkResult::MoreWork;
  for (uint8_t i = 0; i < app_config::kDirectoryBatchSize; ++i) {
    fs::File file = directory_.openNextFile();
    if (!file) {
      finish();
      return WorkResult::Completed;
    }
    const std::string fullPath = file.path();
    const std::string name = path_utils::fileName(fullPath);
    const bool directory = file.isDirectory();
    if (!path_utils::isHiddenName(name) && (directory || path_utils::hasEpubExtension(name))) {
      if (entries_.size() < app_config::kMaxDirectoryEntries) {
        entries_.push_back({name, fullPath, directory, directory ? 0 : file.size(),
                            static_cast<uint64_t>(file.getLastWrite())});
      } else {
        truncated_ = true;
      }
    }
    file.close();
  }
  return WorkResult::MoreWork;
}

void DirectoryScanner::finish() {
  directory_.close();
  running_ = false;
  std::sort(entries_.begin(), entries_.end(), [](const FileEntry& a, const FileEntry& b) {
    if (a.isDirectory != b.isDirectory) return a.isDirectory;
    return path_utils::compareCaseInsensitive(a.name, b.name) < 0;
  });
}
