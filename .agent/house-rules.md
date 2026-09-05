---
title: House rules
updated: 2026-09-05
---

# House rules

The review protocol and the coding rules live in `CLAUDE.md` at the repo root, sections 9 and 10. This file only records what is specific to this repo.

- Branches: `feat/`, `fix/`, `sweep/`. Main receives reviewed code only, on the user's ask.
- Builds: `build.ps1` at the root. A change to `src/dllmain.cpp` is verified by a build with zero errors and a game launch whose `acevo_perf.log` shows the new behaviour.
- Runtime output (logs, CSVs) never enters the repo. Copy it under a session folder outside the repo when analysing.
- The installed copy in the game folder is updated only through `dist/install.ps1`.
