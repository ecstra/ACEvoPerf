---
name: TODO-017-tier-2-variable-rate-shading
kind: todo
description: the engine already asks for MAX combining against a screen space shading rate image and only ever arms it for VR, so binding an image of our own is two vtable hooks, measured as a constant image first to price the ceiling before building an edge aware one
updated: 2026-09-12
links: [optimisation-deepdive-2026-09-12, engine-flags, BUG-002-fps-drop-entering-new-track-sections]
status: open
by: owner
area: render
born: 2026-09-12
done:
---

## What

Owner wording, 2026-09-12: "lets go with the tier2 vrs (since the vrs is barely affecting,
particle system. idk what tier-2 is)."

Tier 1 variable rate shading sets one rate per draw call, which is what Kunos uses on 30
particle materials and what the deep dive's other VRS item was about. It has no distance term,
so a coarse material is coarse at two metres as at two hundred.

Tier 2 adds a **screen space image**: a texture at one value per 16 by 16 pixel tile, saying how
coarsely to shade that part of the screen. The card reports Tier 2 with a 16 pixel tile. It is
the only remaining lever that reduces GPU work on a machine that is GPU bound at 97 percent, and
unlike Tier 1 it is quality preserving by construction, because the image can leave detail alone
where it would be missed.

## Why this is smaller than it looked

The deep dive assumed the engine's image path was unusable because it is shaped for a VR lens.
Reading the code says otherwise. The engine's own per draw call at rva `0x1e3e8e0`:

```
cmp  dword ptr [rdx+0x798], 0     ; VRS enabled at all
je   skip
cmp  byte ptr [rcx+0x10], 0       ; is a shading rate image bound
je   no_combiners
mov  dword ptr [rsp+0x38], 0      ; combiner[0] = PASSTHROUGH
mov  dword ptr [rsp+0x3c], 3      ; combiner[1] = MAX
lea  r8, [rsp+0x38]
jmp  call_it
no_combiners:
xor  r8d, r8d                     ; combiners = nullptr, the image is ignored
call_it:
call qword ptr [rax+0x268]        ; RSSetShadingRate
```

`combiner[1] = MAX` is exactly what a screen space image needs: the coarser of the per draw rate
and the image wins. So the engine is already written to honour an image and simply never arms
the flag outside VR. The flag itself is set at `0x1e3e9a1`, `mov byte ptr [rsi+0x10], al` from a
`setne` on whether an image pointer is non null, in the same function that calls
`RSSetShadingRateImage` at vtable offset `0x270`.

Two vtable offsets, both confirmed from the disassembly:

| offset | slot | method |
|---|---|---|
| `0x268` | 77 | `ID3D12GraphicsCommandList5::RSSetShadingRate` |
| `0x270` | 78 | `ID3D12GraphicsCommandList5::RSSetShadingRateImage` |

Command list vtables are shared per type by the runtime, so hooking one instance hooks every
command list in the process. The mod already has `HookVtableSlot` and already reaches the D3D12
command queue through its DXGI swap chain hook, so the device is one `GetDevice` away.

## Two stages, and the first one decides whether the second happens

**Stage one, price the ceiling.** Build the plumbing and bind a **constant** image, every tile at
2x2. No compute shader, no edge detection. That measures the most Tier 2 could ever be worth on
this machine and shows what it looks like. If forcing the whole screen to 2x2 buys three percent
then an edge aware image that coarsens part of the screen cannot be worth building, and the item
closes with a number. If it buys fifteen, stage two is justified.

**Stage two, only if stage one pays.** Replace the constant with an image generated per frame
from the previous frame's back buffer, coarse only where contrast is low. That is a compute
shader, a root signature, a pipeline state and a dispatch, and it is the expensive half.

## Risks to watch in stage one

- The image stays bound on a command list until changed, so a pass drawn after the main one,
  the UI above all, would be coarsened too. The engine resets with rate 1x1 and null combiners
  when it unbinds, which is the natural place to unbind ours.
- DLSS renders below output resolution, so the render targets are smaller than the swap chain.
  An image sized from the swap chain covers them, since a larger image is allowed and the extra
  tiles are ignored.
- It changes what the game draws, so it is off by default and it is not for online use until it
  has been looked at.

## Done when

Stage one has a measured frame time with the constant image against a control on the parked
protocol, and the owner has seen what 2x2 everywhere looks like. Then either stage two is
justified with a number, or this closes with one.

## Result, 2026-09-12: stage one says no, and stage two is off

Stage one was built and measured at **4x4**, not 2x2, because 4x4 is the ceiling: one shaded
pixel in sixteen is the most Tier 2 can ever save, so a negative there closes the item outright.

The plumbing worked first time. `120x68` tiles of 16 px over a 1920x1080 swap chain, both vtable
hooks in, and the owner could see it: at 4x4 the only visible artefact was aliasing on the white
road markings, and the interface was untouched.

**It bought no frames.** Same view, same spot, back to back, from the owner's own overlay:

| | fps | 1% low | GPU | temp | clock | power |
|---|---|---|---|---|---|---|
| VRS 4x4 | 85 | 59 | 98% | 86 °C | 1597 MHz | 95 W |
| control | 85 | 60 | 98% | 86 °C | 1665 MHz | 98 W |

Same frame rate at a lower clock and lower power. The card is thermally pinned, so removing GPU
work comes back as **clocks rather than frames**. That applies to every GPU side lever on this
machine, not only this one.

The 3 to 4 percent my frame time numbers showed was not shading at all. It was the texture tile
churn being suppressed, because VRS starves the sampler feedback the streamer runs on. Full
story in
[tile-pool-reshuffle-2026-09-12](../docs/research/tile-pool-reshuffle-2026-09-12.md).

**Stage two is not justified.** An edge aware image coarsens less of the screen than a constant
4x4 does, so it cannot beat a result that is already zero, and it would carry the same feedback
problem. The item closes with a number rather than a guess, which is what it was for.

What the work leaves behind is worth more than the feature: the project's first D3D12 command
list and command queue hooks, and the probe that found the reshuffle.
