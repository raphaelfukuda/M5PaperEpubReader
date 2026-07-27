#pragma once

enum class AppState {
  Booting,
  SdError,
  FileBrowser,
  LibraryMenu,
  LibraryPrefetchConfirm,
  LibraryPrefetch,
  WebPortalNetworks,
  WebPortalPassword,
  OpeningBook,
  Reading,
  ReaderMenu,
  ErrorDialog,
  Sleeping
};
