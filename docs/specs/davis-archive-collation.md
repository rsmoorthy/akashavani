# Davis 5-minute archive collation and Weewx field mapping

**Status:** implemented and bench-tested 2026-11-01
**Scope:** data analysis, collation contract and implementation notes.
**Inputs:** the implemented Davis decoder (`firmware/libraries/AkashvaniDavis`),
the captured frames in `firmware/tests/native/vectors/davis_iss_frames.txt`, and
the reference Weewx database `weewx-latest.sdb` (table `archive`).

## 0. Confirmed decisions (2026-11-01)

These decisions supersede the options in the earlier draft:

| Topic | Decision |
| --- | --- |
| Archive units | **SI only** in SQLite (C, hPa, m/s, mm, mm/h, W/m2, UV index, degrees). No Weewx US units in the primary archive. |
| Conversions and derived values | **Python server only.** The ESP32 decodes the Davis bit fields to SI values and aggregates the 5-minute bucket; it does not convert to US units or compute Weewx derived fields. |
| Mean transport and storage | The receiver sends **sum + count** for scalar fields and **vector sums + count** for wind direction. The server divides. SQLite stores the resulting **mean + sample count** for each measured field, plus overall frame counters. |
| AP status page | Shows the latest decoded values in SI. This is the only place the receiver needs current values rather than bucket aggregates. |
| Site configuration | Server-side, with defaults: latitude 13.4867 N, longitude 77.7064 E, altitude 940 m, timezone Asia/Kolkata (IST, UTC+05:30). |
| Pressure | BME280 supplies **station pressure**. The server derives `barometer` and `altimeter` from station pressure, site altitude and temperature. The ISS does not send pressure. |
| BME280 indoor values | BME280 temperature and humidity populate `in_temp_c` and `in_humidity_pct`; `in_dewpoint_c` is derived server-side. |
| Rain | Store both units: `rain_mm` and `rain_in`. The receiver sends tip counts, not converted distances, so the bucket size is a server setting. |
| Rain rate | Store both definitions: the raw Davis `0x5` instantaneous rate and the Weewx-style 15-minute trailing accumulation. |
| UV / solar | Both sensors are assumed present. Solar is assumed to be message `0x6` until confirmed against a capture. |
| ET / maxSolarRad | Deferred; see section 6.2. `maxSolarRad` is a theoretical clear-sky value, not a measurement. ET needs a one-hour radiation/temperature/wind window. |

## 1. Executive summary

The receiver currently gets a sequence of independent 8-byte Davis ISS frames.
Each frame carries:

- wind speed and wind direction on every frame;
- one message-specific field, such as temperature, humidity, rain rate or UV;
- a transmitter ID and a battery-low flag.

To produce a Weewx-like 5-minute record we need four things:

1. **A 5-minute collector** that resolves repeated frames into one value per
   archive field. It must aggregate, not just take the last frame.
2. **A unit contract.** The wire/decoded values are already SI. The archive
   will store SI. Weewx's US units are used only as a reference in this
   document and for validation tests.
3. **Derived fields** that Weewx calculates: dewpoint, windchill, heat index,
   humidex, apparent temperature, wind run, barometer, altimeter, cloud base,
   maximum solar radiation and (optionally) ET. Most are simple formulas and
   were spot-checked against the reference database.
4. **A pressure source.** The ISS frame subset does not contain barometric
   pressure. The receiver's BME280 can supply station pressure; the archive
   `pressure` field is station pressure, while `barometer` and `altimeter` are
   derived with the site altitude. The BME280 is present on the bench board but
   is not yet integrated into the firmware, so its driver and sampling loop are
   part of the implementation.

The reference database also corrects two assumptions that must not be carried
into the implementation:

- its rain increments are **0.01 inch (0.254 mm) per tip**, not the 0.2 mm that
  the current decoder assumes;
- its `rainRate` is **not** the raw Davis rain-rate message. It is a 15-minute
  trailing accumulation, exactly matching Weewx's `RainRater`. The new archive
  stores both definitions, because they answer different questions.

## 2. What the Davis ISS gives us

The current decoder is the source of truth for the bit layout. The following is
the complete set of fields that can be derived from the implemented message
types. Message `0x3` is unknown and carries only the common wind fields; `0x7`
is a legacy/alternate solar mapping and must be confirmed against the real ISS.

| Message | Field | Raw encoding | Decoded unit | Repeats per 5 min? |
| --- | --- | --- | --- | --- |
| every frame | wind speed | byte 1, integer mph | m/s | yes, every frame |
| every frame | wind direction | byte 2, 0-255 | degrees | yes, every frame |
| every frame | transmitter ID | byte 0 bits 0-2 | 1-8 | yes, every frame |
| every frame | battery low | byte 0 bit 3 | boolean | yes, every frame |
| `0x2` | goldcap/supercap voltage | bytes 3-4, 10-bit, /100 | V | once per cycle |
| `0x3` | unknown | - | - | once per cycle |
| `0x4` | UV index | bytes 3-4, 10-bit, /50 | UV index | once per cycle (optional sensor) |
| `0x5` | rain rate | bytes 3-4, 10-bit click interval + high/low-rate flag | clicks/h; mm/h after bucket size | once per cycle |
| `0x6` | solar radiation (VP2) | bytes 3-4, 10-bit, x1.757936 | W/m2 | once per cycle; assumed mapping |
| `0x7` | legacy solar / Vue panel voltage | byte 3 + byte 4 bits 6-7 | W/m2 (unverified) | once per cycle (model dependent) |
| `0x8` | outside temperature | bytes 3-4, 16-bit, F x 160 | C | once per cycle |
| `0x9` | wind gust | byte 3, integer mph | m/s | once per cycle |
| `0xA` | outside humidity | bytes 3-4, 12-bit, % x 10 | % | once per cycle |
| `0xE` | rain counter | byte 3 bits 0-6, tip count | tips | once per cycle |

Additional receiver metadata available but not in the ISS frame:

- RSSI per frame (dBm);
- receive timestamp;
- radio channel;
- CRC/validity status;
- the BME280's station pressure, enclosure temperature and enclosure humidity.

### 2.1 Raw-format details that still need verification

These are not blockers for the data plan, but they must be settled before the
numbers are trusted:

- **Wind direction:** the decoder currently maps `raw * 360 / 255` and rotates
  by 180 degrees. The prototype also contains a `9 + raw * 342 / 255` dead-zone
  mapping. The correct mapping must be confirmed against a known vane position.
- **Negative temperature:** the current decoder reads the 16-bit temperature
  field as unsigned. If the ISS uses a signed value below 0 F, the decoder will
  reject it. The reference database never goes below 48.9 F, so this has not
  been exercised.
- **Solar message:** `0x6` is assumed until the real ISS capture confirms it.
  If the station turns out to use `0x7`, the formula must change; the two are
  not interchangeable.
- **UV/solar sentinels:** `0x3FF` for UV and `0x3FE`/`0x3FF` for solar mean
  "sensor absent" in the current decoder. Keep them as `null`, never zero.
- **Rain bucket size:** the ISS frame carries tip counts, not the collector
  resolution. The reference database indicates 0.01 inch (0.254 mm) tips. The
  receiver will send tip counts; the bucket size will be a **server setting**,
  so changing it does not require reflashing.

## 3. What the reference database tells us

`weewx-latest.sdb` contains 303,559 rows in `archive`:

- `usUnits = 1` (US customary) for every row;
- `interval = 5` minutes for 303,421 rows and 30 minutes for 138 older rows;
- `dateTime` is a Unix epoch, `PRIMARY KEY`, and represents the end of the
  archive interval.

The populated columns are:

```text
altimeter, appTemp, barometer, cloudbase, consBatteryVoltage, dewpoint, ET,
heatindex, humidex, inDewpoint, inHumidity, inTemp, maxSolarRad, outHumidity,
outTemp, pressure, radiation, rain, rainRate, rxCheckPercent, txBatteryStatus,
UV, windchill, windDir, windGust, windGustDir, windrun, windSpeed
```

The remaining columns exist in the schema but are null for this station. They
represent console, extra-sensor, soil/leaf, lightning, air-quality, snow/hail
and similar data that the ISS RF stream does not provide.

### 3.1 Findings from the data

| Finding | Evidence | Consequence |
| --- | --- | --- |
| The database is US customary | `usUnits = 1`; `outTemp` 48.9-99.9, `pressure` 26.46-26.97 inHg, `rain` 0-0.5 in, `windSpeed` 0-20 mph | Define explicit SI-to-US conversion; do not mix units in one row |
| `pressure` is station pressure, not sea-level | `pressure` ~26.9 inHg while `barometer` ~29.8 inHg | Use BME280 station pressure for `pressure`; derive `barometer`/`altimeter` with site altitude |
| Site altitude is around 910-940 m | `cloudbase` and `altimeter` are consistent with about 910 m; the configured default is 940 m; the bench BME280 reads about 910.7 hPa | Use the configured 940 m default and validate the derived pressure fields against a reference |
| Rain tips are 0.01 inch (0.254 mm) | 1,997 of 2,000 positive `rain` values are exact multiples of 0.01 in; the remainder are 0.2 mm values | Make the rain bucket size configurable; do not use the decoder's hardcoded 0.2 mm |
| `rainRate` is a 15-minute trailing rate | Recomputing `sum(rain over previous 900 s) * 4` matches `rainRate` on 295,594 of 303,559 rows; the non-sentinel differences have a median of about 0.04 in/h, and 648 rows are the 655.35 in/h sentinel | Implement Weewx-style `RainRater`, not raw message `0x5` |
| `windrun` is average wind speed x interval | `windSpeed * interval / 60` reproduces the stored miles | Compute from the 5-minute mean wind speed |
| Archive values are interval aggregates | `outTemp`, `outHumidity`, `windSpeed`, `radiation` and `UV` are fractional, not raw integer samples | Use means, not the last frame, for scalar fields |
| Weewx formulas are usable as test vectors | Spot-checked recent rows: dewpoint, windchill, heat index, apparent temperature and windrun match exactly; humidex is within about 0.02 F | Port the Weewx formulas and test against these rows |
| `barometer` is not the simple Weewx sea-level formula at 910 m | A standard reduction at 910 m gives about 29.84 inHg, while the stored value is about 29.77 inHg | Treat the console's `barometer` as a console-specific reduction; document any small difference |
| The database contains bad sentinels | 648 rows have `rainRate` = 655.35 in/h (about 302 rows have real rates above 5 in/h) | Reject `0xFFFF`-style sentinels; store `null`, not a huge number |

The database has no raw 3-second frames and no station metadata (altitude,
latitude, longitude, timezone, sensor inventory). Those must come from the
owner or from the receiver configuration.

## 4. Target 5-minute record

### 4.1 Bucket rules

- Bucket length: 300 seconds, aligned to wall-clock 5-minute boundaries.
- `dateTime`: the **end** of the bucket, as a Unix epoch in UTC. This matches
  Weewx.
- `interval`: 5 (minutes).
- Units marker: `SI` (or `usUnits = 17` if Weewx compatibility is retained).
  The primary archive is SI only; US units are a validation/export concern.
- A bucket with no valid frame is skipped, not written as all zeros. Log the
  gap and the reason.
- Do not create or upload a record when the clock is invalid. This is the
  existing project rule and it also prevents wrong daily files and duplicate
  daily rain totals.
- If the clock becomes invalid during a bucket, discard the bucket or mark it
  incomplete; do not stamp it with a guessed time.
- `message_id` is the idempotency key for the 5-minute record, for example
  `<device-id>-<bucket-end-epoch>-<counter>`. Reuse it on every retry.
- Recommended uniqueness in the server: `UNIQUE(device_id, dateTime)` in
  addition to the existing `(device_id, message_id)`.

### 4.2 Aggregation rules

| Field | Receiver aggregation over 5 minutes | Server finalisation |
| --- | --- | --- |
| wind speed | sum and count of every valid frame | mean = sum / count |
| wind direction | `sum(speed*cos(dir))`, `sum(speed*sin(dir))`, `sum(speed)`, count | vector mean with `atan2`; `null` if the bucket is calm |
| wind gust | maximum `0x9` value and the direction of that frame | keep max; `wind_gust_dir` is approximate because `0x9` has no dedicated gust direction |
| outside temperature | sum and count of valid `0x8` values | mean |
| outside humidity | sum and count of valid `0xA` values | mean |
| station pressure | sum and count of BME280 samples | mean |
| BME280 indoor temperature | sum and count | mean → `in_temp_c` |
| BME280 indoor humidity | sum and count | mean → `in_humidity_pct` |
| UV | sum and count of valid `0x4` values; max optional | mean |
| solar radiation | sum and count of valid `0x6` values; max optional | mean |
| rain | sum of positive `0xE` counter deltas in **tips** | convert tips to mm and inches with the configured bucket size |
| raw Davis rain rate | latest `0x5` raw value + high/low-rate flag | convert to mm/h and in/h |
| Weewx-style rain rate | no receiver work; derived from stored rain rows | trailing 15-minute `rain_mm * 4` |
| battery low | OR of all frames in the bucket | `battery_low` |
| supercap voltage | latest and minimum valid `0x2` value | store both |
| reception quality | `frames_expected`, `frames_valid`, `frames_crc_bad`, `resync_count`, `search_count`, `gap_slots`, `max_gap_slots` | `rx_check_percent` = valid / expected × 100 |

**Chosen approach: send sum + count, divide on the server.** A mean is the right
default for temperature, humidity, pressure, wind speed, UV and radiation over
a 5-minute archive interval. The receiver keeps a running `sum` and `count`
rather than dividing. Division on the ESP32 would be fast enough, but doing it
on the server is better for one implementation, one set of tests and the
ability to change the aggregation rule without reflashing. Sending `sum` and
`count` instead of a mean adds only a few bytes per field per record — at a
5-minute cadence this is on the order of a few MB per year at most, so traffic
is not a reason to compromise the design. If a field is preferred as "latest"
instead, the receiver only needs to keep the last valid value; no sum/count is
needed.

The recommended defaults are:

- **mean:** temperature, humidity, pressure, indoor temperature/humidity, wind
  speed, UV, solar radiation;
- **vector mean:** wind direction;
- **maximum:** wind gust;
- **accumulation:** rain tips;
- **latest:** raw Davis rain rate, battery-low state, supercap voltage;
- **minimum:** supercap voltage.

The receiver should not divide, convert to US units, or calculate derived
weather values. It accumulates sums/counts, maxima, latest values and the rain
tip delta. The server performs the final means and all conversions.

If a field has no valid sample in the bucket, store `null`. Do not substitute
zero. Zero is a valid measurement for wind speed, rain, rain rate, UV at night
and radiation at night.

Time-weighted means are preferable when frames are missing. At the normal 3 s
cadence a plain arithmetic mean is equivalent; if there are long gaps, weight
each sample by the time since the previous valid sample, capped at a few slots.
For the first implementation, a plain mean is acceptable if the gap counters are
stored.

### 4.3 Rain accumulation details

The `0xE` field is a 7-bit rolling tip counter (0-127). The collector must:

1. keep the previous counter value across buckets and across reboots;
2. add a delta only when the new value is greater than the previous value;
3. on wrap, use `new + 128 - old`;
4. on a reset or a large unexplained jump, resync without adding rain and set a
   quality flag;
5. send the **tip delta**, not a distance. The server converts it to mm and
   inches using the configured bucket size;
6. keep the daily total on the server from the stored per-bucket rain, using
   the site timezone (Asia/Kolkata) for the midnight boundary.

The raw `0x5` rain-rate message is **not** an accumulation. It is the
instantaneous rate implied by the time between the most recent tips (the
receiver decodes a click interval). The archive stores it as
`rain_rate_instant_mm_h` / `rain_rate_instant_in_h` and stores the Weewx-style
trailing rate separately as `rain_rate_15m_mm_h` / `rain_rate_15m_in_h`.

## 5. Units and conversions

The primary archive is SI. The reference database is US and is used only for
validation and comparison. The contract is:

- **wire and internal 5-minute record:** SI;
- **SQLite:** SI only (C, hPa, m/s, mm, mm/h, W/m2, UV index, degrees);
- **Grafana:** reads SI directly or applies the small display conversions listed
  in section 5.1;
- **US compatibility:** an optional view/export can reproduce the old Weewx
  units for comparison; it is not the primary storage.

| Quantity | SI / wire | US archive | Conversion |
| --- | --- | --- | --- |
| temperature | C | F | `F = C * 9/5 + 32` |
| temperature difference | C | F | `F = C * 9/5` |
| pressure | hPa | inHg | `inHg = hPa * 0.0295299830714` |
| wind speed / gust | m/s | mph | `mph = m/s / 0.44704` |
| wind direction | degrees | degrees | none |
| rain / rain rate | mm, mm/h | in, in/h | `in = mm / 25.4` |
| radiation | W/m2 | W/m2 | none |
| UV | index | index | none |
| altitude / cloud base | m | ft | `ft = m / 0.3048` |
| wind run | km | miles | `miles = km / 1.609344` |
| ET | mm | in | `in = mm / 25.4` |
| battery status | boolean | 0/1 | none |

For `METRICWX`, Weewx uses C, hPa (mbar), m/s, mm, mm/h, km and m. This is
exactly the canonical SI set already used by the protocol, so SI storage needs
no conversion at ingest. The US view is only for comparison/export.

### 5.1 Grafana-facing units

| Grafana value | SQLite source | Conversion in SQL/Grafana |
| --- | --- | --- |
| Temperature, dewpoint, wind chill, heat index | `out_temp_c`, `dewpoint_c`, `windchill_c`, `heatindex_c` | none, already C |
| Pressure | `barometer_hpa` (or `pressure_hpa` for station pressure) | none, already hPa |
| Solar radiation | `solar_wm2` | none |
| UV index | `uv_index` | none |
| Wind speed, gust | `wind_speed_m_s`, `wind_gust_m_s` | `* 3.6` for km/h, or use the km/h view |
| Wind direction | `wind_dir_deg`, `wind_gust_dir_deg` | none |
| Rain / rain rate | `rain_mm`, `rain_rate_15m_mm_h` | none, already mm / mm/h |

The old Weewx values for wind speed are **mph** when `usUnits = 1`, not km/h.
Check the existing Grafana panel before assuming "as is" is km/h. Wind chill and
heat index in the new archive are **C**, while the Weewx source was F.

## 6. Field-by-field archive mapping

### 6.1 Measured or directly derived from the ISS

| SQLite field | Source | Unit | Aggregation |
| --- | --- | --- | --- |
| `date_time` | bucket end | epoch UTC | end of 5-minute bucket |
| `interval_minutes` | config | minutes | 5 |
| `out_temp_c` | `0x8` | C | mean |
| `out_humidity_pct` | `0xA` | % | mean |
| `wind_speed_m_s` | every frame | m/s | mean |
| `wind_dir_deg` | every frame | degrees | speed-weighted vector mean |
| `wind_gust_m_s` | `0x9` | m/s | max |
| `wind_gust_dir_deg` | wind direction at max gust | degrees | latest at max gust (approximate) |
| `rain_tips` | `0xE` | tips | sum of counter deltas |
| `rain_mm` | derived from `rain_tips` | mm | tips × bucket_mm |
| `rain_in` | derived from `rain_tips` | inches | tips × bucket_in |
| `rain_rate_instant_mm_h` | `0x5` | mm/h | latest raw value converted with bucket size |
| `rain_rate_instant_in_h` | `0x5` | in/h | latest raw value converted with bucket size |
| `rain_rate_15m_mm_h` | derived from `rain_mm` | mm/h | trailing 15 min × 4 |
| `rain_rate_15m_in_h` | derived from `rain_in` | in/h | trailing 15 min × 4 |
| `solar_wm2` | `0x6` (assumed) | W/m2 | mean |
| `uv_index` | `0x4` | index | mean |
| `pressure_hpa` | BME280 | hPa | mean station pressure |
| `in_temp_c` | BME280 | C | mean |
| `in_humidity_pct` | BME280 | % | mean |
| `out_temp_count` | receiver | count | valid `0x8` samples used in the mean |
| `out_humidity_count` | receiver | count | valid `0xA` samples used in the mean |
| `wind_speed_count` | receiver | count | valid frames used in the mean |
| `wind_dir_count` | receiver | count | frames used in the vector mean |
| `wind_gust_count` | receiver | count | valid `0x9` samples |
| `rain_count` | receiver | count | valid `0xE` counter frames |
| `solar_count` | receiver | count | valid `0x6` samples |
| `uv_count` | receiver | count | valid `0x4` samples |
| `pressure_count` | receiver | count | BME280 pressure samples |
| `in_temp_count` | receiver | count | BME280 temperature samples |
| `in_humidity_count` | receiver | count | BME280 humidity samples |
| `supercap_count` | receiver | count | valid `0x2` samples |
| `battery_low` | byte 0 bit 3 | boolean | OR |
| `supercap_v` | `0x2` | V | latest |
| `supercap_min_v` | `0x2` | V | minimum |
| `frames_expected` | receiver | count | expected slots |
| `frames_valid` | receiver | count | valid CRC frames |
| `frames_crc_bad` | receiver | count | bad CRC frames |
| `resync_count` | receiver | count | times silence after lock exceeded the resync threshold |
| `search_count` | receiver | count | times the receiver entered reverse-scan/search mode |
| `gap_slots` | receiver | count | total missed slots inferred from sequence gaps |
| `max_gap_slots` | receiver | count | longest gap in the bucket |
| `rssi_avg_dbm` | receiver | dBm | mean |
| `rssi_min_dbm` | receiver | dBm | minimum |
| `rssi_max_dbm` | receiver | dBm | maximum |

### 6.2 Derived fields

| SQLite field | Inputs | Formula / Weewx equivalent | Priority |
| --- | --- | --- | --- |
| `dewpoint_c` | `out_temp_c`, `out_humidity_pct` | `dewpointC` (Magnus) | required |
| `windchill_c` | `out_temp_c`, `wind_speed_m_s` | `windchillMetricWX`; returns air temperature when T >= 10 C or wind <= 3 mph | required |
| `heatindex_c` | `out_temp_c`, `out_humidity_pct` | `heatindexC` (Weewx "new" algorithm) | required |
| `humidex_c` | `out_temp_c`, `out_humidity_pct` | `humidexC` | required |
| `app_temp_c` | `out_temp_c`, `out_humidity_pct`, `wind_speed_m_s` | `apptempC` (`T + 0.33e - 0.70v - 4.00`) | required |
| `windrun_km` | `wind_speed_m_s`, interval | `wind_speed_m_s * interval_minutes * 60 / 1000` | required |
| `barometer_hpa` | `pressure_hpa`, site altitude, `out_temp_c` | `sealevel_pressure_Metric` | required |
| `altimeter_hpa` | `pressure_hpa`, site altitude | `altimeter_pressure_Metric`, ASOS algorithm | required |
| `cloudbase_m` | `out_temp_c`, `out_humidity_pct`, site altitude | `cloudbase_Metric` | optional |
| `max_solar_wm2` | lat, lon, altitude, bucket time | `solar_rad_RS` (default, atc 0.8) or `solar_rad_Bras` | optional |
| `et_mm` | T min/max, RH min/max, avg radiation, avg wind, site metadata | `evapotranspiration_Metric` over a 1-hour period; default albedo 0.23, cn 37, cd 0.34, wind height 2 m | optional, needs radiation |
| `in_dewpoint_c` | `in_temp_c`, `in_humidity_pct` | `dewpointC` | required with BME280 |

**What ET and maxSolarRad mean:**

- `max_solar_wm2` is the **theoretical clear-sky maximum** solar radiation for
  the site, date and time, calculated from latitude, longitude, altitude and
  solar position. It is not a measurement and does not use the solar sensor.
- `et_mm` is **reference evapotranspiration**: an estimate of water loss from a
  reference grass surface, calculated from temperature, humidity, wind and
  solar radiation over a period (Weewx default: one hour). It needs the solar
  sensor and a full hour of data. It can be added later without changing the
  measured fields.

Formula source: `weewx.wxformulas` and `weewx.uwxutils`, as used by Weewx 5.x.
The formulas should be ported with tests rather than re-derived.

### 6.3 Not available from this station

These columns exist in the reference schema but have no source in the ISS RF
stream and should stay `null` unless extra hardware is added:

| Group | Columns | Why |
| --- | --- | --- |
| Console | `consBatteryVoltage`, `forecast` | These come from the Davis console, not the ISS. The BME280 now supplies `in_temp_c`, `in_humidity_pct` and the derived `in_dewpoint_c`, but it is not a console supply-voltage or forecast source. |
| Extra sensors | `extraTemp1..8`, `extraHumid1..8`, `soilTemp1..4`, `soilMoist1..4`, `leafTemp1..2`, `leafWet1..2` | No extra transmitters/sensors in this stream. |
| Battery detail | `batteryStatus1..8`, `outTempBatteryStatus`, `windBatteryStatus`, `rainBatteryStatus`, `uvBatteryStatus` | The ISS gives one low-battery bit per frame, not per-sensor status. `txBatteryStatus` is the closest aggregate; do not invent separate bits. |
| Console/health | `rxCheckPercent` is derivable; `signal1..8` are not | Keep RSSI as metadata (`rssi_dbm`), not as Weewx `signal` unless the semantics are agreed. |
| Lightning / air quality | `lightning_*`, `co`, `co2`, `nh3`, `no2`, `o3`, `so2`, `pm1_0`, `pm2_5`, `pm10_0`, `noise`, `pb`, `luminosity` | No such sensors. `luminosity` is not the same as solar radiation. |
| Snow / hail / heating | `snow*`, `hail*`, `heatingTemp`, `heatingVoltage`, `referenceVoltage`, `supplyVoltage` | No source. `supplyVoltage` must not be silently used for the ISS supercap. |
| Supercap | no standard Weewx column | The `0x2` goldcap/supercap voltage is useful diagnostics. Store it as `supercap_v` (and `supercap_min_v`), not as `consBatteryVoltage`. |

### 6.4 Supercap naming

The current `DavisObservation` calls the `0x2` value `batteryVoltageV`. That is
misleading: `0x2` is the ISS goldcap/supercap charge status, not the battery
voltage and not the console supply voltage. Before archiving, rename or split
it into `supercap_v`/`supercap_min_v` (diagnostics) and keep the separate
`battery_low` flag from byte 0. Do not map it to `consBatteryVoltage`.

`0x2` is transmitted by the ISS once per message cycle. It is the charge state
of the ISS supercapacitor (the prototype calls it "goldcap charge status"),
decoded as a 10-bit value in volts. `supercap_v` is the latest valid reading in
the bucket; `supercap_min_v` is the lowest reading, which is useful for spotting
a failing capacitor or a long cloudy period. If the ISS model does not send
`0x2`, both fields are `null`.

## 7. Collation and derivation responsibilities

### 7.1 Recommended split

The receiver should own only the time-critical, bounded work:

- collect frames into a 5-minute bucket;
- accumulate sums/counts, maxima, latest values, gust maximum/direction and the
  rain tip delta;
- write one SI record to SD and enqueue it for upload;
- keep the previous rain counter and bucket state in NVS so a reboot does not
  lose the accumulation.

The server should own everything that can change or needs site metadata:

- validate and store the 5-minute record;
- divide sums by counts to produce the means;
- convert rain tips to mm and inches with the configured bucket size;
- convert the raw `0x5` rain rate to mm/h and in/h;
- calculate all derived fields and daily summaries;
- expose Grafana-friendly SI views and an optional US compatibility view.

This keeps the ESP32 code small and lets formula, unit, bucket-size or site
changes be deployed without reflashing. The server must store enough per-bucket
statistics to recompute ET later: at minimum outTemp min/max, outHumidity
min/max, mean radiation, mean wind and rain. If ET is not required initially,
those extra stats can wait, but they should not be lost from the raw data.

Computing derived fields on the device is not recommended for the first slice:
it would add site metadata and formula code to firmware and make formula changes
an OTA event.

### 7.2 Archive record shape

A first server table could look like this (names are illustrative; the values
and units are the contract):

```text
weather_archive
  id
  device_id
  date_time                 -- bucket end, epoch UTC
  interval_minutes          -- 5
  units                     -- 'SI'
  out_temp_c
  out_humidity_pct
  pressure_hpa              -- station pressure
  barometer_hpa             -- derived sea-level pressure
  altimeter_hpa             -- derived altimeter
  wind_speed_m_s
  wind_dir_deg
  wind_gust_m_s
  wind_gust_dir_deg
  rain_tips                 -- interval tip count
  rain_mm
  rain_in
  rain_rate_instant_mm_h    -- raw 0x5
  rain_rate_instant_in_h
  rain_rate_15m_mm_h        -- Weewx-style trailing rate
  rain_rate_15m_in_h
  solar_wm2
  uv_index
  dewpoint_c
  windchill_c
  heatindex_c
  humidex_c
  app_temp_c
  in_temp_c                 -- BME280
  in_humidity_pct           -- BME280
  in_dewpoint_c             -- derived
  out_temp_count
  out_humidity_count
  wind_speed_count
  wind_dir_count
  wind_gust_count
  rain_count
  solar_count
  uv_count
  pressure_count
  in_temp_count
  in_humidity_count
  supercap_count
  windrun_km
  cloudbase_m
  max_solar_wm2
  et_mm
  battery_low               -- boolean
  supercap_v
  supercap_min_v
  rx_check_percent
  frames_expected
  frames_valid
  frames_crc_bad
  resync_count
  search_count
  gap_slots
  max_gap_slots
  rssi_avg_dbm
  rssi_min_dbm
  rssi_max_dbm
  quality_flags             -- JSON or bitmask
  derived_version           -- formula version
```

Site metadata is server-side configuration, not a per-row field:

```text
site_latitude      13.4867 N
site_longitude     77.7064 E
site_altitude_m    940
site_timezone      Asia/Kolkata (UTC+05:30)
rain_bucket_mm     0.254 (pending confirmation)
```

The per-field `*_count` columns are deliberate. They preserve how many valid
samples contributed to each mean, which is useful in the first months to detect
packet loss, a missing message type, or a changed ISS cadence. They cost little
space and can be ignored by Grafana. If they later become noise, keep
`frames_valid`/`frames_expected` and drop the per-field counts.

A compatibility view can rename the fields to Weewx camelCase and convert to US
units for comparison with `weewx-latest.sdb`. It is not the primary storage.

### 7.3 Reception quality (`rxCheckPercent`)

Weewx's Davis driver derives `rxCheckPercent` from the number of wind samples
in the console archive record. We do not have that counter; we have frames. Use:

```text
expected_slots = bucket_seconds / nominal_slot_seconds
rxCheckPercent = 100 * valid_frames / expected_slots
```

For a 5-minute bucket at 3 s, `expected_slots = 100`. Keep the raw counts so the
definition can be changed later. If the real ISS cadence is not exactly 3 s,
make `nominal_slot_seconds` configurable or derive it from the observed
sequence. Do not silently compare against a hardcoded 100 if the cadence
changes.

`rx_check_percent` is therefore a reception-health metric, not a weather
measurement. It is close to Weewx's `rxCheckPercent`, but Weewx uses the console's
wind-sample count while we use received Davis frames.

The RF bench firmware already tracks:

- `RESYNC`: a silence longer than 6 s after a valid lock;
- `SEARCH`: after seven consecutive misses, the receiver switches to reverse
  scan to find the transmitter again;
- `gap_slots` / `max_gap`: missed slots inferred from the simulator sequence.

Those counters are currently in the RF test loop only. The archive collector
must carry the same counters into the 5-minute quality fields so packet loss,
resyncs and search events survive into SQLite. They are not the same as a
hardware reset: the MCU does not reset; the receiver re-acquires the hopping
sequence.

## 8. Data flow and storage changes to plan

This section is future work, not part of this task.

1. **Firmware collector**
   - Add a host-testable `ArchiveCollector` library.
   - Feed it `DavisObservation` plus RSSI and receive time.
   - Produce one `ArchiveRecord` per 5-minute bucket.
   - Persist rain counter and bucket state in NVS.
   - Write one JSON line per 5-minute record to SD (change from per-frame lines;
     optionally keep a separate raw-frame log for diagnosis).
   - Upload the 5-minute record with a stable `message_id`.
2. **Protocol**
   - Add an `archive` envelope/schema (or an additive `type`) with the receiver
     sums/counts, maxima, latest values and rain tip delta. This is a
     wire-format change: JSON Schema, Pydantic model, C++ constants, examples,
     tests and docs must change together.
3. **Server**
   - Add the `weather_archive` table and migration.
   - Divide sums by counts; convert tips/rates with the configured bucket size.
   - Validate SI ranges; reject sentinels; store missing as `NULL`.
   - Compute derived fields on ingest or in a service.
   - Add Grafana-friendly SI views and a US-unit compatibility view/export for
     comparison with `weewx-latest.sdb`.
   - Enforce `UNIQUE(device_id, date_time)` in addition to the existing
     idempotency key.
4. **Operations**
   - Update `docs/operations/archive.md` for the 5-minute record format.
   - Add server configuration for site latitude, longitude, altitude, timezone
     and rain bucket size, with the defaults above.

## 9. Validation plan

### 9.1 Formula test vectors from the reference database

Use rows from `weewx-latest.sdb` as known-good derived-value vectors:

- recompute dewpoint, windchill, heat index, humidex, apparent temperature and
  windrun from `outTemp`, `outHumidity`, `windSpeed` and `interval`;
- recompute `rainRate` from the previous 15 minutes of `rain`;
- recompute `cloudbase` and `altimeter` with the confirmed site altitude;
- compare with a documented tolerance (for example 0.1 C / 0.1 hPa / 0.25 mm).

This is already useful evidence: the spot check in this analysis matched
dewpoint, windchill, heat index, apparent temperature and windrun, and matched
cloudbase/altimeter at about 910 m. Re-run the tests with the configured 940 m
and record any difference; if the difference is systematic, adjust the site
altitude rather than the formulas.

### 9.2 Synthetic frame sequences

Use the simulator and captured frames to build deterministic sequences:

- a normal 5-minute run with every message type;
- missing temperature, humidity, UV and solar frames;
- a CRC-bad frame (must not contribute to any average);
- a rain counter wrap (127 -> 0) and a counter reset;
- a mid-bucket reboot (no double counting);
- a bucket with no frames (skipped);
- a wind direction sequence crossing 0/360 degrees (vector mean must not give
  180 degrees);
- gust direction when the maximum gust occurs in a different frame from the
  maximum wind speed.

### 9.3 Acceptance criteria for the implementation slice

- A 5-minute record is produced from at least 10 minutes of bench RF data with
  the correct bucket end time and `interval = 5`.
- Every populated field has a documented source, unit and aggregation rule.
- Missing sensor fields are `null`, not zero.
- Rain accumulation over a known sequence equals tips x bucket size, including
  a wrap.
- Derived values match the reference formulas/test vectors within tolerance.
- The record is idempotent: re-uploading the same `message_id` creates no
  duplicate.
- No record is archived or uploaded with an invalid clock.
- A reboot does not lose or duplicate the rain counter or the current bucket.
- The US compatibility view reproduces the reference column names and units.

## 10. Resolved decisions and remaining confirmations

Resolved on 2026-11-01:

- SI-only archive; server-side conversions and derived values.
- BME280 station pressure; BME280 temperature/humidity as indoor values.
- Rain stored in both mm and inches; tip counts sent by the receiver.
- Both rain-rate definitions stored.
- UV and solar sensors present; `0x6` assumed for solar until confirmed.
- Site defaults: 13.4867 N, 77.7064 E, 940 m, Asia/Kolkata.
- ET and maxSolarRad deferred; `maxSolarRad` is theoretical, ET needs a
  one-hour radiation window.

Still to confirm:

| # | Item | Why |
| --- | --- | --- |
| 1 | Rain bucket size: 0.01 in (0.254 mm) or 0.2 mm | Reference data says 0.01 in, but the actual ISS collector must be confirmed. |
| 2 | Solar message `0x6` vs `0x7` | The formulas differ; `0x6` is the working assumption. |
| 3 | Wind direction mapping: full-circle vs 9-351 degree dead zone | Affects all archived directions. |
| 4 | Negative-temperature encoding below 0 F | Current decoder treats the field as unsigned. |
| 5 | Whether the BME280 enclosure is vented and calibrated for station pressure | Pressure and barometer depend on it. |
| 6 | Whether to retain a raw-frame log on SD in addition to the 5-minute record | Useful for diagnosis, but not required for the archive. |

## 11. Implementation status (2026-11-01)

Implemented:

- `AkashvaniArchive` collector: five-minute sums/counts, vector wind sums,
  gust maximum/direction, rain tip delta, supercap latest/minimum and quality
  counters. Host tests cover aggregation, rain wrap, vector direction and
  BME-only buckets.
- Davis decoder now exposes raw rain counter, raw `0x5` rate and high/low flag.
- Receiver production path: RF hopping/search (fixed to ignore bad-CRC noise),
  BME280 pressure/indoor sampling, 5-minute record serialization, SD archive and
  archive upload.
- Shared bearer token on the hotspot page (`api_token`, default `secret`),
  stored in NVS and used by `POST /api/v1/archive`.
- Server `archive` table and Alembic migration, archive ingest API with sums/counts
  and derived calculations, admin archive query.
- Protocol schema and example: `protocol/schema/archive.schema.json`,
  `protocol/examples/archive.json`.
- Server tests for ingest, idempotency, missing sensors, token rejection and
  protocol schema validation.

Verified server-side with two 5-minute records: the SQLite `archive` table
contains means, mm/in rain, both rain-rate definitions, derived values and
quality counters.

Hardware bench status: a 12-minute receiver + simulator run completed on
2026-11-01. The receiver produced archive buckets at 16:25, 16:30 and 16:35;
the last two were 100/100 valid frames. Three real receiver records are in
`server/data/akashvani.db` with temperature, humidity, pressure/barometer,
wind, UV, solar, rain fields, derived values and quality counters.

Two RF bugs found and fixed during the run:

- a bad-CRC noise packet was treated as a lock and advanced the channel; bad
  packets are now ignored for lock purposes;
- the RFM69 FIFO/status was not reset between packets, so stale bytes were
  sometimes read as the next packet; `restartRx` now writes FIFO-overrun-clear.

The earlier RF failure was resolved by the explicit RESET pulse and FIFO clear
in the driver; both boards are still sensitive to the USB1 no-RESET wiring, so
power-cycle USB1 before a long RF run.

## 12. Proposed implementation slices (after review)

1. **Collector core (host-testable, no hardware)**
   - `ArchiveCollector`, `ArchiveRecord`, aggregation and quality counters.
   - Unit tests with captured/synthetic frames.
2. **Derived calculations and test vectors**
   - Port the Weewx formulas used above.
   - Add the reference-database tests and tolerance checks.
3. **Server archive schema and endpoint**
   - Migration, Pydantic model, idempotent ingest, query and US compatibility
     view.
4. **Firmware integration**
   - Production receive/hop path (from the RF slice) plus the collector.
   - SD 5-minute records, upload queue, NVS rain state, clock rules.
5. **Bench validation**
   - Receiver + simulator run, 5-minute records, rain wrap/reboot tests, SD
     sample, server rows, comparison with the reference formulas.

## 13. References

- `firmware/libraries/AkashvaniDavis/src/DavisDecoder.cpp`
- `firmware/libraries/AkashvaniDavis/src/DavisObservation.h`
- `firmware/tests/native/vectors/davis_iss_frames.txt`
- `docs/specs/rf-davis-receive.md`
- `docs/operations/archive.md`
- `protocol/schema/observation.schema.json`
- `weewx-latest.sdb`, table `archive`
- Weewx `wxformulas.py`, `wxxtypes.py`, `units.py`, `uwxutils.py`
