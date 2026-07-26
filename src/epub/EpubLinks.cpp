#include "EpubLinks.h"

#include <algorithm>
#include <cctype>
#include "HtmlEntityDecoder.h"
#include "XmlTokenizer.h"
#include "storage/PathUtils.h"

namespace {
std::string attribute(const XmlToken& token, const char* wanted) {
  for (const auto& item : token.attributes)
    if (XmlTokenizer::localName(item.first) == wanted) return html_entities::decode(item.second);
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

std::string normalized(const std::string& value, size_t maximum) {
  const std::string decoded = html_entities::decode(value);
  std::string result; result.reserve(std::min(decoded.size(), maximum)); bool space = false;
  for (unsigned char c : decoded) {
    if (std::isspace(c)) { space = !result.empty(); continue; }
    if (space && result.size() < maximum) result += ' ';
    space = false;
    if (result.size() < maximum) result += static_cast<char>(c);
  }
  return result;
}

bool isExternal(const std::string& href) {
  if (href.size() >= 2 && href[0] == '/' && href[1] == '/') return true;
  const size_t colon = href.find(':');
  const size_t boundary = std::min(href.find('/'), href.find('#'));
  if (colon == std::string::npos || (boundary != std::string::npos && colon > boundary)) return false;
  if (colon == 0 || !std::isalpha(static_cast<unsigned char>(href[0]))) return false;
  for (size_t i = 1; i < colon; ++i) {
    const unsigned char c = href[i];
    if (!std::isalnum(c) && c != '+' && c != '-' && c != '.') return false;
  }
  return true;
}

bool isFootnoteContainer(const XmlToken& token) {
  return hasWord(attribute(token, "type"), "footnote") ||
         attribute(token, "role") == "doc-footnote";
}

EpubLinkKind linkKind(const XmlToken& token) {
  const std::string type = attribute(token, "type"), role = attribute(token, "role");
  if (hasWord(type, "noteref") || role == "doc-noteref") return EpubLinkKind::NoteReference;
  if (hasWord(type, "backlink") || role == "doc-backlink") return EpubLinkKind::Backlink;
  return EpubLinkKind::Internal;
}
}

bool EpubLinkParser::resolveInternal(const std::string& sourceDocument,
                                     const std::string& href, EpubTarget& target) {
  target = {};
  if (sourceDocument.empty() || href.empty() || isExternal(href)) return false;
  const size_t hash = href.find('#');
  const std::string relative = href.substr(0, hash);
  target.fragment = hash == std::string::npos ? std::string{} : href.substr(hash + 1);
  if (relative.empty()) target.documentPath = sourceDocument;
  else target.documentPath = path_utils::resolveRelative(sourceDocument, relative);
  return !target.documentPath.empty() && (!relative.empty() || !target.fragment.empty());
}

bool EpubLinkParser::parse(const std::string& xhtml, const std::string& documentPath,
                           EpubDocumentLinks& output, std::string& error,
                           const EpubLinkLimits& limits) {
  output = {}; output.documentPath = documentPath; error.clear();
  if (!path_utils::isSafeZipPath(documentPath) || limits.maximumLinks == 0 ||
      limits.maximumFragments == 0 || limits.maximumLabelBytes == 0) {
    error = "Documento ou limites de links invalidos"; return false;
  }
  XmlTokenizer tokenizer(xhtml); XmlToken token;
  bool inLink = false; std::string linkText, href; EpubLinkKind currentKind = EpubLinkKind::Internal;
  size_t footnoteDepth = 0; std::string footnoteId, footnoteText;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type == XmlTokenType::Error) { error = tokenizer.error(); return false; }
    const std::string name = XmlTokenizer::localName(token.name);
    if (token.type == XmlTokenType::StartElement) {
      const std::string id = attribute(token, "id");
      if (!id.empty()) {
        if (id.size() > limits.maximumReferenceBytes) { error = "Identificador XHTML excede limite"; return false; }
        if (std::find(output.fragments.begin(), output.fragments.end(), id) == output.fragments.end()) {
          if (output.fragments.size() >= limits.maximumFragments) { error = "Fragmentos XHTML excedem limite"; return false; }
          output.fragments.push_back(id);
        }
      }
      if (footnoteDepth) ++footnoteDepth;
      else if (isFootnoteContainer(token) && !id.empty()) {
        if (output.footnotes.size() >= limits.maximumFootnotes) { error = "Notas de rodape excedem limite"; return false; }
        footnoteDepth = 1; footnoteId = id; footnoteText.clear();
      }
      if (name == "a" && !inLink) {
        inLink = true; linkText.clear(); href = attribute(token, "href"); currentKind = linkKind(token);
        if (href.size() > limits.maximumReferenceBytes) { error = "Destino de link excede limite"; return false; }
      }
      if (token.selfClosing && footnoteDepth) {
        if (--footnoteDepth == 0) { output.footnotes.push_back({footnoteId, normalized(footnoteText, limits.maximumFootnoteBytes)}); }
      }
    } else if (token.type == XmlTokenType::Text) {
      if (inLink && linkText.size() < limits.maximumLabelBytes) linkText.append(token.text, 0, limits.maximumLabelBytes - linkText.size());
      if (footnoteDepth && footnoteText.size() < limits.maximumFootnoteBytes) footnoteText.append(token.text, 0, limits.maximumFootnoteBytes - footnoteText.size());
    } else if (token.type == XmlTokenType::EndElement) {
      if (name == "a" && inLink) {
        if (output.links.size() >= limits.maximumLinks) { error = "Links XHTML excedem limite"; return false; }
        EpubLink link; link.kind = currentKind; link.label = normalized(linkText, limits.maximumLabelBytes);
        if (isExternal(href)) { link.kind = EpubLinkKind::External; link.externalUri = href; output.links.push_back(link); }
        else if (resolveInternal(documentPath, href, link.target)) output.links.push_back(link);
        inLink = false;
      }
      if (footnoteDepth && --footnoteDepth == 0) {
        const std::string text = normalized(footnoteText, limits.maximumFootnoteBytes);
        if (!text.empty()) output.footnotes.push_back({footnoteId, text});
      }
    }
  }
  if (footnoteDepth || inLink) { error = "XHTML terminou dentro de link ou nota"; return false; }
  return true;
}
