#pragma once
#include <functional>
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
               const std::vector<PageAnchor>& previousPages,
               ReaderFontFamily fontFamily = ReaderFontFamily::Compact);
  bool startAtSpine(uint32_t spineIndex);
  WorkResult processNextChunk();
  WorkResult readInputChunk();
  WorkResult processBufferedInput(uint32_t budgetUs);
  WorkResult continueLayout(uint32_t budgetUs);
  uint32_t processCpuOnlyWork(uint32_t budgetUs);
  bool hasCpuOnlyWork() const {
    return inputReady_ || (layoutPending_ && !inputEnded_ && !imagePreparationPending_);
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
  ReaderFontFamily fontFamily() const { return settings_.fontFamily; }
  WorkResult cycleFontFamily();
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
  M5Canvas& presentationCanvas() { return presentationCanvas_ ? *presentationCanvas_ : *pageCanvas_; }
  bool pageContainsImages() const { return pageContainsImages_; }
  void setHighQualityCanvasProvider(std::function<M5Canvas&()> provider) {
    highQualityCanvasProvider_ = std::move(provider);
  }
  PagePresentationSource presentationSource() const { return presentationSource_; }
 private:
  EpubParser& parser_; M5Canvas& initialCanvas_; M5Canvas* pageCanvas_; M5Canvas* freeCanvas_ = nullptr; HtmlTokenizer html_; TextLayoutEngine layout_; ReaderSettings settings_;
  void beginBlankPage(M5Canvas& target, bool preserveInlineImages = false);
  void renderCachedPage(size_t index, bool preserveInlineImages = false);
  bool prepareInlineImages(const std::string& text);
  bool finalizePagePresentation(bool allowHighQuality = true);
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
  struct InlineImageResource {
    std::string source, path, mediaType, data;
    uint32_t width = 0, height = 0;
  };
  const InlineImageResource* findInlineImage(const std::string& source) const;
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
  bool imagePreparationPending_ = false;
  size_t rebuildingPage_ = static_cast<size_t>(-1);
  bool active_ = false, chapterEnded_ = false, prefetching_ = false;
  bool prefetchCanvasReady_ = false, endOfBook_ = false;
  bool pageContainsImages_ = false, prefetchedPageContainsImages_ = false;
  M5Canvas* presentationCanvas_ = nullptr;
  std::function<M5Canvas&()> highQualityCanvasProvider_;
  std::vector<InlineImageResource> inlineImages_;
};
