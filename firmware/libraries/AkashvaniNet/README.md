# AkashvaniNet

Everything that leaves the board, plus the settings that decide where it goes.

| Unit | Responsibility |
| --- | --- |
| `DeviceSettings` | Runtime configuration, fixed-size and copyable |
| `SettingsStore` | NVS persistence and MAC-derived default device ID |
| `WifiService` | AP+STA concurrently; AP is the provisioning/local-admin path |
| `WebConfigServer` | Mobile status/configuration page on port 80 (AP only, no admin token) |
| `ApiClient` | Observation upload and firmware-check calls over HTTP/HTTPS |
| `OtaService` | ArduinoOTA (LAN) and HTTP pull OTA (remote) |

## Provisioning model

The board always brings up its own AP (`akashvani-<last-mac-byte>`) so it
remains reachable when site Wi-Fi is wrong, absent or changed. The AP uses
`10.90.0.1` and hands out `10.90.0.11`-`10.90.0.20` by DHCP. The status page at
`http://10.90.0.1/` refreshes every 5 s and shows time, firmware, STA/AP
addresses, subsystem state and the last weather frame (once the decoder exists).

The configuration form edits the station ID, the Python server URL and up to six
site Wi-Fi SSID/password pairs. The STA side tries them in order. Settings are
written to NVS; a Wi-Fi change schedules a restart so the new profile list takes
effect. A blank device is seeded with the bench defaults from `config.h` and
git-ignored `secrets.h`; once anything is saved through the page, NVS wins.

## Security notes for porting

- The AP page has no separate login. Anyone with the AP WPA2 password can
  change the station ID, server URL and Wi-Fi profiles. This is acceptable
  while the AP password is a bench default; add authentication before field
  deployment.
- `WiFiClientSecure` currently uses `setInsecure()` because the development
  server uses a self-signed certificate. Pin a CA before production.
- The device token is sent in `X-Device-Token`; it is a bearer credential.
  Prefer per-device tokens and rotate them if a card/board is lost.
- The cloud token and site Wi-Fi passwords live in plain NVS. ArduinoOTA still
  uses the NVS admin password, which is not editable from the new page.
- OTA images are not yet signature-verified. The server supplies a SHA-256;
  verifying it during download is a release blocker.
