---
name: BUG-034-an-integrated-gpu-with-a-large-uma-carve-out-is-read-as-a-card-that-size
kind: bug
description: the auto sizes rank adapters by dedicated video memory with no way to tell an integrated GPU from a discrete one before a D3D12 device exists, so an APU whose BIOS carve out is 4 or 8 GB is sized like a card of that size and can outrank a smaller real card beside it
updated: 2026-09-20
links: [DEC-022-every-card-gets-a-size-and-the-pick-is-checked-after, review-2026-09-sweep-review-render, directstorage-streaming, BUG-003-crash-on-startup-with-low-vram]
area: render
status: open
severity: bug
reported: 2026-09-20
parent:
---

## Problem

`DiscreteAdapter` in `src/render/adapter.cpp` walks the DXGI adapters, skips the software one and
keeps whichever reports the most `DedicatedVideoMemory`. `AutoTilePoolMb` and `AutoStagingMb` then
read that figure as the card's budget.

On a discrete card the figure is the card's own memory. On an integrated GPU it is whatever the
firmware carved out of system RAM. Most report a few hundred MB, which the brackets now handle,
but an AMD APU set to a fixed UMA size reports that size, commonly 2, 4 or 8 GB. Such a part is
read as a card of that size and takes the matching bracket, up to 1536 MB of tiles at an 8 GB
carve out. The memory is real system RAM so the allocation itself succeeds, but it is sized by a
rule written for dedicated video memory on a card with its own bandwidth.

The same ranking also decides which adapter the sizes come from. An APU with a large carve out
outranks a smaller discrete card in the same machine, so on a hybrid system the sizes can be read
from the integrated side while the game renders on the discrete one, which is the failure of F-02
of the render review reached by a different route.

## What would settle it

`D3D12_FEATURE_DATA_ARCHITECTURE.UMA` says exactly what is wanted, and it needs an
`ID3D12Device`. The sizes are resolved before any device exists, by necessity: on 2026-09-18 the
engine sized its tile pool 7 ms before it created the swap chain, which is the first place a
device is reachable. Creating a device of our own to ask would mean building and tearing down a
D3D12 device on the game's startup path, which costs more than the answer is worth on the
machines where the answer is the same.

## What is in place instead

`CheckAutoSizeAdapter` compares the adapter the sizes came from against the one the game actually
renders on, once the swap chain exists, and logs a warning naming both cards and the two ini keys
to set by hand. That catches the hybrid half of this. It does not catch a machine whose only GPU
is an APU with a large carve out, where there is no disagreement to find.

## Notes

Raised by the hunter sub agent on the first batch of the render review, 2026-09-20, from the code
and from DXGI's documented behaviour for UMA carve outs. No such machine appears in any session
on disk, every capture is the single NVIDIA laptop, so nothing here is measured. Accepted as a
limit in DEC-022 rather than guessed at.
