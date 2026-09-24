---
name: review-2026-09-sweep-review-engine
kind: review
description: the engine hooks angle of the full review of main, one null pointer that retires all three streamer fixes and a flag writer that can inherit the wrong storage, seven findings
updated: 2026-09-24
links: [spec-reviews, house-rules-agent, directstorage-streaming, engine-flags, reviews-index]
branch: sweep/review-engine
status: open
---

# Review of the engine hooks

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/engine/streamer.cpp`, `flags.cpp`, `exceptions.cpp` and `process.cpp`. The session leak fix has
its own branch and ledger, and the teardown path in streamer.cpp belongs to `fix/review-shutdown`.

The theme is that every failure in here is latching and silent. One unchecked pointer read retires the
three streamer fixes for the rest of the process behind a single log line, and the flag writer can
write the right value to the wrong global and report success.

Seven findings, two bug, three debt, two nit.

Batch 1 closed with both its findings fixed, neither of which had ever fired in 51 sessions and 23,814
kicks, the hunter finding the overlap F-03 feared not possible as far as anything shows. It added two
from its hunter and five from its verifier, all in the wording.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | one fault does not silently retire the fixes for the session | closed, runtime confirmed | 2026-09-24 |
| 2 | the flag writer cannot land on the wrong global | fixing | 2026-09-24 |
| 3 | the throw log cannot eat the game's own exception | pending | |
| 4 | the leftovers | pending | |

## Findings

### F-01: one null allocator pointer silently turns the reload fix, the rank fix and the partial loads off for the whole session
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: e829171, 2026-09-24, `ReadFeedback` returns an empty reading when the texture has no allocator, as `ReadPool` beside it does. A stale allocator is not handled. The hunter found no way for one to be stale, taking a texture's allocator at +0x50 to be the streamer's, which the kick reads for its load gate at its start and which has only been seen freed at exit, BUG-022, though nothing on disk shows the two are one object. No session on disk logged the fault, in 51 with the streamer hooked and 23,814 kicks.

`src/engine/streamer.cpp:422` reads `alloc` from tex+0x50 and dereferences it at +0xA8B0 on the next
line with no null check, while `ReadPool` at line 443 checks the same kind of field.

Failure: a texture seen mid creation or mid teardown carries a null or stale allocator. `OnLevel`
faults, `HookStreamerLevel`'s `__except` calls `Broken`, and from that instant every hook passes
straight through for the rest of the process. The player sees the churn, the ranking and the partial
loads all come back with one log line as the only sign. The project's own memory note adds that the
handled fault also stalls that game thread 120 to 210 ms, because the game's crash logger sees the
access violation before our filter does.

### F-02: the throw log calls an arbitrary virtual slot on the thrown object, and swallows a real C++ exception if that slot throws
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

`src/engine/exceptions.cpp:50`. Line 47 matches on the mangled name, so any game type whose name
contains "error" or "exception" qualifies, not only descendants of std::exception. `vtable[1]` of such
a type is some unrelated virtual method, invoked as `const char*(*)(void*)` with no arguments, and
whatever it leaves in rax is handed to `strncpy_s` at line 73 and read as a string.

Failure: because the call sits inside `__try/__except(EXCEPTION_EXECUTE_HANDLER)` and the build is
/EHsc, a C++ throw from that method arrives as SEH 0xE06D7363 and the mod's filter eats an exception
the game's own catch was waiting for. Gated behind `[developer] throw_log=1`, which is the only reason
this is not breaks.

### F-03: HookGuard is a process wide flag rather than a lock, and it leaks on an SEH unwind
- severity: debt
- found-by: review
- batch: 1
- status: fixed
- fix: 1f948b9 and 2edf3b3, 2026-09-24, a second hook skips its one call and logs the first time, rather than calling `Broken`. The destructor an access violation skips is noted at `HookGuard`, harmless while `Broken` latches every hook off on that path. The overlap below was weighed as possible and is not, as far as anything shows. Kicks are serialised, the scheduler queuing the next only after the last one's job, every hook call is checked to sit in its kick's own frame, and neither half has fired in the 51 sessions on disk with the streamer hooked, 23,814 kicks.

`src/engine/streamer.cpp:559` and `:565`, and again at `:619`. Two problems in one object.

As a flag: the engine's streamer work runs as fiber scheduled jobs, so two workers can enter the same
hook at once. The loser calls `Broken()`, and `g_broken` plus the stub's `g_brokenFlag` stay set for
the session, so all three fixes pass straight through while the log calls it a fault. It does not
serialise anything, it retires the feature.

As a C++ object: `HookGuard` has a destructor and the build is /EHsc, so when
`__except(EXCEPTION_EXECUTE_HANDLER)` catches an access violation raised inside `OnLevel`,
`~HookGuard` never runs and `g_inHook` stays true for the life of the process. Invisible today because
`Broken` latches on the same path and every hook checks `g_broken` first. It becomes real the moment
`Broken` is made recoverable, or a hook is added that consults `g_inHook` without also consulting
`g_broken`.

The hunter then ran on batch 1. It found both fixes right in the source and the object code, an empty
reading safe for every caller, a skipped drop passing through exactly as the unhooked engine does, and
the destructor note true under /EHs, and raised the two below.

### H-01: the file's header still said an overlapping hook sends everything to the engine
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 2edf3b3, 2026-09-24.

`src/engine/streamer.cpp:70` to `:72`, in the list of why the patch is safe. 1f948b9 left it.

### H-02: the fix lines left the stale allocator and the overlap as written, and counted 50 sessions
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 2026-09-24, F-01's line says only a null allocator is handled and what a stale one not arising rests on, F-03's says the overlap is not possible as far as anything shows, and both count 51 sessions and 23,814 kicks.

3e02fee wrote them.

The verifier then ran on batch 1. It found no code defect, both fixes and the destructor note right in
the object code and both counts exact, and raised the three below and two older comments in the same
file. The loop stopped there, all five being precision in the record or a comment.

### V-01: F-01's fix line gave a reason for a stale allocator not arising that nothing on disk backs
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24, it names the premise, a texture's allocator being the streamer's.

a2984a2 wrote it, in F-01's and H-02's fix lines.

### V-02: the overlap line said the fixes stay on in runs where none is on
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 869b0df, 2026-09-24, the line and the header say nothing was switched off.

The hooks also install for the trace alone, and four sessions on disk ran with the fixes off.
1f948b9 and 2edf3b3 wrote it.

### V-03: the reviews index said every batch of the open angles waits on the owner, with engine batch 1 fixing
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 2026-09-24.

V-32's slip in the session leak angle, again.

### V-04: the header counted seven sites where it lists six
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 869b0df, 2026-09-24.

`src/engine/streamer.cpp:47`, wrong since 9f7e83e.

### V-05: the header of streamer.h said only the trace or the reload fix installs the hooks
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 869b0df, 2026-09-24, it names the rank fix and the partial loads as well.

`include/acevo/engine/streamer.h:5`, wrong since 9f7e83e, when `InstallStreamerHooks` began installing
for any of the three fixes.

### F-04: a flag's storage address can be inherited from a previous registration and written as if it were correct
- severity: debt
- found-by: review
- batch: 2
- status: fixed
- fix: 7e8b058, 2026-09-24, the backward walk from each registration forgets what it found before the call to a registration that precedes it, so a site takes only what follows the one before, and a site with no storage of its own there is left unplaced rather than handed the last one's. Before the fix the scan found 204 flags on 0.9.1, which by the 0.9.0 table are 186 real ones and 18 string flags pinned to a neighbour's two addresses, or 185 and 19 if the flag 0.9.1 added is a string, so F-04 fired on every run, harmless only because string flags are never written, H-04. Since 934d6c0 the scan logs how many it placed, and on 0.9.1 that should be those 185 or 186.

`src/engine/flags.cpp:99`. Pass 2 walks back up to 220 bytes from each `FlagRegisterer` call and keeps
the last `lea rax` paired with `mov [rsp+20h],rax`. Registrations in a dynamic initializer sit tens of
bytes apart, so that window routinely spans several earlier call sites.

Failure: a site whose storage was already in a register, or whose `lea` the optimiser hoisted past 220
bytes, has no pair of its own and silently inherits the previous flag's storage, while taking its own
name from its own `lea rdx`. The only checks after that are `image.has(tgt)` and `Writable(st)`, both
true for the wrong global. `WriteFlag` then writes 1, 4 or 8 bytes over an unrelated engine variable
and logs it as a success under the right flag name, so the log gives no hint. Printing the address
beside a known good value once per build would catch it.

### F-05: streamer.cpp keeps private byte for byte copies of two helpers the shared header already exports
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/engine/streamer.cpp:283` and `:753` define static copies of `Fnv1a64` and `AllocNear`, identical
to the non static definitions at `src/core/code_patch.cpp:3` and `:14` apart from one word in a
comment. streamer.cpp does not include `acevo/core/code_patch.h` at all, so it never sees the header it
is duplicating.

### F-06: kIds is read as a count while its neighbours are read as pointers at the same spacing
- severity: nit
- found-by: review
- batch: 4
- status: open
- fix:

`src/engine/streamer.cpp:424`. kAgeLimit 0x0, kIds 0x10, kMips 0x40, kCounts 0x58 and kAges 0x70 sit
0x18 apart from 0x10 onward, which is the spacing of three std::vector triples. Line 424 reads +0x10 as
a uint32 bound while lines 426 to 430 read +0x40, +0x58 and +0x70 as pointers.

If +0x10 is in fact a begin pointer like the others, the bound is the low half of a heap pointer, it
effectively always passes, and `id` then indexes the ages and mips arrays unbounded. The reviewer could
not settle this from the repo. The hashed getters at 0x1F4E7C0 and 0x1F4E7F0 answer it in a minute with
a disassembler. If it is a count, nothing is wrong here and this closes as wontfix with that noted.

### F-07: the flag name is narrowed from wide to narrow characters by iterator assignment
- severity: nit
- found-by: review
- batch: 2
- status: fixed
- fix: 7028979, 2026-09-24, flag names and values go through `WideCharToMultiByte` to UTF-8 the way the streamer's and the load sampler's text does, and the build now has no warnings at all.

`src/engine/flags.cpp:152` and `:153`, `name.assign(wname.begin(), wname.end())`. This is the only
compiler warning in the entire build, C4244 in xutility instantiated from here. Harmless for ASCII flag
names, which is all of them, and it silently mangles anything else. Worth closing because it is the
one thing standing between the build and a clean `/W4` run, which the tools branch wants in order to
turn warnings into errors.

The hunter then ran on batch 2. It found the reset right under the x64 calling convention and in the
object code, a false reset from a stray byte about one chance in a million per game build, with the
same answer every launch, and only ever dropping a site, never moving one, and the conversion sound,
and raised the four below.

### H-03: numeric flags took any text through atoi and atof, so a badly typed size reached the engine as a tiny number
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: 4f94029, 2026-09-24, an int32 or a double is written only when the whole value is one, through `strtoll` and `strtod`, and refused otherwise. Since 30f7232 a `tile_pool_mb` that check would refuse falls back to auto, V-06.

`src/engine/flags.cpp:201` and `:204`. `tile_pool_mb=1,024`, `=1.5` or `=2 GB` wrote 1 or 2 MB into the
engine's tile pool at both slots, logged like any success. The bool branch has refused unknown words
since the proxy core review's V-07, and this branch was missed. Older than the batch.

### H-04: the fix drops every string flag from the count, and nothing said a drop was expected
- severity: nit
- found-by: hunter
- batch: 2
- status: open
- fix: F-04's fix line says what the count was and what it should be, and the system doc takes the next run's figure once there is one, V-09.

String registrations never load a storage slot with `lea rax`, so all 31 of 0.9.0's should be unplaced now, and the
free roam research of 2026-09-06 had already said the walk should stop at the previous registration.
Its run then found all 216.

### H-05: a string flag set in the ini was reported missing from the game
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: 934d6c0, 2026-09-24, an unplaced site stays listed, a placed one wins over it on a repeated name, and the string flag is refused as a string.

7e8b058 caused it, by leaving every string site with no storage, which the scan already dropped before
typing.

### H-06: a refused bool typo ended with no writable storage found
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: 4f94029, 2026-09-24, the value is read once before any storage, so a refusal is said once and returns.

`logs/proxycore-b2-20260920` shows `left alone` once per slot and then `no writable storage found`.
Older than the batch.

The verifier then ran on batch 2. It found F-04, F-07, H-03, H-05 and H-06 closed in the source and the
object code and every value the shipped ini and the docs pass still accepted, and raised the six below,
one of them a bug the strict check caused. The loop stopped there, the rest being precision in the
record.

### V-06: a tile_pool_mb the strict check refuses was never written, and the engine took the whole define
- severity: bug
- found-by: verifier
- batch: 2
- status: fixed
- fix: 30f7232, 2026-09-24, a value that is neither a number nor auto falls back to auto with a note, as `staging_buffer_mb` does since the proxy core review's H-09.

With the shipped `force_canonical_pool_sizes=true` an unwritten tile pool takes the `texturePoolSize`
define, 1433 MB at Low up to 6144 MB at Ultra, the failure DEC-022 and the auto sizes exist to prevent.
`tile_pool_mb=512MB`, which `atoi` used to read as 512, went there. 4f94029 caused it.

### V-07: a number flag with no value was refused quoting a true the player never wrote
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 552fb01, 2026-09-24, it says the flag has no value, and a bare name still means true for a bool.

4f94029 caused it, the bare name's `true` being gflags' rule for bools.

### V-08: F-04's fix line put 185 or 186 real flags and 18 strings behind a count of 204
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, 186 and 18, or 185 and 19, and by the 0.9.0 table rather than shown.

03b6987 wrote it.

### V-09: H-04 was marked fixed on a doc change still to come
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24, H-04 stays open until the next run gives the doc its figure, and says the string sites should be unplaced rather than are.

Since 934d6c0 the scan lists every name, placed or not, so the doc's 203 no longer matches the code
either. 03b6987 wrote it.

### V-10: "one in a million scans" read as a random failure each launch, where the scan reads the same bytes every time
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24.

### V-11: H-05 said 7e8b058 added the skip, which has been there since 57b43fb
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 2026-09-24.

## The runs

Batch 1, one launch on 2026-09-24, `logs/engine-b1-20260924`, 19:44 to 19:47, a short drive and a quit.
The streamer hooked and its line counted up to 142 kicks, 17,630 finer levels wanted and 9,729 loads
turned away for space, with no `a hook faulted` and no `second hook arrived` line, a clean `detached`
and no `Exception Detected` in the game's own log.

## Checked and clean

Nothing in `src/engine/process.cpp` the reviewer believes is a defect. `gpu_priority` is parsed from a
fixed name list in `config.cpp:68` so the cast at line 48 cannot go out of range, the
`NtSetTimerResolution` unit conversion is right in both directions, and the working set floor's hard
minimum plus its half of physical memory check read as deliberate and are off by default.
