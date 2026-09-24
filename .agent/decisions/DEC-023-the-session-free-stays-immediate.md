---
name: DEC-023-the-session-free-stays-immediate
kind: decision
description: the session leak fix keeps freeing a finished session at the next connect rather than retiring it and freeing it one connect later, because the wait would hold a finished session through a whole load, about 50 MB at the Red Bull Ring and likely 100 to 200 MB at bigger tracks, past the owner's bar
updated: 2026-09-24
links: [session-leak-fix, BUG-016-vram-overhead-grows-across-scene-loads, review-2026-09-fix-review-session-leak-safety]
date: 2026-09-24
area: stability
status: standing
superseded-by:
---

## Decision

The free stays where it is. At each connect the mod frees every earlier connection whose only strong
reference left is the entry in its own game mode's list, at once and on the connecting thread. It
relies on nothing but the game thread touching a finished session. Every free in the logs supports
that and nothing proves it.

## Alternatives

- **Retire at one connect and free at the next.** The count goes to zero and the entry is cleared
  at one connect, and the destroy runs at the next unless the count was raised in between. That
  closes F-01 and F-02 of the session leak fix review whichever thread touches the list. It costs one
  finished session held through the next load, about 50 MB at the Red Bull Ring and likely 100 to
  200 MB at bigger tracks or in an AI race, going by how much the heap grew per visit before the fix
  and how much longer the race session took to free. It never piles up. The owner's bar was 50 MB
  and no pile up, and bigger tracks go past it.
- **Take the game's own lock around the list.** That needs the game's code that reads the list,
  which could not be examined here.
- **Find the slot again right before clearing it.** Narrows nothing, since the gap it would close is
  already a few instructions long.

## Consequences

If another thread ever touches a finished session at the moment of the free, a plain copy of the
list's entry or a push that moves the list can race it, and the game would crash. Nothing has shown
that. Every free across the logs ran on `GameThread`, a whole session after the freed one ended. A
crash that names the free, or evidence of another thread touching a game mode's connection list,
reopens this.
