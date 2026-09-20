---
name: review-2026-09-fix-review-session-leak-safety
kind: review
description: the session leak fix angle of the full review of main, the hand rolled shared_ptr surgery and what it does not guard against, six findings, three breaks
updated: 2026-09-20
links: [spec-reviews, house-rules-agent, session-leak-fix, BUG-016-vram-overhead-grows-across-scene-loads, reviews-index]
branch: fix/review-session-leak-safety
status: open
---

# Review of the session leak fix

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/engine/session_leak_fix.cpp`, the fix that closed BUG-016 by taking apart the game's own
shared_ptr control blocks by hand and freeing finished sessions.

The arithmetic holds up. Two reviewers independently checked the offsets against the census dump
numbers the research doc records, the connection at +0x4B8, the game server at +0x160, the game mode's
vector at +0x3E8, the weak count of 3 and the second base at object +0x90, and all of them are right.
The weak count handling in `Free` is right too, a floor of 2 before the two `ReleaseWeak` calls so
`_Delete_this` can only fire on the second.

What the review found is that the fix is correct about the steady state and unguarded against
everything else. It assumes nothing else takes a strong reference, that the game mode is still alive,
that the vector does not move, and that the destructor it runs cannot throw. Each of those is true in
the sessions that were measured and none of them is enforced.

Six findings, three breaks, one bug, one debt, one nit.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the free cannot race a strong copy or a moved vector | pending | |
| 2 | the walk and the teardown cannot fault or throw into the game | pending | |
| 3 | the leftovers | pending | |

## Findings

### F-01: the 1 to 0 compare exchange is safe against weak_ptr::lock and not against a plain shared_ptr copy of the vector entry
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

`src/engine/session_leak_fix.cpp:151`. `weak_ptr::lock` is genuinely safe here, because `_Incref_nz`
refuses at zero, and `shared_from_this` goes through lock, so that path is closed. A plain copy of a
`shared_ptr` is not. The copy loads `_Rep` into a register and then does an unconditional
`_InterlockedIncrement` on `_Uses`.

Failure: a game thread walks RemoteGameMode's connection vector and copies an element. Between its
load of `_Rep` and its increment, our thread reads uses == 1, compare exchanges it to 0, memsets the
entry, which is too late because the pointer is already in the other thread's register, runs `_Destroy`
and drops both weak references, and `_Delete_this` frees the control block. The other thread's
increment then resurrects a destroyed object inside freed memory, and its later `_Decref` writes into
the freed block.

The header's claim at line 28, that a strong copy could only come from the entry being cleared, is true
about the source and does not close the window, because the clear happens after the point of no
return.

### F-02: the entry pointer is captured before the compare exchange and written through after it, with no engine lock
- severity: breaks
- found-by: review
- batch: 1
- status: open
- fix:

`src/engine/session_leak_fix.cpp:149` and `:153`. `EntryFor` returns a raw pointer into the game mode's
vector buffer, then the compare exchange runs, then `memset` writes 16 bytes through that pointer.

Failure: a connection is added to that game mode's list on another thread and the vector reallocates
between the two. The memset zeroes 16 bytes of a freed heap block the allocator may already have handed
to someone else. The read is unsynchronised in the other direction too, since first, last and end are
three separate eight byte loads, so a concurrent push_back can hand the loop an old first with a new
last. The modulo 16 and 64 entry bounds keep the walk short and do not make it point at live memory.

Found independently by two reviewers.

### F-03: EntryFor walks three engine pointers with no fault guard and nothing proves the game mode is still live
- severity: breaks
- found-by: review
- batch: 2
- status: open
- fix:

`src/engine/session_leak_fix.cpp:113`. The cycle guarantees that the game mode holds the connection, not
the reverse.

Failure: a connection that something else held while its game mode was destroyed, which is the session
restart from the pause menu that the system doc names as the one path never run under this fix, later
drops to uses == 1 with a dangling pointer at connection+0x4B8+0x160. `EntryFor` reads gameMode+0x3E8,
+0x3F0 and +0x3F8 out of freed memory and walks up to 64 entries of it. If that block was decommitted
the game thread takes an unhandled access violation and the game dies with the mod on the stack.

`GameModeName` immediately above has a `__try`. `EntryFor`, the memset and `Free` have none, so the file
is inconsistent with its own neighbour and with every hook in streamer.cpp.

### F-04: the connection destructor runs inline inside the game's connect, and a throw from it escapes into make_shared's call site
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`src/engine/session_leak_fix.cpp:154`. The census research doc states plainly that these destructors
have never run in the shipped game, because nothing ever freed a connection. `Free` calls `_Destroy`
synchronously on the connecting thread, after make_shared has returned and before
`LocalServerConnect` has finished wiring the new connection. That teardown is a
ThreadsafeCommunicationChannel, a CommunicationChannelQueue, OnlineServices, a DynamicWeatherService, a
RigidBodyODE and the whole parsed track scene, one of which was measured at 29 ms.

Failure: any of it throws. The exception unwinds out of `HookMakeConnection` through a call site the
game wrote as an infallible make_shared, and the connect unwinds leaving the session it was building
half constructed. Nothing in `Free` catches, logs or reports it.

### F-05: the cleared entry leaves a null shared_ptr inside a live vector whose other readers were never checked
- severity: debt
- found-by: review
- batch: 3
- status: open
- fix:

`src/engine/session_leak_fix.cpp:153`. The memset zeroes the object pointer and the control pointer but
the vector's size is unchanged, so RemoteGameMode's list keeps a null element. `kRegions` hashes
exactly two consumers, the game mode destructor's list release at 0x19150D1 and the list destructor at
0x1921B10, which is what makes the skip safe at teardown.

Failure: any other code that iterates m_connections before that teardown, a periodic tick over live game
modes or a broadcast helper, dereferences element[i] through a null object pointer. Nothing in the
install or in the system doc establishes that no such reader exists.

### F-06: a game mode holding more than 64 connections makes its session permanently unfreeable
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/engine/session_leak_fix.cpp:120`. `EntryFor` rejects any list longer than 64 entries and returns
null, so `FreeFinishedSessions` never selects that control and it stays in `g_connections` forever.

Failure: no crash, just the leak the fix exists to remove, silently, on the one configuration where the
session is biggest, which is a large lobby. Only a successful free logs, so nothing says it happened.
