# Device configuration exchange and admin mirror

**Status:** agreed, in progress (2026-11-12)
**Scope:** device-owned configuration, server mirror, `get_config` / `push_config`
commands, site settings in the admin UI.

## Goal

Every device owns its site configuration (AP page, NVS). The server keeps a
mirror so `/admin` can display it, edit it and push it back. The server never
invents a site-wide location, time zone or altitude.

Credentials that bootstrap the device/server relationship stay local to the
device and are **not** part of this exchange:

- server URL
- server API token
- device token

## Configuration document

One JSON object, versioned:

```json
{
  "version": 1,
  "timezone": "Asia/Kolkata",
  "site": { "latitude": 13.4867, "longitude": 77.7064, "altitude_m": 940.0 },
  "wifi": [{ "ssid": "site-net", "password": "secret123" }],
  "admin_password": "akashavani-admin",
  "upload_enabled": true,
  "archive_enabled": true,
  "upload_interval_seconds": 60,
  "command_poll_interval_seconds": 30
}
```

- `site` is omitted when the device has no coordinates yet.
- `wifi` has at most six profiles; empty SSIDs are not sent.
- `version` is the document version. Unknown versions are rejected.

## Device → server

`PUT /api/v1/devices/me/config` with the document above.

Sent:

- at boot once STA is connected;
- after an AP-page save (config version changes);
- daily as a safety net.

The server stores the document, extracts `timezone`, `latitude`, `longitude`
and `altitude_m`, and increments its `config_version` counter.

## Server → device

`push_config` command carries the configuration document as its payload. The
device validates it, writes it to NVS, applies it, acks, and reboots when the
Wi-Fi profile list changed. The AP is unavailable during the reboot.

`get_config` command asks the device to call `PUT /devices/me/config`
immediately. The ack is `{"reported": true}`; the document itself travels on the
`PUT`, not in the ack.

Both commands are manual `/admin` actions. There is no scheduler and no
command-based polling.

## Admin mirror and sync rules

- The server stores the last reported or saved document in `devices.config`.
- `GET /api/v1/admin/devices/{id}/config` returns the server copy.
- `PUT /api/v1/admin/devices/{id}/config` saves admin edits to the server copy
  only. Nothing reaches the device until `push_config`.
- `push_config` uses the saved server copy (and refreshes it from the request,
  if a payload is supplied).
- `get_config` causes the device copy to overwrite the server copy. Admin edits
  that were saved but not pushed are lost. The admin UI warns before queueing
  `get_config`.

## Archive derivation

`barometer_hpa`, `altimeter_hpa` and `cloudbase_m` need the device altitude.
Until a device has reported one, those columns are `NULL` and the record's
`quality_flags` contains `missing_site_altitude`. There is no server-wide
fallback.

## Acceptance criteria

- AP page edits latitude, longitude and altitude; values survive reboot.
- Boot/AP-save/daily config reports match the NVS values.
- `get_config` overwrites the admin copy with the device copy.
- `push_config` applies timezone/site/Wi-Fi/interval changes on the device;
  Wi-Fi changes reboot the board.
- `get_fw_version` is gone; `get_config` replaces it.
- Admin site-settings tab is usable on a phone-width viewport.
- Server tests cover config report, admin save, push and get, and the archive
  derived-value behaviour with and without altitude.
