#pragma once
#include <string>
#include "epub/EpubParser.h"
#include "epub/HtmlTokenizer.h"
#include "layout/PageAnchor.h"
#include "layout/TextLayoutEngine.h"
#include "storage/DirectoryScanner.h"
#include "reader/PrefetchStateMachine.h"

enum class CanvasState : uint8_t { Free, Rendering, Ready, Submitting, Displayed };
enum class PagePresentationSource : uint8_t {
  ExistingFrontBuffer, ReadyBackBuffer, TextCacheRender, CheckpointRebuild
};

class ReaderController {
 public:
  ReaderController(EpubParser& parser, M5Canvas& canvas)
      : parser_(parser), initialCanvas_(canvas), pageCanvas_(&canvas) {}
  bool start();
  bool startAt(const PageAnchor& anchor, uint16_t fontSize);
  bool startAt(const PageAnchor& anchor, uint16_t fontSize, uint32_t pageNumber,
               const std::vector<PageAnchor>& previousPages);
  bool startAtSpine(uint32_t spineIndex);
  WorkResult processNextChunk();
  WorkResult readInputChunk();
  WorkResult processBufferedInput(uint32_t budgetUs);
  WorkResult continueLayout(uint32_t budgetUs);
  uint32_t processCpuOnlyWork(uint32_t budgetUs);
  bool hasCpuOnlyWork() const {
    return inputReady_ || (layoutPending_ && !inputEnded_);
  }
  WorkResult requestNextPage();
  WorkResult requestPreviousPage();
  WorkResult requestPrefetch();
  PrefetchCancelResult cancelPrefetch();
  void invalidateReadyPrefetch();
  bool hasReadyPrefetchedPage() const { return prefetchState_.ready(); }
  bool hasPrefetchWork() const { return prefetchState_.hasWork(); }
  WorkResult increaseFontSize();
  WorkResult decreaseFontSize();
  uint16_t fontSize() const { return settings_.fontSize; }
  bool redrawCurrentPage();
  PageAnchor currentAnchor() const;
  bool isPrefetching() const { return prefetching_; }
  PrefetchState prefetchState() const { return prefetchState_.state(); }
  uint32_t pageNumber() const {
    return pageNumberBase_ + static_cast<uint32_t>(currentPage_ + 1);
  }
  std::vector<PageAnchor> previousPageAnchors(size_t maximum) const;
  const std::string& error() const { return error_; }
  const EpubBook& book() const { return parser_.book(); }
  M5Canvas& presentationCanvas() { return *pageCanvas_; }
  PagePresentationSource presentationSource() const { return presentationSource_; }
 private:
  EpubParser& parser_; M5Canvas& initialCanvas_; M5Canvas* pageCanvas_; M5Canvas* freeCanvas_ = nullptr; HtmlTokenizer html_; TextLayoutEngine layout_; ReaderSettings settings_;
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
  bool restorePrefetchCheckpoint();
  std::string chapterPath_, error_, pendingText_, workingText_;
  std::vector<std::string> visitedPages_;
  std::vector<PageAnchor> pageAnchors_;
  PageAnchor workingAnchor_;
  PageAnchor prefetchCheckpoint_;
  PrefetchStateMachine prefetchState_;
  M5Canvas prefetchCanvas_{&M5.Display};
  CanvasState pageCanvasState_ = CanvasState::Displayed;
  CanvasState freeCanvasState_ = CanvasState::Free;
  PagePresentationSource presentationSource_ = PagePresentationSource::ExistingFrontBuffer;
  size_t currentPage_ = 0, cachedBytes_ = 0, spineIndex_ = 0;
  uint32_t pageNumberBase_ = 0;
  uint64_t offset_ = 0;
  uint32_t seekTextBytes_ = 0;
  uint8_t inputBuffer_[1024]{};
  size_t inputLength_ = 0;
  bool inputReady_ = false, inputEnded_ = false, layoutPending_ = false;
  size_t rebuildingPage_ = static_cast<size_t>(-1);
  bool active_ = false, chapterEnded_ = false, prefetching_ = false;
  bool prefetchCanvasReady_ = false, endOfBook_ = false;
};
