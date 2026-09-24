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

Batch 2 closed with F-03 fixed and F-04 left as probably not reachable, and added three from two hunters
and seventeen from two verifier passes. The one bug among them was the new check taking the credit for
what the game mode's emptied list does, and the mod now asks the system before each read of a game mode
rather than faulting on one. The run showed a session restarted from the pause menu keeps its game mode.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the free cannot race a strong copy or a moved vector | closed, runtime confirmed | 2026-09-24 |
| 2 | the walk and the teardown cannot fault or throw into the game | closed, runtime confirmed | 2026-09-24 |
| 3 | the leftovers | fixing | 2026-09-24 |

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
- fix: a05bdd5, 2026-09-24, every read of the game mode is fault guarded, and a connection whose game mode no longer starts with a vtable of the exe is let go for good. The hunters found the check is not what keeps the free off a destroyed game mode, H-08, and that the guard still let the game log a crash, H-09. The memset stays unguarded, since it writes the entry the walk matched a few instructions earlier, inside the list checked readable just before, which only F-02's race could move, and the destroy is F-04's.

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

The guard alone would not have been enough, since a game mode the game freed is most likely still
committed heap, and the walk reads stale data there rather than faulting. This note first said the new
check stops the walk finding the connection in the old list, because the heap or the block's next owner
overwrites the vtable word, which H-08 showed wrong. What stops it is the list itself, emptied by the game
mode's destructor, which rests on MSVC's vector leaving its pointers null, H-08. A fault the guard catches
still stalls the thread while the game's crash logger symbolizes it and shows as an `Exception Detected`,
which DEC-023 counts as a reason to look again, so since H-09 each read is checked before it is made and
the guard is the last resort.

### F-04: the connection destructor runs inline inside the game's connect, and a throw from it escapes into make_shared's call site
- severity: bug
- found-by: review
- batch: 2
- status: wontfix
- fix: 2026-09-24, probably not reachable as written, going by MSVC's defaults, since the exe's build flags could not be read, and nothing the mod could add would help. A throw would end the game in the teardown's own noexcept frames before it reached the hook, and a fault caught inside the game's destructor would leave a half destroyed session to run on.

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

Two hunters then ran on batch 2 in parallel, one on the check and one on the guard. They found the
image bounds, the three logged classes, the drop's weak release and every unguarded read right, and
raised the three below.

### H-08: a game mode the game destroyed still passes the check while nothing has written over its block
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 0471ce3 for the comments and the system doc, 2026-09-24 for F-03's note, which now say the emptied list keeps the free off a destroyed game mode, and what that rests on.

`src/engine/session_leak_fix.cpp:147`. The low fragmentation heap writes nothing into a freed slot, and
the game mode's destructors leave the vtable of the last base class they reached at +0, a real vtable of
the exe with a real class name, so a destroyed game mode passes. The comments, the doc and F-03's note
all said it fails.

No wrong free follows. The game mode's destructor released its list (0x19150D1 into 0x1921B10), and
MSVC's vector leaves its pointers null when it goes, so the walk finds nothing and the connection stays
tracked, never freed. That rests on the vector as MSVC builds it, and the exe's copy was not checked for
that, though the mod's own build of a vector destructor keeps all three null stores. Once another object
takes the slot, the walk reads that object's words as a list, where a match is unlikely but not ruled out,
V-17, and a fault was only caught, H-09.

### H-09: a game mode whose memory the heap gave back was found by faulting on it
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: bcde4b4, 2026-09-24, every read of the game mode and its list is checked readable with the system first, and such a connection is let go without a fault. 9e3f840 narrowed the header's wording to those reads.

`src/engine/session_leak_fix.cpp:147` and `:180` to `:187`. The guard caught the fault, but the game's
crash handler sees it first. It writes an `Exception Detected` naming `DSTORAGE.dll` into the player's
game log, the report BUG-022 was fixed to stop, and holds the game thread 120 to 210 ms with `g_lock`
held. The drop that followed was the same one a failed check gives.

### H-10: a live game mode declared as a struct failed the check
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: f921d8c, 2026-09-24, a type name starting `.?AU` passes as well as `.?AV`.

`src/engine/session_leak_fix.cpp:152`. MSVC names a struct `.?AU`. Before a05bdd5 the prefix only chose
the log's name, and after it such a game mode's connections were let go at the connect that would have
freed them, so its sessions leaked again. The 20 logged frees name only classes, and game modes the logs
have never shown, such as Cruise mode's, have not run under the fix.

The verifier then ran on batch 2 as two agents in parallel, one on the code and one on everything written
about it. The code verifier found F-03, H-08, H-09 and H-10 gone, F-04's reason still holding and nothing
new, and noted that the mod's own build of a vector destructor keeps the three null stores H-08 rests on.
The wording verifier raised the ten below.

### V-16: the check's comment said a block the next owner took fails it
- severity: debt
- found-by: verifier
- batch: 2
- status: fixed
- fix: 933f930, 2026-09-24, it fails only once the memory is given back, the heap's links are written over its start, or the block went to something that is not one of the exe's objects, and a block another of the exe's objects took passes with that object's class.

`src/engine/session_leak_fix.cpp:162`. Another of the game's objects with a vtable starts with a vtable of
the exe that has its type information, so it passes, as `EntryFor`'s comment and H-08 both said. 0471ce3
wrote it.

### V-17: "never freed" held only while nothing has taken the block
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 933f930, 2026-09-24, `EntryFor` and the system doc say the emptied list protects while the block still holds the destroyed game mode, and that once another object takes it, a match in that object's words is unlikely but not ruled out, and the header points at `EntryFor` for that. V-26 to V-28 tightened it.

`.agent/docs/systems/session-leak-fix.md:34` and `src/engine/session_leak_fix.cpp:39`. H-09 covered a
fault in such a walk and nothing covered a match. 0471ce3 wrote it.

### V-18: the readable check's comment cited BUG-022 for the stall, which BUG-022 does not record
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: ada4b3d, 2026-09-24, BUG-022 for the report and the telemetry doc's game log section for the 120 to 210 ms stall.

### V-19: H-09 put the stall at about 200 ms, where the rest of the repo says 120 to 210
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24.

### V-20: H-08's fix line credited 0471ce3 with F-03's note, and the note left out what the list rests on
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, the fix line names 9643715's date for the note, and the note names MSVC's vector.

### V-21: F-04's fix line called settled what its own note calls probable
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, probably not reachable as written, going by MSVC's defaults.

The guard hunter had named the one way a throw could still reach the hook, from an `extern "C"` callee in
a build without `/EHr`, and found the mod would stay consistent then, since the destroy runs outside both
lock scopes.

### V-22: H-10 had struct game modes let go at the first connect
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, at the connect that would have freed them.

`EntryFor` runs only at a count of 1, and the manager still holds a session at the next connect, so the
drop came one connect later.

### V-23: "the exe's copy was not read" when the census read it and the patch hashes it
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 933f930 for the comment and the doc, 2026-09-24 for H-08, not checked for that.

The census read 0x1921B10 as the release of the list's use counts. What nobody recorded is whether it
leaves the three pointers null.

### V-24: "left alone" meant a tracked connection in the header and a dropped one in the log
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 6bb42e0, 2026-09-24, the log line and the telemetry doc say `let go of N connection(s) for good`, as the header and the system doc already did.

### V-25: F-03 was marked fixed with the memset its body names unaccounted for
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, F-03's fix line says the memset stays unguarded and why.

A second verifier pass ran on 294dfac. It found V-16 and V-18 to V-25 gone, V-17 all but its lead in,
and every hash and count right, and raised the seven below. The loop stopped there, all seven being
precision in the record, as batch 1's did.

### V-26: the doc's lead in still said a connection whose game mode is gone is not freed
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: f9cd813, 2026-09-24, the lead in is a plain label and the bullet bounds the claim.

`.agent/docs/systems/session-leak-fix.md:34`. The same bullet said a match in another object's words is
not ruled out, and a match is a free. 933f930 wrote it.

### V-27: the header and EntryFor bounded the emptied list's protection by a block that still holds the game mode, which a block the heap wrote its links over also does
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: f9cd813, 2026-09-24, both say while nothing has written over the destroyed game mode, the words `GameModeClass` uses.

`src/engine/session_leak_fix.cpp:196` and `:39`. A block whose start carries the heap's links still holds
the rest of the game mode and fails the check, so its connection is let go rather than left alone. 933f930
wrote it.

### V-28: "the block" in the header could only be read as the control block
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: f9cd813, 2026-09-24, the game mode's memory.

### V-29: F-03's fix line had the memset write 16 bytes the walk read, when the walk reads 8 of them
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, the entry the walk matched, inside the list checked readable just before.

### V-30: F-03's last paragraph had a stub line
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, rewrapped, V-06's slip again.

### V-31: V-17's fix line credited the header with a caveat it only points to
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24.

### V-32: the reviews index said every batch after the first waits on the owner's word, with batch 2 acked
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, it says two batches are closed and the third waits.

33529a6 wrote it, and it went stale when batch 2 was acked.

### F-05: the cleared entry leaves a null shared_ptr inside a live vector whose other readers were never checked
- severity: debt
- found-by: review
- batch: 3
- status: wontfix
- fix: cf65098 took the entry out and 9f18755 put the zeroed entry back, 2026-09-24, the owner leaving the choice to the review. The zeroed entry is seen only by the teardown that runs right after, since the local game server deletes the game mode in the same destroy, and 22 frees across three game mode classes ran that teardown with it and no fault, H-12. Taking it out traded that for a list no run had seen, and let F-02's race write much further, H-11.

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
- status: wontfix
- fix: 2026-09-24, on the owner's word. A list over the limit leaves its connection tracked, which raises the held count in the connect lines, and across the 54 on disk it never goes above 2, so 64 is far past anything seen. The limit is also what keeps a walk through garbage short.

`src/engine/session_leak_fix.cpp:120`. `EntryFor` rejects any list longer than 64 entries and returns
null, so `FreeFinishedSessions` never selects that control and it stays in `g_connections` forever.

Failure: no crash, just the leak the fix exists to remove, silently, on the one configuration where the
session is biggest, which is a large lobby. Only a successful free logs, so nothing says it happened.

The hunter then ran on batch 3. It found the erase itself right, its bounds, its counts and its writes
inside what `EntryFor` had checked, and raised the three below, the first two of which turned F-05's fix
around.

### H-11: taking the entry out read the list's end again after the compare and exchange, so F-02's race could copy across the heap
- severity: debt
- found-by: hunter
- batch: 3
- status: fixed
- fix: 9f18755, 2026-09-24, the zeroed entry is back, so the race writes the 16 bytes DEC-023 weighed.

`src/engine/session_leak_fix.cpp:254` to `:259` at cf65098. A push that moved the list between the walk
and that read left the entry in the old buffer and the end in the new one, so the move either wrapped to
a length near 2^64 or copied everything between the two buffers down 16 bytes, heap headers included. A
push that did not move the list raced the rewrite of its end instead, which could leave the pushed
connection outside the list with its count never released. DEC-023's consequence still described the 16
zeroed bytes. cf65098 caused it.

### H-12: every logged free ran its teardown with the zeroed entry, and taking it out gave the teardown a list no run had seen
- severity: debt
- found-by: hunter
- batch: 3
- status: fixed
- fix: 9f18755, 2026-09-24, the zeroed entry is back.

`src/engine/session_leak_fix.cpp:256` to `:259` at cf65098. The game mode goes in the same destroy, so
only the teardown ever sees the list, and F-05's tick or broadcast over live game modes could never meet
the zeroed entry. The 22 frees on disk, 13 `PaintShopGameMode`, 8 `TimeAttackRemote` and 1
`InstantRaceRemote`, ran that teardown with it and no `Exception Detected`. Teardown code that reads its
own connection by position, with `pop_back`, `erase(begin())` or `back`, would do worse on an empty list,
and nothing shows the game's teardown does not. cf65098 caused it.

### H-13: F-06's reason cited evidence that could never show the failure it closes
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 2026-09-24, it cites the held count in the connect lines, which a list over the limit would raise.

A list over the limit makes no free and no log line, so every free finding its entry says nothing about
it. 3198150 wrote it.

## The runs

Batch 1, one launch on 2026-09-24, `logs/leakfix-b1-20260924`, a track, the menu and a track again,
seven connects in all. The install line named the game thread by id, and every connect came on
`GameThread` with that same id, so the thread check never skipped a free. Five finished sessions were
freed, three `PaintShopGameMode` at 0.2 to 0.3 ms and two `TimeAttackRemote` at 30.7 and 7.1 ms,
counted one to five. The mod detached cleanly and the game's own log has no `Exception Detected`.

Batch 2, one launch on 2026-09-24, `logs/leakfix-b2-20260924`, the menu, a practice on the Nurburgring
24h layout restarted once from the pause menu, the menu and the practice again, four connects in all. The
restart connected nothing and kept its game mode. Every connect came on `GameThread` with the install
line's id. Two finished sessions were freed, the first menu's `PaintShopGameMode` in 0.3 ms, which the
game's own log marks `PaintShopGameMode destroyed` at the same moment, and the restarted practice's
`TimeAttackRemote` in 32.4 ms. Nothing was let go, the mod detached cleanly and the game's own log has no
`Exception Detected`.
