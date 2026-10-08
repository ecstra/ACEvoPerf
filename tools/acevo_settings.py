#!/usr/bin/env python3
r"""acevo_settings.py - view / edit Assetto Corsa EVO's binary settings files.

The game stores settings as protobuf messages (video.videosettings etc.) and
embeds the protobuf schema inside AssettoCorsaEVO.exe. This tool extracts that
schema at runtime (see protodesc.py), so it stays valid across game updates as
long as the field names do not change.

Usage:
  acevo_settings.py show
  acevo_settings.py set graphics.textureQuality=TextureQuality_High graphics.texturePoolSize=TexturePoolSize_High
  acevo_settings.py profile pacing | gpu-relief | vram6-textures | vram6-balanced
  acevo_settings.py profiles
  acevo_settings.py restore [BACKUP_FILE]

Options: --exe PATH (game exe, default ACEVO_GAME_DIR\AssettoCorsaEVO.exe),
         --file PATH (settings file), --message NAME
Every write, restore included, creates <file>.bak-YYYYmmdd-HHMMSS first. Close the game before editing.
Requires: pip install protobuf
"""
import argparse
import glob
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from protodesc import load, msg_class  # noqa: E402
from gamedir import game_file  # noqa: E402
from google.protobuf import descriptor, text_format  # noqa: E402
from google.protobuf.message import Message  # noqa: E402

PROFILES: dict[str, dict[str, str]] = {
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


def default_file() -> str:
    return os.path.join(os.environ.get("USERPROFILE", ""), "Saved Games", "ACE", "video.videosettings")


def load_msg(args: argparse.Namespace) -> Message:
    pool, files = load(args.exe)
    message_type = msg_class(pool, args.message)
    message = message_type()
    with open(args.file, "rb") as settings:
        message.ParseFromString(settings.read())
    return message


def backup(path: str) -> str:
    # Never over an existing backup, which a restore in the same second as the write before it would
    # otherwise replace with the very state it is about to undo.
    stamp = time.strftime("%Y%m%d-%H%M%S")
    copy = f"{path}.bak-{stamp}"
    suffix = 1
    while os.path.exists(copy):
        copy = f"{path}.bak-{stamp}-{suffix}"
        suffix += 1
    shutil.copy2(path, copy)
    return copy


def apply_assignments(
    message: Message,
    assignments: list[str],
) -> None:
    for item in assignments:
        if "=" not in item:
            raise SystemExit(f"expected path=value, got {item!r}")
        path, value = item.split("=", 1)
        parts = path.strip().split(".")
        message_descriptor = message.DESCRIPTOR
        field = None
        for part in parts:
            if message_descriptor is None or part not in message_descriptor.fields_by_name:
                raise SystemExit(f"unknown field {part!r} in {path!r}")
            field = message_descriptor.fields_by_name[part]
            message_descriptor = field.message_type
        if field.type == descriptor.FieldDescriptor.TYPE_STRING and not value.startswith('"'):
            value = '"' + value.replace('"', '\\"') + '"'
        text = " ".join(f"{part} {{" for part in parts[:-1]) + f" {parts[-1]}: {value} " + "}" * (len(parts) - 1)
        try:
            text_format.Merge(text, message)
        except text_format.ParseError as error:
            raise SystemExit(f"cannot set {path}={value}: {error}")
        print(f"set {path} = {value}")


def cmd_show(args: argparse.Namespace) -> None:
    print(text_format.MessageToString(load_msg(args)))


def cmd_set(args: argparse.Namespace) -> None:
    message = load_msg(args)
    apply_assignments(message, args.assignments)
    copy = backup(args.file)
    with open(args.file, "wb") as settings:
        settings.write(message.SerializeToString())
    print(f"written {args.file} (backup: {copy})")


def cmd_profile(args: argparse.Namespace) -> None:
    if args.name not in PROFILES:
        raise SystemExit(f"unknown profile {args.name!r}; available: {', '.join(PROFILES)}")
    args.assignments = [f"{path}={value}" for path, value in PROFILES[args.name].items()]
    cmd_set(args)


def cmd_profiles(args: argparse.Namespace) -> None:
    for name, assignments in PROFILES.items():
        print(name)
        for path, value in assignments.items():
            print(f"    {path} = {value}")


def cmd_restore(args: argparse.Namespace) -> None:
    source = args.backup
    if not source:
        candidates = sorted(glob.glob(args.file + ".bak-*"))
        if not candidates:
            raise SystemExit("no backups found")
        source = candidates[-1]
    if not os.path.exists(source):
        raise SystemExit(f"backup not found: {source}")

    # The state being replaced gets a backup of its own, so a restore of the wrong one can be undone.
    copy = backup(args.file) if os.path.exists(args.file) else None
    shutil.copy2(source, args.file)
    print(f"restored {args.file} from {source}" + (f" (backup of what it replaced: {copy})" if copy else ""))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", help="game exe (default: ACEVO_GAME_DIR\\AssettoCorsaEVO.exe)")
    parser.add_argument("--file", default=default_file())
    parser.add_argument("--message", default="VideoSettings")
    commands = parser.add_subparsers(dest="cmd", required=True)

    commands.add_parser("show").set_defaults(fn=cmd_show)

    assign = commands.add_parser("set")
    assign.add_argument("assignments", nargs="+")
    assign.set_defaults(fn=cmd_set)

    profile = commands.add_parser("profile")
    profile.add_argument("name")
    profile.set_defaults(fn=cmd_profile)

    commands.add_parser("profiles").set_defaults(fn=cmd_profiles)

    restore = commands.add_parser("restore")
    restore.add_argument("backup", nargs="?")
    restore.set_defaults(fn=cmd_restore)

    args = parser.parse_args()
    if args.cmd == "profiles":
        args.fn(args)
        return
    if not args.exe:
        args.exe = game_file("AssettoCorsaEVO.exe")
    if not os.path.exists(args.exe):
        raise SystemExit(f"game exe not found: {args.exe} (use --exe)")
    if args.cmd == "restore":
        if not os.path.isdir(os.path.dirname(os.path.abspath(args.file))):
            raise SystemExit(f"settings folder not found for {args.file} (use --file)")
    elif not os.path.exists(args.file):
        raise SystemExit(f"settings file not found: {args.file} (use --file)")
    args.fn(args)


if __name__ == "__main__":
    main()
