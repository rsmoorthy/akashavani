#pragma once

#include <cstdint>

namespace akashavani {

// A deliberately small status type. Drivers never throw.
enum class Status : uint8_t {
  Ok = 0,
  InvalidArgument,
  NotReady,
  NotFound,
  Timeout,
  IoError,
  Unsupported,
  NoMemory,
};

const char* statusToString(Status status);

inline bool isOk(Status status) { return status == Status::Ok; }

}  // namespace akashavani
