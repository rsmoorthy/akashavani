#pragma once

#include <Arduino.h>

#include <cstdint>

#include "DavisObservation.h"

namespace akashvani {

// Serializes the canonical observation envelope used both for the SD archive
// and the cloud upload. Keeping one serializer avoids divergence between the
// two copies of a record.
bool serializeObservationJson(const char* deviceId, const DavisObservation& observation,
                              const char* messageId, int64_t observedAtEpoch,
                              int16_t rssiDbm, String& output);

}  // namespace akashvani
