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

## What the sampler found, 2026-09-12

The load sampler ran for a 26 minute session on 0.9.1: a night Nurburgring single player,
a Red Bull Ring hotlap, and an online Touristenfahrten server, seven scene loads in all. It
samples the busiest two dozen threads round robin, a thousand times a second.

The biggest single hotspot in game code during a load is a spin lock, and it belongs to the
engine's fiber job queue. The hot address is the `pause` inside a bare test and set loop:

```
0x279fac4: pause
0x279fac6: mov ecx, eax
0x279fac8: xchg dword ptr [rbx], ecx
0x279faca: test ecx, ecx
0x279facc: jne 0x279fac4
```

The function is `0x279fa90`, called from two places, `0x279f260` and `0x27a0fc0`, which are
the paths that log "Ran out of fibers" and "Ran out of jobs" next to the string
"JobQueue Fiber". So it is the job scheduler's own lock.

It is load specific, and not by a little. Its share of all samples, per fifteen second window:

| window | what was loading | spin share |
|---|---|---|
| t+45 | Nurburgring, 16.7 s | 5.8% |
| t+630 | Red Bull Ring, 7.9 s | 3.1% |
| t+1260 | Touristenfahrten online, 17.9 s | 6.1% |
| the other 100 windows | menus and driving | 0.1 to 0.2% |

Every spike is a track load and nothing else in a hundred windows comes near. Game code in
general goes from 5 to 10 percent of samples at rest to 15.7 and 17.8 percent during the two
long loads.

No engine flag reaches the job system. The flag table has `minimumcores` (shrinks every pool,
measured harmful) and nothing else about jobs, fibers, workers or concurrency.

## The per thread answer, 2026-09-12

A second run, one Nurburgring load (16.66 s total, 12.87 s streaming), with the sampler
reporting each thread against its own samples. What a resource worker does during the
streaming phase:

| thread | game code | job queue spin | blocked in a lock |
|---|---|---|---|
| Resource Manager Worker 3 | 42.3% | 26.9% | 45.1% |
| Resource Manager Worker 6 | 52.3% | 19.1% | 33.2% |
| Resource Manager Worker 7 | 51.4% | 18.8% | 35.1% |
| Resource Manager Worker 1 | 15.5% | 11.5% | 79.2% |
| Resource Manager Worker 0 | 15.9% | 9.9% | 77.4% |

The same threads thirty seconds later, sitting in the pits:

| thread | game code | job queue spin | blocked in a lock |
|---|---|---|---|
| Resource Manager Worker 0 | 1.7% | 0.8% | 97.8% |
| Resource Manager Worker 1 | 1.9% | 0.6% | 97.3% |

So at rest a worker is parked and spins 0.8 percent of the time, and during a load it spins
between 10 and 27 percent of the time. That is the answer: **about a quarter of a busy resource
worker's time during a session load is burned in a spin loop that makes no progress**, and most
of the rest of it is blocked. Real work in the engine's own code is 42 to 52 percent on the
busy workers and 16 percent on the ones that are mostly waiting their turn.

The lock is badly built, which is why eleven workers on it hurt so much. The loop re-issues
`xchg` on every iteration rather than reading until the lock looks free, so every spinner takes
the cache line exclusively and slows down the very thread holding the lock:

```
0x279fac4: pause
0x279fac6: mov ecx, eax
0x279fac8: xchg dword ptr [rbx], ecx   <- a locked write, every iteration
0x279faca: test ecx, ecx
0x279facc: jne 0x279fac4
```

## Why the mod stops here

There is nothing proportionate left to do.

- No flag reaches the job system, the fiber count, or the worker count.
- Fixing the spin loop means patching game code. The loop is ten bytes and a read first
  version does not fit in ten bytes, so it needs an inline detour and a trampoline, which is
  machinery this mod does not have and has never needed. It would sit on an address that moves
  with every game build, in the scheduler every thread in the process goes through, and a
  mistake there is a deadlock for everyone who installs it. Against maybe two seconds of a
  seventeen second load.
- The engine's own `Loading thread boost` already takes the loading pool from 2 to 11 workers,
  which is what puts eleven threads on one bad spin lock in the first place. Reducing that is
  not reachable either.

Recommendation: drop this todo the way BUG-012 and BUG-014 were closed, as the engine's own
cost on every card, with the cause named precisely enough to be worth telling Kunos. The
finding stands on its own: a session load is not disk bound, it is bound by the fiber job
queue's lock.

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
