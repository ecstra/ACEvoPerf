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

Seven findings from the review, one breaks, four bug, two debt, plus four the hunter added in batch 1,
one bug, two debt, one nit.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the vtable calls honour the build check the byte patches already do | done | 2026-09-20, ack, runtime confirmed |
| 2 | the menu view is found by identity rather than by a counter | fixed, owner's launch outstanding | 2026-09-20, ack |
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
- fix: 3ab0b2b, 2026-09-20, `.agent/INDEX.md` reads 13 open, checked against the files rather than by eye

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
- fix: 3ab0b2b, 2026-09-20

The test is `size == 0`, which is true both when the settings pointer could not be read and when readable
settings say zero by zero. The line said "could not be read" and the header comment made the same
conflation. Both now say the view has no size, which is what is actually known.

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
