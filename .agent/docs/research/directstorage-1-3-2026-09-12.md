---
name: directstorage-1-3-2026-09-12
kind: doc
description: the DirectStorage runtime taken from 1.2.3 to 1.3.0, the forwarder and core split, the attempt that shipped and silently did nothing, what the changelog really offers this game, why merging the texture requests is impossible, and BypassIO measured to be live and to not matter
updated: 2026-09-12
links: [DEC-015-bundled-directstorage-core-loaded-first, dstorage-dll-is-only-a-forwarder, directstorage-streaming, proxy-architecture, TODO-014-proxy-implements-enqueuerequests-if-the-game-asks]
---

# DirectStorage 1.3.0

## What the versions are

Latest stable is 1.3.0 (June 2025). 1.4.0 exists but only as a preview, and it holds nothing for
this game: Zstd is a compression format the game does not use, and `DSTORAGE_CONFIGURATION2` adds
exactly one field over `DSTORAGE_CONFIGURATION1`, a `CreatorID` GUID that labels the D3D12 queues
DirectStorage creates so profiling tools can attribute them.

The SDK version number is minor and patch only, so 203 is 1.2.3, 204 is 1.2.4, 300 is 1.3.0 and
400 is 1.4.0. The game ships 1.2.3, byte identical to the NuGet package, both halves of it.

The header diff from 1.2.3 to 1.3.0 is purely additive. No existing struct changes size or layout,
and `dstorageerr.h` is identical. The proxy wraps the same ABI either way.

## The forwarder and the core

`dstorage.dll` is about 200 KB and holds no runtime. Each of its four exports resolves a matching
`...Core` symbol out of `dstoragecore.dll` and tail jumps to it, arguments untouched. The only one
that does any work is plain `DStorageSetConfiguration`, which copies the seven fields of the older
struct into the eight field one and zeroes `ForceFileBuffering` before calling the same core entry
point as `DStorageSetConfiguration1`.

How the forwarder finds the core, from its own disassembly:

1. `GetModuleHandleExW` on the running executable, then look for a `DStorageSDKVersion` data
   export on it. `AssettoCorsaEVO.exe` has none, so no minimum version is demanded.
2. `LoadLibraryExW(L"dstoragecore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)`, an inbox copy
   first. Windows 11 26200 has none.
3. On `ERROR_MOD_NOT_FOUND`, build a path from `GetModuleFileNameW(nullptr, ...)`, the game
   executable's own folder, and load from there. This finds the game's copy.
4. If no core loads at all, `DStorageGetFactory` returns `E_NOTIMPL`.

**Nothing checks the forwarder against the core.** Measured on all four pairings with a small
probe executable: a 1.3.0 forwarder on a 1.2.3 core returns `S_OK`, creates queues, offers no
`IDStorageQueue3`, reports no error anywhere and runs the old code. So shipping only the newer
`dstorage_orig.dll`, which is the obvious reading of "upgrade the runtime", is a silent no-op.

## The attempt that failed

The first design put our core in `acevo_perf\dstoragecore.dll` and loaded it by full path before
the forwarder went looking, on the theory that the forwarder's bare name `LoadLibrary` would then
be handed the module already loaded under that name. That behaviour is real and was measured, both
against a copy next to the executable and against `LOAD_LIBRARY_SEARCH_SYSTEM32`.

It shipped, installed and ran, and the log said `DirectStorage 1.2.3 in use`.

`AssettoCorsaEVO.exe` carries a UTF-16 `dstoragecore.dll` string and loads its own copy during
start-up, three seconds before it calls any export of ours. A module's base name is its identity,
so the name was already taken, and `LoadLibraryW` on our full path silently returned the game's
module. Success, no error, wrong runtime. Reproduced exactly in the probe by loading the game's
core first.

The only thing that caught it was reading `DStorageSDKVersion` back off the module that really
loaded and writing it to the log. That check was added on the guess that a mismatch would be
silent, and it caught a real one on its first outing.

## What ships instead

The core ships as `acevo_dstoragecore.dll`, a name nothing else in the process asks for, and the
proxy resolves `DStorageGetFactoryCore`, `DStorageSetConfigurationCore` and
`DStorageCreateCompressionCodecCore` on it and calls them itself, doing the one struct widening
the forwarder would have done. `dstorage_orig.dll` stays in the payload as the fallback, used when
`bundled_runtime=0`, when the file is missing, or when those entry points are not there.

The core needs nothing from the forwarder. It imports WINMM, dxgi, ntdll, kernel32, d3d12 and
oleaut32. Two runtimes resident at once is fine, only ours is ever initialised.

Verified with the game's 1.2.3 core deliberately loaded first and the queue wrapper turned off so
the probe sees the real queue: it answers to `IDStorageQueue3`, so the factory really does come
from 1.3.0.

## What the upgrade is actually worth

Microsoft ships the changelog inside the NuGet package, in `README.md`. Read line by line against
what this game does, one entry lands:

- **`DSTORAGE_DESTINATION_TILES` fixed for resources whose width and height differ** (1.3). This
  is the texture tile queue. The session below put 13639 tile requests and 11 GB through it.
- CPU decompression failing corrupted GDeflate properly (1.3), and the GPU decompression shader
  rebuilt for HLSL 2021 to stop TDRs on older cards (1.2.4): both compression paths, and every
  queue in every session here reads `compressed=0 gdeflate=0`. The game streams uncompressed.
- `IDStorageQueue3`, `EnqueueRequests` and `DSTORAGE_DESTINATION_MULTIPLE_SUBRESOURCES_RANGE`
  (1.3): functions for a title to call, and this title is built against 1.2.

Worth recording because it was got wrong first: the race in the file IO stack when several threads
submit reads, and the deadlock in the built in decompression threadpool, are **1.2.3** fixes. The
game already had them. A web summary attributed the race to 1.2.4 and that went into the changelog
and the readme before the package's own notes were read. Read the package, not the coverage.

## The measurement

One session on 2026-09-12, `logs/ds130-20260912-1403`: start, a lap, back to the main menu, change
to the full Nordschleife, a second lap, back to the menu, quit. 12.7 minutes,
`DirectStorage 1.3.0 in use` confirmed in the log, no errors, no fallbacks, no failed calls.

Streaming throughput against three earlier sessions on the same machine and settings:

| session | runtime | FileToMemory peak | tile queue peak |
|---|---|---|---|
| loadsampler 11:28 | 1.2.3 | 258.2 MB/s | 71.6 MB/s |
| reflex A2 12:50 | 1.2.3 | 275.4 MB/s | 83.8 MB/s |
| reflex C 12:44 | 1.2.3 | 291.1 MB/s | 90.9 MB/s |
| **this one 13:50** | **1.3.0** | **260.7 MB/s** | **86.0 MB/s** |

Inside the spread of the 1.2.3 runs, on both queues.

Hitches by minute, which is the shape that matters more than the total:

| session | distribution |
|---|---|
| reflex A2 | 18, 35 at start, then 1, 2 |
| reflex C | 43 at start, then 2 |
| reflex boost on | 61 at start, 1, then 4, 3 at the end |
| this one | 47 at start, 60 at the Nordschleife load, **1 across nine minutes of driving**, 20 at the menu and quit |

Same shape as every earlier session. Hitches belong to start-up, track loads and menu transitions,
which are BUG-012 and BUG-014 and the job queue spin loop of TODO-013, none of which a storage
runtime can reach. Driving is clean before and after.

## Could the proxy merge texture requests

The one idea the new API surface suggested, and it is dead. Worth writing down with its numbers so
nobody spends another session on it.

The game sends its textures as single subresource `DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION`
requests, 32201 of them in one Nürburgring load alongside 10137 buffer requests. DirectStorage can
take a range of subresources in one request instead, through `MULTIPLE_SUBRESOURCES` (1.2, runs to
the end) or `MULTIPLE_SUBRESOURCES_RANGE` (1.3, takes a count). Both documents say the same
condition: "the source is expected to contain full data for all subresources, starting from
FirstSubresource". So consecutive requests may only merge when they share a resource, step the
subresource index by exactly one, carry straight on in the source, and have no submit, status entry
or fence signal between them.

A counting only survey behind `[profile] merge_survey=1` was built, tested against patterns worked
out by hand, checked against two deliberately broken builds of itself, and run for one load on
2026-09-12, `logs/mergesurvey-20260912-1417`. It accounted for every request, 32201 plus 10137
against the 42338 the queue statistics report.

**Zero of 32201 could merge. The longest run was one.**

| why a run ended | count | share |
|---|---|---|
| the next request names a different resource | 20880 | 65% |
| same resource and the next subresource, but a gap in the source | 9042 | 28% |
| a buffer request came in between | about 2116 | 7% |
| same resource, subresource index jumped | 163 | 0.5% |
| a submit, status or signal | 0 | none |

Two structural facts, either of which alone kills the idea. The game interleaves textures from many
different resources rather than finishing one before starting the next, which accounts for two
thirds of the breaks on its own. And where it does send consecutive mips of one texture, 9042
times, their source buffers are separate allocations rather than one block, so the bytes are not
back to back. Making them contiguous would mean copying, and copying 6.8 GB to save request count
costs far more than the requests do.

Note what the zero in the last row means: barriers never broke a single run. The game's ordering was
never the obstacle. Its memory layout is, and a proxy cannot change that.

The survey came out again after answering the question. It is in the history at commit `317baaa` if
the game's allocation behaviour ever changes.

## Is BypassIO actually engaging

This sat on the optimisation list as "no API exists, would need capturing the runtime's debug
output". Both halves of that are true and the second is worse than it sounds. The 1.3.0 runtime
contains no BypassIO diagnostic strings at all, in either encoding, and the only thing it carries
is the ETW provider name `Microsoft.Windows.Graphics.DirectStorage`. The debug layer flags
(`DSTORAGE_DEBUG_SHOW_ERRORS` and friends) print to a debugger, and nothing in the header asks the
runtime whether BypassIO took. So learning the flag means a private in process ETW session plus TDH
decoding, a few hundred lines, to be told a boolean.

The better question needs no code, because `disable_bypass_io` already exists: is BypassIO worth
anything here? Two Nürburgring loads, same build, same track, one line of ini between them:

| | peak tile queue | peak file to memory | Nürburgring load |
|---|---|---|---|
| BypassIO on, 15:37 | **600.8 MB/s** | 897.7 MB/s | 16.51 s |
| BypassIO off, 15:44 | **391.2 MB/s** | 850.0 MB/s | 15.68 s |

Two things follow, and they point in opposite directions, which is why this is worth writing down.

**It is engaging.** Turning it off costs 35 percent of the tile queue's peak, and the shape of the
traffic changes with it: on, the top seconds run 601, 477, 148, 65, a sharp burst that empties
quickly, off they run 391, 378, 287, 202, flatter and longer. A switch that does nothing does not
move numbers like that, so the path is live on this machine and no verification work is owed.

**And it does not matter to a load.** The load with BypassIO off came out 0.8 s faster, which is
inside the spread already measured for that same track on that same day (15.68, 16.10, 16.51,
16.66 s). That is not a claim that switching it off helps, it is a measurement that the difference
is invisible against the noise, and it agrees with everything else here: a session load is not
drive bound, the drive is about a second of a sixteen second load, and the time goes to the job
queue and to streaming the engine has not asked for yet (TODO-013, BUG-018).

So BypassIO is closed, on evidence, without writing the ETW consumer. It works, and the thing it
makes faster is not the thing that is slow.

## Reading

The runtime is genuinely upgraded and nothing regressed. Nothing improved either, which is what
the changelog predicted before the lap was driven. It ships because being on the current stable
runtime is right, it costs 1.4 MB and no game file, and the one fix that does touch this game sits
on the texture tile path. It is not a performance change and must not be described as one.
