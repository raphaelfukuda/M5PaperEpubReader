#pragma once

#include <FS.h>
#include <string>
#include "FileEntry.h"
#include "hal/SpiBusGuard.h"

class LibraryThumbnailCache {
 public:
  LibraryThumbnailCache(fs::FS& fs, SpiBusGuard& guard) : fs_(fs), guard_(guard) {}

  bool load(const FileEntry& book, std::string& title, uint16_t& width,
            uint16_t& height, std::string& pixels);
  bool save(const FileEntry& book, const std::string& title, uint16_t width,
            uint16_t height, const std::string& pixels);

 private:
  std::string cachePath(const std::string& bookPath) const;
  fs::FS& fs_;
  SpiBusGuard& guard_;
};
