#pragma once
#include <string>
#include "EpubArchive.h"
#include "EpubBook.h"
#include "EpubTableOfContents.h"
#include "storage/DirectoryScanner.h"

class EpubParser {
 public:
  explicit EpubParser(SpiBusGuard& guard) : archive_(guard) {}
  bool start(const std::string& filePath);
  WorkResult processNextChunk();
  bool metadataReady() const;
  const EpubBook& book() const { return book_; }
  const std::string& error() const { return error_; }
  const std::string& tocError() const { return tocError_; }
  EpubArchive& archive() { return archive_; }
 private:
  enum class Phase { Idle, OpenArchive, ReadContainer, ParseContainer, ReadOpf,
                     ParseOpf, MeasureSpine, ReadToc, ParseToc, Done, Failed };
  bool parseContainer();
  bool parseOpf();
  void fail(const std::string& message);
  EpubArchive archive_;
  EpubBook book_;
  std::string buffer_;
  std::string error_;
  std::string tocError_;
  EpubTocDocument tocDocument_;
  size_t measureSpineIndex_ = 0;
  Phase phase_ = Phase::Idle;
};
