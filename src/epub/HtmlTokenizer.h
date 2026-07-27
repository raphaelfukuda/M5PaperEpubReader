#pragma once
#include <cstdint>
#include <string>

class HtmlTokenizer {
 public:
  void reset();
  std::string feed(const uint8_t* data, size_t length, bool finalChunk);
 private:
  void finishTag(std::string& output);
  void finishEntity(std::string& output);
  bool inTag_ = false;
  bool inEntity_ = false;
  bool skipping_ = false;
  std::string tag_;
  std::string entity_;
  std::string utf8Pending_;
  char layoutBoundary_ = 0;
};
