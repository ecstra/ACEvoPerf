---
name: review-2026-09-fix-review-cohtml-build-guard
kind: review
description: the Cohtml angle of the full review of main, the vtable calls that ignore the build check every byte patch honours, seven findings, one breaks
updated: 2026-09-20
links: [spec-reviews, house-rules-agent, responsive-ui, reviews-index]
branch: fix/review-cohtml-build-guard
status: open
---

# Review of the Cohtml hooks and the responsive UI

## Summary

One angle of the full review of main run on 2026-09-20 at high, fourteen reviewers over the whole
tree with no branch under review. This angle covers `src/ui/responsive_ui.cpp` and
`src/ui/cohtml_hooks.cpp`, the two files that reach into Coherent Gameface by vtable index. Three
independent reviewers and a check of my own all landed on the same defect, which is the most serious
thing the review found and the only one that ships on by default to every player.

Seven findings, one breaks, four bug, two debt.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the vtable calls honour the build check the byte patches already do | fixed, owner to verify | 2026-09-20, ack |
| 2 | the menu view is found by identity rather than by a counter | pending | |
| 3 | the page fixes script survives its own error paths | pending | |
| 4 | the moved work thread and its stop flag | pending | |

## Findings

### F-01: the page fixes and the resource work move call hardcoded Cohtml vtable slots with no build check, while the four patched parts stand down
- severity: breaks
- found-by: review
- batch: 1
- status: fixed
- fix: 6578fb4, 2026-09-20, `InstallCohtmlHooks` checks the loaded UI engine's own stamp and hooks nothing that reaches a Cohtml vtable on any other build

`InstallResponsiveUi` at `src/ui/responsive_ui.cpp:354` calls `InstallRestyleFix`,
`InstallMenuRefreshFix`, `InstallStyleMatchFix` and `InstallChildRemovalFix`, each of which compares
the exe's `TimeDateStamp` and `SizeOfImage` and then hashes every byte region it is about to touch,
and each of which refuses cleanly on a mismatch with a log line. The next line then runs
`AddCohtmlViewListener(&OnView)` and `AddCohtmlLibraryListener(&OnLibrary)` unconditionally.

`OnView` at line 211 is three statements:

```cpp
if (number != 1) return;
auto addInitialScript = (PFN_AddInitialScript)(*(void***)view)[cohtml_slot::kViewAddInitialScript];
addInitialScript(view, kPageFixesScript);
```

No stamp check, no bounds check on the slot index. `OnLibrary` overwrites library vtable slots 2, 3
and 5 with `Hook_StopWorkers`, `Hook_Uninitialize` and `Hook_ExecuteWork`. On the installer side,
`InstallCohtmlHooks` at `src/ui/cohtml_hooks.cpp:136` resolves `Library::Initialize` by its mangled
name and then patches `Library::CreateSystem` and `System::CreateView` by fixed index on whatever
`cohtml.WindowsDesktop.dll` happens to be loaded. The mangled name catches a signature change and
does not catch a vtable reorder.

`InstallUiFrameHooks` at line 122 does check the stamp, but only to gate the two EvoUi frame slots,
and it returns early without stopping the rest of `InstallCohtmlHooks`.

Failure: Kunos ships a newer Coherent Gameface with one added virtual anywhere in `IView` or
`ILibrary`. The four byte patches log "not the build this was written against, nothing patched", and
the log therefore reads as if the responsive UI is off. The first `System::CreateView` still fires,
slot 61 is now some other method or past the end of the vtable, and the game makes an indirect call
with the wrong signature before a menu has ever drawn. `src/ui/ui_probe.cpp:1186` reads the same slot
and is developer only, `responsive_ui.cpp:213` is on by default and reaches everybody.

`.agent/docs/systems/responsive-ui.md:13` states that every part checks the build it was read from
before it changes anything. That sentence was false for these two.

**The fix, 6578fb4.** `InstallCohtmlHooks` now reads the loaded `cohtml.WindowsDesktop.dll` PE
`TimeDateStamp` and compares it with `0x675439C7`, the stamp of 1.61.0.3 that the slot numbers were read
from, which `responsive-ui.md` already recorded in its Limits section. On any other stamp it logs the
version it found beside the one it wanted and returns before `g_origInitialize` is set and before the
import is patched, so `Library::CreateSystem`, `System::CreateView` and every registered listener are
unreachable and no slot index is ever used. The frame hooks above it are unaffected, since those are exe
vtable slots already guarded by the exe stamp.

The version is read from the module's own version resource through `FindResourceW` and `LockResource`,
kernel32 only, so the build gains no new import and no new library. It is cosmetic, used only to make the
log line readable, and a module carrying no version resource logs zeros rather than failing.

One thing still to confirm before this merges: `0x675439C7` comes from the repo's own documentation rather
than from a fresh read of the binary, because `ACEVO_GAME_DIR` is not set on this machine and the project
rule allows no other way to reach the game folder. If that constant is stale the guard will refuse on a
build it should accept, and the log line will say so in as many words on the first launch.

### F-02: the menu and HUD view is identified by a process wide creation counter, so a recreated view is never recognised again
- severity: bug
- found-by: review
- batch: 2
- status: open
- fix:

`Hook_CreateView` at `src/ui/cohtml_hooks.cpp:75` hands listeners `int number = ++g_viewsCreated`, a
counter that only ever climbs, and both `responsive_ui.cpp:212` and `ui_probe.cpp:1185` return unless
it is exactly 1. The recorded sessions show 53 views created in one run, because the car displays are
Cohtml views too and are built again at every session load.

Failure: a HUD reload or a device rebuild, which BUG-013 already ties to a device change, destroys
and recreates the menu view. The replacement arrives as view #54, never receives the page fixes
script, and the fixes behind BUG-025 and BUG-026 are gone for the rest of the session with nothing in
the log. `g_mainView` in the probe keeps pointing at the freed view.

### F-03: the vehicle setup Init wrapper uninstalls itself on the first request of any name, so the mark that gates re-init can strand for three seconds
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

In the page fixes script, line 115 sets `__acevoInitPendingSince` and the wrapper at 116 restores
`client.request` at line 117 before it looks at the request name at line 118.

Failure: `stockInit` issues any other request first, or a nested `ks-page-vehiclesetup` on the same
shared Client wraps and unwraps in between. The real Init call then reaches the unwrapped
`stockRequest`, its callback is never wrapped, and `__acevoInitPendingSince` is never zeroed. For the
next three seconds every `init()` on that element returns at line 108 and does nothing, so closing
and reopening vehicle setup inside that window shows the previous build.

### F-04: the finally restores client.request but not the pending mark, so a throw out of the page's own init swallows its retry
- severity: bug
- found-by: review
- batch: 3
- status: open
- fix:

The `finally` at lines 126 and 127 puts `client.request` back but leaves `__acevoInitPendingSince` at
the value set on line 115, because only the response callback at line 120 clears it.

Failure: `stockInit` throws part way through building the setup page, which is exactly the shape
BUG-014 recorded when the patched row build read the filter input before it existed. The page's
second Init a few milliseconds later, the one BUG-026 exists to suppress, now hits the three second
guard and returns silently, so the half built page stays on screen instead of being rebuilt.

### F-05: g_movingStopped is never cleared, so after Cohtml is stopped and initialised again the resource work move is off for good while the log still says it is on
- severity: bug
- found-by: review
- batch: 4
- status: open
- fix:

`Hook_StopWorkers` or `Hook_Uninitialize` calls `StopMovingWork`, which latches `g_movingStopped` at
`src/ui/responsive_ui.cpp:319`. If the game initialises Cohtml again, `Hook_LibraryInitialize` calls
`OnLibrary`, which creates a second semaphore and a second `MovedWorkThread` at line 340 and leaks
both, while `MoveWork` returns false forever.

Failure: every stylesheet parse goes back on the frame thread with the 30 to 60 ms stalls the move
exists to remove, and the log line still reports the move as installed.

### F-06: StopMovingWork waits INFINITE on the stopping thread for a job that may need that same thread
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/responsive_ui.cpp:321` sleeps the frame thread on `g_movedIdle` until `g_movedRunning` reaches
0. The comment at line 225 records that the game runs queued jobs on the frame thread.

Failure: the frame thread calls Cohtml `StopWorkers` or `Uninitialize` at shutdown while
`MovedWorkThread` is inside resource work that needs a game job run. Neither side moves and the game
never exits.

### F-07: the moved work thread runs forever and nothing at DLL detach drains it
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`StopMovingWork` runs only from `Hook_StopWorkers` and `Hook_Uninitialize`.

Failure: the game exits without reaching either, on a crash path or a shutdown that skips
`Library::Uninitialize`. The loader kills `MovedWorkThread` wherever it is, possibly inside Cohtml's
stylesheet parse or image decode holding a heap or Cohtml lock, and then `DllMain`'s
DLL_PROCESS_DETACH runs `LogClose` on the exiting thread. That is the same exit hang the shutdown
branch covers, and it is BUG-022's shape.

## Not filed

`src/ui/restyle_fix.cpp`, `src/ui/child_removal_fix.cpp`, `src/ui/style_match_fix.cpp` and
`src/ui/menu_refresh_fix.cpp` carry findings of their own, filed in
`.agent/reviews/2026-09-sweep-review-ui/ledger.md`, because they are a different surface and a
different branch.

Checked and clean: the install order is deliberately interleaved, child removal's hashed regions stop
at 0x3EDA75, exactly one byte before style match's patch at 0x3EDA76, and no region covers another
part's patch site. The chained `ExecuteWork` hooks resolve in the right order with no recursion.
