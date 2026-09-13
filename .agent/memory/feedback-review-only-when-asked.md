---
name: feedback-review-only-when-asked
kind: memory
description: a code review runs only when the owner names one, closing a branch does not imply it, and review findings are fixed without the hunter and verifier loop
updated: 2026-09-13
links: [house-rules-agent, spec-reviews]
type: feedback
---

A code review runs only when the owner asks for one by name. Asking to close a branch out means the
gate launch, the merge on the owner's word and deleting the branch, not a review. When findings do
need fixing, they are fixed directly, one commit per fix, without the hunter and verifier sub-agents
of the batch loop.

Why: on 2026-09-13 a review ran when the owner had only asked to close the reload fix branch, and the
owner said "I did not want a code review. but ok. no code review till I ask. no hunter or verifier
needed."

Apply it by never starting the review protocol of the house rules on your own judgment, and by
skipping the hunter and verifier steps unless the owner asks for them.
