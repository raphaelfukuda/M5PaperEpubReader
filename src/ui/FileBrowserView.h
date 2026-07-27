#pragma once

#include <string>
#include <vector>
#include "View.h"
#include "hal/DisplayManager.h"
#include "storage/FileEntry.h"
struct EpubBook;

struct LibraryBookPreview {
  std::string path;
  std::string title;
  std::string thumbnailPixels;
  uint16_t thumbnailWidth = 0;
  uint16_t thumbnailHeight = 0;
  uint32_t useStamp = 0;
};

class FileBrowserView : public View {
 public:
  explicit FileBrowserView(DisplayManager& display) : display_(display) {}
  void setModel(const std::string& path, const std::vector<FileEntry>* entries,
                bool scanning, bool truncated, const std::string& error);
  void render() override;
  void nextPage();
  void previousPage();
  size_t pageCount() const;
  size_t page() const { return page_; }
  void setPage(size_t page);
  int itemAt(int32_t x, int32_t y) const;
  int navigationAt(int32_t x, int32_t y) const;
  bool headerAt(int32_t y) const;
  void setLoading(bool loading) { loading_ = loading; }
  void showSelection(int32_t x, int32_t y);
  void clearPreviews();
  void setPreview(LibraryBookPreview preview);
  bool hasPreview(const std::string& path) const;
  std::vector<FileEntry> visibleBooks() const;
  std::vector<FileEntry> visibleAndNextBooks(size_t& visibleCount) const;
  void showBookSelected(const FileEntry& entry);
  void showBookInfo(const EpubBook& book);
  void showBookError(const std::string& message);
  void renderLibraryMenu(bool confirmation = false);
  void renderPrefetchProgress(bool scanning, size_t completed, size_t total,
                              bool done, bool truncated);

 private:
  size_t itemsPerPage() const { return 4; }
  size_t visibleItemCount() const;
  std::string truncateToWidth(const std::string& text, int32_t width);
  DisplayManager& display_;
  std::string path_ = "/";
  std::string error_;
  const std::vector<FileEntry>* entries_ = nullptr;
  size_t page_ = 0;
  bool scanning_ = false;
  bool truncated_ = false;
  bool loading_ = false;
  std::vector<LibraryBookPreview> previews_;
  uint32_t previewUseStamp_ = 0;
};
