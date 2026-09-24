---
name: review-2026-09-fix-review-session-leak-safety
kind: review
description: the session leak fix angle of the full review of main, the hand rolled shared_ptr surgery and what it does not guard against, six findings, three breaks
updated: 2026-09-24
links: [spec-reviews, house-rules-agent, session-leak-fix, BUG-016-vram-overhead-grows-across-scene-loads, reviews-index, DEC-023-the-session-free-stays-immediate]
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

Batch 1 closed with both its findings left open on the owner's choice, DEC-023, and added seven from
two hunters and fifteen from four verifier passes. The hunters' one real gap was the free running on
whatever thread connected, which the mod now checks. The verifiers' fifteen were all in the record,
mostly how DEC-023 described the race it leaves open.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the free cannot race a strong copy or a moved vector | closed, runtime confirmed | 2026-09-24 |
| 2 | the walk and the teardown cannot fault or throw into the game | fixing | 2026-09-24 |
| 3 | the leftovers | pending | |

## Findings

### F-01: the 1 to 0 compare exchange is safe against weak_ptr::lock and not against a plain shared_ptr copy of the vector entry
- severity: breaks
- found-by: review
- batch: 1
- status: wontfix
- fix: b89e2d9, 2026-09-24, not closed. The header and the doc now state the assumption the free rests on, that only the game thread touches a finished session, and DEC-023 records the owner's choice not to wait a connect before the destroy.

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

Why it stays open. Closing it needs either the game's own lock around the list, which needs the
game's code that reads the list, or a free that waits a connect while the hook holds the list's
strong reference itself, releasing it the normal way at the next connect so a racing copy only ever
raises a live count. The code could not be examined here, since an agent's script reading the exe's
class tables was stopped by a safety check and that route was left alone. The wait would hold one
finished session through the next load, about 50 MB at the Red Bull Ring and likely 100 to 200 MB
at bigger tracks, and the owner's bar was 50 MB and no pile up. What the logs show, counted by a
second agent on 2026-09-24 and with that day's batch 1 run added, is 50 connects and 20 frees,
every one on `GameThread`, the game's main thread, which also builds every connection, local server
and game mode. Every freed session had ended a whole session earlier, 6.3 to 144 s before, and no
game log kept from those runs holds an exception after a free. The game does take a physics lock
when a session changes, so a second thread in that path is not ruled out.

### F-02: the entry pointer is captured before the compare exchange and written through after it, with no engine lock
- severity: breaks
- found-by: review
- batch: 1
- status: wontfix
- fix: b89e2d9, 2026-09-24, not closed. Waiting a connect before the destroy would not close it either, since the entry is still found and cleared at the first connect, H-02. It needs the game's lock or no write into the list, and the write has to stay. Finding the entry again right before the clear was weighed and dropped, since the gap it would close is already a few instructions long.

`src/engine/session_leak_fix.cpp:149` and `:153`. `EntryFor` returns a raw pointer into the game mode's
vector buffer, then the compare exchange runs, then `memset` writes 16 bytes through that pointer.

Failure: a connection is added to that game mode's list on another thread and the vector reallocates
between the two. The memset zeroes 16 bytes of a freed heap block the allocator may already have handed
to someone else. The read is unsynchronised in the other direction too, since first, last and end are
three separate eight byte loads, so a concurrent push_back can hand the loop an old first with a new
last. The modulo 16 and 64 entry bounds keep the walk short and do not make it point at live memory.

Found independently by two reviewers.

The clear cannot simply go. Without it the list's own release, run inside the destroy when the game
mode goes, takes the count from 0 to minus 1, and MSVC's weak lock refuses only at exactly 0, so a
lock later in the same teardown would succeed on an object being destroyed. A hunter asked on
2026-09-24 whether the write was needed at all, and that is why it stays.

### H-01: the free ran on whatever thread called the connect, and nothing checked it was the game thread
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: ef53db1, 2026-09-24, the free runs only when the connect is on the thread the mod was loaded on, the process's first, and a connect anywhere else tracks its connection and says it freed nothing.

The header stated it as fact. A connect from another thread, on a path no log covers such as the
untested pause menu restart or in a later build, would have destroyed a finished session there
while `GameThread` ran its frame, the race DEC-023 assumes away, and two connects at once would have
run two teardowns in parallel, since `g_lock` covers the pick and not the destroy. All 43 connect
lines on disk before the fix, and all 278 of the game's own `Server connection` steps across the 90
game logs on disk then, name `GameThread`, so no logged path changes.

### H-02: DEC-023 said waiting a connect before the destroy closes both races, and it closes only the copy
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: ef53db1 for the header and the doc, 2026-09-24 for DEC-023 and F-02's fix line.

In that design the entry is still found and cleared at the first connect, so a push that moves the
list between the two still writes into a freed buffer. A reopen would have picked it believing both
were closed.

### H-03: DEC-023's consequence and its reopen trigger did not match how the two races fail
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: 2026-09-24, DEC-023 says where each race fails and names triggers that can fire.

The copy leaves another thread holding a count on a deleted block, and the push leaves zeroed bytes
in a freed buffer, so a crash can come later on another thread or in the heap. The first rewrite
said neither faults in the free and waited for a fault after a freed line, which V-01 corrected. The
trigger is now any `Exception Detected`, unexplained exit or freeze at or after a load with the fix
on, other than the two events DEC-023 weighs, a connect line saying it was not the game thread, or
evidence of another thread touching the list, the freeze from V-05 and the exceptions from V-10 and
V-14.

### H-04: an out of memory throw from a push under g_lock left the lock held
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: ef53db1, 2026-09-24, a scoped guard holds the lock, the pick reserves its vector before anything moves, and `Track` pushes before it takes the weak reference.

Both hunters raised it. The next connect would have hung the game thread in
`AcquireSRWLockExclusive`, since SRW locks are not recursive, and a throw partway through the pick
lost the connections already moved, with their weak references.

### H-05: the freed count was raised and read with no lock
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: ef53db1, 2026-09-24, an atomic.

Reachable only with two connects freeing at once, which H-01's check now rules out.

### H-06: DEC-023 said the free relies on nothing but the game thread touching a finished session
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 2026-09-24, it names the connect's thread and the manager still holding the session before last as well.

### H-07: "with no fault" was stronger than the logs kept
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: ef53db1 for the header and the doc, 2026-09-24 for DEC-023, "with no fault in any game log kept from those runs".

`logs/render-b4-20260920` kept no game log, and the 2026-09-18 play session's mod log was
overwritten, so its seven frees exist only in BUG-016's notes.

The verifier then ran on batch 1. It confirmed H-01, H-02, H-04, H-05, H-06 and H-07 gone and F-01
and F-02 open for true reasons, and raised the three below.

### V-01: DEC-023 said neither race faults in the free, and its trigger waited for a fault after a freed line
- severity: debt
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, the consequence says a copy that lets go during the teardown crashes on either thread, inside the free included, and the trigger is any fault at or after a load with the fix on.

A copy that raises the count from 0 to 1 and lets go within the 0.2 to 31 ms a teardown takes in the
logs runs the destructor a second time while the free is still in it. The freed line is written only
once `Free` returns, so a crash in a run's first free leaves no freed line to follow. H-03's rewrite,
1f85426, caused it.

### V-02: the wait as sketched would not have closed the copy race either
- severity: debt
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, then V-04's correction the same day.

A copy that raises the count from 0 to 1 and lets go leaves it at 0 again, after the game's own
release has already destroyed the connection on that thread, so a deferred destroy that checked the
count alone would run a second time on a dead connection. The first fix said reading the weak count
as well would do, which V-04 showed wrong. It changes the record of the rejected design, not the
decision.

### V-03: the hook's new comment used the wording H-06 took out of DEC-023
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, it names the manager holding the session before last as well. Its pointer to the header was V-07's to correct.

The comment landed in ef53db1 a minute before H-06's fix took the same words out of DEC-023 in
1f85426.

A second verifier pass ran on 1d1b723. It found the code right and raised the six below, one of
them against the wait's record again and one against the reopen trigger.

### V-04: reading the weak count as well still would not close F-01
- severity: debt
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, DEC-023 and F-01 describe the design that does, the hook holding the list's strong reference through the wait and releasing it the normal way.

MSVC's last release takes the count to 0, runs the destroy, and only then drops the weak count, so a
racing copy that lets go just before the next connect's check leaves both counts looking untouched
while its destroy is still running, and the deferred destroy would run beside it. No read of the
counts can see a release in progress. Holding the strong reference means a racing copy only ever
raises a live count.

### V-05: the reopen trigger had no word for a freeze, and the one freeze on record after frees was cleared by timing alone
- severity: debt
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, the trigger counts a freeze, DEC-023 names BUG-032, and BUG-032 and BUG-016 say the 76 s gap does not settle it alone.

A double destroy or a corrupted heap can hang the game rather than crash it. BUG-032 froze at a
thirty AI race start in the launch that had freed seven sessions, and ruled the free out because the
last free was 76 s earlier, which the corrected consequences say proves nothing. Video memory was
over budget there, the likelier cause, so it is linked rather than taken as a reopen.

### V-06: two paragraphs were left unwrapped
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, F-01's paragraph and DEC-023's first alternative rewrapped. V-15's slip of the overlay ledger, here too.

### V-07: the hook's comment said see the header, which in this file means the .h, and the assumption is in the top comment
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, it says the comment at the top of this file.

### V-08: DEC-023 had the copy raise the count only while the teardown runs
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, it names a raise on a block already deleted, which is itself a write into freed memory.

### V-09: V-03 said the comment put back wording H-06 had taken out, when git has the comment first
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

A third pass ran on 0db59e5. It found the rewritten wait closes the copy race as claimed and the
trigger covers crash, freeze and exit, and raised the four below.

### V-10: DEC-023's trigger was met by the freeze its next paragraph set aside, and by TODO-030
- severity: debt
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, the two events are named as weighed and the trigger counts any other, BUG-032 on the owner's read that it was the memory budget and TODO-030 through its own test with the fix switched off. The first form of this fix scoped the trigger by date, which V-14 corrected.

BUG-032 is an unexplained freeze after a load with the fix on, which the trigger as written counted,
and TODO-030's crash on 0.3.2 is an unexplained exit at a load with the fix on by default.

### V-11: H-03 still gave the trigger without the freeze
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

### V-12: V-03's fix line credited it with V-07's change
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

### V-13: F-01's and H-01's log counts left out the batch 1 run
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, F-01 counts 50 connects and 20 frees with the run added and a shortest gap of 6.3 s, and H-01 says its 43 were the lines on disk before the fix.

A fourth pass ran on 2f7a992, confirmed every count against the logs, and raised the two below. The
loop stopped there, both being precision in the record rather than anything about the code.

### V-14: the trigger's date window still took in TODO-030, which was reported the same day
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, the trigger counts any event other than the two DEC-023 weighs, rather than events from a date.

### V-15: H-01's count of the game's own connect steps left out older logs
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, 278 steps across the 90 game logs on disk before the fix, 285 across 91 with the batch 1 run, every one on `GameThread`.

### F-03: EntryFor walks three engine pointers with no fault guard and nothing proves the game mode is still live
- severity: breaks
- found-by: review
- batch: 2
- status: fixed
- fix: a05bdd5, 2026-09-24, the list is walked only when the game mode looks live, its vtable in the exe's image with a readable class name, every read of the game mode is fault guarded, and a connection that fails is let go for good.

`src/engine/session_leak_fix.cpp:113`. The cycle guarantees that the game mode holds the connection, not
the reverse.

Failure: a connection that something else held while its game mode was destroyed, which is the session
restart from the pause menu that the system doc names as the one path never run under this fix, later
drops to uses == 1 with a dangling pointer at connection+0x4B8+0x160. `EntryFor` reads gameMode+0x3E8,
+0x3F0 and +0x3F8 out of freed memory and walks up to 64 entries of it. If that block was decommitted
the game thread takes an unhandled access violation and the game dies with the mod on the stack.

`GameModeName` immediately above has a `__try`. `EntryFor`, the memset and `Free` have none, so the file
is inconsistent with its own neighbour and with every hook in streamer.cpp.

Noted by batch 1's hunter for this batch. The guard belongs around each control rather than the
whole loop, since a loop left partway leaves every later control in `finished` already taken out of
`g_connections`, never freed and still holding the hook's weak reference.

The guard alone would not have been enough. A game mode the game freed is most likely still committed
heap, so the walk reads stale data rather than faulting, and could find the connection in the old list,
free it, and delete the game mode a second time inside the destroy. The liveness check is what stops
that, since the heap or the block's next owner overwrites the vtable word. The guard is a backstop for a
page that was decommitted. A fault it catches still stalls the thread while the game's crash logger
symbolizes it, and shows as an `Exception Detected`, which DEC-023 counts as a reason to look again.

### F-04: the connection destructor runs inline inside the game's connect, and a throw from it escapes into make_shared's call site
- severity: bug
- found-by: review
- batch: 2
- status: wontfix
- fix: 2026-09-24, not reachable as written, and nothing the mod could add would help. A throw ends the game in the teardown's own noexcept frames before it can reach the hook, and a fault caught inside the game's destructor would leave a half destroyed session to run on.

`src/engine/session_leak_fix.cpp:154`. The census research doc states plainly that these destructors
have never run in the shipped game, because nothing ever freed a connection. `Free` calls `_Destroy`
synchronously on the connecting thread, after make_shared has returned and before
`LocalServerConnect` has finished wiring the new connection. That teardown is a
ThreadsafeCommunicationChannel, a CommunicationChannelQueue, OnlineServices, a DynamicWeatherService, a
RigidBodyODE and the whole parsed track scene, one of which was measured at 29 ms.

Failure: any of it throws. The exception unwinds out of `HookMakeConnection` through a call site the
game wrote as an infallible make_shared, and the connect unwinds leaving the session it was building
half constructed. Nothing in `Free` catches, logs or reports it.

Noted by batch 1's hunter for this batch, probably not reachable as written. MSVC's
`_Ref_count_obj2::_Destroy` is `noexcept`, and so are destructors unless declared otherwise, so a C++
throw from the teardown calls `std::terminate` inside the game's frames and never reaches the hook,
where a catch would have nothing to catch. An access violation, F-03's, does pass through `noexcept`
frames, so a `__try` would see that. The exe's own build flags could not be read to confirm it.

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

## The runs

Batch 1, one launch on 2026-09-24, `logs/leakfix-b1-20260924`, a track, the menu and a track again,
seven connects in all. The install line named the game thread by id, and every connect came on
`GameThread` with that same id, so the thread check never skipped a free. Five finished sessions were
freed, three `PaintShopGameMode` at 0.2 to 0.3 ms and two `TimeAttackRemote` at 30.7 and 7.1 ms,
counted one to five. The mod detached cleanly and the game's own log has no `Exception Detected`.
