#pragma once

#include <stddef.h>
#include <cstdint>
#include <string>
#include <vector>
#include "EpubManifestItem.h"

struct EpubImageReference {
  std::string path;
  std::string manifestId;
  std::string mediaType;
  std::string altText;
};

namespace epub_content {
bool isRasterMediaType(const std::string& mediaType);
bool discoverRasterImages(const std::string& xhtml,
                          const std::string& documentPath,
                          const std::vector<EpubManifestItem>& manifest,
                          size_t maximumReferences,
                          std::vector<EpubImageReference>& output,
                          std::string& error);
bool discoverCover(const std::string& opfXml,
                   const std::vector<EpubManifestItem>& manifest,
                   EpubImageReference& cover);
bool imageDimensions(const uint8_t* data, size_t length,
                     const std::string& mediaType,
                     uint32_t& width, uint32_t& height);
}  // namespace epub_content
