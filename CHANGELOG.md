# Changelog

What the mod does, what it fixes and how, one entry per user visible change. Newest first.
Verified means measured in `acevo_perf.log` or seen by the owner in the game on the reference
machine (RTX 3060 Laptop 6 GB, Assetto Corsa EVO 0.9.0+release.48).

## Unreleased

### Fixed

- Crashes on car change, track change and at startup on 6 GB cards. Cause: the game asks
  DirectStorage for a 1 GB staging buffer, which the runtime keeps twice in video memory, so
  2 GB of a 6 GB card were gone before the first texture. The mod caps the staging buffer at
  128 MB (`staging_buffer_mb`). Verified: four car swaps and a car plus track swap in a row
  with no crash.
- Missing icons in the vehicle hub and the menus. Before the fix they loaded sometimes and most
  of the time the tiles stayed empty with no icon at all, and they dropped out again after a
  while in a session. Same cause, the staging buffers starved the video memory the icon
  textures needed, same fix. Verified: icons load every time and stay through car and track
  changes.
- Blurry road, tyre and ground textures, worse after restarting a session (text on the tarmac
  turned to mush). Cause: the engine sizes its texture and mesh pools from the memory left over
  during scene transitions, which gave 633 MB in a race and 526 MB after a restart. The mod sets
  the engine flags `force_canonical_pool_sizes=true` and `tile_pool_mb=1024`, so the pool is
  created once at 1024 MB (mesh cap 1433 MB) and never shrinks. Verified: sharp textures on lap
  four, unchanged after a restart. Texture quality must be Ultra in the game settings for the
  full effect.

### Added

- Package override layer: files under `acevo_mods\<package path>` next to the exe replace or add
  entries of `content.kspkg` without touching the 64 GB package. The proxy rewrites the package
  table in memory when the game reads it and points the game's DirectStorage requests at the
  loose files. Verified with a replaced and an added file. This is the base for content changes.
- Engine flags from the ini: any bool, int32 or double gflag of the game can be set under
  `[flags]`, the mod finds the flag storage inside the exe at start (the release build ignores
  flags on the command line). Defaults on: `enable_pso_cache=true` (fewer shader compile stalls
  after the first run of a combination), `no_intro=true`.
- Process tweaks: above normal priority class, Windows power throttling off for the game, 0.5 ms
  timer resolution.
- Telemetry: `acevo_perf.log` (everything applied, DirectStorage queues and requests, hitches over
  `hitch_ms` with the streaming activity around them), `acevo_perf_timeline.csv` (one line per
  second: fps, hitches, streaming volume, VRAM against budget, CPU), `acevo_perf_frames.csv` (one
  line per frame). `tools/telemetry_report.py` summarises a session folder and joins an
  `nvidia-smi` sample log on the clock second.
- Per frame streaming counters: the frames CSV carries the tile, package to memory and memory to
  GPU requests enqueued since the previous frame, so a slow frame can be matched to streaming.
- Throw log (`[log] throw_log=1`, off by default): counts the C++ exceptions the game throws
  by throw site and type and logs the busiest sites every ten seconds, because the render
  thread was seen spending about two percent of its time in exception unwinding.
- Drag and drop install: the zip holds `dstorage.dll` (the mod), `dstorage_orig.dll` (Microsoft's
  DirectStorage 1.2.3 runtime, byte identical to the game's own), `acevo_perf.ini` and a readme.
  No scripts.

### Tools

- `tools/kspkg.py`: inspect, list, extract and verify `content.kspkg` (64 MB table, FNV-1a 64 path
  hashes, the XOR cipher with its phase restarting at every entry).
- `tools/acevo_settings.py`: view and edit the binary settings files with the protobuf schema
  pulled from the exe, profiles for a 6 GB card, backups before every write.
- `tools/protodesc.py`: the schema extractor behind it. `tools/data/` holds the recovered flag
  table and the schema dump.

### Known, not fixed yet

- Grass and trees change colour as the car gets close and the road ahead sharpens late
  (BUG-001, BUG-006). The mesh fade distances are the engine's own, but the colour change is
  texture residency on a 6 GB card, it does not happen on a 32 GB one. Queued behind the
  optimisation pass.
- Frame drops in a few sections of the Nordschleife (BUG-002) and a 1 percent low that sits 20
  to 25 fps under the average (BUG-009). Nineteen measured laps narrowed the slow frames down to
  the render thread handing its main command list to the GPU a few milliseconds late in heavy
  views, with the swap chain, fences, waits, the GPU clock, the CPU clock, streaming and every
  graphics setting ruled out one by one. No fix in the mod yet, the hunt is parked with its
  evidence and its remaining leads (TODO-010). The drop after a window switch (BUG-013) is the
  pause and HUD reload stalls passing through a rolling counter, plus the device rebuild on a
  device change.
