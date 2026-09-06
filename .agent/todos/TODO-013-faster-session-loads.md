---
name: TODO-013-faster-session-loads
kind: todo
description: cut the session load, 15 s on the Nordschleife with the disk mostly idle, by finding where the loading workers spend their time and serving them faster from the proxy
updated: 2026-09-06
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

## Steps

1. Measurement build: bring the sampler back from commit `a119e8c` for the loading workers
   only, sample one Nürburgring load, name the top functions and modules. One build, one load.
2. Depending on the answer: a read set cache in the proxy (record the requests of a scene
   load, prefetch them at full drive speed into memory at the next load of the same scene,
   serve DirectStorage from memory), or the decipher taken off the game's workers, or both.

## Done when

The Nürburgring session load, measured by the game's own loading profiler line, drops by a
third or more with the mod installed, and the cause of the remaining time is named.
