# AkashvaniRadio

Radio hardware is behind `IRadio` so the application never talks to SPI,
registers or a vendor library directly.

```text
sketch/app ──> IRadio ──> Rfm69Radio   (current bench hardware)
                    └───> Rfm95Radio   (alternate seam, still a stub)
                    └───> MockRadio    (native tests / bench)
```

## RFM69 / Davis

`Rfm69Radio` implements the SX1231 packet path validated in
`prototype/lib2/DavisRFM69`:

- 19.2 kbps FSK, 4.8 kHz deviation, Gaussian BT=0.5 shaping;
- `CB 89` sync, fixed eight-byte payload, hardware CRC off;
- 51-entry US Davis frequency table (`DavisChannels.h`);
- bit reversal on TX/RX, RSSI sampling, FIFO access and channel selection.

The driver writes the complete configuration on every boot. This is required on
the USB1 simulator board because its RFM69 RESET pin is not connected and the
chip retains its previous mode and registers across MCU resets. The SX1231 RESET
line is active high; drive it low for normal operation.

Register values are intentionally centralised in `Rfm69Radio.cpp`. Do not change
them without repeating the packet-success tests in
`docs/specs/rf-davis-receive.md`.

## RFM95

`Rfm95Radio::begin()` still checks the SPI wiring and version register and
returns `Status::Unsupported`. The RFM95 seam is retained for the alternate
module, but the bench path is RFM69.

Davis frames are decoded in `AkashvaniDavis`, never in this library.
