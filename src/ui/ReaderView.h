#pragma once
#include "hal/DisplayManager.h"
#include "epub/EpubBook.h"
class ReaderView {
 public: explicit ReaderView(DisplayManager& display) : display_(display) {}
  void renderPageChrome(const EpubBook& book, uint32_t pageNumber,
                        M5Canvas& canvas,
                        RefreshIntent intent = RefreshIntent::ReadingPage);
 private: DisplayManager& display_;
};
