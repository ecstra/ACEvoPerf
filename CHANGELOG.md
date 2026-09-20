# Changelog

Every version of ACEvoPerf and what changed in it, newest first.

## 0.4 (unreleased)

### Changed

- Cards with less than 5 GB of video memory, and built in graphics, now get a smaller share set aside for textures instead of the amount a 6 GB card gets.

## 0.3.2 (2026-09-18)

### Fixed

- Laggy menus when hovering, scrolling or dragging a slider, worst on the settings, controls and vehicle setup pages.
- Menu pages stuttering as they open.
- Menus in a session running at a third of the frame rate.
- The controls page freezing when it opens and when you click a bindings group.
- Vehicle setup loading twice every time it opens.
- Blurry trackside big screens.
- Your car, the grass and the kerbs going blurry in races with AI.
- Textures reloading the same detail over and over, even with the car parked.
- Uneven frame pacing while driving, which pulled the 1% lows well below the average frame rate.
- Instant frame drops while driving when a HUD warning like Wrong Way goes away.
- A memory leak growing with every track and menu you load.

### Added

- NVIDIA Reflex, for lower input lag on NVIDIA cards.
- High GPU priority for the game.
- The mod now comes with DirectStorage 1.3.0 and uses it in place of the game's 1.2.3.
- `working_set_floor_mb`, off by default, keeps part of the game in memory so Windows can't page it out.
- Developer settings `streaming_trace`, `load_sampler`, `ui_probe`, `hud_schedule_test` and `memory_census` for testing the mod, all off by default.

### Changed

- `acevo_perf.ini` has one line per setting with a short note beside it.
- Diagnostic settings moved to a new `[developer]` section. If you turned one on in an older ini, set it again there.
- The commented out engine flags are gone from the ini. Any engine flag still works under `[flags]`.

### Known issues

- The heaviest menu pages can still hitch for a moment as they open.
- The 1% lows still sit below the average frame rate while driving.
- In a full race, AI cars and some trackside buildings can still look blurry. Texture memory runs out and the game fills it with your own car first.
- Sweeping the mouse or dragging something quickly in the menus can still drop the frame rate for a moment.

## 0.3.1 (2026-09-06)

### Fixed

- With the CSV logs off, `acevo_perf.log` stopped listing slow frames after the first five.

### Changed

- The two CSV logs are off by default. `timeline=1` and `frames=1` in the ini turn them on.

## 0.3.0 (2026-09-06)

The first release.

### Fixed

- Crashes at startup and on car or track changes.
- Missing icons in the vehicle hub and menus.
- Mushy road, tyre and ground textures, worst after restarting a session. Set texture quality to Ultra in the game for the full effect.

### Added

- Texture memory and the loading buffer are sized for your card automatically. A number in the ini overrides it.
- Engine flags can be set in the ini under `[flags]`. The shader cache and the intro skip are on by default.
- Above normal CPU priority for the game, with Windows power throttling off and a finer system timer.
- `acevo_perf.log` in the game folder lists what the mod did. Optional CSV logs record every second and every frame.
- A warning in the log when a laptop's display runs off a different GPU than the one rendering the game.
- An `acevo_mods` folder. Files in it replace the game's files without touching its content package.
- Python tools in `tools/` to look inside the content package and edit the game's settings files.
