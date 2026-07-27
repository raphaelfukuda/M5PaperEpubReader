#pragma once

#include <stdint.h>
#include <string>

struct FileEntry {
  FileEntry() = default;
  FileEntry(const std::string& entryName, const std::string& path, bool directory,
            uint64_t entrySize, uint64_t modified = 0)
      : name(entryName), fullPath(path), isDirectory(directory), size(entrySize),
        modifiedTime(modified) {}
  std::string name;
  std::string fullPath;
  bool isDirectory = false;
  uint64_t size = 0;
  uint64_t modifiedTime = 0;
};
