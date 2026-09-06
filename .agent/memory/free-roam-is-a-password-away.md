---
name: free-roam-is-a-password-away
kind: memory
description: 0.9.0 ships the Free Roam mode complete in code and gated by a plain text password plus a hidden menu panel, the content is the Nürburgring scene and stops at the complex
updated: 2026-09-06
links: [free-roam-unlock-2026-09-06, DEC-011-no-free-roam-mod, engine-flags, package-override-layer]
type: project
---

The Free Roam request handler compares the string flag `freeroaming_psw` with a literal that
sits in plain text in the exe, and the Drive menu hides its SOLO FREEROAM panel with one style
rule keyed on the release build class. Behind the gate the handler asks the content tables for
a track named "Eifel" with a "Cruise" event, which 0.9.0 does not carry, and the Nürburgring
scene is the Eifel scene (track type Openworld). The surroundings are twenty low detail
terrain chunks without physics, no road network, no map data.

It matters because it makes the mode a data question, not an engine one: two table lines, a
chart stub and a physics container served by the overlay made it run for an afternoon, and
because the mod ships none of it (DEC-011).

Apply it by pointing anyone who asks at `free-roam-unlock-2026-09-06`, and by checking for
`content\tracks\eifel` in the package after a game update before the topic is reopened.
