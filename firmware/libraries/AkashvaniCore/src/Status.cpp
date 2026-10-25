#include "Status.h"

namespace akashvani {

const char* statusToString(Status status) {
  switch (status) {
    case Status::Ok:
      return "ok";
    case Status::InvalidArgument:
      return "invalid-argument";
    case Status::NotReady:
      return "not-ready";
    case Status::NotFound:
      return "not-found";
    case Status::Timeout:
      return "timeout";
    case Status::IoError:
      return "io-error";
    case Status::Unsupported:
      return "unsupported";
    case Status::NoMemory:
      return "no-memory";
  }
  return "unknown";
}

}  // namespace akashvani
