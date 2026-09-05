---
title: Set engine flags by writing FLAGS_ storage found through a code scan, not via the command line
date: 2026-09-05
status: accepted
---

## Context

The engine declares 216 gflags but the release build only parses a small whitelist of single dash
switches. Injecting `--name=value` into the command line (through the CRT's command line copy and
IAT hooks on `GetCommandLineA/W`) was implemented and verified to reach `argv`, yet no flag took
effect.

## Decision

Scan the game's `.text` at start for the `FlagRegisterer` call sites (`lea r9,[__FILE__]`, the
storage pointers stored at `[rsp+20h]` and `[rsp+28h]`, the constructor call), recover name and
type for each flag, and write the requested values straight into the storage. Done twice, at
attach and at first DirectStorage use, so both early and late readers see the value.

## Alternatives

- Command line injection: removed, see context.
- Hardcoded addresses for this build: breaks on every game update.
- Patching the whitelist parser: fragile and invasive.

## Consequences

- Any bool, int32 or double flag works. String flags are not supported (they are `std::string`
  objects with non trivial layout).
- A codegen change in a future build can make the scan find nothing. The log reports the count so
  this is visible immediately.
