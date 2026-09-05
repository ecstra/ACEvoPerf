---
title: Facts about the game build the mod targets
updated: 2026-09-05
---

# Game build

Target: Assetto Corsa EVO 0.9.0+release.48, exe TimeDateStamp 0x6A8D8C54 (built 2026-08-25).

- Streaming pipeline (observed through the proxy): three DirectStorage queues at capacity 8192,
  `FileToMemory Queue` (package to CPU memory, XOR decoded on CPU), `GpuUpload Memory Queue`
  (CPU memory to buffers and texture regions), `GpuUpload File Queue` (package straight into
  reserved resource tiles). No compression on any request.
- The game requests a 1024 MB staging buffer and never calls `DStorageSetConfiguration`
  (read from the proxy log).
- Command line: only `-no_intro`, `-dx12_dred`, `-direct`, `-freeroaming_psw`, `-log_file=`,
  `-log_<level>=<logger>` and the build type switches `-Editor`, `-Modder`, `-AiTester`,
  `-Server` are honoured (verified by launching with `-log_file=` and by the absence of any
  effect from `--flag=value`).
- gflags: 216 declared, 203 recoverable by the runtime scan, four constructor instantiations
  (bool, int32, double, string). Full table in `tools/gflags_full.tsv` (extracted from the exe).
- Content package: table of contents in the last 64 MB (older community tools assume 32 MB),
  slots of 256 bytes sorted by FNV 1a 64 of the UTF 16 path, XOR key `C1 35 11 7D A9 21 97 9F`
  applied by absolute offset, `.texturemips` stored plain (verified against all 122,398 entries).
- Settings files under `Saved Games\ACE` are bare protobuf messages, schema embedded in the exe
  (92 proto files, 1629 messages, extracted with `tools/protodesc.py`).
- Loading thread pools on a 16 thread CPU: render 5, physics 6, loading 2 (from the game log).
