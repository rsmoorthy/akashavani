# AkashvaniNet

Everything that leaves the board, plus the settings that decide where it goes.

| Unit | Responsibility |
| --- | --- |
| `DeviceSettings` | Runtime configuration, fixed-size and copyable |
| `SettingsStore` | NVS persistence and MAC-derived default device ID |
| `WifiService` | AP+STA concurrently; AP is the provisioning/local-admin path |
| `WebConfigServer` | Small local admin API + page, protected by an admin password |
| `ApiClient` | Observation upload and firmware-check calls over HTTP/HTTPS |
| `OtaService` | ArduinoOTA (LAN) and HTTP pull OTA (remote) |

## Provisioning model

The board always brings up its own AP (`Akashvani-<id>`) so it remains
reachable when site Wi-Fi is wrong, absent or changed. STA credentials, server
URL and device token are written to NVS through the local admin page. Cloud
upload starts only when STA is connected, time is valid and the device is
provisioned.

## Security notes for porting

- `WiFiClientSecure` currently uses `setInsecure()` because the development
  server uses a self-signed certificate. Pin a CA before production.
- The device token is sent in `X-Device-Token`; it is a bearer credential.
  Prefer per-device tokens and rotate them if a card/board is lost.
- The local admin password is separate from the cloud token, but both live in
  NVS. Consider encrypted NVS for production.
- OTA images are not yet signature-verified. The server supplies a SHA-256;
  verifying it during download is a release blocker.
