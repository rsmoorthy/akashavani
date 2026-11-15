#pragma once

#include <cstddef>
#include <cstdint>

#include "Status.h"

namespace akashavani {

class IArchive {
 public:
  virtual ~IArchive() = default;

  virtual Status begin() = 0;
  virtual Status append(const char* record, size_t length) = 0;
  virtual Status flush() = 0;
  virtual bool isMounted() const = 0;
  virtual uint64_t recordsWritten() const = 0;
};

}  // namespace akashavani
