---
name: DEC-002-flags-by-memory-write
kind: decision
description: engine flags are set by writing FLAGS_ storage found through a code scan, not via the command line
updated: 2026-09-05
links: [engine-flags, release-build-ignores-gflags-cli]
date: 2026-09-05
area: engine-flags
status: standing
superseded-by:
---

## Decision

At attach and again at first DirectStorage use, scan the game's `.text` for the `FlagRegisterer`
call sites (the `lea r9,[__FILE__]` load, the two storage pointers stored at `[rsp+20h]` and
`[rsp+28h]`, the constructor call), recover name and type per flag, and write the values from the
ini straight into the storage (`ScanFlags` and `ApplyFlags` in `src/dllmain.cpp`).

## Alternatives

- Command line injection: implemented first (CRT command line copy plus import hooks on
  `GetCommandLineA/W`), verified to reach `argv`, and useless because the release build only
  parses a whitelist of single dash switches. Removed.
- Hardcoded addresses for this build: break on every game update.
- Patching the whitelist parser: fragile and invasive.

## Consequences

- Any bool, int32 or double flag works. String flags are not supported, they are `std::string`
  objects.
- A codegen change in a future build can make the scan find nothing. The log prints the count of
  flags found so this is visible at once.
