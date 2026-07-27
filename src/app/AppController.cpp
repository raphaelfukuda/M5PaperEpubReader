#include "AppController.h"
#include "ui/CoverRenderer.h"
#include "epub/EpubContentDiscovery.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <new>
#if M5EPUB_ENABLE_WEB_PORTAL
#include <ESPmDNS.h>
#endif
#include "AppConfig.h"
#include "UiStrings.h"
#include "diagnostics/Logger.h"
#include "storage/PathUtils.h"

namespace {
constexpr const char* kReadingStatePath = "/.m5epub-reading-state";
constexpr const char* kCardPrefetchQueuePath = "/.m5epub-cache/card-prefetch.tmp";
constexpr size_t kMaximumQueuedPathBytes = 2048;

struct CardPrefetchRecord {
  uint16_t pathLength = 0;
  uint64_t size = 0;
  uint64_t modifiedTime = 0;
};
}  // namespace

void AppController::begin() {
  const uint32_t started = millis();
  Logger::begin();
  auto config = M5.config();
  M5.begin(config);
  void* libraryParserMemory = heap_caps_malloc(
      sizeof(EpubParser), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (libraryParserMemory)
    libraryParser_ = new (libraryParserMemory) EpubParser(spiBus_);
  Serial.printf("M5EPUB_MEMORY,component=library_parser,bytes=%u,status=%s\n",
                static_cast<unsigned>(sizeof(EpubParser)),
                libraryParser_ ? "ready" : "disabled");
  power_.begin();
  preferences_.begin("m5epub", false);
  const uint8_t savedLanguage = preferences_.getUChar("language", 0);
  ui_strings::setLanguage(savedLanguage == 1 ? UiLanguage::Portuguese
                                             : UiLanguage::English);

  const bool boardOk = M5.getBoard() == m5::board_t::board_M5Paper;
  const bool displayOk = display_.begin();
  reader_.setHighQualityCanvasProvider([this]() -> M5Canvas& {
    return display_.imageCanvas();
  });
  const bool touchOk = touch_.begin();
  display_.waitUntilIdle();
  const SdCardStatus sd = sdCard_.begin();

  Logger::hardwareReport(sd.mounted, sd.sizeBytes, sd.mountMs);
  Serial.printf("Canvas 1-bit: %s; boot submit at %lu ms\n",
                displayOk ? "ready" : "allocation failed", millis() - started);
  if (!boardOk || !displayOk) state_ = AppState::ErrorDialog;
  else if (sd.mounted) {
    state_ = AppState::FileBrowser;
    startDirectory(sdCard_.directoryExists("/Books") ? "/Books" : "/");
  } else {
    state_ = AppState::SdError;
  }
  lastInteractionMs_ = millis();
}

void AppController::tick() {
  M5.update();
  display_.pollPanelTiming();
#if M5EPUB_ENABLE_WEB_PORTAL
  servicePortal();
#endif
  const AppEvent event = touch_.poll();
  const bool increasePressed = M5.BtnA.wasPressed();
  const bool centerPressed = M5.BtnB.wasPressed();
  const bool decreasePressed = M5.BtnC.wasPressed();
  const bool leverInteraction = increasePressed || centerPressed || decreasePressed;
  if (event.type != AppEventType::None || leverInteraction) {
    inputRecognizedUs_ = micros();
    lastInteractionMs_ = millis();
  }
  if (state_ == AppState::Reading && leverInteraction)
    if (increasePressed || decreasePressed)
      handleReaderLever(increasePressed ? PendingReaderAction::IncreaseFont
                                        : PendingReaderAction::DecreaseFont);
  if (centerPressed) requestManualRefresh();
  if (state_ == AppState::FileBrowser) handleBrowserEvent(event);
  else if (state_ == AppState::LibraryMenu ||
           state_ == AppState::LibraryPrefetchConfirm ||
           state_ == AppState::LibraryPrefetch)
    handleLibraryMenuEvent(event);
  else if (state_ == AppState::Reading) handleReaderEvent(event);
  else if (state_ == AppState::ReaderMenu) handleReaderMenuEvent(event);
#if M5EPUB_ENABLE_WEB_PORTAL
  else if (state_ == AppState::WebPortalNetworks ||
           state_ == AppState::WebPortalPassword)
    handleWebPortalEvent(event);
#endif
  else if (state_ == AppState::ErrorDialog &&
           event.type == AppEventType::Tap && bookSelected_) {
    state_ = AppState::FileBrowser; bookSelected_ = false;
    browserView_.setModel(currentPath_, &scanner_.entries(), false,
                          scanner_.wasTruncated(), scanner_.error()); browserDirty_ = true;
  }

  if (state_ == AppState::Reading && !M5.Display.displayBusy() &&
      !reader_.isPrefetching() && !pendingReaderActions_.empty()) {
    const PendingActionEntry pending = pendingReaderActions_.pop();
    executeReaderAction(pending.action, pending.queuedAtUs);
  }

  if (M5.Display.displayBusy() && reader_.hasCpuOnlyWork()) {
    const uint32_t usefulUs =
        reader_.processCpuOnlyWork(app_config::kCpuWorkBudgetPerTickUs);
    display_.pollPanelTiming(usefulUs);
  }
  serviceManualRefresh();

  if (state_ == AppState::OpeningBook && M5.Display.displayBusy() == false) {
    if (!readingLayout_) {
      const WorkResult work = epubParser_.processNextChunk();
      if (work == WorkResult::Completed) {
      const EpubBook& book = epubParser_.book();
      Serial.printf("EPUB parsed: title=%s author=%s language=%s manifest=%u spine=%u opf=%s\n", book.title.c_str(), book.author.c_str(), book.language.c_str(), static_cast<unsigned>(book.manifest.size()), static_cast<unsigned>(book.spine.size()), book.packagePath.c_str());
      for (size_t i = 0; i < book.spine.size(); ++i) Serial.printf("Spine[%u]: %s\n", static_cast<unsigned>(i), book.spine[i].idref.c_str());
        loadCurrentBookCover();
        if (!startReaderWithSavedState()) { Serial.printf("Reader start error: %s\n", reader_.error().c_str()); browserView_.showBookError(reader_.error()); state_ = AppState::ErrorDialog; bookSelected_ = true; }
        else readingLayout_ = true;
      } else if (work == WorkResult::Failed) {
      Serial.printf("EPUB parse error: %s\n", epubParser_.error().c_str()); browserView_.showBookError(epubParser_.error()); state_ = AppState::ErrorDialog; bookSelected_ = true;
      }
    } else {
      const WorkResult work = reader_.processNextChunk();
      if (work == WorkResult::Completed) { renderCurrentReaderPage(); state_ = AppState::Reading; bookSelected_ = true; markReadingStateDirty(); recordPageReady(); Serial.printf("XHTML page rendered: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
      else if (work == WorkResult::Failed) { browserView_.showBookError(reader_.error()); state_ = AppState::ErrorDialog; bookSelected_ = true; Serial.printf("Reader error: %s\n", reader_.error().c_str()); }
    }
  }

  if (state_ == AppState::Reading && M5.Display.displayBusy() == false) {
    if (forcedPersistPending_) {
      if (persistReadingState(forcedPersistReason_, true))
        forcedPersistPending_ = false;
    } else {
      PersistReason reason = PersistReason::IdleTimeout;
      if (persistPolicy_.shouldSave(millis(), false,
                                    !pendingReaderActions_.empty() || reader_.isPrefetching(),
                                    reason))
        persistReadingState(reason);
    }
    if (!reader_.isPrefetching()) reader_.requestPrefetch();
    if (reader_.isPrefetching()) {
      const WorkResult work = reader_.processNextChunk();
      if (work == WorkResult::Completed) Serial.println("Next page prefetched");
      else if (work == WorkResult::Failed) Serial.printf("Prefetch stopped: %s\n", reader_.error().c_str());
    }
  }

  if (scanner_.isRunning() && M5.Display.displayBusy() == false) {
    const WorkResult work = scanner_.processNextBatch();
    if (work == WorkResult::Completed || work == WorkResult::Failed) {
      browserView_.setModel(currentPath_, &scanner_.entries(), false,
                            scanner_.wasTruncated(), scanner_.error());
      if (browserRestorePending_ && browserRestorePath_ == currentPath_) {
        browserView_.setPage(browserRestorePage_);
        browserRestorePending_ = false;
        Serial.printf("M5EPUB_LIBRARY_NAV,status=restored,path=%s,page=%u\n",
                      currentPath_.c_str(),
                      static_cast<unsigned>(browserView_.page() + 1));
      }
      browserDirty_ = true;
      scheduleLibraryPreviews();
      Serial.printf("Directory scan: path=%s entries=%u truncated=%s\n",
                    currentPath_.c_str(), static_cast<unsigned>(scanner_.entries().size()),
                    scanner_.wasTruncated() ? "yes" : "no");
    }
  }
  serviceLibraryPreviews();
  serviceCardPrefetch();
  if (browserDirty_ && M5.Display.displayBusy() == false) {
    browserView_.render();
    browserDirty_ = false;
  }
  const uint32_t now = millis();
  if (now - lastInteractionMs_ >= app_config::kSleepAfterInactivityMs &&
      canEnterAutomaticSleep()) {
    enterSleepMode();
  }
  if (now - lastYieldMs_ >= app_config::kIdleYieldIntervalMs) {
    lastYieldMs_ = now;
    yield();
  }
}

void AppController::renderCurrentReaderPage() {
  readerView_.renderPageChrome(
      reader_.book(), reader_.pageNumber(), reader_.presentationCanvas(),
      reader_.pageContainsImages() ? RefreshIntent::ImageQuality
                                   : RefreshIntent::ReadingPage);
}

void AppController::handleReaderLever(PendingReaderAction pendingAction) {
  if (M5.Display.displayBusy() || reader_.isPrefetching()) {
    queueReaderAction(pendingAction);
    return;
  }
  executeReaderAction(pendingAction);
}

void AppController::executeReaderAction(PendingReaderAction pendingAction,
                                        uint32_t queuedAtUs) {
  if (queuedAtUs != 0)
    Serial.printf("M5EPUB_INPUT,event=%s,status=executed,queue_delay_us=%lu\n",
                  pendingReaderActionName(pendingAction),
                  static_cast<unsigned long>(micros() - queuedAtUs));
  if (pendingAction == PendingReaderAction::OpenMenu) {
    reader_.invalidateReadyPrefetch();
    readerMenuView_.render(reader_.book(), reader_.currentAnchor(),
                           reader_.pageNumber(), reader_.fontSize(), reader_.fontFamily(),
                           coverData_, coverMediaType_);
    state_ = AppState::ReaderMenu;
    Serial.println("Reader menu opened");
    return;
  }
  if (pendingAction == PendingReaderAction::PreviousPage) {
    beginPageTurnMeasurement(PageTurnKind::CachedPrevious);
    const WorkResult result = reader_.requestPreviousPage();
    if (result == WorkResult::Completed) { renderCurrentReaderPage(); markReadingStateDirty(); recordPageReady(); Serial.printf("Previous page: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
    else if (result == WorkResult::MoreWork) {
      state_ = AppState::OpeningBook;
      readingLayout_ = true;
      bookSelected_ = false;
      Serial.println("Rebuilding previous page");
    } else {
      measuringPageTurn_ = false;
    }
    return;
  }
  if (pendingAction == PendingReaderAction::NextPage) {
    beginPageTurnMeasurement(PageTurnKind::CachedNext);
    const WorkResult result = reader_.requestNextPage();
    if (result == WorkResult::Completed) { renderCurrentReaderPage(); markReadingStateDirty(); recordPageReady(); Serial.printf("Cached next page: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
    else if (result == WorkResult::MoreWork) { state_ = AppState::OpeningBook; readingLayout_ = true; bookSelected_ = false; Serial.println("Generating next page"); }
    else { measuringPageTurn_ = false; Serial.println("End of current chapter"); }
    return;
  }
  if (pendingAction != PendingReaderAction::IncreaseFont &&
      pendingAction != PendingReaderAction::DecreaseFont) return;
  const uint32_t now = millis();
  if (now - lastLeverActionMs_ < 600) return;
  const bool increase = pendingAction == PendingReaderAction::IncreaseFont;
  const WorkResult result = increase ? reader_.increaseFontSize()
                                     : reader_.decreaseFontSize();
  const char* action = increase ? "increase" : "decrease";
  beginPageTurnMeasurement(PageTurnKind::FontReflow);
  lastLeverActionMs_ = now;
  const PageAnchor anchor = reader_.currentAnchor();
  Serial.printf("Lever font %s: %u px; anchor=%lu:%lu\n", action,
                reader_.fontSize(),
                static_cast<unsigned long>(anchor.spineIndex),
                static_cast<unsigned long>(anchor.parserCheckpoint));
  if (result == WorkResult::Completed) {
    renderCurrentReaderPage();
    markReadingStateDirty();
    requestForcedPersist(PersistReason::FontChanged);
    recordPageReady();
  } else if (result == WorkResult::MoreWork) {
    state_ = AppState::OpeningBook;
    readingLayout_ = true;
  } else {
    measuringPageTurn_ = false;
    if (result == WorkResult::Failed) {
      Serial.printf("Font reflow failed: %s\n", reader_.error().c_str());
      browserView_.showBookError(reader_.error());
      state_ = AppState::ErrorDialog;
      bookSelected_ = true;
    }
  }
}

void AppController::queueReaderAction(PendingReaderAction action) {
  const bool queued = pendingReaderActions_.enqueue(action, micros());
  if (queued && action != PendingReaderAction::NextPage && reader_.hasPrefetchWork())
    reader_.cancelPrefetch();
  Serial.printf("M5EPUB_INPUT,event=%s,status=%s,display_busy=%u,prefetch_active=%u\n",
                pendingReaderActionName(action), queued ? "queued" : "coalesced",
                M5.Display.displayBusy() ? 1 : 0, reader_.isPrefetching() ? 1 : 0);
}

void AppController::requestManualRefresh() {
  if (!manualRefreshPending_) manualRefreshQueuedUs_ = micros();
  manualRefreshPending_ = true;
  Serial.printf("M5EPUB_REFRESH_INPUT,event=manual_quality,status=queued,display_busy=%u\n",
                M5.Display.displayBusy() ? 1 : 0);
}

void AppController::serviceManualRefresh() {
  if (!manualRefreshPending_ || M5.Display.displayBusy() ||
      state_ == AppState::Sleeping || state_ == AppState::Booting ||
      state_ == AppState::OpeningBook)
    return;
  M5Canvas& source = state_ == AppState::Reading
                         ? reader_.presentationCanvas()
                         : display_.canvas();
  if (!display_.submitCanvas(source, RefreshIntent::ManualCleanup)) return;
  manualRefreshPending_ = false;
  Serial.printf("M5EPUB_REFRESH_INPUT,event=manual_quality,status=submitted,queue_delay_us=%lu\n",
                static_cast<unsigned long>(micros() - manualRefreshQueuedUs_));
}

void AppController::handleReaderEvent(const AppEvent& event) {
  const bool tap = event.type == AppEventType::Tap;
  PendingReaderAction action = PendingReaderAction::None;
  if (tap && event.y < app_config::kReaderTopActionHeight)
    action = PendingReaderAction::OpenMenu;
  else if ((tap && event.x < display_.width() / 2) || event.type == AppEventType::SwipeRight)
    action = PendingReaderAction::PreviousPage;
  else if ((tap && event.x >= display_.width() / 2) || event.type == AppEventType::SwipeLeft)
    action = PendingReaderAction::NextPage;
  if (action == PendingReaderAction::None) return;
  if (M5.Display.displayBusy() || reader_.isPrefetching()) {
    queueReaderAction(action);
    return;
  }
  executeReaderAction(action);
}

void AppController::handleReaderMenuEvent(const AppEvent& event) {
  if (M5.Display.displayBusy() || event.type != AppEventType::Tap) return;
  const ReaderMenuAction action = readerMenuView_.actionAt(event.x, event.y);
  if (action == ReaderMenuAction::BackToLibrary) {
    markReadingStateDirty();
    persistReadingState(PersistReason::BackToLibrary, true);
    state_ = AppState::FileBrowser;
    bookSelected_ = false;
    browserView_.setModel(currentPath_, &scanner_.entries(), false,
                          scanner_.wasTruncated(), scanner_.error());
    coverData_.clear();
    coverMediaType_.clear();
    scheduleLibraryPreviews();
    browserDirty_ = true;
    Serial.println("Reader menu: back to library");
  } else if (action == ReaderMenuAction::OpenTableOfContents) {
    tocPage_ = 0;
    readerMenuView_.renderTableOfContents(reader_.book(), tocPage_);
  } else if (action == ReaderMenuAction::TocPreviousPage) {
    if (tocPage_ > 0) --tocPage_;
    readerMenuView_.renderTableOfContents(reader_.book(), tocPage_);
  } else if (action == ReaderMenuAction::TocNextPage) {
    const size_t pages = std::max<size_t>(1, (reader_.book().tableOfContents.size() + 8) / 9);
    if (tocPage_ + 1 < pages) ++tocPage_;
    readerMenuView_.renderTableOfContents(reader_.book(), tocPage_);
  } else if (action == ReaderMenuAction::None) {
    const int tocIndex = readerMenuView_.tocIndexAt(
        event.y, reader_.book().tableOfContents.size());
    if (tocIndex < 0) return;
    const EpubTocEntry& entry = reader_.book().tableOfContents[tocIndex];
    std::string idref;
    for (const auto& manifest : reader_.book().manifest) {
      if (manifest.href == entry.documentPath) { idref = manifest.id; break; }
    }
    uint32_t spineIndex = UINT32_MAX;
    for (size_t i = 0; i < reader_.book().spine.size(); ++i) {
      if (reader_.book().spine[i].idref == idref) {
        spineIndex = static_cast<uint32_t>(i);
        break;
      }
    }
    if (spineIndex == UINT32_MAX || !reader_.startAtSpine(spineIndex)) {
      Serial.printf("TOC destination unavailable: %s\n", entry.documentPath.c_str());
      return;
    }
    persistPolicy_.recordSaved(millis());
    beginPageTurnMeasurement(PageTurnKind::TocNavigation);
    state_ = AppState::OpeningBook;
    readingLayout_ = true;
    bookSelected_ = false;
    Serial.printf("TOC navigation: spine=%lu title=%s\n",
                  static_cast<unsigned long>(spineIndex), entry.title.c_str());
  } else if (action == ReaderMenuAction::RestartBook) {
    readerMenuView_.renderRestartConfirmation();
    Serial.println("Reader menu: restart confirmation");
  } else if (action == ReaderMenuAction::ToggleLanguage) {
    const UiLanguage language = ui_strings::language() == UiLanguage::English
                                    ? UiLanguage::Portuguese
                                    : UiLanguage::English;
    ui_strings::setLanguage(language);
    preferences_.putUChar("language", static_cast<uint8_t>(language));
    readerMenuView_.render(reader_.book(), reader_.currentAnchor(),
                           reader_.pageNumber(), reader_.fontSize(), reader_.fontFamily(),
                           coverData_, coverMediaType_);
    Serial.printf("UI language: %s\n", ui_strings::languageName());
  } else if (action == ReaderMenuAction::EnterSleep) {
    enterSleepMode();
  } else if (action == ReaderMenuAction::CycleFontFamily) {
    beginPageTurnMeasurement(PageTurnKind::FontReflow);
    const WorkResult result = reader_.cycleFontFamily();
    markReadingStateDirty(false);
    requestForcedPersist(PersistReason::FontChanged);
    if (result == WorkResult::MoreWork) {
      state_ = AppState::OpeningBook;
      readingLayout_ = true;
      bookSelected_ = false;
    } else if (result == WorkResult::Completed) {
      renderCurrentReaderPage();
      state_ = AppState::Reading;
      recordPageReady();
    } else {
      measuringPageTurn_ = false;
    }
  } else if (action == ReaderMenuAction::CancelRestart) {
    readerMenuView_.render(reader_.book(), reader_.currentAnchor(),
                           reader_.pageNumber(), reader_.fontSize(), reader_.fontFamily(),
                           coverData_, coverMediaType_);
  } else if (action == ReaderMenuAction::ConfirmRestart) {
    bool stateRemoved = false;
    {
      ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
      stateRemoved = bus && readingStateStore_.remove(kReadingStatePath);
    }
    if (!stateRemoved) {
      Serial.printf("Reading state reset failed: %s\n",
                    readingStateStore_.error().c_str());
      return;
    }
    persistPolicy_.recordSaved(millis());
    beginPageTurnMeasurement(PageTurnKind::GeneratedNext);
    if (!reader_.start()) {
      measuringPageTurn_ = false;
      browserView_.showBookError(reader_.error());
      state_ = AppState::ErrorDialog;
      bookSelected_ = true;
      return;
    }
    state_ = AppState::OpeningBook;
    readingLayout_ = true;
    bookSelected_ = false;
    Serial.println("Reader menu: book restarted from beginning");
  } else if (action == ReaderMenuAction::Close) {
    beginPageTurnMeasurement(PageTurnKind::MenuReturn);
    if (reader_.redrawCurrentPage())
      renderCurrentReaderPage();
    recordPageReady();
    state_ = AppState::Reading;
    Serial.println("Reader menu closed");
  }
}

bool AppController::canEnterAutomaticSleep() const {
#if M5EPUB_ENABLE_WEB_PORTAL
  if (portal_.running() || wifi_.state() == WifiState::Scanning ||
      wifi_.state() == WifiState::Connecting) return false;
#endif
  if (M5.Display.displayBusy() || scanner_.isRunning() || reader_.isPrefetching())
    return false;
  return state_ == AppState::FileBrowser || state_ == AppState::Reading ||
         state_ == AppState::ReaderMenu;
}

void AppController::enterSleepMode() {
  if (M5.Display.displayBusy()) return;
  const bool resumeReading = state_ == AppState::Reading ||
                             state_ == AppState::ReaderMenu;
  if (resumeReading) {
    markReadingStateDirty();
    persistReadingState(PersistReason::EnterSleep, true);
  }
  M5Canvas& canvas = resumeReading && !coverData_.empty()
                         ? display_.imageCanvas()
                         : display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(&fonts::Font2);
  canvas.setTextSize(2);
  canvas.setTextDatum(middle_center);
  const bool coverDrawn = resumeReading && drawCoverImage(
      canvas, coverData_, coverMediaType_, 18, 18,
      display_.width() - 36, display_.height() - 36);
  if (!coverDrawn) {
    canvas.drawString(ui_strings::get().sleepMode, display_.width() / 2,
                      display_.height() / 2 - 25);
    canvas.setFont(&fonts::Font0);
    canvas.setTextSize(1);
    canvas.drawString(ui_strings::get().wakeWithLever, display_.width() / 2,
                      display_.height() / 2 + 35);
  }
  display_.submitCanvas(canvas, coverDrawn ? RefreshIntent::SleepCoverQuality
                                           : RefreshIntent::FullQuality);
  display_.waitUntilIdle();
  state_ = AppState::Sleeping;
  Serial.printf("Entering low-power sleep; resume=%s\n", resumeReading ? "yes" : "no");
  Serial.flush();
  power_.enterLowPowerSleep();
  lastInteractionMs_ = millis();
  M5.update();
  if (resumeReading && reader_.redrawCurrentPage()) {
    readerView_.renderPageChrome(reader_.book(), reader_.pageNumber(),
                                 reader_.presentationCanvas(),
                                 RefreshIntent::WakeFromSleep);
    state_ = AppState::Reading;
  } else {
    state_ = AppState::FileBrowser;
    browserView_.setModel(currentPath_, &scanner_.entries(), false,
                          scanner_.wasTruncated(), scanner_.error());
    browserDirty_ = true;
  }
  Serial.println("Woke from low-power sleep");
}

bool AppController::startReaderWithSavedState() {
  ReadingState saved;
  bool hasMatchingState = false;
  {
    ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
    hasMatchingState = bus && readingStateStore_.load(kReadingStatePath, saved) &&
                       saved.bookPath == epubParser_.book().filePath;
  }
  if (hasMatchingState) {
    PageAnchor anchor;
    anchor.spineIndex = saved.spineIndex;
    anchor.uncompressedOffset = saved.textOffset;
    anchor.parserCheckpoint = saved.parserCheckpoint;
    std::vector<PageAnchor> history;
    history.reserve(saved.previousPages.size());
    for (const ReadingHistoryEntry& entry : saved.previousPages) {
      PageAnchor previous;
      previous.spineIndex = entry.spineIndex;
      previous.uncompressedOffset = entry.textOffset;
      previous.parserCheckpoint = entry.parserCheckpoint;
      history.push_back(previous);
    }
    if (reader_.startAt(anchor, saved.fontSize, saved.pageNumber, history,
                        static_cast<ReaderFontFamily>(saved.fontFamily))) {
      Serial.printf("Reading state restored: spine=%lu checkpoint=%lu font=%u page=%lu history=%u\n",
                    static_cast<unsigned long>(anchor.spineIndex),
                    static_cast<unsigned long>(anchor.parserCheckpoint), saved.fontSize,
                    static_cast<unsigned long>(saved.pageNumber),
                    static_cast<unsigned>(history.size()));
      return true;
    }
    Serial.printf("Saved reading state ignored: %s\n", reader_.error().c_str());
  }
  return reader_.start();
}

void AppController::loadCurrentBookCover() {
  loadBookCover(epubParser_, app_config::kMaximumCoverBytes,
                coverData_, coverMediaType_);
}

bool AppController::loadBookCover(EpubParser& parser, size_t maximumBytes,
                                  std::string& data,
                                  std::string& mediaType) {
  data.clear();
  mediaType.clear();
  const EpubBook& book = parser.book();
  std::string coverPath = book.coverPath;
  std::string coverType = book.coverMediaType;
  if (coverPath.empty() && !book.spine.empty()) {
    std::string wrapperPath;
    for (const auto& item : book.manifest) {
      if (item.id == book.spine.front().idref &&
          item.mediaType == "application/xhtml+xml") {
        wrapperPath = item.href;
        break;
      }
    }
    std::string wrapper;
    if (!wrapperPath.empty() && parser.archive().readEntry(
            wrapperPath, app_config::kMaximumCoverDocumentBytes, wrapper)) {
      std::vector<EpubImageReference> images;
      std::string discoveryError;
      if (epub_content::discoverRasterImages(wrapper, wrapperPath, book.manifest,
                                               4, images, discoveryError) &&
          !images.empty()) {
        coverPath = images.front().path;
        coverType = images.front().mediaType;
        Serial.printf("M5EPUB_COVER,status=wrapper_fallback,path=%s\n",
                      coverPath.c_str());
      }
    }
  }
  if (coverPath.empty() ||
      (coverType != "image/jpeg" && coverType != "image/png")) {
    Serial.println("M5EPUB_COVER,status=not_found");
    return false;
  }
  if (!parser.archive().readEntry(coverPath, maximumBytes, data)) {
    Serial.printf("Cover skipped: %s\n", parser.archive().error().c_str());
    data.clear();
    return false;
  }
  mediaType = coverType;
  Serial.printf("M5EPUB_COVER,status=ready,path=%s,bytes=%u,type=%s\n",
                coverPath.c_str(), static_cast<unsigned>(data.size()),
                mediaType.c_str());
  return true;
}

void AppController::scheduleLibraryPreviews() {
  if (libraryParser_) libraryParser_->archive().close();
  size_t visibleCount = 0;
  const std::vector<FileEntry> candidates =
      browserView_.visibleAndNextBooks(visibleCount);
  libraryPreviewQueue_.clear();
  libraryVisibleQueueCount_ = 0;
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (browserView_.hasPreview(candidates[i].fullPath)) continue;
    libraryPreviewQueue_.push_back(candidates[i]);
    if (i < visibleCount) ++libraryVisibleQueueCount_;
  }
  libraryPreviewIndex_ = 0;
  libraryPreviewActive_ = false;
  browserView_.setLoading(libraryVisibleQueueCount_ != 0);
}

void AppController::serviceLibraryPreviews() {
  if (!libraryParser_ || state_ != AppState::FileBrowser || scanner_.isRunning() ||
      M5.Display.displayBusy() || browserDirty_) return;
  if (libraryPreviewIndex_ >= libraryPreviewQueue_.size()) return;
  const FileEntry& entry = libraryPreviewQueue_[libraryPreviewIndex_];
  if (!libraryPreviewActive_) {
    LibraryBookPreview cached;
    cached.path = entry.fullPath;
    if (libraryThumbnailCache_.load(
            entry, cached.title, cached.thumbnailWidth,
            cached.thumbnailHeight, cached.thumbnailPixels)) {
      browserView_.setPreview(std::move(cached));
      Serial.printf("M5EPUB_LIBRARY_CACHE,status=hit,path=%s\n",
                    entry.fullPath.c_str());
      ++libraryPreviewIndex_;
      if (libraryPreviewIndex_ == libraryVisibleQueueCount_) {
        browserView_.setLoading(false);
        browserDirty_ = true;
      }
      return;
    }
    Serial.printf("M5EPUB_LIBRARY_CACHE,status=miss,path=%s\n",
                  entry.fullPath.c_str());
    libraryParser_->start(entry.fullPath);
    libraryPreviewActive_ = true;
    Serial.printf("M5EPUB_LIBRARY_COVER,status=start,path=%s\n",
                  entry.fullPath.c_str());
  }
  const WorkResult result = libraryParser_->processNextChunk();
  const bool metadataReady = libraryParser_->metadataReady();
  if (result == WorkResult::MoreWork && !metadataReady) return;
  if (result == WorkResult::Completed || metadataReady) {
    LibraryBookPreview preview;
    preview.path = entry.fullPath;
    preview.title = libraryParser_->book().title;
    std::string coverData;
    std::string coverMediaType;
    if (loadBookCover(*libraryParser_, app_config::kMaximumLibraryCoverBytes,
                      coverData, coverMediaType)) {
      createCoverThumbnail4(
          coverData, coverMediaType, app_config::kLibraryThumbnailWidth,
          app_config::kLibraryThumbnailHeight, preview.thumbnailWidth,
          preview.thumbnailHeight, preview.thumbnailPixels);
    }
    const bool cacheSaved = libraryThumbnailCache_.save(
        entry, preview.title, preview.thumbnailWidth, preview.thumbnailHeight,
        preview.thumbnailPixels);
    Serial.printf(
        "M5EPUB_LIBRARY_CACHE,status=%s,path=%s,width=%u,height=%u,bytes=%u\n",
        cacheSaved ? "saved" : "save_failed", entry.fullPath.c_str(),
        static_cast<unsigned>(preview.thumbnailWidth),
        static_cast<unsigned>(preview.thumbnailHeight),
        static_cast<unsigned>(preview.thumbnailPixels.size()));
    browserView_.setPreview(std::move(preview));
  } else {
    Serial.printf("M5EPUB_LIBRARY_COVER,status=failed,path=%s,error=%s\n",
                  entry.fullPath.c_str(), libraryParser_->error().c_str());
  }
  libraryParser_->archive().close();
  libraryPreviewActive_ = false;
  ++libraryPreviewIndex_;
  // Rendering each cover separately forces one slow quality refresh per book.
  // Keep the initial placeholders visible and present the completed page once.
  if (libraryPreviewIndex_ == libraryVisibleQueueCount_) {
    browserView_.setLoading(false);
    browserDirty_ = true;
  }
}

void AppController::handleLibraryMenuEvent(const AppEvent& event) {
  if (event.type != AppEventType::Tap || M5.Display.displayBusy()) return;
  if (state_ == AppState::LibraryMenu) {
    if (event.y >= 170 && event.y < 270) {
      state_ = AppState::LibraryPrefetchConfirm;
      browserView_.renderLibraryMenu(true);
    }
#if M5EPUB_ENABLE_WEB_PORTAL
    else if (event.y >= 315 && event.y < 420) {
      if (portal_.running() || wifi_.state() == WifiState::Connected ||
          wifi_.state() == WifiState::Connecting ||
          wifi_.state() == WifiState::Scanning)
        stopUploadServer();
      else
        requestPortalStart(false);
    } else if (event.y >= 470 && event.y < 552) {
      requestPortalStart(true);
    }
#endif
    else if (event.y >= 700 && event.y < 782) {
      state_ = AppState::FileBrowser;
      browserDirty_ = true;
      scheduleLibraryPreviews();
    }
    return;
  }
  if (state_ == AppState::LibraryPrefetchConfirm) {
    if (event.y >= 390 && event.y < 472) beginCardPrefetch();
    else if (event.y >= 545 && event.y < 627) {
      state_ = AppState::LibraryMenu;
#if M5EPUB_ENABLE_WEB_PORTAL
      renderLibraryPortalState(false);
#else
      browserView_.renderLibraryMenu(false);
#endif
    }
    return;
  }
  if (state_ == AppState::LibraryPrefetch && event.y >= 700 && event.y < 782)
    finishCardPrefetch(cardPrefetchPhase_ != CardPrefetchPhase::Done);
}

#if M5EPUB_ENABLE_WEB_PORTAL
void AppController::requestPortalStart(bool showNetworkScreen) {
  if (portal_.running()) portal_.end();
  power_.enableRadio();
  portalStartRequested_ = !showNetworkScreen;
  portalNetworkScreenRequested_ = showNetworkScreen;
  portalNetworksRenderPending_ = showNetworkScreen;
  portalFailureStartedMs_ = 0;
  if (!wifi_.beginScan()) {
    stopUploadServer();
    return;
  }
  if (showNetworkScreen) {
    state_ = AppState::WebPortalNetworks;
    webPortalView_.renderNetworks(wifi_);
  } else {
    renderLibraryPortalState(true);
  }
}

void AppController::startHttpPortal() {
  WebPortalService::Hooks hooks;
  hooks.acquireSd = [this]() {
    display_.waitUntilIdle();
    return spiBus_.tryAcquire(SpiBusOwner::SdCard);
  };
  hooks.releaseSd = [this]() { spiBus_.release(SpiBusOwner::SdCard); };
  hooks.onActivity = [this](const PortalStatus& status) {
    portalStatus_ = status;
    portalStatusDirty_ = true;
    if (status.activity == PortalActivity::UploadCompleted ||
        status.activity == PortalActivity::FilesChanged)
      portalLibraryChanged_ = true;
  };
  if (!portal_.begin(hooks, "/")) {
    wifi_.stop();
    power_.disableRadio();
    portalFailureStartedMs_ = millis();
    return;
  }
  if (!MDNS.begin("m5paper"))
    Serial.println("M5EPUB_PORTAL,mdns=failed");
  else
    MDNS.addService("http", "tcp", 80);
  portalStartRequested_ = false;
  portalNetworkScreenRequested_ = false;
  state_ = AppState::LibraryMenu;
  renderLibraryPortalState(false);
  Serial.printf("M5EPUB_PORTAL,status=started,ssid=%s,ip=%s\n",
                wifi_.ssid().c_str(), wifi_.ip().c_str());
}

void AppController::stopUploadServer() {
  const bool changed = portalLibraryChanged_;
  if (portal_.running()) portal_.end();
  MDNS.end();
  wifi_.stop();
  power_.disableRadio();
  portalStartRequested_ = false;
  portalNetworkScreenRequested_ = false;
  portalStatusDirty_ = false;
  portalLibraryChanged_ = false;
  if (state_ == AppState::WebPortalNetworks ||
      state_ == AppState::WebPortalPassword)
    state_ = AppState::LibraryMenu;
  if (state_ == AppState::LibraryMenu) renderLibraryPortalState(false);
  if (changed) {
    browserView_.clearPreviews();
    state_ = AppState::FileBrowser;
    startDirectory(currentPath_);
  }
  Serial.println("M5EPUB_PORTAL,status=stopped,radio=off");
}

void AppController::renderLibraryPortalState(bool partial) {
  const ui_strings::Text& text = ui_strings::get();
  const char* state = text.uploadOff;
  std::string address;
  if (portal_.running()) {
    state = text.uploadOn;
    address = wifi_.ip() + "  m5paper.local";
    if (portal_.uploadActive()) {
      char progress[48];
      const uint32_t percent = portalStatus_.totalBytes
          ? static_cast<uint32_t>(portalStatus_.completedBytes * 100ULL /
                                  portalStatus_.totalBytes) : 0;
      snprintf(progress, sizeof(progress), "  %lu%%", static_cast<unsigned long>(percent));
      address += progress;
    }
  } else if (wifi_.state() == WifiState::Scanning ||
             wifi_.state() == WifiState::Connecting) {
    state = text.uploadConnecting;
  } else if (wifi_.state() == WifiState::Failed) {
    state = wifi_.lastError().c_str();
  }
  browserView_.renderLibraryMenu(false, state, address.c_str(), partial);
}

void AppController::servicePortal() {
  const uint32_t now = millis();
  wifi_.poll(now);
  if (portal_.running()) {
    portal_.poll();
    const uint32_t portalIdleMs = portal_.idleMs(millis());
    if (portalIdleMs >= app_config::kPortalIdleTimeoutMs) {
      Serial.printf("M5EPUB_PORTAL,status=timeout,idle_ms=%lu\n",
                    static_cast<unsigned long>(portalIdleMs));
      stopUploadServer();
      return;
    }
    if (portalStatusDirty_ && state_ == AppState::LibraryMenu &&
        !M5.Display.displayBusy()) {
      portalStatusDirty_ = false;
      renderLibraryPortalState(true);
    }
  }
  if (wifi_.state() == WifiState::ScanDone) {
    if (portalStartRequested_) {
      bool connecting = false;
      for (const auto& network : wifi_.networks()) {
        if (network.saved && wifi_.connectSaved(network.ssid)) {
          connecting = true;
          renderLibraryPortalState(true);
          break;
        }
      }
      if (!connecting) {
        portalStartRequested_ = false;
        state_ = AppState::WebPortalNetworks;
        portalNetworksRenderPending_ = true;
        webPortalView_.renderNetworks(wifi_);
        portalNetworksRenderPending_ = false;
      }
    } else if (state_ == AppState::WebPortalNetworks &&
               portalNetworksRenderPending_ &&
               !M5.Display.displayBusy()) {
      webPortalView_.renderNetworks(wifi_);
      portalNetworksRenderPending_ = false;
    }
  } else if (wifi_.state() == WifiState::Connected && !portal_.running()) {
    startHttpPortal();
  } else if (wifi_.state() == WifiState::Failed && !portalFailureStartedMs_) {
    portalStartRequested_ = false;
    portalFailureStartedMs_ = now;
    if (state_ == AppState::WebPortalNetworks)
      webPortalView_.renderNetworks(wifi_);
    else if (state_ == AppState::LibraryMenu)
      renderLibraryPortalState(true);
  }
  if (portalFailureStartedMs_ &&
      state_ != AppState::WebPortalNetworks &&
      state_ != AppState::WebPortalPassword &&
      now - portalFailureStartedMs_ >= 4000) {
    portalFailureStartedMs_ = 0;
    stopUploadServer();
  }
}

void AppController::handleWebPortalEvent(const AppEvent& event) {
  if (event.type != AppEventType::Tap || M5.Display.displayBusy()) return;
  if (state_ == AppState::WebPortalNetworks) {
    const WebPortalViewHit hit = webPortalView_.hitNetworks(
        event.x, event.y, wifi_.networks().size());
    if (hit.action == WebPortalViewAction::Cancel) {
      stopUploadServer();
    } else if (hit.action == WebPortalViewAction::Rescan) {
      portalFailureStartedMs_ = 0;
      wifi_.beginScan();
      portalNetworksRenderPending_ = true;
      webPortalView_.renderNetworks(wifi_);
    } else if (hit.action == WebPortalViewAction::ForgetAll) {
      portalFailureStartedMs_ = 0;
      wifi_.forgetAll();
      wifi_.beginScan();
      portalNetworksRenderPending_ = true;
      webPortalView_.renderNetworks(wifi_);
    } else if (hit.action == WebPortalViewAction::Network &&
               hit.networkIndex < wifi_.networks().size()) {
      portalSelectedSsid_ = wifi_.networks()[hit.networkIndex].ssid;
      if (wifi_.networks()[hit.networkIndex].saved) {
        wifi_.connectSaved(portalSelectedSsid_);
      } else if (!wifi_.networks()[hit.networkIndex].secured) {
        wifi_.connect(portalSelectedSsid_, "", true);
      } else {
        portalPassword_.clear();
        state_ = AppState::WebPortalPassword;
        webPortalView_.renderPassword(portalSelectedSsid_, portalPassword_,
                                      portalRememberPassword_);
      }
    }
  } else {
    const WebPortalViewHit hit = webPortalView_.hitPassword(event.x, event.y);
    if (hit.action == WebPortalViewAction::Key && portalPassword_.size() < 64)
      portalPassword_ += hit.key;
    else if (hit.action == WebPortalViewAction::Backspace && !portalPassword_.empty())
      portalPassword_.pop_back();
    else if (hit.action == WebPortalViewAction::ToggleRemember)
      portalRememberPassword_ = !portalRememberPassword_;
    else if (hit.action == WebPortalViewAction::Connect) {
      if (wifi_.connect(portalSelectedSsid_, portalPassword_, portalRememberPassword_)) {
        state_ = AppState::LibraryMenu;
        renderLibraryPortalState(false);
        return;
      }
    } else if (hit.action == WebPortalViewAction::Cancel) {
      state_ = AppState::WebPortalNetworks;
      webPortalView_.renderNetworks(wifi_);
      return;
    }
    webPortalView_.renderPassword(portalSelectedSsid_, portalPassword_,
                                  portalRememberPassword_);
  }
}
#endif

void AppController::beginCardPrefetch() {
  if (!libraryParser_) return;
  libraryParser_->archive().close();
  libraryPreviewActive_ = false;
  cardPrefetchDirectory_.close();
  cardPrefetchQueue_.close();
  cardPrefetchDirectories_.clear();
  cardPrefetchDirectories_.push_back("/");
  cardPrefetchBook_ = {};
  cardPrefetchBookActive_ = false;
  cardPrefetchTruncated_ = false;
  cardPrefetchTotal_ = 0;
  cardPrefetchCompleted_ = 0;
  cardPrefetchLastRenderMs_ = 0;
  {
    ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
    if (!bus) return;
    if (!SD.exists("/.m5epub-cache")) SD.mkdir("/.m5epub-cache");
    if (SD.exists(kCardPrefetchQueuePath)) SD.remove(kCardPrefetchQueuePath);
    cardPrefetchQueue_ = SD.open(kCardPrefetchQueuePath, FILE_WRITE);
  }
  if (!cardPrefetchQueue_) {
    cardPrefetchPhase_ = CardPrefetchPhase::Done;
    state_ = AppState::LibraryPrefetch;
    cardPrefetchTruncated_ = true;
    renderCardPrefetchProgress(true);
    return;
  }
  cardPrefetchPhase_ = CardPrefetchPhase::Scanning;
  state_ = AppState::LibraryPrefetch;
  renderCardPrefetchProgress(true);
  Serial.println("M5EPUB_CARD_PREFETCH,status=started");
}

void AppController::serviceCardPrefetch() {
  if (state_ != AppState::LibraryPrefetch ||
      cardPrefetchPhase_ == CardPrefetchPhase::Idle ||
      cardPrefetchPhase_ == CardPrefetchPhase::Done ||
      M5.Display.displayBusy())
    return;

  if (cardPrefetchPhase_ == CardPrefetchPhase::Scanning) {
    bool scanFinished = false;
    {
      ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
      if (!bus) return;
      if (!cardPrefetchDirectory_) {
        if (cardPrefetchDirectories_.empty()) {
          scanFinished = true;
        } else {
          const std::string path = cardPrefetchDirectories_.back();
          cardPrefetchDirectories_.pop_back();
          cardPrefetchDirectory_ = SD.open(path.c_str(), FILE_READ);
          if (!cardPrefetchDirectory_) {
            cardPrefetchTruncated_ = true;
            scanFinished = cardPrefetchDirectories_.empty();
          }
        }
      }
      for (uint8_t i = 0; !scanFinished && cardPrefetchDirectory_ &&
                          i < app_config::kCardPrefetchScanBatchSize; ++i) {
        fs::File file = cardPrefetchDirectory_.openNextFile();
        if (!file) {
          cardPrefetchDirectory_.close();
          scanFinished = cardPrefetchDirectories_.empty();
          break;
        }
        const std::string fullPath = file.path();
        const std::string name = path_utils::fileName(fullPath);
        if (!path_utils::isHiddenName(name)) {
          if (file.isDirectory()) {
            cardPrefetchDirectories_.push_back(fullPath);
          } else if (path_utils::hasEpubExtension(name) &&
                     fullPath.size() <= kMaximumQueuedPathBytes) {
            CardPrefetchRecord record;
            record.pathLength = static_cast<uint16_t>(fullPath.size());
            record.size = file.size();
            record.modifiedTime = static_cast<uint64_t>(file.getLastWrite());
            const bool queued =
                cardPrefetchQueue_.write(
                    reinterpret_cast<const uint8_t*>(&record), sizeof(record)) ==
                    sizeof(record) &&
                cardPrefetchQueue_.write(
                    reinterpret_cast<const uint8_t*>(fullPath.data()),
                    fullPath.size()) == fullPath.size();
            if (queued) ++cardPrefetchTotal_;
            else cardPrefetchTruncated_ = true;
          }
        }
        file.close();
      }
      if (scanFinished) {
        cardPrefetchDirectory_.close();
        cardPrefetchQueue_.close();
        cardPrefetchQueue_ = SD.open(kCardPrefetchQueuePath, FILE_READ);
        cardPrefetchPhase_ = CardPrefetchPhase::Indexing;
      }
    }
    renderCardPrefetchProgress();
    return;
  }

  if (!cardPrefetchBookActive_) {
    if (cardPrefetchCompleted_ >= cardPrefetchTotal_) {
      cardPrefetchPhase_ = CardPrefetchPhase::Done;
      cardPrefetchQueue_.close();
      {
        ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
        if (bus && SD.exists(kCardPrefetchQueuePath))
          SD.remove(kCardPrefetchQueuePath);
      }
      renderCardPrefetchProgress(true);
      Serial.printf("M5EPUB_CARD_PREFETCH,status=completed,books=%u\n",
                    static_cast<unsigned>(cardPrefetchCompleted_));
      return;
    }
    CardPrefetchRecord record;
    std::string path;
    {
      ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
      if (!bus || !cardPrefetchQueue_ ||
          cardPrefetchQueue_.read(reinterpret_cast<uint8_t*>(&record),
                                  sizeof(record)) != sizeof(record) ||
          record.pathLength == 0 || record.pathLength > kMaximumQueuedPathBytes) {
        cardPrefetchTruncated_ = true;
        cardPrefetchCompleted_ = cardPrefetchTotal_;
        return;
      }
      path.resize(record.pathLength);
      if (cardPrefetchQueue_.read(reinterpret_cast<uint8_t*>(&path[0]),
                                  path.size()) != path.size()) {
        cardPrefetchTruncated_ = true;
        cardPrefetchCompleted_ = cardPrefetchTotal_;
        return;
      }
    }
    cardPrefetchBook_ = FileEntry(path_utils::fileName(path), path, false,
                                  record.size, record.modifiedTime);
    std::string title, pixels;
    uint16_t width = 0, height = 0;
    if (libraryThumbnailCache_.load(cardPrefetchBook_, title, width, height,
                                    pixels)) {
      ++cardPrefetchCompleted_;
      renderCardPrefetchProgress();
      return;
    }
    libraryParser_->start(cardPrefetchBook_.fullPath);
    cardPrefetchBookActive_ = true;
  }

  const WorkResult result = libraryParser_->processNextChunk();
  const bool metadataReady = libraryParser_->metadataReady();
  if (result == WorkResult::MoreWork && !metadataReady) return;
  if (result == WorkResult::Completed || metadataReady) {
    std::string coverData, coverMediaType, pixels;
    uint16_t width = 0, height = 0;
    if (loadBookCover(*libraryParser_, app_config::kMaximumLibraryCoverBytes,
                      coverData, coverMediaType))
      createCoverThumbnail4(
          coverData, coverMediaType, app_config::kLibraryThumbnailWidth,
          app_config::kLibraryThumbnailHeight, width, height, pixels);
    libraryThumbnailCache_.save(cardPrefetchBook_, libraryParser_->book().title,
                                width, height, pixels);
  } else {
    cardPrefetchTruncated_ = true;
  }
  libraryParser_->archive().close();
  cardPrefetchBookActive_ = false;
  ++cardPrefetchCompleted_;
  renderCardPrefetchProgress();
}

void AppController::renderCardPrefetchProgress(bool force) {
  const uint32_t now = millis();
  if (!force && now - cardPrefetchLastRenderMs_ <
                    app_config::kCardPrefetchProgressRefreshMs)
    return;
  cardPrefetchLastRenderMs_ = now;
  browserView_.renderPrefetchProgress(
      cardPrefetchPhase_ == CardPrefetchPhase::Scanning,
      cardPrefetchPhase_ == CardPrefetchPhase::Scanning ? cardPrefetchTotal_
                                                        : cardPrefetchCompleted_,
      cardPrefetchTotal_, cardPrefetchPhase_ == CardPrefetchPhase::Done,
      cardPrefetchTruncated_);
}

void AppController::finishCardPrefetch(bool cancelled) {
  if (libraryParser_) libraryParser_->archive().close();
  cardPrefetchDirectory_.close();
  cardPrefetchQueue_.close();
  cardPrefetchDirectories_.clear();
  cardPrefetchBookActive_ = false;
  {
    ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
    if (bus && SD.exists(kCardPrefetchQueuePath)) SD.remove(kCardPrefetchQueuePath);
  }
  Serial.printf("M5EPUB_CARD_PREFETCH,status=%s,completed=%u,total=%u\n",
                cancelled ? "cancelled" : "closed",
                static_cast<unsigned>(cardPrefetchCompleted_),
                static_cast<unsigned>(cardPrefetchTotal_));
  cardPrefetchPhase_ = CardPrefetchPhase::Idle;
  state_ = AppState::FileBrowser;
  browserView_.setModel(currentPath_, &scanner_.entries(), false,
                        scanner_.wasTruncated(), scanner_.error());
  browserDirty_ = true;
  scheduleLibraryPreviews();
}

void AppController::markReadingStateDirty(bool pageChanged) {
  persistPolicy_.markDirty(millis(), pageChanged);
}

void AppController::requestForcedPersist(PersistReason reason) {
  forcedPersistPending_ = true;
  forcedPersistReason_ = reason;
}

bool AppController::persistReadingState(PersistReason reason, bool forced) {
  if (!persistPolicy_.state().dirty || !sdCard_.status().mounted ||
      M5.Display.displayBusy())
    return false;
  const PageAnchor anchor = reader_.currentAnchor();
  ReadingState state;
  state.bookPath = reader_.book().filePath;
  state.spineIndex = anchor.spineIndex;
  state.textOffset = anchor.uncompressedOffset;
  state.parserCheckpoint = anchor.parserCheckpoint;
  state.fontSize = reader_.fontSize();
  state.fontFamily = static_cast<uint8_t>(reader_.fontFamily());
  state.pageNumber = reader_.pageNumber();
  const std::vector<PageAnchor> previous = reader_.previousPageAnchors(
      app_config::kPersistedPreviousPageAnchors);
  state.previousPages.reserve(previous.size());
  for (const PageAnchor& page : previous) {
    ReadingHistoryEntry entry;
    entry.spineIndex = page.spineIndex;
    entry.textOffset = page.uncompressedOffset;
    entry.parserCheckpoint = page.parserCheckpoint;
    state.previousPages.push_back(entry);
  }
  ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
  if (!bus) return false;
  const uint32_t started = micros();
  if (readingStateStore_.save(kReadingStatePath, state)) {
    persistPolicy_.recordSaved(millis());
    ++readingStateWrites_;
    Serial.printf("M5EPUB_PERSIST,reason=%s,duration_us=%lu,page_count=%lu,writes=%lu,forced=%u\n",
                  persistReasonName(reason),
                  static_cast<unsigned long>(micros() - started),
                  static_cast<unsigned long>(state.pageNumber),
                  static_cast<unsigned long>(readingStateWrites_), forced ? 1U : 0U);
    return true;
  }
  Serial.printf("Reading state save failed: %s\n", readingStateStore_.error().c_str());
  return false;
}

void AppController::beginPageTurnMeasurement(PageTurnKind kind) {
  pageTurnStartedMs_ = millis();
  measuringPageTurn_ = true;
  const uint32_t handlingUs = micros();
  const uint32_t eventUs = inputRecognizedUs_ == 0 ? handlingUs : inputRecognizedUs_;
  display_.beginPageTurnMetric(kind, eventUs, handlingUs - eventUs);
}

void AppController::recordPageReady() {
  display_.markPageReady(micros());
  const uint32_t now = millis();
  if (measuringBookOpen_) {
    readerMetrics_.lastBookOpenMs = now - bookOpenStartedMs_;
    ++readerMetrics_.completedBookOpens;
    measuringBookOpen_ = false;
    Serial.printf("Metric book_open_ms=%lu count=%lu\n",
                  static_cast<unsigned long>(readerMetrics_.lastBookOpenMs),
                  static_cast<unsigned long>(readerMetrics_.completedBookOpens));
  }
  if (measuringPageTurn_) {
    readerMetrics_.lastPageTurnMs = now - pageTurnStartedMs_;
    ++readerMetrics_.completedPageTurns;
    measuringPageTurn_ = false;
    const DisplayRefreshMetrics& refresh = display_.refreshMetrics();
    Serial.printf("Metric page_turn_ms=%lu count=%lu refresh_quality=%lu refresh_text=%lu periodic=%lu\n",
                  static_cast<unsigned long>(readerMetrics_.lastPageTurnMs),
                  static_cast<unsigned long>(readerMetrics_.completedPageTurns),
                  static_cast<unsigned long>(refresh.qualityRefreshes),
                  static_cast<unsigned long>(refresh.textRefreshes),
                  static_cast<unsigned long>(refresh.periodicQualityRefreshes));
  }
}

void AppController::startDirectory(const std::string& path) {
  currentPath_ = path.empty() ? "/" : path;
  bookSelected_ = false;
  if (libraryParser_) libraryParser_->archive().close();
  libraryPreviewQueue_.clear();
  libraryPreviewActive_ = false;
  if (!scanner_.start(currentPath_)) {
    browserView_.setModel(currentPath_, &scanner_.entries(), false, false,
                          scanner_.error());
  } else {
    browserView_.setModel(currentPath_, &scanner_.entries(), true, false, {});
  }
  browserDirty_ = true;
}

void AppController::handleBrowserEvent(const AppEvent& event) {
  if (event.type == AppEventType::None || event.type == AppEventType::TouchPressed ||
      event.type == AppEventType::TouchReleased) return;
  if (bookSelected_) {
    if (event.type == AppEventType::Tap) {
      bookSelected_ = false;
      browserView_.setModel(currentPath_, &scanner_.entries(), false,
                            scanner_.wasTruncated(), scanner_.error());
      browserDirty_ = true;
    }
    return;
  }
  if (scanner_.isRunning() || M5.Display.displayBusy()) return;
  if (event.type == AppEventType::SwipeUp || event.type == AppEventType::SwipeLeft) {
    const size_t previous = browserView_.page();
    browserView_.nextPage();
    browserDirty_ = browserView_.page() != previous;
    if (browserDirty_) scheduleLibraryPreviews();
    return;
  }
  if (event.type == AppEventType::SwipeDown || event.type == AppEventType::SwipeRight) {
    const size_t previous = browserView_.page();
    browserView_.previousPage();
    browserDirty_ = browserView_.page() != previous;
    if (browserDirty_) scheduleLibraryPreviews();
    return;
  }
  if (event.type != AppEventType::Tap) return;
  if (browserView_.headerAt(event.y)) {
    if (libraryParser_) libraryParser_->archive().close();
    libraryPreviewActive_ = false;
    state_ = AppState::LibraryMenu;
#if M5EPUB_ENABLE_WEB_PORTAL
    renderLibraryPortalState(false);
#else
    browserView_.renderLibraryMenu(false);
#endif
    return;
  }
  const int navigation = browserView_.navigationAt(event.x, event.y);
  if (navigation != 0) {
    const size_t previous = browserView_.page();
    if (navigation < 0) browserView_.previousPage();
    else browserView_.nextPage();
    browserDirty_ = browserView_.page() != previous;
    if (browserDirty_) scheduleLibraryPreviews();
    return;
  }
  const int logicalIndex = browserView_.itemAt(event.x, event.y);
  if (logicalIndex < 0) return;
  browserView_.showSelection(event.x, event.y);
  if (currentPath_ != "/" && logicalIndex == 0) {
    const std::string parentPath = path_utils::parent(currentPath_);
    browserRestorePending_ = false;
    if (!browserHistory_.empty() && browserHistory_.back().path == parentPath) {
      browserRestorePath_ = browserHistory_.back().path;
      browserRestorePage_ = browserHistory_.back().page;
      browserRestorePending_ = true;
      browserHistory_.pop_back();
    }
    startDirectory(parentPath);
    return;
  }
  const size_t entryIndex = static_cast<size_t>(logicalIndex) - (currentPath_ == "/" ? 0 : 1);
  if (entryIndex >= scanner_.entries().size()) return;
  const FileEntry& entry = scanner_.entries()[entryIndex];
  if (entry.isDirectory) {
    BrowserHistoryEntry history;
    history.path = currentPath_;
    history.page = browserView_.page();
    browserHistory_.push_back(history);
    browserRestorePending_ = false;
    startDirectory(entry.fullPath);
  } else {
    if (libraryParser_) libraryParser_->archive().close();
    libraryPreviewActive_ = false;
    browserView_.clearPreviews();
    Serial.printf("EPUB selected: %s (%llu bytes)\n", entry.fullPath.c_str(), entry.size);
    browserView_.showBookSelected(entry); epubParser_.start(entry.fullPath);
    bookOpenStartedMs_ = millis(); measuringBookOpen_ = true;
    measuringPageTurn_ = false;
    state_ = AppState::OpeningBook; bookSelected_ = false; readingLayout_ = false;
  }
}
