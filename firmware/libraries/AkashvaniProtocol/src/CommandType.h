#pragma once

#include <cstdint>

namespace akashvani {
namespace protocol {

enum class CommandType : uint8_t {
  Reboot = 0,
  SyncTime = 1,
  SetConfig = 2,
  CheckForUpdate = 3,
  UploadLogs = 4,
  SelfTest = 5,
  GetFwVersion = 6,
  DownloadNewFw = 7,
  BootWithNewFw = 8,
  Unknown = 255,
};

const char* toString(CommandType type);
CommandType commandTypeFromString(const char* value);

}  // namespace protocol
}  // namespace akashvani
