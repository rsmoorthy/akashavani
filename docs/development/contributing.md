# Contributing

## Workflow

```bash
git switch -c feat/short-description
make setup
make fw-build-all
make test
make lint
```

Before opening a pull request:

- [ ] native firmware tests pass;
- [ ] both sketches compile;
- [ ] server tests, Ruff and mypy pass;
- [ ] protocol schemas/examples are updated if the wire format changed;
- [ ] docs and pin maps are updated for hardware/behaviour changes;
- [ ] no credentials, site names or captured private data are committed;
- [ ] new TODOs reference an issue or an ADR.

## Code style

### C++

- C++17, no exceptions/RTTI, no dynamic allocation in the receive path.
- 2-space indent, 100-column limit; see `.clang-format`.
- `akashvani` namespace; files under a library's `src/`.
- Drivers return `Status`; reserve `bool` for predicates.
- Keep host-safe files free of `Arduino.h`.
- Fixed-size buffers for anything that runs per frame.

### Python

- Ruff format + lint, mypy on `src/`.
- Type annotations on public functions.
- Pydantic models for all request/response bodies.
- Routers thin; move reusable logic into `services/`.
- Tests use the real app lifespan and a temporary database.

### Documentation

- Prefer concrete commands and file paths.
- Mark unverified hardware assumptions explicitly.
- Update the ADR table when making a structural decision.

## Adding a firmware library

1. Create `firmware/libraries/AkashvaniXxx/` with `library.properties`, `README.md`
   and `src/`.
2. Keep the dependency direction acyclic; do not make `AkashvaniDavis` depend on
   Wi-Fi.
3. Put pure logic in files listed in `firmware/tests/native/CMakeLists.txt` and
   add tests.
4. Add an umbrella header if the component exposes several types.
5. Wire it through a sketch's `app.cpp`, not directly from the `.ino`.

## Changing the protocol

1. Change the JSON Schema in `protocol/schema/`.
2. Update the example and the schema test.
3. Update `server/src/akashvani_server/schemas.py`.
4. Update `firmware/libraries/AkashvaniProtocol` if constants/validators change.
5. Update `ObservationJson` or the API client if fields move.
6. Decide: is this additive (same `v`) or breaking (new `v`)?
7. Update `docs/protocol/README.md`.

Never silently change a unit. `temperature_c` means Celsius, not Fahrenheit,
forever.

## Secrets

- Never commit real Wi-Fi credentials, device tokens, admin tokens or site
  details.
- Use `server/.env` (git-ignored), NVS on the device, or environment variables.
- Use obviously fake tokens in tests and examples.
- If a credential is committed, rotate it; deleting the commit is not enough.

## Hardware changes

Record:

- board revision and module variant;
- exact pin map and strapping-pin considerations;
- power rails and current measurements;
- antenna type and legal frequency/power;
- bring-up sketch output;
- any deviations from `docs/hardware/pinmap.md`.

## Review checklist

Reviewers should ask:

- Does this work when the cloud is down? When the SD card is missing?
- Does it block the loop for an unbounded time?
- Is the observation still idempotent across retries?
- Does a failed OTA leave a bootable image?
- Are secrets absent from logs and archives?
- Is the unchanged path still covered by tests?
