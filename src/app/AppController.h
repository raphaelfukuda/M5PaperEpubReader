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
#include "input/PendingReaderActions.h"
#include "storage/PersistPolicy.h"
#include "AppConfig.h"

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
  PersistPolicy persistPolicy_{app_config::kReadingStateIdleSaveDelayMs,
                               app_config::kReadingStateMaxSaveIntervalMs,
                               app_config::kReadingStatePageSaveThreshold};
  bool forcedPersistPending_ = false;
  PersistReason forcedPersistReason_ = PersistReason::ExplicitRequest;
  uint32_t readingStateWrites_ = 0;
  uint32_t lastYieldMs_ = 0;
  uint32_t lastLeverActionMs_ = 0;
  uint32_t lastInteractionMs_ = 0;
  uint32_t bookOpenStartedMs_ = 0;
  uint32_t pageTurnStartedMs_ = 0;
  bool measuringBookOpen_ = false;
  bool measuringPageTurn_ = false;
  size_t tocPage_ = 0;
  ReaderRuntimeMetrics readerMetrics_;
  PendingReaderActions pendingReaderActions_;

  void startDirectory(const std::string& path);
  void handleBrowserEvent(const AppEvent& event);
  void handleReaderEvent(const AppEvent& event);
  void handleReaderMenuEvent(const AppEvent& event);
  void handleReaderLever(PendingReaderAction action);
  void executeReaderAction(PendingReaderAction action, uint32_t queuedAtUs = 0);
  void queueReaderAction(PendingReaderAction action);
  bool canEnterAutomaticSleep() const;
  void enterSleepMode();
  bool startReaderWithSavedState();
  void markReadingStateDirty(bool pageChanged = true);
  void requestForcedPersist(PersistReason reason);
  bool persistReadingState(PersistReason reason, bool forced = false);
  void beginPageTurnMeasurement(PageTurnKind kind);
  void recordPageReady();
  uint32_t inputRecognizedUs_ = 0;
  bool manualRefreshPending_ = false;
  uint32_t manualRefreshQueuedUs_ = 0;
  void requestManualRefresh();
  void serviceManualRefresh();
};
