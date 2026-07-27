#pragma once

#include <stdint.h>
#include "epub/EpubBook.h"
#include "hal/DisplayManager.h"
#include "layout/PageAnchor.h"
#include "layout/LayoutStyle.h"

enum class ReaderMenuAction {
  None,
  BackToLibrary,
  OpenTableOfContents,
  TocPreviousPage,
  TocNextPage,
  TocBack,
  RestartBook,
  ToggleLanguage,
  CycleFontFamily,
  EnterSleep,
  ConfirmRestart,
  CancelRestart,
  Close
};

class ReaderMenuView {
 public:
  explicit ReaderMenuView(DisplayManager& display) : display_(display) {}
  void render(const EpubBook& book, const PageAnchor& anchor,
              uint32_t pageNumber, uint16_t fontSize,
              ReaderFontFamily fontFamily, const std::string& coverData,
              const std::string& coverMediaType);
  void renderRestartConfirmation();
  void renderTableOfContents(const EpubBook& book, size_t page = 0);
  ReaderMenuAction actionAt(int32_t x, int32_t y) const;
  int tocIndexAt(int32_t y, size_t entryCount) const;

 private:
  DisplayManager& display_;
  bool confirmingRestart_ = false;
  bool showingToc_ = false;
  size_t tocPage_ = 0;
};
