#include "ReadingStateCodec.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>

namespace {
std::string escape(const std::string& value) {
  static const char hex[] = "0123456789ABCDEF";
  std::string result;
  for (unsigned char c : value) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' || c == '-') result += static_cast<char>(c);
    else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
  }
  return result;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool unescape(const std::string& value, std::string& result) {
  result.clear();
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '%') { result += value[i]; continue; }
    if (i + 2 >= value.size()) return false;
    const int high = hexValue(value[i + 1]), low = hexValue(value[i + 2]);
    if (high < 0 || low < 0) return false;
    result += static_cast<char>((high << 4) | low); i += 2;
  }
  return true;
}

bool parseUnsigned(const std::map<std::string, std::string>& fields, const char* key, uint64_t maximum, uint64_t& result) {
  const auto it = fields.find(key);
  if (it == fields.end() || it->second.empty() ||
      !std::isdigit(static_cast<unsigned char>(it->second[0]))) return false;
  char* end = nullptr;
  const unsigned long long parsed = std::strtoull(it->second.c_str(), &end, 10);
  if (!end || *end != '\0' || parsed > maximum) return false;
  result = parsed; return true;
}
}

namespace reading_state_codec {
bool encode(const ReadingState& state, std::string& output) {
  if (state.bookPath.empty() || state.version != ReadingState::kCurrentVersion) return false;
  std::ostringstream stream;
  stream << "M5EPUB-STATE\nversion=" << state.version << "\nbook=" << escape(state.bookPath)
         << "\nspine=" << state.spineIndex << "\noffset=" << state.textOffset
         << "\ncheckpoint=" << state.parserCheckpoint << "\nfont=" << state.fontSize
         << "\nspacing=" << state.lineSpacing << "\nmargin=" << state.horizontalMargin << "\n";
  output = stream.str(); return true;
}

bool decode(const std::string& input, ReadingState& state, std::string& error) {
  error.clear();
  if (input.size() > 4096 || input.compare(0, 13, "M5EPUB-STATE\n") != 0) { error = "Cabecalho de estado invalido"; return false; }
  std::map<std::string, std::string> fields;
  size_t position = 13;
  while (position < input.size()) {
    const size_t end = input.find('\n', position), equals = input.find('=', position);
    const size_t lineEnd = end == std::string::npos ? input.size() : end;
    if (equals == std::string::npos || equals >= lineEnd) { error = "Linha de estado invalida"; return false; }
    fields[input.substr(position, equals - position)] = input.substr(equals + 1, lineEnd - equals - 1);
    position = lineEnd + 1;
  }
  ReadingState parsed; uint64_t value = 0;
  if (!parseUnsigned(fields, "version", UINT32_MAX, value) || value != ReadingState::kCurrentVersion) { error = "Versao de estado nao suportada"; return false; } parsed.version = value;
  const auto book = fields.find("book");
  if (book == fields.end() || !unescape(book->second, parsed.bookPath) || parsed.bookPath.empty()) { error = "Caminho do livro invalido"; return false; }
  if (!parseUnsigned(fields, "spine", UINT32_MAX, value)) { error = "Indice do spine invalido"; return false; } parsed.spineIndex = value;
  if (!parseUnsigned(fields, "offset", UINT64_MAX, parsed.textOffset)) { error = "Offset invalido"; return false; }
  if (!parseUnsigned(fields, "checkpoint", UINT32_MAX, value)) { error = "Checkpoint invalido"; return false; } parsed.parserCheckpoint = value;
  if (!parseUnsigned(fields, "font", UINT16_MAX, value) || value < 8) { error = "Fonte invalida"; return false; } parsed.fontSize = value;
  if (!parseUnsigned(fields, "spacing", UINT16_MAX, value)) { error = "Espacamento invalido"; return false; } parsed.lineSpacing = value;
  if (!parseUnsigned(fields, "margin", UINT16_MAX, value)) { error = "Margem invalida"; return false; } parsed.horizontalMargin = value;
  state = parsed; return true;
}
}
