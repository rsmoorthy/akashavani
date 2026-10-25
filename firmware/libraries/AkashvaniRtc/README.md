# AkashvaniRtc

Time is safety-critical: an observation with a wrong timestamp is worse than no
observation. The receiver therefore treats "time not valid" as a hard reason not
to upload, while still archiving locally with the best timestamp available.

```text
DS3231 (battery-backed) ──┐
                          ├──> TimeService ──> epoch seconds
SNTP (when Wi-Fi up) ─────┘
```

- `IClock` / `FixedClock`: host-safe.
- `Ds3231Clock`: I2C driver using Adafruit RTClib.
- `TimeService`: seeds system time from the RTC, syncs via SNTP when online, and
  writes network time back to the RTC.

The RTC is not authoritative after an SNTP sync has succeeded; the network is.
`TimeService::nowEpoch()` exposes the selected value and `source()` reports it for
the local status page.
