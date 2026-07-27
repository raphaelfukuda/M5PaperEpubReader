#pragma once
#include <FS.h>
#include <string>
#include <unzipLIB.h>
#include "hal/SpiBusGuard.h"

class EpubArchive {
 public:
  explicit EpubArchive(SpiBusGuard& guard) : busGuard_(guard) {}
  bool open(const std::string& path);
  void close();
  bool readEntry(const std::string& entryPath, size_t maximumBytes, std::string& output);
  bool entryUncompressedSize(const std::string& entryPath, uint64_t& size);
  bool beginEntry(const std::string& entryPath, uint64_t maximumBytes);
  int readEntryChunk(uint8_t* buffer, size_t length);
  bool endEntry();
  bool cancelEntry();
  const std::string& error() const { return error_; }
 private:
  static void* openCallback(const char* filename, int32_t* size);
  static void closeCallback(void* context);
  static int32_t readCallback(void* context, uint8_t* buffer, int32_t length);
  static int32_t seekCallback(void* context, int32_t position, int origin);
  SpiBusGuard& busGuard_;
  UNZIP zip_;
  std::string path_;
  std::string error_;
  bool opened_ = false;
  bool entryOpen_ = false;
  uint64_t entrySize_ = 0;
  uint64_t entryRead_ = 0;
};
