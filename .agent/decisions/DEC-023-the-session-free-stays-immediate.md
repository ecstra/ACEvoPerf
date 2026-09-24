---
name: DEC-023-the-session-free-stays-immediate
kind: decision
description: the session leak fix keeps freeing a finished session at the next connect on the game thread rather than retiring it and freeing it one connect later, because the wait would hold a finished session through a whole load, about 50 MB at the Red Bull Ring and likely 100 to 200 MB at bigger tracks, past the owner's bar, and would close only the copy race
updated: 2026-09-24
links: [session-leak-fix, BUG-016-vram-overhead-grows-across-scene-loads, BUG-032-the-game-freezes-at-a-thirty-ai-race-start, TODO-030-try-to-reproduce-the-crash-joining-a-session-with-a-custom-track, review-2026-09-fix-review-session-leak-safety]
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

- **Retire at one connect and let go at the next.** At one connect the entry is cleared while the
  count stays at 1, so the hook holds the list's strong reference itself, and at the next connect
  it releases that reference the way any `std::shared_ptr` does. A racing copy then only ever raises
  a live count, and whichever release comes last destroys the connection. Reading the counts at the
  next connect instead cannot work, since no read can see a release that is still running its
  destroy. This closes F-01 of the session leak fix review, the copy, whichever thread makes it. It
  does not close F-02, the push, since the entry is still found and cleared at the first connect. It
  costs one finished session held through the next load, about 50 MB at the Red Bull Ring and likely
  100 to 200 MB at bigger tracks or in an AI race, going by how much the heap grew per visit before
  the fix and how much longer the race session took to free. It never piles up. The owner's bar was
  50 MB and no pile up, and bigger tracks go past it.
- **Take the game's own lock around the list.** That needs the game's code that reads the list,
  which could not be examined here.
- **Find the slot again right before clearing it.** Narrows nothing, since the gap it would close is
  already a few instructions long.

## Consequences

If another thread ever touches a finished session at the moment of the free, it can race it. A
plain copy of the list's entry raises the count on a connection the free is tearing down, or on a
block the free has already deleted, which is itself a write into freed memory. If that copy lets go
while the teardown runs, 0.2 to 31 ms in the logs, its release runs the destructor a second time
and the crash can land on either thread, inside the free included. If it lets go later, its release
writes freed memory on its own thread. A push that moves the list leaves the free's 16 zeroed bytes
in a freed buffer, which shows up as heap corruption wherever that block is used next. Any of these
can hang the game as well as crash it. Every free across the logs ran on `GameThread` a whole session
after the freed one ended, with no fault in any game log kept from those runs.

What reopens this is any `Exception Detected`, unexplained exit or freeze at or after a load in a
session with the fix on, other than the two weighed below, since the free runs inside the load's
connect and writes its freed line only once it returns, a connect line saying it was not the game
thread, or evidence of another thread touching a game mode's connection list.

Two events were weighed when this was written. BUG-032 froze a thirty AI race start at the
Nürburgring 76 s after the last free of its launch, with video memory over budget. The gap alone
does not rule the free out, since a race with it can surface that much later, and the owner's read
is the memory budget, not the mod. TODO-030 is a player's crash joining a session on 0.3.2 with
custom tracks installed, and its own tests include `session_leak_fix=0`, which reopens this if that
is what clears the crash.
