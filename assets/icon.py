"""Render the ACEvoPerf icon: ACE over PERF in Bahnschrift Bold Condensed on a black tile.

Writes icon-1024.png and icon-512.png next to this file. Needs Pillow and the Bahnschrift
font that ships with Windows.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).parent
SIZE = 1024
SUPERSAMPLE = 4
W = SIZE * SUPERSAMPLE
FONT_PATH = "C:/Windows/Fonts/bahnschrift.ttf"

SURFACE = (17, 17, 17)
TOP_COLOUR = (245, 245, 245)
BOTTOM_COLOUR = (225, 6, 0)
CORNER_RADIUS = 0.22
MARGIN = 0.165
GAP = 0.06
TRACKING_EM = -0.02


def condensed_bold(size: int) -> ImageFont.FreeTypeFont:
    font = ImageFont.truetype(FONT_PATH, size)
    font.set_variation_by_axes([700, 75])
    return font


def tracked_width(
    text: str,
    font: ImageFont.FreeTypeFont,
    tracking: float,
) -> float:
    advances = sum(font.getlength(ch) for ch in text)
    return advances + tracking * (len(text) - 1)


def font_for_width(
    text: str,
    target: float,
) -> tuple[ImageFont.FreeTypeFont, float]:
    probe = condensed_bold(1000)
    width_at_1000 = tracked_width(text, probe, TRACKING_EM * 1000)
    size = int(1000 * target / width_at_1000)
    return condensed_bold(size), TRACKING_EM * size


def cap_height(font: ImageFont.FreeTypeFont) -> float:
    left, top, right, bottom = font.getbbox("H", anchor="ls")
    return -top


def draw_tracked(
    draw: ImageDraw.ImageDraw,
    x: float,
    baseline: float,
    text: str,
    font: ImageFont.FreeTypeFont,
    tracking: float,
    fill: tuple[int, int, int],
) -> None:
    for ch in text:
        draw.text((x, baseline), ch, font=font, fill=fill, anchor="ls")
        x += font.getlength(ch) + tracking


def render() -> Image.Image:
    canvas = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle(
        (0, 0, W - 1, W - 1),
        radius=int(W * CORNER_RADIUS),
        fill=SURFACE,
    )

    margin = W * MARGIN
    block_width = W - 2 * margin
    top_font, top_tracking = font_for_width("ACE", block_width)
    bottom_font, bottom_tracking = font_for_width("PERF", block_width)

    top_cap = cap_height(top_font)
    bottom_cap = cap_height(bottom_font)
    gap = W * GAP
    block_top = (W - (top_cap + gap + bottom_cap)) / 2

    draw_tracked(
        draw,
        x=margin,
        baseline=block_top + top_cap,
        text="ACE",
        font=top_font,
        tracking=top_tracking,
        fill=TOP_COLOUR,
    )
    draw_tracked(
        draw,
        x=margin,
        baseline=block_top + top_cap + gap + bottom_cap,
        text="PERF",
        font=bottom_font,
        tracking=bottom_tracking,
        fill=BOTTOM_COLOUR,
    )

    return canvas.resize((SIZE, SIZE), Image.LANCZOS)


if __name__ == "__main__":
    icon = render()
    icon.save(OUT / "icon-1024.png")
    icon.resize((512, 512), Image.LANCZOS).save(OUT / "icon-512.png")
    print("icon-1024.png and icon-512.png written")
