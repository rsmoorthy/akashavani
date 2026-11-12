# AkashvaniDisplay

The application renders a `DisplayState`; it never includes a panel driver.
A missing or unsupported panel does not block data collection: `begin()` returns
`Unsupported` and the caller keeps `NullDisplay` semantics (or simply checks the
status).

Keep the state model UI-agnostic: strings and simple status values, no fonts,
colours or coordinates. That lets a larger touch UI reuse the same state while
adding pages and controls.

Implementations:

- `Ssd1306Display`: 128x64 monochrome SSD1306-class I²C panel (the bench
  hardware, address `0x3C` on SDA 21 / SCL 22). Uses Adafruit SSD1306/GFX.
  Layout for the 128x64 two-colour ("Y-B") panel: the top 16 px yellow band is
  an inverted `Akashvani` title; the blue area holds `WiFi <SSID>` (or
  `AP <SSID>` when the station link is down), IP/RSSI, the three application
  lines and a `T R S` status row. Enable with `config::kEnableDisplay` and set
  `config::kDisplayAddress`.
- `St7735Display`: 1" 80x160 panel over shared SPI, still a stub that returns
  `Unsupported`. Kept as the production-panel seam.
- `TouchPanelDisplay`: 3.5" panel with local config pages, backed by the same
  HTTP settings service as the captive portal (not started).

The SSD1306 driver renders the whole frame and pushes it over I²C (about 1 KiB),
so call it on a slow cadence (the sketches use 2 s), never per RF frame.
