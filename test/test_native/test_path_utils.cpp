#include <unity.h>
#include <cstring>
#include "epub/HtmlEntityDecoder.h"
#include "epub/HtmlTokenizer.h"
#include "epub/EpubContentDiscovery.h"
#include "epub/EpubLinks.h"
#include "epub/EpubTableOfContents.h"
#include "layout/PageAnchor.h"
#include "layout/BasicCssStyle.h"
#include "layout/TextStyle.h"
#include "storage/PathUtils.h"
#include "storage/ReadingStateCodec.h"
#include "refresh/RefreshPolicy.h"
#include "input/PendingReaderActions.h"
#include "reader/PrefetchStateMachine.h"
#include "display/DisplayRegion.h"
#include "display/DirtyRegion.h"
#include "display/ReaderDirtyFlags.h"
#include "storage/PersistPolicy.h"
#include "display/CanvasMemoryPolicy.h"

namespace {
std::string feed(HtmlTokenizer& tokenizer, const char* text, bool finalChunk) {
  return tokenizer.feed(reinterpret_cast<const uint8_t*>(text),
                        std::strlen(text), finalChunk);
}

std::string withoutStyleControls(const std::string& text) {
  std::string plain;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == text_style_control::kEscape && i + 1 < text.size()) {
      ++i;
      continue;
    }
    plain += text[i];
  }
  return plain;
}
}  // namespace

void setUp() {}
void tearDown() {}

void test_reader_dirty_flags_classify_chrome_and_body() {
  ReaderDirtyFlags flags = classifyReaderDirtyRegion({430, 10, 80, 30}, 540, 960);
  TEST_ASSERT_TRUE(hasDirtyFlag(flags, ReaderDirtyFlags::Header));
  TEST_ASSERT_TRUE(hasDirtyFlag(flags, ReaderDirtyFlags::Battery));
  TEST_ASSERT_FALSE(hasDirtyFlag(flags, ReaderDirtyFlags::Body));
  flags = classifyReaderDirtyRegion({20, 100, 500, 700}, 540, 960);
  TEST_ASSERT_TRUE(hasDirtyFlag(flags, ReaderDirtyFlags::Body));
  TEST_ASSERT_FALSE(hasDirtyFlag(flags, ReaderDirtyFlags::Footer));
}

void test_persist_policy_debounce_threshold_and_busy() {
  PersistPolicy policy;
  PersistReason reason = PersistReason::ExplicitRequest;
  policy.markDirty(1000);
  TEST_ASSERT_FALSE(policy.shouldSave(15999, false, false, reason));
  TEST_ASSERT_TRUE(policy.shouldSave(16000, false, false, reason));
  TEST_ASSERT_EQUAL(static_cast<int>(PersistReason::IdleTimeout), static_cast<int>(reason));
  policy.recordSaved(16000);
  for (int i = 0; i < 5; ++i) policy.markDirty(17000 + i);
  TEST_ASSERT_FALSE(policy.shouldSave(18000, true, false, reason));
  TEST_ASSERT_FALSE(policy.shouldSave(18000, false, true, reason));
  TEST_ASSERT_TRUE(policy.shouldSave(18000, false, false, reason));
  TEST_ASSERT_EQUAL(static_cast<int>(PersistReason::PageThreshold), static_cast<int>(reason));
}

void test_canvas_memory_policy_preserves_internal_margin() {
  CanvasMemoryStats safe{200000, 150000, 3000000};
  TEST_ASSERT_EQUAL(static_cast<int>(CanvasMemoryKind::Psram),
                    static_cast<int>(chooseCanvasMemory(
                        CanvasMemoryPreference::Auto, 64800, 65536, safe)));
  TEST_ASSERT_EQUAL(static_cast<int>(CanvasMemoryKind::InternalRam),
                    static_cast<int>(chooseCanvasMemory(
                        CanvasMemoryPreference::InternalRam, 64800, 65536, safe)));
  CanvasMemoryStats fragmented{200000, 50000, 3000000};
  TEST_ASSERT_EQUAL(static_cast<int>(CanvasMemoryKind::Psram),
                    static_cast<int>(chooseCanvasMemory(
                        CanvasMemoryPreference::Auto, 64800, 65536, fragmented)));
  CanvasMemoryStats tight{120000, 100000, 3000000};
  TEST_ASSERT_EQUAL(static_cast<int>(CanvasMemoryKind::Psram),
                    static_cast<int>(chooseCanvasMemory(
                        CanvasMemoryPreference::InternalRam, 64800, 65536, tight)));
}

void test_epub_extension_is_case_insensitive() {
  TEST_ASSERT_TRUE(path_utils::hasEpubExtension("livro.epub"));
  TEST_ASSERT_TRUE(path_utils::hasEpubExtension("LIVRO.EPUB"));
  TEST_ASSERT_TRUE(path_utils::hasEpubExtension("Livro.EpUb"));
  TEST_ASSERT_FALSE(path_utils::hasEpubExtension("livro.epub.zip"));
  TEST_ASSERT_FALSE(path_utils::hasEpubExtension("epub"));
}

void test_hidden_names() {
  TEST_ASSERT_TRUE(path_utils::isHiddenName(".metadata"));
  TEST_ASSERT_FALSE(path_utils::isHiddenName("book.epub"));
}

void test_refresh_policy_normal_burst_wake_and_reflow() {
  RefreshPolicy policy;
  RefreshRequest request;
  request.reason = RefreshReason::NormalPageTurn;
  request.changedAreaRatio = 0.7f;
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshProfile::Fast),
                    static_cast<int>(policy.decide(request).effective));
  request.reason = RefreshReason::RapidPageTurn;
  request.consecutiveRapidTurns = 2;
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshProfile::Fastest),
                    static_cast<int>(policy.decide(request).effective));
  request.reason = RefreshReason::WakeFromSleep;
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshProfile::Quality),
                    static_cast<int>(policy.decide(request).effective));
  request.reason = RefreshReason::FontReflow;
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshProfile::Text),
                    static_cast<int>(policy.decide(request).effective));
  request.reason = RefreshReason::ManualCleanup;
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshProfile::Quality),
                    static_cast<int>(policy.decide(request).effective));
}

void test_refresh_policy_limits_force_cleanup_and_reset() {
  RefreshPolicyConfig config;
  config.maxFastRefreshesBeforeCleanup = 2;
  RefreshPolicy policy(config);
  RefreshRequest request;
  request.changedAreaRatio = 0.7f;
  request.nowMs = 100;
  RefreshDecision first = policy.decide(request);
  policy.recordSubmitted(first);
  RefreshDecision cleanup = policy.decide(request);
  TEST_ASSERT_TRUE(cleanup.cleanupForced);
  TEST_ASSERT_EQUAL(static_cast<int>(RefreshProfile::Quality),
                    static_cast<int>(cleanup.effective));
  TEST_ASSERT_EQUAL_STRING("fast_limit", cleanup.cleanupCause);
  policy.recordSubmitted(cleanup);
  TEST_ASSERT_EQUAL_UINT16(0, policy.budget().fastRefreshes);
  TEST_ASSERT_EQUAL_UINT16(0, policy.budget().readingRefreshes);
  TEST_ASSERT_EQUAL_UINT32(1, policy.budget().historicalCleanups);
}

void test_refresh_policy_fastest_reading_and_timeout_limits() {
  RefreshPolicyConfig config;
  config.maxConsecutiveFastestRefreshes = 2;
  config.maxReadingRefreshesBeforeQuality = 2;
  config.maxMillisecondsWithoutQualityRefresh = 50;
  RefreshPolicy fastestPolicy(config);
  RefreshRequest burst;
  burst.reason = RefreshReason::RapidPageTurn;
  burst.consecutiveRapidTurns = 2;
  burst.changedAreaRatio = 0.7f;
  fastestPolicy.recordSubmitted(fastestPolicy.decide(burst));
  TEST_ASSERT_TRUE(fastestPolicy.decide(burst).cleanupForced);

  RefreshPolicy timeoutPolicy(config);
  RefreshDecision quality;
  quality.effective = RefreshProfile::Quality;
  quality.submittedAtMs = 10;
  timeoutPolicy.recordSubmitted(quality);
  RefreshRequest normal;
  normal.changedAreaRatio = 0.7f;
  normal.nowMs = 60;
  RefreshDecision timeout = timeoutPolicy.decide(normal);
  TEST_ASSERT_TRUE(timeout.cleanupForced);
  TEST_ASSERT_EQUAL_STRING("quality_timeout", timeout.cleanupCause);
}

void test_ghosting_budget_counts_partial_and_saturates() {
  GhostingBudget budget;
  budget.fastRefreshes = UINT16_MAX;
  budget.record(RefreshProfile::Fast, true);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, budget.fastRefreshes);
  TEST_ASSERT_EQUAL_UINT16(1, budget.partialRefreshes);
  TEST_ASSERT_EQUAL_UINT16(1, budget.readingRefreshes);
  budget.resetAfterQuality(1234);
  TEST_ASSERT_EQUAL_UINT16(0, budget.partialRefreshes);
  TEST_ASSERT_EQUAL_UINT32(1234, budget.lastQualityRefreshMs);
}

void test_pending_input_priority_coalescence_and_limit() {
  PendingReaderActions queue;
  TEST_ASSERT_TRUE(queue.enqueue(PendingReaderAction::NextPage, 10));
  TEST_ASSERT_TRUE(queue.enqueue(PendingReaderAction::NextPage, 11));
  TEST_ASSERT_TRUE(queue.enqueue(PendingReaderAction::NextPage, 12));
  TEST_ASSERT_FALSE(queue.enqueue(PendingReaderAction::NextPage, 13));
  TEST_ASSERT_EQUAL_UINT8(3, queue.pendingNextCount());
  TEST_ASSERT_TRUE(queue.enqueue(PendingReaderAction::OpenMenu, 20));
  TEST_ASSERT_EQUAL_UINT8(0, queue.pendingNextCount());
  TEST_ASSERT_EQUAL(static_cast<int>(PendingReaderAction::OpenMenu),
                    static_cast<int>(queue.pop().action));
}

void test_pending_input_previous_and_back_cancel_lower_priority() {
  PendingReaderActions queue;
  queue.enqueue(PendingReaderAction::NextPage, 1);
  queue.enqueue(PendingReaderAction::PreviousPage, 2);
  TEST_ASSERT_EQUAL(static_cast<int>(PendingReaderAction::PreviousPage),
                    static_cast<int>(queue.pop().action));
  TEST_ASSERT_TRUE(queue.empty());
  queue.enqueue(PendingReaderAction::IncreaseFont, 3);
  queue.enqueue(PendingReaderAction::OpenMenu, 4);
  queue.enqueue(PendingReaderAction::BackToLibrary, 5);
  TEST_ASSERT_EQUAL(static_cast<int>(PendingReaderAction::BackToLibrary),
                    static_cast<int>(queue.pop().action));
  TEST_ASSERT_TRUE(queue.empty());
}

void test_prefetch_state_machine_cancel_restore_and_repeat() {
  PrefetchStateMachine state;
  TEST_ASSERT_TRUE(state.start());
  TEST_ASSERT_TRUE(state.hasWork());
  TEST_ASSERT_EQUAL(static_cast<int>(PrefetchCancelResult::DeferredUntilSafePoint),
                    static_cast<int>(state.requestCancel()));
  TEST_ASSERT_EQUAL(static_cast<int>(PrefetchCancelResult::DeferredUntilSafePoint),
                    static_cast<int>(state.requestCancel()));
  TEST_ASSERT_TRUE(state.transition(PrefetchState::Restoring));
  state.restored(true);
  TEST_ASSERT_EQUAL(static_cast<int>(PrefetchState::Idle),
                    static_cast<int>(state.state()));
  TEST_ASSERT_FALSE(state.hasWork());
}

void test_prefetch_state_machine_ready_is_preserved_and_error_is_terminal() {
  PrefetchStateMachine state;
  state.start();
  TEST_ASSERT_TRUE(state.transition(PrefetchState::Parsing));
  TEST_ASSERT_TRUE(state.transition(PrefetchState::LayingOut));
  TEST_ASSERT_TRUE(state.transition(PrefetchState::Ready));
  TEST_ASSERT_TRUE(state.ready());
  TEST_ASSERT_EQUAL(static_cast<int>(PrefetchCancelResult::PreservedAsCache),
                    static_cast<int>(state.requestCancel()));
  state.reset();
  state.start();
  TEST_ASSERT_TRUE(state.transition(PrefetchState::Failed));
  TEST_ASSERT_EQUAL(static_cast<int>(PrefetchCancelResult::Failed),
                    static_cast<int>(state.requestCancel()));
}

void test_display_region_clamp_union_intersection_and_alignment() {
  DisplayRegion clipped = DisplayRegion(-10, -20, 100, 80).clamped(540, 960);
  TEST_ASSERT_EQUAL_INT32(0, clipped.x);
  TEST_ASSERT_EQUAL_INT32(0, clipped.y);
  TEST_ASSERT_EQUAL_INT32(90, clipped.width);
  TEST_ASSERT_EQUAL_INT32(60, clipped.height);
  DisplayRegion united = DisplayRegion::unite(DisplayRegion(10, 20, 30, 40),
                                               DisplayRegion(30, 10, 50, 20));
  TEST_ASSERT_EQUAL_INT32(10, united.x);
  TEST_ASSERT_EQUAL_INT32(10, united.y);
  TEST_ASSERT_EQUAL_INT32(70, united.width);
  TEST_ASSERT_EQUAL_INT32(50, united.height);
  DisplayRegion overlap = DisplayRegion::intersect(DisplayRegion(0, 0, 20, 20),
                                                    DisplayRegion(10, 5, 20, 30));
  TEST_ASSERT_EQUAL_UINT32(150, overlap.area());
  DisplayRegion aligned = DisplayRegion(3, 5, 6, 10).aligned(4, 540, 960);
  TEST_ASSERT_EQUAL_INT32(0, aligned.x);
  TEST_ASSERT_EQUAL_INT32(12, aligned.width);
}

void test_display_region_empty_ratio_fullscreen_and_overflow() {
  TEST_ASSERT_TRUE(DisplayRegion(0, 0, 0, 10).empty());
  TEST_ASSERT_TRUE(DisplayRegion::intersect(DisplayRegion(0, 0, 2, 2),
                                            DisplayRegion(3, 3, 2, 2)).empty());
  DisplayRegion full(0, 0, 540, 960);
  TEST_ASSERT_EQUAL_UINT32(518400, full.area());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, full.ratio(518400));
  DisplayRegion overflow(INT32_MAX - 4, INT32_MAX - 4, 100, 100);
  TEST_ASSERT_EQUAL_UINT32(10000, overflow.area());
  TEST_ASSERT_TRUE(overflow.clamped(540, 960).empty());
  DisplayRegion expanded = DisplayRegion(2, 2, 2, 2).expanded(4).clamped(540, 960);
  TEST_ASSERT_EQUAL_INT32(0, expanded.x);
  TEST_ASSERT_EQUAL_INT32(0, expanded.y);
}

void test_dirty_region_identical_pixel_line_and_separate_changes() {
  uint8_t first[4] = {0, 0, 0, 0};
  uint8_t second[4] = {0, 0, 0, 0};
  DirtyRegionResult same = compareMonoBuffers(first, second, 16, 2, 2);
  TEST_ASSERT_TRUE(same.valid);
  TEST_ASSERT_TRUE(same.bounds.empty());
  second[0] = 0x80;
  DirtyRegionResult pixel = compareMonoBuffers(first, second, 16, 2, 2);
  TEST_ASSERT_EQUAL_UINT32(1, pixel.changedPixels);
  TEST_ASSERT_EQUAL_INT32(0, pixel.bounds.x);
  TEST_ASSERT_EQUAL_INT32(1, pixel.bounds.width);
  second[3] = 0x01;
  DirtyRegionResult separate = compareMonoBuffers(first, second, 16, 2, 2);
  TEST_ASSERT_EQUAL_UINT32(2, separate.changedPixels);
  TEST_ASSERT_EQUAL_INT32(16, separate.bounds.width);
  TEST_ASSERT_EQUAL_INT32(2, separate.bounds.height);
}

void test_dirty_region_stride_non_byte_width_and_last_bit() {
  uint8_t first[6] = {0, 0, 0xFF, 0, 0, 0xFF};
  uint8_t second[6] = {0, 0, 0x00, 0, 0x40, 0x00};
  DirtyRegionResult result = compareMonoBuffers(first, second, 10, 2, 3);
  TEST_ASSERT_TRUE(result.valid);
  TEST_ASSERT_EQUAL_UINT32(1, result.changedPixels);
  TEST_ASSERT_EQUAL_INT32(9, result.bounds.x);
  TEST_ASSERT_EQUAL_INT32(1, result.bounds.y);
  TEST_ASSERT_EQUAL_INT32(1, result.bounds.width);
  TEST_ASSERT_FALSE(compareMonoBuffers(first, second, 10, 2, 1).valid);
}

void test_join_parent_and_file_name() {
  TEST_ASSERT_EQUAL_STRING("/Books", path_utils::join("/", "Books").c_str());
  TEST_ASSERT_EQUAL_STRING("/Books/Fiction", path_utils::join("/Books", "Fiction").c_str());
  TEST_ASSERT_EQUAL_STRING("/Books", path_utils::parent("/Books/Fiction").c_str());
  TEST_ASSERT_EQUAL_STRING("/", path_utils::parent("/Books").c_str());
  TEST_ASSERT_EQUAL_STRING("Fiction", path_utils::fileName("/Books/Fiction/").c_str());
}

void test_case_insensitive_comparison() {
  TEST_ASSERT_EQUAL(0, path_utils::compareCaseInsensitive("Alpha", "alpha"));
  TEST_ASSERT_LESS_THAN(0, path_utils::compareCaseInsensitive("alpha", "Beta"));
  TEST_ASSERT_GREATER_THAN(0, path_utils::compareCaseInsensitive("Zulu", "beta"));
}

void test_html_entities_support_portuguese_and_numeric_forms() {
  TEST_ASSERT_EQUAL_STRING(
      u8"ação & São José — €",
      html_entities::decode("a&ccedil;&atilde;o &amp; S&atilde;o Jos&#233; "
                            "&#8212; &#x20AC;").c_str());
}

void test_unknown_and_invalid_entities_are_preserved() {
  TEST_ASSERT_EQUAL_STRING("&desconhecida; &#xD800; &#invalida;",
                           html_entities::decode(
                               "&desconhecida; &#xD800; &#invalida;").c_str());
}

void test_tokenizer_collapses_source_formatting_but_keeps_structure() {
  HtmlTokenizer tokenizer;
  tokenizer.reset();
  const std::string output = withoutStyleControls(feed(
      tokenizer,
      "<html><head><title>Ignorar</title></head><body><p>Uma frase\n "
      "  continua aqui.</p><p>Outro par&aacute;grafo.<br>Fim.</p>"
      "</body></html>", true));
  TEST_ASSERT_EQUAL_STRING(
      u8"\nUma frase continua aqui.\nOutro parágrafo.\nFim.\n",
      output.c_str());
}

void test_tokenizer_normalizes_nbsp_as_breakable_space() {
  HtmlTokenizer tokenizer;
  tokenizer.reset();
  TEST_ASSERT_EQUAL_STRING("palavra seguinte",
                           feed(tokenizer, "palavra&nbsp;seguinte", true).c_str());
}

void test_tokenizer_preserves_entities_tags_and_utf8_across_chunks() {
  HtmlTokenizer tokenizer;
  tokenizer.reset();
  std::string output;
  output += feed(tokenizer, "<p>Jo&at", false);
  output += feed(tokenizer, "ilde;o ", false);
  output += feed(tokenizer, "  tem a", false);
  const char utf8Part1[] = {'\xC3'};
  output += tokenizer.feed(reinterpret_cast<const uint8_t*>(utf8Part1), 1, false);
  const char utf8Part2[] = {'\xA7', '\xC3', '\xA3', 'o', '<', '/', 'p', '>'};
  output += tokenizer.feed(reinterpret_cast<const uint8_t*>(utf8Part2),
                           sizeof(utf8Part2), true);
  TEST_ASSERT_EQUAL_STRING(u8"\nJoão tem ação\n",
                           withoutStyleControls(output).c_str());
}

void test_tokenizer_does_not_duplicate_layout_boundaries_between_chunks() {
  HtmlTokenizer tokenizer;
  tokenizer.reset();
  std::string output = feed(tokenizer, "texto ", false);
  output += feed(tokenizer, "  contínuo<p>", false);
  output += feed(tokenizer, "<p>seguinte", true);
  TEST_ASSERT_EQUAL_STRING(u8"texto contínuo\nseguinte",
                           withoutStyleControls(output).c_str());
}

void test_page_anchor_defaults_and_round_trip_values() {
  PageAnchor anchor;
  TEST_ASSERT_EQUAL_UINT32(0, anchor.spineIndex);
  TEST_ASSERT_EQUAL_UINT64(0, anchor.uncompressedOffset);
  TEST_ASSERT_EQUAL_UINT32(0, anchor.parserCheckpoint);
  anchor.spineIndex = 12;
  anchor.uncompressedOffset = UINT64_C(0x100000005);
  anchor.parserCheckpoint = 98765;
  TEST_ASSERT_EQUAL_UINT32(12, anchor.spineIndex);
  TEST_ASSERT_EQUAL_UINT64(UINT64_C(0x100000005), anchor.uncompressedOffset);
  TEST_ASSERT_EQUAL_UINT32(98765, anchor.parserCheckpoint);
}

void test_discovers_only_safe_manifest_raster_images() {
  std::vector<EpubManifestItem> manifest = {
      {"cover", "OPS/images/cover.jpg", "image/jpeg"},
      {"vector", "OPS/images/art.svg", "image/svg+xml"}};
  std::vector<EpubImageReference> images;
  std::string error;
  TEST_ASSERT_TRUE(epub_content::discoverRasterImages(
      "<html><body><img src='../images/cover.jpg#page' alt='Capa'/>"
      "<img src='../../outside.png'/><img src='https://example/img.png'/>"
      "<img src='../images/art.svg'/></body></html>",
      "OPS/text/chapter.xhtml", manifest, 4, images, error));
  TEST_ASSERT_EQUAL_UINT32(1, images.size());
  TEST_ASSERT_EQUAL_STRING("OPS/images/cover.jpg", images[0].path.c_str());
  TEST_ASSERT_EQUAL_STRING("cover", images[0].manifestId.c_str());
  TEST_ASSERT_EQUAL_STRING("Capa", images[0].altText.c_str());
}

void test_image_discovery_enforces_reference_limit() {
  std::vector<EpubManifestItem> manifest = {
      {"one", "OPS/one.png", "image/png"},
      {"two", "OPS/two.png", "image/png"}};
  std::vector<EpubImageReference> images;
  std::string error;
  TEST_ASSERT_FALSE(epub_content::discoverRasterImages(
      "<body><img src='one.png'/><img src='two.png'/></body>",
      "OPS/chapter.xhtml", manifest, 1, images, error));
  TEST_ASSERT_FALSE(error.empty());
  TEST_ASSERT_TRUE(images.empty());
}

void test_cover_discovery_and_image_dimensions() {
  std::vector<EpubManifestItem> manifest = {
      {"front", "OPS/front.jpg", "image/jpeg", "cover-image"},
      {"other", "OPS/other.png", "image/png", ""}};
  EpubImageReference cover;
  TEST_ASSERT_TRUE(epub_content::discoverCover(
      "<package><metadata><meta name='cover' content='front'/></metadata></package>",
      manifest, cover));
  TEST_ASSERT_EQUAL_STRING("OPS/front.jpg", cover.path.c_str());
  const uint8_t png[] = {0x89, 'P', 'N', 'G', 0, 0, 0, 0, 0, 0, 0, 0,
                         'I', 'H', 'D', 'R', 0, 0, 2, 0, 0, 0, 3, 0};
  uint32_t width = 0, height = 0;
  TEST_ASSERT_TRUE(epub_content::imageDimensions(
      png, sizeof(png), "image/png", width, height));
  TEST_ASSERT_EQUAL_UINT32(512, width);
  TEST_ASSERT_EQUAL_UINT32(768, height);
}

void test_basic_css_accepts_only_bounded_supported_declarations() {
  const BasicCssStyle style = basic_css::parseDeclarations(
      "text-align: center; font-weight: 700; font-style: italic; "
      "text-indent: 24px; line-height: 140%; color: red");
  TEST_ASSERT_EQUAL(static_cast<int>(BasicTextAlign::Center),
                    static_cast<int>(style.textAlign));
  TEST_ASSERT_TRUE(style.bold);
  TEST_ASSERT_TRUE(style.italic);
  TEST_ASSERT_TRUE(style.hasTextIndent);
  TEST_ASSERT_EQUAL_INT16(24, style.textIndentPx);
  TEST_ASSERT_TRUE(style.hasLineHeightPercent);
  TEST_ASSERT_EQUAL_UINT16(140, style.lineHeightPercent);

  const BasicCssStyle rejected = basic_css::parseDeclarations(
      "text-indent: 99999px; line-height: 5%; background: url(http://x)");
  TEST_ASSERT_FALSE(rejected.hasTextIndent);
  TEST_ASSERT_FALSE(rejected.hasLineHeightPercent);
}

void test_reading_state_codec_round_trip_and_rejects_corruption() {
  ReadingState source;
  source.bookPath = "/Books/João = notas\n.epub";
  source.spineIndex = 7;
  source.textOffset = UINT64_C(0x100000005);
  source.parserCheckpoint = 123456;
  source.fontSize = 40;
  source.fontFamily = 1;
  source.lineSpacing = 135;
  source.horizontalMargin = 32;
  source.pageNumber = 87;
  ReadingHistoryEntry previous;
  previous.spineIndex = 6;
  previous.textOffset = 9988;
  previous.parserCheckpoint = 4455;
  source.previousPages.push_back(previous);
  std::string encoded;
  TEST_ASSERT_TRUE(reading_state_codec::encode(source, encoded));
  ReadingState decoded;
  std::string error;
  TEST_ASSERT_TRUE(reading_state_codec::decode(encoded, decoded, error));
  TEST_ASSERT_EQUAL_STRING(source.bookPath.c_str(), decoded.bookPath.c_str());
  TEST_ASSERT_EQUAL_UINT32(source.spineIndex, decoded.spineIndex);
  TEST_ASSERT_EQUAL_UINT64(source.textOffset, decoded.textOffset);
  TEST_ASSERT_EQUAL_UINT16(source.fontSize, decoded.fontSize);
  TEST_ASSERT_EQUAL_UINT8(source.fontFamily, decoded.fontFamily);
  TEST_ASSERT_EQUAL_UINT32(87, decoded.pageNumber);
  TEST_ASSERT_EQUAL_UINT32(1, decoded.previousPages.size());
  TEST_ASSERT_EQUAL_UINT32(4455, decoded.previousPages[0].parserCheckpoint);

  const std::string legacy =
      "M5EPUB-STATE\nversion=1\nbook=/Books/legacy.epub\nspine=2\n"
      "offset=50\ncheckpoint=40\nfont=24\nspacing=0\nmargin=24\n";
  ReadingState migrated;
  TEST_ASSERT_TRUE(reading_state_codec::decode(legacy, migrated, error));
  TEST_ASSERT_EQUAL_UINT32(ReadingState::kCurrentVersion, migrated.version);
  TEST_ASSERT_EQUAL_UINT32(1, migrated.pageNumber);
  TEST_ASSERT_EQUAL_UINT8(2, migrated.fontFamily);
  TEST_ASSERT_TRUE(migrated.previousPages.empty());

  ReadingState unchanged;
  unchanged.bookPath = "sentinela";
  TEST_ASSERT_FALSE(reading_state_codec::decode(
      "M5EPUB-STATE\nversion=1\nbook=%GG\n", unchanged, error));
  TEST_ASSERT_EQUAL_STRING("sentinela", unchanged.bookPath.c_str());
}

void test_epub3_toc_discovery_and_nested_navigation() {
  const std::string opf =
      "<package><manifest><item id='nav' href='nav/toc.xhtml' "
      "media-type='application/xhtml+xml' properties='landmarks nav'/></manifest>"
      "<spine/></package>";
  EpubTocDocument document;
  std::string error;
  TEST_ASSERT_TRUE(EpubTableOfContents::discover(
      opf, "OPS/package.opf", document, error));
  TEST_ASSERT_EQUAL(static_cast<int>(EpubTocFormat::NavigationDocument),
                    static_cast<int>(document.format));
  TEST_ASSERT_EQUAL_STRING("OPS/nav/toc.xhtml", document.path.c_str());
  std::vector<EpubTocEntry> entries;
  TEST_ASSERT_TRUE(EpubTableOfContents::parse(
      "<html><nav epub:type='toc'><ol><li><a href='../c1.xhtml#inicio'>"
      " Cap&iacute;tulo  1 </a><ol><li><a href='../c1.xhtml#sec'>Seção</a>"
      "</li></ol></li></ol></nav></html>",
      document, entries, error, 8));
  TEST_ASSERT_EQUAL_UINT32(2, entries.size());
  TEST_ASSERT_EQUAL_STRING(u8"Capítulo 1", entries[0].title.c_str());
  TEST_ASSERT_EQUAL_UINT16(0, entries[0].depth);
  TEST_ASSERT_EQUAL_UINT16(1, entries[1].depth);
  TEST_ASSERT_EQUAL_STRING("OPS/c1.xhtml", entries[1].documentPath.c_str());
  TEST_ASSERT_EQUAL_STRING("sec", entries[1].fragment.c_str());
}

void test_epub2_ncx_and_toc_limits() {
  EpubTocDocument document;
  document.format = EpubTocFormat::Ncx;
  document.path = "OPS/toc.ncx";
  std::vector<EpubTocEntry> entries;
  std::string error;
  const std::string ncx =
      "<ncx><navMap><navPoint><navLabel><text>Primeiro</text></navLabel>"
      "<content src='chapter.xhtml'/></navPoint><navPoint><navLabel>"
      "<text>Segundo</text></navLabel><content src='two.xhtml'/></navPoint>"
      "</navMap></ncx>";
  TEST_ASSERT_FALSE(EpubTableOfContents::parse(ncx, document, entries, error, 1));
  TEST_ASSERT_FALSE(error.empty());
  TEST_ASSERT_TRUE(EpubTableOfContents::parse(ncx, document, entries, error, 2));
  TEST_ASSERT_EQUAL_UINT32(2, entries.size());
}

void test_links_classify_internal_external_notes_and_backlinks() {
  EpubDocumentLinks links;
  std::string error;
  TEST_ASSERT_TRUE(EpubLinkParser::parse(
      "<html><body><p id='inicio'><a href='next.xhtml#alvo'>Próximo</a> "
      "<a href='https://example.test' role='doc-noteref'>Externo</a> "
      "<a href='#nota' epub:type='noteref'>1</a></p>"
      "<aside id='nota' epub:type='footnote'>Texto <b>da nota</b> "
      "<a href='#inicio' role='doc-backlink'>voltar</a></aside></body></html>",
      "OPS/chapter.xhtml", links, error));
  TEST_ASSERT_EQUAL_UINT32(4, links.links.size());
  TEST_ASSERT_EQUAL(static_cast<int>(EpubLinkKind::Internal),
                    static_cast<int>(links.links[0].kind));
  TEST_ASSERT_EQUAL_STRING("OPS/next.xhtml", links.links[0].target.documentPath.c_str());
  TEST_ASSERT_EQUAL(static_cast<int>(EpubLinkKind::External),
                    static_cast<int>(links.links[1].kind));
  TEST_ASSERT_EQUAL(static_cast<int>(EpubLinkKind::NoteReference),
                    static_cast<int>(links.links[2].kind));
  TEST_ASSERT_EQUAL(static_cast<int>(EpubLinkKind::Backlink),
                    static_cast<int>(links.links[3].kind));
  TEST_ASSERT_EQUAL_UINT32(1, links.footnotes.size());
  TEST_ASSERT_EQUAL_STRING("Texto da nota voltar", links.footnotes[0].text.c_str());

  EpubTarget target;
  TEST_ASSERT_FALSE(EpubLinkParser::resolveInternal(
      "OPS/chapter.xhtml", "../../outside.xhtml", target));
  TEST_ASSERT_FALSE(EpubLinkParser::resolveInternal(
      "OPS/chapter.xhtml", "javascript:alert(1)", target));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_epub_extension_is_case_insensitive);
  RUN_TEST(test_hidden_names);
  RUN_TEST(test_refresh_policy_normal_burst_wake_and_reflow);
  RUN_TEST(test_refresh_policy_limits_force_cleanup_and_reset);
  RUN_TEST(test_refresh_policy_fastest_reading_and_timeout_limits);
  RUN_TEST(test_ghosting_budget_counts_partial_and_saturates);
  RUN_TEST(test_pending_input_priority_coalescence_and_limit);
  RUN_TEST(test_pending_input_previous_and_back_cancel_lower_priority);
  RUN_TEST(test_prefetch_state_machine_cancel_restore_and_repeat);
  RUN_TEST(test_prefetch_state_machine_ready_is_preserved_and_error_is_terminal);
  RUN_TEST(test_display_region_clamp_union_intersection_and_alignment);
  RUN_TEST(test_display_region_empty_ratio_fullscreen_and_overflow);
  RUN_TEST(test_dirty_region_identical_pixel_line_and_separate_changes);
  RUN_TEST(test_dirty_region_stride_non_byte_width_and_last_bit);
  RUN_TEST(test_reader_dirty_flags_classify_chrome_and_body);
  RUN_TEST(test_persist_policy_debounce_threshold_and_busy);
  RUN_TEST(test_canvas_memory_policy_preserves_internal_margin);
  RUN_TEST(test_join_parent_and_file_name);
  RUN_TEST(test_case_insensitive_comparison);
  RUN_TEST(test_html_entities_support_portuguese_and_numeric_forms);
  RUN_TEST(test_unknown_and_invalid_entities_are_preserved);
  RUN_TEST(test_tokenizer_collapses_source_formatting_but_keeps_structure);
  RUN_TEST(test_tokenizer_normalizes_nbsp_as_breakable_space);
  RUN_TEST(test_tokenizer_preserves_entities_tags_and_utf8_across_chunks);
  RUN_TEST(test_tokenizer_does_not_duplicate_layout_boundaries_between_chunks);
  RUN_TEST(test_page_anchor_defaults_and_round_trip_values);
  RUN_TEST(test_discovers_only_safe_manifest_raster_images);
  RUN_TEST(test_image_discovery_enforces_reference_limit);
  RUN_TEST(test_cover_discovery_and_image_dimensions);
  RUN_TEST(test_basic_css_accepts_only_bounded_supported_declarations);
  RUN_TEST(test_reading_state_codec_round_trip_and_rejects_corruption);
  RUN_TEST(test_epub3_toc_discovery_and_nested_navigation);
  RUN_TEST(test_epub2_ncx_and_toc_limits);
  RUN_TEST(test_links_classify_internal_external_notes_and_backlinks);
  return UNITY_END();
}
