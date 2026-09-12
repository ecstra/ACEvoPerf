---
name: TODO-013-faster-session-loads
kind: todo
description: cut the session load, 17 s on the Nurburgring with three quarters of it in one streaming phase that issues tens of thousands of 30 KB requests at a fifth of the drive's speed, by reading ahead in the proxy and serving those requests from memory
updated: 2026-09-12
links: [directstorage-streaming, telemetry, one-percent-low-hunt-2026-09-05, content-package]
status: open
by: owner
area: streaming
born: 2026-09-06
done:
---

## What

Owner wording, 2026-09-06: "Faster Session: 100% we can do this."

## Where it stands

The game's own loading profiler (the `[LoadingProfiler]` lines of its log) on the release
session of 2026-09-06 13:11: the Nürburgring 24h practice loaded in 15.05 s. The critical path
was the online handshake (0.9 s), the physics scene (1.6 s), the world initialisation on the
game thread (1.9 s) and then track resources streaming, 10.8 s for 1528 meshes, 1237 textures
and 439 instance sets, with the player car's physics (5.9 s) and graphics (2.0 s) loading in
parallel and finishing last. During the streaming stretch the mod's timeline shows the
FileToMemory queue averaging 260 MB/s with one second peaks of 900 MB/s, on a Samsung PM991a
NVMe good for about 3 GB/s, and the process CPU at 40 to 80 percent across 16 logical
processors while the game had boosted its loading pool from 2 to 11 workers. So the load is
neither disk bound nor idle: the request and processing pattern is the limit.

Every file the load pulls to memory is XOR ciphered in the package (the `.texturemips` tiles
are plain), so the game deciphers about 4 GB per session load on the CPU. The decipher loop
was not found yet, the key is stored as a qword in a package reader object.

## Measured again on 0.9.1, 2026-09-11

The owner's session of 2026-09-11 loaded the Nurburgring three times. The shape has not moved
and the numbers are a little worse than the 0.9.0 figures above:

| load | total | track resources streaming | share |
|---|---|---|---|
| 19:48 | 16.92 s | 12.61 s | 74.5% |
| 19:55 | 17.00 s | 12.64 s | 74.4% |
| 20:29 | 17.68 s | 13.14 s | 74.3% |

Everything else is small and fixed: the online handshake 0.72 s, the physics scene 1.6 s, the
world initialisation 1.9 s, committing meshes 1.6 s. Three quarters of a session load is one
phase, `Track resources streaming`, and that phase is the whole target.

The mod's own queue statistics during that phase name the limit. Over the ten seconds of the
19:48 load:

| queue | requests | volume | rate | average request |
|---|---|---|---|---|
| FileToMemory | 8972 | 2.7 GB | 269 MB/s | 30 KB |
| GpuUpload Memory | 25885 | 1.5 GB | 147 MB/s | 5.8 KB |
| GpuUpload File (tiles) | 1001 | 0.5 GB | 52 MB/s | 53 KB |

About 470 MB/s in total from a Samsung PM991a that does around 3 GB/s, with the CPU at 40 to
80 percent and nothing saturated. The game issues tens of thousands of requests averaging
30 KB and 5.8 KB, so the cost is per request, not per byte. That is why the disk looks idle
and the load still takes 13 seconds.

This supersedes step 1 below. The sampler build is no longer needed to find where the time
goes, the queue statistics already say it.

## Readahead was built on 2026-09-12 and thrown away

The plan was to read the package in big blocks on a worker thread and answer the game's small
requests from that memory. It was written, it compiled clean, and it was deleted the same hour
for two independent reasons. Neither is worth rediscovering.

**DirectStorage will not accept it.** A queue is created with a source type and, in the words
of the header, it is "the source type of requests that this DirectStorage queue can accept".
The two queues that reach the drive are both created with `DSTORAGE_REQUEST_SOURCE_FILE`:

```
CreateQueue name='FileToMemory Queue'    source=FILE
CreateQueue name='GpuUpload File Queue'  source=FILE
CreateQueue name='GpuUpload Memory Queue' source=MEMORY
```

So a request on either of them cannot be rewritten to a memory source. The one queue that
takes memory sources is already fed from the game's own memory and never touches the drive.
Serving cached bytes into the loading path is not available through this API.

**The ceiling was about a second anyway.** The load moves roughly 3.3 GB. At the drive's
3 GB/s that is 1.1 seconds of reading inside a phase that takes 12.6. Per resource the
arithmetic is worse for the idea: 3204 resources across 11 workers is 291 each, so a worker
spends about 43 ms on every resource it handles, where its share of the reading is under a
millisecond. Even a cache that answered every request instantly could not have reached the
third this todo asks for.

The page cache variant, reading the package into the Windows cache so the game's own reads hit
memory, needs `disable_bypass_io=1` to have any effect at all, because BypassIO skips the file
system cache. It trades away the fast path to maybe recover part of one second. Not worth it
without evidence that the reads matter, and the numbers above say they do not.

## Steps

1. The sampler build is back on, and it is the only honest next step. Bring the sampler from
   commit `a119e8c` back for the Resource Manager Workers, sample one Nurburgring load, and
   name what those 43 ms per resource are. The queue statistics ruled the drive out, they say
   nothing about what the workers do with the bytes once they have them.
2. Only then decide whether the mod has a lever at all. The honest prior after the numbers
   above is that it may not, and that this closes the way BUG-012 and BUG-014 did, as the
   engine's own cost on every card.

Not the decipher either. The 4 GB of XOR per load was the other suspect and it does not fit:
it is memory bandwidth work spread over eleven workers, and the CPU never saturates.

## Done when

The Nürburgring session load, measured by the game's own loading profiler line, drops by a
third or more with the mod installed, and the cause of the remaining time is named.
