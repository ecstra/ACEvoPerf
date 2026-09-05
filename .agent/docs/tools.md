---
title: Python tools
updated: 2026-09-05
---

# Tools

All in `tools/`, Python 3.10 or newer. `pip install protobuf` is the only dependency, needed by
the settings tool and the schema extractor.

## kspkg.py

Reads `content.kspkg`. Format details are in [moddability.md](moddability.md).

- `info`: size, table of contents location, entry counts, cipher counts
- `list [-f GLOB] [--sort path|size|offset]`: entries, with `D` directory, `X` ciphered, `P` plain
- `extract GLOB... [-o OUTDIR]`: decode and write files
- `cat PATH`: decoded bytes to stdout
- `verify`: recompute every path hash
- `stats`: size by extension and cipher flag

The package path defaults to the known install folders, pass `-p` otherwise.

## acevo_settings.py

Reads and edits the game's binary settings files using the schema pulled out of the exe at run
time, so it keeps working across updates as long as field names are stable.

- `show`: print the file as protobuf text
- `set path=value ...`: dotted field paths, enum values by name, strings quoted automatically
- `profile NAME`: apply a named group of assignments (`profiles` lists them)
- `restore [BACKUP]`: put back the newest or a named backup

Every write first copies the file to `<file>.bak-<timestamp>`. Close the game before editing.

## protodesc.py

Library used by the settings tool. `load(exe)` returns a descriptor pool and the raw
`FileDescriptorProto` objects found in the exe. `proto_schema.txt` is a text dump of all of them.

## gflags_full.tsv

Every engine flag with file, name, type, default and help text, recovered from the exe. The mod
accepts any bool, int32 or double name from this table in its `[flags]` section.
