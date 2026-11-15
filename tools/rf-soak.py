#!/usr/bin/env python3
"""Run a production-mode RF soak and produce a Markdown report.

Unlike tools/rf-bench.py, this uses the normal receiver image (interrupt-driven
RX task and channel scanning) and the normal transmitter simulator. It:

  * starts the simulator with R1;
  * captures both serial ports for the requested duration;
  * optionally queues a download_new_fw OTA in the middle of the run;
  * reads the server archive buckets for the device;
  * writes a report with valid/expected frames, CRC failures, gaps, resync and
    search counts, and queue overflows.

Port order can change when the boards re-enumerate. Verify with esptool
read_mac before running.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import pathlib
import re
import sys
import threading
import time
import urllib.error
import urllib.request

import serial

QUEUE_RE = re.compile(r"RX queue full; dropped=(\d+)")
TX_RE = re.compile(r"\bTX t=")
BAD_STATS_RE = re.compile(
    r"RX_BAD total=(\d+) rssi_min=(-?\d+) rssi_avg=(-?\d+) rssi_max=(-?\d+)"
)
BAD_FRAME_RE = re.compile(
    r"RX_BAD epoch=(\d+) ch=(\d+) rssi=(-?\d+) raw=([0-9A-F ]+)"
)
RX_BAD_RE = re.compile(r"\bRX_BAD\b")
SEQ_RE = re.compile(r"\bseq=")
HOP_RE = re.compile(r"\bHOP\b")
SEARCH_RE = re.compile(r"\bSEARCH\b")
RESYNC_RE = re.compile(r"\bRESYNC\b")


class SerialLog:
    def __init__(self, port: str, baud: int, path: pathlib.Path, label: str) -> None:
        self.port = port
        self.label = label
        self.path = path
        self.lines: list[str] = []
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self.serial = serial.Serial(port, baud, timeout=0.2, write_timeout=1)
        # Release the CP2102 auto-reset lines instead of holding the board in reset.
        self.serial.dtr = False
        self.serial.rts = False
        self._file = path.open("w", encoding="utf-8")
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._thread.start()

    def _read_loop(self) -> None:
        while not self._stop.is_set():
            try:
                raw = self.serial.readline()
            except Exception as exc:  # noqa: BLE001 - log and stop this reader
                self._file.write(f"READER-ERROR {exc}\n")
                self._file.flush()
                return
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if not line:
                continue
            with self._lock:
                self.lines.append(line)
            self._file.write(line + "\n")
            self._file.flush()

    def send(self, command: str) -> None:
        self.serial.write((command + "\n").encode("ascii"))
        self.serial.flush()

    def close(self) -> None:
        self._stop.set()
        self._thread.join(timeout=2)
        try:
            self.serial.close()
        except Exception:  # noqa: BLE001
            pass
        self._file.close()

    def snapshot(self) -> list[str]:
        with self._lock:
            return list(self.lines)


def api_json(
    url: str,
    token: str,
    method: str = "GET",
    body: dict | None = None,
) -> object:
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(url, data=data, method=method)
    request.add_header("Authorization", f"Bearer {token}")
    if body is not None:
        request.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(request, timeout=20) as response:
        return json.load(response)


def queue_command(base: str, token: str, device: str, kind: str, payload: dict) -> dict:
    created = api_json(
        f"{base}/api/v1/admin/devices/{device}/commands",
        token,
        "POST",
        {"type": kind, "payload": payload},
    )
    assert isinstance(created, dict)
    command_id = created["id"]
    deadline = time.monotonic() + 180
    latest = created
    while time.monotonic() < deadline:
        rows = api_json(
            f"{base}/api/v1/admin/devices/{device}/commands?limit=20", token
        )
        assert isinstance(rows, list)
        for row in rows:
            if row.get("id") == command_id:
                latest = row
                break
        if latest.get("status") in {"acked", "failed"}:
            return latest
        time.sleep(2)
    return latest


def parse_archive_time(value: str) -> dt.datetime:
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def fetch_archive(base: str, token: str, device: str, start: dt.datetime,
                  end: dt.datetime) -> list[dict]:
    rows = api_json(
        f"{base}/api/v1/admin/archive?device_id={device}&limit=100", token
    )
    assert isinstance(rows, list)
    selected: list[dict] = []
    for row in rows:
        stamp = parse_archive_time(row["date_time"])
        # The bucket end is the timestamp. Include only buckets that overlap the
        # run; a bucket that ended before the run started is from earlier data.
        if stamp > start and stamp <= end + dt.timedelta(minutes=5):
            selected.append(row)
    selected.sort(key=lambda row: row["date_time"])
    return selected


def serial_counts(lines: list[str]) -> dict[str, object]:
    counts: dict[str, object] = {
        "tx_slots": 0,
        "rx_bad": 0,
        "bad_total": 0,
        "bad_rssi_min": 0,
        "bad_rssi_avg": 0,
        "bad_rssi_max": 0,
        "strong_bad": [],
        "valid_lines": 0,
        "hops": 0,
        "searches": 0,
        "resyncs": 0,
        "queue_drops": 0,
    }
    for line in lines:
        if TX_RE.search(line):
            counts["tx_slots"] = int(counts["tx_slots"]) + 1
        match = BAD_STATS_RE.search(line)
        if match:
            counts["bad_total"] = int(match.group(1))
            counts["bad_rssi_min"] = int(match.group(2))
            counts["bad_rssi_avg"] = int(match.group(3))
            counts["bad_rssi_max"] = int(match.group(4))
        else:
            match = BAD_FRAME_RE.search(line)
            if match:
                counts["rx_bad"] = int(counts["rx_bad"]) + 1
                strong = counts["strong_bad"]
                assert isinstance(strong, list)
                strong.append(
                    {
                        "epoch": int(match.group(1)),
                        "ch": int(match.group(2)),
                        "rssi": int(match.group(3)),
                        "raw": match.group(4).strip(),
                    }
                )
            elif RX_BAD_RE.search(line):
                counts["rx_bad"] = int(counts["rx_bad"]) + 1
        if SEQ_RE.search(line):
            counts["valid_lines"] = int(counts["valid_lines"]) + 1
        if HOP_RE.search(line):
            counts["hops"] = int(counts["hops"]) + 1
        if SEARCH_RE.search(line):
            counts["searches"] = int(counts["searches"]) + 1
        if RESYNC_RE.search(line):
            counts["resyncs"] = int(counts["resyncs"]) + 1
        match = QUEUE_RE.search(line)
        if match:
            counts["queue_drops"] = max(int(counts["queue_drops"]), int(match.group(1)))
    return counts


def write_report(
    path: pathlib.Path,
    args: argparse.Namespace,
    started: dt.datetime,
    ended: dt.datetime,
    archive: list[dict],
    rx_counts: dict[str, object],
    tx_counts: dict[str, object],
    ota_result: dict | None,
) -> str:
    lines: list[str] = []
    lines.append("# RF soak report")
    lines.append("")
    lines.append(f"- Started (UTC): `{started.isoformat()}`")
    lines.append(f"- Ended (UTC): `{ended.isoformat()}`")
    lines.append(f"- Duration: {args.duration:.0f} s")
    lines.append(f"- Receiver port: `{args.rx_port}`")
    lines.append(f"- Transmitter port: `{args.tx_port}`")
    lines.append(f"- Device: `{args.device_id}`")
    lines.append("")
    lines.append("## Transmitter")
    lines.append("")
    lines.append(f"- Serial `TX` lines: {tx_counts['tx_slots']}")
    lines.append("")

    if archive:
        total_valid = 0
        total_effective = 0.0
        lines.append("## Server archive buckets")
        lines.append("")
        lines.append(
            "| Bucket end (UTC) | Overlap s | Valid | Eff. expected | Valid % | CRC bad | Gap slots | Resync | Search | RSSI avg |"
        )
        lines.append(
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"
        )
        for row in archive:
            stamp = parse_archive_time(row["date_time"])
            bucket_start = stamp - dt.timedelta(minutes=5)
            overlap = max(
                0.0,
                (min(ended, stamp) - max(started, bucket_start)).total_seconds(),
            )
            effective = overlap * 1000.0 / args.slot_ms if args.slot_ms > 0 else 0.0
            percent = (
                100.0 * row["frames_valid"] / effective if effective > 0 else 0.0
            )
            total_valid += row["frames_valid"]
            total_effective += effective
            rssi = row["rssi_avg_dbm"]
            rssi_text = "" if rssi is None else f"{rssi:.1f}"
            lines.append(
                f"| {row['date_time']} | {overlap:.0f} | {row['frames_valid']} | "
                f"{effective:.0f} | {percent:.1f}% | {row['frames_crc_bad']} | "
                f"{row['gap_slots']} | {row['resync_count']} | {row['search_count']} | "
                f"{rssi_text} |"
            )
        total_percent = (
            100.0 * total_valid / total_effective if total_effective > 0 else 0.0
        )
        lines.append("")
        lines.append(
            f"**Totals:** {total_valid} valid against {total_effective:.0f} "
            f"window-adjusted expected slots ({total_percent:.1f}%)."
        )
        lines.append("")
        lines.append(
            "`Eff. expected` uses the overlap of each bucket with the run and "
            "the configured slot interval, so a partial first/last bucket is not "
            "penalised by the full 100-slot denominator. Only finalized buckets "
            "are available; the final partial bucket may not be uploaded yet."
        )
    else:
        lines.append("## Server archive buckets")
        lines.append("")
        lines.append("No archive rows were returned for the window.")
    lines.append("")
    lines.append("## Receiver serial counters")
    lines.append("")
    lines.append(f"- Frame lines seen: {rx_counts['valid_lines']} (serial capture may truncate)")
    lines.append(f"- RX_BAD lines: {rx_counts['rx_bad']} (serial capture may truncate)")
    lines.append(f"- Hops: {rx_counts['hops']}")
    lines.append(f"- Search entries: {rx_counts['searches']}")
    lines.append(f"- Resync events: {rx_counts['resyncs']}")
    lines.append(f"- Maximum queue drops: {rx_counts['queue_drops']}")
    lines.append("")
    bad_total = int(rx_counts["bad_total"])
    lines.append("## Invalid frame RSSI")
    lines.append("")
    if bad_total:
        lines.append(
            f"- Count: {bad_total}, RSSI min/avg/max: "
            f"{rx_counts['bad_rssi_min']} / {rx_counts['bad_rssi_avg']} / "
            f"{rx_counts['bad_rssi_max']} dBm"
        )
    else:
        lines.append("- No periodic invalid-frame RSSI summary was logged.")
    strong = rx_counts["strong_bad"]
    assert isinstance(strong, list)
    lines.append(
        f"- Strong invalid frames logged individually (RSSI threshold -85 dBm): {len(strong)}"
    )
    if strong:
        lines.append("")
        lines.append("| Epoch | Channel | RSSI dBm | Raw |")
        lines.append("| ---: | ---: | ---: | --- |")
        for row in strong[-20:]:
            lines.append(
                f"| {row['epoch']} | {row['ch']} | {row['rssi']} | `{row['raw']}` |"
            )
    lines.append("")
    if ota_result is not None:
        lines.append("## OTA event")
        lines.append("")
        lines.append(f"- Status: `{ota_result.get('status')}`")
        lines.append(f"- Result: `{json.dumps(ota_result.get('result'))}`")
        if ota_result.get("error"):
            lines.append(f"- Error: `{ota_result['error']}`")
        lines.append("")
    lines.append("## Interpretation")
    lines.append("")
    lines.append(
        "`frames_valid` against the window-adjusted expected slots comes from "
        "the device archive and is the authoritative receive count. Missed "
        "frames can come from independent channel scanning (the simulator "
        "advances one channel per slot while the receiver dwells on a channel), "
        "not only from blocking commands. `frames_crc_bad` counts noise-level "
        "packets that pass the two-symbol sync tolerance; a high value is a "
        "bench-noise observation. Invalid frames are counted in the RX task and "
        "are not placed in the frame queue."
    )
    text = "\n".join(lines) + "\n"
    path.write_text(text, encoding="utf-8")
    return text


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rx-port", required=True, help="receiver serial port")
    parser.add_argument("--tx-port", required=True, help="transmitter serial port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=600.0)
    parser.add_argument(
        "--slot-ms",
        type=float,
        default=3000.0,
        help="transmitter slot interval used for window-adjusted expectations",
    )
    parser.add_argument(
        "--boot-wait",
        type=float,
        default=6.0,
        help="seconds to wait after opening the ports before starting R1",
    )
    parser.add_argument("--outdir", default="/tmp/opencode/rf-soak")
    parser.add_argument("--server", default="http://127.0.0.1:8000")
    parser.add_argument(
        "--admin-token",
        default=os.environ.get("AKASHAVANI_ADMIN_TOKEN", "dev-admin-token"),
    )
    parser.add_argument("--device-id", required=True)
    parser.add_argument(
        "--ota-version",
        default=None,
        help="optional: queue download_new_fw for this version at --ota-at seconds",
    )
    parser.add_argument("--ota-at", type=float, default=300.0)
    args = parser.parse_args()

    outdir = pathlib.Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    stamp = dt.datetime.now(dt.UTC).strftime("%Y%m%d-%H%M%S")
    rx_log = outdir / f"{stamp}-receiver.log"
    tx_log = outdir / f"{stamp}-transmitter.log"
    report_path = outdir / f"{stamp}-report.md"
    print(f"receiver log:    {rx_log}")
    print(f"transmitter log: {tx_log}")
    print(f"report:          {report_path}")

    rx = SerialLog(args.rx_port, args.baud, rx_log, "RX")
    tx = SerialLog(args.tx_port, args.baud, tx_log, "TX")
    started = dt.datetime.now(dt.UTC)
    started_mono = time.monotonic()
    ota_result: dict | None = None
    try:
        if args.boot_wait > 0:
            print(f"waiting {args.boot_wait:.0f}s for boot...", flush=True)
            time.sleep(args.boot_wait)
        tx.send("R1")
        started = dt.datetime.now(dt.UTC)
        started_mono = time.monotonic()
        print("transmitter started (R1); collecting...", flush=True)
        next_status = started_mono + 60.0
        while True:
            now = time.monotonic()
            elapsed = now - started_mono
            if elapsed >= args.duration:
                break
            if args.ota_version and ota_result is None and elapsed >= args.ota_at:
                print(f"queueing download_new_fw {args.ota_version}...", flush=True)
                try:
                    ota_result = queue_command(
                        args.server,
                        args.admin_token,
                        args.device_id,
                        "download_new_fw",
                        {"version": args.ota_version},
                    )
                    print(f"  OTA status: {ota_result.get('status')}", flush=True)
                except Exception as exc:  # noqa: BLE001 - report and keep soaking
                    ota_result = {"status": "host-error", "error": str(exc)}
                    print(f"  OTA queue failed: {exc}", file=sys.stderr)
            if now >= next_status:
                print(f"  {elapsed:.0f}/{args.duration:.0f}s", flush=True)
                next_status += 60.0
            time.sleep(1.0)
    finally:
        try:
            tx.send("S")
        except Exception:  # noqa: BLE001
            pass
        time.sleep(1)
        rx.close()
        tx.close()
    ended = dt.datetime.now(dt.UTC)

    try:
        archive = fetch_archive(
            args.server, args.admin_token, args.device_id, started, ended
        )
    except Exception as exc:  # noqa: BLE001 - still write the serial report
        print(f"warning: could not read archive rows: {exc}", file=sys.stderr)
        archive = []

    rx_counts = serial_counts(rx.lines)
    tx_counts = serial_counts(tx.lines)
    text = write_report(
        report_path, args, started, ended, archive, rx_counts, tx_counts, ota_result
    )
    print()
    print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
