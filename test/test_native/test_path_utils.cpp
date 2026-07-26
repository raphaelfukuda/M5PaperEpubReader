#include <unity.h>
#include <cstring>
#include "epub/HtmlEntityDecoder.h"
#include "epub/HtmlTokenizer.h"
#include "epub/EpubContentDiscovery.h"
#include "epub/EpubLinks.h"
#include "epub/EpubTableOfContents.h"
#include "layout/PageAnchor.h"
#include "layout/BasicCssStyle.h"
#include "storage/PathUtils.h"
#include "storage/ReadingStateCodec.h"

namespace {
std::string feed(HtmlTokenizer& tokenizer, const char* text, bool finalChunk) {
  return tokenizer.feed(reinterpret_cast<const uint8_t*>(text),
                        std::strlen(text), finalChunk);
}
}  // namespace

void setUp() {}
void tearDown() {}

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
  const std::string output = feed(
      tokenizer,
      "<html><head><title>Ignorar</title></head><body><p>Uma frase\n "
      "  continua aqui.</p><p>Outro par&aacute;grafo.<br>Fim.</p>"
      "</body></html>", true);
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
  const char utf8Part2[] = {'\xA7', 'a', 'o', '<', '/', 'p', '>'};
  output += tokenizer.feed(reinterpret_cast<const uint8_t*>(utf8Part2),
                           sizeof(utf8Part2), true);
  TEST_ASSERT_EQUAL_STRING(u8"\nJoão tem ação\n", output.c_str());
}

void test_tokenizer_does_not_duplicate_layout_boundaries_between_chunks() {
  HtmlTokenizer tokenizer;
  tokenizer.reset();
  std::string output = feed(tokenizer, "texto ", false);
  output += feed(tokenizer, "  contínuo<p>", false);
  output += feed(tokenizer, "<p>seguinte", true);
  TEST_ASSERT_EQUAL_STRING(u8"texto contínuo\nseguinte", output.c_str());
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
  source.bookPath = u8"/Books/João = notas\n.epub";
  source.spineIndex = 7;
  source.textOffset = UINT64_C(0x100000005);
  source.parserCheckpoint = 123456;
  source.fontSize = 40;
  source.lineSpacing = 135;
  source.horizontalMargin = 32;
  std::string encoded;
  TEST_ASSERT_TRUE(reading_state_codec::encode(source, encoded));
  ReadingState decoded;
  std::string error;
  TEST_ASSERT_TRUE(reading_state_codec::decode(encoded, decoded, error));
  TEST_ASSERT_EQUAL_STRING(source.bookPath.c_str(), decoded.bookPath.c_str());
  TEST_ASSERT_EQUAL_UINT32(source.spineIndex, decoded.spineIndex);
  TEST_ASSERT_EQUAL_UINT64(source.textOffset, decoded.textOffset);
  TEST_ASSERT_EQUAL_UINT16(source.fontSize, decoded.fontSize);

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
  RUN_TEST(test_basic_css_accepts_only_bounded_supported_declarations);
  RUN_TEST(test_reading_state_codec_round_trip_and_rejects_corruption);
  RUN_TEST(test_epub3_toc_discovery_and_nested_navigation);
  RUN_TEST(test_epub2_ncx_and_toc_limits);
  RUN_TEST(test_links_classify_internal_external_notes_and_backlinks);
  return UNITY_END();
}
