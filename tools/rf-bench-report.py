#!/usr/bin/env python3
"""Summarise a tools/rf-bench.py run from the two serial logs.

Usage:
    tools/rf-bench-report.py RECEIVER.log TRANSMITTER.log

The output is Markdown intended to be pasted into the handoff.
"""

from __future__ import annotations

import argparse
import pathlib
import re
from dataclasses import dataclass, field

HOST_RE = re.compile(r"^\s*([0-9.]+)\s+(.*)$")
SECTION_RE = re.compile(r"=====\s+(TEST\d)\s+.*?\s+(START|END)\s+=====")
RX_RE = re.compile(
    r"ch=(\d+) seq=(\d+) gap=(\d+) rssi=(-?\d+) msg=([A-Za-z0-9_-]+) "
    r"tx=(\d+) wind=([-0-9.]+)m/s dir=([-0-9.]+) battery_low=(\d) raw=([0-9A-F ]+) (.*)$"
)
RX_BAD_RE = re.compile(r"RX_BAD t=(\d+) ch=(\d+) seq=(\d+) rssi=(-?\d+) status=(\S+) raw=([0-9A-F ]+)")
HOP_RE = re.compile(r"HOP t=(\d+) from=(\d+) to=(\d+).*reason=(\S+) miss=(\d+) search=(\d)")
RESYNC_RE = re.compile(r"RESYNC t=(\d+) silence_ms=(\d+) ch=(\d+) seq=(-?\d+) rssi=(-?\d+)")
SEARCH_RE = re.compile(r"SEARCH t=(\d+) after (\d+) missed slots")
STATS_RE = re.compile(
    r"STATS t=(\d+) ch=(\d+) valid=(\d+) bad_crc=(\d+) gap_slots=(\d+) max_gap=(\d+) "
    r"resyncs=(\d+) rssi_avg=(-?\d+) rssi_min=(-?\d+) rssi_max=(-?\d+) search=(\d)"
)
TX_RE = re.compile(
    r"TX t=(\d+) ch=(\d+) seq=(\d+) slot=(\d+) msg=([A-Za-z0-9_-]+) raw=([0-9A-F ]+) tx_ms=(\d+)"
)
SKIP_RE = re.compile(r"SKIP t=(\d+) ch=(\d+) seq=(\d+) slot=(\d+) skip_left=(\d+)")
SKIP_PLAN_RE = re.compile(r"SKIP_PLAN next_skips=(\d+) cooldown=(\d+)")
RESTART_RE = re.compile(r"RESTART t=(\d+) next_ch=(\d+) next_seq=(\d+) after=(\d+)ms")
TX_STATS_RE = re.compile(
    r"TX_STATS t=(\d+) scenario=(\S+) running=(\d) slots=(\d+) tx=(\d+) skip=(\d+) "
    r"fail=(\d+) ch=(\d+) seq=(\d+)"
)


@dataclass
class Scenario:
    name: str
    rx_start: float | None = None
    rx_end: float | None = None
    tx_start: float | None = None
    tx_end: float | None = None
    rx: list[dict] = field(default_factory=list)
    rx_bad: list[dict] = field(default_factory=list)
    hops: list[dict] = field(default_factory=list)
    resyncs: list[dict] = field(default_factory=list)
    searches: list[dict] = field(default_factory=list)
    stats: list[dict] = field(default_factory=list)
    tx: list[dict] = field(default_factory=list)
    skips: list[dict] = field(default_factory=list)
    skip_plans: list[dict] = field(default_factory=list)
    restarts: list[dict] = field(default_factory=list)
    tx_stats: list[dict] = field(default_factory=list)

    def in_rx_window(self, host_time: float) -> bool:
        return (self.rx_start is None or host_time >= self.rx_start) and (
            self.rx_end is None or host_time <= self.rx_end
        )

    def in_tx_window(self, host_time: float) -> bool:
        return (self.tx_start is None or host_time >= self.tx_start) and (
            self.tx_end is None or host_time <= self.tx_end
        )


def parse_log(path: pathlib.Path) -> list[Scenario]:
    scenarios: dict[str, Scenario] = {}
    current: Scenario | None = None
    is_receiver = "receiver" in path.name

    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = HOST_RE.match(raw_line)
        if not match:
            continue
        host_time = float(match.group(1))
        line = match.group(2)

        section = SECTION_RE.search(line)
        if section:
            key, phase = section.group(1), section.group(2)
            scenario = scenarios.setdefault(key, Scenario(key))
            if is_receiver:
                if phase == "START":
                    scenario.rx_start = host_time
                else:
                    scenario.rx_end = host_time
            else:
                if phase == "START":
                    scenario.tx_start = host_time
                else:
                    scenario.tx_end = host_time
            current = scenario
            continue

        if current is None:
            continue

        if is_receiver:
            m = RX_RE.search(line)
            if m:
                current.rx.append(
                    dict(
                        host=host_time,
                        t=0,
                        ch=int(m.group(1)),
                        seq=int(m.group(2)),
                        gap=int(m.group(3)),
                        rssi=int(m.group(4)),
                        msg=m.group(5),
                        raw=m.group(10).strip(),
                    )
                )
                continue
            m = RX_BAD_RE.search(line)
            if m:
                current.rx_bad.append(
                    dict(host=host_time, t=int(m.group(1)), ch=int(m.group(2)),
                         rssi=int(m.group(4)), status=m.group(5))
                )
                continue
            m = HOP_RE.search(line)
            if m:
                current.hops.append(
                    dict(host=host_time, t=int(m.group(1)), frm=int(m.group(2)),
                         to=int(m.group(3)), reason=m.group(4), miss=int(m.group(5)),
                         search=int(m.group(6)))
                )
                continue
            m = RESYNC_RE.search(line)
            if m:
                current.resyncs.append(
                    dict(host=host_time, t=int(m.group(1)), silence_ms=int(m.group(2)),
                         ch=int(m.group(3)), seq=int(m.group(4)), rssi=int(m.group(5)))
                )
                continue
            m = SEARCH_RE.search(line)
            if m:
                current.searches.append(dict(host=host_time, t=int(m.group(1)),
                                             misses=int(m.group(2))))
                continue
            m = STATS_RE.search(line)
            if m:
                current.stats.append(
                    dict(
                        host=host_time,
                        t=int(m.group(1)),
                        ch=int(m.group(2)),
                        valid=int(m.group(3)),
                        bad_crc=int(m.group(4)),
                        gap_slots=int(m.group(5)),
                        max_gap=int(m.group(6)),
                        resyncs=int(m.group(7)),
                        rssi_avg=int(m.group(8)),
                        rssi_min=int(m.group(9)),
                        rssi_max=int(m.group(10)),
                        search=int(m.group(11)),
                    )
                )
                continue
        else:
            m = TX_RE.search(line)
            if m:
                current.tx.append(
                    dict(host=host_time, t=int(m.group(1)), ch=int(m.group(2)),
                         seq=int(m.group(3)), slot=int(m.group(4)), msg=m.group(5),
                         raw=m.group(6).strip(), tx_ms=int(m.group(7)))
                )
                continue
            m = SKIP_RE.search(line)
            if m:
                current.skips.append(
                    dict(host=host_time, t=int(m.group(1)), ch=int(m.group(2)),
                         seq=int(m.group(3)), slot=int(m.group(4)))
                )
                continue
            m = SKIP_PLAN_RE.search(line)
            if m:
                current.skip_plans.append(dict(host=host_time, skips=int(m.group(1))))
                continue
            m = RESTART_RE.search(line)
            if m:
                current.restarts.append(dict(host=host_time, t=int(m.group(1)),
                                             after_ms=int(m.group(4))))

    return [scenarios[key] for key in sorted(scenarios)]


def pct(part: int, total: int) -> str:
    return f"{(100.0 * part / total):.1f}%" if total else "n/a"


def summarise(receiver: pathlib.Path, transmitter: pathlib.Path) -> str:
    scenarios = {s.name: s for s in parse_log(receiver)}
    for scenario in parse_log(transmitter):
        if scenario.name in scenarios:
            target = scenarios[scenario.name]
            target.tx = scenario.tx
            target.skips = scenario.skips
            target.skip_plans = scenario.skip_plans
            target.restarts = scenario.restarts
            target.tx_stats = scenario.tx_stats
            target.tx_start = scenario.tx_start
            target.tx_end = scenario.tx_end

    lines: list[str] = []
    lines.append("# RF bench summary\n")
    lines.append(f"- receiver log: `{receiver}`")
    lines.append(f"- transmitter log: `{transmitter}`\n")
    lines.append(
        "| Test | Slots | TX | Skip | Valid | Bad CRC | Gap slots | Max gap | Resyncs | "
        "Searches | Success | RSSI avg/min/max |"
    )
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |")

    for name in sorted(scenarios):
        s = scenarios[name]
        if not s.tx and not s.rx:
            continue
        if s.tx_stats:
            tx_count = s.tx_stats[-1]["tx"]
            skip_count = s.tx_stats[-1]["skip"]
            slots = s.tx_stats[-1]["slots"]
        else:
            tx_count = len(s.tx)
            skip_count = len(s.skips)
            slots = tx_count + skip_count
        valid = len(s.rx)
        rssi = [entry["rssi"] for entry in s.rx]
        avg = sum(rssi) / len(rssi) if rssi else 0
        rssi_text = f"{avg:.0f} / {min(rssi)} / {max(rssi)}" if rssi else "n/a"
        gap_slots = sum(entry["gap"] for entry in s.rx)
        max_gap = max((entry["gap"] for entry in s.rx), default=0)
        lines.append(
            f"| {name} | {slots} | {tx_count} | {skip_count} | {valid} | {len(s.rx_bad)} | "
            f"{gap_slots} | {max_gap} | {len(s.resyncs)} | {len(s.searches)} | "
            f"{pct(valid, tx_count)} | {rssi_text} dBm |"
        )

    lines.append("\n## Channel tracking\n")
    for name in sorted(scenarios):
        s = scenarios[name]
        if not s.rx or not s.tx:
            continue
        tx_sorted = sorted(s.tx, key=lambda entry: entry["host"])
        mismatches = []
        matched = 0
        for entry in s.rx:
            if not tx_sorted:
                break
            nearest = min(tx_sorted, key=lambda tx: abs(tx["host"] - entry["host"]))
            if abs(nearest["host"] - entry["host"]) <= 0.5:
                matched += 1
                if nearest["ch"] != entry["ch"]:
                    mismatches.append(entry)
        lines.append(
            f"- {name}: {len(s.rx)} valid frames, {matched} matched to a transmitter "
            f"slot within 0.5 s, {len(mismatches)} channel mismatches."
        )
        if name == "TEST2":
            lines.append(
                f"  - skipped slots {len(s.skips)}, receiver gap slots {sum(e['gap'] for e in s.rx)}, "
                f"skip plans {len(s.skip_plans)}."
            )

    lines.append("\n## Restarts and resync\n")
    for name in sorted(scenarios):
        s = scenarios[name]
        if not s.restarts:
            continue
        for index, restart in enumerate(s.restarts, start=1):
            after = [entry for entry in s.rx if entry["host"] > restart["host"]]
            catch = after[0] if after else None
            if catch is None:
                lines.append(f"- {name} restart {index}: no valid frame after restart.")
            else:
                lines.append(
                    f"- {name} restart {index} at host {restart['host']:.1f}s: first valid "
                    f"frame after {catch['host'] - restart['host']:.1f}s on channel "
                    f"{catch['ch']} seq {catch['seq']} (RSSI {catch['rssi']} dBm)."
                )
        for resync in s.resyncs:
            lines.append(
                f"- {name} RESYNC: silence {resync['silence_ms']/1000.0:.1f}s, "
                f"channel {resync['ch']}, seq {resync['seq']}."
            )

    lines.append("\n## Final statistics lines\n")
    for name in sorted(scenarios):
        s = scenarios[name]
        if not s.stats:
            continue
        last = s.stats[-1]
        lines.append(
            f"- {name}: valid={last['valid']} bad_crc={last['bad_crc']} "
            f"gap_slots={last['gap_slots']} max_gap={last['max_gap']} "
            f"resyncs={last['resyncs']} rssi_avg={last['rssi_avg']} "
            f"rssi_min={last['rssi_min']} rssi_max={last['rssi_max']} "
            f"search={last['search']}"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("receiver", type=pathlib.Path)
    parser.add_argument("transmitter", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    report = summarise(args.receiver, args.transmitter)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
