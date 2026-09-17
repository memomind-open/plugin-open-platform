"""Shared ELF records and manifest validation for the unified GMP v2 packer."""
import re
import struct

ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIIIIIII")
SECTION_HEADER = struct.Struct("<IIIIIIIIII")
SYMBOL = struct.Struct("<IIIBBH")
RELA = struct.Struct("<IIi")
PROTOCOL_ID = re.compile(r"^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$")
PROTOCOL_VERSION = re.compile(r"^\d+(?:\.\d+){0,3}$")
class PackageError(RuntimeError):
    pass


def validate_provided_protocols(manifest: dict) -> None:
    provides = manifest.get("provides")
    if provides is None:
        return
    if not isinstance(provides, dict):
        raise PackageError("manifest provides must be an object")
    protocols = provides.get("protocols")
    if not isinstance(protocols, list) or len(protocols) > 16:
        raise PackageError("manifest provides.protocols must be a list of at most 16 items")
    seen: set[str] = set()
    for protocol in protocols:
        if not isinstance(protocol, dict):
            raise PackageError("provided protocol must be an object")
        protocol_id = protocol.get("id")
        version = protocol.get("version")
        if not isinstance(protocol_id, str) or not PROTOCOL_ID.fullmatch(protocol_id):
            raise PackageError(f"invalid provided protocol id: {protocol_id!r}")
        if not isinstance(version, str) or not PROTOCOL_VERSION.fullmatch(version):
            raise PackageError(f"invalid provided protocol version: {version!r}")
        if protocol_id in seen:
            raise PackageError(f"duplicate provided protocol: {protocol_id}")
        seen.add(protocol_id)
