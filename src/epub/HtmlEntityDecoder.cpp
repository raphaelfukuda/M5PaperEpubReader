#include "HtmlEntityDecoder.h"

#include <cstdlib>
#include <cstdint>
#include <cstring>

namespace {

struct NamedEntity {
  const char* name;
  uint16_t codepoint;
};

// HTML 4 Latin-1 entities commonly found in Portuguese EPUB files.
constexpr NamedEntity kNamedEntities[] = {
    {"nbsp", 0x00A0}, {"iexcl", 0x00A1}, {"cent", 0x00A2},
    {"pound", 0x00A3}, {"copy", 0x00A9}, {"ordf", 0x00AA},
    {"laquo", 0x00AB}, {"reg", 0x00AE}, {"deg", 0x00B0},
    {"plusmn", 0x00B1}, {"sup2", 0x00B2}, {"sup3", 0x00B3},
    {"micro", 0x00B5}, {"para", 0x00B6}, {"middot", 0x00B7},
    {"ordm", 0x00BA}, {"raquo", 0x00BB}, {"frac14", 0x00BC},
    {"frac12", 0x00BD}, {"frac34", 0x00BE}, {"Agrave", 0x00C0},
    {"Aacute", 0x00C1}, {"Acirc", 0x00C2}, {"Atilde", 0x00C3},
    {"Auml", 0x00C4}, {"Ccedil", 0x00C7}, {"Egrave", 0x00C8},
    {"Eacute", 0x00C9}, {"Ecirc", 0x00CA}, {"Euml", 0x00CB},
    {"Igrave", 0x00CC}, {"Iacute", 0x00CD}, {"Icirc", 0x00CE},
    {"Iuml", 0x00CF}, {"Ograve", 0x00D2}, {"Oacute", 0x00D3},
    {"Ocirc", 0x00D4}, {"Otilde", 0x00D5}, {"Ouml", 0x00D6},
    {"Ugrave", 0x00D9}, {"Uacute", 0x00DA}, {"Ucirc", 0x00DB},
    {"Uuml", 0x00DC}, {"Yacute", 0x00DD}, {"agrave", 0x00E0},
    {"aacute", 0x00E1}, {"acirc", 0x00E2}, {"atilde", 0x00E3},
    {"auml", 0x00E4}, {"ccedil", 0x00E7}, {"egrave", 0x00E8},
    {"eacute", 0x00E9}, {"ecirc", 0x00EA}, {"euml", 0x00EB},
    {"igrave", 0x00EC}, {"iacute", 0x00ED}, {"icirc", 0x00EE},
    {"iuml", 0x00EF}, {"ograve", 0x00F2}, {"oacute", 0x00F3},
    {"ocirc", 0x00F4}, {"otilde", 0x00F5}, {"ouml", 0x00F6},
    {"ugrave", 0x00F9}, {"uacute", 0x00FA}, {"ucirc", 0x00FB},
    {"uuml", 0x00FC}, {"yacute", 0x00FD}, {"yuml", 0x00FF},
};

void appendUtf8(std::string& out, unsigned long cp) {
  if (cp <= 0x7F) {
    out += static_cast<char>(cp);
  } else if (cp <= 0x7FF) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

bool appendNamedEntity(std::string& out, const std::string& entity) {
  if (entity == "amp") { out += '&'; return true; }
  if (entity == "lt") { out += '<'; return true; }
  if (entity == "gt") { out += '>'; return true; }
  if (entity == "quot") { out += '"'; return true; }
  if (entity == "apos") { out += '\''; return true; }
  for (const auto& item : kNamedEntities) {
    if (entity == item.name) {
      appendUtf8(out, item.codepoint);
      return true;
    }
  }
  return false;
}

}  // namespace

std::string html_entities::decode(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size();) {
    if (input[i] != '&') {
      out += input[i++];
      continue;
    }
    const size_t semicolon = input.find(';', i + 1);
    if (semicolon == std::string::npos || semicolon - i > 12) {
      out += input[i++];
      continue;
    }
    const std::string entity = input.substr(i + 1, semicolon - i - 1);
    bool decoded = appendNamedEntity(out, entity);
    if (!decoded && !entity.empty() && entity[0] == '#') {
      char* end = nullptr;
      const int base = entity.size() > 1 &&
                               (entity[1] == 'x' || entity[1] == 'X')
                           ? 16
                           : 10;
      const char* start = entity.c_str() + (base == 16 ? 2 : 1);
      const unsigned long cp = std::strtoul(start, &end, base);
      if (*start && end && *end == 0 && cp <= 0xFFFF &&
          !(cp >= 0xD800 && cp <= 0xDFFF)) {
        appendUtf8(out, cp);
        decoded = true;
      }
    }
    if (!decoded) out.append(input, i, semicolon - i + 1);
    i = semicolon + 1;
  }
  return out;
}
