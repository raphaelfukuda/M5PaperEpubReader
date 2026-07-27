#include "LibraryThumbnailCache.h"

#include <SD.h>
#include <cstring>

namespace {
constexpr char kCacheDirectory[] = "/.m5epub-cache";
constexpr uint32_t kMagic = 0x4D354C54;  // M5LT
constexpr uint16_t kVersion = 2;
constexpr uint32_t kMaximumTitleBytes = 1024;
constexpr uint32_t kMaximumPathBytes = 2048;
constexpr uint32_t kMaximumThumbnailBytes = 128 * 1024;

struct CacheHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t width;
  uint16_t height;
  uint16_t reserved;
  uint64_t epubSize;
  uint64_t modifiedTime;
  uint32_t pathLength;
  uint32_t titleLength;
  uint32_t pixelBytes;
};

uint32_t fnv1a(const std::string& value) {
  uint32_t hash = 2166136261UL;
  for (const uint8_t byte : value) {
    hash ^= byte;
    hash *= 16777619UL;
  }
  return hash;
}

bool readExact(File& file, void* destination, size_t length) {
  return file.read(static_cast<uint8_t*>(destination), length) == length;
}

bool writeExact(File& file, const void* source, size_t length) {
  return file.write(static_cast<const uint8_t*>(source), length) == length;
}
}  // namespace

std::string LibraryThumbnailCache::cachePath(const std::string& bookPath) const {
  char name[48];
  snprintf(name, sizeof(name), "%s/%08lx.bin", kCacheDirectory,
           static_cast<unsigned long>(fnv1a(bookPath)));
  return name;
}

bool LibraryThumbnailCache::load(const FileEntry& book, std::string& title,
                                 uint16_t& width, uint16_t& height,
                                 std::string& pixels) {
  title.clear();
  pixels.clear();
  ScopedSpiBus bus(guard_, SpiBusOwner::SdCard);
  if (!bus) return false;
  const std::string path = cachePath(book.fullPath);
  if (!fs_.exists(path.c_str())) return false;
  File file = fs_.open(path.c_str(), FILE_READ);
  if (!file) return false;
  CacheHeader header{};
  if (!readExact(file, &header, sizeof(header)) || header.magic != kMagic ||
      header.version != kVersion || header.epubSize != book.size ||
      header.modifiedTime != book.modifiedTime ||
      header.pathLength == 0 || header.pathLength > kMaximumPathBytes ||
      header.titleLength > kMaximumTitleBytes ||
      header.pixelBytes > kMaximumThumbnailBytes ||
      ((header.width == 0 || header.height == 0)
           ? header.pixelBytes != 0
           : header.pixelBytes !=
                 (static_cast<uint32_t>(header.width) * header.height + 1U) / 2U)) {
    return false;
  }
  std::string storedPath(header.pathLength, '\0');
  title.resize(header.titleLength);
  pixels.resize(header.pixelBytes);
  if (!readExact(file, &storedPath[0], storedPath.size()) ||
      (!title.empty() && !readExact(file, &title[0], title.size())) ||
      (!pixels.empty() && !readExact(file, &pixels[0], pixels.size())) ||
      storedPath != book.fullPath) {
    title.clear();
    pixels.clear();
    return false;
  }
  width = header.width;
  height = header.height;
  return true;
}

bool LibraryThumbnailCache::save(const FileEntry& book, const std::string& title,
                                 uint16_t width, uint16_t height,
                                 const std::string& pixels) {
  if (book.fullPath.empty() || book.fullPath.size() > kMaximumPathBytes ||
      title.size() > kMaximumTitleBytes ||
      ((width == 0 || height == 0)
           ? !pixels.empty()
           : pixels.size() != (static_cast<size_t>(width) * height + 1U) / 2U) ||
      pixels.size() > kMaximumThumbnailBytes)
    return false;
  ScopedSpiBus bus(guard_, SpiBusOwner::SdCard);
  if (!bus) return false;
  if (!fs_.exists(kCacheDirectory) && !fs_.mkdir(kCacheDirectory)) return false;
  const std::string destination = cachePath(book.fullPath);
  const std::string temporary = destination + ".tmp";
  if (fs_.exists(temporary.c_str())) fs_.remove(temporary.c_str());
  File file = fs_.open(temporary.c_str(), FILE_WRITE);
  if (!file) return false;
  CacheHeader header{kMagic, kVersion, width, height, 0, book.size,
                     book.modifiedTime,
                     static_cast<uint32_t>(book.fullPath.size()),
                     static_cast<uint32_t>(title.size()),
                     static_cast<uint32_t>(pixels.size())};
  const bool written = writeExact(file, &header, sizeof(header)) &&
                       writeExact(file, book.fullPath.data(), book.fullPath.size()) &&
                       (title.empty() || writeExact(file, title.data(), title.size())) &&
                       (pixels.empty() || writeExact(file, pixels.data(), pixels.size()));
  file.close();
  if (!written) {
    fs_.remove(temporary.c_str());
    return false;
  }
  if (fs_.exists(destination.c_str())) fs_.remove(destination.c_str());
  return fs_.rename(temporary.c_str(), destination.c_str());
}
