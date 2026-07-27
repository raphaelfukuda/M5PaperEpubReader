#include "FileBrowserView.h"

#include <algorithm>
#include "AppConfig.h"
#include "UiStrings.h"
#include "epub/EpubBook.h"
#include "layout/ReaderFont.h"
#include "layout/PortugueseTextRenderer.h"
#include "CoverRenderer.h"

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

size_t FileBrowserView::visibleItemCount() const {
  const size_t parent = path_ == "/" ? 0 : 1;
  return parent + (entries_ ? entries_->size() : 0);
}

size_t FileBrowserView::pageCount() const {
  return std::max<size_t>(1, (visibleItemCount() + itemsPerPage() - 1) /
                                itemsPerPage());
}

void FileBrowserView::nextPage() {
  if (page_ + 1 < pageCount()) ++page_;
}

void FileBrowserView::previousPage() {
  if (page_ > 0) --page_;
}

void FileBrowserView::setPage(size_t page) {
  page_ = std::min(page, pageCount() - 1);
}

int FileBrowserView::itemAt(int32_t x, int32_t y) const {
  if (y < app_config::kBrowserHeaderHeight ||
      y >= display_.height() - app_config::kBrowserFooterHeight ||
      x < 0 || x >= display_.width()) return -1;
  const int32_t contentHeight = display_.height() - app_config::kBrowserHeaderHeight -
                                app_config::kBrowserFooterHeight;
  const size_t row = (y - app_config::kBrowserHeaderHeight) / (contentHeight / 2);
  const size_t column = x < display_.width() / 2 ? 0 : 1;
  const size_t index = page_ * itemsPerPage() + row * 2 + column;
  return index < visibleItemCount() ? static_cast<int>(index) : -1;
}

int FileBrowserView::navigationAt(int32_t x, int32_t y) const {
  if (y < display_.height() - app_config::kBrowserFooterHeight ||
      y >= display_.height() || pageCount() <= 1)
    return 0;
  if (x < display_.width() / 3 && page_ > 0) return -1;
  if (x >= (display_.width() * 2) / 3 && page_ + 1 < pageCount()) return 1;
  return 0;
}

bool FileBrowserView::headerAt(int32_t y) const {
  return y >= 0 && y < app_config::kBrowserHeaderHeight;
}

void FileBrowserView::clearPreviews() { previews_.clear(); }

void FileBrowserView::setPreview(LibraryBookPreview preview) {
  preview.useStamp = ++previewUseStamp_;
  for (auto& existing : previews_) {
    if (existing.path == preview.path) {
      existing = std::move(preview);
      return;
    }
  }
  previews_.push_back(std::move(preview));
  constexpr size_t kMaximumRamPreviews = 20;
  if (previews_.size() > kMaximumRamPreviews) {
    const auto oldest = std::min_element(
        previews_.begin(), previews_.end(),
        [](const LibraryBookPreview& left, const LibraryBookPreview& right) {
          return left.useStamp < right.useStamp;
        });
    previews_.erase(oldest);
  }
}

bool FileBrowserView::hasPreview(const std::string& path) const {
  return std::any_of(previews_.begin(), previews_.end(),
                     [&path](const LibraryBookPreview& preview) {
                       return preview.path == path;
                     });
}

std::vector<FileEntry> FileBrowserView::visibleBooks() const {
  std::vector<FileEntry> result;
  if (!entries_) return result;
  const size_t first = page_ * itemsPerPage();
  const size_t end = std::min(visibleItemCount(), first + itemsPerPage());
  for (size_t logical = first; logical < end; ++logical) {
    if (path_ != "/" && logical == 0) continue;
    const size_t index = logical - (path_ == "/" ? 0 : 1);
    if (index < entries_->size() && !(*entries_)[index].isDirectory)
      result.push_back((*entries_)[index]);
  }
  return result;
}

std::vector<FileEntry> FileBrowserView::visibleAndNextBooks(
    size_t& visibleCount) const {
  std::vector<FileEntry> result = visibleBooks();
  visibleCount = result.size();
  if (!entries_ || page_ + 1 >= pageCount()) return result;
  const size_t first = (page_ + 1) * itemsPerPage();
  const size_t end = std::min(visibleItemCount(), first + itemsPerPage());
  for (size_t logical = first; logical < end; ++logical) {
    if (path_ != "/" && logical == 0) continue;
    const size_t index = logical - (path_ == "/" ? 0 : 1);
    if (index < entries_->size() && !(*entries_)[index].isDirectory)
      result.push_back((*entries_)[index]);
  }
  return result;
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
  M5Canvas& canvas = previews_.empty() ? display_.canvas() : display_.imageCanvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(reader_font::forSize(24, ReaderFontFamily::Compact));
  canvas.setTextSize(1);
  canvas.setTextDatum(middle_left);
  const ui_strings::Text& text = ui_strings::get();
  canvas.drawString(text.library, 20, 25);
  canvas.setFont(reader_font::forSize(16, ReaderFontFamily::Compact));
  canvas.drawString(truncateToWidth(path_, display_.width() - 40).c_str(), 20, 65);
  canvas.drawFastHLine(0, app_config::kBrowserHeaderHeight - 1, display_.width(), TFT_BLACK);

  if (!error_.empty()) {
    canvas.setTextDatum(middle_center);
    canvas.drawString(error_.c_str(), display_.width() / 2, display_.height() / 2);
  } else {
    const size_t first = page_ * itemsPerPage();
    const size_t count = visibleItemCount();
    const int32_t contentTop = app_config::kBrowserHeaderHeight;
    const int32_t contentHeight = display_.height() - contentTop -
                                  app_config::kBrowserFooterHeight;
    const int32_t cardWidth = display_.width() / 2;
    const int32_t cardHeight = contentHeight / 2;
    for (size_t slot = 0; slot < itemsPerPage() && first + slot < count; ++slot) {
      const size_t logicalIndex = first + slot;
      std::string label;
      const FileEntry* entry = nullptr;
      if (path_ != "/" && logicalIndex == 0) {
        label = text.parentDirectory;
      } else {
        const size_t entryIndex = logicalIndex - (path_ == "/" ? 0 : 1);
        entry = &(*entries_)[entryIndex];
        label = entry->name;
      }
      const int32_t column = slot % 2;
      const int32_t row = slot / 2;
      const int32_t left = column * cardWidth;
      const int32_t top = contentTop + row * cardHeight;
      const int32_t coverX = left + 30;
      const int32_t coverY = top + 14;
      const int32_t coverWidth = cardWidth - 60;
      const int32_t coverHeight = cardHeight - 78;
      canvas.drawRect(coverX, coverY, coverWidth, coverHeight, TFT_BLACK);
      bool coverDrawn = false;
      if (entry && !entry->isDirectory) {
        for (auto& preview : previews_) {
          if (preview.path == entry->fullPath) {
            preview.useStamp = ++previewUseStamp_;
            coverDrawn = drawCoverThumbnail4(
                canvas, preview.thumbnailPixels, preview.thumbnailWidth,
                preview.thumbnailHeight, coverX + 3, coverY + 3,
                coverWidth - 6, coverHeight - 6);
            if (!preview.title.empty()) label = preview.title;
            break;
          }
        }
      }
      if (!coverDrawn) {
        canvas.setTextDatum(middle_center);
        canvas.setFont(reader_font::forSize(24, ReaderFontFamily::Compact));
        if (!entry) {
          const int32_t centerX = left + cardWidth / 2;
          const int32_t centerY = coverY + coverHeight / 2;
          canvas.drawFastHLine(centerX - 48, centerY, 96, TFT_BLACK);
          canvas.drawLine(centerX - 48, centerY, centerX - 10, centerY - 38,
                          TFT_BLACK);
          canvas.drawLine(centerX - 48, centerY, centerX - 10, centerY + 38,
                          TFT_BLACK);
          canvas.drawLine(centerX - 47, centerY + 1, centerX - 9,
                          centerY + 39, TFT_BLACK);
        } else {
          canvas.drawString(entry->isDirectory ? "DIR" : "EPUB",
                            left + cardWidth / 2, coverY + coverHeight / 2);
        }
      }
      canvas.setFont(reader_font::forSize(16, ReaderFontFamily::Compact));
      canvas.setTextDatum(top_left);
      const std::string visibleLabel = portuguese_text::truncateToWidth(
          canvas, label, cardWidth - 28);
      const int32_t labelX = left + (cardWidth -
          portuguese_text::width(canvas, visibleLabel)) / 2;
      portuguese_text::draw(canvas, visibleLabel, labelX,
                            top + cardHeight - 52);
    }
    if (count == 0 && !scanning_) {
      canvas.setTextDatum(middle_center);
      canvas.drawString(text.emptyDirectory, display_.width() / 2,
                        display_.height() / 2);
    }
  }

  canvas.setTextDatum(middle_center);
  char footer[64];
  if (loading_) snprintf(footer, sizeof(footer), "%s", text.loading);
  else if (scanning_) snprintf(footer, sizeof(footer), "%s", text.scanning);
  else if (truncated_) snprintf(footer, sizeof(footer), "%s", text.listTruncated);
  else snprintf(footer, sizeof(footer), "%s %u/%u", text.page, static_cast<unsigned>(page_ + 1),
                static_cast<unsigned>(pageCount()));
  canvas.drawString(footer, display_.width() / 2,
                    display_.height() - app_config::kBrowserFooterHeight / 2);
  canvas.setFont(reader_font::forSize(24, ReaderFontFamily::Compact));
  if (page_ > 0) {
    canvas.drawRect(8, display_.height() - app_config::kBrowserFooterHeight + 5,
                    74, app_config::kBrowserFooterHeight - 10, TFT_BLACK);
    canvas.drawString("<", 45,
                      display_.height() - app_config::kBrowserFooterHeight / 2);
  }
  if (page_ + 1 < pageCount()) {
    canvas.drawRect(display_.width() - 82,
                    display_.height() - app_config::kBrowserFooterHeight + 5,
                    74, app_config::kBrowserFooterHeight - 10, TFT_BLACK);
    canvas.drawString(">", display_.width() - 45,
                      display_.height() - app_config::kBrowserFooterHeight / 2);
  }
  display_.submitCanvas(canvas, previews_.empty() ? RefreshIntent::FullQuality
                                                   : RefreshIntent::ImageQuality);
}

void FileBrowserView::showSelection(int32_t x, int32_t y) {
  const int item = itemAt(x, y);
  if (item < 0) return;
  const size_t slot = static_cast<size_t>(item) % itemsPerPage();
  const int32_t contentHeight = display_.height() - app_config::kBrowserHeaderHeight -
                                app_config::kBrowserFooterHeight;
  display_.highlightRegion((slot % 2) * display_.width() / 2,
                           app_config::kBrowserHeaderHeight +
                               (slot / 2) * (contentHeight / 2),
                           display_.width() / 2, contentHeight / 2);
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

void FileBrowserView::renderLibraryMenu(bool confirmation) {
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(reader_font::forSize(24, ReaderFontFamily::Compact));
  canvas.setTextSize(1);
  canvas.setTextDatum(top_left);
  const ui_strings::Text& text = ui_strings::get();
  const std::string heading = confirmation ? text.prefetchCard : text.libraryMenu;
  portuguese_text::draw(canvas, heading,
      (display_.width() - portuguese_text::width(canvas, heading)) / 2, 35);
  canvas.drawFastHLine(28, 85, display_.width() - 56, TFT_BLACK);
  canvas.setFont(reader_font::forSize(16, ReaderFontFamily::Compact));
  if (confirmation) {
    portuguese_text::draw(canvas, text.prefetchWarning,
        (display_.width() - portuguese_text::width(canvas, text.prefetchWarning)) / 2,
        190);
    const std::string detail1 = ui_strings::language() == UiLanguage::Portuguese
        ? "As capas serão preparadas antecipadamente"
        : "Covers will be prepared in advance";
    const std::string detail2 = ui_strings::language() == UiLanguage::Portuguese
        ? "para deixar a biblioteca mais rápida."
        : "to make the library faster.";
    portuguese_text::draw(canvas, detail1,
        (display_.width() - portuguese_text::width(canvas, detail1)) / 2, 245);
    portuguese_text::draw(canvas, detail2,
        (display_.width() - portuguese_text::width(canvas, detail2)) / 2, 280);
    canvas.drawRect(45, 390, display_.width() - 90, 82, TFT_BLACK);
    portuguese_text::draw(canvas, text.startPrefetch,
        (display_.width() - portuguese_text::width(canvas, text.startPrefetch)) / 2,
        420);
    canvas.drawRect(45, 545, display_.width() - 90, 82, TFT_BLACK);
    portuguese_text::draw(canvas, text.cancel,
        (display_.width() - portuguese_text::width(canvas, text.cancel)) / 2,
        575);
  } else {
    canvas.drawRect(45, 170, display_.width() - 90, 100, TFT_BLACK);
    portuguese_text::draw(canvas, text.prefetchCard,
        (display_.width() - portuguese_text::width(canvas, text.prefetchCard)) / 2,
        207);
    canvas.drawRect(45, 700, display_.width() - 90, 82, TFT_BLACK);
    portuguese_text::draw(canvas, text.closeMenu,
        (display_.width() - portuguese_text::width(canvas, text.closeMenu)) / 2,
        730);
  }
  display_.submitFull(RefreshIntent::FullQuality);
}

void FileBrowserView::renderPrefetchProgress(bool scanning, size_t completed,
                                             size_t total, bool done,
                                             bool truncated) {
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setTextSize(1);
  canvas.setFont(reader_font::forSize(24, ReaderFontFamily::Compact));
  canvas.setTextDatum(top_left);
  const ui_strings::Text& text = ui_strings::get();
  const std::string heading = done ? text.completed : text.prefetchCard;
  portuguese_text::draw(canvas, heading,
      (display_.width() - portuguese_text::width(canvas, heading)) / 2, 70);
  canvas.setFont(reader_font::forSize(16, ReaderFontFamily::Compact));
  std::string phase = scanning ? text.scanningCard : text.indexingCovers;
  if (done && truncated)
    phase = ui_strings::language() == UiLanguage::Portuguese
                ? "Concluído com limite de livros" : "Completed with book limit";
  portuguese_text::draw(canvas, phase,
      (display_.width() - portuguese_text::width(canvas, phase)) / 2, 210);
  const int32_t barX = 45;
  const int32_t barY = 315;
  const int32_t barWidth = display_.width() - 90;
  canvas.drawRect(barX, barY, barWidth, 52, TFT_BLACK);
  uint32_t percent = 0;
  if (done) percent = 100;
  else if (scanning) percent = static_cast<uint32_t>((completed % 10) + 1);
  else if (total != 0) percent = static_cast<uint32_t>((completed * 100ULL) / total);
  const int32_t fill = static_cast<int32_t>((barWidth - 6) * percent / 100);
  if (fill > 0) canvas.fillRect(barX + 3, barY + 3, fill, 46, TFT_BLACK);
  char count[80];
  if (scanning)
    snprintf(count, sizeof(count), "%u EPUB", static_cast<unsigned>(completed));
  else
    snprintf(count, sizeof(count), "%u / %u  (%lu%%)",
             static_cast<unsigned>(completed), static_cast<unsigned>(total),
             static_cast<unsigned long>(percent));
  canvas.setTextDatum(middle_center);
  canvas.drawString(count, display_.width() / 2, 435);
  canvas.drawRect(45, 700, display_.width() - 90, 82, TFT_BLACK);
  const char* action = done ? text.closeMenu : text.cancel;
  canvas.setTextDatum(top_left);
  portuguese_text::draw(canvas, action,
      (display_.width() - portuguese_text::width(canvas, action)) / 2, 730);
  display_.submitFull(done ? RefreshIntent::FullQuality
                           : RefreshIntent::InteractiveFeedback);
}
