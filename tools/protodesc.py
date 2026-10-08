"""protodesc.py - the protobuf schema the game carries inside AssettoCorsaEVO.exe.

The exe embeds each .proto it was built with as a serialized FileDescriptorProto. This finds
them by their file name field, cuts each one where its fields stop parsing, and loads them into
one descriptor pool in dependency order, so acevo_settings.py can read and write the settings
files with the game's own schema.

A descriptor the pool refuses is reported and left out, never counted as loaded, so a message
that depends on it fails by name instead of resolving against a partial schema.
"""
import re
import sys

from google.protobuf import descriptor_pb2, descriptor_pool, message_factory
from google.protobuf.message import Message

# a FileDescriptorProto starts with field 1, its name, which ends in .proto, then one of the
# fields that follow it
PROTO_NAME = re.compile(rb"\x0a[\x01-\x7f]([\x20-\x7e]{3,120}?\.proto)[\x12\x1a\x22\x2a\x32]")
HIGHEST_FIELD = 12


def read_varint(data: bytes, at: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        byte = data[at]
        value |= (byte & 0x7F) << shift
        at += 1
        if byte < 0x80:
            return value, at
        shift += 7
        if shift > 63:
            raise ValueError("varint longer than 64 bits")


def descriptor_end(data: bytes, start: int) -> int:
    """Where the descriptor starting at start stops, the first byte that is not one of its fields."""
    at = start
    while at < len(data):
        try:
            tag, after = read_varint(data, at)
            field, wire = tag >> 3, tag & 7
            if field == 0 or field > HIGHEST_FIELD or wire not in (0, 2):
                break
            if wire == 0:
                _, after = read_varint(data, after)
            else:
                length, after = read_varint(data, after)
                if after + length > len(data):
                    break
                after += length
        except (IndexError, ValueError):
            break
        at = after
    return at


def find_descriptors(exe: bytes) -> dict[str, descriptor_pb2.FileDescriptorProto]:
    found: dict[str, descriptor_pb2.FileDescriptorProto] = {}
    for match in PROTO_NAME.finditer(exe):
        start = match.start()
        name_length = exe[start + 1]
        name = exe[start + 2:start + 2 + name_length]
        if not name.endswith(b".proto") or name.decode() in found:
            continue
        try:
            proto = descriptor_pb2.FileDescriptorProto.FromString(exe[start:descriptor_end(exe, start)])
        except Exception:
            continue    # a byte run that only looked like a descriptor
        if proto.name == name.decode():
            found[proto.name] = proto
    return found


def load(exe_path: str) -> tuple[descriptor_pool.DescriptorPool, dict[str, descriptor_pb2.FileDescriptorProto]]:
    with open(exe_path, "rb") as exe:
        found = find_descriptors(exe.read())

    pool = descriptor_pool.DescriptorPool()
    pending = dict(found)
    added: set[str] = set()
    while pending:
        ready = [name for name, proto in pending.items() if all(dependency in added for dependency in proto.dependency)]
        # A cycle or a dependency the exe does not carry leaves nothing ready, and then the rest are tried
        # as they are, so the pool reports what is missing.
        for name in ready or list(pending):
            proto = pending.pop(name)
            try:
                pool.Add(proto)
            except Exception as error:
                print(f"protodesc: {name} could not be loaded into the schema ({error}), left out", file=sys.stderr)
                continue
            added.add(name)
    return pool, found


def msg_class(pool: descriptor_pool.DescriptorPool, full_name: str) -> type[Message]:
    return message_factory.GetMessageClass(pool.FindMessageTypeByName(full_name))
