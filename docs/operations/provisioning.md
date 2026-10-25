# Provisioning a receiver

A new board always starts its own Wi-Fi AP, so provisioning does not depend on
the site network being correct.

## 1. Create the device on the server

Choose/derive the device ID printed on the serial log (or use the MAC-derived
`ak-xxxxxx`) and create a token:

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
| SSID | `Akashvani-<device-id>` |
| Password | `akashvani-setup` |
| Board address | `http://192.168.4.1/` |
| Admin password | `akashvani-admin` |

The AP stays active after the site Wi-Fi is configured, so a mistaken site
credential does not lock you out.

## 3. Configure site Wi-Fi and cloud

1. Open `http://192.168.4.1/`.
2. Enter the admin password and unlock settings.
3. Set:
   - site Wi-Fi SSID/password;
   - cloud base URL, e.g. `http://192.168.1.100:8000` during development;
   - the device token from step 1;
   - upload interval (default 60 s);
   - upload/archive toggles.
4. Save, then reboot from the page.
5. Watch the serial log at 115200 baud.

Expected messages:

```text
settings device=ak-3f2a1b provisioned=yes
wifi AP up: Akashvani-ak-3f2a1b (192.168.4.1)
wifi connecting to <ssid>
wifi connected: <ip>
web local admin at http://192.168.4.1/
boot ready: AP=Akashvani-ak-3f2a1b device=ak-3f2a1b
```

Uploads start only when Wi-Fi is connected, time is valid and the token/URL are
set. Until the RF driver is ported, the radio remains idle; this is expected.

## 4. Verify

```bash
curl -sS http://127.0.0.1:8000/api/v1/admin/devices \
  -H 'Authorization: Bearer dev-admin-token'
```

Check `last_seen_at` and `firmware_version` after an upload. On the board, the
local status page shows the AP/STA addresses, time source, radio/archive flags
and queue depth.

## Credentials and storage

| Secret | Location | Notes |
| --- | --- | --- |
| site Wi-Fi password | NVS | plain text in the scaffold |
| device token | NVS | bearer credential; rotate if lost |
| AP password | NVS | default shared password until changed |
| admin password | NVS | local admin only; separate from token |

Change the default AP and admin passwords before deployment. There is no
per-board unique default yet; that is a hardening task.

## Token rotation

Create the same device again with a new token:

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/devices \
  -H 'Authorization: Bearer dev-admin-token' \
  -H 'Content-Type: application/json' \
  -d '{"id":"ak-3f2a1b","name":"Roof receiver","token":"new-token"}'
```

Update the device through the local admin page and reboot. The old token stops
working immediately after the server call. If a later request still uses it, the
device has not picked up the new setting.

## Factory reset

There is no physical reset button in the scaffold. Options:

- erase only the NVS partition with `esptool.py` while preserving firmware;
- full flash erase (`esptool.py erase_flash`), then reflash;
- add an admin endpoint/button that calls `SettingsStore::clear()` and reboots
  (recommended for field service).

A full erase also removes the firmware; have the OTA/latest image available
before doing it onsite.

## Site checklist

- [ ] AP and admin passwords changed from defaults.
- [ ] Device token unique to this board.
- [ ] Site Wi-Fi uses a dedicated/IoT network where possible.
- [ ] Time is valid (DS3231 battery fitted, SNTP reachable).
- [ ] SD card mounted and writable.
- [ ] Radio and antenna connected; frequency/power legal for the site.
- [ ] Cloud endpoint reachable and TLS verified (after pinning work).
- [ ] Enclosure, strain relief and service access checked.
- [ ] Board ID, MAC, firmware version and location recorded.
