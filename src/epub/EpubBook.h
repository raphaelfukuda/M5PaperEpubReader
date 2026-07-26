#pragma once
#include <string>
#include <vector>
#include "EpubManifestItem.h"
#include "EpubSpineItem.h"
#include "EpubTableOfContents.h"
struct EpubBook {
  std::string filePath, packagePath, title, author, language;
  uint64_t totalLinearBytes = 0;
  std::vector<EpubManifestItem> manifest;
  std::vector<EpubSpineItem> spine;
  std::vector<EpubTocEntry> tableOfContents;
};
