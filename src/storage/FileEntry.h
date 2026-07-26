#pragma once

#include <stdint.h>
#include <string>

struct FileEntry {
  FileEntry() = default;
  FileEntry(const std::string& entryName, const std::string& path, bool directory,
            uint64_t entrySize)
      : name(entryName), fullPath(path), isDirectory(directory), size(entrySize) {}
  std::string name;
  std::string fullPath;
  bool isDirectory = false;
  uint64_t size = 0;
};
