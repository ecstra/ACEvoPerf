---
name: feedback-ship-as-installable-mod
kind: memory
description: the owner wants the result installable by anyone, not a one machine hack
updated: 2026-09-05
links: [TODO-004-release-packaging]
type: feedback
---

The owner asked for best practices "so that anyone can install this as a mod later". Every change
is judged against a stranger installing it from a zip: no machine specific paths in the shipped
files, an installer that finds the game, an uninstaller that leaves no trace, and settings in one
commented ini.

It matters because it rules out quick local hacks (hardcoded addresses, edited game files) even
when they would be faster.

Apply it by keeping the game folder detection in `dist/install.ps1` generic and by treating
TODO-004 as part of every feature, not a chore for later.
