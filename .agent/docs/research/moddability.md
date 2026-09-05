---
name: moddability
kind: doc
description: how Assetto Corsa EVO 0.9.0 is built and what a mod can realistically change
updated: 2026-09-05
links: [content-package, engine-flags, settings-files, directstorage-streaming]
---

# Moddability

Measured on build 0.9.0+release.48 (exe TimeDateStamp 0x6A8D8C54, 2026-08-25). The details of
each system are in their own docs, this is the assessment.

## Engine

Kunos in house engine (`ksRenderer`, `ksPlatformCore`, `ksPhysicsAC`, `ksAudio`, `ksHTML`),
Direct3D 12 only, DXC shader compiler at run time, D3D12MA allocator, DLSS 310.5, FidelityFX 4.1
(FSR 3 upscaler and frame interpolation), FMOD Studio with Resonance Audio, protobuf 3.11 for
every data format, gflags for engine switches, Optick and PIX runtimes present, ODE physics core.
The UI is Coherent Gameface (Cohtml 1.61) with V8, so the whole menu and HUD is HTML, JS, CSS and
LESS. Thread pools on a 16 thread CPU: render 5, physics 6, loading 2.

## Editor and modder mode

The exe contains the content editor: `-Editor` and `-Modder` switches, the `editor_modder` flag,
`InputAction_Editor_*` actions, terrain and spline tools, `cookTexture` with GPU block
compression, DDS import, `ModdedCarContentData` in `LogicScene.proto`. The supported route for
cars is the ACE SDK on Steam Tools plus the `Saved Games\ACE\mods` folder.

## Hooking surface

`dstorage.dll` is a plain import next to the exe, as are `WinPixEventRuntime.dll`,
`OptickCore.dll`, `amd_fidelityfx_loader_dx12.dll` and `cohtml.WindowsDesktop.dll`. `dxgi.dll` and
`d3d12.dll` come from System32 and can be hooked through the import table from any loaded DLL. No
anti cheat or binary integrity check was found. The online backend validates content SHA tables
(`dump_sha_tables` flag). The exe is unpacked, has ASLR, no CFG, full RTTI and full protobuf
descriptors.

## What is realistically moddable

| level | how | risk |
|---|---|---|
| Video, input and audio settings beyond the UI | edit the protobuf files with `tools/acevo_settings.py` | none, backups are automatic |
| Engine behaviour (pools, PSO cache, GI, LOD budgets, AI and netcode test flags) | flags through this DLL | low, any flag reverts |
| Streaming and VRAM behaviour | the DirectStorage proxy | low |
| Textures, meshes, materials, UI, sounds | extract from the package, edit, repack or run unpacked | medium, online content validation |
| Cars | official mods folder plus the ACE SDK | supported |
| Tracks and shaders | not supported by Kunos yet, shaders compile from DXIL in the package | high |
