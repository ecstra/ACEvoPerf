---
name: DEC-016-agility-sdk-tried-and-removed
kind: decision
description: the Agility SDK was built, made to work by adding D3D12SDKVersion and D3D12SDKPath to the running exe, measured six ways, found to change nothing on any of them, and removed from the mod rather than shipped as a knob
updated: 2026-09-12
links: [DEC-015-bundled-directstorage-core-loaded-first, DEC-002-flags-by-memory-write, DEC-001-dstorage-proxy-as-loader, BUG-016-vram-overhead-grows-across-scene-loads, optimisation-deepdive-2026-09-12]
date: 2026-09-12
area: render
status: standing
superseded-by:
---

## Decision

The DirectX 12 Agility SDK was built into the mod, made to work, measured, and then **removed
entirely**. No code, no bundled runtime, no ini key. This file is the whole record, so nobody
spends the day on it twice.

It was worth doing. The deep dive had killed it on reasoning that turned out to be wrong, and
the owner asked for the answer rather than the guess. The answer is that it changes nothing on
this game.

## What was built, in case it is ever needed again

Windows ships one D3D12 runtime for the whole machine. A program opts into a newer one by
exporting `D3D12SDKVersion`, a UINT, and `D3D12SDKPath`, a pointer to a path relative to the exe.
This game exports neither.

The other route, `ID3D12SDKConfiguration::SetSDKVersion`, is documented as developer mode only,
and `d3d12.dll` carries the refusal string itself: `D3D12: SetSDKVersion is only available when
developer mode is enabled`. That is a Windows setting, so it is out of bounds for anything
shipped to users.

So the working route is to **add the two exports to the running executable's export table**
before its entry point. `d3d12.dll` reads them with `GetProcAddress` on the main module, so what
matters is that the table answers, not how the entries got there. Every RVA in an export table is
relative to the module base, so the new tables have to live inside the exe's own image, and the
tail padding of a section is the only unused space that qualifies. On this build that is `.data`,
3312 bytes of loader zero fill against the 694 the tables needed.

Two details that made it work:

- **Read the version out of the shipped core**, do not write it down. `d3d12.dll` refuses a core
  whose version is not the one the exe asked for: `D3D12SDKVersion(%u) from D3D12Core !=
  requested D3D12SDKVersion(%u)`. Mapping the core with `LOAD_LIBRARY_AS_IMAGE_RESOURCE` gives
  image layout without running any of its code.
- **Verify through `GetProcAddress` and roll back if it fails.** A malformed export table breaks
  name resolution for the whole process, so the code asked the way `d3d12.dll` would ask and put
  the original data directory back if either value did not read.

It worked first time in the game: `D3D12Core.dll` 1.619.5 loaded from the mod's folder, every
session clean. It also settled a question that could not be answered without running it:
`d3d12.dll` reads those exports **lazily, at first device creation**, not during its own
initialisation, so a proxy DLL is early enough.

## The lesson worth keeping, which cost the most to learn

The first version chose the largest section padding it could find and made it writable. In a
throwaway test exe that was `.text`.

**`VirtualProtect` works on whole pages.** Making padding inside a code section writable takes
execute permission away from the code sharing that page, which is the code doing the writing. It
faults on the next instruction fetch, at an address equal to the instruction pointer.

That would have been a startup crash on a user's machine. It was caught only because the
technique was tested on a throwaway exe that patched its own export table before it went
anywhere near the game. Anything in this project that writes into a mapped image inherits the
rule: never choose an executable section, prefer writable data, put the protection back.

## Why it was measured to nothing

The game's own code reaches `ID3D12Device12` and stops. Counting only IIDs that code takes the
address of, rather than ones the linker dragged in: `Device`, `Device1`, `Device2`, `Device4`,
`Device8`, `Device10`, `Device12` and `GraphicsCommandList5`. `Device3, 5, 6, 7, 9, 11, 13, 14`
and `CommandList6, 7, 8` sit in the binary with **zero** references.

The reference machine's inbox runtime is `10.0.26100.9278` on Windows build 26200, which already
provides `Device10` and `Device12`. There was nothing left for a newer core to hand it.

Six measurements, all negative:

| test | result |
|---|---|
| 2 parked runs against 3 controls | treatment runs differed from each other by more than from the controls |
| Nürburgring driving, 42k frames each way | every percentile p1 to p99 within 0.6 percent |
| Red Bull Ring hotlaps, 52k against 44k frames | saturated end within 0.8 percent, light end 5.5 percent the other way |
| 28 minute session, six scene loads | reproduced BUG-016's overhead curve, 328 spike against the control's 360 |
| video memory | 4568 MB peak against 4590 for the control |
| 1 percent lows | no consistent direction |

The driving sessions exist because the owner raised the right objection: a parked view samples one
scene load level, so an effect confined to saturated frames would be invisible to it. Comparing
whole distributions rather than means is what answers that, and the distributions lie on top of
each other.

## Alternatives, and why removal beat shipping it off by default

Keeping it as an off by default option was the plan for about an hour. It was dropped because a
knob that does nothing is not free:

- It hands a user a **different D3D12 runtime from the one their Windows shipped with**, tested
  against exactly one GPU, one driver and one Windows build.
- It patches the exe's export table, which is a real risk surface on machines nobody has tried.
- It adds 5 MB to a download for a measured zero.
- Every option in an ini is something a user can get wrong and something the next person has to
  read past.

The one population it would genuinely have served is a user on an older Windows whose inbox
runtime predates `Device10` and `Device12`, where the game's query fails and it falls back. That
is real but unmeasured, unreachable for testing, and speculative enough not to carry the other
three costs.

If a future game build starts asking for interfaces past `Device12`, this file has the recipe.

## Consequences

- One of the nine findings the deep dive's kills got wrong is now closed properly: the kill's
  **reasoning** was wrong, its **conclusion** was right.
- BUG-016 gains a ruled out cause. Swapping the whole D3D12 runtime changed nothing, which makes
  the runtime's own allocator a weaker candidate for that overhead than it was.
- The IID reference census is a technique worth reusing: it separates what a binary links from
  what it actually calls, and it turned a guess about the game's D3D12 surface into a fact.
