# 0001 — Monorepo for firmware, protocol and server

## Context

Firmware, simulator, server and wire contracts change together. The project is
small, has one maintainer/team, and benefits from a single review and CI run.

## Decision

Keep firmware, protocol schemas, server, docs and tooling in one repository with
clear top-level boundaries.

## Consequences

- Protocol changes are reviewed in one place; firmware and server tests can use
  the same schemas/examples.
- CI runs all layers, at the cost of a longer pipeline.
- Versioning is by release tags; the repo is not split into separately versioned
  packages yet.
- A future independent server deployment can still be built from `server/`
  without splitting the repository.
