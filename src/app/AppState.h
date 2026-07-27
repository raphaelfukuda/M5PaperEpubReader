#pragma once

enum class AppState {
  Booting,
  SdError,
  FileBrowser,
  LibraryMenu,
  LibraryPrefetchConfirm,
  LibraryPrefetch,
  OpeningBook,
  Reading,
  ReaderMenu,
  ErrorDialog,
  Sleeping
};
