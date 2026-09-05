# Contributing

The rules of the road, human or agent. The operating version is
`.agent/house-rules.md` and CLAUDE.md carries agent
behavior.

## The absolutes

- **No commit lands on main directly.** All work rides a branch: one
  branch, one intent.
- **No PR exists before the branch's code review has closed.** The owner
  calls the timing and the agent runs it, its ledger lives under
  `.agent/reviews/`, and every finding is fixed, deferred with a written
  reason, or wontfixed with a written reason before the PR opens.
- **No merge without the owner's explicit ask.** Same for opening the PR
  itself.
- **Single author, no co-authors, no AI attribution anywhere.** Not in
  commits, not in PR titles or bodies, no generated-with footers.
- **Gates before review.** Suites, analyzer, builds, and validators are
  green before a review starts. Reviewers judge working code.
- **Secrets only in the gitignored root `.env`.** Never in code, docs,
  `.agent/`, or git history.
- **No git worktrees.** Disabled outright for agents. The one exception
  is the owner explicitly asking, confined to `.agent/.worktrees/` and
  torn down after.

## The short loop

Branch from fresh main, work in small undoable commits, keep the gates
green, ask the owner to run the review, fix it batch by batch, then PR
and merge on their word. The branch dies after merge, its ledger is the
record.
