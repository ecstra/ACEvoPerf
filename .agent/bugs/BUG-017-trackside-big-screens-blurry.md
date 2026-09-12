---
name: BUG-017-trackside-big-screens-blurry
kind: bug
description: the trackside big screens are visibly blurry because their flipbook ships cooked at half size with only three of twelve mip levels, and the engine picks its mip from the whole 8 by 8 sheet rather than the frame on show, so a coarse mip costs eight times the detail
updated: 2026-09-12
links: [content-package, package-override-layer, BUG-001-texture-low-mip-shown-before-streaming]
area: streaming
status: open
severity: medium
reported: 2026-09-12
parent:
---

## Symptom

Owner, 2026-09-12, with a screenshot of the Nürburgring GP start line: "the displays within the
map is blurry, never noticed it till now (its a moving display)". Then, importantly: "there is a
display in main menu as well. and thats not blurry. The displays on track are blurry and looks
more like 32x32 not 256x256 or 512x512."

## What it is

The screens are `big_screen.mesh` placed six times by
`content\tracks\nurburgring\containers\big_screens.scene`. Their picture surface is
`big_screen_picture.material`, which binds one flipbook texture,
`content\tracks\common_assets\textures\flipbooks\led_evo_4096_64f.texture`, and animates it:

| material parameter | value |
|---|---|
| `Flipbook_Enable` | 1.0 |
| `Flipbook_Columns` | 8.0 |
| `Flipbook_Rows` | 8.0 |
| `Flipbook_Duration` | 8.0 s |
| `Flipbook_FrameBlending` | 1.0 |
| `ksMipScaler` | not set |

So one texture holds 64 frames in an 8 by 8 grid and the shader shows one at a time. No mip bias
comes from the material.

The menu display is a different thing and explains why it looks fine:
`interns\garage_ac_evo\materials\garage_display.material` has no flipbook at all, it is plain
static textures.

## Evidence

Headers decoded against `TextureMetadata` in `tools/data/proto_schema.txt`:

| flipbook | shipped | mipLevels | cook shrink | source PNG |
|---|---|---|---|---|
| **led_evo_4096_64f**, the track screens | 2048x2048 | **3** | **2x** | 4096x4096 |
| led_siati_4096_64f, same folder | 2048x2048 | 12 | 2x | 4096x4096 |
| logo_16-9_8x8, same folder | 4096x4096 | 13 | 1x | 4096x4096 |

The mip count is confirmed by arithmetic rather than inferred. Mips 0, 1 and 2 of a 2048 square
BC1 texture are 2097152 plus 524288 plus 131072 bytes, which is 2752512, exactly the size of the
shipped `.texturemips`. The tiling agrees: 512 by 256 texel tiles of 64 KB,
`tileCountForSubresource` 32, 8 and 2, which is 42 tiles and the same 2752512 bytes.

So the asset loses detail twice before the engine even chooses a mip. It was cooked at a 2x shrink
from a 4096 source, unlike its sibling `logo_16-9_8x8` in the same folder, and it ships three mip
levels where its other sibling `led_siati_4096_64f`, identical in size and shrink, ships twelve.

Then the flipbook multiplies the cost of any coarse mip. Mip 0 gives 2048 divided by 8, so 256 by
256 per frame. Mip 2, the coarsest that ships, gives 512 divided by 8, so **64 by 64 per frame**
on a full size trackside screen, which is what the owner is seeing.

## Fix

Serve a replacement through the mod's own package override layer with the mip chain cut to one
level, so the engine has nothing coarse to fall back to and must sample the 2048 sheet. That is a
4x sharpening against the current worst case and it invents nothing: `tools/texture_mips.py`
writes a payload that is a byte for byte prefix of the shipped one, verified by hash. The cost is
that the texture stays fully resident, 2 MB.

The tool is committed, the asset is not. It is the game's own content, so each machine generates
it from its own `content.kspkg`.

What this does not recover is the 2x cook shrink. Getting 512 by 512 per frame needs the 4096
source art, which is not in the package. That half is Kunos's to fix and is worth reporting.

## Verification

Pending, one session with the override in place.
