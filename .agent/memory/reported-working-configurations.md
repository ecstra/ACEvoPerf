---
name: reported-working-configurations
kind: memory
description: the machines players report the mod working on and where each report came from, Nvidia and Radeon and Linux through Proton, plus the open reports nobody has reproduced and why none of them points at a card, because the readme claims a list and nothing else in the repo recorded it
updated: 2026-09-20
type: project
links: [public-docs, DEC-013-overtake-front-door-github-mirror, BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache]
---

The mod is developed on one machine, an RTX 3060 Laptop GPU with 6 GB. Everything else in the `Overview`
section of `README.md` is a player report, and this is where those reports live so the claim has a source
that can be checked before a release repeats it.

## Reported working

From the Overtake listing reviews and the r/assettocorsaevo thread, all on 0.3.1 or 0.3.2.

- RTX 2060 6 GB, twice, one with an i7 7700 and one with a Ryzen 5 7600 on a SATA SSD
- RTX 3060 Ti with a Ryzen 5
- RTX 3070 Ti
- RTX 3080 10 GB
- RTX 4050 6 GB laptop, named VRAM overflow and stuttering as what stopped
- RTX 4060, twice, one of them 8 GB at 1440p with DLSS Quality
- Radeon RX 6600, "works great". That reporter turned Reflex off in the ini, which is only sensible on an
  AMD card and says nothing either way about the rest of the mod, since Reflex is one small addition
  beside the streaming, memory and UI work.
- One report of a 6 GB card that did not say which

## Reported working on Linux, 2026-09-20

Reported on the reddit thread by the user who had asked a day earlier whether it would work, after the
owner answered that he had no idea and to give it a shot.

- Arch Linux, kernel 7.2.6-zen2-1-zen, x86_64
- COSMIC 1.8.0, 2560x1440 at 165 Hz
- Ryzen 9 5900X, RTX 3080, 31.23 GiB
- GE-Proton11-7
- The game's own launch options were
  `VKD3D_CONFIG=force_raw_va_cbv PROTON_ENABLE_WAYLAND=1 gamemoderun %command%`

Those launch options are the reporter's own for the game and are not something the mod asks for. One
report on one distribution and one Proton build is not a supported platform, which is why the readme says
"reported working" rather than "supported".

## Open reports

Three reports nobody has reproduced. The hardware is written down only because the reporter gave it, and
not as a grouping. Nothing in any of them points at the card, and the owner's call of 2026-09-19 on the
first one is the standing reading for all three: the card and the fault are unrelated. A setting, a
leftover shader cache, a driver, or something else local to that machine is at least as likely as either
the GPU or the mod.

- Fences take on a glass look. Reported once, by someone on a 4090.
- Cars glowing. Reported once, by someone on a 4070 Ti with a 14700K and 64 GB. The owner's answer was to
  restart the game and send the log if it persisted, and no log came back.
- Single player improved and multiplayer freezes stayed. Reported once, by someone who said only "Radeon
  here". There is no Radeon on hand to look into it.

These are not in the readme list, because nobody on those machines said the mod worked for them, not
because the cards are suspect. Reading a list of faults as a list of bad cards is exactly the inference to
avoid.

## What this is not

Not a support matrix and not a promise. Nothing here was tested by us, every line is somebody's word, and
three of the reports are unexplained. Keep it that way in the readme's wording.

## How to apply it

A new report goes in here with its date and its source when it arrives, and the readme line changes only
when this file does. A card whose only report is a fault goes in the section above it, not in the list.
