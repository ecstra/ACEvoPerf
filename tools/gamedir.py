"""gamedir.py - where the game is installed, shared by the tools in this folder.

The tools never guess a machine specific path. Set the environment variable
ACEVO_GAME_DIR to the folder that holds AssettoCorsaEVO.exe, or pass the
tool's own path option (-p / --exe).
"""
import os

ENV = "ACEVO_GAME_DIR"


def game_dir():
    d = os.environ.get(ENV, "")
    if not d:
        raise SystemExit(f"set {ENV} to the game folder (the one with AssettoCorsaEVO.exe) or pass the path option")
    if not os.path.isdir(d):
        raise SystemExit(f"{ENV}={d} is not a folder")
    return d


def game_file(name):
    return os.path.join(game_dir(), name)
