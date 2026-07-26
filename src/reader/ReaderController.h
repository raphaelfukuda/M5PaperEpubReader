#pragma once
#include <string>
#include "epub/EpubParser.h"
#include "epub/HtmlTokenizer.h"
#include "layout/PageAnchor.h"
#include "layout/TextLayoutEngine.h"
#include "storage/DirectoryScanner.h"

class ReaderController {
 public:
  ReaderController(EpubParser& parser, M5Canvas& canvas) : parser_(parser), canvas_(canvas) {}
  bool start();
  bool startAt(const PageAnchor& anchor, uint16_t fontSize);
  bool startAtSpine(uint32_t spineIndex);
  WorkResult processNextChunk();
  WorkResult requestNextPage();
  WorkResult requestPreviousPage();
  WorkResult requestPrefetch();
  WorkResult increaseFontSize();
  WorkResult decreaseFontSize();
  uint16_t fontSize() const { return settings_.fontSize; }
  bool redrawCurrentPage();
  PageAnchor currentAnchor() const;
  bool isPrefetching() const { return prefetching_; }
  uint32_t pageNumber() const { return static_cast<uint32_t>(currentPage_ + 1); }
  const std::string& error() const { return error_; }
  const EpubBook& book() const { return parser_.book(); }
 private:
  EpubParser& parser_; M5Canvas& canvas_; HtmlTokenizer html_; TextLayoutEngine layout_; ReaderSettings settings_;
  void beginBlankPage(M5Canvas& target);
  void renderCachedPage(size_t index);
  bool commitPage();
  bool validCacheState() const;
  WorkResult failReader(const std::string& message);
  bool openNextLinearSpine();
  bool openLinearSpineAt(uint32_t index);
  WorkResult rebuildPage(size_t index);
  WorkResult prepareUncachedPage(M5Canvas& target);
  WorkResult reflowCurrentPage(uint16_t newSize);
  bool resetSession();
  std::string chapterPath_, error_, pendingText_, workingText_;
  std::vector<std::string> visitedPages_;
  std::vector<PageAnchor> pageAnchors_;
  PageAnchor workingAnchor_;
  M5Canvas prefetchCanvas_{&M5.Display};
  size_t currentPage_ = 0, cachedBytes_ = 0, spineIndex_ = 0;
  uint64_t offset_ = 0;
  uint32_t seekTextBytes_ = 0;
  size_t rebuildingPage_ = static_cast<size_t>(-1);
  bool active_ = false, chapterEnded_ = false, prefetching_ = false;
  bool prefetchCanvasReady_ = false, endOfBook_ = false;
};
