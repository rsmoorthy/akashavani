# 0002 — arduino-cli with local Arduino libraries

## Context

The firmware is Arduino-based and targets ESP32-S3. We need reproducible builds,
shared component libraries and two sketches, without committing generated IDE
state or installing project libraries globally.

## Decision

- Use `arduino-cli` as the only build driver.
- Keep reusable code in standard Arduino libraries under
  `firmware/libraries/`.
- Pass them at build time with `--libraries firmware/libraries`.
- Pin the ESP32 core and external libraries in `tools/fw-setup.sh`.
- Select the FQBN explicitly in `tools/fw-build.sh`; keep
  `sketch.yaml` as IDE metadata with `default_fqbn` only.
- Use `build.defines` for the firmware version define.

We do **not** use sketch build profiles: arduino-cli rejects `--libraries`
together with `--profile`, and profiles only resolve Library Manager libraries,
not local path libraries.

## Consequences

- A checkout is self-contained; no global library installation is needed beyond
  pinned external dependencies.
- IDE users must add the same local library path or use the Make targets; the
  CLI is the canonical build.
- FQBN defaults are duplicated between `sketch.yaml` and `tools/fw-common.sh`.
  If they drift, the CLI wins; update both when changing boards.
- Overriding `build.extra_flags` is dangerous because it replaces core flags.
  Additive defines must use `build.defines`.
