---
name: feedback-ship-as-installable-mod
kind: memory
description: the owner wants the result installable by anyone, not a one machine hack
updated: 2026-09-05
links: [TODO-004-release-packaging]
type: feedback
---

The owner asked for best practices "so that anyone can install this as a mod later" and then
sharpened it: "The installation must be as simple as drag and drop to your game folder thats it."
Every change is judged against a stranger copying files from a zip: no scripts to run, no machine
specific paths in the shipped files, an uninstall that is a delete and a rename, settings in one
commented ini.

It matters because it rules out installer logic and quick local hacks (hardcoded addresses,
edited game files) even when they would be faster.

Apply it by keeping the payload to files that work by being present (DEC-007) and by treating
packaging as part of every feature, not a chore for later.
