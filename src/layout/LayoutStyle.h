#pragma once
#include <stdint.h>
enum class ReaderFontFamily : uint8_t { Book = 0, Sans = 1, Compact = 2 };

struct ReaderSettings { uint16_t fontSize = 16, marginLeft = 28, marginRight = 28, marginTop = 70, marginBottom = 55; float lineSpacing = 1.35f; uint8_t refreshPolicy = 0; ReaderFontFamily fontFamily = ReaderFontFamily::Compact; };
