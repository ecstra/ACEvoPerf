---
name: TODO-015-merge-consecutive-texture-requests
kind: todo
description: find out whether the game's single subresource texture requests arrive in runs that could travel as one multi subresource request, and merge them in the proxy if they do
updated: 2026-09-12
links: [DEC-015-bundled-directstorage-core-loaded-first, directstorage-1-3-2026-09-12, directstorage-streaming, TODO-013-faster-session-loads]
status: open
by: owner
area: streaming
born: 2026-09-12
done:
---

## What

Owner wording, 2026-09-12: "take the measurement, ill launch and load a track".

One session of a lap, a track change and a second lap sent **91498 requests** through the
`GpuUpload Memory Queue`, **55575** of them `DSTORAGE_REQUEST_DESTINATION_TEXTURE_REGION`, each
naming one resource, one subresource index and a box, for 6.8 GB.

DirectStorage can take a range of subresources in a single request instead, through
`DSTORAGE_DESTINATION_MULTIPLE_SUBRESOURCES` (1.2, from a first subresource to the end) or
`DSTORAGE_DESTINATION_MULTIPLE_SUBRESOURCES_RANGE` (1.3, with a count so the run can stop early).
The catch is in the documentation for both: "the source is expected to contain full data for all
subresources, starting from FirstSubresource". So a merge is only legal when consecutive requests
name the same resource, consecutive subresource indices, and source bytes that carry straight on
from one another, with no submit, status entry or fence signal in between.

Whether the game behaves that way is unknown, so it gets measured before anything is written.

## Where it stands

`MergeSurvey` is built and wired into `QueueProxy`, behind `[profile] merge_survey=1`, off by
default. It counts runs and why each one ended, and never changes a request. Its counting is
covered by a test in the scratchpad that is checked against two deliberately broken copies of the
implementation, one with the barrier handling gutted and one with the source contiguity check
removed, and both are caught.

Waiting on one session with the flag on, then the `[merge]` lines in `acevo_perf.log`.

## Done when

Either:

- The survey says runs are rare or short, the idea is closed in the research record with its
  numbers, and the survey either stays as an off by default diagnostic or comes out.
- The survey says a worthwhile share of those 55575 requests sit in mergeable runs, in which case
  `QueueProxy::EnqueueRequest` grows a small pending buffer that holds a run back, flushes it as
  one multi subresource request when the run ends, and flushes on every barrier. That path has to
  keep `OverlayRedirect` in front of it and keep the statistics counting what the game asked for,
  not what was sent.

## Worth knowing before starting the second branch

The expected win is small. The load bottleneck measured in TODO-013 is the engine's job queue spin
loop, not DirectStorage, and the drive was about a second of a thirteen second load. Fewer requests
is less work in a place that was not the constraint. This is worth doing because it is cheap and
real, not because it is expected to move a load time.
