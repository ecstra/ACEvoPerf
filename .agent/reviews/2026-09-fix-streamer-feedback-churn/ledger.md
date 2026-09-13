---
name: review-2026-09-fix-streamer-feedback-churn
kind: review
description: the review of the texture streamer reload fix branch, two findings, both about state keyed by an address the engine reuses
updated: 2026-09-13
links: [spec-reviews, house-rules-agent, DEC-017-streamer-reload-fix-refuses-the-drop, texture-streamer-flip-2026-09-13]
branch: fix/streamer-feedback-churn
status: closed
---

# Review of fix/streamer-feedback-churn

## Summary

The branch finds why the texture streamer drops and reloads the same mip forever and ships a fix
that refuses that drop, on by default behind `streamer_reload_fix`. It also adds the streaming trace
and the runtime write detector used to prove the fix safe, and moves every diagnostic under a
`[developer]` section. Reviewed at high over `main...HEAD` on 2026-09-13, two findings, both fixed.
A launch with the trace on then showed 1,235 drops refused, 443 flips pinned, no hook fault and 0
streamed textures created writable.

## Batches

| batch | theme | status | owner ack |
|---|---|---|---|
| 1 | the fix's per texture state survives an address reuse | done | 2026-09-13, fix it fast, no hunter or verifier |
| 2 | the write detector's first seen check misses reused addresses | done | 2026-09-13, fix it fast, no hunter or verifier |

## Findings

### F-01: pin state survives a Texture* address reuse with the same level count
- severity: bug
- found-by: review
- batch: 1
- status: fixed
- fix: fa2f239, 2026-09-13, the state also resets when the package path or the D3D12 resource differs

`Touch` in `src/engine/streamer.cpp:374` keys state by the texture's address and resets it only when
the level count differs. A new texture allocated where a pinned one was freed, with the same number
of levels, inherits `pinFine`, `pinCoarse`, `pinMip`, `pinCount` and the last drop record. The fix can
then refuse a drop for a texture that never flipped, or pin one on its first load. `described` also
carries over, so the trace never writes a tex row for the new texture and joins it to the old path.

### F-02: the writable flag check is skipped for a streamed resource at a reused address
- severity: debt
- found-by: review
- batch: 2
- status: fixed
- fix: 07a5321, 2026-09-13, the flags are read at every tile request and a changed entry is counted again

`NoteStreamedResource` in `src/render/texture_writes.cpp:189` reads a resource's creation flags only
the first time its address is seen. A later tiled texture created at the same address with
`ALLOW_UNORDERED_ACCESS` or `ALLOW_RENDER_TARGET` is never counted, so the `[writes]` line can say 0
created writable while one exists. The 2026-09-13 sessions saw about 1,040 reuses, so the evidence
behind DEC-017 only covers the first resource at each address. Developer only, off in the shipped ini.
