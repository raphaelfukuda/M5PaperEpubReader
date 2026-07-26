#pragma once
#include <string>
struct EpubSpineItem {
  std::string idref;
  bool linear = true;
  uint64_t contentOffset = 0;
  uint64_t contentSize = 0;
};
