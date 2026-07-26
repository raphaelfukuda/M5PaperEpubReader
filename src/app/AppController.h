#pragma once

#include <SD.h>
#include <Preferences.h>
#include "AppState.h"
#include "hal/DisplayManager.h"
#include "hal/PowerManager.h"
#include "hal/SdCardService.h"
#include "hal/SpiBusGuard.h"
#include "hal/TouchController.h"
#include "storage/DirectoryScanner.h"
#include "storage/ReadingStateStore.h"
#include "ui/FileBrowserView.h"
#include "epub/EpubParser.h"
#include "reader/ReaderController.h"
#include "ui/ReaderView.h"
#include "ui/ReaderMenuView.h"
#include "diagnostics/PerformanceMetrics.h"

class AppController {
 public:
  void begin();
  void tick();
  AppState state() const { return state_; }

 private:
  AppState state_ = AppState::Booting;
  SpiBusGuard spiBus_;
  DisplayManager display_{spiBus_};
  SdCardService sdCard_{spiBus_};
  ReadingStateStore readingStateStore_{SD};
  TouchController touch_;
  PowerManager power_;
  Preferences preferences_;
  DirectoryScanner scanner_{spiBus_};
  FileBrowserView browserView_{display_};
  EpubParser epubParser_{spiBus_};
  ReaderController reader_{epubParser_, display_.canvas()};
  ReaderView readerView_{display_};
  ReaderMenuView readerMenuView_{display_};
  std::string currentPath_ = "/";
  bool browserDirty_ = false;
  bool bookSelected_ = false;
  bool readingLayout_ = false;
  bool readingStateDirty_ = false;
  uint32_t lastYieldMs_ = 0;
  uint32_t lastLeverActionMs_ = 0;
  uint32_t bookOpenStartedMs_ = 0;
  uint32_t pageTurnStartedMs_ = 0;
  bool measuringBookOpen_ = false;
  bool measuringPageTurn_ = false;
  size_t tocPage_ = 0;
  ReaderRuntimeMetrics readerMetrics_;

  void startDirectory(const std::string& path);
  void handleBrowserEvent(const AppEvent& event);
  void handleReaderEvent(const AppEvent& event);
  void handleReaderMenuEvent(const AppEvent& event);
  void handleReaderLever();
  bool startReaderWithSavedState();
  void markReadingStateDirty();
  void persistReadingState();
  void beginPageTurnMeasurement();
  void recordPageReady();
};
