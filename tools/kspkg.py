#!/usr/bin/env python3
"""kspkg.py - inspect / extract Assetto Corsa EVO content.kspkg (verified on 0.9.0).

Layout (this build):
  * file data from offset 0, then a table of contents (TOC) occupying the last
    64 MB of the file (0x4000000; older builds used 32 MB). Slots are 256 bytes,
    sorted ascending by 64-bit path hash, unused slots are zero.
  * slot: path[0xE4] (UTF-8, NUL padded) | u8 pad | u8 pad | u16 flags | u16 pathlen
          | u64 hash | u64 size | u64 offset   (flags live at 0xE4 as a u16:
          bit0 = directory, bit8 = payload is XOR ciphered)
  * hash: FNV-1a 64 over the path encoded as UTF-16LE.
  * cipher: XOR with the 8-byte key below, indexed by ABSOLUTE file offset mod 8.
    The whole TOC is ciphered; payloads only when bit8 is set (everything except
    the big streamed .texturemips tile files, which are stored plain so
    DirectStorage can DMA them straight into GPU tile pools).

Usage:
  kspkg.py [-p content.kspkg] info
  kspkg.py [-p ...] list [-f GLOB] [--sort path|size|offset]
  kspkg.py [-p ...] extract GLOB [GLOB ...] [-o OUTDIR]
  kspkg.py [-p ...] cat PATH            (raw decoded bytes to stdout)
  kspkg.py [-p ...] verify              (recompute every hash)
  kspkg.py [-p ...] stats               (size by extension / xor flag)
"""
import argparse, fnmatch, os, struct, sys
from collections import Counter, defaultdict, namedtuple

KEY = bytes.fromhex("c135117da921979f")
SLOT = 256
FLAG_DIR = 1 << 0
FLAG_XOR = 1 << 8
TOC_SIZES = (0x4000000, 0x2000000)

Entry = namedtuple("Entry", "path flags hash size offset")


def fnv1a64_utf16(path: str) -> int:
    h = 0xcbf29ce484222325
    for b in path.encode("utf-16le"):
        h = ((h ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return h


def xor_at(buf: bytes, abs_off: int) -> bytes:
    """XOR buf with the key phase-aligned to its absolute file offset."""
    n = len(buf)
    if n == 0:
        return buf
    r = abs_off % 8
    krot = KEY[r:] + KEY[:r]
    ks = (krot * (n // 8 + 2))[:n]
    return (int.from_bytes(buf, "little") ^ int.from_bytes(ks, "little")).to_bytes(n, "little")


def default_path():
    for c in (r"C:\InfinityX\Games\Assetto Corsa EVO\content.kspkg",
              r"C:\Program Files (x86)\Steam\steamapps\common\Assetto Corsa EVO\content.kspkg"):
        if os.path.exists(c):
            return c
    return "content.kspkg"


def parse_slot(raw: bytes):
    path = raw[:0xE4].split(b"\0", 1)[0]
    flags, plen = struct.unpack_from("<HH", raw, 0xE4)
    h, size, off = struct.unpack_from("<QQQ", raw, 0xE8)
    if not path or plen != len(path) or not all(32 <= c < 127 for c in path):
        return None
    return Entry(path.decode(), flags, h, size, off)


def read_toc(f, fsize):
    for tbl in TOC_SIZES:
        if tbl >= fsize:
            continue
        start = fsize - tbl
        f.seek(start)
        dec = xor_at(f.read(tbl), start)
        first = parse_slot(dec[:SLOT])
        if first is None or first.offset + first.size > fsize:
            continue
        entries = []
        for o in range(0, tbl, SLOT):
            e = parse_slot(dec[o:o + SLOT])
            if e is None:
                break
            entries.append(e)
        return entries, tbl
    raise SystemExit("could not locate a valid table of contents (unknown package layout)")


def open_pkg(path):
    f = open(path, "rb")
    fsize = os.path.getsize(path)
    entries, tbl = read_toc(f, fsize)
    return f, fsize, entries, tbl


def read_entry(f, e: Entry, chunk=8 << 20):
    f.seek(e.offset)
    remaining = e.size
    pos = e.offset
    while remaining:
        n = min(chunk, remaining)
        buf = f.read(n)
        if not buf:
            break
        if e.flags & FLAG_XOR:
            buf = xor_at(buf, pos)
        yield buf
        pos += len(buf)
        remaining -= len(buf)


def cmd_info(a):
    f, fsize, entries, tbl = open_pkg(a.package)
    files = [e for e in entries if not e.flags & FLAG_DIR]
    dirs = len(entries) - len(files)
    xored = sum(1 for e in files if e.flags & FLAG_XOR)
    print(f"package      : {a.package}")
    print(f"size         : {fsize:,} bytes ({fsize / 2**30:.1f} GiB)")
    print(f"toc          : last {tbl // 2**20} MB, {tbl // SLOT:,} slots, {len(entries):,} used ({len(files):,} files, {dirs:,} dirs)")
    print(f"toc start    : 0x{fsize - tbl:x}")
    print(f"payload      : {sum(e.size for e in files):,} bytes; xor-ciphered files: {xored:,}, plain: {len(files) - xored:,}")
    print(f"sorted by    : hash ascending -> {all(entries[i].hash <= entries[i+1].hash for i in range(len(entries)-1))}")
    roots = Counter(e.path.split('\\')[0] for e in entries)
    print("roots        : " + ", ".join(f"{k} ({v})" for k, v in roots.most_common()))


def cmd_list(a):
    f, fsize, entries, tbl = open_pkg(a.package)
    sel = [e for e in entries if not a.filter or fnmatch.fnmatch(e.path.lower(), a.filter.lower())]
    key = {"path": lambda e: e.path.lower(), "size": lambda e: -e.size, "offset": lambda e: e.offset}[a.sort]
    for e in sorted(sel, key=key):
        kind = "D" if e.flags & FLAG_DIR else ("X" if e.flags & FLAG_XOR else "P")
        print(f"{kind} {e.size:>12,} {e.offset:>14,}  {e.path}")
    print(f"{len(sel):,} entries", file=sys.stderr)


def cmd_extract(a):
    f, fsize, entries, tbl = open_pkg(a.package)
    n = 0
    for e in entries:
        if e.flags & FLAG_DIR:
            continue
        if not any(fnmatch.fnmatch(e.path.lower(), g.lower()) for g in a.globs):
            continue
        out = os.path.join(a.out, e.path)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "wb") as fo:
            for buf in read_entry(f, e):
                fo.write(buf)
        n += 1
        if a.verbose:
            print(e.path)
    print(f"extracted {n} file(s) to {a.out}", file=sys.stderr)


def cmd_cat(a):
    f, fsize, entries, tbl = open_pkg(a.package)
    want = a.path.replace("/", "\\").lower()
    for e in entries:
        if e.path.lower() == want:
            out = sys.stdout.buffer
            for buf in read_entry(f, e):
                out.write(buf)
            return
    raise SystemExit(f"not found: {a.path}")


def cmd_verify(a):
    f, fsize, entries, tbl = open_pkg(a.package)
    bad = [e for e in entries if fnv1a64_utf16(e.path) != e.hash]
    print(f"{len(entries):,} entries, {len(bad)} hash mismatches")
    for e in bad[:20]:
        print("  ", e.path, hex(e.hash), hex(fnv1a64_utf16(e.path)))


def cmd_stats(a):
    f, fsize, entries, tbl = open_pkg(a.package)
    st = defaultdict(lambda: [0, 0, 0, 0])
    for e in entries:
        if e.flags & FLAG_DIR:
            continue
        name = e.path.rsplit("\\", 1)[-1]
        ext = name.rsplit(".", 1)[-1].lower() if "." in name else "<none>"
        x = 1 if e.flags & FLAG_XOR else 0
        st[ext][x] += 1
        st[ext][2 + x] += e.size
    print(f"{'ext':28s} {'plain#':>8s} {'xor#':>8s} {'plain MB':>10s} {'xor MB':>10s}")
    for ext, (p, x, pb, xb) in sorted(st.items(), key=lambda kv: -(kv[1][2] + kv[1][3])):
        print(f"{ext:28s} {p:8d} {x:8d} {pb / 2**20:10.1f} {xb / 2**20:10.1f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--package", default=default_path())
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("info").set_defaults(fn=cmd_info)
    p = sub.add_parser("list"); p.add_argument("-f", "--filter"); p.add_argument("--sort", default="path", choices=["path", "size", "offset"]); p.set_defaults(fn=cmd_list)
    p = sub.add_parser("extract"); p.add_argument("globs", nargs="+"); p.add_argument("-o", "--out", default="extracted"); p.add_argument("-v", "--verbose", action="store_true"); p.set_defaults(fn=cmd_extract)
    p = sub.add_parser("cat"); p.add_argument("path"); p.set_defaults(fn=cmd_cat)
    sub.add_parser("verify").set_defaults(fn=cmd_verify)
    sub.add_parser("stats").set_defaults(fn=cmd_stats)
    a = ap.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
