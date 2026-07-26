#include "EpubTableOfContents.h"

#include <algorithm>
#include <cctype>
#include <map>
#include "HtmlEntityDecoder.h"
#include "XmlTokenizer.h"
#include "storage/PathUtils.h"

namespace {
struct ManifestRecord { std::string href, mediaType, properties; };
constexpr uint16_t kMaximumTocDepth = 64;
constexpr size_t kMaximumTocLabelBytes = 512;

std::string attribute(const XmlToken& token, const char* wanted) {
  for (const auto& item : token.attributes)
    if (XmlTokenizer::localName(item.first) == wanted) return item.second;
  return {};
}

bool hasWord(const std::string& words, const char* wanted) {
  size_t position = 0;
  while (position < words.size()) {
    while (position < words.size() && std::isspace(static_cast<unsigned char>(words[position]))) ++position;
    const size_t start = position;
    while (position < words.size() && !std::isspace(static_cast<unsigned char>(words[position]))) ++position;
    if (words.compare(start, position - start, wanted) == 0) return true;
  }
  return false;
}

std::string trimAndCollapse(const std::string& input) {
  std::string decoded = html_entities::decode(input), output; bool pendingSpace = false;
  for (unsigned char c : decoded) {
    if (std::isspace(c)) { pendingSpace = !output.empty(); continue; }
    if (pendingSpace) output += ' ';
    output += static_cast<char>(c); pendingSpace = false;
  }
  return output;
}

bool resolveTarget(const std::string& base, const std::string& href, EpubTocEntry& entry) {
  const size_t hash = href.find('#');
  const std::string relative = href.substr(0, hash);
  entry.fragment = hash == std::string::npos ? std::string{} : href.substr(hash + 1);
  if (relative.empty()) { entry.documentPath = base; return !entry.fragment.empty(); }
  entry.documentPath = path_utils::resolveRelative(base, relative);
  return !entry.documentPath.empty();
}

bool parseNcx(const std::string& xml, const std::string& path, std::vector<EpubTocEntry>& entries, std::string& error, size_t maximum) {
  XmlTokenizer tokenizer(xml); XmlToken token; uint16_t depth = 0; bool inLabel = false;
  std::string label; std::vector<size_t> openEntries;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type == XmlTokenType::Error) { error = tokenizer.error(); return false; }
    const std::string name = XmlTokenizer::localName(token.name);
    if (token.type == XmlTokenType::StartElement && name == "navPoint") {
      if (entries.size() >= maximum) { error = "Sumario excede limite"; return false; }
      if (depth >= kMaximumTocDepth) { error = "Sumario excede profundidade limite"; return false; }
      entries.push_back({}); entries.back().depth = depth++; openEntries.push_back(entries.size() - 1);
    } else if (token.type == XmlTokenType::EndElement && name == "navPoint") {
      if (depth) --depth; if (!openEntries.empty()) openEntries.pop_back();
    } else if (token.type == XmlTokenType::StartElement && name == "text" && !openEntries.empty()) { inLabel = true; label.clear(); }
    else if (token.type == XmlTokenType::Text && inLabel && label.size() < kMaximumTocLabelBytes) label.append(token.text, 0, kMaximumTocLabelBytes - label.size());
    else if (token.type == XmlTokenType::EndElement && name == "text" && inLabel) { entries[openEntries.back()].title = trimAndCollapse(label); inLabel = false; }
    else if (token.type == XmlTokenType::StartElement && name == "content" && !openEntries.empty()) {
      if (!resolveTarget(path, attribute(token, "src"), entries[openEntries.back()])) { error = "Destino NCX inseguro"; return false; }
    }
  }
  entries.erase(std::remove_if(entries.begin(), entries.end(), [](const EpubTocEntry& e) { return e.title.empty() || e.documentPath.empty(); }), entries.end());
  if (entries.empty()) { error = "NCX sem entradas validas"; return false; }
  return true;
}

bool parseNav(const std::string& xml, const std::string& path, std::vector<EpubTocEntry>& entries, std::string& error, size_t maximum) {
  XmlTokenizer tokenizer(xml); XmlToken token; bool inToc = false, inLink = false; int navDepth = 0, listDepth = 0;
  std::string title, href;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type == XmlTokenType::Error) { error = tokenizer.error(); return false; }
    const std::string name = XmlTokenizer::localName(token.name);
    if (token.type == XmlTokenType::StartElement && name == "nav") {
      const bool isToc = hasWord(attribute(token, "type"), "toc") || attribute(token, "role") == "doc-toc";
      if (!inToc && isToc) { inToc = true; navDepth = 1; } else if (inToc) ++navDepth;
    } else if (token.type == XmlTokenType::EndElement && name == "nav" && inToc) {
      if (--navDepth == 0) inToc = false;
    } else if (inToc && token.type == XmlTokenType::StartElement && (name == "ol" || name == "ul")) { if (listDepth >= kMaximumTocDepth) { error = "Sumario excede profundidade limite"; return false; } ++listDepth; }
    else if (inToc && token.type == XmlTokenType::EndElement && (name == "ol" || name == "ul")) { if (listDepth) --listDepth; }
    else if (inToc && token.type == XmlTokenType::StartElement && name == "a") { inLink = true; title.clear(); href = attribute(token, "href"); }
    else if (inLink && token.type == XmlTokenType::Text && title.size() < kMaximumTocLabelBytes) title.append(token.text, 0, kMaximumTocLabelBytes - title.size());
    else if (inLink && token.type == XmlTokenType::EndElement && name == "a") {
      if (entries.size() >= maximum) { error = "Sumario excede limite"; return false; }
      EpubTocEntry entry; entry.title = trimAndCollapse(title); entry.depth = listDepth > 0 ? listDepth - 1 : 0;
      if (!entry.title.empty() && resolveTarget(path, href, entry)) entries.push_back(entry);
      inLink = false;
    }
  }
  if (entries.empty()) { error = "Documento de navegacao sem entradas validas"; return false; }
  return true;
}
}

bool EpubTableOfContents::discover(const std::string& opfXml, const std::string& packagePath, EpubTocDocument& document, std::string& error) {
  document = {}; error.clear(); XmlTokenizer tokenizer(opfXml); XmlToken token;
  std::map<std::string, ManifestRecord> manifest; std::string spineToc;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type == XmlTokenType::Error) { error = tokenizer.error(); return false; }
    if (token.type != XmlTokenType::StartElement) continue;
    const std::string name = XmlTokenizer::localName(token.name);
    if (name == "item") manifest[attribute(token, "id")] = {attribute(token, "href"), attribute(token, "media-type"), attribute(token, "properties")};
    else if (name == "spine") spineToc = attribute(token, "toc");
  }
  for (const auto& item : manifest) if (hasWord(item.second.properties, "nav")) {
    document.path = path_utils::resolveRelative(packagePath, item.second.href);
    if (document.path.empty()) { error = "Caminho do documento de navegacao inseguro"; return false; }
    document.format = EpubTocFormat::NavigationDocument; return true;
  }
  auto ncx = manifest.find(spineToc);
  if (ncx == manifest.end()) for (auto it = manifest.begin(); it != manifest.end(); ++it) if (it->second.mediaType == "application/x-dtbncx+xml") { ncx = it; break; }
  if (ncx != manifest.end()) {
    document.path = path_utils::resolveRelative(packagePath, ncx->second.href);
    if (document.path.empty()) { error = "Caminho NCX inseguro"; return false; }
    document.format = EpubTocFormat::Ncx; return true;
  }
  error = "EPUB sem sumario reconhecido"; return false;
}

bool EpubTableOfContents::parse(const std::string& xml, const EpubTocDocument& document,
                                std::vector<EpubTocEntry>& entries, std::string& error, size_t maximumEntries) {
  entries.clear(); error.clear();
  if (document.path.empty() || maximumEntries == 0) { error = "Documento de sumario invalido"; return false; }
  if (document.format == EpubTocFormat::Ncx) return parseNcx(xml, document.path, entries, error, maximumEntries);
  if (document.format == EpubTocFormat::NavigationDocument) return parseNav(xml, document.path, entries, error, maximumEntries);
  error = "Formato de sumario nao definido"; return false;
}
