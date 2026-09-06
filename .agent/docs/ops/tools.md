---
name: tools
kind: doc
description: the Python tools in tools/ and what each command does
updated: 2026-09-06
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
