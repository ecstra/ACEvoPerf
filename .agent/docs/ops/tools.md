---
name: tools
kind: doc
description: the Python tools in tools/ and what each command does
updated: 2026-09-12
links: [content-package, settings-files, telemetry]
---

# Tools

All in `tools/`, Python 3.10 or newer (`py -3` on Windows, `python3` elsewhere). `pip install
protobuf` is the only dependency, needed by the settings tool and the schema extractor.

No tool guesses where the game is. Set the environment variable `ACEVO_GAME_DIR` to the folder
that holds `AssettoCorsaEVO.exe` (`gamedir.py` reads it), or pass the tool's own path option.
`build.ps1 -Install` uses the same variable to copy a fresh build into the game folder.

## kspkg.py

Reads `content.kspkg` (format in content-package).

- `info`: size, table location, entry counts, cipher counts
- `list [-f GLOB] [--sort path|size|offset]`: entries, `D` directory, `X` ciphered, `P` plain
- `extract GLOB... [-o OUTDIR]`: decode and write files
- `cat PATH`: decoded bytes to stdout
- `verify`: recompute every path hash
- `stats`: size by extension and cipher flag

The package is `ACEVO_GAME_DIR\content.kspkg`, pass `-p` for another file.

## texture_mips.py

Reads a cooked `.texture` header (`TextureMetadata`, schema in `tools/data/proto_schema.txt`) and
can cut its mip chain.

- `show NAME`: size, mip levels, format and the whole `tilingInfo`, tile sizes and the per
  subresource tile offsets and counts
- `strip NAME --keep N --out DIR`: writes a `.texture` and `.texturemips` pair holding only the
  first `N` mip levels, for the override folder

`NAME` is a package path without the extension. The payload it writes is a byte for byte prefix
of the shipped one, so nothing is invented and nothing in the package is touched.

Its reason to exist is BUG-017: the trackside big screens use a flipbook of 64 frames in an 8 by 8
grid, so the engine's mip choice is driven by the whole sheet rather than the frame on show and
every coarse mip costs eight times the detail. Keeping only mip 0 leaves nothing coarse to fall
back to. The tool is committed, the asset it produces is not, because that is the game's own
content and each machine makes it from its own package.

## acevo_settings.py

Reads and edits the game's binary settings files with the schema pulled from the exe.

- `show`: print the file as protobuf text
- `set path=value ...`: dotted field paths, enum values by name, strings quoted for you
- `profile NAME`: apply a named group (`profiles` lists them)
- `restore [BACKUP]`: put back the newest or a named backup

Every write first copies the file to `<file>.bak-<timestamp>`. Close the game before editing.

## protodesc.py

Library used by the settings tool. `load(exe)` returns a descriptor pool and the raw
`FileDescriptorProto` objects found in the exe. `tools/data/proto_schema.txt` is the text dump.

## telemetry_report.py

`telemetry_report.py SESSION_DIR [--slow-fps N] [--from HH:MM:SS] [--to HH:MM:SS]` summarises a
session folder: frame time percentiles, per second fps, VRAM against budget, streaming volume and
batch sizes, CPU, GPU clocks and throttle reasons, slow clusters with everything joined on the
clock second, the logged hitches and PSO activity from the game log.

## data/gflags_full.tsv

Every engine flag with file, name, type, default and help, recovered from the exe. Data files the
tools produce or read live under `tools/data/`.
