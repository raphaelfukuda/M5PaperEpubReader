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
}

void AppController::tick() {
  M5.update();
  const AppEvent event = touch_.poll();
  if (state_ == AppState::Reading) handleReaderLever();
  if (state_ == AppState::FileBrowser) handleBrowserEvent(event);
  else if (state_ == AppState::Reading) handleReaderEvent(event);
  else if (state_ == AppState::ReaderMenu) handleReaderMenuEvent(event);
  else if (state_ == AppState::ErrorDialog &&
           event.type == AppEventType::Tap && bookSelected_) {
    state_ = AppState::FileBrowser; bookSelected_ = false;
    browserView_.setModel(currentPath_, &scanner_.entries(), false,
                          scanner_.wasTruncated(), scanner_.error()); browserDirty_ = true;
  }

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
      if (work == WorkResult::Completed) { readerView_.renderPageChrome(reader_.book(), reader_.pageNumber()); state_ = AppState::Reading; bookSelected_ = true; markReadingStateDirty(); recordPageReady(); Serial.printf("XHTML page rendered: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
      else if (work == WorkResult::Failed) { browserView_.showBookError(reader_.error()); state_ = AppState::ErrorDialog; bookSelected_ = true; Serial.printf("Reader error: %s\n", reader_.error().c_str()); }
    }
  }

  if (state_ == AppState::Reading && M5.Display.displayBusy() == false) {
    if (readingStateDirty_) persistReadingState();
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
  if (now - lastYieldMs_ >= app_config::kIdleYieldIntervalMs) {
    lastYieldMs_ = now;
    yield();
  }
}

void AppController::handleReaderLever() {
  if (M5.Display.displayBusy() || reader_.isPrefetching()) return;
  const uint32_t now = millis();
  if (now - lastLeverActionMs_ < 600) return;
  WorkResult result = WorkResult::Idle;
  const char* action = nullptr;
  if (M5.BtnA.wasPressed()) {
    result = reader_.increaseFontSize();
    action = "increase";
  } else if (M5.BtnC.wasPressed()) {
    result = reader_.decreaseFontSize();
    action = "decrease";
  }
  if (!action) return;
  beginPageTurnMeasurement();
  lastLeverActionMs_ = now;
  const PageAnchor anchor = reader_.currentAnchor();
  Serial.printf("Lever font %s: %u px; anchor=%lu:%lu\n", action,
                reader_.fontSize(),
                static_cast<unsigned long>(anchor.spineIndex),
                static_cast<unsigned long>(anchor.parserCheckpoint));
  if (result == WorkResult::Completed) {
    readerView_.renderPageChrome(reader_.book(), reader_.pageNumber());
    markReadingStateDirty();
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

void AppController::handleReaderEvent(const AppEvent& event) {
  if (M5.Display.displayBusy() || reader_.isPrefetching()) return;
  const bool tap = event.type == AppEventType::Tap;
  if (tap && event.y < app_config::kReaderTopActionHeight) {
    readerMenuView_.render(reader_.book(), reader_.currentAnchor(),
                           reader_.pageNumber(), reader_.fontSize());
    state_ = AppState::ReaderMenu;
    Serial.println("Reader menu opened");
    return;
  }
  const bool previous = (tap && event.x < display_.width() / 2) || event.type == AppEventType::SwipeRight;
  const bool next = (tap && event.x >= display_.width() / 2) || event.type == AppEventType::SwipeLeft;
  if (previous) {
    beginPageTurnMeasurement();
    const WorkResult result = reader_.requestPreviousPage();
    if (result == WorkResult::Completed) { readerView_.renderPageChrome(reader_.book(), reader_.pageNumber()); markReadingStateDirty(); recordPageReady(); Serial.printf("Previous page: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
    else if (result != WorkResult::MoreWork) measuringPageTurn_ = false;
  } else if (next) {
    beginPageTurnMeasurement();
    const WorkResult result = reader_.requestNextPage();
    if (result == WorkResult::Completed) { readerView_.renderPageChrome(reader_.book(), reader_.pageNumber()); markReadingStateDirty(); recordPageReady(); Serial.printf("Cached next page: %lu\n", static_cast<unsigned long>(reader_.pageNumber())); }
    else if (result == WorkResult::MoreWork) { state_ = AppState::OpeningBook; readingLayout_ = true; bookSelected_ = false; Serial.println("Generating next page"); }
    else { measuringPageTurn_ = false; Serial.println("End of current chapter"); }
  }
}

void AppController::handleReaderMenuEvent(const AppEvent& event) {
  if (M5.Display.displayBusy() || event.type != AppEventType::Tap) return;
  const ReaderMenuAction action = readerMenuView_.actionAt(event.x, event.y);
  if (action == ReaderMenuAction::BackToLibrary) {
    markReadingStateDirty();
    persistReadingState();
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
    readingStateDirty_ = false;
    beginPageTurnMeasurement();
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
    readingStateDirty_ = false;
    beginPageTurnMeasurement();
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
    if (reader_.redrawCurrentPage())
      readerView_.renderPageChrome(reader_.book(), reader_.pageNumber());
    state_ = AppState::Reading;
    Serial.println("Reader menu closed");
  }
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
    if (reader_.startAt(anchor, saved.fontSize)) {
      Serial.printf("Reading state restored: spine=%lu checkpoint=%lu font=%u\n",
                    static_cast<unsigned long>(anchor.spineIndex),
                    static_cast<unsigned long>(anchor.parserCheckpoint), saved.fontSize);
      return true;
    }
    Serial.printf("Saved reading state ignored: %s\n", reader_.error().c_str());
  }
  return reader_.start();
}

void AppController::markReadingStateDirty() { readingStateDirty_ = true; }

void AppController::persistReadingState() {
  if (!readingStateDirty_ || !sdCard_.status().mounted || M5.Display.displayBusy()) return;
  const PageAnchor anchor = reader_.currentAnchor();
  ReadingState state;
  state.bookPath = reader_.book().filePath;
  state.spineIndex = anchor.spineIndex;
  state.textOffset = anchor.uncompressedOffset;
  state.parserCheckpoint = anchor.parserCheckpoint;
  state.fontSize = reader_.fontSize();
  ScopedSpiBus bus(spiBus_, SpiBusOwner::SdCard);
  if (!bus) return;
  if (readingStateStore_.save(kReadingStatePath, state)) readingStateDirty_ = false;
  else Serial.printf("Reading state save failed: %s\n", readingStateStore_.error().c_str());
}

void AppController::beginPageTurnMeasurement() {
  pageTurnStartedMs_ = millis();
  measuringPageTurn_ = true;
}

void AppController::recordPageReady() {
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
