#!/usr/bin/env python3
"""Run the Davis/RFM69 bench scenarios on USB0 (receiver) and USB1 (simulator).

The firmware in this repository is deliberately command driven so the same
images can run all three tests without reflashing:

  transmitter commands: R1 (continuous), R2 (random skipped channels),
                        R3 (two automatic restarts), S (stop), X (restart), ?
  receiver commands:    R (reset channel/statistics), S or ? (print stats)

The script only orchestrates serial I/O and timestamps every line. It does not
interpret RF results; see docs/specs/rf-davis-receive.md for the acceptance checks.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import pathlib
import sys
import threading
import time

import serial


class SerialCapture:
    def __init__(self, port: str, baud: int, path: pathlib.Path, tag: str) -> None:
        self.port = port
        self.baud = baud
        self.path = path
        self.tag = tag
        self._stop = threading.Event()
        self._serial = serial.Serial(port, baud, timeout=0.2, write_timeout=1)
        # Do not hold the ESP32 in reset. The CP2102 auto-reset circuit is
        # normally released by both lines being deasserted.
        self._serial.dtr = False
        self._serial.rts = False
        self._start = time.monotonic()
        self._thread = threading.Thread(target=self._read_loop, daemon=True)
        self._file = path.open("w", encoding="utf-8")
        self._thread.start()

    def _read_loop(self) -> None:
        while not self._stop.is_set():
            try:
                raw = self._serial.readline()
            except Exception as exc:  # noqa: BLE001 - report and stop this reader
                self._file.write(f"{time.monotonic() - self._start:8.3f} READER-ERROR {exc}\n")
                self._file.flush()
                return
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if not line:
                continue
            elapsed = time.monotonic() - self._start
            self._file.write(f"{elapsed:8.3f} {line}\n")
            self._file.flush()

    def mark(self, text: str) -> None:
        elapsed = time.monotonic() - self._start
        self._file.write(f"{elapsed:8.3f} ===== {text} =====\n")
        self._file.flush()

    def send(self, command: str) -> None:
        self.mark(f"HOST-TX {command!r}")
        self._serial.write((command + "\n").encode("ascii"))
        self._serial.flush()

    def wait_for(self, needle: str, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        # Read from the log file tail rather than duplicating the reader.
        while time.monotonic() < deadline:
            try:
                text = self.path.read_text(encoding="utf-8", errors="replace")
            except FileNotFoundError:
                text = ""
            if needle in text:
                return True
            time.sleep(0.2)
        return False

    def close(self) -> None:
        self._stop.set()
        self._thread.join(timeout=2)
        try:
            self._serial.close()
        except Exception:  # noqa: BLE001
            pass
        self._file.close()


def run_scenario(name: str, command: str, seconds: float, receiver: SerialCapture,
                 transmitter: SerialCapture) -> None:
    print(f"\n=== {name}: {command} for {seconds:.0f}s ===", flush=True)
    receiver.send("R")
    time.sleep(1.0)
    receiver.mark(f"{name} START")
    transmitter.mark(f"{name} START")
    transmitter.send(command)
    started = time.monotonic()
    next_report = started + 30.0
    while True:
        now = time.monotonic()
        if now - started >= seconds:
            break
        if now >= next_report:
            print(f"  {name}: {now - started:.0f}/{seconds:.0f}s", flush=True)
            next_report += 30.0
        time.sleep(1.0)
    try:
        transmitter.send("S")
        time.sleep(2.0)
        receiver.send("S")
        time.sleep(1.0)
    except serial.SerialException as exc:
        print(f"warning: serial closed while stopping {name}: {exc}", file=sys.stderr)
    receiver.mark(f"{name} END")
    transmitter.mark(f"{name} END")
    print(f"=== {name} complete ===", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receiver-port", default="/dev/ttyUSB0")
    parser.add_argument("--transmitter-port", default="/dev/ttyUSB1")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--scenario", choices=("1", "2", "3", "all"), default="all")
    parser.add_argument("--duration", type=float, default=900.0)
    parser.add_argument("--outdir", default="/tmp/opencode/rf-bench")
    parser.add_argument("--skip-boot-wait", action="store_true")
    args = parser.parse_args()

    outdir = pathlib.Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")

    receiver_log = outdir / f"{stamp}-receiver.log"
    transmitter_log = outdir / f"{stamp}-transmitter.log"
    print(f"receiver log:    {receiver_log}")
    print(f"transmitter log: {transmitter_log}")

    receiver = SerialCapture(args.receiver_port, args.baud, receiver_log, "RX")
    transmitter = SerialCapture(args.transmitter_port, args.baud, transmitter_log, "TX")
    try:
        if not args.skip_boot_wait:
            print("waiting for receiver and transmitter boot banners...", flush=True)
            if not receiver.wait_for("RF bench mode", 20.0):
                print("warning: receiver boot banner not seen", file=sys.stderr)
            if not transmitter.wait_for("READY", 20.0):
                print("warning: transmitter READY not seen", file=sys.stderr)

        scenarios = {
            "1": ("TEST1 continuous", "R1"),
            "2": ("TEST2 random-skip", "R2"),
            "3": ("TEST3 restarts", "R3"),
        }
        selected = scenarios.keys() if args.scenario == "all" else [args.scenario]
        for key in selected:
            name, command = scenarios[key]
            run_scenario(name, command, args.duration, receiver, transmitter)

        receiver.send("S")
        transmitter.send("?")
        time.sleep(1.0)
    finally:
        receiver.close()
        transmitter.close()
    print("done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
