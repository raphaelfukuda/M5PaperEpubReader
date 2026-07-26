#pragma once
#include "hal/DisplayManager.h"
#include "epub/EpubBook.h"
class ReaderView {
 public: explicit ReaderView(DisplayManager& display) : display_(display) {}
  void renderPageChrome(const EpubBook& book, uint32_t pageNumber);
 private: DisplayManager& display_;
};
