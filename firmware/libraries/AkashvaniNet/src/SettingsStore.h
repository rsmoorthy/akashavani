#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "DeviceSettings.h"
#include "Status.h"

namespace akashvani {

// Persists settings in NVS. Calling load() on a blank device generates a
// device ID from the STA MAC and fills safe development defaults.
class SettingsStore {
 public:
  explicit SettingsStore(const char* name = "akashvani");

  Status begin();
  Status load(DeviceSettings& settings);
  Status save(const DeviceSettings& settings);
  Status clear();

  bool isOpen() const { return open_; }
  // True once NVS holds values saved through the admin page or a previous
  // firmware. Used to decide whether development defaults may seed a blank
  // device.
  bool configured();
  const char* lastError() const { return lastError_; }

 private:
  void applyDefaults(DeviceSettings& settings);
  void copyString(char* destination, size_t size, const String& value, const char* fallback);

  Preferences preferences_;
  const char* name_;
  bool open_ = false;
  char lastError_[64] = {};
};

}  // namespace akashvani
