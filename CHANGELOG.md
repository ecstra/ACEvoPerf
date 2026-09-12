# Changelog

What the mod does, what it fixes and how, one entry per user visible change. Newest first.
Verified means measured in `acevo_perf.log` or seen by the owner in the game on the reference
machine (RTX 3060 Laptop 6 GB, Assetto Corsa EVO 0.9.0+release.48).

## Unreleased

### Added

- The mod brings its own DirectStorage runtime, Microsoft 1.3.0, and uses it instead of the 1.2.3
  the game ships. Being honest about the size of this: going through Microsoft's changelog line by
  line, exactly one fix between the two versions lands on a path this game uses, `DSTORAGE_TILES`
  destinations for resources whose width and height differ. That is the texture tile queue, which
  a short menu session already put 2263 requests and 1.6 GB through. Everything else in 1.2.4 and
  1.3.0 is either a compression fix, and this game streams uncompressed (`compressed=0 gdeflate=0`
  in every queue), or a new API for the game to call, which a game built against 1.2 never will.
  So this is being on the current runtime with one relevant fix, not a speed increase, and no
  measured frame rate or load time change is claimed.
  Nothing of the game is replaced or renamed. `dstorage.dll` is only a forwarder and the runtime
  is `dstoragecore.dll` beside it, which the game loads itself during start-up, so the mod ships
  its copy as `acevo_dstoragecore.dll`, a name nothing else asks for, and calls it directly. A
  game update cannot undo it and uninstalling is still a delete. Verified by reading the version
  back off the runtime that really loaded rather than trusting what was shipped, which the log
  reports at start: `DirectStorage 1.3.0 in use`. That check earned itself immediately, the first
  attempt at this shipped correctly and still ran 1.2.3, and nothing but that line said so.
  `bundled_runtime=0` in the ini goes back to the game's own runtime, and so does a missing file,
  both saying so in the log.
- NVIDIA Reflex, in a game that ships none. It is not a frame rate limiter and never caps
  anything: it stops the CPU queueing frames further ahead of the GPU than it can use, so the
  input behind a frame is newer. NVIDIA only, silently idle on anything else. Verified by
  asking the driver itself rather than trusting the mod: the log reports
  `low latency mode ON` from `NvAPI_D3D_GetSleepStatus` after a thousand frames, with zero
  refused calls. Honest result on the reference machine: no frame rate change, measured over
  four two lap runs, because that laptop is 97 percent GPU bound and pinned at 86 degrees for
  the whole run, so nothing on the CPU side can add frames there. Latency, which is the point
  of Reflex, was not measured. `reflex` in the ini, `reflex_boost` for low latency boost which
  ships off because a thermally capped card has no clocks for it to hold up. On AMD and Intel
  the mod checks the vendor of the adapter the game renders on and never loads NVIDIA's
  library at all, so there is nothing to go wrong: having an NVIDIA driver on the machine is
  not enough, it has to be the card doing the rendering.
- The game now runs at high GPU scheduling priority. Windows gives every process a priority
  class for its GPU work, separate from the CPU one, and nothing was setting it for this game.
  It decides whose work the GPU scheduler takes first when something else is also drawing: a
  browser, an overlay, or on a laptop the integrated chip compositing the desktop. Verified:
  the log reads the value back after setting it and reports `normal -> high`. Honest caveat,
  this is a correct thing to do rather than something measured to be faster, and on a machine
  with nothing else on the GPU it will do nothing at all. `gpu_priority` in the ini,
  `unchanged` to leave it alone.
- An optional working set floor (`working_set_floor_mb`, off by default) keeps a minimum
  amount of the game resident so Windows cannot page it out under memory pressure and fault
  it back in mid corner. Off by default on purpose: a floor too big for the machine is
  refused, and one met by squeezing everything else is worse than the trimming it prevents.

### Fixed

- At night the car's own headlights stopped lighting the trees, and at Oulton Park the track
  as well, while other cars' headlights in multiplayer looked right. Cause: the mod switched
  on the engine's pipeline state cache (`enable_pso_cache`), which the game itself ships off,
  and the cache does not always hand back the pipeline it was asked for. The flag is off by
  default now. It only ever saved shader compilation stalls on repeat runs, it fixed nothing.
  Reported twice on the mod's Overtake listing, one of the reporters found the flag. If you
  have seen this, also delete `Saved Games\ACE\pipeline.library`, the mod cannot reach that
  file and the game does not clear it when it updates.

### Verified

- The mod works on game version 0.9.1+release.6 with no change. The flag scan found 204 flags
  against 0.9.0's 203, wrote all four at their new addresses, capped the staging buffer and
  created the 1024 MB tile pool once. Addresses move with every game build, names do not,
  which is what the scan is for.

## 0.3.1 (2026-09-06)

### Changed

- The two CSV files (`acevo_perf_timeline.csv`, `acevo_perf_frames.csv`) are off by default,
  `timeline=1` and `frames=1` in the ini turn them on. The log stays on, it is the file to post
  with a problem report.

### Fixed

- With both CSVs off, the hitch lines in the log stopped after the first five of a session and
  the throw log never wrote a line, because the thread that refills the hitch budget and ticks
  the throw log only ran for the CSVs. It runs whenever the mod loads now, the sampling work
  only while a CSV is on.

## 0.3.0 (2026-09-06)

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

### Improved

- The texture tile pool and the DirectStorage staging buffer are sized from the card at start
  (`tile_pool_mb=auto`, `staging_buffer_mb=auto`, the defaults now): the mod reads the render
  adapter's dedicated memory the moment the game creates its DXGI factory, before the renderer
  sizes its pools, and picks 1024, 1536, 2048 or 3072 MB of tiles and 128, 192 or 256 MB of
  staging for cards under 7, 11 and 15 GB and above. The same zip is right on any card, a
  number in the ini still overrides. Verified: 5994 MB card, 1024 and 128 chosen and applied.

### Added

- Display owner check: at swap chain creation the log says which adapter owns the monitor the
  window sits on. When it is not the render adapter (laptops with two GPUs), a warning explains
  that every frame is copied to the other adapter before it is shown, about a millisecond per
  frame and more on slow frames, and that a display wired to the render adapter avoids it.
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

- Not the mod's to fix: the road ahead sharpening late and the grass and trees fading and
  changing colour as the car gets close happen on every card (BUG-001, BUG-006). They are the
  engine's own mip and level of detail distances, written into the content meshes, and the
  game's Custom level of detail setting moves them at a frame rate cost.
- Not the mod's to fix either: the 1.2 second freeze at every session start and on back to
  pits is the engine parsing its 63 MB track preset (zlib blobs of one message per value), the
  same on every card (BUG-012). `disable_dynamic_track=true` in the ini removes it together
  with the track evolution.
- Frame drops in a few sections of the Nordschleife (BUG-002) and a 1 percent low that sits 20
  to 25 fps under the average (BUG-009). Nineteen measured laps narrowed the slow frames down to
  the render thread handing its main command list to the GPU a few milliseconds late in heavy
  views, with the swap chain, fences, waits, the GPU clock, the CPU clock, streaming and every
  graphics setting ruled out one by one. No fix in the mod yet, the hunt is parked with its
  evidence and its remaining leads (TODO-010). The drop after a window switch (BUG-013) is the
  pause and HUD reload stalls passing through a rolling counter, plus the device rebuild on a
  device change.
- Not the mod's to fix: the menus and the settings, controls and vehicle setup pages lag on
  open, on every switch and while they are used (BUG-014). The cost is in the game's own UI
  pages and scripts, and the mod changes nothing in the UI. A menu that stops responding after
  a window switch is the game pausing its UI until real input reaches the window, click into
  it.
