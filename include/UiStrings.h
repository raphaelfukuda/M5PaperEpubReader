#pragma once

enum class UiLanguage : unsigned char { English = 0, Portuguese = 1 };

namespace ui_strings {
struct Text {
  const char* library;
  const char* scanning;
  const char* emptyDirectory;
  const char* parentDirectory;
  const char* directoryPrefix;
  const char* epubPrefix;
  const char* listTruncated;
  const char* selectedBook;
  const char* page;
  const char* readingMenu;
  const char* backToLibrary;
  const char* tableOfContents;
  const char* restartBook;
  const char* closeMenu;
  const char* zoom;
  const char* fontFamily;
  const char* chapter;
  const char* approximateProgress;
  const char* restartQuestion;
  const char* restartWarning;
  const char* restartConfirm;
  const char* cancel;
  const char* language;
  const char* invalidEpub;
  const char* touchToReturn;
  const char* sleepNow;
  const char* sleepMode;
  const char* wakeWithLever;
  const char* loading;
  const char* libraryMenu;
  const char* prefetchCard;
  const char* prefetchWarning;
  const char* startPrefetch;
  const char* scanningCard;
  const char* indexingCovers;
  const char* completed;
};

const Text& get();
UiLanguage language();
void setLanguage(UiLanguage language);
const char* languageName();
}  // namespace ui_strings
