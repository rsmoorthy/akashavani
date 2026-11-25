#include "SdStorage.h"

#include <cstdio>
#include <cstring>

namespace akashavani {
namespace {

bool startsWith(const char* value, const char* prefix) {
  return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

bool hasParentComponent(const char* path) {
  const char* cursor = path;
  while (*cursor != '\0') {
    const char* slash = std::strchr(cursor, '/');
    const size_t length = slash == nullptr ? std::strlen(cursor)
                                           : static_cast<size_t>(slash - cursor);
    if (length == 2 && cursor[0] == '.' && cursor[1] == '.') {
      return true;
    }
    if (slash == nullptr) {
      break;
    }
    cursor = slash + 1;
  }
  return false;
}

}  // namespace

bool SdStorage::isValidPath(const char* path, bool allowRoot) {
  if (path == nullptr) {
    return false;
  }
  const size_t length = std::strlen(path);
  if (length == 0 || length >= kMaxPath || path[0] != '/') {
    return false;
  }
  if (length == 1) {
    return allowRoot;
  }
  if (path[length - 1] == '/' || std::strstr(path, "//") != nullptr ||
      std::strchr(path, '\\') != nullptr || hasParentComponent(path)) {
    return false;
  }
  for (const char* p = path; *p != '\0'; ++p) {
    if (static_cast<unsigned char>(*p) < 0x20) {
      return false;
    }
  }
  return true;
}

bool SdStorage::isAllowedPath(const char* path, bool allowRoot) {
  if (!isValidPath(path, allowRoot)) {
    return false;
  }
  if (std::strcmp(path, "/") == 0) {
    return allowRoot;
  }
  const char* roots[] = {"/archive", "/raw", "/logs", "/state"};
  for (const char* root : roots) {
    const size_t length = std::strlen(root);
    if (std::strcmp(path, root) == 0 ||
        (startsWith(path, root) && path[length] == '/')) {
      return true;
    }
  }
  return false;
}

Status SdStorage::status(uint64_t& totalBytes, uint64_t& usedBytes) {
  if (SD.cardType() == CARD_NONE) {
    return Status::NotReady;
  }
  totalBytes = SD.totalBytes();
  usedBytes = SD.usedBytes();
  return Status::Ok;
}

Status SdStorage::list(const char* path, SdStorageEntry* entries, size_t capacity,
                       size_t& count, bool& truncated) {
  count = 0;
  truncated = false;
  if (entries == nullptr || capacity == 0 || !isAllowedPath(path, true)) {
    return Status::InvalidArgument;
  }
  File dir = SD.open(path);
  if (!dir) {
    return Status::NotFound;
  }
  if (!dir.isDirectory()) {
    dir.close();
    return Status::InvalidArgument;
  }
  File entry = dir.openNextFile();
  while (entry) {
    if (count >= capacity) {
      truncated = true;
      entry.close();
      break;
    }
    const char* rawName = entry.name();
    const char* name = rawName;
    if (rawName != nullptr) {
      const char* slash = std::strrchr(rawName, '/');
      if (slash != nullptr) {
        name = slash + 1;
      }
    }
    SdStorageEntry& output = entries[count++];
    std::snprintf(output.name, sizeof(output.name), "%s", name == nullptr ? "" : name);
    output.directory = entry.isDirectory();
    output.size = output.directory ? 0 : static_cast<uint32_t>(entry.size());
    output.modified = output.directory ? 0 : static_cast<uint32_t>(entry.getLastWrite());
    entry.close();
    entry = dir.openNextFile();
  }
  entry.close();
  dir.close();
  return Status::Ok;
}

Status SdStorage::readChunk(const char* path, uint32_t offset, uint8_t* buffer,
                            size_t capacity, size_t& read, bool& eof) {
  read = 0;
  eof = false;
  if (buffer == nullptr || capacity == 0 || !isAllowedPath(path, false)) {
    return Status::InvalidArgument;
  }
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return Status::NotFound;
  }
  if (file.isDirectory()) {
    file.close();
    return Status::InvalidArgument;
  }
  const size_t size = file.size();
  if (offset >= size) {
    eof = true;
    file.close();
    return Status::Ok;
  }
  if (!file.seek(offset)) {
    file.close();
    return Status::IoError;
  }
  const size_t remaining = size - offset;
  const size_t wanted = remaining < capacity ? remaining : capacity;
  read = file.read(buffer, wanted);
  eof = offset + read >= size;
  file.close();
  return read == wanted ? Status::Ok : Status::IoError;
}

Status SdStorage::ensureDirectory(const char* directory) {
  if (directory == nullptr || directory[0] != '/' ||
      std::strlen(directory) >= kMaxPath) {
    return Status::InvalidArgument;
  }
  char buffer[kMaxPath];
  std::snprintf(buffer, sizeof(buffer), "%s", directory);
  const size_t length = std::strlen(buffer);
  for (size_t i = 1; i <= length; ++i) {
    if (buffer[i] == '/' || buffer[i] == '\0') {
      const char saved = buffer[i];
      buffer[i] = '\0';
      if (!SD.exists(buffer) && !SD.mkdir(buffer)) {
        return Status::IoError;
      }
      buffer[i] = saved;
    }
  }
  return Status::Ok;
}

}  // namespace akashavani
