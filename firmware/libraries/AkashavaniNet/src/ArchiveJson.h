#pragma once

#include <Arduino.h>

#include <cstddef>

#include "ArchiveRecord.h"

namespace akashavani {

// Serializes one completed archive bucket to the JSON body expected by
// POST /api/v1/archive. Returns false when the record cannot be represented.
bool serializeArchiveJson(const char* deviceId, const ArchiveRecord& record,
                          const char* messageId, String& output);

}  // namespace akashavani
