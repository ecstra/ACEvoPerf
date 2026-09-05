---
name: release-build-ignores-gflags-cli
kind: memory
description: the release build only parses a whitelist of single dash switches, not gflags syntax
updated: 2026-09-05
links: [DEC-002-flags-by-memory-write, engine-flags]
type: project
---

`--name=value` on the command line does nothing in the release build, even though 216 gflags are
compiled in. Only `-no_intro`, `-dx12_dred`, `-direct`, `-freeroaming_psw`, `-log_file=<path>`,
`-log_<level>=<logger>` and the build type switches `-Editor`, `-Modder`, `-AiTester`, `-Server`
are read. Established on 0.9.0 by launching with `--minimumcores=true` (no effect on the thread
pool line) and with `-log_file=` (file created).

It matters because it rules out the obvious route for every engine tweak and is why the mod writes
flag storage directly.

Apply it by never promising a flag through launch options, and by re testing the whitelist with
`-log_file=` after a game update.
