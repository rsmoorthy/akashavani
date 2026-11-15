#include "CommandType.h"

#include <cstring>

namespace akashavani {
namespace protocol {

const char* toString(CommandType type) {
  switch (type) {
    case CommandType::Reboot:
      return "reboot";
    case CommandType::SyncTime:
      return "sync_time";
    case CommandType::SetConfig:
      return "set_config";
    case CommandType::CheckForUpdate:
      return "check_for_update";
    case CommandType::UploadLogs:
      return "upload_logs";
    case CommandType::SelfTest:
      return "self_test";
    case CommandType::GetConfig:
      return "get_config";
    case CommandType::DownloadNewFw:
      return "download_new_fw";
    case CommandType::BootWithNewFw:
      return "boot_with_new_fw";
    case CommandType::GetPendingCommands:
      return "get_pending_commands";
    case CommandType::GetDataAgain:
      return "get_data_again";
    case CommandType::DownloadRawData:
      return "download_raw_data";
    case CommandType::PushConfig:
      return "push_config";
    case CommandType::Unknown:
      return "unknown";
  }
  return "unknown";
}

CommandType commandTypeFromString(const char* value) {
  if (value == nullptr) {
    return CommandType::Unknown;
  }
  if (std::strcmp(value, "reboot") == 0) {
    return CommandType::Reboot;
  }
  if (std::strcmp(value, "sync_time") == 0) {
    return CommandType::SyncTime;
  }
  if (std::strcmp(value, "set_config") == 0) {
    return CommandType::SetConfig;
  }
  if (std::strcmp(value, "check_for_update") == 0) {
    return CommandType::CheckForUpdate;
  }
  if (std::strcmp(value, "upload_logs") == 0) {
    return CommandType::UploadLogs;
  }
  if (std::strcmp(value, "self_test") == 0) {
    return CommandType::SelfTest;
  }
  if (std::strcmp(value, "get_config") == 0) {
    return CommandType::GetConfig;
  }
  if (std::strcmp(value, "download_new_fw") == 0) {
    return CommandType::DownloadNewFw;
  }
  if (std::strcmp(value, "boot_with_new_fw") == 0) {
    return CommandType::BootWithNewFw;
  }
  if (std::strcmp(value, "get_pending_commands") == 0) {
    return CommandType::GetPendingCommands;
  }
  if (std::strcmp(value, "get_data_again") == 0) {
    return CommandType::GetDataAgain;
  }
  if (std::strcmp(value, "download_raw_data") == 0) {
    return CommandType::DownloadRawData;
  }
  if (std::strcmp(value, "push_config") == 0) {
    return CommandType::PushConfig;
  }
  return CommandType::Unknown;
}

}  // namespace protocol
}  // namespace akashavani
