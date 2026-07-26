#pragma once

#include <FS.h>
#include <string>
#include "ReadingState.h"

class ReadingStateStore {
 public:
  explicit ReadingStateStore(fs::FS& filesystem) : filesystem_(filesystem) {}
  bool save(const std::string& path, const ReadingState& state);
  bool load(const std::string& path, ReadingState& state);
  bool remove(const std::string& path);
  const std::string& error() const { return error_; }
 private:
  fs::FS& filesystem_;
  std::string error_;
};
