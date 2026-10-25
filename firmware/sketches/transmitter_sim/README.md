# Transmitter simulator sketch

Bench transmitter for testing the receiver before a real Davis station is
available. It generates bounded, deterministic observations with
`DavisSimulator` and sends them through the same radio abstraction as the
receiver.

```bash
make fw-build BOARD=transmitter_sim
make fw-flash BOARD=transmitter_sim PORT=/dev/ttyACM0
```

## Current state

`DavisEncoder` and `Rfm95Radio::transmit()` are stubs, so the board currently
logs observations and reports that encoding/transmission is not ported. Once the
validated Davis encoding is added to `AkashvaniDavis`, the simulator becomes a
real RF source without any change to this sketch.

`config::kDryRun` keeps the observation log enabled regardless of encoder state.
Set `kTransmitIntervalMs` to speed up or slow down the scenario.

The simulator deliberately has no Wi-Fi, SD card or display: it is the minimal
"not all components present" board from the project description.
