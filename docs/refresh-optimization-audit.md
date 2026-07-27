# Refresh optimization audit

Audit date: 2026-07-26. Target: original ESP32 M5Paper only. The API checks
below use the resolved M5GFX 0.2.26 and M5Unified 0.2.19 sources under
`.pio/libdeps/m5stack-paper`, not an upstream development branch.

| # | Finding | Status | Evidence | Performance consequence |
|---:|---|---|---|---|
| 1 | `DisplayManager` owns a full-screen 1-bit canvas in PSRAM. | Confirmed | `src/hal/DisplayManager.cpp`, `DisplayManager::begin()` | Every full submission reads a 64,800-byte PSRAM sprite before the panel update. |
| 2 | `ReadingPage` selects `epd_text`. | Confirmed | `src/hal/DisplayManager.cpp`, `DisplayManager::setRefreshIntent()` | Normal turns always request the slower GL16 text waveform. |
| 3 | `submitFull()` waits, pushes the sprite, then calls `display()`. | Confirmed | `src/hal/DisplayManager.cpp`, `DisplayManager::submitFull()` | Submission is serialized behind the previous physical update and uploads the whole sprite. |
| 4 | Every reading-page turn submits the full screen. | Confirmed | `src/ui/ReaderView.cpp`, `ReaderView::renderPageChrome()`; `DisplayManager::submitFull()` | Static header and mostly static chrome are retransmitted and refreshed. |
| 5 | The reader owns `prefetchCanvas_`. | Confirmed | `src/reader/ReaderController.h`; `ReaderController::resetSession()` | A second full-screen 1-bit buffer is already allocated and can support real double buffering. |
| 6 | Prefetch lays out into the second canvas. | Confirmed | `src/reader/ReaderController.cpp`, `requestPrefetch()` and `prepareUncachedPage()` | CPU rendering work is performed early, but its rendered pixels are not reused for presentation. |
| 7 | Consuming a prefetched page redraws its cached text into the main canvas. | Confirmed | `src/reader/ReaderController.cpp`, `requestNextPage()` and `renderCachedPage()` | Layout/drawing is duplicated even though the back canvas previously held the ready page. |
| 8 | A ready `prefetchCanvas_` is not submitted directly. | Confirmed | `src/reader/ReaderController.cpp`; no display submission API accepts this canvas | The fastest possible prefetched path is absent. |
| 9 | Prefetch advances only while `displayBusy()` is false. | Confirmed | `src/app/AppController.cpp`, `AppController::tick()` | Physical panel time is CPU-idle instead of being used for speculative CPU work. |
| 10 | Reader events are ignored while prefetch is active. | Confirmed | `src/app/AppController.cpp`, `handleReaderEvent()` and `handleReaderLever()` | Explicit input can feel lost and prefetch incorrectly outranks the user. |
| 11 | Reader events are ignored while the display is busy. | Confirmed | `src/app/AppController.cpp`, `handleReaderEvent()`, `handleReaderLever()`, `handleReaderMenuEvent()` | Input is sampled but discarded instead of queued for safe execution. |
| 12 | Progress may be saved at the first idle tick after each page. | Partially confirmed | `src/app/AppController.cpp`, `tick()`, `markReadingStateDirty()`, `persistReadingState()` | It never writes during display busy, but it runs before prefetch as soon as the panel is idle and can add SD latency after nearly every turn. |
| 13 | Display and SD share one `SpiBusGuard`. | Confirmed | `src/app/AppController.h`; `src/hal/DisplayManager.cpp`; `src/epub/EpubArchive.cpp`; `src/hal/SdCardService.cpp` | Concurrent bus access is prevented, but scheduling and guard duration remain important. |
| 14 | Runtime documentation reports about 4 MB PSRAM rather than the initial 8 MB expectation. | Confirmed | `docs/performance.md`, “Primeira medição física”; `src/diagnostics/Logger.cpp` | Two 64,800-byte 1-bit canvases are viable, but all further allocations require measured margins. |

## Verified display APIs

M5GFX 0.2.26 declares regional `display(x, y, w, h)`, `waitDisplay()`,
`displayBusy()`, `M5Canvas::getBuffer()` and sprite submission. In
`Panel_IT8951::display()` the modes map as follows:

| M5GFX mode | IT8951 update mode |
|---|---|
| `epd_fastest` | `DU4` |
| `epd_fast` | `DU` |
| `epd_text` | `GL16` |
| `epd_quality` (default branch) | `GC16` |

`Panel_IT8951::_set_area()` expands horizontal update bounds to an internal
four-pixel boundary. M5GFX already owns this controller-specific behavior, so
the application must use the public regional API rather than raw IT8951
commands. No physical speed or ghosting conclusion follows from this audit.

## Architectural constraint discovered

`EpubArchive::readEntryChunk()` holds the SD guard only around a bounded ZIP
read, then releases it before `HtmlTokenizer` and layout execute in
`ReaderController::processNextChunk()`. The SPI/CPU boundary therefore exists
partially already, but the public work API still combines both operations in a
single cooperative step and cannot schedule CPU-only continuation during panel
busy time.
