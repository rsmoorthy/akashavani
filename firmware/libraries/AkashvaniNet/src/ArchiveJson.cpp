#include "ArchiveJson.h"

#include <ArduinoJson.h>

namespace akashvani {

bool serializeArchiveJson(const char* deviceId, const ArchiveRecord& record,
                          const char* messageId, String& output) {
  if (deviceId == nullptr || deviceId[0] == '\0' || messageId == nullptr ||
      messageId[0] == '\0') {
    return false;
  }

  JsonDocument doc;
  doc["v"] = 1;
  doc["type"] = "archive";
  doc["device_id"] = deviceId;
  doc["message_id"] = messageId;
  doc["bucket_end_epoch"] = static_cast<long long>(record.bucketEndEpoch);
  doc["interval_minutes"] = record.intervalMinutes;

  JsonObject source = doc["source"].to<JsonObject>();
  source["protocol"] = "davis";
  if (record.transmitterId > 0) {
    source["transmitter_id"] = record.transmitterId;
  }

  JsonObject sums = doc["sums"].to<JsonObject>();
  sums["out_temp_c"] = record.outTempCSum;
  sums["out_humidity_pct"] = record.outHumidityPctSum;
  sums["pressure_hpa"] = record.pressureHpaSum;
  sums["in_temp_c"] = record.inTempCSum;
  sums["in_humidity_pct"] = record.inHumidityPctSum;
  sums["wind_speed_m_s"] = record.windSpeedMsSum;
  sums["uv_index"] = record.uvIndexSum;
  sums["solar_wm2"] = record.solarWm2Sum;

  JsonObject counts = doc["counts"].to<JsonObject>();
  counts["out_temp"] = record.outTempCount;
  counts["out_humidity"] = record.outHumidityCount;
  counts["pressure"] = record.pressureCount;
  counts["in_temp"] = record.inTempCount;
  counts["in_humidity"] = record.inHumidityCount;
  counts["wind_speed"] = record.windSpeedCount;
  counts["wind_dir"] = record.windDirCount;
  counts["wind_gust"] = record.windGustCount;
  counts["rain"] = record.rainCount;
  counts["solar"] = record.solarCount;
  counts["uv"] = record.uvCount;
  counts["supercap"] = record.supercapCount;

  JsonObject windDir = doc["wind_dir"].to<JsonObject>();
  windDir["sin_sum"] = record.windDirSinSum;
  windDir["cos_sum"] = record.windDirCosSum;
  windDir["speed_sum"] = record.windDirSpeedSum;

  JsonObject gust = doc["wind_gust"].to<JsonObject>();
  if (record.windGustCount > 0) {
    gust["max_m_s"] = record.windGustMaxMs;
    gust["dir_deg"] = record.windGustDirDeg;
  } else {
    gust["max_m_s"] = nullptr;
    gust["dir_deg"] = nullptr;
  }

  JsonObject rain = doc["rain"].to<JsonObject>();
  rain["tips"] = record.rainTips;
  if (record.rainRateValid) {
    rain["rate_raw"] = record.rainRateRaw;
    rain["rate_high"] = record.rainRateHigh;
    rain["rate_valid"] = true;
  } else {
    rain["rate_raw"] = nullptr;
    rain["rate_high"] = false;
    rain["rate_valid"] = false;
  }

  JsonObject latest = doc["latest"].to<JsonObject>();
  latest["battery_low"] = record.batteryLow;
  if (record.supercapCount > 0) {
    latest["supercap_v"] = record.supercapV;
    latest["supercap_min_v"] = record.supercapMinV;
  } else {
    latest["supercap_v"] = nullptr;
    latest["supercap_min_v"] = nullptr;
  }

  JsonObject quality = doc["quality"].to<JsonObject>();
  quality["frames_expected"] = record.framesExpected;
  quality["frames_valid"] = record.framesValid;
  quality["frames_crc_bad"] = record.framesCrcBad;
  quality["resync_count"] = record.resyncCount;
  quality["search_count"] = record.searchCount;
  quality["gap_slots"] = record.gapSlots;
  quality["max_gap_slots"] = record.maxGapSlots;
  if (record.rssiCount > 0) {
    quality["rssi_avg_dbm"] = record.rssiSum / static_cast<double>(record.rssiCount);
    quality["rssi_min_dbm"] = record.rssiMin;
    quality["rssi_max_dbm"] = record.rssiMax;
  } else {
    quality["rssi_avg_dbm"] = nullptr;
    quality["rssi_min_dbm"] = nullptr;
    quality["rssi_max_dbm"] = nullptr;
  }

  output = "";
  if (serializeJson(doc, output) == 0) {
    return false;
  }
  return true;
}

}  // namespace akashvani
