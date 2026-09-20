---
name: reported-working-configurations
kind: memory
description: the machines players have reported the mod working on, including Linux through Proton, and where each report came from, because the readme claims them and nothing else in the repo recorded them
updated: 2026-09-20
type: project
links: [public-docs, DEC-013-overtake-front-door-github-mirror, build-and-release]
---

The mod is developed on one machine, an RTX 3060 Laptop GPU with 6 GB, and everything else in the
`Overview` section of `README.md` is a player report. This is where those reports live, so the readme's
claims have a source that can be checked before a release repeats them.

**Linux through Proton, 2026-09-20.** Reported on the r/assettocorsaevo thread by the user who had asked
a day earlier whether it would work, after the owner answered that he had no idea and to give it a shot.

- Arch Linux, kernel 7.2.6-zen2-1-zen, x86_64
- COSMIC 1.8.0, 2560x1440 at 165 Hz
- Ryzen 9 5900X, RTX 3080, 31.23 GiB
- GE-Proton11-7
- The game's own launch options were
  `VKD3D_CONFIG=force_raw_va_cbv PROTON_ENABLE_WAYLAND=1 gamemoderun %command%`

Worth knowing rather than assuming: the launch options are the reporter's own for the game and are not
something the mod asks for. One report on one distribution and one Proton build is not a supported
platform, and the readme says "reported working" rather than "supported" for that reason.

**Nvidia cards, before 2026-09-20.** RTX 2060, 3060 Ti, 3070 Ti, 4050 and 4060, from the Overtake listing
comments. The individual comments were not recorded at the time, which is what this file exists to stop
happening again.

**Why it matters.** The proxy replaces a Microsoft redistributable and calls DirectStorage directly, so
there was no reason to expect the translation layer to carry it. That it does means a Proton user hitting
a problem is worth taking seriously rather than being told the platform is untested.

**How to apply it.** A new report goes in here with its date and its source when it arrives, and the
readme line changes only when this file does.
