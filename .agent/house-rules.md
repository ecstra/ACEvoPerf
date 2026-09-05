---
name: house-rules-agent
kind: doc
description: the branch contract and code review protocol, the agent's operating version
updated: 2026-08-14
links: [spec-reviews, spec-bugs, spec-todos, agent-readme]
---

# House Rules

The development rules of every project that carries this directory. A
project may keep a short human cut at `docs/house-rules.md` and the hard
bans in a root CONTRIBUTING.md, both pointing here. The project's own
CLAUDE.md section 0 names its exact gates and tooling.

## The branch contract

1. **One branch, one intent.** A feature, a fix, a restructure: one
   coherent intent per branch, named `feat/<slug>`, `fix/<slug>`, or
   `sweep/<slug>`. Size is measured by intent, never by commit count: a
   long branch serving one intent is fine, a second intent born mid
   branch forks its own branch the day it appears, it never rides.
2. **Sweeps carry the small stuff.** Small bugs and small features group
   into a sweep branch by surface, one screen or one subsystem per sweep.
   A sweep never contains a breaking change.
3. **Branch from fresh main.** Merge whole, never cherry pick out of an
   unreviewed branch. A merged branch is deleted, its review ledger is
   the record.
4. **Main is reviewed code only.** Gates first, then review, then PR,
   then merge. No step skips.

## The gates

Before any review: every suite, analyzer, and build the project's
CLAUDE.md section 0 names, green. The review judges code that already
works, reviewers never burn effort on red suites.

## The review protocol

1. **The timing is the owner's, the running is the agent's.** The owner
   says when a branch gets reviewed and the agent never decides that on
   its own. On the word, the agent runs the review itself with the
   `code-review` skill at the level the branch earns: low or medium for
   a small or mechanical branch, high or max where the surface is wide
   or the logic is subtle. `ultra` is the owner's alone, being paid and
   cloud run, so the agent neither reaches for it nor asks for it by
   name. Codex `/review --base main` (GPT) is a second pair of eyes the
   owner can add. All of it works on the local branch, so no PR exists
   yet, by rule.
2. **Findings first, whole.** The agent reports every finding ranked by
   severity before touching anything. No fixing during reporting, no
   silent triage.
3. **The ledger.** All findings land in
   `.agent/reviews/<yyyy-mm>-<branch-slug>/ledger.md` per
   [spec/reviews.md](spec/reviews.md): severity graded (breaks, bug,
   debt, nit), split into themed batches ordered by severity,
   correctness first, nits last. A finding not worth fixing gets a
   written reason, so no future review re litigates it.
4. **The batch loop.** One batch at a time:
   - Fix the batch, one commit per fix, the hash recorded beside the
     finding.
   - A hunter sub agent sweeps the batch's scope for related bugs the
     review missed. Hunter finds enter the ledger as findings
     (`found-by: hunter`) and are fixed in the same batch. No backlog.
   - A separate verifier sub agent checks bug wise, not code wise: is
     each bug actually gone, did anything new surface in scope. The loop
     repeats until the verifier comes back clean.
   - Sub agents run one at a time, sequentially. This is the named
     carve-out from the single threaded research rule.
5. **Batch close.** Mark it in the ledger, report to the owner, and name
   anything unfixable in scope. Unfixables defer into `.agent/bugs/` as
   their own files, linked both ways, and earn a future branch sparingly,
   on the owner's call.
6. **The owner's ack gates each batch.** The next batch starts only when
   they say so.
7. **The closing pass.** Breaking or changing branches get one more
   normal review over the branch as it stands after the last batch, to
   catch what the fixes introduced. Sweeps skip this.

## Git rules

- Commits are allowed and expected: one commit per change, so every
  change is undoable and addressable by hash.
- Single author, no co-author, no AI attribution anywhere: not in
  commits, not in PR titles or bodies, no generated-with footers.
- Commit messages in the owner's voice: added:/fixed:/improved:/updated:/
  removed:, one short sentence, two clauses at most.
- Push after each commit.
- PRs only on the owner's explicit ask in the current message, through
  the project's PR tooling, base main. Merges only on the owner's
  explicit ask.
- Never commit to main directly.
- Worktrees are disabled: never create one, and never use a worktree
  isolation mode for sub-agents. If the owner ever explicitly asks, the
  one sanctioned home is `.agent/.worktrees/` (gitignored), nowhere
  else, torn down when the task ends.

## The trackers

Defects go to [bugs/](spec/bugs.md), work goes to [todos/](spec/todos.md),
the agent classifies on intake and uses todos extensively (multi step
work gets filed, not carried in a session's head). Owner written todos
get picked up, cleaned, and filed with their wording preserved, and on
completion the agent tells the owner which of their own notes to tick.
