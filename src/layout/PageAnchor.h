#pragma once
#include <stdint.h>
struct PageAnchor { uint32_t spineIndex = 0; uint64_t uncompressedOffset = 0; uint32_t parserCheckpoint = 0; };
