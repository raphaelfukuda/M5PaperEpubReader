#include "AppController.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include "AppConfig.h"
#include "UiStrings.h"
#include "diagnostics/Logger.h"
#include "storage/PathUtils.h"

namespace { constexpr const char* kReadingStatePath = "/.m5epub-reading-state"; }

void AppController::begin() {
  const uint32_t started = millis();
  Logger::begin();
  auto config = M5.config();
  M5.begin(config);
  power_.begin();
  preferences_.begin("m5epub", false);
  const uint8_t savedLanguage = preferences_.getUChar("language", 0);
  ui_strings::setLanguage(savedLanguage == 1 ? UiLanguage::Portuguese
                                             : UiLanguage::English);

  const bool boardOk = M5.getBoard() == m5::board_t::board_M5Paper;
  const bool displayOk = display_.begin();
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
  else if (state_ == AppState::Reading) handleReaderEvent(event);
  else if (state_ == AppState::ReaderMenu) handleReaderMenuEvent(event);
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
        if (!startReaderWithSavedState()) { Serial.printf("Reader start error: %s\n", reader_.error().c_str()); browserView_.showBookError(reader_.error()); state_ = AppState::ErrorDialog; bookSelected_ = true; }
        else readingLayout_ = true;
      } else if (work == WorkResult::Failed) {
      Serial.printf("EPUB parse error: %s\n", epubParser_.error().c_str()); browserView_.showBookError(epubParser_.error()); state_ = AppState::ErrorDialog; bookSelected_ = true;
      }
    } else {
      const WorkResult work = reader_.processNextChunk();
      if (work == WorkResult::Completed) { readerView_.renderPageChrome(reader_.book(), reader_.pageNumber(), reader_.presentationCanvas()); state_ = AppState::Reading; bookSelected_ = true; markReadingStateDirty(); recordPageReady(); Serial.printf("XHTML page rendered: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
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
      browserDirty_ = true;
      Serial.printf("Directory scan: path=%s entries=%u truncated=%s\n",
                    currentPath_.c_str(), static_cast<unsigned>(scanner_.entries().size()),
                    scanner_.wasTruncated() ? "yes" : "no");
    }
  }
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
                           reader_.pageNumber(), reader_.fontSize());
    state_ = AppState::ReaderMenu;
    Serial.println("Reader menu opened");
    return;
  }
  if (pendingAction == PendingReaderAction::PreviousPage) {
    beginPageTurnMeasurement(PageTurnKind::CachedPrevious);
    const WorkResult result = reader_.requestPreviousPage();
    if (result == WorkResult::Completed) { readerView_.renderPageChrome(reader_.book(), reader_.pageNumber(), reader_.presentationCanvas()); markReadingStateDirty(); recordPageReady(); Serial.printf("Previous page: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
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
    if (result == WorkResult::Completed) { readerView_.renderPageChrome(reader_.book(), reader_.pageNumber(), reader_.presentationCanvas()); markReadingStateDirty(); recordPageReady(); Serial.printf("Cached next page: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
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
    readerView_.renderPageChrome(reader_.book(), reader_.pageNumber(), reader_.presentationCanvas());
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
                           reader_.pageNumber(), reader_.fontSize());
    Serial.printf("UI language: %s\n", ui_strings::languageName());
  } else if (action == ReaderMenuAction::EnterSleep) {
    enterSleepMode();
  } else if (action == ReaderMenuAction::CancelRestart) {
    readerMenuView_.render(reader_.book(), reader_.currentAnchor(),
                           reader_.pageNumber(), reader_.fontSize());
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
      readerView_.renderPageChrome(reader_.book(), reader_.pageNumber(), reader_.presentationCanvas());
    recordPageReady();
    state_ = AppState::Reading;
    Serial.println("Reader menu closed");
  }
}

bool AppController::canEnterAutomaticSleep() const {
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
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(&fonts::Font2);
  canvas.setTextSize(2);
  canvas.setTextDatum(middle_center);
  canvas.drawString(ui_strings::get().sleepMode, display_.width() / 2,
                    display_.height() / 2 - 45);
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  canvas.drawString(ui_strings::get().wakeWithLever, display_.width() / 2,
                    display_.height() / 2 + 35);
  display_.submitFull(RefreshIntent::FullQuality);
  display_.waitUntilIdle();
  state_ = AppState::Sleeping;
  Serial.printf("Entering low-power sleep; resume=%s\n", resumeReading ? "yes" : "no");
  Serial.flush();
  power_.enterLowPowerSleep();
  lastInteractionMs_ = millis();
  M5.update();
  if (resumeReading && reader_.redrawCurrentPage()) {
    readerView_.renderPageChrome(reader_.book(), reader_.pageNumber(), reader_.presentationCanvas());
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
    if (reader_.startAt(anchor, saved.fontSize, saved.pageNumber, history)) {
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
    return;
  }
  if (event.type == AppEventType::SwipeDown || event.type == AppEventType::SwipeRight) {
    const size_t previous = browserView_.page();
    browserView_.previousPage();
    browserDirty_ = browserView_.page() != previous;
    return;
  }
  if (event.type != AppEventType::Tap) return;
  const int logicalIndex = browserView_.itemAt(event.y);
  if (logicalIndex < 0) return;
  browserView_.showSelection(event.y);
  if (currentPath_ != "/" && logicalIndex == 0) {
    startDirectory(path_utils::parent(currentPath_));
    return;
  }
  const size_t entryIndex = static_cast<size_t>(logicalIndex) - (currentPath_ == "/" ? 0 : 1);
  if (entryIndex >= scanner_.entries().size()) return;
  const FileEntry& entry = scanner_.entries()[entryIndex];
  if (entry.isDirectory) {
    startDirectory(entry.fullPath);
  } else {
    Serial.printf("EPUB selected: %s (%llu bytes)\n", entry.fullPath.c_str(), entry.size);
    browserView_.showBookSelected(entry); epubParser_.start(entry.fullPath);
    bookOpenStartedMs_ = millis(); measuringBookOpen_ = true;
    measuringPageTurn_ = false;
    state_ = AppState::OpeningBook; bookSelected_ = false; readingLayout_ = false;
  }
}
