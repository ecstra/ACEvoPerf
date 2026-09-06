"""Render the readme header: ACEVOPERF in the pixel lettering the other repos use, EVO outlined,
on a dark card that merges with GitHub's dark theme.

Writes header.png next to this file. Needs Pillow only, the glyphs are defined below. The
outline of a word is a strip along every cell edge that faces an empty cell, which is what keeps
letters that touch only at a corner, like the V, in one piece.
"""

from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).parent
RED = (225, 6, 0)
BACKGROUND = (13, 17, 23)
CORNER_RADIUS = 0.5
SUPERSAMPLE = 2
CELL = 64 * SUPERSAMPLE
ROWS = 7
COLS = 5
LETTER_GAP = 1
MARGIN = 1
OUTLINE = 0.2

GLYPHS: dict[str, list[str]] = {
    "A": [".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"],
    "C": [".XXXX", "X....", "X....", "X....", "X....", "X....", ".XXXX"],
    "E": ["XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "XXXXX"],
    "V": ["X...X", "X...X", "X...X", "X...X", "X...X", ".X.X.", "..X.."],
    "O": [".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."],
    "P": ["XXXX.", "X...X", "X...X", "XXXX.", "X....", "X....", "X...."],
    "R": ["XXXX.", "X...X", "X...X", "XXXX.", "X.X..", "X..X.", "X...X"],
    "F": ["XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "X...."],
}

WORDS: list[tuple[str, bool]] = [
    ("ACE", True),
    ("EVO", False),
    ("PERF", True),
]

Cell = tuple[int, int]


def word_cells(
    text: str,
    col0: int,
) -> set[Cell]:
    cells: set[Cell] = set()
    for index, ch in enumerate(text):
        left = col0 + index * (COLS + LETTER_GAP)
        for row, line in enumerate(GLYPHS[ch]):
            for col, mark in enumerate(line):
                if mark == "X":
                    cells.add((row, left + col))
    return cells


def word_cols(text: str) -> int:
    return len(text) * COLS + (len(text) - 1) * LETTER_GAP


def draw_filled(
    draw: ImageDraw.ImageDraw,
    cells: set[Cell],
    x0: int,
    y0: int,
) -> None:
    for row, col in cells:
        x = x0 + col * CELL
        y = y0 + row * CELL
        draw.rectangle((x, y, x + CELL - 1, y + CELL - 1), fill=RED)


def draw_outlined(
    draw: ImageDraw.ImageDraw,
    cells: set[Cell],
    x0: int,
    y0: int,
) -> None:
    stroke = int(CELL * OUTLINE)
    for row, col in cells:
        x = x0 + col * CELL
        y = y0 + row * CELL
        right = x + CELL - 1
        bottom = y + CELL - 1
        if (row - 1, col) not in cells:
            draw.rectangle((x, y, right, y + stroke - 1), fill=RED)
        if (row + 1, col) not in cells:
            draw.rectangle((x, bottom - stroke + 1, right, bottom), fill=RED)
        if (row, col - 1) not in cells:
            draw.rectangle((x, y, x + stroke - 1, bottom), fill=RED)
        if (row, col + 1) not in cells:
            draw.rectangle((right - stroke + 1, y, right, bottom), fill=RED)


def render() -> Image.Image:
    total_cols = sum(word_cols(text) for text, _ in WORDS) + (len(WORDS) - 1) * LETTER_GAP
    width = (total_cols + 2 * MARGIN) * CELL
    height = (ROWS + 2 * MARGIN) * CELL
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle(
        (0, 0, width - 1, height - 1),
        radius=int(CELL * CORNER_RADIUS),
        fill=BACKGROUND,
    )

    x0 = MARGIN * CELL
    y0 = MARGIN * CELL
    col0 = 0
    for text, filled in WORDS:
        cells = word_cells(text, col0)
        if filled:
            draw_filled(draw, cells, x0, y0)
        else:
            draw_outlined(draw, cells, x0, y0)
        col0 += word_cols(text) + LETTER_GAP

    return canvas.resize((width // SUPERSAMPLE, height // SUPERSAMPLE), Image.LANCZOS)


if __name__ == "__main__":
    render().save(OUT / "header.png")
    print("header.png written")
