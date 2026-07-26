#include "ReaderController.h"
#include "AppConfig.h"

bool ReaderController::resetSession() {
  if (active_ || !chapterEnded_) parser_.archive().cancelEntry();
  active_ = false; error_.clear(); chapterPath_.clear(); pendingText_.clear(); workingText_.clear(); visitedPages_.clear(); pageAnchors_.clear(); workingAnchor_ = {}; cachedBytes_ = 0; currentPage_ = 0; spineIndex_ = 0; offset_ = 0; seekTextBytes_ = 0; rebuildingPage_ = static_cast<size_t>(-1); chapterEnded_ = true; prefetching_ = false; endOfBook_ = false;
  if (!prefetchCanvasReady_) {
    prefetchCanvas_.setColorDepth(1);
    prefetchCanvas_.setPsram(true);
    prefetchCanvasReady_ = prefetchCanvas_.createSprite(canvas_.width(), canvas_.height()) != nullptr;
    if (!prefetchCanvasReady_) { error_ = "Sem PSRAM para cache da proxima pagina"; return false; }
    Serial.printf("Reader prefetch canvas: %ld x %ld, PSRAM free=%u\n", static_cast<long>(canvas_.width()), static_cast<long>(canvas_.height()), ESP.getFreePsram());
  }
  return true;
}

bool ReaderController::start() {
  if (!resetSession()) return false;
  if (!openNextLinearSpine()) { if (error_.empty()) error_ = "Spine sem capitulo linear"; return false; }
  beginBlankPage(canvas_); active_ = true; return true;
}

bool ReaderController::startAt(const PageAnchor& anchor, uint16_t fontSize) {
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
  beginBlankPage(canvas_); active_ = true; return true;
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
  html_.reset(); pendingText_.clear(); offset_ = 0; chapterEnded_ = false; endOfBook_ = false;
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
  html_.reset(); pendingText_.clear(); offset_ = 0; chapterEnded_ = false;
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
void ReaderController::renderCachedPage(size_t index) { beginBlankPage(canvas_); layout_.processText(visitedPages_[index]); layout_.finish(); currentPage_ = index; }

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

WorkResult ReaderController::processNextChunk() {
  if (!active_) return error_.empty() ? WorkResult::Idle : WorkResult::Failed; uint8_t buffer[1024]; const int count = parser_.archive().readEntryChunk(buffer, sizeof(buffer));
  if (count < 0) return failReader(parser_.archive().error());
  offset_ += count; const bool ended = count == 0; pendingText_ += html_.feed(buffer, count, ended);
  if (seekTextBytes_ != 0) {
    const size_t skipped = pendingText_.size() < seekTextBytes_ ? pendingText_.size() : seekTextBytes_;
    pendingText_.erase(0, skipped); seekTextBytes_ -= static_cast<uint32_t>(skipped);
    if (seekTextBytes_ != 0) {
      if (ended) return failReader("Checkpoint fora do capitulo");
      return WorkResult::MoreWork;
    }
  }
  const size_t consumed = layout_.processText(pendingText_); workingText_.append(pendingText_, 0, consumed); pendingText_.erase(0, consumed);
  if (layout_.pageFull()) { if (!commitPage()) return failReader(error_); active_ = false; prefetching_ = false; return WorkResult::Completed; }
  if (ended) { chapterEnded_ = true; layout_.finish(); if ((!workingText_.empty() || visitedPages_.empty()) && !commitPage()) return failReader(error_); if (!parser_.archive().endEntry()) return failReader(parser_.archive().error()); active_ = false; prefetching_ = false; return WorkResult::Completed; }
  return WorkResult::MoreWork;
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
    if (visitedPages_[currentPage_ + 1].empty()) return rebuildPage(currentPage_ + 1);
    renderCachedPage(currentPage_ + 1); return WorkResult::Completed;
  }
  return prepareUncachedPage(canvas_);
}

WorkResult ReaderController::requestPrefetch() {
  if (!validCacheState()) return failReader("Cache de paginas inconsistente");
  if (!prefetchCanvasReady_ || active_ || prefetching_ || endOfBook_ ||
      currentPage_ + 1 < visitedPages_.size()) return WorkResult::Idle;
  prefetching_ = true;
  const WorkResult result = prepareUncachedPage(prefetchCanvas_);
  if (result != WorkResult::MoreWork) prefetching_ = false;
  return result;
}

WorkResult ReaderController::reflowCurrentPage(uint16_t newSize) {
  if (!validCacheState()) return failReader("Cache de paginas inconsistente");
  if (active_ || visitedPages_.empty() || newSize == settings_.fontSize) return WorkResult::Idle;
  settings_.fontSize = newSize;
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
  beginBlankPage(canvas_);
  active_ = true;
  return WorkResult::MoreWork;
}
