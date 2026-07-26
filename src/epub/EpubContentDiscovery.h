#pragma once

#include <stddef.h>
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
}  // namespace epub_content
