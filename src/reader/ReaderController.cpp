#include "ReaderController.h"
#include "AppConfig.h"
#include <algorithm>
#include "display/CanvasMemoryPolicy.h"
#include <esp_heap_caps.h>

bool ReaderController::resetSession() {
  if (active_ || !chapterEnded_) parser_.archive().cancelEntry();
  active_ = false; error_.clear(); chapterPath_.clear(); pendingText_.clear(); workingText_.clear(); visitedPages_.clear(); pageAnchors_.clear(); workingAnchor_ = {}; prefetchCheckpoint_ = {}; prefetchState_.reset(); cachedBytes_ = 0; currentPage_ = 0; pageNumberBase_ = 0; spineIndex_ = 0; offset_ = 0; seekTextBytes_ = 0; inputLength_ = 0; inputReady_ = inputEnded_ = layoutPending_ = false; rebuildingPage_ = static_cast<size_t>(-1); chapterEnded_ = true; prefetching_ = false; endOfBook_ = false;
  if (!prefetchCanvasReady_) {
    prefetchCanvas_.setColorDepth(1);
    const size_t requested = static_cast<size_t>((initialCanvas_.width() + 7) / 8) *
                             static_cast<size_t>(initialCanvas_.height());
    const CanvasMemoryStats memory{heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                                   heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                                   ESP.getFreePsram()};
#if M5EPUB_CANVAS_MEMORY_INTERNAL
    constexpr CanvasMemoryPreference preference = CanvasMemoryPreference::InternalRam;
#elif M5EPUB_CANVAS_MEMORY_PSRAM
    constexpr CanvasMemoryPreference preference = CanvasMemoryPreference::Psram;
#else
    constexpr CanvasMemoryPreference preference = CanvasMemoryPreference::Auto;
#endif
    const CanvasMemoryKind selected = chooseCanvasMemory(
        preference, requested, app_config::kInternalHeapSafetyMargin, memory);
    prefetchCanvas_.setPsram(selected == CanvasMemoryKind::Psram);
    prefetchCanvasReady_ = prefetchCanvas_.createSprite(initialCanvas_.width(), initialCanvas_.height()) != nullptr;
    if (!prefetchCanvasReady_) Serial.println("M5EPUB_BUFFER,double=0,reason=allocation_failed");
    Serial.printf("M5EPUB_MEMORY,canvas=back,requested=%u,selected=%s,success=%u,internal_free=%u,internal_largest=%u,psram_free=%u\n",
                  static_cast<unsigned>(requested), canvasMemoryKindName(selected),
                  prefetchCanvasReady_ ? 1U : 0U,
                  static_cast<unsigned>(memory.internalFree),
                  static_cast<unsigned>(memory.internalLargestBlock),
                  static_cast<unsigned>(memory.psramFree));
  }
  pageCanvas_ = &initialCanvas_;
  freeCanvas_ = prefetchCanvasReady_ ? &prefetchCanvas_ : nullptr;
  pageCanvasState_ = CanvasState::Displayed;
  freeCanvasState_ = CanvasState::Free;
  presentationSource_ = PagePresentationSource::ExistingFrontBuffer;
  return true;
}

bool ReaderController::start() {
  if (!resetSession()) return false;
  if (!openNextLinearSpine()) { if (error_.empty()) error_ = "Spine sem capitulo linear"; return false; }
  beginBlankPage(*pageCanvas_); active_ = true; return true;
}

bool ReaderController::startAt(const PageAnchor& anchor, uint16_t fontSize) {
  const std::vector<PageAnchor> noHistory;
  return startAt(anchor, fontSize, 1, noHistory);
}

bool ReaderController::startAt(const PageAnchor& anchor, uint16_t fontSize,
                               uint32_t pageNumber,
                               const std::vector<PageAnchor>& previousPages) {
  if (!resetSession()) return false;
  if (fontSize != 16 && fontSize != 24 && fontSize != 32 && fontSize != 36 && fontSize != 40) {
    error_ = "Tamanho de fonte salvo invalido"; return false;
  }
  settings_.fontSize = fontSize;
  if (!openLinearSpineAt(anchor.spineIndex)) {
    if (error_.empty()) error_ = "Posicao salva fora do spine";
    return false;
  }
  workingAnchor_ = anchor;
  seekTextBytes_ = anchor.parserCheckpoint;
  pageAnchors_ = previousPages;
  visitedPages_.resize(previousPages.size());
  const uint32_t restoredIndex = static_cast<uint32_t>(previousPages.size());
  pageNumberBase_ = pageNumber > restoredIndex + 1
                        ? pageNumber - restoredIndex - 1
                        : 0;
  beginBlankPage(*pageCanvas_); active_ = true; return true;
}

bool ReaderController::startAtSpine(uint32_t spineIndex) {
  PageAnchor anchor;
  anchor.spineIndex = spineIndex;
  return startAt(anchor, settings_.fontSize);
}

bool ReaderController::openLinearSpineAt(uint32_t index) {
  const EpubBook& book = parser_.book();
  if (index >= book.spine.size()) return false;
  const EpubSpineItem& spine = book.spine[index];
  if (!spine.linear) return false;
  chapterPath_.clear();
  for (const auto& item : book.manifest)
    if (item.id == spine.idref && item.mediaType == "application/xhtml+xml") { chapterPath_ = item.href; break; }
  if (chapterPath_.empty()) return false;
  if (!parser_.archive().beginEntry(chapterPath_, 64ULL * 1024 * 1024)) { error_ = parser_.archive().error(); return false; }
  html_.reset(); pendingText_.clear(); inputLength_ = 0; inputReady_ = inputEnded_ = layoutPending_ = false; offset_ = 0; chapterEnded_ = false; endOfBook_ = false;
  spineIndex_ = index + 1;
  workingAnchor_.spineIndex = index;
  workingAnchor_.uncompressedOffset = 0;
  workingAnchor_.parserCheckpoint = 0;
  return true;
}

bool ReaderController::openNextLinearSpine() {
  const EpubBook& book = parser_.book(); chapterPath_.clear();
  while (spineIndex_ < book.spine.size()) { const EpubSpineItem& spine = book.spine[spineIndex_++]; if (!spine.linear) continue; for (const auto& item : book.manifest) if (item.id == spine.idref && item.mediaType == "application/xhtml+xml") { chapterPath_ = item.href; break; } if (!chapterPath_.empty()) break; }
  if (chapterPath_.empty()) { endOfBook_ = true; return false; }
  if (!parser_.archive().beginEntry(chapterPath_, 64ULL * 1024 * 1024)) { error_ = parser_.archive().error(); return false; }
  html_.reset(); pendingText_.clear(); inputLength_ = 0; inputReady_ = inputEnded_ = layoutPending_ = false; offset_ = 0; chapterEnded_ = false;
  workingAnchor_.spineIndex = static_cast<uint32_t>(spineIndex_ - 1);
  workingAnchor_.uncompressedOffset = 0;
  workingAnchor_.parserCheckpoint = 0;
  return true;
}

void ReaderController::beginBlankPage(M5Canvas& target) { layout_.begin(target, settings_); workingText_.clear(); }
bool ReaderController::validCacheState() const { return visitedPages_.size() == pageAnchors_.size() && (visitedPages_.empty() || currentPage_ < visitedPages_.size()); }

WorkResult ReaderController::failReader(const std::string& message) {
  error_ = message.empty() ? "Falha de leitura sem detalhe" : message;
  active_ = false; prefetching_ = false; rebuildingPage_ = static_cast<size_t>(-1);
  parser_.archive().endEntry();
  return WorkResult::Failed;
}

bool ReaderController::commitPage() {
  if (!validCacheState()) { error_ = "Cache de paginas inconsistente"; return false; }
  if (rebuildingPage_ == static_cast<size_t>(-1) && visitedPages_.size() >= app_config::kMaxPageHistoryEntries) { error_ = "Historico de paginas excede limite"; return false; }
  cachedBytes_ += workingText_.size();
  if (rebuildingPage_ != static_cast<size_t>(-1)) {
    for (size_t i = rebuildingPage_; i < visitedPages_.size(); ++i) cachedBytes_ = visitedPages_[i].size() > cachedBytes_ ? 0 : cachedBytes_ - visitedPages_[i].size();
    visitedPages_.resize(rebuildingPage_ + 1);
    pageAnchors_.resize(rebuildingPage_ + 1);
    visitedPages_[rebuildingPage_] = workingText_;
    pageAnchors_[rebuildingPage_] = workingAnchor_;
    currentPage_ = rebuildingPage_;
    rebuildingPage_ = static_cast<size_t>(-1);
  } else {
    visitedPages_.push_back(workingText_); pageAnchors_.push_back(workingAnchor_);
    if (!prefetching_) currentPage_ = visitedPages_.size() - 1;
  }
  workingAnchor_.parserCheckpoint += static_cast<uint32_t>(workingText_.size());
  workingAnchor_.uncompressedOffset = offset_;
  // Keep every lightweight anchor, but evict old page text independently.
  while (cachedBytes_ > app_config::kMaxVisitedPageTextBytes) {
    size_t victim = 0;
    while (victim < visitedPages_.size() && (victim == currentPage_ || visitedPages_[victim].empty())) ++victim;
    if (victim == visitedPages_.size()) break;
    cachedBytes_ -= visitedPages_[victim].size();
    visitedPages_[victim].clear();
  }
  return validCacheState();
}
void ReaderController::renderCachedPage(size_t index) { beginBlankPage(*pageCanvas_); layout_.processText(visitedPages_[index]); layout_.finish(); currentPage_ = index; presentationSource_ = PagePresentationSource::TextCacheRender; }

bool ReaderController::redrawCurrentPage() {
  if (currentPage_ >= visitedPages_.size() || visitedPages_[currentPage_].empty())
    return false;
  renderCachedPage(currentPage_);
  return true;
}

PageAnchor ReaderController::currentAnchor() const {
  return currentPage_ < pageAnchors_.size() ? pageAnchors_[currentPage_]
                                            : workingAnchor_;
}

std::vector<PageAnchor> ReaderController::previousPageAnchors(size_t maximum) const {
  std::vector<PageAnchor> result;
  if (currentPage_ == 0 || pageAnchors_.empty() || maximum == 0) return result;
  const size_t end = std::min(currentPage_, pageAnchors_.size());
  const size_t begin = end > maximum ? end - maximum : 0;
  result.insert(result.end(), pageAnchors_.begin() + begin, pageAnchors_.begin() + end);
  return result;
}

WorkResult ReaderController::processNextChunk() {
  if (prefetching_ && prefetchState_.state() == PrefetchState::CancelRequested)
    return restorePrefetchCheckpoint() ? WorkResult::Idle
                                       : failReader("Falha ao restaurar prefetch");
  if (!active_) return error_.empty() ? WorkResult::Idle : WorkResult::Failed;
  if (inputReady_) return processBufferedInput(app_config::kCpuWorkBudgetPerTickUs);
  if (layoutPending_) return continueLayout(app_config::kCpuWorkBudgetPerTickUs);
  return readInputChunk();
}

WorkResult ReaderController::readInputChunk() {
  if (!active_ || inputReady_ || layoutPending_) return WorkResult::Idle;
  if (prefetching_) prefetchState_.transition(PrefetchState::ReadingInput);
  const int count = parser_.archive().readEntryChunk(inputBuffer_, sizeof(inputBuffer_));
  if (count < 0) return failReader(parser_.archive().error());
  offset_ += count;
  inputLength_ = static_cast<size_t>(count);
  inputEnded_ = count == 0;
  inputReady_ = true;
  return WorkResult::MoreWork;
}

WorkResult ReaderController::processBufferedInput(uint32_t budgetUs) {
  (void)budgetUs;
  if (!active_ || !inputReady_) return WorkResult::Idle;
  if (prefetching_) prefetchState_.transition(PrefetchState::Parsing);
  pendingText_ += html_.feed(inputBuffer_, inputLength_, inputEnded_);
  inputReady_ = false;
  inputLength_ = 0;
  if (seekTextBytes_ != 0) {
    const size_t skipped = pendingText_.size() < seekTextBytes_ ? pendingText_.size() : seekTextBytes_;
    pendingText_.erase(0, skipped); seekTextBytes_ -= static_cast<uint32_t>(skipped);
    if (seekTextBytes_ != 0) {
      if (inputEnded_) return failReader("Checkpoint fora do capitulo");
      inputEnded_ = false;
      return WorkResult::MoreWork;
    }
  }
  layoutPending_ = true;
  return WorkResult::MoreWork;
}

WorkResult ReaderController::continueLayout(uint32_t budgetUs) {
  (void)budgetUs;
  if (!active_ || !layoutPending_) return WorkResult::Idle;
  if (prefetching_) prefetchState_.transition(PrefetchState::LayingOut);
  const size_t consumed = layout_.processText(pendingText_); workingText_.append(pendingText_, 0, consumed); pendingText_.erase(0, consumed);
  layoutPending_ = false;
  if (layout_.pageFull()) { const bool wasPrefetching = prefetching_; inputEnded_ = false; if (!commitPage()) return failReader(error_); active_ = false; prefetching_ = false; if (wasPrefetching) { prefetchState_.transition(PrefetchState::Ready); freeCanvasState_ = CanvasState::Ready; } return WorkResult::Completed; }
  if (inputEnded_) { const bool wasPrefetching = prefetching_; inputEnded_ = false; chapterEnded_ = true; layout_.finish(); if ((!workingText_.empty() || visitedPages_.empty()) && !commitPage()) return failReader(error_); if (!parser_.archive().endEntry()) return failReader(parser_.archive().error()); active_ = false; prefetching_ = false; if (wasPrefetching) { prefetchState_.transition(PrefetchState::Ready); freeCanvasState_ = CanvasState::Ready; } return WorkResult::Completed; }
  if (prefetching_) prefetchState_.transition(PrefetchState::ReadingInput);
  return WorkResult::MoreWork;
}

uint32_t ReaderController::processCpuOnlyWork(uint32_t budgetUs) {
  const uint32_t started = micros();
  uint8_t transitions = 0;
  while (hasCpuOnlyWork() && micros() - started < budgetUs && transitions < 2) {
    if (inputReady_)
      processBufferedInput(budgetUs - (micros() - started));
    else if (layoutPending_ && !inputEnded_)
      continueLayout(budgetUs - (micros() - started));
    ++transitions;
  }
  return micros() - started;
}

WorkResult ReaderController::prepareUncachedPage(M5Canvas& target) {
  if (!validCacheState()) return failReader("Cache de paginas inconsistente");
  if (active_) return WorkResult::MoreWork;
  if (endOfBook_) return WorkResult::Idle;
  if (chapterEnded_) {
    if (!openNextLinearSpine()) return error_.empty() ? WorkResult::Idle : WorkResult::Failed;
  }
  beginBlankPage(target);
  if (!pendingText_.empty()) { const size_t consumed = layout_.processText(pendingText_); workingText_.append(pendingText_, 0, consumed); pendingText_.erase(0, consumed); if (layout_.pageFull()) { if (!commitPage()) return failReader(error_); return WorkResult::Completed; } }
  active_ = true; return WorkResult::MoreWork;
}

WorkResult ReaderController::requestNextPage() {
  if (!validCacheState()) return failReader("Cache de paginas inconsistente");
  if (prefetching_ || active_) return WorkResult::MoreWork;
  if (currentPage_ + 1 < visitedPages_.size()) {
    if (prefetchState_.ready() && freeCanvas_) {
      M5Canvas* oldFront = pageCanvas_;
      pageCanvas_ = freeCanvas_;
      freeCanvas_ = oldFront;
      pageCanvasState_ = CanvasState::Displayed;
      freeCanvasState_ = CanvasState::Free;
      ++currentPage_;
      presentationSource_ = PagePresentationSource::ReadyBackBuffer;
      prefetchState_.reset();
      Serial.println("M5EPUB_BUFFER,swap=1,presentation_source=ready_back_buffer");
      return WorkResult::Completed;
    }
    if (visitedPages_[currentPage_ + 1].empty()) return rebuildPage(currentPage_ + 1);
    renderCachedPage(currentPage_ + 1); prefetchState_.reset(); return WorkResult::Completed;
  }
  presentationSource_ = PagePresentationSource::ExistingFrontBuffer;
  return prepareUncachedPage(*pageCanvas_);
}

WorkResult ReaderController::requestPrefetch() {
  if (!validCacheState()) return failReader("Cache de paginas inconsistente");
  if (!prefetchCanvasReady_ || !freeCanvas_ || active_ || prefetching_ || endOfBook_ ||
      currentPage_ + 1 < visitedPages_.size()) return WorkResult::Idle;
  prefetchCheckpoint_ = workingAnchor_;
  if (!prefetchState_.start()) return WorkResult::Idle;
  prefetching_ = true;
  freeCanvasState_ = CanvasState::Rendering;
  const WorkResult result = prepareUncachedPage(*freeCanvas_);
  if (result == WorkResult::Completed) {
    prefetching_ = false;
    prefetchState_.transition(PrefetchState::Ready);
    freeCanvasState_ = CanvasState::Ready;
  } else if (result != WorkResult::MoreWork) {
    prefetching_ = false;
    prefetchState_.reset();
    freeCanvasState_ = CanvasState::Free;
  }
  return result;
}

PrefetchCancelResult ReaderController::cancelPrefetch() {
  const PrefetchCancelResult result = prefetchState_.requestCancel();
  if (result == PrefetchCancelResult::DeferredUntilSafePoint)
    Serial.println("M5EPUB_PREFETCH,status=cancel_requested");
  else if (result == PrefetchCancelResult::PreservedAsCache)
    Serial.println("M5EPUB_PREFETCH,status=preserved_as_cache");
  return result;
}

void ReaderController::invalidateReadyPrefetch() {
  if (!prefetchState_.ready()) return;
  prefetchState_.reset();
  freeCanvasState_ = CanvasState::Free;
  Serial.println("M5EPUB_BUFFER,ready_invalidated=1");
}

bool ReaderController::restorePrefetchCheckpoint() {
  if (!prefetchState_.transition(PrefetchState::Restoring)) return false;
  if (!parser_.archive().cancelEntry()) {
    prefetchState_.restored(false);
    return false;
  }
  active_ = false;
  prefetching_ = false;
  freeCanvasState_ = CanvasState::Free;
  pendingText_.clear();
  workingText_.clear();
  inputLength_ = 0;
  inputReady_ = inputEnded_ = layoutPending_ = false;
  if (!openLinearSpineAt(prefetchCheckpoint_.spineIndex)) {
    prefetchState_.restored(false);
    return false;
  }
  workingAnchor_ = prefetchCheckpoint_;
  seekTextBytes_ = prefetchCheckpoint_.parserCheckpoint;
  active_ = false;
  prefetchState_.restored(true);
  Serial.printf("M5EPUB_PREFETCH,status=cancelled,checkpoint=%lu:%lu\n",
                static_cast<unsigned long>(prefetchCheckpoint_.spineIndex),
                static_cast<unsigned long>(prefetchCheckpoint_.parserCheckpoint));
  return true;
}

WorkResult ReaderController::reflowCurrentPage(uint16_t newSize) {
  if (!validCacheState()) return failReader("Cache de paginas inconsistente");
  if (active_ || visitedPages_.empty() || newSize == settings_.fontSize) return WorkResult::Idle;
  settings_.fontSize = newSize;
  invalidateReadyPrefetch();
  return rebuildPage(currentPage_);
}

WorkResult ReaderController::increaseFontSize() {
  if (settings_.fontSize >= 40) return WorkResult::Idle;
  if (settings_.fontSize < 24) return reflowCurrentPage(24);
  if (settings_.fontSize < 32) return reflowCurrentPage(32);
  return reflowCurrentPage(settings_.fontSize < 36 ? 36 : 40);
}

WorkResult ReaderController::decreaseFontSize() {
  if (settings_.fontSize <= 16) return WorkResult::Idle;
  if (settings_.fontSize > 36) return reflowCurrentPage(36);
  if (settings_.fontSize > 32) return reflowCurrentPage(32);
  return reflowCurrentPage(settings_.fontSize > 24 ? 24 : 16);
}

WorkResult ReaderController::requestPreviousPage() {
  if (!validCacheState()) return failReader("Cache de paginas inconsistente");
  if (active_ || currentPage_ == 0 || visitedPages_.empty()) return WorkResult::Idle;
  invalidateReadyPrefetch();
  if (visitedPages_[currentPage_ - 1].empty()) return rebuildPage(currentPage_ - 1);
  renderCachedPage(currentPage_ - 1); return WorkResult::Completed;
}

WorkResult ReaderController::rebuildPage(size_t index) {
  if (index >= pageAnchors_.size() || active_) return WorkResult::Idle;
  const PageAnchor anchor = pageAnchors_[index];
  if (!chapterEnded_ && !parser_.archive().cancelEntry()) return failReader(parser_.archive().error());
  if (!openLinearSpineAt(anchor.spineIndex)) { if (error_.empty()) error_ = "Nao foi possivel reconstruir a pagina"; return WorkResult::Failed; }
  rebuildingPage_ = index;
  seekTextBytes_ = anchor.parserCheckpoint;
  workingAnchor_ = anchor;
  beginBlankPage(*pageCanvas_);
  presentationSource_ = PagePresentationSource::CheckpointRebuild;
  active_ = true;
  return WorkResult::MoreWork;
}
