#pragma once

#include <stdint.h>
#include <string>
#include <vector>

enum class EpubTocFormat { None, Ncx, NavigationDocument };

struct EpubTocDocument {
  EpubTocFormat format = EpubTocFormat::None;
  std::string path;
};

struct EpubTocEntry {
  std::string title;
  std::string documentPath;
  std::string fragment;
  uint16_t depth = 0;
};

class EpubTableOfContents {
 public:
  // Discovers EPUB 3 `properties="nav"` first, then EPUB 2 spine `toc`/NCX.
  static bool discover(const std::string& opfXml, const std::string& packagePath,
                       EpubTocDocument& document, std::string& error);
  static bool parse(const std::string& xml, const EpubTocDocument& document,
                    std::vector<EpubTocEntry>& entries, std::string& error,
                    size_t maximumEntries = 1024);
};
