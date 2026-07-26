#include "ReadingStateStore.h"
#include "ReadingStateCodec.h"

bool ReadingStateStore::save(const std::string& path, const ReadingState& state) {
  error_.clear(); std::string encoded;
  if (path.empty() || !reading_state_codec::encode(state, encoded)) { error_ = "Estado de leitura invalido"; return false; }
  const std::string temporary = path + ".tmp";
  if (filesystem_.exists(temporary.c_str())) filesystem_.remove(temporary.c_str());
  fs::File file = filesystem_.open(temporary.c_str(), FILE_WRITE);
  if (!file) { error_ = "Nao foi possivel criar estado temporario"; return false; }
  const size_t written = file.write(reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
  file.flush(); file.close();
  if (written != encoded.size()) { filesystem_.remove(temporary.c_str()); error_ = "Gravacao incompleta do estado"; return false; }
  const std::string backup = path + ".bak";
  if (filesystem_.exists(backup.c_str())) filesystem_.remove(backup.c_str());
  const bool hadPrevious = filesystem_.exists(path.c_str());
  if (hadPrevious && !filesystem_.rename(path.c_str(), backup.c_str())) {
    filesystem_.remove(temporary.c_str()); error_ = "Nao foi possivel preservar estado anterior"; return false;
  }
  if (!filesystem_.rename(temporary.c_str(), path.c_str())) {
    if (hadPrevious) filesystem_.rename(backup.c_str(), path.c_str());
    filesystem_.remove(temporary.c_str()); error_ = "Nao foi possivel concluir estado"; return false;
  }
  if (filesystem_.exists(backup.c_str())) filesystem_.remove(backup.c_str());
  return true;
}

bool ReadingStateStore::load(const std::string& path, ReadingState& state) {
  error_.clear(); fs::File file = filesystem_.open(path.c_str(), FILE_READ);
  if (!file) { error_ = "Estado de leitura nao encontrado"; return false; }
  if (file.size() > 4096) { file.close(); error_ = "Estado de leitura excede limite"; return false; }
  std::string encoded; encoded.reserve(file.size());
  while (file.available()) encoded += static_cast<char>(file.read());
  file.close(); return reading_state_codec::decode(encoded, state, error_);
}

bool ReadingStateStore::remove(const std::string& path) {
  error_.clear();
  if (path.empty()) { error_ = "Caminho de estado invalido"; return false; }
  const std::string temporary = path + ".tmp";
  const std::string backup = path + ".bak";
  if (filesystem_.exists(temporary.c_str()) &&
      !filesystem_.remove(temporary.c_str())) {
    error_ = "Nao foi possivel remover estado temporario";
    return false;
  }
  if (filesystem_.exists(backup.c_str()) &&
      !filesystem_.remove(backup.c_str())) {
    error_ = "Nao foi possivel remover backup do estado";
    return false;
  }
  if (filesystem_.exists(path.c_str()) && !filesystem_.remove(path.c_str())) {
    error_ = "Nao foi possivel remover estado de leitura";
    return false;
  }
  return true;
}
