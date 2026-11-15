#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <cstddef>

#include "DeviceSettings.h"

namespace akashvani {

// IANA name bound used by the firmware. The server validates the name itself;
// this only rejects empty, over-long or obviously malformed values.
bool isValidTimezoneName(const char* value, size_t capacity);

// Serializes the device-owned configuration document sent to the server.
bool serializeDeviceConfigJson(const DeviceSettings& settings, String& output);

// Applies a `push_config` document in place. Fields absent from the document
// keep their current values. Returns false and fills `error` on invalid input.
bool applyDeviceConfigJson(JsonVariantConst document, DeviceSettings& settings,
                           bool& networkChanged, char* error, size_t errorSize);

}  // namespace akashvani
