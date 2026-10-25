# AkashvaniRadio

Radio hardware is behind `IRadio` so the application never talks to SPI,
registers or a vendor library directly.

```text
sketch/app ──> IRadio ──> Rfm95Radio   (on-target)
                    └───> MockRadio    (native tests / bench)
                    └───> Rfm69Radio   (planned)
```

## Porting note

`Rfm95Radio::begin()` currently checks the SPI wiring and the RFM95 version
register, then returns `Status::Unsupported`. Replace the bodies marked
`TODO(port)` with the proven prototype implementation. Keep the register-level
details in this library, not in a sketch.

Davis reception may require raw/direct mode rather than packet mode. If so,
extend `IRadio` with a raw-receive method rather than leaking RFM95 registers
into `AkashvaniDavis`.

## Planned RFM69 support

Add `Rfm69Radio` implementing the same interface. A stub already exists and
returns `Status::Unsupported`; port the SX1231 configuration there. The board
configuration is selected per sketch through `RadioConfig::chip`; the Davis
decoder must not know which radio produced a frame.
