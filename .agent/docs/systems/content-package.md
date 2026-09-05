---
name: content-package
kind: doc
description: the content.kspkg layout, table of contents, hash and cipher, as read by tools/kspkg.py
updated: 2026-09-05
links: [tools, community-tools-assume-32mb-toc, moddability]
---

# Content package

`content.kspkg`, 64.3 GiB on 0.9.0. Read and written about by `tools/kspkg.py`.

- One file. Data first, table of contents in the last 64 MB (`size - 0x4000000`).
- 262,144 slots of 256 bytes, 122,398 used (117,470 files and 4,928 directories), sorted by
  hash, unused slots zero. Slot layout: `path[0xE4]`, `u16 flags` at 0xE4 (bit 0 directory,
  bit 8 XOR ciphered), `u16 pathlen`, `u64 hash`, `u64 size`, `u64 offset`.
- Hash: FNV 1a 64 over the UTF 16 LE path (`fnv1a64_utf16` in `kspkg.py`), verified on every
  entry by `kspkg.py verify`.
- Cipher: XOR with the 8 byte key `C1 35 11 7D A9 21 97 9F` indexed by absolute file offset
  modulo 8 (`xor_at`). The table and 78,687 files are ciphered (meshes 10 GB, animations 1.3 GB,
  scenes, materials, audio banks, UI). All 38,783 `.texturemips` files (50 GB) are stored plain
  so DirectStorage can DMA tiles into GPU memory.
- Roots: `content` (cars, tracks, weather, sfx, characters), `editor` (2,297 files, the editor's
  own assets), `uiresources` (the Gameface UI, 949 files), `system`, `serverconfig`, `cfg`.
- Asset formats are protobuf messages whose schemas sit in the exe. `tools/data/proto_schema.txt`
  lists all 92 files and 1,629 messages. Textures are cooked as 64 KB tiled resources
  (`TextureMetadata.TilingInfo`) in BC1, BC3, BC4, BC5, BC6H and BC7.
- 73 cars and 20 tracks. The largest single assets are FMOD banks (96 MB), track mask textures
  (86 MB), scenes (81 MB) and dynamic track presets (60 MB).

## Loose files and repacking

Community tooling reports that with `content.kspkg` absent the game reads the same paths from a
`content\` folder next to the exe. There is no overlay while the package exists. Repacking is
feasible by appending data before the table and rewriting the table at `newsize - 64 MB` in hash
order. Neither is used by the mod.
