#!/usr/bin/env python3
"""kspkg.py - inspect / extract Assetto Corsa EVO content.kspkg (verified on 0.9.0).

Layout (this build):
  * file data from offset 0, then a table of contents (TOC) occupying the last
    64 MB of the file (0x4000000; older builds used 32 MB). Slots are 256 bytes,
    sorted ascending by 64-bit path hash, unused slots are zero.
  * slot: path[0xE4] (UTF-8, NUL padded) | u16 flags | u16 pathlen | u64 hash
          | u64 size | u64 offset   (flags at 0xE4: bit0 = directory, bit8 =
          payload is XOR ciphered, pathlen at 0xE6)
  * hash: FNV-1a 64 over the path encoded as UTF-16LE.
  * cipher: XOR with the 8-byte key below, indexed by the offset INSIDE the entry
    (byte i of a file is XORed with key[i % 8]). The TOC starts on an 8 byte
    boundary, so for the table this equals the absolute offset. Payloads are
    ciphered only when bit8 is set (everything except the big streamed
    .texturemips tile files, which are stored plain so DirectStorage can DMA
    them straight into GPU tile pools).

Usage:
  kspkg.py [-p content.kspkg] info
  kspkg.py [-p ...] list [-f GLOB] [--sort path|size|offset]
  kspkg.py [-p ...] extract GLOB [GLOB ...] [-o OUTDIR]
  kspkg.py [-p ...] cat PATH            (raw decoded bytes to stdout)
  kspkg.py [-p ...] verify              (recompute every hash)
  kspkg.py [-p ...] stats               (size by extension / xor flag)

A slot that cannot be read is left out with a warning, an entry that runs past the
end of the package fails with a message and a nonzero exit, and extract never
writes outside its output folder.
"""
import argparse
import fnmatch
import os
import struct
import sys
from collections import Counter, defaultdict, namedtuple
from typing import BinaryIO, Callable, Iterator, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gamedir import game_file  # noqa: E402

KEY = bytes.fromhex("c135117da921979f")
SLOT = 256
EMPTY_SLOT = bytes(SLOT)
FLAG_DIR = 1 << 0
FLAG_XOR = 1 << 8
TOC_SIZES = (0x4000000, 0x2000000)

Entry = namedtuple("Entry", "path flags hash size offset")


def fnv1a64_utf16(path: str) -> int:
    value = 0xcbf29ce484222325
    for byte in path.encode("utf-16le"):
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value


def xor_at(data: bytes, offset_in_entry: int) -> bytes:
    """XOR data with the key phase-aligned to offset_in_entry, the offset inside the entry."""
    length = len(data)
    if length == 0:
        return data
    phase = offset_in_entry % 8
    rotated = KEY[phase:] + KEY[:phase]
    stream = (rotated * (length // 8 + 2))[:length]
    return (int.from_bytes(data, "little") ^ int.from_bytes(stream, "little")).to_bytes(length, "little")


def parse_slot(raw: bytes) -> Optional[Entry]:
    path_bytes = raw[:0xE4].split(b"\0", 1)[0]
    flags, path_length = struct.unpack_from("<HH", raw, 0xE4)
    path_hash, size, offset = struct.unpack_from("<QQQ", raw, 0xE8)
    if not path_bytes or path_length != len(path_bytes):
        return None
    try:
        path = path_bytes.decode("utf-8")
    except UnicodeDecodeError:
        return None
    if any(ord(char) < 32 for char in path):
        return None
    return Entry(path, flags, path_hash, size, offset)


def read_toc(package: BinaryIO, package_size: int) -> tuple[list[Entry], int]:
    for table_size in TOC_SIZES:
        if table_size >= package_size:
            continue
        package.seek(package_size - table_size)
        table = xor_at(package.read(table_size), 0)
        first = parse_slot(table[:SLOT])
        if first is None or first.offset + first.size > package_size:
            continue

        # The used slots come first and the zero tail ends the table, so a slot with bytes in it that
        # does not read is a real entry this tool cannot name, and the listing says it left one out.
        entries: list[Entry] = []
        unreadable = 0
        for at in range(0, table_size, SLOT):
            raw = table[at:at + SLOT]
            if raw == EMPTY_SLOT:
                break
            entry = parse_slot(raw)
            if entry is None:
                unreadable += 1
                continue
            entries.append(entry)
        if unreadable:
            print(f"warning: {unreadable} table slot(s) could not be read and are left out of everything below", file=sys.stderr)
        return entries, table_size
    raise SystemExit("could not locate a valid table of contents (unknown package layout)")


def open_pkg(path: str) -> tuple[BinaryIO, int, list[Entry], int]:
    package = open(path, "rb")
    package_size = os.path.getsize(path)
    entries, table_size = read_toc(package, package_size)
    return package, package_size, entries, table_size


def read_entry(package: BinaryIO, entry: Entry, chunk: int = 8 << 20) -> Iterator[bytes]:
    package.seek(entry.offset)
    remaining = entry.size
    position = 0
    while remaining:
        data = package.read(min(chunk, remaining))
        if not data:
            raise SystemExit(f"{entry.path} runs past the end of the package, {remaining:,} of its {entry.size:,} bytes are missing")
        if entry.flags & FLAG_XOR:
            data = xor_at(data, position)
        yield data
        position += len(data)
        remaining -= len(data)


def inside(folder: str, target: str) -> bool:
    try:
        return os.path.commonpath([folder, target]) == folder
    except ValueError:
        return False    # another drive


def cmd_info(args: argparse.Namespace) -> None:
    package, package_size, entries, table_size = open_pkg(args.package)
    files = [entry for entry in entries if not entry.flags & FLAG_DIR]
    dirs = len(entries) - len(files)
    xored = sum(1 for entry in files if entry.flags & FLAG_XOR)
    print(f"package      : {args.package}")
    print(f"size         : {package_size:,} bytes ({package_size / 2**30:.1f} GiB)")
    print(f"toc          : last {table_size // 2**20} MB, {table_size // SLOT:,} slots, {len(entries):,} used ({len(files):,} files, {dirs:,} dirs)")
    print(f"toc start    : 0x{package_size - table_size:x}")
    print(f"payload      : {sum(entry.size for entry in files):,} bytes; xor-ciphered files: {xored:,}, plain: {len(files) - xored:,}")
    print(f"sorted by    : hash ascending -> {all(entries[i].hash <= entries[i + 1].hash for i in range(len(entries) - 1))}")
    roots = Counter(entry.path.split("\\")[0] for entry in entries)
    print("roots        : " + ", ".join(f"{root} ({count})" for root, count in roots.most_common()))


def cmd_list(args: argparse.Namespace) -> None:
    package, package_size, entries, table_size = open_pkg(args.package)
    selected = [entry for entry in entries if not args.filter or fnmatch.fnmatch(entry.path.lower(), args.filter.lower())]
    sort_keys: dict[str, Callable[[Entry], object]] = {
        "path": lambda entry: entry.path.lower(),
        "size": lambda entry: -entry.size,
        "offset": lambda entry: entry.offset,
    }
    for entry in sorted(selected, key=sort_keys[args.sort]):
        kind = "D" if entry.flags & FLAG_DIR else ("X" if entry.flags & FLAG_XOR else "P")
        print(f"{kind} {entry.size:>12,} {entry.offset:>14,}  {entry.path}")
    print(f"{len(selected):,} entries", file=sys.stderr)


def cmd_extract(args: argparse.Namespace) -> None:
    package, package_size, entries, table_size = open_pkg(args.package)
    out_root = os.path.realpath(args.out)
    extracted = 0
    refused = 0
    for entry in entries:
        if entry.flags & FLAG_DIR:
            continue
        if not any(fnmatch.fnmatch(entry.path.lower(), pattern.lower()) for pattern in args.globs):
            continue

        # The path comes from the package, so a crafted one could name a drive or climb out of the folder.
        target = os.path.realpath(os.path.join(out_root, entry.path))
        if not inside(out_root, target):
            print(f"refused {entry.path}, it would be written outside {args.out}", file=sys.stderr)
            refused += 1
            continue
        os.makedirs(os.path.dirname(target), exist_ok=True)
        try:
            with open(target, "wb") as out:
                for data in read_entry(package, entry):
                    out.write(data)
        except SystemExit:
            os.remove(target)
            raise
        extracted += 1
        if args.verbose:
            print(entry.path)
    print(f"extracted {extracted} file(s) to {args.out}", file=sys.stderr)
    if refused:
        raise SystemExit(f"{refused} file(s) refused for paths outside the output folder")


def cmd_cat(args: argparse.Namespace) -> None:
    package, package_size, entries, table_size = open_pkg(args.package)
    wanted = args.path.replace("/", "\\").lower()
    for entry in entries:
        if entry.path.lower() != wanted:
            continue
        out = sys.stdout.buffer
        for data in read_entry(package, entry):
            out.write(data)
        return
    raise SystemExit(f"not found: {args.path}")


def cmd_verify(args: argparse.Namespace) -> None:
    package, package_size, entries, table_size = open_pkg(args.package)
    bad = [entry for entry in entries if fnv1a64_utf16(entry.path) != entry.hash]
    print(f"{len(entries):,} entries, {len(bad)} hash mismatches")
    for entry in bad[:20]:
        print("  ", entry.path, hex(entry.hash), hex(fnv1a64_utf16(entry.path)))


def cmd_stats(args: argparse.Namespace) -> None:
    package, package_size, entries, table_size = open_pkg(args.package)
    by_extension: defaultdict[str, list[int]] = defaultdict(lambda: [0, 0, 0, 0])
    for entry in entries:
        if entry.flags & FLAG_DIR:
            continue
        name = entry.path.rsplit("\\", 1)[-1]
        extension = name.rsplit(".", 1)[-1].lower() if "." in name else "<none>"
        xored = 1 if entry.flags & FLAG_XOR else 0
        by_extension[extension][xored] += 1
        by_extension[extension][2 + xored] += entry.size
    print(f"{'ext':28s} {'plain#':>8s} {'xor#':>8s} {'plain MB':>10s} {'xor MB':>10s}")
    for extension, (plain, xored, plain_bytes, xored_bytes) in sorted(by_extension.items(), key=lambda item: -(item[1][2] + item[1][3])):
        print(f"{extension:28s} {plain:8d} {xored:8d} {plain_bytes / 2**20:10.1f} {xored_bytes / 2**20:10.1f}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-p", "--package", help="path of content.kspkg (default: ACEVO_GAME_DIR\\content.kspkg)")
    commands = parser.add_subparsers(dest="cmd", required=True)

    commands.add_parser("info").set_defaults(fn=cmd_info)

    listing = commands.add_parser("list")
    listing.add_argument("-f", "--filter")
    listing.add_argument("--sort", default="path", choices=["path", "size", "offset"])
    listing.set_defaults(fn=cmd_list)

    extract = commands.add_parser("extract")
    extract.add_argument("globs", nargs="+")
    extract.add_argument("-o", "--out", default="extracted")
    extract.add_argument("-v", "--verbose", action="store_true")
    extract.set_defaults(fn=cmd_extract)

    cat = commands.add_parser("cat")
    cat.add_argument("path")
    cat.set_defaults(fn=cmd_cat)

    commands.add_parser("verify").set_defaults(fn=cmd_verify)
    commands.add_parser("stats").set_defaults(fn=cmd_stats)

    args = parser.parse_args()
    if not args.package:
        args.package = game_file("content.kspkg")
    if not os.path.exists(args.package):
        raise SystemExit(f"package not found: {args.package} (use -p)")
    args.fn(args)


if __name__ == "__main__":
    main()
