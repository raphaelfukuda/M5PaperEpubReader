#include "EpubArchive.h"
#include <SD.h>
#include <algorithm>
#include "storage/PathUtils.h"

void* EpubArchive::openCallback(const char* filename, int32_t* size) {
  fs::File file = SD.open(filename, FILE_READ);
  if (!file) return nullptr;
  fs::File* handle = new fs::File(file);
  if (!handle) { file.close(); return nullptr; }
  *size = handle->size();
  return handle;
}
void EpubArchive::closeCallback(void* context) {
  ZIPFILE* zip = static_cast<ZIPFILE*>(context);
  fs::File* file = static_cast<fs::File*>(zip->fHandle);
  if (file) { file->close(); delete file; zip->fHandle = nullptr; }
}
int32_t EpubArchive::readCallback(void* context, uint8_t* buffer, int32_t length) { ZIPFILE* zip = static_cast<ZIPFILE*>(context); fs::File* file = static_cast<fs::File*>(zip->fHandle); return file ? file->read(buffer, length) : 0; }
int32_t EpubArchive::seekCallback(void* context, int32_t position, int origin) { ZIPFILE* zip = static_cast<ZIPFILE*>(context); fs::File* file = static_cast<fs::File*>(zip->fHandle); if (!file) return 0; if (origin == SEEK_END) position += zip->iSize; else if (origin == SEEK_CUR) position += file->position(); return file->seek(position); }

bool EpubArchive::open(const std::string& path) {
  close(); error_.clear();
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard); if (!bus) { error_ = "Barramento SPI ocupado"; return false; }
  const int result = zip_.openZIP(path.c_str(), openCallback, closeCallback, readCallback, seekCallback);
  if (result != UNZ_OK) { error_ = "ZIP invalido ou inacessivel (codigo " + std::to_string(result) + ")"; return false; }
  path_ = path; opened_ = true; return true;
}
void EpubArchive::close() { if (opened_) { ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard); if (bus) { if (entryOpen_) zip_.closeCurrentFile(); zip_.closeZIP(); } } entryOpen_ = false; opened_ = false; entrySize_ = 0; entryRead_ = 0; }

bool EpubArchive::readEntry(const std::string& entryPath, size_t maximumBytes, std::string& output) {
  output.clear(); error_.clear();
  if (!opened_ || !path_utils::isSafeZipPath(entryPath)) { error_ = "Caminho ZIP inseguro ou arquivo fechado"; return false; }
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard); if (!bus) { error_ = "Barramento SPI ocupado"; return false; }
  int rc = zip_.locateFile(entryPath.c_str()); if (rc != UNZ_OK) { error_ = "Entrada ausente: " + entryPath; return false; }
  unz_file_info info{}; char name[257]{}; rc = zip_.getFileInfo(&info, name, sizeof(name), nullptr, 0, nullptr, 0);
  if (rc != UNZ_OK || (info.compression_method != 0 && info.compression_method != 8)) { error_ = "Metodo ZIP nao suportado"; return false; }
  if (info.uncompressed_size > maximumBytes) { error_ = "Entrada ZIP excede limite"; return false; }
  output.reserve(info.uncompressed_size); rc = zip_.openCurrentFile(); if (rc != UNZ_OK) { error_ = "Falha ao abrir entrada ZIP"; return false; }
  uint8_t buffer[1024]; size_t total = 0;
  while (total < info.uncompressed_size) { const int read = zip_.readCurrentFile(buffer, std::min<size_t>(sizeof(buffer), info.uncompressed_size - total)); if (read < 0) { error_ = "Falha durante descompressao"; zip_.closeCurrentFile(); return false; } if (read == 0) break; output.append(reinterpret_cast<char*>(buffer), read); total += read; }
  rc = zip_.closeCurrentFile(); if (rc != UNZ_OK || total != info.uncompressed_size) { error_ = "CRC ou EOF inesperado"; return false; }
  return true;
}

bool EpubArchive::entryUncompressedSize(const std::string& entryPath,
                                        uint64_t& size) {
  size = 0; error_.clear();
  if (!opened_ || entryOpen_ || !path_utils::isSafeZipPath(entryPath)) {
    error_ = "Entrada ZIP insegura ou estado invalido"; return false;
  }
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard);
  if (!bus) { error_ = "Barramento SPI ocupado"; return false; }
  int rc = zip_.locateFile(entryPath.c_str());
  if (rc != UNZ_OK) { error_ = "Entrada ausente: " + entryPath; return false; }
  unz_file_info info{};
  rc = zip_.getFileInfo(&info, nullptr, 0, nullptr, 0, nullptr, 0);
  if (rc != UNZ_OK) { error_ = "Falha ao medir entrada ZIP"; return false; }
  size = info.uncompressed_size;
  return true;
}

bool EpubArchive::beginEntry(const std::string& entryPath, uint64_t maximumBytes) {
  error_.clear(); if (!opened_ || entryOpen_ || !path_utils::isSafeZipPath(entryPath)) { error_ = "Entrada ZIP insegura ou estado invalido"; return false; }
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard); if (!bus) { error_ = "Barramento SPI ocupado"; return false; }
  int rc = zip_.locateFile(entryPath.c_str()); if (rc != UNZ_OK) { error_ = "Capitulo ausente: " + entryPath; return false; }
  unz_file_info info{}; rc = zip_.getFileInfo(&info, nullptr, 0, nullptr, 0, nullptr, 0);
  if (rc != UNZ_OK || (info.compression_method != 0 && info.compression_method != 8)) { error_ = "Compressao de capitulo nao suportada"; return false; }
  if (info.uncompressed_size > maximumBytes) { error_ = "Capitulo excede limite plausivel"; return false; }
  rc = zip_.openCurrentFile(); if (rc != UNZ_OK) { error_ = "Falha ao abrir capitulo"; return false; }
  entryOpen_ = true; entrySize_ = info.uncompressed_size; entryRead_ = 0; return true;
}

int EpubArchive::readEntryChunk(uint8_t* buffer, size_t length) {
  if (!entryOpen_ || !buffer || length == 0) { error_ = "Leitura ZIP em estado invalido"; return -1; }
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard); if (!bus) { error_ = "Barramento SPI ocupado durante leitura"; return -2; }
  const int result = zip_.readCurrentFile(buffer, length);
  if (result < 0) error_ = "Falha incremental de descompressao";
  else if (result == 0 && entryRead_ != entrySize_) error_ = "Cartao removido ou EOF inesperado no capitulo";
  else entryRead_ += static_cast<uint64_t>(result);
  if (result == 0 && entryRead_ != entrySize_) return -3;
  return result;
}

bool EpubArchive::endEntry() {
  if (!entryOpen_) return true; ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard); if (!bus) return false;
  const int rc = zip_.closeCurrentFile(); entryOpen_ = false; const bool complete = entryRead_ == entrySize_; entrySize_ = 0; entryRead_ = 0; if (rc != UNZ_OK || !complete) { error_ = "CRC invalido ou capitulo incompleto"; return false; } return true;
}

bool EpubArchive::cancelEntry() {
  if (!entryOpen_) return true;
  ScopedSpiBus bus(busGuard_, SpiBusOwner::SdCard);
  if (!bus) { error_ = "Barramento SPI ocupado ao cancelar capitulo"; return false; }
  zip_.closeCurrentFile();
  entryOpen_ = false;
  entrySize_ = 0;
  entryRead_ = 0;
  return true;
}
