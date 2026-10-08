---
name: spec-reviews
kind: doc
description: format and lifecycle for .agent/reviews, the code review ledgers
updated: 2026-09-23
links: [conventions, house-rules-agent, spec-bugs]
---

# Spec: reviews/

One folder per code review, named `<yyyy-mm>-<branch-slug>`, born when
the review runs, permanent once born. The full protocol lives in
[house-rules.md](../house-rules.md), this spec covers the paper.

## The ledger

`ledger.md` inside the review's folder. Standard frontmatter,
`kind: review`, plus `branch:` and `status: open | closed`. The body:

1. **Summary**: the review's scope (what the branch did) and the final
   count when closed.
2. **Batches**: the batch table. Number, theme, status
   (pending, fixing, verified, done), the owner's ack noted per batch.
3. **Findings**: one block per finding, ordered by batch, then by the pass
   that raised it in the order the passes ran, then by severity. A pass's
   blocks sit together because they answer the fixes before them.

```
### F-07: <one line claim>
- severity: breaks | bug | debt | nit
- found-by: review | hunter | verifier
- batch: 2
- status: open | fixed | wontfix | deferred
- fix: <commit hash, date, when fixed>
```

The id is the handle a commit or another ledger uses to name a finding, so it
is unique across the whole ledger and never restarts per batch. `F-` is the
review's own, `H-` the hunter's, `V-` the verifier's, each running on from the
last one used anywhere in the file.

A finding another angle's hunter or verifier raises in a file this angle owns
is handed over here and takes the next `F-` id. It joins the review's own list
and waits for a batch like them, its found-by names who raised it, and its body
names the angle it came from.

A wontfix carries its reason inline. A finding too big for its branch is
`deferred`: it graduates into bugs/ as its own file, linked both ways,
and that is the only path a finding leaves its ledger.

## Lifecycle

When the last batch closes and the branch merges, the ledger flips to
`status: closed` and the reviews INDEX regroups it. Nothing moves and
nothing archives: bug files and decisions link into ledgers by name, and
git is the archive.
