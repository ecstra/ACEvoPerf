---
name: review-2026-09-sweep-review-ui-fixes
kind: review
description: the four shipped cohtml patches in the full review of main, a marking window that skips invalidation and a row of load bearing assumptions written down nowhere, twelve findings and one added by another angle's verifier
updated: 2026-09-23
links: [spec-reviews, house-rules-agent, responsive-ui, reviews-index]
branch: sweep/review-ui-fixes
status: open
---

# Review of the four shipped cohtml patches

## Summary

One angle of the full review of main run on 2026-09-20 at high. This angle covers
`src/ui/child_removal_fix.cpp`, `restyle_fix.cpp`, `style_match_fix.cpp` and `menu_refresh_fix.cpp`,
the four byte patches behind the responsive UI. They ship on by default.
`src/ui/responsive_ui.cpp` and `src/ui/cohtml_hooks.cpp` belong to `fix/review-cohtml-build-guard`.

These four are the best guarded code in the project. Every one checks the game stamp, hashes every
byte region before it writes, computes every displacement in advance and refuses cleanly on a
mismatch. The findings are about what happens after the patch lands: a marking window that produces a
silently stale page, hooks that walk engine pointers with no fault guard, and a set of assumptions
between the four files that nothing in the code records.

Twelve findings, one bug, seven debt, four nit. F-13 was added on 2026-09-23 by the verifier of
`sweep/review-overlay`, whose fix for a player's own stylesheet in the mods folder made it reachable
for the main stylesheet.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | a removal during a stylesheet parse still invalidates | pending | |
| 2 | the hooks cannot fault the game | pending | |
| 3 | a failed install says so | pending | |
| 4 | the load bearing assumptions are written down | pending | |

## Findings

### F-01: a feature set whose rules are not filed yet counts as narrowed with no compounds, so a removal in that window marks nothing and skips Cohtml's own invalidation
- severity: bug
- found-by: review
- batch: 1
- status: open
- fix:

`src/ui/child_removal_fix.cpp:333`. `Hook_ConstructFeatureSet` inserts an empty FeatureSet with
anyChild false, and `Hook_AddRuleFeatures` then fills it rule by rule. Between those two the set reads
as narrowed.

Failure: the responsive UI's `MovedWorkThread` runs a stylesheet parse off the frame thread, which is
the whole point of the move, while the UI thread removes a child in that style scope.
`MarkChildrenThatLookAtPosition` returns true after marking nothing, so the engine's own whole subtree
invalidation is skipped and that element's subtree never restyles. The page is silently stale.

### F-02: FeatureSetOf walks element to document to styler to bucket table with no null check, from a hook that has no fault guard
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/ui/child_removal_fix.cpp:263`. `Hook_ChildRemoved` runs on every child removal in the process,
including removals during document teardown and on elements outside any document. It dereferences
element+0x38, then document+0x248, then styles+0x1A0, then document+0x288 and +0x298, all before the
caller's own guard at line 265 gets a chance.

Failure: Cohtml's own slot 49 may bail earlier than the lookup this reproduces, so a torn down or
partly destroyed document reaches memory here that the engine's own path never touches. Nothing in this
file handles a fault, so that is a crash for the player, and by the project's own note even a handled
access violation costs the thread 120 to 210 ms.

### F-03: Patch patches[8] is hand sized for exactly the current site list, three lines after the count is computed
- severity: debt
- found-by: review
- batch: 2
- status: open
- fix:

`src/ui/child_removal_fix.cpp:444`. `constructorCount` is computed at line 443 from
`kConstructorCalls`, then line 444 declares the array and lines 448, 453 and 458 fill it with
`patches[patchCount++]` and no bound. Six constructor sites plus the rule add plus the removal is
exactly 8.

Failure: one more RVA added to `kConstructorCalls` writes past a stack array with no diagnostic, in a
function that is already writing into another module's code. Nothing ties the two numbers together, not
a `static_assert`, not a comment.

### F-04: a failed write of the one patch that matters leaves the other seven in and still logs the fix as on
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/ui/child_removal_fix.cpp:481`. `patches[7]` is the only hook that changes behaviour, indices 0 to
6 only feed the feature set table. If `WriteCode` fails for index 7 the code takes `continue`, falls
out of the loop, and reaches the success log at 489 printing "child removal fix on at 7 of 8 places".

Failure: nothing is fixed, every removal still takes Cohtml's whole subtree invalidation, and the
constructor and add rule hooks keep filling `g_featureSets` to 4096 entries for a table nobody reads.
The log tells the owner the opposite of what happened.

### F-05: the one failure path in InstallRestyleFix that does not free the stub page leaks it
- severity: nit
- found-by: review
- batch: 3
- status: open
- fix:

`src/ui/restyle_fix.cpp:129`. Every other bail out at lines 104, 115 and 122 calls `VirtualFree` on the
cave. The VirtualProtect failure at 128 returns without it, leaking the 4 KB reservation for the
process lifetime. Harmless in size, and it is the odd one out.

### F-06: the restyle stub reads the invalidation kind out of the caller's saved rbp, which only still works under the probe by accident of register allocation
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/restyle_fix.cpp:54`. The stub does `cmp byte ptr [rbp+0x30], 5`, which is valid only while the
immediate caller of 0x37B690 is cohtml 0x37BC60 with the kind still in ebp. With `ui_probe=1` the probe
replaces that call with its own C++ `Hook_Invalidate`, which then calls 0x37B690 itself. The reviewer
disassembled `build/ui_probe.obj` to settle it: `Hook_Invalidate` pushes rbp in its prologue and does
not write it before `call rax` at offset 0x56, so the kind survives today by luck.

Failure: any edit that makes MSVC allocate rbp before that call flips the guard. State changes take the
sibling walk again, meaning the fix is silently off, and a class or id change whose stale rbp byte
happens to read 5 skips the walk, leaving following siblings unrestyled. Every probe measurement the
responsive UI is tuned on rides on this.

### F-07: three cohtml fixes have byte windows sized to stop one byte short of another fix's patch site, so the install order is load bearing and stated nowhere in code
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/style_match_fix.cpp:207`. style_match patches 0x3EDA76 while child_removal hashes
0x3EDA50..0x3EDA76 and 0x3ED9D0..0x3EDAF9, and restyle hashes 0x37BC60..0x37BD11 while ui_probe
patches exactly 0x37BD11. Today `InstallResponsiveUi` runs restyle, menus, styles then children, and
dllmain runs the probe after, so every hash is taken before the next patch lands.

Failure: reorder any two of those calls, or widen one region by a byte, and the later fix refuses to
install with a "not the code this was written against" line that names the wrong cause. This is a
deliberate arrangement that reads as a coincidence.

### F-08: the two files disagree on how big a compound is, 72 bytes against a 0x40 stride
- severity: nit
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/style_match_fix.cpp:119` calls rdi "the compound, 72 bytes with its first simple selector in
place", while `src/ui/child_removal_fix.cpp:83` walks the compound array with `(index - 1) * 0x40`.
Both read the same shape, type at +0 and value at +8.

If 0x48 is the real stride, `CompoundAt` misreads every compound from index 2 on, which is where the
leaderboard rule's sibling combinator lives, and the fake memory test behind BUG-029 cannot catch it
because it is built from the same constant. 0x40 is probably right, since kCompounds at 0x40 leaves
exactly 0x40 for the inline compound and a wrong stride would have crashed the verification session, so
the comment is most likely the thing that is wrong. One of the two is, and only the disassembly settles
it.

### F-09: FeatureSetOf open codes a hash table probe with eight unexplained offsets in a file that names every other offset
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/child_removal_fix.cpp:261`. This file puts every structure offset into `namespace node` and
`namespace selector` with a comment each, and documents every RVA in its header block. Then
`FeatureSetOf` reads 0x248, 0x1A0, 0x288, 0x298, 0x290 and 0x299, strides by 0x18, multiplies the scope
by 0x9E3779B97F4A7C15 and walks a distance byte, with one line of comment naming only which engine
lookup it mirrors. Nothing records what the buckets are, why the probe stops on `entry[0] < distance`,
or what the line 274 fallback returns.

### F-10: OnLoadUrl ignores which view navigated, so any view's page load moves the schedule the main surface runs on
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/menu_refresh_fix.cpp:170`. The first parameter, the view, is unnamed and unused, and only the
URL is inspected. The car dashboard displays are Cohtml views too and load their own documents.

Failure: a display whose URL ends in "/hud.html" or matches any of `kMenuPages` writes `g_menuPage` and
the schedule byte as if the main view had navigated. Shipped this is masked because both branches of
`WriteSchedule` resolve to the same value, but with `hud_schedule_test=1` a display's load pins the
schedule and silently invalidates the turn the test thinks it is measuring. The system doc states the
stub records whether the main view shows a known menu page, which the code does not check.

### F-11: the patched stub carries two comparisons and two branches that can never be taken in a shipped run
- severity: debt
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/menu_refresh_fix.cpp:88`. The stub bytes at 88 and 90 encode comparisons against 0 and 2. The
schedule byte is written only by `WriteSchedule` at line 149 as
`g_menuPage ? kMainEveryFrame : g_hudSchedule`, so it is 1 either way unless `g_hudSchedule` changes,
and its one write at line 306 is inside `MenuRefreshTick`, which returns at 289 unless
`[developer] hud_schedule_test` is on. So `kGameRotation` and `kEverySurfaceEveryFrame` are engine
patch surface reachable only by a diagnostic that ships off. The comment on line 68 admits it.

### F-12: EncodeJump is written twice and both files already include the header that provides it
- severity: nit
- found-by: review
- batch: 4
- status: open
- fix:

`src/ui/menu_refresh_fix.cpp:193` and `src/ui/restyle_fix.cpp:65`, two identical five line bodies. Both
files include `acevo/core/code_patch.h` on their line 2, and `EncodeRel32(0xE9, from, destination, out)`
from that header is exactly what each one does. A reader sees three spellings of one operation.

### F-13: a player's own stylesheet can hold the rule the restyle stub assumes no stylesheet has
- severity: debt
- found-by: verifier
- batch: 4
- status: open
- fix:

`src/ui/restyle_fix.cpp:22` states the premise. The game's stylesheets hold no rule with a
pseudo-class to the left of `+` or `~`, so the stub skips the sibling walk for state changes. The
stub goes in whenever `responsive_ui` is on (`src/ui/responsive_ui.cpp:544`), and nothing checks the
premise against the stylesheets the overlay actually serves.

Failure: a UI mod in the mods folder ships its own `uiresources\css\uicomponents.css` with a rule
like `.row:hover + .hint`, and `responsive_ui` is on, the default. Hovering the row should restyle
the hint. The stub skips the walk, so the hint never changes, and nothing in the log says why.

Raised by the verifier on batch 1 of `sweep/review-overlay`. Its H-01 fix (1c8ab05) lets a player's
own copy of the main stylesheet reach the game at all, where before the mod's narrowed copy took its
slot, and the premise was checked against that copy. A player's copy of any other stylesheet was
already open to it. Left here because the file belongs to this angle. The fix may need more than
writing the premise down, for example standing the stub down when the overlay serves a stylesheet
of the player's.

## Checked and clean

The install order is deliberately interleaved and no region covers another part's patch site, which is
what F-07 is about recording rather than changing. The x64 unwind data in style_match_fix is right,
including the two `UWOP_SAVE_NONVOL` entries that land in the caller's home space. The `kMarkStub`
assembly is correct on stack alignment, shadow space and volatile registers.
