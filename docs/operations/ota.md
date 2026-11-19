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
| `get_config` | `{}` | `reported`; the document is sent with `PUT /api/v1/devices/me/config` |
| `push_config` | full configuration document | `applied`, `reboot_required` |
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
- queue `get_config`, `push_config`, `download_new_fw`, `boot_with_new_fw`,
  `reboot`, `sync_time` and `self_test`;
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
  -d '{"version":"0.2.0","channel":"stable","filename":"akashavani-0.2.0-receiver.ino.bin"}'
```

## LAN OTA (Wi-Fi, no server commands)

The receiver also runs the ArduinoOTA listener on the site LAN (port 3232,
hostname = device ID, e.g. `ak-2731e0.local`). This path needs no server command
and writes the inactive slot directly.

1. Build the receiver image for the same version:

   ```bash
   make fw-build BOARD=receiver
   ```

2. Upload over the LAN:

   ```bash
   source tools/fw-common.sh
   "$ARDUINO_CLI" upload \
     --fqbn "$(select_fqbn)" \
     --port ak-2731e0.local \
     --protocol network \
     --upload-field password=akashavani-admin \
     --input-dir firmware/build/receiver \
     firmware/sketches/receiver
   ```

   If mDNS does not resolve, use the device's STA IP from the admin device row
   or AP page instead of the hostname. The password is the **OTA password over
   Wi-Fi** in `/admin` → Site Settings (NVS `admin_pw`), not the AP or server
   token.

3. ArduinoOTA resets the board into the new image. The server sees the new
   `X-Firmware-Version` on the next poll.

`kEnableLanOta` must be `true` in `firmware/sketches/receiver/config.h`. This is
a trusted-LAN maintenance path; images are not signed and the link is not
encrypted. Keep USB recovery available.

### GUI tools (Windows and macOS)

The device speaks the ArduinoOTA protocol, so any tool that supports an
“Arduino network port” / `espota` can upload over Wi-Fi. The practical GUI
options are:

| Tool | Platforms | How it works |
| --- | --- | --- |
| **Arduino IDE 2.x** | Windows, macOS, Linux | `Tools → Port` shows the board as a network port (e.g. `ak-2731e0 at 192.168.29.84`). Select it instead of the USB port and click **Upload**. The IDE asks for the OTA password over Wi-Fi the first time. |
| **PlatformIO IDE** (VS Code extension) | Windows, macOS, Linux | Set `upload_protocol = espota`, `upload_port = ak-2731e0.local` (or the IP) and `upload_flags = --auth=akashavani-admin` in `platformio.ini`, then use the **Upload** button. This also works when mDNS discovery is blocked. |
| **Arduino IDE 1.8.x** | Windows, macOS, Linux | Same network-port workflow as IDE 2.x; older but still works. |
| **`espota.py`** (CLI, shipped with the ESP32 core) | Any | Not a GUI, but it is what most GUI wrappers call. Use it directly for a prebuilt `.bin`. |

Arduino IDE 2.x is the simplest macOS/Windows GUI when the computer and device
are on the same LAN:

1. Install the ESP32 core version used by this project (see
   `AKASHAVANI_ESP32_CORE`, currently `3.0.2`).
2. Open `firmware/sketches/receiver/receiver.ino`.
3. Select the ESP32 board and keep the sketch's `partitions.csv` scheme.
4. Select the **network** port under `Tools → Port`; do not select the USB
   serial port.
5. Click **Upload** and enter `akashavani-admin` (or the current **OTA password
   over Wi-Fi**) when prompted.

If the network port does not appear:

- confirm the device is connected to site Wi-Fi (the AP-only state does not
  advertise on your LAN);
- allow Arduino IDE/mDNS through the OS firewall; on macOS allow “local
  network” access, on Windows allow the private-network firewall rule;
- ensure the computer and device are on the same subnet and multicast/mDNS is
  not blocked;
- fall back to PlatformIO with the device IP, or the `arduino-cli` command
  above.

GUI tools build the sketch from source. To push an already-built
`akashavani-receiver-0.3.1.bin` without rebuilding, use `espota.py` from the
ESP32 core, for example:

```bash
python ~/.arduino15/packages/esp32/hardware/esp32/3.0.2/tools/espota.py \
  -i 192.168.29.84 -p 3232 -a akashavani-admin \
  -f firmware/build/receiver/receiver.ino.bin
```

Third-party GUI wrappers around `espota.py` exist for Windows (for example
ESP-Ota-UI and espota-gui), but they are not maintained by this project. Prefer
Arduino IDE 2.x or PlatformIO unless you specifically need a prebuilt-binary
uploader.

These LAN tools bypass `download_new_fw` and `boot_with_new_fw`: the upload
goes straight to the inactive slot and the device restarts when it completes.


## Example: 0.3.0 → 0.3.1 via the server

1. Set `firmware/VERSION` to `0.3.1`.
2. Build and test:

   ```bash
   make fw-build-all
   make fw-test
   make server-test
   ```

3. Build/export the receiver image and copy it into the server's firmware
   directory:

   ```bash
   make fw-export
   ```

   This prints the SHA-256 and the registration command. The file is
   `server/data/firmware/akashavani-receiver-0.3.1.bin`.

4. Register the release (or use **Firmware → Upload and register** in
   `/admin`):

   ```bash
   curl -sS -X POST http://127.0.0.1:8000/api/v1/admin/firmware \
     -H 'Authorization: Bearer dev-admin-token' \
     -H 'Content-Type: application/json' \
     -d '{"version":"0.3.1","channel":"stable","filename":"akashavani-receiver-0.3.1.bin"}'
   ```

5. Confirm `GET /api/v1/admin/firmware` lists `0.3.1`.

6. Queue the download:

   ```bash
   curl -sS -X POST \
     http://127.0.0.1:8000/api/v1/admin/devices/ak-2731e0/commands \
     -H 'Authorization: Bearer dev-admin-token' \
     -H 'Content-Type: application/json' \
     -d '{"type":"download_new_fw","payload":{"version":"0.3.1"}}'
   ```

7. Watch `/admin` → Commands until the command is `acked` with `staged: true`,
   the expected `sha256`, `bytes` and `partition`. If it fails, the running
   `0.3.0` image is untouched.

8. Queue the boot:

   ```bash
   curl -sS -X POST \
     http://127.0.0.1:8000/api/v1/admin/devices/ak-2731e0/commands \
     -H 'Authorization: Bearer dev-admin-token' \
     -H 'Content-Type: application/json' \
     -d '{"type":"boot_with_new_fw","payload":{"version":"0.3.1"}}'
   ```

9. Confirm the device reboots, the serial banner says `Akashavani 0.3.1`, and
   `/admin` shows firmware `0.3.1`. The config should report again on boot.

There is no automatic rollback. If the new image does not boot, recover over
USB.

The RFM69 receive path keeps running during the download: DIO0 notifies a
high-priority task that drains the FIFO into a 128-frame RAM queue. The main
loop processes those frames after the command finishes, with their original
receive timestamps and channels. If a blocking operation outlasts the queue,
overflow is logged and the dropped count is incremented.

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
   restarts, and reports the new version on later device requests and on its
   AP page.
7. Confirm the running partition changed (`app0` ↔ `app1`).

## Device flow

1. Lease pending commands while STA Wi-Fi is connected.
2. The running firmware version is reported with every device request and on
   the AP page, not via a command.
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
4. Confirm the device row and AP page report the new version and the other
   partition.
5. Confirm settings and NVS survive. The SD archive is not yet implemented.
6. A deliberate `boot_with_new_fw` without a prior download must fail without
   rebooting.

Never test OTA on a device you cannot physically recover.
