# Provisioning a receiver

A new board always starts its own Wi-Fi AP, so provisioning does not depend on
the site network being correct.

## 1. Create the device on the server

The device ID is shown **on the board's own status page** (header line
`Device … · MAC …`) and in `GET /api/status` as `device_id`/`mac`, so no serial
console is needed in the field. A blank board derives it from the MAC as
`ak-xxxxxx`. The serial log prints it as `device=ak-xxxxxx` if a console is
available. Create a token for that ID:

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/devices \
  -H 'Authorization: Bearer dev-admin-token' \
  -H 'Content-Type: application/json' \
  -d '{"id":"ak-3f2a1b","name":"Roof receiver","token":"replace-with-a-long-random-token"}'
```

The server stores only the token hash. Record the plaintext token securely; it
cannot be recovered.

## 2. Connect to the board AP

| Setting | Default |
| --- | --- |
| SSID | `akashvani-XX` (`XX` = last MAC byte, e.g. `akashvani-e0`) |
| Password | `doordarshan` |
| Board address | `http://10.90.0.1/` |
| DHCP pool | `10.90.0.11` – `10.90.0.20` |

The AP stays active after the site Wi-Fi is configured, so a mistaken site
credential does not lock you out. The AP password and DHCP range are constants
in `firmware/sketches/receiver/config.h`.

## 3. Configure site Wi-Fi and cloud

1. Open `http://10.90.0.1/` on a phone or laptop connected to the AP.
2. Fill in the Python server URL (e.g. `http://192.168.1.100:8000` during
   development).
3. Paste the **device token** from step 1. This is the credential the board
   presents to the server; the server maps it to the device record. It is
   separate from the shared server API token.
4. Choose the device's **time zone** and, if known, latitude, longitude and
   altitude. Altitude is required for derived sea-level pressure and cloud base.
5. Fill in up to six site Wi-Fi SSID/password pairs. They are tried in order.
   Passwords are hidden until "Show passwords" is pressed.
6. Save. The page validates SSID lengths, WPA2 password lengths, duplicate
   SSIDs, the server URL, the tokens and the site coordinates in JavaScript
   before submitting.
7. If the Wi-Fi list changed, the board restarts automatically to apply it;
   reconnect to the AP in a few seconds.
8. Optionally watch the serial log at **115200 8N1** (see the note below).

Serial console note:

- Log lines are CRLF-terminated, so `minicom`/`screen` no longer staircase.
- The first log line is held for 3 s after power-up
  (`config::kSerialStartupDelayMs`), so a console started right after applying
  power still catches the application banner. The ROM bootloader text is
  printed before that and cannot be held.
- Power-cycling the board also re-enumerates the CP2102 USB serial port; close
  and reopen the console (or re-run `arduino-cli monitor`) afterwards. A burst
  of bootloader characters at the moment of reset is normal.
- If nothing appears, check the baud rate is 115200 and that another program is
  not holding the port.

Expected messages:

```text
settings device=ak-3f2a1b station=1 wifi_profiles=1 provisioned=yes
wifi AP up: akashvani-1b (10.90.0.1) DHCP 10.90.0.11-10.90.0.20
wifi connecting to <ssid> (profile 1/1)
web local admin at http://10.90.0.1/
boot ready: AP=akashvani-1b device=ak-3f2a1b
```

Uploads start only when Wi-Fi is connected, time is valid and the token/URL are
set. A blank bench device starts with the development token from `secrets.h`;
the AP page lets you replace it with the token created in step 1. Until the RF
driver is ported, the radio remains idle; this is expected.

## 4. Verify

```bash
curl -sS http://127.0.0.1:8000/api/v1/admin/devices \
  -H 'Authorization: Bearer dev-admin-token'
```

Check `last_seen_at` and `firmware_version` after an upload. On the board, the
local status page has Status and Configuration tabs. The Status tab shows
date/time, firmware, STA connection/SSID/IP, AP state, radio/archive flags,
queue depth and last-known weather readings with current-bucket frame counts.
It refreshes every 5 seconds.

## Credentials and storage

| Secret | Location | Notes |
| --- | --- | --- |
| site Wi-Fi passwords | NVS (`wifi_pass_0`…`wifi_pass_5`) | plain text in the scaffold |
| device token | NVS (`dev_token`) | edited on the AP page; rotate if lost |
| AP password | firmware constant (`kApPassword`) | shared bench default, change in code |
| ArduinoOTA password | NVS (`admin_pw`) | editable in admin Site Settings |

The AP page has no separate login. Anyone who knows the AP password can change
the server URL, device token and Wi-Fi profiles. Add authentication and a
per-board AP password before field deployment.

## Token rotation

Create the same device again with a new token:

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/devices \
  -H 'Authorization: Bearer dev-admin-token' \
  -H 'Content-Type: application/json' \
  -d '{"id":"ak-3f2a1b","name":"Roof receiver","token":"new-token"}'
```

Enter the new token on the board's AP page (Device token field) and save. The
old token stops working immediately after the server call. If a later request
still uses it, the device has not picked up the new setting.

## Factory reset

There is no physical reset button in the scaffold. Options:

- erase only the NVS partition with `esptool.py` while preserving firmware;
- full flash erase (`esptool.py erase_flash`), then reflash;
- add an admin endpoint/button that calls `SettingsStore::clear()` and reboots
  (recommended for field service).

A full erase also removes the firmware; have the OTA/latest image available
before doing it onsite.

## Site checklist

- [ ] AP password (`kApPassword`) changed from the shared bench default.
- [ ] ArduinoOTA password (`admin_pw`) changed from the default.
- [ ] Device token unique to this board.
- [ ] Site Wi-Fi uses a dedicated/IoT network where possible.
- [ ] Time is valid (DS3231 battery fitted, SNTP reachable).
- [ ] SD card mounted and writable.
- [ ] Radio and antenna connected; frequency/power legal for the site.
- [ ] Cloud endpoint reachable and TLS verified (after pinning work).
- [ ] Enclosure, strain relief and service access checked.
- [ ] Board ID, MAC, firmware version and location recorded.
