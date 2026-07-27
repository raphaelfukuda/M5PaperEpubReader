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
#include "storage/LibraryThumbnailCache.h"
#include "AppConfig.h"
#if M5EPUB_ENABLE_WEB_PORTAL
#include "net/WifiService.h"
#include "net/WebPortalService.h"
#include "ui/WebPortalView.h"
#endif

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
  LibraryThumbnailCache libraryThumbnailCache_{SD, spiBus_};
  TouchController touch_;
  PowerManager power_;
  Preferences preferences_;
  DirectoryScanner scanner_{spiBus_};
  FileBrowserView browserView_{display_};
  EpubParser epubParser_{spiBus_};
  EpubParser* libraryParser_ = nullptr;
  ReaderController reader_{epubParser_, display_.canvas()};
  ReaderView readerView_{display_};
  ReaderMenuView readerMenuView_{display_};
#if M5EPUB_ENABLE_WEB_PORTAL
  WifiService wifi_;
  WebPortalService portal_{SD};
  WebPortalView webPortalView_{display_};
  PortalStatus portalStatus_;
  bool portalStatusDirty_ = false;
  bool portalStartRequested_ = false;
  bool portalNetworkScreenRequested_ = false;
  bool portalNetworksRenderPending_ = false;
  bool portalLibraryChanged_ = false;
  bool portalRememberPassword_ = true;
  std::string portalSelectedSsid_;
  std::string portalPassword_;
  uint32_t portalFailureStartedMs_ = 0;
#endif
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
  std::string coverData_;
  std::string coverMediaType_;
  std::vector<FileEntry> libraryPreviewQueue_;
  size_t libraryPreviewIndex_ = 0;
  bool libraryPreviewActive_ = false;
  size_t libraryVisibleQueueCount_ = 0;
  enum class CardPrefetchPhase : uint8_t { Idle, Scanning, Indexing, Done };
  CardPrefetchPhase cardPrefetchPhase_ = CardPrefetchPhase::Idle;
  std::vector<std::string> cardPrefetchDirectories_;
  fs::File cardPrefetchDirectory_;
  fs::File cardPrefetchQueue_;
  FileEntry cardPrefetchBook_;
  bool cardPrefetchBookActive_ = false;
  bool cardPrefetchTruncated_ = false;
  size_t cardPrefetchTotal_ = 0;
  size_t cardPrefetchCompleted_ = 0;
  uint32_t cardPrefetchLastRenderMs_ = 0;
  struct BrowserHistoryEntry {
    std::string path;
    size_t page = 0;
  };
  std::vector<BrowserHistoryEntry> browserHistory_;
  bool browserRestorePending_ = false;
  std::string browserRestorePath_;
  size_t browserRestorePage_ = 0;

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
  void loadCurrentBookCover();
  bool loadBookCover(EpubParser& parser, size_t maximumBytes,
                     std::string& data, std::string& mediaType);
  void scheduleLibraryPreviews();
  void serviceLibraryPreviews();
  void handleLibraryMenuEvent(const AppEvent& event);
  void beginCardPrefetch();
  void serviceCardPrefetch();
  void finishCardPrefetch(bool cancelled);
  void renderCardPrefetchProgress(bool force = false);
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
  void renderCurrentReaderPage();
#if M5EPUB_ENABLE_WEB_PORTAL
  void requestPortalStart(bool showNetworkScreen);
  void servicePortal();
  void startHttpPortal();
  void stopUploadServer();
  void handleWebPortalEvent(const AppEvent& event);
  void renderLibraryPortalState(bool partial = false);
#endif
};
