"""Inspect a cooked ACE `.texture` header, and cut its mip chain.

Textures in `content.kspkg` are cooked as 64 KB tiled resources described by a
`TextureMetadata` protobuf (schema in `tools/data/proto_schema.txt`). The header names the
size, the mip count and, in `tilingInfo`, where each mip's tiles sit in the `.texturemips`
payload beside it.

Cutting the chain exists for one reason, the trackside big screens. Their flipbook
(`led_evo_4096_64f`) is a sheet of 64 frames in an 8 by 8 grid, so the engine's mip choice
is driven by the whole sheet rather than by the one frame being shown, and a coarse mip
costs eight times the detail rather than two. Leaving only mip 0 gives the engine nothing
coarse to fall back to. See BUG-017.

Nothing here invents detail: the payload it writes is a byte for byte prefix of the one it
read. Nothing here writes into the game either, it produces loose files for the mod's
override folder, so the package is never touched.

    py -3 tools/texture_mips.py show <name>
    py -3 tools/texture_mips.py strip <name> --keep 1 --out <dir>

`<name>` is a package path, for example
`content\\tracks\\common_assets\\textures\\flipbooks\\led_evo_4096_64f`, without the
extension. The game folder comes from `ACEVO_GAME_DIR` as with every other tool.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

WIRE_VARINT, WIRE_64, WIRE_LEN, WIRE_32 = 0, 1, 2, 5
TILE_BYTES = 65536

Field = tuple[int, int, object]

FIELD_NAMES = {
    1: "width",
    2: "height",
    3: "mipLevels",
    4: "format",
    6: "depthOrArraySize",
    7: "isCubemap",
    8: "isVolume",
    10: "sourceImages",
    11: "conversionSettings",
    12: "tilingInfo",
}

TILING_NAMES = {
    1: "tileWidthInTexels",
    2: "tileHeightInTexels",
    3: "tileDepthInTexels",
    4: "tileOffsetForSubresource",
    5: "tileCountForSubresource",
    6: "legacyTileRowPitch",
    7: "rowPitchForTile",
}


def read_varint(data: bytes, at: int) -> tuple[int, int]:
    result = shift = 0
    while True:
        byte = data[at]
        at += 1
        result |= (byte & 0x7F) << shift
        shift += 7
        if not byte & 0x80:
            return result, at


def write_varint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        out.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(out)


def parse(data: bytes) -> list[Field]:
    """Flat (field, wire, value) list, order and repeats preserved so it can be re-emitted."""
    fields: list[Field] = []
    at = 0
    while at < len(data):
        key, at = read_varint(data, at)
        field, wire = key >> 3, key & 7
        if wire == WIRE_VARINT:
            value, at = read_varint(data, at)
            fields.append((field, wire, value))
        elif wire == WIRE_LEN:
            size, at = read_varint(data, at)
            fields.append((field, wire, data[at : at + size]))
            at += size
        elif wire == WIRE_32:
            fields.append((field, wire, data[at : at + 4]))
            at += 4
        elif wire == WIRE_64:
            fields.append((field, wire, data[at : at + 8]))
            at += 8
        else:
            raise ValueError(f"unknown wire type {wire} at byte {at}")
    return fields


def emit(fields: list[Field]) -> bytes:
    out = bytearray()
    for field, wire, value in fields:
        out += write_varint((field << 3) | wire)
        if wire == WIRE_VARINT:
            out += write_varint(int(value))
        else:
            blob = bytes(value)  # type: ignore[arg-type]
            if wire == WIRE_LEN:
                out += write_varint(len(blob))
            out += blob
    return bytes(out)


def packed(blob: bytes) -> list[int]:
    values: list[int] = []
    at = 0
    while at < len(blob):
        value, at = read_varint(blob, at)
        values.append(value)
    return values


def varint_of(fields: list[Field], field: int) -> int:
    return next(int(v) for f, w, v in fields if f == field and w == WIRE_VARINT)


def blob_of(fields: list[Field], field: int) -> bytes:
    return next(bytes(v) for f, w, v in fields if f == field and w == WIRE_LEN)  # type: ignore[arg-type]


def kspkg_cat(game_dir: Path, package_path: str, into: Path) -> None:
    """Decode one package entry to a file, reusing kspkg.py rather than repeating its cipher."""
    tool = Path(__file__).with_name("kspkg.py")
    with into.open("wb") as out:
        subprocess.run(
            [sys.executable, str(tool), "cat", package_path],
            stdout=out,
            check=True,
            env={**__import__("os").environ, "ACEVO_GAME_DIR": str(game_dir)},
        )


def show(header: bytes) -> None:
    for field, wire, value in parse(header):
        label = FIELD_NAMES.get(field, f"field{field}")
        if wire == WIRE_VARINT:
            print(f"  {label:18} = {value}")
            continue
        print(f"  {label:18} = <{len(value)} bytes>")  # type: ignore[arg-type]
        if field != 12:
            continue
        for tf, tw, tv in parse(bytes(value)):  # type: ignore[arg-type]
            tlabel = TILING_NAMES.get(tf, f"field{tf}")
            rendered = tv if tw == WIRE_VARINT else packed(bytes(tv))  # type: ignore[arg-type]
            print(f"      {tlabel:26} = {rendered}")


def strip(
    header: bytes,
    payload: bytes,
    keep: int,
) -> tuple[bytes, bytes]:
    fields = parse(header)
    levels = varint_of(fields, 3)
    if keep >= levels:
        raise SystemExit(f"nothing to do, the texture already has only {levels} mip levels")

    tiling = parse(blob_of(fields, 12))
    offsets = packed(blob_of(tiling, 4))
    counts = packed(blob_of(tiling, 5))
    pitches = packed(blob_of(tiling, 7))

    kept_tiles = sum(counts[:keep])
    kept_bytes = kept_tiles * TILE_BYTES
    if kept_bytes > len(payload):
        raise SystemExit(
            f"{kept_tiles} tiles is {kept_bytes} bytes but the payload is only {len(payload)}, "
            "the tiling assumption is wrong and nothing was written"
        )

    new_tiling: list[Field] = []
    for tf, tw, tv in tiling:
        if tf == 4:
            new_tiling.append((tf, tw, b"".join(write_varint(x) for x in offsets[:keep])))
        elif tf == 5:
            new_tiling.append((tf, tw, b"".join(write_varint(x) for x in counts[:keep])))
        elif tf == 7:
            # one entry per tile rather than per subresource, so it follows the tile count
            new_tiling.append((tf, tw, b"".join(write_varint(x) for x in pitches[:kept_tiles])))
        else:
            new_tiling.append((tf, tw, tv))

    new_fields: list[Field] = []
    for field, wire, value in fields:
        if field == 3 and wire == WIRE_VARINT:
            new_fields.append((field, wire, keep))
        elif field == 12 and wire == WIRE_LEN:
            new_fields.append((field, wire, emit(new_tiling)))
        else:
            new_fields.append((field, wire, value))

    return emit(new_fields), payload[:kept_bytes]


def main() -> int:
    import os

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["show", "strip"])
    parser.add_argument("name", help="package path without the extension")
    parser.add_argument("--keep", type=int, default=1, help="mip levels to keep (default 1)")
    parser.add_argument("--out", type=Path, help="directory to write the loose files into")
    parser.add_argument("--game", type=Path, help="game folder, else ACEVO_GAME_DIR")
    args = parser.parse_args()

    game = args.game or Path(os.environ.get("ACEVO_GAME_DIR", ""))
    if not game or not (game / "AssettoCorsaEVO.exe").exists():
        raise SystemExit("set ACEVO_GAME_DIR to the folder holding AssettoCorsaEVO.exe")

    scratch = Path(os.environ.get("TEMP", ".")) / "acevo_texture_mips"
    scratch.mkdir(parents=True, exist_ok=True)
    header_file = scratch / "header.bin"
    payload_file = scratch / "payload.bin"
    kspkg_cat(game, args.name + ".texture", header_file)
    header = header_file.read_bytes()

    print(f"{args.name}.texture:")
    show(header)

    if args.command == "show":
        return 0
    if not args.out:
        raise SystemExit("strip needs --out")

    kspkg_cat(game, args.name + ".texturemips", payload_file)
    payload = payload_file.read_bytes()
    new_header, new_payload = strip(header, payload, args.keep)

    leaf = args.name.replace("/", "\\").split("\\")[-1]
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / f"{leaf}.texture").write_bytes(new_header)
    (args.out / f"{leaf}.texturemips").write_bytes(new_payload)
    print(
        f"\n  kept {args.keep} mip level(s), {len(new_payload):,} of {len(payload):,} bytes, "
        f"a byte for byte prefix of the original"
    )
    print(f"  wrote {args.out / (leaf + '.texture')}")
    print(f"  wrote {args.out / (leaf + '.texturemips')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
