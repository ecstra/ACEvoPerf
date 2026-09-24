---
name: review-2026-09-fix-review-cohtml-build-guard
kind: review
description: the Cohtml angle of the full review of main, the vtable calls that ignore the build check every byte patch honours, seven findings, one breaks
updated: 2026-09-24
links: [spec-reviews, house-rules-agent, responsive-ui, reviews-index]
branch: fix/review-cohtml-build-guard
status: closed
---

# Review of the Cohtml hooks and the responsive UI

## Summary

One angle of the full review of main run on 2026-09-20 at high, fourteen reviewers over the whole
tree with no branch under review. This angle covers `src/ui/responsive_ui.cpp` and
`src/ui/cohtml_hooks.cpp`, the two files that reach into Coherent Gameface by vtable index. Three
independent reviewers and a check of my own all landed on the same defect, which is the most serious
thing the review found and the only one that ships on by default to every player.

Seven findings from the review, one breaks, four bug, two debt, plus four the hunter added in batch 1,
one bug, two debt, one nit.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the vtable calls honour the build check the byte patches already do | done | 2026-09-20, ack, runtime confirmed |
| 2 | the menu view is found by identity rather than by a counter | done | 2026-09-20, ack, runtime confirmed |
| 3 | the page fixes script survives its own error paths | done | 2026-09-20, ack, runtime confirmed |
| 4 | the moved work thread and its stop flag | done, and the path it fixes was thought never to run, corrected in H-23 | 2026-09-20, ack, runtime confirmed |
| 5 | what the page fixes script costs, and saying when it is not there | done | 2026-09-20, ack, runtime confirmed |

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

The constant was open for a few minutes and is now settled. `0x675439C7` came from this repo's own
documentation rather than from the binary, because `ACEVO_GAME_DIR` is not set on this machine and the
project rule allows no other route to the game folder. It was confirmed two ways. The owner had the game
running, so the loaded module was read out of the live process: stamp `0x675439C7`, image `0x0070F000`,
version 1.61.0.3. The hunter then found the same pair already hardcoded in five other files
(`restyle_fix.cpp:89`, `style_match_fix.cpp:230`, `menu_refresh_fix.cpp:224`,
`child_removal_fix.cpp:386`, `ui_probe.cpp:1015`), and no refusal line from any of them appears in any
recorded log. The guard will not turn away a build it should accept.

### F-02: the menu and HUD view is identified by a process wide creation counter, so a recreated view is never recognised again
- severity: bug
- found-by: review
- batch: 2
- status: fixed
- fix: 8145a1a, 2026-09-20, the menu view is told by its size rather than by its ordinal, and the listener carries a `mainView` flag instead of both consumers testing `number == 1`

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
- status: fixed
- fix: ffbe6b9, 2026-09-20, the wrapper steps aside only once the Init it is waiting for has come past, confirmed by `setup init 2 ignored 1` on a probe run

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
- status: fixed
- fix: ffbe6b9 then 29d4edf, 2026-09-20, the mark is cleared when no Init of ours went out, tracked in a flag rather than inferred from the wrapper

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
- status: fixed
- fix: 7657e7c, 2026-09-20, `OnLibrary` clears the flag under the lock and reuses the one thread and semaphore instead of making a second pair

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
- status: fixed
- fix: 7657e7c then d10cb26, 2026-09-20, the wait is bounded against a real deadline and a call given up on is not waited for again

`src/ui/responsive_ui.cpp:321` sleeps the frame thread on `g_movedIdle` until `g_movedRunning` reaches
0. The comment at line 225 records that the game runs queued jobs on the frame thread.

Failure: the frame thread calls Cohtml `StopWorkers` or `Uninitialize` at shutdown while
`MovedWorkThread` is inside resource work that needs a game job run. Neither side moves and the game
never exits.

### F-07: the moved work thread runs forever and nothing at DLL detach drains it
- severity: debt
- found-by: review
- batch: 4
- status: deferred to fix/review-shutdown
- fix: handed over to `fix/review-shutdown` as its F-05, 2026-09-24. The exit hang below turned out to end the process instead, that branch's H-02.

Deferred rather than fixed here. The fix is a stop at DLL detach, and the detach path belongs to
`fix/review-shutdown`, which already owns the identical hazard in `log.cpp`, `timeline.cpp`,
`load_sampler.cpp` and `streamer.cpp`. Doing it in this branch would put two branches on the same
teardown, and the shutdown branch has to decide the order of that teardown as a whole rather than one
thread at a time. Its F-01 is the same defect from the other end.

`StopMovingWork` runs only from `Hook_StopWorkers` and `Hook_Uninitialize`.

Failure: the game exits without reaching either, on a crash path or a shutdown that skips
`Library::Uninitialize`. The loader kills `MovedWorkThread` wherever it is, possibly inside Cohtml's
stylesheet parse or image decode holding a heap or Cohtml lock, and then `DllMain`'s
DLL_PROCESS_DETACH runs `LogClose` on the exiting thread. That is the same exit hang the shutdown
branch covers, and it is BUG-022's shape.

### H-01: the resource work move installs and parks forever when the game UI's frame end is not hooked
- severity: bug
- found-by: hunter
- batch: 1
- status: fixed
- fix: 1dae423, 2026-09-20, `OnLibrary` asks `UiFrameEndHooked()` first and stays out with a log line when it is false

The exe and the UI engine are checked separately, which is the whole point of F-01, and that separation
has a second edge nobody had looked at. Kunos ships a new exe and leaves Cohtml alone, which is the likely
shape of the next update since Cohtml is a third party binary. `InstallUiFrameHooks` fails the exe stamp,
logs that the UI frame is not followed and returns, so `Hook_EndFrame` never installs and
`responsive_ui`'s `OnFrameEnd` never runs. The Cohtml guard below it passes, so `OnLibrary` creates the
semaphore, starts `MovedWorkThread`, hooks library slots 2, 3 and 5 and logs "resource work the game's
frame thread picks up runs on the mod's thread".

`g_frameThread` stays 0 for the whole session, so the compare at `responsive_ui.cpp:282` never matches,
nothing is ever moved, and `MovedWorkThread` blocks on a semaphore nobody signals until the process dies.
The owner reads the log, sees the move installed, and measures the stalls it was supposed to have removed.

### H-02: the new guard tested the stamp alone while five other files test the stamp and the image size
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: 8a0c786, 2026-09-20, the guard requires both, using the constants the byte patch files already carry

A Cohtml that keeps its stamp and changes its image size, which is a relink, a repack, or a redistribution
wrapped by DRM or anti cheat, would have passed the vtable guard and failed all five byte patch guards. The
mod would then reach Cohtml vtables on exactly the binary its own patches had just refused, which is the
inverse of the asymmetry F-01 exists to close.

### H-03: the read added to survive an unknown build could fault on one
- severity: debt
- found-by: hunter
- batch: 1
- status: fixed
- fix: 8a0c786, 2026-09-20, `ReadModuleBuild` checks the DOS, NT and optional header magics and wraps the whole read in `__try`

Two holes in the fix itself. `SizeofResource` returns the size the resource directory claims and the loader
never validates it against the image, so a packed or obfuscated build can claim a size past the end of its
own mapping and `ModuleFileVersion`'s walk reads unmapped memory. And the DOS and NT headers were walked
with no `e_magic` or `Signature` check, while `PatchIatByAddress` forty lines away checks both and wraps
its walk in `__try`. This runs at DLL_PROCESS_ATTACH before the game's entry point, so either one is a
startup crash on a foreign build, inside the code added to make foreign builds safe.

### H-04: the view hook read its settings before anything had validated them
- severity: nit
- found-by: hunter
- batch: 1
- status: fixed
- fix: 3f56c8e, 2026-09-20, the two reads tolerate a null

`Hook_CreateView` reads settings+0x10 and +0x14 before calling the original, so a `CreateView` with a null
settings pointer faults in the mod rather than being refused by Cohtml. The engine's own API requires the
pointer, so this is a hardening gap rather than a live defect, and it was the one read in the file that
happened ahead of any validation.

### H-05: the first fix let an unreadable view hand the menu identity to a car display
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed
- fix: e79a13d, 2026-09-20, the ordinal is tried before the size, so view number one is the menu view whatever its settings read

The first cut of F-02 replaced the ordinal with the size outright. A view whose settings pointer could not
be read, the case H-04 hardened, has a width and height of zero, the `size != 0` guard skips it, and the
next view claims the identity permanently. That next view is a 1024x1024 car display, so every dashboard
of that size for the rest of the session is handed both the page fixes script and the probe script while
the menu view gets neither.

That is worse than F-02 itself, which only ever left the scripts unapplied. The ordinal was immune,
because `number == 1` holds whatever the settings read. Trying it first keeps that immunity and keeps the
size match for the remake case, so the rule is now strictly better than either half alone.

### H-06: the size claim latches for the session, so a remake at a new resolution is still missed, and silently
- severity: bug
- found-by: hunter
- batch: 2
- status: fixed in part, the silence is closed and the limit stands
- fix: e79a13d, 2026-09-20, a later view at least as big as the claimed one now logs that it is not being treated as the menu view

A resolution change is the most plausible cause of the teardown this fix exists to survive, and it is
exactly the case the size match cannot cover, because the remade view comes back at the new size while the
claim still holds the old one. The hunter's point that stands is the silence: F-02's whole complaint was
that an unrecognised menu view says nothing, and the first cut reproduced that.

The log line closes the silence. The limit itself is left open deliberately. The signal that would close
it is the swap chain size, which `dxgi_hooks.cpp:19` already sees five seconds before view number one in
every recorded log and which tracks a resize. Reaching for it would make the page fixes depend on
`[dxgi] enabled`, and a setting silently disabling an unrelated fix is the bug class this whole review
exists to remove, so it is not worth trading one for the other here. If the log line is ever seen in a
real session, that is the evidence to revisit it with.

### H-07: the page fixes doc still said the script goes to the first view
- severity: debt
- found-by: hunter
- batch: 2
- status: fixed
- fix: e79a13d, 2026-09-20

Commit 8145a1a changed four source files and no doc, the same upkeep miss as V-03 one commit earlier on
the same file. Twice in one branch is a pattern rather than a slip.

### H-08: the probe's per second line labelled the main view's clock "view #1 clock"
- severity: nit
- found-by: hunter
- batch: 2
- status: fixed
- fix: e79a13d, 2026-09-20, the label reads "main view clock"

With the fix working, a remade menu view is correctly recognised as number 54 and its clock is printed
under a heading naming view 1, next to a view 1 row that has stopped advancing. The one line that proves
the fix worked read as though it had not.

### H-09: g_mainView is a raw pointer to an object whose death nothing follows
- severity: debt
- found-by: hunter
- batch: 2
- status: deferred to sweep/review-ui-probe
- fix:

Nothing in the project hooks view destruction. After the menu view is destroyed `g_mainView` keeps its
value, and a car display allocated on the freed block compares equal, so the advance clock follows the
dashboard and the eviction loop protects its row until the remade menu view reassigns the pointer. The
pointer is only ever compared and never dereferenced, so there is no fault, and the probe ships off. The
ordinal had no equivalent hole, because an ordinal cannot be recycled, so this is a cost of the change and
is recorded as one. It belongs to the probe's own branch because the fix is the probe's to make.

### H-10: two live views at the claimed size are both told they are the menu view, and the probe's script is not harmless in a car display
- severity: debt
- found-by: hunter
- batch: 2
- status: deferred to sweep/review-ui-probe
- fix:

The page fixes script is close to harmless in a dashboard, since neither `ks-page-vehiclesetup` nor
`ks-page-settings-controls` exists there and no patch ever fires. The probe's script is not. Its only
exclusion is a check for hud.html, so a dashboard falls into `installChangeCounters()`, the path the probe
deliberately keeps off the HUD because that page makes about twenty thousand writes a second and a
dashboard is the same shape. Needs a size collision, which does not exist on this hardware, where the
dashboards are power of two render targets and the menu view matches the swap chain exactly.

### V-06: the log added for H-06 fired on a resolution increase and stayed silent on a decrease
- severity: bug
- found-by: verifier
- batch: 2
- status: fixed
- fix: 853fae7, 2026-09-20, the log is removed and the limit is filed as BUG-033

The condition was `size >= claimed` on a packed value with width in the high word, which is a width major
lexicographic order and not a size comparison at all. Claimed 1920x1080 and remade at 1280x720 compares
smaller, so nothing was logged, the page fixes did not go in, and nothing said so, which is F-02's
original complaint reproduced exactly.

The direction matters. A fullscreen to windowed change, which is the BUG-013 device change this fix exists
to survive, remakes the view at the smaller client size. So the likely case was the silent one and the
justification written in this ledger for the line was true of one direction only.

The line also misdescribed its own test, since 1440x2560 has 1.8 times the pixels of 1920x1080 and was
silent while 2560x480 has 0.6 times the pixels and fired claiming to be at least as big. A heuristic that
is wrong in the common direction and misleading in its wording is worse than no line, so it is gone and
the limit is a filed bug instead, which is what house rule 9.6 asks for.

### V-07: the header carried the superseded first cut of the rule
- severity: debt
- found-by: verifier
- batch: 2
- status: fixed
- fix: 853fae7, 2026-09-20

`include/acevo/ui/cohtml_hooks.h` still said the view is told by size rather than by being first, which
e79a13d made false when it put the ordinal back in front. That commit corrected the systems doc and the
source comment and left the header, which is the file both consumers actually read. The same upkeep miss
as H-07, one file over, which is the third time in this branch.

### V-08: a zero size first view disabled the size path for the session and said nothing
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 853fae7, 2026-09-20, it logs once that only the first view will be recognised

H-05's capture is gone, and the degraded mode it leaves behind was itself silent, which is the same class
of problem H-06 was raised over.

### V-09: a non first view at exactly the first view's size takes the identity
- severity: bug, unobserved
- found-by: verifier
- batch: 2
- status: wontfix, this is the trade the fix is
- fix:

Impossible before this batch. It is the direct cost of recognising a remake at all: to accept a later view
of the claimed size as the menu view, you accept any view of that size. Such a view would receive both
scripts, and in the probe `g_mainView` would move to it, so the main view clock would follow the wrong
view. Not present in any recorded session, where the menu view matches the swap chain exactly and the
dashboards are power of two render targets at 1024x1024, 512x2048, 512x512 and 512x128. The ordinal is
tried first, so the menu view itself is never the one that loses out, only the collision would gain.

What would settle it is a session at a resolution matching a car display, 1024x1024 windowed for example.
Recorded here rather than guarded against, because guarding needs the same external signal BUG-033 needs.

## What the hunter established about F-02 itself

F-02 has never fired. Across twelve recorded sessions, including focus switches, pauses, a logged device
change and about fifty session loads, every one shows exactly one 1920x1080 view created and one script
install. The fix is preventative, which is worth knowing when weighing how much machinery it deserves.

## The hunter's verdict on the fix

The fix held under attack. What was tried and why each failed:

- **another path to a slot.** All six uses of a `cohtml_slot::` constant sit inside `Hook_CreateSystem`,
  `Hook_LibraryInitialize` or a listener, every listener is only ever reached from those two hooks, and
  `MovedWorkThread`, the one that re-reads slot 5 live, is only created inside `OnLibrary`. The single
  entry to the whole graph is the patched exe import, and that patch now happens after the compare.
- **a cached slot derived pointer.** `g_origInitialize` is set after the check, every other original is
  filled inside a hook, and the byte patch files cache Cohtml addresses only after their own checks.
- **a null handle at install with the library loaded later.** Real in theory, not here, because
  `child_removal_fix` and `style_match_fix` patch Cohtml bytes in the same DllMain pass and the logs show
  them installing. If it ever were not loaded, `GetProcAddress` returns null and the function returns
  before anything is hooked, which fails safe.
- **`g_installed` set before the check.** One call site, no retry wanted, and the early exit leaves
  nothing half installed.

### V-01: the version scan trusted a size the loader never checks, and its bound could wrap
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 8af4fad, 2026-09-20, the scan is capped at 1024 bytes and the bound is written so it cannot wrap

H-03 turned this read from a crash into a handled fault and did not stop the read itself. `SizeofResource`
returns the resource directory's own claim and nothing clamps it to the module's image, so a packed build
claiming a large size had the loop scan the rest of the image and stop only by taking an access violation
into the `__except`. With a size near the top of the range, `at + 16` wrapped in DWORD arithmetic, so
termination depended entirely on hitting an unmapped page. The fixed block sits in the first few dozen
bytes of every version resource, so a small ceiling costs nothing and removes both.

### V-02: the refusal line could print numbers that contradict it
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 8af4fad, 2026-09-20, a failed read gets its own line and prints no numbers

`ReadModuleBuild` writes the stamp and the image size before it reads the version, so a fault in the
resource walk returned false with both already correct, and the refusal line then printed a matching stamp
and image beside the words "nothing that uses its vtables runs". Only reachable on a malformed version
resource, and it is the line somebody reads when they are already confused.

### V-03: the doc missed the second way the resource work move stands down
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 5e1a521, 2026-09-20, both the shared hooks section and the resource work move section say which half of the check each part needs

The paragraph written with 6578fb4 said the page fixes and the move go out together, which stopped being
true when 1dae423 landed an hour later. A new exe with the same Cohtml now takes the move out and leaves
the page fixes in. The upkeep rule wanted that doc moving in the same round as the code.

### V-04: the doc fix for V-03 claimed the resource work move is the only part that can be out, and three parts go out in the case it describes
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 6341a1c, 2026-09-20, the paragraph splits the parts three ways instead

`restyle_fix.cpp:29` and `menu_refresh_fix.cpp:29` carry the same exe stamp and image size that
`cohtml_hooks.cpp:11` compares, so a new exe with the UI engine unchanged fails all three by the same
comparison at the same moment. Three of the doc's own seven parts stand down, not one. The sentence was
only true in the narrower case where the exe stamp matches and the EvoUi vtable thunks do not, which is
not the case the paragraph is about. Written while fixing V-03 and wrong on the same day, which is the
argument for the verifier looking at doc commits and not only code ones.

### V-05: the doc named one of the two ways the UI engine check refuses
- severity: nit
- found-by: verifier
- batch: 1
- status: fixed
- fix: 6341a1c, 2026-09-20, the sentence names the unreadable engine path as well

V-02 added a second refusal at `cohtml_hooks.cpp:223` that deliberately prints no version, and the doc
sentence written in the same round still said the log names the version it found beside the one it
wanted. True on one path of two.

## The verifier's verdict

Clean. All five defects gone, no correctness regression, three nits of its own listed above.

The one it was asked to be hardest on was H-01's second direction, since standing the move down on a
machine where it used to work would be worse than the bug. It walked the real order and the edge holds:
`InstallResponsiveUi` registers `OnFrameEnd` before `InstallCohtmlHooks` runs, `InstallUiFrameHooks` sets
`g_origEndFrame` twenty lines before the exe import is patched, and that import is the only way
`OnLibrary` can ever be entered, so `UiFrameEndHooked()` is already true by then. `HookVtableSlot` writes
`*orig` only after `VirtualProtect` succeeds, so a non null value really does mean the slot carries the
wrapper. No false negative and no false positive. It also noted that in the one case where the new guard
does stand the move down, an exe stamp or thunk mismatch, `g_frameThread` would have stayed 0 anyway, so
the move was already dead and nothing is lost.

## The runtime gate, 2026-09-20

Owner driven launch of 0.3.3 with the batch installed, on game 0.9.1 and Cohtml 1.61.0.3:

```
[09:27:15.003] [restyle] UI restyle fix on, ...
[09:27:15.003] [menus] menu refresh fix on, ...
[09:27:15.004] [styles] style matching fix on at 5 of 5 places, ...
[09:27:15.004] [children] child removal fix on at 8 of 8 places, ...
[09:27:15.004] [cohtml] UI engine 1.61.0.3 (stamp 0x675439C7), the build its objects were read from
[09:27:15.004] [cohtml] Library::Initialize, 1 import slot(s) of the exe patched
[09:27:20.803] [responsive ui] resource work the game's frame thread picks up runs on the mod's thread
[09:27:20.827] [cohtml] view #1 1920x1080 created
[09:27:20.827] [responsive ui] page fixes added to the menu and HUD view
```

That closes the batch on all three counts. The guard recognises the supported build and says so, so the
constants are right and F-01's fix does not turn away the build it is meant to accept. The import is still
patched one line later, so nothing downstream broke. And both responsive UI parts still install, which is
H-01's non regression observed rather than argued, since the resource work move is exactly what would have
gone missing if `UiFrameEndHooked()` had been wrong. All four byte patches still report their full counts,
5 of 5 and 8 of 8.

Batch 1 is done. Gates green, review, hunter and verifier all closed, runtime confirmed.

## The runtime gate for batch 2, 2026-09-20

A second owner driven launch, with both batches installed, over two session loads:

```
[10:04:12.583] [cohtml] UI engine 1.61.0.3 (stamp 0x675439C7), the build its objects were read from
[10:04:19.064] [responsive ui] resource work the game's frame thread picks up runs on the mod's thread
[10:04:19.091] [cohtml] view #1 1920x1080 created, the menu and HUD view
[10:04:19.091] [responsive ui] page fixes added to the menu and HUD view
[10:04:24.341] [cohtml] view #2 1024x1024 created
[10:04:24.342] [cohtml] view #3 512x128 created
[10:04:24.344] [cohtml] view #4 512x512 created
[10:04:35.494] [cohtml] view #5 512x2048 created
```

Eight views over the session, exactly one carrying the menu and HUD marker, one page fixes install, no
size warning, no engine refusal and no work move stand down. Both 1024x1024 displays and both 512x128
displays arrive unmarked, which is the collision of V-09 not happening, and the second session load makes
its displays again without any of them claiming the identity.

That is the identification working end to end: the menu view recognised, every car display correctly
passed over, and the page fixes landing exactly once. What it does not exercise is the case the batch
exists for, a teardown and a remake, which no recorded session has ever produced. BUG-033 holds the part
that remains unproven.

Batch 2 is done.

A second verifier pass then ran over the two commits made after the first clean verdict, since those were
unverified by definition. It proved the bound arithmetic exactly, the largest `at` is 1008 and the last
byte read is `size - 1`, checked the 1024 cap against seven real Windows PEs where the fixed block always
sits at offset 40 in a resource of 896 to 940 bytes, and confirmed the log split leaves no path silent. It
returned not clean, on the documentation rather than the code, which is V-04 and V-05 above.

## The batch 2 verifier's verdict

Not clean on the first pass, on V-06 above. F-02 and H-05 both proved gone.

F-02: a menu view remade at 1920x1080 as view 54 packs to the same size as the claim, is recognised, gets
both scripts, and `g_mainView` moves to it. The ordinal path is exactly as reliable as before, since
`firstView ||` short circuits. It also noted that `Hook_CreateView` returns before the counter increments
on a refused creation, so view number one is always a view that exists.

H-05, walked in all three cases it was asked for. A first view of size zero still takes the identity and
every later view then needs `size != 0 && size == 0`, which is unsatisfiable, so no car display can take
it. A later view of size zero is rejected. Two concurrent creations cannot both be first, because the
counter is a sequentially consistent read modify write, and only the first view writes the claimed size,
so a race can cause a missed match and never a false claim.

It also cleared the change I was least sure of, the probe's eviction rule. Rows hold unique view pointers,
so at most one row can equal `g_mainView` and with 32 slots at least 31 stay candidates, and the write is
guarded regardless. It called the change a net improvement, because the old rule pinned the dead view one
row forever after a remake and left the live menu view evictable.

Two things it raised are the owner's rather than mine. `CHANGELOG.md` has an empty `0.3.3 (unreleased)`
heading while this branch restores menu fixes after a device change, which is player visible, and that
wants a line before any release. And nothing here has been run: no launch has exercised batch 2, so
everything above is source and build only.

### V-10: filing BUG-033 left the spine's bug count stale
- severity: debt
- found-by: verifier
- batch: 2
- status: fixed
- fix: 95c5c96, 2026-09-20, `.agent/INDEX.md` reads 13 open, checked against the files rather than by eye

`spec/conventions.md` says an index that disagrees with its folder is a bug, and CLAUDE.md 8.1 asks for
the index line in the same commit as the change. 853fae7 added a bug file and touched the bugs index and
not the spine.

This is the fourth miss of this exact class on one branch, after V-03, H-07 and V-07, and it came out of
the commit that was fixing the third one. That is not four slips, it is a working method that treats the
paper as cleanup after the code rather than as part of the change.

What changes for the rest of the review: a commit that adds, removes or reclassifies a tracker file moves
its folder index and the spine in the same commit, and the counts are checked by counting the files, not
by reading them off the previous line. The check is two shell commands and it now runs before any commit
that touches `.agent/`.

### V-11: the zero size log named one of the two ways a view can have no size
- severity: nit
- found-by: verifier
- batch: 2
- status: fixed
- fix: 95c5c96, 2026-09-20

The test is `size == 0`, which is true both when the settings pointer could not be read and when readable
settings say zero by zero. The line said "could not be read" and the header comment made the same
conflation. Both now say the view has no size, which is what is actually known.

### H-11: a held controls refresh rebuilds a page the player has already left
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 29d4edf, 2026-09-20, the timer drops the refresh when the element is no longer connected

The game answers a slider drag with a soft refresh every 30 to 40 ms, so a drag almost always ends with
one refresh sitting in the waiting slot and up to 100 ms left on its timer. The player releases the slider
and presses Escape or opens another settings page inside the same document. Nothing cancels the timer, so
it fires afterwards and calls the stock refresh on the controls element, rebuilding every binding row and
resyncing every slider, which BUG-025 measures at 17 to 52 ms plus a whole page navigation scan.

The stall lands a frame into the page the player just opened and is read as that page's cost. If the
element is detached the rebuild throws instead and the catch sends it to the console only, so the frame
cost appears with nothing in acevo_perf.log to explain it. Stock had no such window, because its refresh
ran while the page was still up.

### H-12: the fix for F-04 cleared a mark whose request was genuinely in flight
- severity: bug
- found-by: hunter
- batch: 3
- status: fixed
- fix: 29d4edf, 2026-09-20, whether our own Init went out is tracked in a flag instead of inferred, and only our own wrapper is taken back

ffbe6b9 inferred "no Init went out" from `client.request !== stockRequest`. With two vehicle setup
elements on a shared Client that inference is wrong in both directions. Element A installs its wrapper and
has not sent Init yet. Element B inits inside that window, captures A's wrapper as its stock request and
installs its own. B's init sends Init, which unwinds through both wrappers down to the real request, so by
the time B's `finally` runs the real request is back in place, B compares it against A's wrapper, concludes
no Init went out, puts A's wrapper back and zeroes its own mark while its Init is in flight. B's duplicate
init a few milliseconds later is then no longer suppressed and BUG-026 returns for B.

Created by the F-04 fix, not by F-03. Before it the same interleaving left the marks alone. Unobserved,
and it needs two of those elements overlapping, which nothing in the repo evidences.

### H-13: the same inference only held while the page sends its request synchronously
- severity: debt
- found-by: hunter
- batch: 3
- status: fixed
- fix: 29d4edf, 2026-09-20

"The wrapper is still installed at return" means "no Init will ever go out" only if the send is
synchronous. If the page ever deferred it by a microtask or a timeout, the synchronous part of init would
return with the wrapper still in place, the mark would be cleared, and BUG-026 would be quietly back on
every open with the ignored counter reading zero.

That is the same shape as the defect this batch is fixing, a lifetime tied to the wrong event, one level
up. The evidence says the send is synchronous today, since BUG-026 was verified fixed and an async send
under the old code would have stranded the mark for three seconds on every open, which was never reported.
Tracking the flag removes the assumption rather than resting on it. With an async send the fix now
degrades to no suppression at all rather than to suppression that sticks, which is the right direction to
fail in.

### H-14: the trailing navigation scan did not claim the frame it landed in
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 29d4edf, 2026-09-20, the callback sets the scanned frame

A call arriving later in the same frame found the previous frame's number and took the full scan path, so
that frame paid two whole page scans at about 3.1 ms each, which is exactly the cost the fold exists to
remove. The lap of 2026-09-15 counted 1,011 calls, 919 folded and 144 scans, 52 of them trailing, so 52
frames in one lap each began with a scan that did not protect them.

### H-15: the one assignment that reaches the page's own object sat outside the try
- severity: nit
- found-by: hunter
- batch: 3
- status: fixed
- fix: 29d4edf, 2026-09-20, the install falls back to the stock init if the client refuses the write

`client.request = ...` ran in strict mode outside any guard. If Cohtml's binding layer ever made that
property non writable, an accessor without a setter, or sealed the client, the assignment would throw
straight out of the patched init, the page's own init would never run and vehicle setup would open blank.
The system doc says every patch wraps the stock method and falls back to it, and this one did not, over
the one write that reaches an object the mod does not own.

## The batch 3 verifier's verdict

Clean. It went further than the source and extracted the game's own `uiresources\js\components.js` from
content.kspkg, which settles three things the ledger had been reasoning about blind.

H-13's assumption is now confirmed rather than assumed. `VehicleSetupPage.init` is nothing but
`this.Client.request("Init", cb)` as its first statement, so the send really is synchronous, and the name
and the two argument shape both match what the wrapper tests for. The `name !== 'Init'` check became load
bearing with the F-04 fix, since before it a wrong name still suppressed by accident through a stranded
mark and after it a wrong name would clear the mark and put BUG-026 back silently, so having it checked
against the real call matters.

H-12's premise turns out to be unreachable in this build. `VehicleSetupPage`'s constructor does
`this.Client = new MessageHandler("CarSetup")`, so every element has its own Client rather than sharing
one, and the templates contain exactly one `ks-page-vehiclesetup`. The fix is correct and defensive
rather than load bearing here.

H-11's guard is confirmed as the right one. `data-bind-if` is what puts the controls page on screen, so
leaving it really does disconnect the element, and the game's own code uses the same `isConnected` test.
Using `=== false` rather than a falsy test is deliberate and right: where the property is missing the
refresh still runs, whereas a falsy test would silently drop the last step of every drag, which is the one
thing the hold exists to preserve.

One residual it names, which predates the batch. In the chained wrapper case the outer element's own Init
goes out unwrapped, so its mark is cleared by the inner element's answer or by the three second timeout
rather than by its own. It degrades to no suppression, never to a stranded mark, and it is structural to
the chained design rather than something this batch created.

## The runtime gate for batch 3, 2026-09-20

Owner driven menu session with `[developer] ui_probe=1`, which is the only way these counters reach a
log, read from the game's own log because the page script reports through Cohtml's console. The probe was
switched back off afterwards.

Vehicle setup, opened and reopened:

```
navigation calls 19 scans 3 skipped 17  | setup init 2 ignored 1
navigation calls 38 scans 4 skipped 35  | setup init 2 ignored 1
navigation calls 55 scans 3 skipped 53  | setup init 2 ignored 1
```

`setup init 2 ignored 1` on all three opens. The page asks twice, the wrapper catches the second, and the
suppression BUG-026 rests on is live under the rewritten wrapper. That is the line the verifier named as
the one that would read `ignored 0` if the wrapper had stopped catching the Init, which is the silent
failure the batch was most at risk of.

The controls page under a slider drag:

```
navigation calls 30 scans 26 skipped 14  | controls refreshes 11 held 23
navigation calls 33 scans 31 skipped 14  | controls refreshes 11 held 23
navigation calls 19 scans 19 skipped 5   | controls refreshes 5 held 7
```

Thirty four soft refreshes a second arriving, eleven run and twenty three held, so the throttle is doing
its job. The navigation fold shows its two regimes plainly: 154 of 159 calls folded on an ordinary menu
page, and a much weaker ratio during the drag, which is expected rather than a regression, because each
refresh rebuilds its rows in its own frame and each of those frames legitimately earns one scan.

No `responsive ui ... failed` line anywhere in the session, and every page reports `responsive ui page
fixes on`, so the script itself installed and none of its patches threw.

What this run cannot show. H-11's guard has no counter, since the drop path returns silently, so a refresh
skipped because its page had gone is indistinguishable in the log from one that never arrived. The run
proves nothing went wrong after leaving the page mid drag and does not prove the guard fired. Making that
observable is a counter in the script and belongs with the rest of batch 5.

Batch 3 is done.

### H-16: the stop was set by two hooks and cleared by one restart trigger
- severity: bug
- found-by: hunter
- batch: 4
- status: fixed
- fix: d10cb26, 2026-09-20, `Hook_StopWorkers` clears the flag after the original returns

F-05's fix attached the clear to `Library::Initialize` alone, while `StopMovingWork` is called from both
`Hook_StopWorkers` and `Hook_Uninitialize`. A `StopWorkers` the game does not follow with an
`Initialize`, because the library object survives, left the flag latched and the move refusing every call
for the rest of the process, with the install line still standing from start-up and nothing contradicting
it. That is F-05's exact failure on the sibling path its own fix did not cover.

### H-17: a call that was given up on made every later stop pay the full timeout, twice over at shutdown
- severity: bug
- found-by: hunter
- batch: 4
- status: fixed
- fix: d10cb26, 2026-09-20, the give up is remembered and no later stop waits again

`g_movedRunning` is decremented only when the moved call returns, and the case the bound exists for is
precisely the one where it never does, because the frame thread it is waiting on has left for
`StopWorkers`. So the counter stayed at one for the life of the process. `Hook_StopWorkers` paid 5000 ms,
then `Hook_Uninitialize` paid another 5000 ms on the same stuck counter, freezing the frame thread for
ten seconds at every exit. The bound added to keep a player out of a hang would have produced one.

### H-18: after the timeout the caller tore down the library the moved thread was still inside
- severity: bug
- found-by: hunter
- batch: 4
- status: fixed, contained rather than removed
- fix: d10cb26, 2026-09-20, the call is guarded and says so in the log if it faults

The comment written with F-06's fix named the trade as risking "tearing Cohtml down under its own worker"
without naming what that is. `g_origStopWorkers` joins and destroys the worker pool and its queues while
the mod thread is executing an item out of one, then `g_origUninitialize` frees the library, and the mod
thread's return path or its next vtable read lands in freed memory and makes an indirect call through it.
On a thread the game knows nothing about, with no handler, that is an unhandled access violation.

Contained, not removed. The fault is now caught and logged. What removes it is not giving up while a call
is live, which is what V-12 restored.

### H-19: the wait was documented as bounded and was not
- severity: debt
- found-by: hunter
- batch: 4
- status: fixed
- fix: d10cb26, 2026-09-20, the bound is a deadline from `GetTickCount64` and the sleep gets the remaining time

A spurious wake, which `SleepConditionVariableSRW` is documented to allow, restarted the whole five
seconds because nothing tracked elapsed time across iterations. With one worker thread `g_movedRunning`
is only ever zero or one, so a genuine wake always means zero and every restart of the timer was a
spurious one.

### H-20: the drain removed entries without taking back their semaphore counts
- severity: debt
- found-by: hunter
- batch: 4
- status: fixed
- fix: d10cb26, 2026-09-20, each drained entry consumes its count with a zero timeout wait

Left behind, those counts wake the thread once per drained entry after the next restart, each wake taking
the lock the frame thread wants. Worse at the ceiling: a full queue drained puts the count at
`kMaxMoved`, so the first `ReleaseSemaphore` after the restart fails, its return is unchecked, and that
entry sits queued while `Hook_ExecuteWork` has already returned 0 claiming the work is handled.

### H-21: the install line was printed whatever the three hooks actually did
- severity: debt
- found-by: hunter
- batch: 4
- status: fixed
- fix: d10cb26, 2026-09-20, the line is printed only once `g_origExecuteWork` is set

`HookVtableSlot` returns silently when `VirtualProtect` fails, which is what a page protection product
would produce, leaving nothing hooked and nothing moved while the log still said the move was installed.
F-01 and H-01's theme one layer down.

### H-22: the frame thread is a recyclable id that was never cleared
- severity: nit
- found-by: hunter
- batch: 4
- status: fixed
- fix: d10cb26, 2026-09-20, `StopMovingWork` zeroes it and the next frame end learns it again

Windows reuses a thread id once the game's UI frame thread exits, and `Hook_ExecuteWork` would then match
the new owner of that id and move its work. The same identity by token class as F-02 and H-09.

### H-23: Cohtml slots 2 and 3 have no recorded provenance and have never been seen firing
- severity: debt
- found-by: hunter
- batch: 4
- status: open, made observable rather than settled
- fix:

The research that established this vtable records slot 1 as `CreateSystem` and slot 5 as `ExecuteWork`
and names slots 2 and 3 nowhere. `responsive-ui.md` asserts them, and both are written on every player's
machine. If slot 2 is not `StopWorkers`, the mod replaces some other virtual with a `void(void*)` that
runs the drain and then calls the original with clobbered argument registers. That is F-01's failure shape
with the build check passing rather than failing.

Twelve recorded sessions show all three slots hooking and none shows either firing, because nothing
logged the drain. d10cb26 adds a line on every stop, so the next session that stops Cohtml will say so.
Settling it properly needs the disassembly, which is not this branch's work, and the log line is what
makes the next report actionable.

**Answered in part on 2026-09-20.** An owner driven launch and a clean quit through the game's own exit,
with the drain line in place, produced no drain line at all. The session ends `[streamer] at exit` then
`detached`, so the mod's own teardown ran and the game simply never calls either hook.

That does not prove slot 2 is `StopWorkers`, and it removes most of the reason to care. The danger in a
wrong slot number is replacing a virtual the game actually calls, and if slot 2 or 3 were some other live
method the drain line would have appeared. Whatever those two slots are, nothing in a normal session calls
them. The finding stays open because the numbers are still unproven, at a severity the evidence no longer
supports treating as urgent.

**Corrected on 2026-09-24.** The drain line is written only when work was left over or a call never came
back, so the quit above says nothing about whether either hook ran, and the game's own log has
`Uninitializing COHTML library!` 0.8 to 5.9 s before `detached` in every session that reached `detached`.
`fix/review-shutdown` F-05 has `Hook_Uninitialize` write a line every time, so the next quit shows whether
slot 3 fires as the game uninitialises.

**Answered for slot 3 on 2026-09-24.** On that branch's quit the hook on slot 3 wrote its line at
16:52:35.336 and the game's own `Uninitializing COHTML library!` came at 16:52:35.341, from inside the call
the hook had just passed on. Slot 3 is `Uninitialize` in all but a disassembly. Slot 2 is still unseen,
so the finding stays open for it.

### V-12: giving up on a call was permanent, so every later teardown destroyed the library under a live one
- severity: bug
- found-by: verifier
- batch: 4
- status: fixed
- fix: eb1c6f4, 2026-09-20, the flag clears when the abandoned call comes back

H-17's fix set `g_movedAbandoned` and never cleared it. The batch's own reasoning says the wedge is
temporary, since the timeout is what frees the frame thread to go run the job the moved call is waiting
on, so the normal outcome is that the call returns a moment later and the thread is healthy again. With
the flag latched, every stop after that first timeout skipped the wait entirely, tore the library down
under a live call, and made H-18's guard the normal path instead of the last resort. A Cohtml lock held by
the mod thread when it faults is never released either, which can wedge the very `StopWorkers` that
follows.

Clearing it when `g_movedRunning` reaches zero keeps H-17 closed, because a thread that is genuinely
wedged never reaches that line.

### V-13: the queue accepted work while the only thread that drains it was still out
- severity: bug
- found-by: verifier
- batch: 4
- status: fixed
- fix: eb1c6f4, 2026-09-20, `MoveWork` refuses while a call has been given up on

`MoveWork` tested only the stop flag and the queue depth. After a restart cleared the stop, it would
accept sixty four consecutive calls and return 0 for each, claiming work that nothing could run, until the
queue filled and the engine's own inline path resumed. Impossible before this batch, because the latch
kept everything inline.

### V-14: the semaphore was signalled outside the lock, so the drain could miss a count
- severity: debt
- found-by: verifier
- batch: 4
- status: fixed
- fix: eb1c6f4, 2026-09-20, the release moves inside the lock

A stop landing between the push and the release drained the entry and found no count to take, and the
release then landed on an empty queue. Harmless on its own, and it is exactly the ceiling case H-20's fix
claims to prevent.

### V-15: the fault guard caught every exception code, not only the access violation
- severity: nit
- found-by: verifier
- batch: 4
- status: fixed
- fix: eb1c6f4, 2026-09-20, the filter tests `EXCEPTION_ACCESS_VIOLATION` and lets everything else through

Catching everything would swallow a stack overflow, and the log call in the handler would then fault again
on the unreset guard page.

### V-16: on a restart with a different vtable the install line can still claim the move is on
- severity: nit
- found-by: verifier
- batch: 4
- status: wontfix, recorded
- fix:

`HookVtableSlot` returns early when its own original is already set, so if a restart ever handed
`OnLibrary` a library whose vtable had moved, nothing would be hooked and H-21's guard would still see a
non null `g_origExecuteWork` from the first library and print the line. It needs a relocated or different
vtable, which the UI engine check in `cohtml_hooks.cpp` already refuses at the module level, and the state
stays self consistent because the two stop hooks are equally unhooked. Not worth a flag to track per
library when the module check above it is the real guard.

## The batch 4 verifier's verdict

Not clean on the first pass, on V-12 above, which was introduced by the fix for H-17.

What it proved gone: F-05's restart path, walked global by global through stop, uninitialise, initialise
and `OnLibrary`. H-16's window, where the clear now sits before a teardown, turns out to be safe because
`Hook_ExecuteWork` also demands the current thread be the frame thread and `StopMovingWork` zeroes that,
so the window only opens if the game runs another frame end, which is the case the fix exists for, and
`Hook_Uninitialize`'s own drain catches anything queued in it. H-19's arithmetic holds, with the remaining
time always between 1 and 5000 so the cast cannot produce `INFINITE`. H-20's takeback cannot block under
the lock and cannot steal another entry's count. H-22 cannot suppress a legitimate move for longer than
one frame.

It also confirmed the fault guard is safe: no mod lock is held when `RunMovedWork` is called, and `Log`
takes only its own lock with no call out while holding it, so neither new log line creates a cycle.

One process finding of its own, and a fair one. The ledger had no rows for any batch 4 hunter find and
still read `status: open` on F-05 and F-06 while both were fixed, because this batch ran the verifier
before writing the paper, where batches 2 and 3 wrote it first.

Closing that out surfaced two more of the same: F-03 and F-04 also still read open, fixed in batch 3 and
confirmed on a probe run two batches earlier. So the count on this branch is V-03, H-07, V-07, V-10 and
these, which is six instances of one habit across four batches. The rule written after V-10, that a commit
moves its own paper, only ever covered the tracker indexes. It now covers the ledger's own status lines
too: a finding's status and fix field move in the commit that fixes it, not when its batch closes.

## The runtime gate for batch 4, 2026-09-20

Owner driven launch to the main menu and a quit through the game's own exit, probe off.

```
[11:17:30.817] [cohtml] UI engine 1.61.0.3 (stamp 0x675439C7), the build its objects were read from
[11:17:36.433] [responsive ui] resource work the game's frame thread picks up runs on the mod's thread
[11:17:36.458] [responsive ui] page fixes added to the menu and HUD view
[11:17:58.164] [streamer] at exit: ...
[11:17:58.164] detached
```

Two things settled, one of them uncomfortable.

The install path is right. The line still prints, which means `g_origExecuteWork` really was set, so the
guard added for H-21 is not refusing a working install, and the `g_movedThreadUp` gate did not break the
first time through.

And the stop path never runs. No drain line anywhere in the session, on an exit clean enough to reach
`detached`. So the game does not call `Library::StopWorkers` or `Library::Uninitialize` on the way out at
all, and with no Cohtml restart in any recorded session either, the whole of `StopMovingWork`, the bounded
wait, the drain, the abandonment flag and the fault guard is code that has never executed and has no known
path that would execute it.

Worth saying plainly rather than burying: batch 4 spent two rounds of hunter and verifier, five commits
and three of my own regressions on a path that does not run. The fixes are correct and their cost at
runtime is nothing, so there is no case for taking them out, and F-05's defect was real in the sense that
the code was wrong. But the effort was out of proportion to the risk, and the reason nobody could tell is
the same reason H-23 was unfalsifiable: there was no log line. The one line added here answered in a
single launch a question twelve recorded sessions could not.

The lesson for the batches that follow: when a fix's whole surface is a path with no evidence of ever
running, log it first and weigh it second.

**Corrected on 2026-09-24, from `fix/review-shutdown`.** The line added here is written only when work was
left over or a call never came back, so its absence did not show the stop never ran, and the lesson
applies to it too, a line that proves a path runs has to be written every time it does. See H-23.

Batch 4 is done.

### F-08: the script installs its machinery on hud.html, where none of its patches can apply
- severity: debt
- found-by: hunter, batch 3
- batch: 5
- status: fixed
- fix: 3586169, 2026-09-20, the script returns at once on hud.html and installs nothing there

The same view carries the menus and the driving HUD, so the script is evaluated again at every session
load. Neither page element exists on the HUD and the navigation fold sees one call in a whole session,
while the frame counter it needs is a callback every frame for the rest of the session, on the page
BUG-009 ties to the one percent lows.

Fixed twice. The first attempt (0d907f7) started the counter lazily from the navigation fold instead of
gating on the page, on the assumption that the fold would not install on the HUD. H-24 showed from the
repo's own logs that it does.

### F-09: the log claimed the page fixes were in, and the marker proved only that the script started
- severity: debt
- found-by: hunter, batch 3
- batch: 5
- status: fixed
- fix: 0d907f7, 2026-09-20, the marker is published last and the log line says the script was given to the view

`window.__acevoUiFixes` was assigned at the top of the script, so its presence meant the script had begun.
It is now the last statement, so it means the script ran to the end. The mod's own side cannot see that at
all, so its line now claims only what it knows.

### F-10: the navigation fold's only way out of the folded state is a future frame
- severity: debt
- found-by: hunter, batch 3
- batch: 5
- status: wontfix, no timer can close it
- fix:

Tried and removed. A `setTimeout` alongside the frame looked like the obvious fallback and cannot work:
`ui-lag-hunt-2026-09-06.md:68` records that when the window loses activation the UI's clock stands still,
and Cohtml dispatches timers out of the same view advance that runs frame callbacks, so a timer deadline
in a frozen view never comes due either. It could not fire in the one case it was added for, and it made
things worse on the way (H-26 and H-27).

What actually recovers a frozen view is its first input event, which resumes frame callbacks too. So the
folded state is never stuck for longer than the view itself is, and there is nothing left to fix.

### F-11: the controls throttle spaced refresh starts rather than the gaps between them
- severity: debt
- found-by: hunter, batch 3
- batch: 5
- status: fixed
- fix: 0d907f7, 2026-09-20, the stamp moved into a finally after the refresh

The window was consumed by the refresh itself, so a rebuild costing more than 100 ms made the next arrival
due the moment it returned. Refreshes then ran back to back with no idle frame between them and the held
counter read zero exactly when the page was slowest.

### F-12: the moved work counter counts calls taken and never calls refused
- severity: nit
- found-by: hunter, batch 3
- batch: 5
- status: fixed in part
- fix: 0d907f7 then 3586169, 2026-09-20, a full queue says so once per burst

The queue filling is the one condition that hands the stall back to the frame thread mid session and it
was invisible. It now logs, and re-arms when the burst drains so a second fill is not swallowed. Counting
completions as well as queued calls would need the probe's line to change and is not worth that on its
own, so it stays unfixed with this written down.

### H-24: the lazy frame counter starts on hud.html too, so the first fix removed nothing
- severity: bug
- found-by: hunter
- batch: 5
- status: fixed
- fix: 3586169, 2026-09-20, the whole script returns on hud.html by pathname, as the probe's sibling script does

The gate chosen was that the navigation fold had installed. `hud.html` sets `SpatialNavigation` with a
`makeFocusable` like every other page, so the fold installs there and starts the counter anyway.

The evidence was already in the repo and I did not look. `logs/ui-probe-a-20260914/game_log.txt` records
the probe patching navigation on hud.html three times, from a guard identical to this one, and four
2026-09-15 logs show this script's own counter reading `navigation calls 1 scans 1 skipped 0` on hud.html
at every session load, which can only increment inside our own wrapper. Over 1,188 recorded hud.html
seconds the fold is installed on every one of them.

### H-25: moving the counter out of the top level dropped the invariant H-14's fix rests on
- severity: bug
- found-by: hunter
- batch: 5
- status: fixed
- fix: 3586169, 2026-09-20, the counter is back at the top level and the comment saying why is back with it

The comment deleted in the same edit read "Registered before any page script, so it runs first in every
animation frame and the page's callbacks see the number of the frame they run in." That was load bearing.
Frame callbacks run in registration order and this one re-registers itself as it runs, so wherever it
first lands it stays. Registered by the initial script it is first by construction. Registered from the
`SpatialNavigation` setter it lands behind every callback the document registered before that assignment,
and a `makeFocusable` call made from one of those reads the previous frame's number. Worse, a trailing
scan registered from such a callback is queued ahead of the counter for the next frame, so it claims the
old frame number and the next fresh call takes the full scan path. That is H-14 exactly, the finding batch
3 fixed and whose runtime gate counted 52 such frames in one lap.

Deleting a comment that records why something is where it is, in the commit that moves it, is the clearest
instance on this branch of the thing section 2.1 of the project's own rules exists to prevent.

### H-26: the timer fallback cannot fire in the case it was added for
- severity: bug
- found-by: hunter
- batch: 5
- status: fixed
- fix: 3586169, 2026-09-20, the timer is gone and F-10 is closed as wontfix with the reason

See F-10. The research doc says the view's clock stands still when the window loses activation, and Cohtml
dispatches timers from the same advance, so the timer is dead in exactly the frozen case. What it did buy
was a real cost: one allocation and one dead dispatch per fold episode, about ten a second on the controls
page during a drag, on the page the fold exists to speed up.

### H-27: on a frame longer than the timer's delay the timer won and claimed a stale frame number
- severity: bug
- found-by: hunter
- batch: 5
- status: fixed
- fix: 3586169, 2026-09-20, the timer is gone

Both callbacks come due at the same advance once the frame interval passes 100 ms, and a due timer runs
before that turn's frame callbacks, so the trailing scan fired before the counter incremented, claimed the
outgoing frame, and the first fresh call of the new frame took a full scan. That frame then paid two whole
page scans. Frames over 100 ms are not exotic on these pages: the deep dive measures controls group
switches at 197 to 494 ms and the vehicle setup open at 237.8 ms, which are precisely the frames BUG-025
is about.

### H-28: the fix changed three documented behaviours and moved no paper
- severity: debt
- found-by: hunter
- batch: 5
- status: fixed
- fix: 50d7719, 2026-09-20

Sixth instance on this branch, after V-03, H-07, V-07, V-10 and the ledger's own status lines, and it came
one batch after the rule was widened to cover exactly this. Noting it here rather than widening the rule
again, because the rule is not the problem.

### H-29: the queue full line was latched for the life of the process
- severity: nit
- found-by: hunter
- batch: 5
- status: fixed
- fix: 3586169, 2026-09-20, the flag re-arms when the queue drains

The condition it reports is a burst that drains and recurs, so a permanent latch would have reported the
first fill and hidden every later one. The same latch shape as F-05 and H-16, on the line added to close
that kind of blindness.

### H-30: the install line was gated on one of the three hooks it claims
- severity: nit
- found-by: hunter
- batch: 5
- status: fixed
- fix: 3586169, 2026-09-20, all three originals are checked

With the two stop hooks missing there is nothing to drain the queue at teardown and a moved call can reach
a library that has already gone, which is worse than not moving at all. Thin, since the three slots share
a vtable and almost certainly a page, and the same one hook asymmetry H-21 was raised over.

### V-17: the three hook guard declined to claim the move without actually turning it off
- severity: nit
- found-by: verifier
- batch: 5
- status: fixed
- fix: 1069800, 2026-09-20, the refusal latches the stop flag as well as logging

H-30's guard returns when any of the three originals is null, and its comment says leaving the stop hooks
off is worse than not moving at all. The early return did not make that true. `Hook_ExecuteWork` can
already be in the vtable, the stop flag was cleared eight lines above, the worker is up and the frame
thread is known, so the move would have kept running with nothing to drain it at teardown while the line
said it was out. Before H-30 the log and the behaviour agreed, because a null work hook really did mean
the move was off. The guard now latches the stop, so the words are true again.

Reachability is near zero, since the three slots share a vtable page. It is filed at its real weight and
fixed anyway, because a comment claiming a protection the code does not deliver is the fault this branch
exists to remove.

### V-18: the queue full line never re-arms on the stop path
- severity: nit
- found-by: verifier
- batch: 5
- status: fixed
- fix: 1069800, 2026-09-20, the flag clears beside the stop flag in `Hook_StopWorkers`

H-29 re-arms the line when the worker drains the queue to empty. `StopMovingWork` drains it in its own
loop and never touches the flag, then `Hook_StopWorkers` clears the stop and the move resumes, so a queue
that was full when the engine stopped stayed silent for the rest of the process. The same latch shape as
F-05, H-16 and H-29 again, on the line added to close that blindness, for the third time.

### V-19: the parts table still quoted the log string the batch renamed
- severity: debt
- found-by: verifier
- batch: 5
- status: fixed
- fix: 1069800, 2026-09-20

`responsive-ui.md:29` held `[responsive ui] page fixes added` while the code logs `page fixes script given
to the menu and HUD view`, so the doc contradicted its own prose forty lines down and grepping the
documented string returned nothing. That Log column is how a session is verified.

Seventh instance of this class on the branch, and it is in 50d7719, the commit filed as closing the sixth.
Worth stating plainly rather than widening the rule an third time: the rule was already right, the failure
is that I check the prose I am writing and not the tables and strings around it.

### V-20: the probe reported the intended HUD state in the same word it uses for failure
- severity: nit
- found-by: verifier
- batch: 5
- status: fixed
- fix: 1069800, 2026-09-20, the HUD gets its own wording

With F-08 the script installs nothing on hud.html by design, so the probe's line started reading
`responsive ui page fixes off` there, which is the same word it prints when the script failed, and every
recorded HUD line before this read `on`. A log reader would take the intended state for a regression. The
probe already computed `onHud` on the line above and was not using it.

### V-21: the controls throttle's description no longer matched its mechanism
- severity: nit
- found-by: verifier
- batch: 5
- status: fixed
- fix: 1069800, 2026-09-20, both the doc bullet and the source comment say what F-11 made it do

"At most once every 100 ms" survives as a true upper bound and stopped being what the code does. With a
50 ms rebuild it now allows one refresh per 150 ms, because the window is measured from the end of a
refresh rather than its start. Both now say that.

## The batch 5 verifier's verdict

Not clean, on V-17 to V-21. All six defects proved gone.

Its evidence on F-08 is worth keeping. The pathname expression matches `ui_probe.cpp:38` character for
character, `location.pathname` excludes a query and a fragment by spec, and across every recorded session
that same expression yields only lowercase page names with no query, fragment or trailing slash, so it can
neither miss nor match a menu page. Every side effect in the script is textually after the gate, so the
HUD document gets no callbacks, no property redefinition and no marker. And the counter cannot leak in
from the previous document, because ten `ui probe ... loaded` lines in one recorded session behind a
same shaped guard prove every document load gets a fresh context.

It also measured what the gate gives up: over 1,188 recorded hud.html seconds the fold logged `calls 0`
1,181 times and `calls 1 scans 1 skipped 0` seven times, never once folding anything, while `ingame.html`
still installs and does real work. So the gate is narrow in the right direction.

One caveat it raised and I am leaving alone: the old stamp ordering incidentally protected against a
synchronous re-entrant `onDevicesChanged`, and the new one would run such a call immediately. It needs the
page's own rebuild to dispatch another refresh synchronously, and the Cohtml binding path is asynchronous,
so it is not reachable. Recorded rather than guarded.

## The runtime gate for batch 5, 2026-09-20

Owner driven launch with `ui_probe=1`, menus then a session so the HUD loads, then a quit. The probe was
switched back off afterwards.

The mod's own log confirms the build under test is the right one, which matters because the first attempt
at this gate was run against a stale install and proved nothing:

```
[12:00:11.027] [responsive ui] page fixes script given to the menu and HUD view
```

The game's log has the gate working page by page:

```
ui probe intro.html loaded, responsive ui page fixes on
ui probe menu.html loaded, responsive ui page fixes on
ui probe settings.html loaded, responsive ui page fixes on
ui probe singleplayer.html loaded, responsive ui page fixes on
ui probe ingame.html loaded, responsive ui page fixes on
ui probe hud.html loaded, responsive ui page fixes stay out of this page by design
```

Five menu pages install, the HUD does not, and V-20's wording is what makes the last line readable rather
than looking like a failure. The counters behind it agree: over the session `hud.html` logged 0 navigation
calls, `ingame.html` 49 and `settings.html` 164. So the script is out of the driving page and doing its
work everywhere else, which is the whole of F-08.

Batch 5 is done, and with it every batch on this branch.

## Hunter and verifier finds outside this batch's scope

Two, both already filed against other branches, recorded here so the extra detail is not lost.

`src/engine/exceptions.cpp:50`, filed as `sweep/review-engine` F-02. The hunter adds two things the review
did not have. A `throw new SomeError` throws a pointer, so `*(void***)object` reads the object address
itself as a vtable pointer. And because `Message` returns an empty string on failure, the guard at line 73
stays false, so the same wild call is repeated on every later throw from that site, each one inside the
shared lock, turning a hot throw site into a repeating multi hundred millisecond stall.

`src/core/iat.cpp:71`, filed as `sweep/review-proxy-core` F-14. Worth doing early rather than late: the
recorded logs already read "DXGI: hooked Cohtml Library::CreateSystem" and five more like it, and those
are the lines that prove which Cohtml slots were taken, which is exactly what a launch verifying this
branch has to read.

The verifier added a third, and it is the one that most limits what this batch achieved. H-03 hardened the
PE walk in `cohtml_hooks.cpp`, and the five sibling files walk the same headers with no `__try` and no
magic checks, `restyle_fix.cpp:78` and `:88` being the clearest. They run from `dllmain.cpp:69`, before
`InstallCohtmlHooks` at `:71`. So the DLL_PROCESS_ATTACH crash on a foreign build that H-03 describes is
still reachable, just from `restyle_fix` rather than from here, and this branch cannot close it without
taking files that belong to `sweep/review-ui-fixes`. It should be the first thing that branch does.

## Not filed

`src/ui/restyle_fix.cpp`, `src/ui/child_removal_fix.cpp`, `src/ui/style_match_fix.cpp` and
`src/ui/menu_refresh_fix.cpp` carry findings of their own, filed in
`.agent/reviews/2026-09-sweep-review-ui/ledger.md`, because they are a different surface and a
different branch.

Checked and clean: the install order is deliberately interleaved, child removal's hashed regions stop
at 0x3EDA75, exactly one byte before style match's patch at 0x3EDA76, and no region covers another
part's patch site. The chained `ExecuteWork` hooks resolve in the right order with no recursion.
