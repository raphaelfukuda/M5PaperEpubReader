#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

enum class EpubLinkKind { Internal, NoteReference, Backlink, External };

struct EpubTarget {
  std::string documentPath;
  std::string fragment;
};

struct EpubLink {
  EpubLinkKind kind = EpubLinkKind::Internal;
  std::string label;
  EpubTarget target;
  // Original URI is retained only for external links; it is never opened here.
  std::string externalUri;
};

struct EpubFootnote {
  std::string fragment;
  std::string text;
};

struct EpubDocumentLinks {
  std::string documentPath;
  std::vector<std::string> fragments;
  std::vector<EpubLink> links;
  std::vector<EpubFootnote> footnotes;
};

struct EpubLinkLimits {
  size_t maximumLinks = 512;
  size_t maximumFragments = 1024;
  size_t maximumFootnotes = 128;
  size_t maximumLabelBytes = 512;
  size_t maximumReferenceBytes = 2048;
  size_t maximumFootnoteBytes = 16 * 1024;
};

class EpubLinkParser {
 public:
  static bool parse(const std::string& xhtml, const std::string& documentPath,
                    EpubDocumentLinks& output, std::string& error,
                    const EpubLinkLimits& limits = EpubLinkLimits{});

  // Resolves only same-book links. Absolute URIs and unsafe ZIP paths return false.
  static bool resolveInternal(const std::string& sourceDocument,
                              const std::string& href, EpubTarget& target);
};
