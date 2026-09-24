---
name: DEC-023-the-session-free-stays-immediate
kind: decision
description: the session leak fix keeps freeing a finished session at the next connect on the game thread rather than retiring it and freeing it one connect later, because the wait would hold a finished session through a whole load, about 50 MB at the Red Bull Ring and likely 100 to 200 MB at bigger tracks, past the owner's bar, and would close only the copy race
updated: 2026-09-24
links: [session-leak-fix, BUG-016-vram-overhead-grows-across-scene-loads, review-2026-09-fix-review-session-leak-safety]
date: 2026-09-24
area: stability
status: standing
superseded-by:
---

## Decision

The free stays where it is. At each connect on the game thread the mod frees every earlier
connection whose only strong reference left is the entry in its own game mode's list, at once. It
rests on only the game thread touching a finished session, on the connect running on the game
thread, which the mod checks since ef53db1, and on the manager still holding the session before last
at each connect, which is why every free comes a whole session late. Every free in the logs supports
all three, and nothing proves the first or the last. The fault guards and the rest of that review
are its later batches.

## Alternatives

- **Retire at one connect and free at the next.** The count goes to zero and the entry is cleared
  at one connect, and the destroy runs at the next only if nothing touched the connection in
  between. The count alone cannot tell that, since a copy that raises it and lets go leaves it at
  zero again after the game's own release has already destroyed the connection, so the weak count,
  which that release lowers, has to be checked as well. Done that way it closes F-01 of the session
  leak fix review, the copy, whichever thread makes it. It does not close F-02, the push, since the
  entry is still found and cleared at the first connect. It costs one
  finished session held through the next load, about 50 MB at the Red Bull Ring and likely 100 to
  200 MB at bigger tracks or in an AI race, going by how much the heap grew per visit before the fix
  and how much longer the race session took to free. It never piles up. The owner's bar was 50 MB
  and no pile up, and bigger tracks go past it.
- **Take the game's own lock around the list.** That needs the game's code that reads the list,
  which could not be examined here.
- **Find the slot again right before clearing it.** Narrows nothing, since the gap it would close is
  already a few instructions long.

## Consequences

If another thread ever touches a finished session at the moment of the free, it can race it. A
plain copy of the list's entry raises the count on a connection the free is tearing down. If that
copy lets go while the teardown runs, 0.2 to 31 ms in the logs, its release runs the destructor a
second time and the crash can land on either thread, inside the free included. If it lets go later,
its release writes freed memory on its own thread. A push that moves the list leaves the free's 16
zeroed bytes in a freed buffer, which shows up as heap corruption wherever that block is used next.
Every free across the logs ran on `GameThread` a whole session after the freed one ended, with no
fault in any game log kept from those runs.

What reopens this is any `Exception Detected` or unexplained exit at or after a load in a session with
the fix on, since the free runs inside the load's connect and writes its freed line only once it
returns, a connect line saying it was not the game thread, or evidence of another thread touching a
game mode's connection list.
