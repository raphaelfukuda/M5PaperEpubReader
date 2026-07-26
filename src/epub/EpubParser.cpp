#include "EpubParser.h"
#include "AppConfig.h"
#include "HtmlEntityDecoder.h"
#include "XmlTokenizer.h"
#include "storage/PathUtils.h"

bool EpubParser::start(const std::string& filePath) { archive_.close(); book_ = {}; book_.filePath = filePath; buffer_.clear(); error_.clear(); tocError_.clear(); tocDocument_ = {}; measureSpineIndex_ = 0; phase_ = Phase::OpenArchive; return true; }
void EpubParser::fail(const std::string& message) { error_ = message; phase_ = Phase::Failed; archive_.close(); }

WorkResult EpubParser::processNextChunk() {
  switch (phase_) {
    case Phase::OpenArchive: if (!archive_.open(book_.filePath)) { fail(archive_.error()); return WorkResult::Failed; } phase_ = Phase::ReadContainer; return WorkResult::MoreWork;
    case Phase::ReadContainer: if (!archive_.readEntry("META-INF/container.xml", app_config::kMaxContainerXmlBytes, buffer_)) { fail(archive_.error()); return WorkResult::Failed; } phase_ = Phase::ParseContainer; return WorkResult::MoreWork;
    case Phase::ParseContainer: if (!parseContainer()) return WorkResult::Failed; phase_ = Phase::ReadOpf; return WorkResult::MoreWork;
    case Phase::ReadOpf: if (!archive_.readEntry(book_.packagePath, app_config::kMaxOpfBytes, buffer_)) { fail(archive_.error()); return WorkResult::Failed; } phase_ = Phase::ParseOpf; return WorkResult::MoreWork;
    case Phase::ParseOpf:
      if (!parseOpf()) return WorkResult::Failed;
      EpubTableOfContents::discover(buffer_, book_.packagePath, tocDocument_, tocError_);
      buffer_.clear(); measureSpineIndex_ = 0; book_.totalLinearBytes = 0;
      phase_ = Phase::MeasureSpine; return WorkResult::MoreWork;
    case Phase::MeasureSpine:
      if (measureSpineIndex_ < book_.spine.size()) {
        EpubSpineItem& spine = book_.spine[measureSpineIndex_++];
        spine.contentOffset = book_.totalLinearBytes;
        if (spine.linear) {
          for (const auto& item : book_.manifest) {
            if (item.id == spine.idref && item.mediaType == "application/xhtml+xml") {
              uint64_t size = 0;
              if (archive_.entryUncompressedSize(item.href, size)) {
                spine.contentSize = size;
                book_.totalLinearBytes += size;
              }
              break;
            }
          }
        }
        return WorkResult::MoreWork;
      }
      phase_ = tocDocument_.path.empty() ? Phase::Done : Phase::ReadToc;
      return phase_ == Phase::Done ? WorkResult::Completed : WorkResult::MoreWork;
    case Phase::ReadToc:
      if (!archive_.readEntry(tocDocument_.path, app_config::kMaxTocDocumentBytes, buffer_)) {
        tocError_ = archive_.error(); buffer_.clear(); phase_ = Phase::Done; return WorkResult::Completed;
      }
      phase_ = Phase::ParseToc; return WorkResult::MoreWork;
    case Phase::ParseToc:
      EpubTableOfContents::parse(buffer_, tocDocument_, book_.tableOfContents,
                                 tocError_, app_config::kMaxTocEntries);
      buffer_.clear(); phase_ = Phase::Done; return WorkResult::Completed;
    case Phase::Done: return WorkResult::Completed;
    case Phase::Failed: return WorkResult::Failed;
    default: return WorkResult::Idle;
  }
}

bool EpubParser::parseContainer() {
  XmlTokenizer tokenizer(buffer_); XmlToken token;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type == XmlTokenType::Error) { fail(tokenizer.error()); return false; }
    if (token.type == XmlTokenType::StartElement && XmlTokenizer::localName(token.name) == "rootfile") {
      for (const auto& attribute : token.attributes) if (XmlTokenizer::localName(attribute.first) == "full-path") book_.packagePath = attribute.second;
      if (!path_utils::isSafeZipPath(book_.packagePath)) { fail("container.xml contem caminho OPF inseguro"); return false; }
      return true;
    }
  }
  fail("container.xml sem rootfile"); return false;
}

bool EpubParser::parseOpf() {
  XmlTokenizer tokenizer(buffer_); XmlToken token; std::string capture; std::string text;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type == XmlTokenType::Error) { fail(tokenizer.error()); return false; }
    if (token.type == XmlTokenType::StartElement) {
      const std::string name = XmlTokenizer::localName(token.name);
      if (name == "title" || name == "creator" || name == "language") { capture = name; text.clear(); }
      else if (name == "item") {
        if (book_.manifest.size() >= app_config::kMaxManifestItems) { fail("Manifest excede limite"); return false; }
        EpubManifestItem item; for (const auto& a : token.attributes) { const std::string key = XmlTokenizer::localName(a.first); if (key == "id") item.id = a.second; else if (key == "href") item.href = path_utils::resolveRelative(book_.packagePath, a.second); else if (key == "media-type") item.mediaType = a.second; }
        if (!item.id.empty() && !item.href.empty()) book_.manifest.push_back(item);
      } else if (name == "itemref") {
        if (book_.spine.size() >= app_config::kMaxSpineItems) { fail("Spine excede limite"); return false; }
        EpubSpineItem item; for (const auto& a : token.attributes) { const std::string key = XmlTokenizer::localName(a.first); if (key == "idref") item.idref = a.second; else if (key == "linear" && a.second == "no") item.linear = false; } if (!item.idref.empty()) book_.spine.push_back(item);
      }
    } else if (token.type == XmlTokenType::Text && !capture.empty()) text += token.text;
    else if (token.type == XmlTokenType::EndElement && XmlTokenizer::localName(token.name) == capture) {
      const std::string decoded = html_entities::decode(text); if (capture == "title" && book_.title.empty()) book_.title = decoded; else if (capture == "creator" && book_.author.empty()) book_.author = decoded; else if (capture == "language" && book_.language.empty()) book_.language = decoded; capture.clear();
    }
  }
  if (book_.spine.empty()) { fail("OPF contem spine vazio"); return false; }
  for (const auto& spine : book_.spine) { bool found = false; for (const auto& item : book_.manifest) if (item.id == spine.idref) { found = true; break; } if (!found) { fail("Spine referencia item ausente: " + spine.idref); return false; } }
  return true;
}
