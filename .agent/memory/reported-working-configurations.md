---
name: reported-working-configurations
kind: memory
description: the machines players report the mod working on and where each report came from, Nvidia and Radeon and Linux through Proton, plus the open reports, most of the graphical ones now BUG-039, the shader cache the mod turns on, because the readme claims a list and nothing else in the repo recorded it
updated: 2026-10-09
type: project
links: [public-docs, DEC-013-overtake-front-door-github-mirror, BUG-015-night-headlights-do-not-light-trees-with-the-pso-cache, BUG-039-the-pso-cache-draws-some-materials-wrong]
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
- RTX 5070 Ti with a 9800X3D, on 0.4.0, from an Overtake review on 2026-10-09, better GPU and video memory
  use than 0.3.2, the first card above 6 GB on record running 0.4's budget sizing
- RTX 4060 8 GB, the shader cache reporter of the open reports below, on 0.4.0 with no issues, 2026-10-09
- An 8 GB card of unstated model on 0.4.0 with `tile_pool_mb=auto`, confirmed by its owner, smooth in a 26
  car online lobby apart from network spikes, 2026-10-09. The same player had set 5120 by hand on 0.3.2
  to stop load screen hangs on car changes.

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

## On Linux with 0.4.0, 2026-10-09

A GitHub issue from a player under Proton with an RTX 3080 10 GB on one 1440p screen: 90 to 100 fps on
0.3.2, about 20 with a broken rear view mirror on 0.4.0, and both fixed by the build with the 0.3.2 table,
published as `v0.4.0-linux` (BUG-040, DEC-027). The readme's Known issues points Linux players at it.

## Open reports

The hardware is written down only because the reporter gave it, and not as a grouping. The owner's call
of 2026-09-19 on the first one was that the card and the fault are unrelated. Since 2026-10-08 most of the
graphical ones read as BUG-039, the shader cache the mod turns on, which the flag off cured for everyone
who tried it. That leans toward the 40 series without proving it, and it is a fault of the cache rather
than of any card.

- Fences take on a glass look. Reported by someone on a 4090, then on a 4060 8 GB on every launch, and
  seen on the owner's machine on 2026-09-24. Now one symptom of BUG-039, the shader cache the mod turns
  on. The same 4060 player saw pit fencing flashing every frame, and a second machine of theirs, a 2070
  8 GB, never showed either.
- Cars glowing. Reported once, by someone on a 4070 Ti with a 14700K and 64 GB. The owner's answer was to
  restart the game and send the log if it persisted, and no log came back. Possibly BUG-039 as well.
- Single player improved and multiplayer freezes stayed. Reported once, by someone who said only "Radeon
  here". There is no Radeon on hand to look into it.
- Worse performance, stutters at fixed places on a track and trees bright white in night races, on the
  game's 0.9. Reported once, on 2026-09-25, with no card, no settings, no numbers and no log. The white
  trees now read as BUG-039, and the stutters are still unexplained. The same player later found the game
  smooth at Ultra, with liveries sharpening slowly on a game installed on a hard disk.
- A car's own main beam vanishing at night on a server, seen in VR, card not given. Read as BUG-039.

The 4060 8 GB player above also reported much steadier frame rates with the mod, which makes a third 4060
in the list above in all but name.

These are not in the readme list, because nobody on those machines said the mod worked for them, not
because the cards are suspect. Reading a list of faults as a list of bad cards is exactly the inference to
avoid.

## What this is not

Not a support matrix and not a promise. Nothing here was tested by us, every line is somebody's word, and
four of the reports are unexplained, one of them now seen here. Keep it that way in the readme's wording.

## How to apply it

A new report goes in here with its date and its source when it arrives, and the readme line changes only
when this file does. A card whose only report is a fault goes in the section above it, not in the list.
