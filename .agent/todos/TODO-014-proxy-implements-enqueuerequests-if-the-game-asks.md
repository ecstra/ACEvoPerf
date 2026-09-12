---
name: TODO-014-proxy-implements-enqueuerequests-if-the-game-asks
kind: todo
description: implement IDStorageQueue3 in the queue proxy if a future game build ever asks for it, because today the proxy declines it to keep the overlay and the statistics in the path
updated: 2026-09-12
links: [DEC-015-bundled-directstorage-core-loaded-first, directstorage-streaming, content-package]
status: open
by: agent
area: streaming
born: 2026-09-12
done:
---

## What

`QueueProxy` implements `IDStorageQueue2` and nothing newer. Since the mod started shipping
DirectStorage 1.3.0 (DEC-015) the real queue underneath it also offers `IDStorageQueue3`, whose
`EnqueueRequests` takes a whole array of requests plus a fence in one call.

The proxy declines that interface and returns `E_NOINTERFACE`, the same answer a 1.2 runtime
gives. Declining is the safe answer rather than the complete one: every request the game makes
has to pass through `QueueProxy::EnqueueRequest`, because that is where the overlay rewrites a
package read into a loose file read and where the streaming statistics are counted. Handing the
real queue over for `IDStorageQueue3` would hand over `EnqueueRequest` with it, since Queue3
derives from Queue2, and both the loose file overrides and the statistics would silently stop.

Assetto Corsa EVO is built against DirectStorage 1.2 and does not know the interface exists, so
nothing asks today.

## Done when

Either of these, whichever comes first:

- The log line `the game asked for IDStorageQueue3 and was declined` appears in a real session,
  meaning a game build now wants it. Then `QueueProxy` grows `EnqueueRequests`, running every
  request in the array through the same counting and `OverlayRedirect` path that
  `EnqueueRequest` uses, before passing the array to the real queue.
- The mod stops proxying queues at all, which would make the whole question moot.

## Not this

Calling `EnqueueRequests` ourselves to batch what the game enqueues one at a time. That was
considered and closed in DEC-015: the drive is about a second of a thirteen second load, so per
request overhead is not where the time goes, and batching means owning the ordering between the
requests and the fence signals the game interleaves with them.
