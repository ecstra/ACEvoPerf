# Changelog

What the mod does, what it fixes and how, one entry per user visible change. Newest first.
Verified means measured in `acevo_perf.log` or seen by the owner in the game on the reference
machine (RTX 3060 Laptop 6 GB, Assetto Corsa EVO 0.9.0+release.48).

## Unreleased

### Fixed

- Crashes on car change, track change and at startup on 6 GB cards, and the menu and car select
  icons that stopped loading after a while. Cause: the game asks DirectStorage for a 1 GB staging
  buffer, which the runtime keeps twice in video memory, so 2 GB of a 6 GB card were gone before
  the first texture. The mod caps the staging buffer at 128 MB (`staging_buffer_mb`). Verified:
  four car swaps and a car plus track swap in a row with no crash, icons stay.
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
- Swap chain diagnostics: every `SetMaximumFrameLatency`, `ResizeBuffers` and
  `SetFullscreenState` call the game makes is logged with its timestamp. `max_frame_latency` in
  the ini is 0 by default (the game's own value, 2, stays). Any other value is forced on every
  call, and 1 halves the frame rate on a GPU bound machine, so it is a diagnostic knob. This is
  the trace for the 1 percent low drop after a window switch or a session restart (BUG-013).
- Input polling diagnostics (`[input] probe=1`): XInput and DirectInput polls are counted and
  timed per second into the timeline CSV, single polls over 1 ms are logged.
- Device event log (`[input] device_events=1`, on by default): every device arrival, removal and
  audio endpoint change Windows sends to the process is logged with its time, because the game
  answers them by rebuilding its input devices and restarting its audio (a 660 ms frame).
- Per frame streaming counters: the frames CSV carries the tile, package to memory and memory to
  GPU requests enqueued since the previous frame, so a slow frame can be matched to streaming.
- Per frame wait split: the frames CSV carries `present_ms` (how long the Present call blocked),
  `wait_ms` (every wait call of the render thread in the frame, in the game, in D3D12 and in the
  driver) and `fence_ms` (the part of it spent on events D3D12 fences signal, which is waiting for
  the GPU). The log gets a `[wait]` line per minute and handle, so a slow frame can be read as GPU
  wait, other wait, present or render thread work. The report prints that split for the slowest
  1 percent against the faster half.
- Frame limiter (`[dxgi] fps_limit=N`, off by default): holds the present call to the interval
  with a sleep and a short spin, accurate to tens of microseconds, for an even pace.
- Render thread sampler (`[profile] sampler=1`, off by default): samples where the render thread
  is every 250 us and writes per frame counts by module into `acevo_perf_samples.csv`, with the
  system DLLs split into waits, locks, heap and memory copies, and logs the game code addresses
  that show up most in slow frames, the system functions the thread waits in and the game call
  sites under them. Costs about one core while on, for analysis sessions only.
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

- The first second after a texture streams in shows a lower mip (engine feedback loop, BUG-001).
- Grass and distant object pop in (level of detail scales, BUG-006).
- Frame drops in a few sections of the Nordschleife and low 1 percent lows: the GPU is at 100
  percent with thermal slowdown active for most of the lap (BUG-002, BUG-009). The 1 percent low
  also falls after a window switch or repeated session restarts (BUG-013), under investigation
  with the swap chain and input polling diagnostics.
