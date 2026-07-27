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
  std::string coverData;
  std::string coverMediaType;
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
  int itemAt(int32_t x, int32_t y) const;
  void showSelection(int32_t x, int32_t y);
  void clearPreviews();
  void setPreview(LibraryBookPreview preview);
  std::vector<FileEntry> visibleBooks() const;
  void showBookSelected(const FileEntry& entry);
  void showBookInfo(const EpubBook& book);
  void showBookError(const std::string& message);

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
  std::vector<LibraryBookPreview> previews_;
};
