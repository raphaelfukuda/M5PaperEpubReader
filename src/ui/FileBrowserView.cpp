#include "FileBrowserView.h"

#include <algorithm>
#include "AppConfig.h"
#include "UiStrings.h"
#include "epub/EpubBook.h"

void FileBrowserView::setModel(const std::string& path,
                               const std::vector<FileEntry>* entries,
                               bool scanning, bool truncated,
                               const std::string& error) {
  if (path_ != path) page_ = 0;
  path_ = path;
  entries_ = entries;
  scanning_ = scanning;
  truncated_ = truncated;
  error_ = error;
  const size_t pages = pageCount();
  if (page_ >= pages) page_ = pages - 1;
}

size_t FileBrowserView::rowsPerPage() const {
  return (display_.height() - app_config::kBrowserHeaderHeight -
          app_config::kBrowserFooterHeight) / app_config::kBrowserRowHeight;
}

size_t FileBrowserView::visibleItemCount() const {
  const size_t parent = path_ == "/" ? 0 : 1;
  return parent + (entries_ ? entries_->size() : 0);
}

size_t FileBrowserView::pageCount() const {
  const size_t rows = rowsPerPage();
  return std::max<size_t>(1, (visibleItemCount() + rows - 1) / rows);
}

void FileBrowserView::nextPage() {
  if (page_ + 1 < pageCount()) ++page_;
}

void FileBrowserView::previousPage() {
  if (page_ > 0) --page_;
}

int FileBrowserView::itemAt(int32_t y) const {
  if (y < app_config::kBrowserHeaderHeight ||
      y >= display_.height() - app_config::kBrowserFooterHeight) return -1;
  const size_t row = (y - app_config::kBrowserHeaderHeight) /
                     app_config::kBrowserRowHeight;
  const size_t index = page_ * rowsPerPage() + row;
  return index < visibleItemCount() ? static_cast<int>(index) : -1;
}

std::string FileBrowserView::truncateToWidth(const std::string& text, int32_t width) {
  M5Canvas& canvas = display_.canvas();
  if (canvas.textWidth(text.c_str()) <= width) return text;
  std::string result = text;
  while (result.size() > 3) {
    result.resize(result.size() - 1);
    if (canvas.textWidth((result + "...").c_str()) <= width) return result + "...";
  }
  return "...";
}

void FileBrowserView::render() {
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextDatum(middle_left);
  canvas.setTextSize(2);
  const ui_strings::Text& text = ui_strings::get();
  canvas.drawString(text.library, 20, 25);
  canvas.setTextSize(1);
  canvas.drawString(truncateToWidth(path_, display_.width() - 40).c_str(), 20, 65);
  canvas.drawFastHLine(0, app_config::kBrowserHeaderHeight - 1, display_.width(), TFT_BLACK);

  if (!error_.empty()) {
    canvas.setTextDatum(middle_center);
    canvas.drawString(error_.c_str(), display_.width() / 2, display_.height() / 2);
  } else {
    const size_t rows = rowsPerPage();
    const size_t first = page_ * rows;
    const size_t count = visibleItemCount();
    for (size_t row = 0; row < rows && first + row < count; ++row) {
      const size_t logicalIndex = first + row;
      std::string label;
      if (path_ != "/" && logicalIndex == 0) {
        label = text.parentDirectory;
      } else {
        const size_t entryIndex = logicalIndex - (path_ == "/" ? 0 : 1);
        const FileEntry& entry = (*entries_)[entryIndex];
        label = entry.isDirectory ? text.directoryPrefix : text.epubPrefix;
        label += entry.name;
      }
      const int32_t top = app_config::kBrowserHeaderHeight + row * app_config::kBrowserRowHeight;
      canvas.drawString(truncateToWidth(label, display_.width() - 40).c_str(), 20,
                        top + app_config::kBrowserRowHeight / 2);
      canvas.drawFastHLine(15, top + app_config::kBrowserRowHeight - 1,
                           display_.width() - 30, TFT_BLACK);
    }
    if (count == 0 && !scanning_) {
      canvas.setTextDatum(middle_center);
      canvas.drawString(text.emptyDirectory, display_.width() / 2,
                        display_.height() / 2);
    }
  }

  canvas.setTextDatum(middle_center);
  char footer[64];
  if (scanning_) snprintf(footer, sizeof(footer), "%s", text.scanning);
  else if (truncated_) snprintf(footer, sizeof(footer), "%s", text.listTruncated);
  else snprintf(footer, sizeof(footer), "%s %u/%u", text.page, static_cast<unsigned>(page_ + 1),
                static_cast<unsigned>(pageCount()));
  canvas.drawString(footer, display_.width() / 2,
                    display_.height() - app_config::kBrowserFooterHeight / 2);
  display_.submitFull(RefreshIntent::FullQuality);
}

void FileBrowserView::showSelection(int32_t y) {
  const int item = itemAt(y);
  if (item < 0) return;
  const size_t row = static_cast<size_t>(item) % rowsPerPage();
  const int32_t top = app_config::kBrowserHeaderHeight + row * app_config::kBrowserRowHeight;
  display_.highlightRegion(0, top, display_.width(), app_config::kBrowserRowHeight);
}

void FileBrowserView::showBookSelected(const FileEntry& entry) {
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextDatum(middle_center);
  canvas.setTextSize(2);
  canvas.drawString(ui_strings::get().selectedBook, display_.width() / 2, 180);
  canvas.setTextSize(1);
  canvas.drawString(truncateToWidth(entry.name, display_.width() - 50).c_str(),
                    display_.width() / 2, 280);
  canvas.drawString(ui_strings::get().touchToReturn, display_.width() / 2, 430);
  display_.submitFull(RefreshIntent::FullQuality);
}

void FileBrowserView::showBookInfo(const EpubBook& book) {
  M5Canvas& canvas = display_.canvas(); canvas.fillScreen(TFT_WHITE); canvas.setTextColor(TFT_BLACK, TFT_WHITE); canvas.setTextDatum(middle_center);
  canvas.setTextSize(2); canvas.drawString("EPUB aberto", display_.width() / 2, 90); canvas.setTextSize(1);
  canvas.drawString(truncateToWidth(book.title.empty() ? "(sem titulo)" : book.title, display_.width() - 40).c_str(), display_.width() / 2, 180);
  canvas.drawString(truncateToWidth(book.author.empty() ? "(autor desconhecido)" : book.author, display_.width() - 40).c_str(), display_.width() / 2, 240);
  canvas.drawString(("Idioma: " + (book.language.empty() ? std::string("-") : book.language)).c_str(), display_.width() / 2, 300);
  char counts[96]; snprintf(counts, sizeof(counts), "Manifest: %u   Spine: %u", static_cast<unsigned>(book.manifest.size()), static_cast<unsigned>(book.spine.size())); canvas.drawString(counts, display_.width() / 2, 360);
  canvas.drawString(truncateToWidth("OPF: " + book.packagePath, display_.width() - 40).c_str(), display_.width() / 2, 420);
  canvas.drawString("Toque para voltar a biblioteca", display_.width() / 2, 520); display_.submitFull(RefreshIntent::FullQuality);
}

void FileBrowserView::showBookError(const std::string& message) {
  M5Canvas& canvas = display_.canvas(); canvas.fillScreen(TFT_WHITE); canvas.setTextColor(TFT_BLACK, TFT_WHITE); canvas.setTextDatum(middle_center);
  canvas.setTextSize(2); canvas.drawString(ui_strings::get().invalidEpub, display_.width() / 2, 180); canvas.setTextSize(1);
  canvas.drawString(truncateToWidth(message, display_.width() - 40).c_str(), display_.width() / 2, 300); canvas.drawString(ui_strings::get().touchToReturn, display_.width() / 2, 420); display_.submitFull(RefreshIntent::FullQuality);
}
