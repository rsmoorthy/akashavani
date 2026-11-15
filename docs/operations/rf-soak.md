# RF receive soak test

Use this to measure how many Davis frames the receiver captures over a longer
window, including SD writes, archive uploads and optional OTA downloads.

The tool is `tools/rf-soak.py` / `make rf-soak`. It drives the production
receiver image, not the RF bench mode.

## Prepare

1. Flash both boards:

   ```bash
   ./tools/fw-flash.sh receiver /dev/ttyUSB1
   ./tools/fw-flash.sh transmitter_sim /dev/ttyUSB0
   ```

   Do not assume the port numbers: they can swap when the boards re-enumerate.
   Confirm with:

   ```bash
   for p in /dev/ttyUSB0 /dev/ttyUSB1; do
     echo -n "$p "; python3 \
       ~/.arduino15/packages/esp32/tools/esptool_py/4.6/esptool.py \
       --port "$p" read_mac 2>&1 | grep '^MAC:' | tail -1
   done
   ```

   The receiver must have the SD card inserted (`sd mounted` in its boot log).

2. Start the server with the admin API reachable at `127.0.0.1:8000`, and
   confirm the receiver is online:

   ```bash
   curl -s -H 'Authorization: Bearer dev-admin-token' \
     http://127.0.0.1:8000/api/v1/admin/devices
   ```

## Run

For a 10-minute run:

```bash
make rf-soak RX_PORT=/dev/ttyUSB1 TX_PORT=/dev/ttyUSB0 DEVICE_ID=ak-2731e0 DURATION=600
```

The transmitter is started with `R1` (one slot every 3 s) after a short boot
wait, and stopped with `S` at the end. The script writes:

- `<timestamp>-receiver.log`
- `<timestamp>-transmitter.log`
- `<timestamp>-report.md`

under `/tmp/opencode/rf-soak/`, and prints the report.

To stage a `download_new_fw` in the middle of the run, register the release
first, then pass its version:

```bash
make rf-soak ... OTA_VERSION=0.3.1
```

The OTA is only staged; it is not booted. This exercises the blocking download
path while the RF task keeps receiving.

## Report fields

The server archive is the authoritative receive count:

- `frames_valid` against a **window-adjusted** expected slot count: the tool
  prorates each 5-minute bucket by how much of it overlaps the run, so a
  partial first/last bucket is not penalised by the full 100-slot denominator;
- `frames_crc_bad` — packets that passed sync but failed the Davis CRC (noise);
- `gap_slots`, `resync_count`, `search_count` — receiver scan behaviour;
- `rssi_avg_dbm`.

It also reports invalid-frame RSSI diagnostics:

- a periodic `RX_BAD total=... rssi_min=... rssi_avg=... rssi_max=...` line;
- individual `RX_BAD epoch=... ch=... rssi=... raw=...` lines for invalid
  frames at or above -85 dBm;
- a report section listing the invalid-frame min/avg/max and the strongest
  recent invalid frames. Weaker noise is aggregated only, so it cannot flood
  the serial console.

The server stores the per-bucket invalid-frame RSSI summary in `archive` as
`crc_bad_rssi_min_dbm`, `crc_bad_rssi_avg_dbm` and `crc_bad_rssi_max_dbm`, so
the values are also visible through `/api/v1/admin/archive` and SQL queries.

The privacy/bandwidth trade-off is deliberate: a frame that should have been
valid but fails CRC at strong RSSI is logged with its epoch/channel/raw bytes,
which is exactly what you need to correlate it with a transmitter slot.

Serial counters in the report are supplementary; a busy 115200 serial stream can
truncate lines.

Only **finalized** 5-minute buckets appear. If the run ends mid-bucket, that
last partial bucket may not have been uploaded yet; run `DURATION=660` or wait a
few minutes and re-read `/api/v1/admin/archive`.

## Typical baseline

A healthy bench run with the simulator at 0 dBm and independent channel
scanning captures roughly 95–100% of transmitted slots after the standby/FIFO
fix. Misses can still come from independent channel scanning (the simulator
advances one channel per 3 s while the receiver dwells about 3.2 s per channel).
A sudden drop during a known blocking operation, queue overflow, or a cluster
of strong invalid frames after the command is the signal to investigate.
