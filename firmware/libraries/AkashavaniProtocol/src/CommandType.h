#pragma once

#include <cstdint>

namespace akashavani {
namespace protocol {

enum class CommandType : uint8_t {
  Reboot = 0,
  SyncTime = 1,
  SetConfig = 2,
  CheckForUpdate = 3,
  UploadLogs = 4,
  SelfTest = 5,
  GetConfig = 6,
  DownloadNewFw = 7,
  BootWithNewFw = 8,
  GetPendingCommands = 9,
  GetDataAgain = 10,
  DownloadRawData = 11,
  PushConfig = 12,
  Unknown = 255,
};

const char* toString(CommandType type);
CommandType commandTypeFromString(const char* value);

}  // namespace protocol
}  // namespace akashavani
