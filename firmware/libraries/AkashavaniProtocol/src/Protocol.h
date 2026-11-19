#pragma once

#include <cstddef>
#include <cstdint>

namespace akashavani {
namespace protocol {

constexpr int kVersion = 1;
constexpr size_t kMaxMessageBytes = 4096;
constexpr size_t kDeviceIdMaxLength = 32;
constexpr size_t kMessageIdMaxLength = 64;

// Canonical observation field names. Keep in sync with
// protocol/schema/observation.schema.json.
namespace field {
constexpr const char* kTemperatureC = "temperature_c";
constexpr const char* kHumidityPct = "humidity_pct";
constexpr const char* kPressureHpa = "pressure_hpa";
constexpr const char* kWindSpeedMs = "wind_speed_ms";
constexpr const char* kWindDirectionDeg = "wind_direction_deg";
constexpr const char* kWindGustMs = "wind_gust_ms";
constexpr const char* kRainRateMmH = "rain_rate_mm_h";
constexpr const char* kRainDayMm = "rain_day_mm";
constexpr const char* kUvIndex = "uv_index";
constexpr const char* kSolarRadiationWm2 = "solar_radiation_wm2";
constexpr const char* kBatteryVoltageV = "battery_voltage_v";
constexpr const char* kTransmitterId = "transmitter_id";
constexpr const char* kStationChannel = "station_channel";
constexpr const char* kSequence = "sequence";
}  // namespace field

}  // namespace protocol
}  // namespace akashavani
