#include "EpubContentDiscovery.h"

#include <algorithm>
#include <cctype>
#include "XmlTokenizer.h"
#include "storage/PathUtils.h"

namespace {
std::string lowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  return value;
}

std::string attribute(const XmlToken& token, const char* wanted) {
  for (const auto& item : token.attributes)
    if (lowerAscii(XmlTokenizer::localName(item.first)) == wanted)
      return item.second;
  return {};
}

std::string removeFragmentAndQuery(const std::string& reference) {
  const size_t suffix = reference.find_first_of("?#");
  return reference.substr(0, suffix);
}
}  // namespace

bool epub_content::discoverCover(const std::string& opfXml,
                                 const std::vector<EpubManifestItem>& manifest,
                                 EpubImageReference& cover) {
  cover = {};
  std::string legacyId;
  XmlTokenizer tokenizer(opfXml);
  XmlToken token;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type != XmlTokenType::StartElement ||
        lowerAscii(XmlTokenizer::localName(token.name)) != "meta") continue;
    if (lowerAscii(attribute(token, "name")) == "cover")
      legacyId = attribute(token, "content");
  }
  const EpubManifestItem* fallback = nullptr;
  for (const auto& item : manifest) {
    if (!isRasterMediaType(item.mediaType)) continue;
    const std::string id = lowerAscii(item.id);
    const std::string href = lowerAscii(item.href);
    const std::string properties = lowerAscii(item.properties);
    if (item.id == legacyId || properties.find("cover-image") != std::string::npos ||
        id.find("cover") != std::string::npos || href.find("cover") != std::string::npos) {
      fallback = &item;
      if (item.id == legacyId || properties.find("cover-image") != std::string::npos) break;
    }
  }
  if (!fallback) return false;
  cover.path = fallback->href;
  cover.manifestId = fallback->id;
  cover.mediaType = lowerAscii(fallback->mediaType);
  return true;
}

bool epub_content::imageDimensions(const uint8_t* data, size_t length,
                                   const std::string& mediaType,
                                   uint32_t& width, uint32_t& height) {
  width = height = 0;
  const std::string type = lowerAscii(mediaType);
  if (!data) return false;
  if (type == "image/png" && length >= 24 && data[0] == 0x89 && data[1] == 'P') {
    width = (static_cast<uint32_t>(data[16]) << 24) |
            (static_cast<uint32_t>(data[17]) << 16) |
            (static_cast<uint32_t>(data[18]) << 8) | data[19];
    height = (static_cast<uint32_t>(data[20]) << 24) |
             (static_cast<uint32_t>(data[21]) << 16) |
             (static_cast<uint32_t>(data[22]) << 8) | data[23];
    return width && height;
  }
  if (type == "image/jpeg" && length >= 4 && data[0] == 0xFF && data[1] == 0xD8) {
    size_t i = 2;
    while (i + 8 < length) {
      if (data[i] != 0xFF) { ++i; continue; }
      const uint8_t marker = data[i + 1];
      if (marker == 0xD8 || marker == 0xD9) { i += 2; continue; }
      const uint16_t segment = static_cast<uint16_t>(data[i + 2] << 8) | data[i + 3];
      if (segment < 2 || i + 2 + segment > length) return false;
      if ((marker >= 0xC0 && marker <= 0xC3) ||
          (marker >= 0xC5 && marker <= 0xC7) ||
          (marker >= 0xC9 && marker <= 0xCB) ||
          (marker >= 0xCD && marker <= 0xCF)) {
        height = static_cast<uint16_t>(data[i + 5] << 8) | data[i + 6];
        width = static_cast<uint16_t>(data[i + 7] << 8) | data[i + 8];
        return width && height;
      }
      i += 2 + segment;
    }
  }
  return false;
}

bool epub_content::isRasterMediaType(const std::string& mediaType) {
  const std::string type = lowerAscii(mediaType);
  return type == "image/jpeg" || type == "image/png" || type == "image/gif" ||
         type == "image/bmp" || type == "image/webp";
}

bool epub_content::discoverRasterImages(
    const std::string& xhtml, const std::string& documentPath,
    const std::vector<EpubManifestItem>& manifest, size_t maximumReferences,
    std::vector<EpubImageReference>& output, std::string& error) {
  output.clear();
  error.clear();
  if (!path_utils::isSafeZipPath(documentPath)) {
    error = "Caminho XHTML inseguro";
    return false;
  }
  XmlTokenizer tokenizer(xhtml);
  XmlToken token;
  while ((token = tokenizer.next()).type != XmlTokenType::End) {
    if (token.type == XmlTokenType::Error) {
      error = tokenizer.error();
      output.clear();
      return false;
    }
    if (token.type != XmlTokenType::StartElement) continue;
    const std::string name = lowerAscii(XmlTokenizer::localName(token.name));
    std::string source;
    if (name == "img" || name == "image") source = attribute(token, "src");
    if (source.empty() && name == "image") source = attribute(token, "href");
    if (source.empty() && name == "object") source = attribute(token, "data");
    source = removeFragmentAndQuery(source);
    if (source.empty()) continue;
    const std::string resolved = path_utils::resolveRelative(documentPath, source);
    if (resolved.empty()) continue;  // External, absolute and traversal paths.
    for (const auto& item : manifest) {
      if (item.href != resolved || !isRasterMediaType(item.mediaType)) continue;
      if (output.size() >= maximumReferences) {
        error = "XHTML excede limite de referencias de imagem";
        output.clear();
        return false;
      }
      EpubImageReference reference;
      reference.path = resolved;
      reference.manifestId = item.id;
      reference.mediaType = lowerAscii(item.mediaType);
      reference.altText = attribute(token, "alt");
      output.push_back(reference);
      break;
    }
  }
  return true;
}
