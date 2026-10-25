# AkashvaniDisplay

The application renders a `DisplayState`; it never includes a panel driver.
Today the receiver can use `NullDisplay`, so a missing or unsupported panel does
not block data collection. The 1" LCD and the future 3.5" touch panel get
separate implementations behind `IDisplay`.

Keep the state model UI-agnostic: strings and simple status values, no fonts,
colours or coordinates. That lets a larger touch UI reuse the same state while
adding pages and controls.

Planned implementations:

- `St7735Display`: 1" 80x160 panel over shared SPI. A stub is present and
  returns `Unsupported`; enable it with `config::kEnableDisplay`.
- `TouchPanelDisplay`: 3.5" panel with local config pages, backed by the same
  HTTP settings service as the captive portal.
