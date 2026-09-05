#!/usr/bin/env python3
r"""acevo_settings.py - view / edit Assetto Corsa EVO's binary settings files.

The game stores settings as protobuf messages (video.videosettings etc.) and
embeds the protobuf schema inside AssettoCorsaEVO.exe. This tool extracts that
schema at runtime (see protodesc.py), so it stays valid across game updates as
long as the field names do not change.

Usage:
  acevo_settings.py show
  acevo_settings.py set graphics.textureQuality=TextureQuality_High graphics.texturePoolSize=TexturePoolSize_High
  acevo_settings.py profile vram6-textures | vram6-balanced
  acevo_settings.py profiles
  acevo_settings.py restore [BACKUP_FILE]

Options: --exe PATH (game exe, default ACEVO_GAME_DIR\AssettoCorsaEVO.exe),
         --file PATH (settings file), --message NAME
Every write creates <file>.bak-YYYYmmdd-HHMMSS first. Close the game before editing.
Requires: pip install protobuf
"""
import argparse, glob, os, shutil, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from protodesc import load, msg_class  # noqa: E402
from gamedir import game_file  # noqa: E402
from google.protobuf import text_format, descriptor  # noqa: E402

PROFILES = {
    # Frame pacing: fewer per frame cubemap faces, and the present path of a
    # fullscreen swap chain instead of the composited window.
    "pacing": {
        "graphics.carReflection.carReflectionQuality": "CarReflectionQuality_Medium",
        "display.is_fullscreen": "true",
    },
    # GPU relief without touching texture quality: about 22 percent fewer
    # rendered pixels plus the four Ultra effects one step down.
    "gpu-relief": {
        "graphics.upscaling.dlss_preset": "DLSS_Quality",
        "graphics.clouds.cloudsQuality": "CloudsQuality_High",
        "graphics.volumetricsQuality": "VolumetricsQuality_High",
        "graphics.motionBlur.quality": "MotionBlurQuality_Medium",
        "graphics.grass": "GrassDensity_High",
    },
    # Uses the VRAM the mod frees (staging buffer cap) for sharper textures only.
    "vram6-textures": {
        "graphics.textureQuality": "TextureQuality_High",
        "graphics.texturePoolSize": "TexturePoolSize_High",
    },
    # Trades the most VRAM/GPU-hungry effects for headroom on a 6 GB laptop GPU.
    "vram6-balanced": {
        "graphics.textureQuality": "TextureQuality_High",
        "graphics.texturePoolSize": "TexturePoolSize_High",
        "graphics.clouds.cloudsQuality": "CloudsQuality_High",
        "graphics.volumetricsQuality": "VolumetricsQuality_High",
        "graphics.motionBlur.quality": "MotionBlurQuality_Medium",
        "graphics.grass": "GrassDensity_High",
        "graphics.giUpdateFrequency": "GlobalIlluminationUpdateFrequency_Medium",
        "graphics.carReflection.carReflectionQuality": "CarReflectionQuality_Medium",
        "graphics.depthOfField": "DepthOfFieldQuality_Off",
    },
}


def default_file():
    return os.path.join(os.environ.get("USERPROFILE", ""), "Saved Games", "ACE", "video.videosettings")


def load_msg(a):
    pool, files = load(a.exe)
    cls = msg_class(pool, a.message)
    m = cls()
    with open(a.file, "rb") as f:
        m.ParseFromString(f.read())
    return m


def backup(path):
    b = f"{path}.bak-{time.strftime('%Y%m%d-%H%M%S')}"
    shutil.copy2(path, b)
    return b


def apply_assignments(m, assignments):
    for item in assignments:
        if "=" not in item:
            raise SystemExit(f"expected path=value, got {item!r}")
        path, value = item.split("=", 1)
        parts = path.strip().split(".")
        d = m.DESCRIPTOR
        fd = None
        for p in parts:
            if d is None or p not in d.fields_by_name:
                raise SystemExit(f"unknown field {p!r} in {path!r}")
            fd = d.fields_by_name[p]
            d = fd.message_type
        if fd.type == descriptor.FieldDescriptor.TYPE_STRING and not value.startswith('"'):
            value = '"' + value.replace('"', '\\"') + '"'
        text = " ".join(f"{p} {{" for p in parts[:-1]) + f" {parts[-1]}: {value} " + "}" * (len(parts) - 1)
        try:
            text_format.Merge(text, m)
        except text_format.ParseError as e:
            raise SystemExit(f"cannot set {path}={value}: {e}")
        print(f"set {path} = {value}")


def cmd_show(a):
    print(text_format.MessageToString(load_msg(a)))


def cmd_set(a):
    m = load_msg(a)
    apply_assignments(m, a.assignments)
    b = backup(a.file)
    with open(a.file, "wb") as f:
        f.write(m.SerializeToString())
    print(f"written {a.file} (backup: {b})")


def cmd_profile(a):
    if a.name not in PROFILES:
        raise SystemExit(f"unknown profile {a.name!r}; available: {', '.join(PROFILES)}")
    a.assignments = [f"{k}={v}" for k, v in PROFILES[a.name].items()]
    cmd_set(a)


def cmd_profiles(a):
    for name, kv in PROFILES.items():
        print(name)
        for k, v in kv.items():
            print(f"    {k} = {v}")


def cmd_restore(a):
    src = a.backup
    if not src:
        cands = sorted(glob.glob(a.file + ".bak-*"))
        if not cands:
            raise SystemExit("no backups found")
        src = cands[-1]
    shutil.copy2(src, a.file)
    print(f"restored {a.file} from {src}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", help="game exe (default: ACEVO_GAME_DIR\\AssettoCorsaEVO.exe)")
    ap.add_argument("--file", default=default_file())
    ap.add_argument("--message", default="VideoSettings")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("show").set_defaults(fn=cmd_show)
    p = sub.add_parser("set"); p.add_argument("assignments", nargs="+"); p.set_defaults(fn=cmd_set)
    p = sub.add_parser("profile"); p.add_argument("name"); p.set_defaults(fn=cmd_profile)
    sub.add_parser("profiles").set_defaults(fn=cmd_profiles)
    p = sub.add_parser("restore"); p.add_argument("backup", nargs="?"); p.set_defaults(fn=cmd_restore)
    a = ap.parse_args()
    if not a.exe:
        a.exe = game_file("AssettoCorsaEVO.exe")
    if not os.path.exists(a.exe):
        raise SystemExit(f"game exe not found: {a.exe} (use --exe)")
    if a.cmd != "restore" and not os.path.exists(a.file):
        raise SystemExit(f"settings file not found: {a.file} (use --file)")
    a.fn(a)


if __name__ == "__main__":
    main()
