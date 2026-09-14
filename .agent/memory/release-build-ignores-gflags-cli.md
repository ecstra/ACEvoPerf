---
name: release-build-ignores-gflags-cli
kind: memory
description: the release build only parses a whitelist of single dash switches, not gflags syntax, and the whitelist includes the per logger log level switches
updated: 2026-09-14
links: [DEC-002-flags-by-memory-write, engine-flags, mesh-level-of-detail-2026-09-14]
type: project
---

`--name=value` on the command line does nothing in the release build, even though 216 gflags are
compiled in. Established on 0.9.0 by launching with `--minimumcores=true` (no effect on the thread
pool line) and with `-log_file=` (file created). On 0.9.1 the whitelist function at `0x6A27C0` holds 14
entries, among them `-no_intro`, `-dx12_dred`, `-direct`, `-freeroaming_psw`, `-log_file=<path>`,
`-log_off`, `-vr`, `-url` and the per logger level switches such as `-log_info=<logger>`. The build type
switches (`-Editor`, `-Modder`, `-AiTester`, `-Server`) are not read in that function.

It matters because it rules out the obvious route for every engine tweak and is why the mod writes
flag storage directly. The level switches are the one useful exception, since loggers the engine
creates later read the name lists, so `-log_info=meshStreamer` should turn on the mesh streamer's own
`tracked, used, budget` line (TODO-022's deep dive, not yet run in game, logger names are case
sensitive).

Apply it by never promising a flag through launch options, by re testing the whitelist with
`-log_file=` after a game update, and by reaching for `-log_info=` or `-log_debug=` when an engine
logger already writes the number wanted.
