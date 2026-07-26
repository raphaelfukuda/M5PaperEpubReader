#include "ReaderMenuView.h"

#include <algorithm>
#include "UiStrings.h"
#include "layout/PortugueseTextRenderer.h"
#include "layout/ReaderFont.h"

namespace {
constexpr int32_t kHorizontalMargin = 28;
constexpr int32_t kTopButtonTop = 105;
constexpr int32_t kButtonHeight = 82;
constexpr int32_t kRestartButtonTop = 205;
constexpr int32_t kTocButtonTop = 305;
constexpr int32_t kLanguageButtonTop = 405;
constexpr int32_t kSleepButtonTop = 505;
constexpr int32_t kCloseButtonBottomMargin = 35;
constexpr int32_t kTocHeaderBottom = 95;
constexpr int32_t kTocRowHeight = 82;
constexpr size_t kTocRowsPerPage = 9;

void drawCentered(M5Canvas& canvas, const std::string& text, int32_t centerX,
                  int32_t y) {
  portuguese_text::draw(canvas, text,
                        centerX - portuguese_text::width(canvas, text) / 2, y);
}
}  // namespace

void ReaderMenuView::render(const EpubBook& book, const PageAnchor& anchor,
                            uint32_t pageNumber, uint16_t fontSize) {
  confirmingRestart_ = false;
  showingToc_ = false;
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(reader_font::forSize(16));
  canvas.setTextSize(reader_font::scaleForSize(16));
  canvas.setTextDatum(top_center);
  const ui_strings::Text& text = ui_strings::get();
  drawCentered(canvas, text.readingMenu, display_.width() / 2, 28);
  canvas.drawFastHLine(kHorizontalMargin, 72,
                       display_.width() - 2 * kHorizontalMargin, TFT_BLACK);

  canvas.drawRect(kHorizontalMargin, kTopButtonTop,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  canvas.setTextDatum(middle_center);
  drawCentered(canvas, text.backToLibrary, display_.width() / 2,
               kTopButtonTop + kButtonHeight / 2 - canvas.fontHeight() / 2);

  canvas.drawRect(kHorizontalMargin, kRestartButtonTop,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  drawCentered(canvas, text.restartBook, display_.width() / 2,
               kRestartButtonTop + kButtonHeight / 2 - canvas.fontHeight() / 2);

  canvas.drawRect(kHorizontalMargin, kTocButtonTop,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  drawCentered(canvas, text.tableOfContents, display_.width() / 2,
               kTocButtonTop + kButtonHeight / 2 - canvas.fontHeight() / 2);

  canvas.drawRect(kHorizontalMargin, kLanguageButtonTop,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  const std::string languageLabel = std::string(text.language) + ": " +
                                    ui_strings::languageName();
  drawCentered(canvas, languageLabel, display_.width() / 2,
               kLanguageButtonTop + kButtonHeight / 2 - canvas.fontHeight() / 2);

  canvas.drawRect(kHorizontalMargin, kSleepButtonTop,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  drawCentered(canvas, text.sleepNow, display_.width() / 2,
               kSleepButtonTop + kButtonHeight / 2 - canvas.fontHeight() / 2);

  canvas.setTextDatum(top_left);
  char zoom[40];
  snprintf(zoom, sizeof(zoom), "%s: %u px", text.zoom, static_cast<unsigned>(fontSize));
  canvas.drawString(zoom, kHorizontalMargin, 610);

  const uint32_t chapterCount = static_cast<uint32_t>(book.spine.size());
  const uint32_t chapter = chapterCount == 0
                               ? 0
                               : std::min<uint32_t>(anchor.spineIndex + 1,
                                                    chapterCount);
  char position[80];
  snprintf(position, sizeof(position), "%s %lu/%lu - %s %lu", text.chapter,
           static_cast<unsigned long>(chapter),
           static_cast<unsigned long>(chapterCount),
           text.page,
           static_cast<unsigned long>(pageNumber));
  portuguese_text::draw(canvas, position, kHorizontalMargin, 655);

  uint32_t progress = 0;
  if (book.totalLinearBytes != 0 && anchor.spineIndex < book.spine.size()) {
    const EpubSpineItem& spine = book.spine[anchor.spineIndex];
    const uint64_t inside = std::min<uint64_t>(anchor.uncompressedOffset,
                                               spine.contentSize);
    const uint64_t completed = spine.contentOffset + inside;
    progress = static_cast<uint32_t>((completed * 100ULL) /
                                     book.totalLinearBytes);
  }
  char progressLabel[48];
  snprintf(progressLabel, sizeof(progressLabel), "%s: %lu%%", text.approximateProgress,
           static_cast<unsigned long>(progress));
  portuguese_text::draw(canvas, progressLabel, kHorizontalMargin, 700);
  const int32_t barTop = 750;
  const int32_t barWidth = display_.width() - 2 * kHorizontalMargin;
  canvas.drawRect(kHorizontalMargin, barTop, barWidth, 24, TFT_BLACK);
  if (progress != 0) {
    const int32_t fillWidth = ((barWidth - 4) * progress) / 100;
    canvas.fillRect(kHorizontalMargin + 2, barTop + 2, fillWidth, 20,
                    TFT_BLACK);
  }

  const int32_t closeTop = display_.height() - kCloseButtonBottomMargin -
                           kButtonHeight;
  canvas.drawRect(kHorizontalMargin, closeTop,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  canvas.setTextDatum(middle_center);
  drawCentered(canvas, text.closeMenu, display_.width() / 2,
               closeTop + kButtonHeight / 2 - canvas.fontHeight() / 2);
  display_.submitFull(RefreshIntent::InteractiveFeedback);
}

void ReaderMenuView::renderRestartConfirmation() {
  confirmingRestart_ = true;
  showingToc_ = false;
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(reader_font::forSize(16));
  canvas.setTextSize(reader_font::scaleForSize(16));
  canvas.setTextDatum(top_center);
  const ui_strings::Text& text = ui_strings::get();
  drawCentered(canvas, text.restartQuestion, display_.width() / 2, 80);
  drawCentered(canvas, text.restartWarning,
               display_.width() / 2, 155);

  canvas.drawRect(kHorizontalMargin, 285,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  canvas.setTextDatum(middle_center);
  drawCentered(canvas, text.restartConfirm, display_.width() / 2,
               285 + kButtonHeight / 2 - canvas.fontHeight() / 2);

  canvas.drawRect(kHorizontalMargin, 405,
                  display_.width() - 2 * kHorizontalMargin, kButtonHeight,
                  TFT_BLACK);
  drawCentered(canvas, text.cancel, display_.width() / 2,
               405 + kButtonHeight / 2 - canvas.fontHeight() / 2);
  display_.submitFull(RefreshIntent::InteractiveFeedback);
}

void ReaderMenuView::renderTableOfContents(const EpubBook& book, size_t page) {
  confirmingRestart_ = false;
  showingToc_ = true;
  const size_t pageCount = std::max<size_t>(1, (book.tableOfContents.size() +
                                               kTocRowsPerPage - 1) /
                                              kTocRowsPerPage);
  tocPage_ = std::min(page, pageCount - 1);
  M5Canvas& canvas = display_.canvas();
  canvas.fillScreen(TFT_WHITE);
  canvas.setTextColor(TFT_BLACK, TFT_WHITE);
  canvas.setFont(reader_font::forSize(16));
  canvas.setTextSize(reader_font::scaleForSize(16));
  canvas.setTextDatum(top_center);
  drawCentered(canvas, ui_strings::get().tableOfContents,
               display_.width() / 2, 25);
  canvas.drawFastHLine(kHorizontalMargin, 72,
                       display_.width() - 2 * kHorizontalMargin, TFT_BLACK);
  canvas.setTextDatum(middle_left);
  const size_t first = tocPage_ * kTocRowsPerPage;
  for (size_t row = 0; row < kTocRowsPerPage &&
                       first + row < book.tableOfContents.size(); ++row) {
    const EpubTocEntry& entry = book.tableOfContents[first + row];
    const int32_t top = kTocHeaderBottom + row * kTocRowHeight;
    std::string label(std::min<uint16_t>(entry.depth, 3) * 2, ' ');
    label += portuguese_text::truncateToWidth(
        canvas, entry.title, display_.width() - 2 * kHorizontalMargin - 8);
    portuguese_text::draw(canvas, label, kHorizontalMargin + 4,
                          top + (kTocRowHeight - canvas.fontHeight()) / 2);
    canvas.drawFastHLine(kHorizontalMargin, top + kTocRowHeight - 1,
                         display_.width() - 2 * kHorizontalMargin, TFT_BLACK);
  }
  canvas.setTextDatum(bottom_center);
  char footer[64];
  snprintf(footer, sizeof(footer), "<  %s %u/%u  >",
           ui_strings::get().page, static_cast<unsigned>(tocPage_ + 1),
           static_cast<unsigned>(pageCount));
  canvas.drawString(footer, display_.width() / 2, display_.height() - 12);
  display_.submitFull(RefreshIntent::InteractiveFeedback);
}

int ReaderMenuView::tocIndexAt(int32_t y, size_t entryCount) const {
  if (!showingToc_ || y < kTocHeaderBottom ||
      y >= kTocHeaderBottom + static_cast<int32_t>(kTocRowsPerPage * kTocRowHeight))
    return -1;
  const size_t index = tocPage_ * kTocRowsPerPage +
                       static_cast<size_t>((y - kTocHeaderBottom) / kTocRowHeight);
  return index < entryCount ? static_cast<int>(index) : -1;
}

ReaderMenuAction ReaderMenuView::actionAt(int32_t x, int32_t y) const {
  if (x < kHorizontalMargin || x >= display_.width() - kHorizontalMargin)
    return showingToc_ && y >= display_.height() - 95
               ? (x < display_.width() / 2 ? ReaderMenuAction::TocPreviousPage
                                            : ReaderMenuAction::TocNextPage)
               : ReaderMenuAction::None;
  if (showingToc_) {
    if (y >= display_.height() - 95)
      return x < display_.width() / 2 ? ReaderMenuAction::TocPreviousPage
                                      : ReaderMenuAction::TocNextPage;
    return ReaderMenuAction::None;
  }
  if (confirmingRestart_) {
    if (y >= 285 && y < 285 + kButtonHeight)
      return ReaderMenuAction::ConfirmRestart;
    if (y >= 405 && y < 405 + kButtonHeight)
      return ReaderMenuAction::CancelRestart;
    return ReaderMenuAction::None;
  }
  if (y >= kTopButtonTop && y < kTopButtonTop + kButtonHeight)
    return ReaderMenuAction::BackToLibrary;
  if (y >= kRestartButtonTop && y < kRestartButtonTop + kButtonHeight)
    return ReaderMenuAction::RestartBook;
  if (y >= kTocButtonTop && y < kTocButtonTop + kButtonHeight)
    return ReaderMenuAction::OpenTableOfContents;
  if (y >= kLanguageButtonTop && y < kLanguageButtonTop + kButtonHeight)
    return ReaderMenuAction::ToggleLanguage;
  if (y >= kSleepButtonTop && y < kSleepButtonTop + kButtonHeight)
    return ReaderMenuAction::EnterSleep;
  const int32_t closeTop = display_.height() - kCloseButtonBottomMargin -
                           kButtonHeight;
  if (y >= closeTop && y < closeTop + kButtonHeight)
    return ReaderMenuAction::Close;
  return ReaderMenuAction::None;
}
