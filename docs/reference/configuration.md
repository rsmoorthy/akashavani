# Configuration reference

## Firmware build variables

| Variable | Default | Purpose |
| --- | --- | --- |
| `AKASHVANI_FLASH_SIZE` | `4M` | `16M` selects the 16 MB partition scheme |
| `AKASHVANI_FQBN` | unset | full FQBN override; wins over flash size |
| `AKASHVANI_FQBN_4MB` | `...min_spiffs...` | replace the 4 MB default |
| `AKASHVANI_FQBN_16MB` | `...app3M_fat9M_16MB...` | replace the 16 MB default |
| `AKASHVANI_ESP32_CORE` | `3.0.2` | core version installed by `fw-setup` |
| `AKASHVANI_HOST` / `PORT` | `127.0.0.1` / `8000` | local server binding |

`firmware/VERSION` is the source of the firmware version used by the build and
OTA tooling.

## Receiver `config.h`

| Constant | Default | Notes |
| --- | --- | --- |
| `kSerialBaud` | 115200 | |
| `kSpiSckPin` / `kSpiMisoPin` / `kSpiMosiPin` | 12 / 13 / 11 | shared bus |
| `kRadioCsPin` / `kRadioResetPin` / `kRadioDio0Pin` | 10 / 16 / 15 | RFM95 |
| `kRadioFrequencyMhz` | 915.0 | legal band/power is site-specific |
| `kRadioBitrateKbps` / `kRadioDeviationKhz` / `kRadioBandwidthKhz` | 4.8 / 5 / 125 | placeholders until ported |
| `kSdCsPin` | 9 | |
| `kI2cSdaPin` / `kI2cSclPin` | 4 / 5 | DS3231 |
| `kDisplayCsPin` / `kDisplayDcPin` / `kDisplayResetPin` | 8 / 7 / 6 | reserved |
| `kStatusLedPin` | 2 | |
| `kEnableSdArchive` / `kEnableRtc` / `kEnableDisplay` / `kEnableLanOta` | true/true/false/true | |
| `kRadioReceiveTimeoutMs` | 100 | loop latency budget |
| `kMaxPendingUploads` | 8 | RAM queue depth |
| `kMaxUploadAttempts` | 8 | then drop (archive remains) |
| `kUploadRetryBaseMs` / `kUploadRetryMaxMs` | 5000 / 300000 | exponential backoff |

The transmitter simulator has its own `config.h`: pins, transmitter ID, seed,
interval and `kDryRun`.

## NVS settings

Namespace `akashvani`, written by the local admin page.

| Key | Default | Secret |
| --- | --- | --- |
| `dev_id` | MAC-derived `ak-xxxxxx` | no |
| `token` | empty | yes |
| `server` | `http://192.168.1.100:8000` | no |
| `wifi_ssid` | empty | no |
| `wifi_pass` | empty | yes |
| `ap_pass` | `akashvani-setup` | yes |
| `admin_pw` | `akashvani-admin` | yes |
| `upload` / `archive` | true / true | no |
| `upl_int` / `cmd_int` | 60 / 60 seconds | no |

The AP SSID is generated as `Akashvani-<device-id>` and is not stored.

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
| `AKASHVANI_DEVICE_SEEDS` | `sim-001:dev-sim-token:Simulator` | comma-separated seeds |
| `AKASHVANI_ADMIN_TOKEN` | `dev-admin-token` | admin bearer token |
| `AKASHVANI_MAX_BODY_BYTES` | 65536 | ingest limit |
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
| 4 MB | `min_spiffs` | 1.9 MB x2 |
| 4 MB | `no_fs` | 2.0 MB x2 |
| 16 MB | `app3M_fat9M_16MB` | 3 MB x2 |

See `firmware/partitions/README.md` for custom tables.
