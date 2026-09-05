---
title: Assetto Corsa EVO 0.9.0 moddability notes
updated: 2026-09-05
---

# Moddability

Everything below was measured on build 0.9.0+release.48 (exe TimeDateStamp 0x6A8D8C54,
2026-08-25) with the scripts in `tools/`. Offsets and counts belong to this build, the mechanisms
should outlive it.

## Engine

- Kunos in house engine (`ksRenderer`, `ksPlatformCore`, `ksPhysicsAC`, `ksAudio`, `ksHTML`),
  Direct3D 12 only, DXC shader compiler at run time, D3D12MA allocator, DLSS 310.5,
  FidelityFX 4.1 (FSR 3 upscaler and frame interpolation), FMOD Studio with Resonance Audio,
  protobuf 3.11 for every data format, gflags for engine switches, Optick and PIX runtimes
  present, ODE physics core.
- The UI is Coherent Gameface (Cohtml 1.61) with V8. The whole menu and HUD is HTML, JS, CSS and
  LESS under `uiresources\` in the package (949 files, entry pages `intro.html` and `menu.html`).
- Thread pools on a 16 thread CPU: render 5, physics 6, loading 2 (boosted to 6 while loading).

## Content package

`content.kspkg`, 64.3 GiB.

- One file. Data first, table of contents in the last 64 MB (`size - 0x4000000`). Community
  tools assume 32 MB, which was true for earlier builds.
- 262,144 slots of 256 bytes, 122,398 used (117,470 files and 4,928 directories), sorted by
  hash, unused slots zero. Slot layout: `path[0xE4]`, `u16 flags` at 0xE4 (bit 0 directory,
  bit 8 XOR ciphered), `u16 pathlen`, `u64 hash`, `u64 size`, `u64 offset`.
- Hash: FNV 1a 64 over the UTF 16 LE path, verified on every entry.
- Cipher: XOR with the 8 byte key `C1 35 11 7D A9 21 97 9F` indexed by absolute file offset
  modulo 8. The table of contents and 78,687 files are ciphered (meshes 10 GB, animations 1.3 GB,
  scenes, materials, audio banks, UI). All 38,783 `.texturemips` files (50 GB) are stored plain,
  so DirectStorage can move tiles straight into GPU memory.
- Roots: `content` (cars, tracks, weather, sfx, characters), `editor` (2,297 files, the editor's
  own assets ship with the game), `uiresources`, `system`, `serverconfig`, `cfg`.
- Asset formats are protobuf messages whose schemas sit in the exe (`Renderer.proto` holds
  `TextureMetadata`, `MaterialData` and `VideoSettings`, then `Mesh.proto`, `Scene.proto`,
  `Weather.proto`, `CarData.proto` and so on). `tools/proto_schema.txt` lists all 92 files and
  1,629 messages. Textures are cooked as 64 KB tiled resources in BC1, BC3, BC4, BC5, BC6H and BC7.
- 73 cars and 20 tracks. The largest single assets are FMOD banks (96 MB), track mask textures
  (86 MB), scenes (81 MB) and dynamic track presets (60 MB).
- Loose files: community tooling reports that with `content.kspkg` absent the game reads the same
  paths from a `content\` folder next to the exe. There is no overlay while the package exists.
  Repacking is feasible by appending data before the table and rewriting the table at
  `newsize - 64 MB` in hash order.

## Streaming pipeline

Observed through the proxy. Three DirectStorage queues, all at capacity 8192 (the maximum),
priority normal, no compression anywhere.

| queue | source and destination | 75 second menu session |
|---|---|---|
| `FileToMemory Queue` | package to CPU memory, then XOR decoded on the CPU | 11,448 requests, 1.23 GB, largest 82 MB |
| `GpuUpload Memory Queue` | CPU memory to buffers and texture regions | 11,041 requests, 854 MB, largest 32 MB |
| `GpuUpload File Queue` | package to reserved resource tiles | 1,373 requests, 798 MB, largest 16 MB |

The game never calls `DStorageSetConfiguration` and asks for a 1024 MB staging buffer. The runtime
keeps two of them in VRAM, so 2 GB of a 6 GB card is gone before a texture loads. The engine then
computes `[Tile Pool] remainder N MB -> texture pool N/2.5` and the same for the mesh streamer in
`DeviceAllocator.cpp`, which gave 400 MB each on the test machine. With a 128 MB staging buffer the
remainder is 2789 MB and both pools 1115 MB. `force_canonical_pool_sizes` yields fixed
1433 MB and 1433 MB.

## Settings and user data

- `%USERPROFILE%\Saved Games\ACE\` holds `video.videosettings`, `audio.audiosettings`, the input
  files, `ui_storage.uistorage`, `GameModes\`, `Logs\`, `Replay\` and `mods\` (the official car
  mod folder since 0.8.1, server side `Saved Games\ACE-Server\mods`).
- Settings files are bare protobuf messages. `video.videosettings` parses as `VideoSettings` and
  exposes fields the UI does not, for example `graphics.texturePoolSize` (Low to Ultra), the
  `levelOfDetailCustomSettings` distance scales, the clouds custom block,
  `car_visibility.max_ahead_car`, the frame rate limits and the VR block. `tools/acevo_settings.py`
  reads and writes them.
- `GraphicsSettingsOverride` files (`.graphicsoverride`, referenced by `SceneGraphicsSettings`)
  let a scene override AA, shadows, LOD, clouds, exposure and sun position. Per track tuning lives
  there.

## Engine flags

- 216 `DEFINE_*` flags (126 bool, 32 double, 31 string, 27 int32). Names, defaults and help text
  are in `tools/gflags_full.tsv`, recovered from the `FlagRegisterer` call sites in the exe.
- The release build does not run the generic gflags parser on the command line. Only a whitelist
  of single dash switches is honoured: `-no_intro`, `-dx12_dred`, `-direct`, `-freeroaming_psw`,
  `-log_file=<path>` (verified, it creates the file), `-log_critical`, `-log_error`,
  `-log_warning`, `-log_info`, `-log_debug`, `-log_trace` (each taking a logger name), and the
  build type switches `-Editor`, `-Modder`, `-AiTester`, `-Server`. `--helpfull` is swallowed.
- Every other flag is reachable only from inside the process, which is what the mod does. See the
  decision record on flags by memory write.

## Editor and modder mode

The exe contains the content editor: `-Editor` and `-Modder` switches, the `editor_modder` flag,
`InputAction_Editor_*` actions, terrain and spline tools, `cookTexture` with GPU block compression,
DDS import, and `ModdedCarContentData` in `LogicScene.proto`. The supported route is the ACE SDK on
Steam Tools.

## Hooking surface for a DLL mod

- `dstorage.dll` is a plain import next to the exe, as are `WinPixEventRuntime.dll`,
  `OptickCore.dll`, `amd_fidelityfx_loader_dx12.dll` and `cohtml.WindowsDesktop.dll`. `dxgi.dll`
  and `d3d12.dll` come from System32 and can be hooked through the import table from any loaded DLL.
- No anti cheat or binary integrity check was found. The online backend validates content SHA
  tables (`dump_sha_tables` flag). The exe is unpacked, has ASLR, no CFG, full RTTI and full
  protobuf descriptors, which makes it very readable.

## What is realistically moddable today

| level | how | risk |
|---|---|---|
| Video, input and audio settings beyond the UI | edit the protobuf files with `acevo_settings.py` | none, backups are automatic |
| Engine behaviour (pools, PSO cache, GI, LOD budgets, AI and netcode test flags) | flags through this DLL | low, any flag can be reverted |
| Streaming and VRAM behaviour | the DirectStorage proxy | low |
| Textures, meshes, materials, UI, sounds | extract from the package, edit, repack or run unpacked | medium, online content validation |
| Cars | official `Saved Games\ACE\mods` plus the ACE SDK | supported |
| Tracks and shaders | not yet supported by Kunos, shaders compile from DXIL in the package | high |
