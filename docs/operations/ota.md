# OTA updates

There are two independent paths:

| Path | Transport | Use |
| --- | --- | --- |
| ArduinoOTA | LAN, device listens on port 3232 | bench and site LAN maintenance |
| HTTP pull | `GET /api/v1/firmware/{version}/download` | remote updates from the cloud |

Both write to the inactive OTA slot. The running image is not overwritten.

## Release process

1. Bump `firmware/VERSION` (for example `0.2.0`).
2. Build and run the test suites.
3. Build and export the image:

   ```bash
   make fw-build BOARD=receiver
   make fw-test
   make fw-export
   ```

4. Register the release on the server:

   ```bash
   curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/firmware \
     -H 'Authorization: Bearer dev-admin-token' \
     -H 'Content-Type: application/json' \
     -d '{"version":"0.2.0","channel":"stable","filename":"akashvani-receiver-0.2.0.bin"}'
   ```

5. Verify the check endpoint with the current device version.
6. Trigger `check_for_update` on one device first:

   ```bash
   curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/devices/ak-3f2a1b/commands \
     -H 'Authorization: Bearer dev-admin-token' \
     -H 'Content-Type: application/json' \
     -d '{"type":"check_for_update"}'
   ```

7. Watch the device log and confirm the new version appears in the next upload
   header/device row.
8. Roll out to the rest of the fleet.

`channel` allows a staging ring (for example `beta`) before `stable`. The
firmware defaults to `stable`.

## Device flow

1. Lease the `check_for_update` command; acknowledge it.
2. `GET /api/v1/firmware/check?current_version=...&channel=stable`.
3. If an update exists, download with the device token.
4. Write to the inactive partition.
5. Reboot.
6. Report the new version on the next upload.

## Partition requirement

OTA needs `otadata` and two application slots. The scaffold uses `min_spiffs`
(4 MB) or `app3M_fat9M_16MB` (16 MB). Do not select a no-OTA layout.

## Current limitations (release blockers)

- **Checksum:** the server provides SHA-256, but the firmware does not yet
  verify it while streaming. Implement a streaming hash and refuse to boot/commit
  on mismatch.
- **Signature:** images are not signed. Add secure boot + signed OTA before
  exposing the service beyond a trusted network.
- **TLS:** `WiFiClientSecure::setInsecure()` is used for development. Pin the
  production CA and remove `setInsecure()`.
- **Rollback:** the ESP32 OTA mechanism can boot the previous slot if configured,
  but this scaffold does not explicitly enable/verify rollback. Add an
  application-level health check and rollback policy.
- **Resume:** downloads restart from zero; there is no range/resume support.
- **Power:** an interrupted update must leave the previous slot bootable. Test
  power loss during download and write on the bench.

## Testing an update

On the bench:

1. Run the server and register a higher version.
2. Enqueue `check_for_update`.
3. Confirm the device downloads, reboots and reports the new version.
4. Confirm the SD archive and settings survive.
5. Re-register the old version and roll back manually if needed.

Never test OTA on a device you cannot physically recover.
