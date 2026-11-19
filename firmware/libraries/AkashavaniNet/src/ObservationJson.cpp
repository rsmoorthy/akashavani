#include "ObservationJson.h"

#include <ArduinoJson.h>

#include "MessageType.h"
#include "Protocol.h"
#include "TimeUtils.h"

namespace akashavani {

bool serializeObservationJson(const char* deviceId, const DavisObservation& observation,
                              const char* messageId, int64_t observedAtEpoch,
                              int16_t rssiDbm, String& output) {
  if (deviceId == nullptr || messageId == nullptr || observedAtEpoch <= 0) {
    return false;
  }
  char timestamp[21];
  if (!formatIso8601Utc(observedAtEpoch, timestamp, sizeof(timestamp))) {
    return false;
  }

  JsonDocument doc;
  doc["v"] = protocol::kVersion;
  doc["type"] = protocol::toString(protocol::MessageType::Observation);
  doc["device_id"] = deviceId;
  doc["message_id"] = messageId;
  doc["observed_at"] = timestamp;
  doc["sequence"] = observation.sequence;

  JsonObject source = doc["source"].to<JsonObject>();
  source["protocol"] = "davis";
  source["channel"] = observation.channel;
  if (observation.transmitterId >= 1) {
    source["transmitter_id"] = observation.transmitterId;
  }
  source["rssi_dbm"] = rssiDbm;

  JsonObject data = doc["data"].to<JsonObject>();
  if (observation.has(DavisField::Temperature)) {
    data[protocol::field::kTemperatureC] = observation.temperatureC;
  }
  if (observation.has(DavisField::Humidity)) {
    data[protocol::field::kHumidityPct] = observation.humidityPct;
  }
  if (observation.has(DavisField::Pressure)) {
    data[protocol::field::kPressureHpa] = observation.pressureHpa;
  }
  if (observation.has(DavisField::WindSpeed)) {
    data[protocol::field::kWindSpeedMs] = observation.windSpeedMs;
  }
  if (observation.has(DavisField::WindDirection)) {
    data[protocol::field::kWindDirectionDeg] = observation.windDirectionDeg;
  }
  if (observation.has(DavisField::WindGust)) {
    data[protocol::field::kWindGustMs] = observation.windGustMs;
  }
  if (observation.has(DavisField::RainRate)) {
    data[protocol::field::kRainRateMmH] = observation.rainRateMmPerHour;
  }
  if (observation.has(DavisField::RainDay)) {
    data[protocol::field::kRainDayMm] = observation.rainDayMm;
  }
  if (observation.has(DavisField::UvIndex)) {
    data[protocol::field::kUvIndex] = observation.uvIndex;
  }
  if (observation.has(DavisField::SolarRadiation)) {
    data[protocol::field::kSolarRadiationWm2] = observation.solarRadiationWm2;
  }
  if (observation.has(DavisField::BatteryVoltage)) {
    data[protocol::field::kBatteryVoltageV] = observation.batteryVoltageV;
  }

  output = "";
  output.reserve(768);
  serializeJson(doc, output);
  return true;
}

}  // namespace akashavani
