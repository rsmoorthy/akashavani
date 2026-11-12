# OTA updates

There are two independent paths:

| Path | Transport | Use |
| --- | --- | --- |
| ArduinoOTA | LAN, device listens on port 3232 | bench and site LAN maintenance |
| Server commands + HTTP pull | `GET /api/v1/firmware/{version}/download` | remote two-step updates |

Both write to the inactive OTA slot. The running image is not overwritten.

## Two-step command flow

The device polls the server for commands every 30 seconds. OTA is split so an
operator can download first and choose when to reboot:

| Command | Payload | Result |
| --- | --- | --- |
| `get_fw_version` | `{}` | project version, running partition, build date/time, free sketch space, station ID, MAC |
| `download_new_fw` | `version` (server fills `url`, `sha256`, `size_bytes`) | staged slot, byte count, verified SHA-256 |
| `boot_with_new_fw` | `version` | selects the staged slot and restarts |

`download_new_fw` uses the ESP-IDF OTA API (`esp_ota_begin` / `esp_ota_write` /
`esp_ota_end`). Calling `esp_ota_end` validates the image and writes it to the
inactive partition, but deliberately does **not** call
`esp_ota_set_boot_partition`. `boot_with_new_fw` checks the version recorded in
NVS by the download, calls `esp_ota_set_boot_partition`, acknowledges the
command, and only then restarts. A boot command with no matching staged image
fails without rebooting.

The current firmware/partition layout has no bootloader rollback policy. Once
the staged slot is selected it remains selected.

## Admin upload and UI

Start the server and open `http://192.168.29.31:8000/admin`. Enter the admin
bearer token (`dev-admin-token` by default in development). The page can:

- list devices with station ID, MAC, firmware version and last-seen time;
- upload a new firmware image with a version and channel;
- queue `get_fw_version`, `download_new_fw`, `boot_with_new_fw`, `reboot`,
  `sync_time` and `self_test`;
- inspect command payload, status, result and error history.

The equivalent upload API is:

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/firmware/upload \
  -H 'Authorization: Bearer dev-admin-token' \
  -F 'version=0.2.0' \
  -F 'channel=stable' \
  -F 'notes=bench image' \
  -F 'file=@firmware/build/receiver/receiver.ino.bin;type=application/octet-stream'
```

Server-side registration by filename still works if the file is already in
`server/data/firmware/`:

```bash
curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/firmware \
  -H 'Authorization: Bearer dev-admin-token' \
  -H 'Content-Type: application/json' \
  -d '{"version":"0.2.0","channel":"stable","filename":"akashvani-0.2.0-receiver.ino.bin"}'
```

## Release process

1. Bump `firmware/VERSION`.
2. Build and run the test suites.
3. Build the receiver image:

   ```bash
   make fw-build BOARD=receiver
   make fw-test
   ```

4. Upload the image and queue `download_new_fw` from the admin UI or API.
5. Wait for the download acknowledgement and confirm the SHA-256 and staged
   partition in the result.
6. Queue `boot_with_new_fw` with the same version. The device acknowledges,
   restarts, and reports the new version in `get_fw_version` and on later
   uploads.
7. Confirm the running partition changed (`app0` ↔ `app1`).

## Device flow

1. Lease pending commands while STA Wi-Fi is connected.
2. `get_fw_version` returns `AKASHVANI_FW_VERSION`, not the ESP-IDF descriptor
   version.
3. `download_new_fw` resolves a relative URL against the configured server,
   downloads with the device token, streams a SHA-256, writes the inactive slot,
   and aborts on size/hash failure.
4. The sketch records `staged_version` in NVS namespace `ota_meta`.
5. `boot_with_new_fw` checks NVS, verifies the partition contains a valid app,
   selects it, clears the marker, acknowledges, and restarts.

## Partition requirement

OTA needs `otadata` and two application slots. The receiver sketch carries
`firmware/sketches/receiver/partitions.csv`:

```text
app0  0x10000  0x1E0000   (1,966,080 bytes = 1.875 MiB)
app1  0x1F0000 0x1E0000   (1,966,080 bytes = 1.875 MiB)
```

The 16 MB profile still uses the stock `app3M_fat9M_16MB` scheme. Do not select
a no-OTA layout.

## Current limitations (release blockers)

- **Signature:** images are not signed. Add secure boot + signed OTA before
  exposing the service beyond a trusted network.
- **TLS:** `WiFiClientSecure::setInsecure()` is used for HTTPS development.
  Pin the production CA and remove `setInsecure()`.
- **Rollback:** no application-level health check or bootloader rollback policy
  is implemented or tested.
- **Resume:** downloads restart from zero; there is no range/resume support.
- **Power:** an interrupted download aborts the staged image and leaves the
  current slot bootable, but power loss during the download has not been drilled
  on hardware yet.
- **Credentials:** the bench SSID/password/token are in git-ignored
  `firmware/sketches/receiver/secrets.h`; replace with provisioning.

## Testing an update

On the bench:

1. Run the server and upload a higher version.
2. Queue `download_new_fw`; confirm the result reports the staged slot and the
   expected SHA-256.
3. Queue `boot_with_new_fw`; confirm the reset and the new version banner.
4. Confirm `get_fw_version` reports the new version and the other slot.
5. Confirm settings and NVS survive. The SD archive is not yet implemented.
6. A deliberate `boot_with_new_fw` without a prior download must fail without
   rebooting.

Never test OTA on a device you cannot physically recover.
