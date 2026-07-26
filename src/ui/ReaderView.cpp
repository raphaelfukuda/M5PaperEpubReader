#include "ReaderView.h"
#include "layout/PortugueseTextRenderer.h"
#include "UiStrings.h"
#include "layout/ReaderFont.h"
void ReaderView::renderPageChrome(const EpubBook& book, uint32_t pageNumber) { M5Canvas& canvas = display_.canvas(); canvas.setFont(reader_font::forSize(16)); canvas.setTextColor(TFT_BLACK, TFT_WHITE); canvas.setTextDatum(top_left); const std::string title = portuguese_text::truncateToWidth(canvas, book.title, display_.width() - 56); portuguese_text::draw(canvas, title, 28, 18); canvas.drawFastHLine(28, 52, display_.width() - 56, TFT_BLACK); canvas.setTextDatum(bottom_right); char footer[48]; snprintf(footer, sizeof(footer), "%s %lu", ui_strings::get().page, static_cast<unsigned long>(pageNumber)); canvas.drawString(footer, display_.width() - 28, display_.height() - 15); display_.submitFull(RefreshIntent::ReadingPage); }
