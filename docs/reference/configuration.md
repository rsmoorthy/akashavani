# Configuration reference

## Firmware build variables

| Variable | Default | Purpose |
| --- | --- | --- |
| `AKASHVANI_FLASH_SIZE` | `4M` | `16M` selects the 16 MB partition scheme |
| `AKASHVANI_FQBN` | unset | full FQBN override; wins over flash size |
| `AKASHVANI_FQBN_4MB` | `esp32:esp32:esp32:...min_spiffs` | replace the 4 MB default |
| `AKASHVANI_FQBN_16MB` | `esp32:esp32:esp32:...app3M_fat9M_16MB` | replace the 16 MB default |
| `AKASHVANI_ESP32_CORE` | `3.0.2` | core version installed by `fw-setup` |
| `AKASHVANI_HOST` / `PORT` | `127.0.0.1` / `8000` | local server binding |

The default target is the ESP32 classic bench board. The receiver sketch carries
`partitions.csv` with two 1.875 MiB OTA slots; see
[Firmware build](../build/firmware.md).

`firmware/VERSION` is the source of the firmware version used by the build and
OTA tooling.

## Receiver `config.h`

| Constant | Default | Notes |
| --- | --- | --- |
| `kSerialBaud` | 115200 | |
| `kSpiSckPin` / `kSpiMisoPin` / `kSpiMosiPin` | 18 / 19 / 23 | observed VSPI bench wiring |
| `kRadioCsPin` / `kRadioResetPin` / `kRadioDio0Pin` | 5 / 17 / 13 | USB1 RESET is not connected; harmless |
| `kRadioFrequencyMhz` | 915.0 | legal band/power is site-specific |
| `kRadioBitrateKbps` / `kRadioDeviationKhz` / `kRadioBandwidthKhz` | 4.8 / 5 / 125 | placeholders until ported |
| `kSdCsPin` | 16 | observed bench value |
| `kI2cSdaPin` / `kI2cSclPin` | 21 / 22 | observed bench value |
| `kDisplayAddress` | `0x3C` | SSD1306-class 128x64 I²C status panel on the shared bus |
| `kDisplayCsPin` / `kDisplayDcPin` / `kDisplayResetPin` | 8 / 7 / 6 | unused ST7735 seam; the bench panel is I²C |
| `kStatusLedPin` | 2 | |
| `kApSsidPrefix` | `akashvani-` | last MAC byte is appended, e.g. `akashvani-e0` |
| `kApPassword` | `doordarshan` | WPA2 password for the provisioning AP |
| `kApChannel` | 6 | AP channel; STA may override when both are active |
| `kApMaxClients` | 4 | simultaneous AP clients |
| `kApIp` / `kApGateway` / `kApSubnet` | `10.90.0.1` / `10.90.0.1` / `255.255.255.0` | AP interface |
| `kApDhcpFirst` / `kApDhcpLast` | `10.90.0.11` / `10.90.0.20` | DHCP pool handed to AP clients |
| `kDeviceToken` | from `secrets.h` | git-ignored development token |
| `kWifiSsid` / `kWifiPassword` | from `secrets.h` | git-ignored development credentials |
| `kServerBaseUrl` | `http://192.168.29.31:8000` | bench server |
| `kCommandPollIntervalSeconds` | 30 | command/health lease interval |
| `kEnableSdArchive` / `kEnableRtc` / `kEnableDisplay` / `kEnableLanOta` | true/true/true/true | |
| `kRadioReceiveTimeoutMs` | 100 | loop latency budget |
| `kMaxPendingUploads` | 8 | RAM queue depth |
| `kMaxUploadAttempts` | 8 | then drop (archive remains) |
| `kUploadRetryBaseMs` / `kUploadRetryMaxMs` | 5000 / 300000 | exponential backoff |

Copy `firmware/sketches/receiver/secrets.example.h` to `secrets.h` and fill in
the Wi-Fi password and device token. `secrets.h` is git-ignored.

The bench boards are ESP32-D0WD-V3 and use the pins above. See
[Pin map](../hardware/pinmap.md#observed-bench-wiring-2026-10-26).

## NVS settings

Namespace `akashvani`, written by the local AP page.

| Key | Default | Secret |
| --- | --- | --- |
| `dev_id` | MAC-derived `ak-xxxxxx` | no |
| `token` | empty | yes |
| `api_token` | `secret` | yes |
| `server` | `http://192.168.1.100:8000` | no |
| `wifi_ssid_0` … `wifi_ssid_5` | empty | no |
| `wifi_pass_0` … `wifi_pass_5` | empty | yes |
| `admin_pw` | `akashvani-admin` | yes (ArduinoOTA only) |
| `upload` / `archive` | true / true | no |
| `upl_int` / `cmd_int` | 60 / 30 seconds | no |
| `configured` | set by the first save | no |

The AP password and DHCP range are compiled in, not stored in NVS.

On a blank device (no `configured` marker and no legacy keys) the receiver seeds
the MAC-derived device ID, the bench server URL, the git-ignored `secrets.h`
credentials and a 30-second command poll from `config.h`. Once anything is
saved through the AP page, those seed values are not applied again on later
boots. Legacy keys `wifi_ssid` / `wifi_pass` from earlier firmware are migrated
into profile 0.

The OTA staged release is stored in NVS namespace `ota_meta` under
`staged_version` until `boot_with_new_fw` consumes it.

## Local AP page

| Setting | Value |
| --- | --- |
| AP SSID | `akashvani-XX`, where `XX` is the last MAC byte in hex |
| AP password | `doordarshan` (`kApPassword`) |
| AP address | `http://10.90.0.1/` |
| DHCP pool | `10.90.0.11` – `10.90.0.20` |
| Status refresh | 5 seconds |

The page has no separate admin login; the AP WPA2 password is the boundary. It
shows time, firmware, STA connection/SSID/IP, AP state, radio/SD flags, queue
depth and the last weather frame when one exists. The form saves the server URL,
device token and up to six site Wi-Fi profiles together. Passwords are hidden
until "Show passwords" is pressed. A Wi-Fi change restarts the board after the
response is sent; server URL and token changes apply immediately.

## Server environment

All variables use the `AKASHVANI_` prefix. Values can also be placed in
`server/.env`.

| Variable | Default | Purpose |
| --- | --- | --- |
| `AKASHVANI_ENVIRONMENT` | `development` | `test` auto-creates tables |
| `AKASHVANI_DEBUG` | `false` | |
| `AKASHVANI_LOG_LEVEL` | `INFO` | |
| `AKASHVANI_DATABASE_URL` | `sqlite:///./data/akashvani.db` | SQLAlchemy URL |
| `AKASHVANI_DATA_DIR` | `./data` | runtime directory |
| `AKASHVANI_FIRMWARE_DIR` | `./data/firmware` | OTA image directory |
| `AKASHVANI_DEVICE_SEEDS` | `sim-001:dev-sim-token:Simulator,station-1:dev-station-1-token:Bench station 1` | comma-separated seeds |
| `AKASHVANI_ADMIN_TOKEN` | `dev-admin-token` | admin bearer token |
| `AKASHVANI_API_TOKEN` | `secret` | shared archive bearer token; also accepted by the admin API |
| `AKASHVANI_SITE_LATITUDE` | `13.4867` | site latitude |
| `AKASHVANI_SITE_LONGITUDE` | `77.7064` | site longitude |
| `AKASHVANI_SITE_ALTITUDE_M` | `940` | site altitude in metres |
| `AKASHVANI_SITE_TIMEZONE` | `Asia/Kolkata` | site timezone |
| `AKASHVANI_RAIN_BUCKET_MM` | `0.254` | rain collector tip size (0.01 in) |
| `AKASHVANI_MAX_BODY_BYTES` | 65536 | ingest limit |
| `AKASHVANI_MAX_FIRMWARE_BYTES` | 8388608 | admin upload limit |
| `AKASHVANI_MAX_FUTURE_SKEW_SECONDS` | 300 | timestamp tolerance |
| `AKASHVANI_COMMAND_DEFAULT_TTL_SECONDS` | 3600 | default command expiry |
| `AKASHVANI_CORS_ORIGINS` | empty | comma-separated allowed origins |

Production must override the admin token and device seeds; defaults are for
local development only.

## HTTP endpoints

See [Protocol](../protocol/README.md) and the generated OpenAPI page at `/docs`.
The admin API is under `/api/v1/admin/*`; device API is under
`/api/v1/devices/me/*` and `/api/v1/observations`.

## Partition schemes

| Flash | Scheme | OTA slots |
| --- | --- | --- |
| 4 MB | receiver `partitions.csv` / `min_spiffs` | 1,966,080 B (1.875 MiB) x2 |
| 4 MB | `no_fs` | 2.0 MB x2 |
| 16 MB | `app3M_fat9M_16MB` | 3 MB x2 |

See `firmware/partitions/README.md` for custom tables.
