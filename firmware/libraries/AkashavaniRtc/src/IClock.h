#pragma once

#include <cstdint>

#include "Status.h"

namespace akashavani {

class IClock {
 public:
  virtual ~IClock() = default;

  virtual Status begin() = 0;
  virtual bool isValid() const = 0;
  virtual int64_t nowEpoch() = 0;
  virtual Status setEpoch(int64_t epochSeconds) = 0;
  virtual const char* name() const = 0;
};

}  // namespace akashavani
