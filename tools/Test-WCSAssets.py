"""Read-only installed asset audit; this does not compile or load Enfusion resources.

PAC1 layout reference: https://github.com/WoozyMasta/pac1/blob/master/reader.go
Only archive indexes and resource-database bytes are read. No packages are extracted or edited.
"""
import argparse
import json
import re
import struct
from pathlib import Path


def project(path):
    text = path.read_text(encoding="utf-8-sig")
    guid = re.search(r'(?m)^\s*GUID\s+"([0-9A-Fa-f]{16})"', text)
    if not guid:
        raise ValueError(f"Invalid project: {path}")
    block = re.search(r"\bDependencies\s*\{([^{}]*)\}", text, re.S)
    return guid[1].upper(), [x.upper() for x in re.findall(r'"([0-9A-Fa-f]{16})"', block[1] if block else "")]


def archive_paths(path):
    """Read the PAC1 FILE directory, seeking past payload chunks rather than reading large assets."""
    with path.open("rb") as stream:
        header = stream.read(12)
        if header[:4] != b"FORM" or header[8:] != b"PAC1":
            raise ValueError(f"Not a PAC1 archive: {path}")
        size = path.stat().st_size
        offset = 12
        tree = None
        while offset + 8 <= size:
            stream.seek(offset)
            kind, length = struct.unpack(">4sI", stream.read(8))
            if offset + 8 + length > size:
                raise ValueError(f"Truncated archive chunk: {path}")
            if kind == b"FILE":
                tree = stream.read(length)
                break
            offset += 8 + length
        if tree is None:
            raise ValueError(f"No FILE directory in {path}")
    result = set()

    def node(at, parent):
        kind, length = tree[at:at + 2]
        at += 2
        name = tree[at:at + length].decode("utf-8")
        at += length
        relative = (parent + "/" + name).strip("/")
        if kind == 0:
            children = struct.unpack_from("<I", tree, at)[0]
            at += 4
            for _ in range(children):
                at = node(at, relative)
        elif kind == 1:
            payload, packed, _ = struct.unpack_from("<III", tree, at)
            if payload + packed > size:
                raise ValueError(f"Truncated archive entry: {path}: {relative}")
            result.add(relative.lower())
            at += 24
        else:
            raise ValueError(f"Unsupported FILE node {kind} in {path}")
        return at

    if node(0, "") != len(tree):
        raise ValueError(f"Unexpected FILE directory tail: {path}")
    return result


def audit(repo, downloads, base_addons):
    addon = repo / "AROpenResistance/Open Resistance"
    projects = {}
    candidates = list(downloads.glob("*/addon.gproj")) + list(base_addons.glob("*/*.gproj"))
    # Explicit local projects win over downloaded copies, matching the launcher.
    candidates += [addon / "addon.gproj", repo / "OptionalAddons/OpenResistanceWCS/addon.gproj"]
    for path in candidates:
        guid, dependencies = project(path)
        projects[guid] = path.parent, dependencies
    ordered, visiting, seen = [], set(), set()

    def visit(guid):
        if guid in visiting:
            raise ValueError(f"Dependency cycle at {guid}")
        if guid in seen:
            return
        if guid not in projects:
            raise ValueError(f"Missing project {guid}")
        visiting.add(guid)
        folder, dependencies = projects[guid]
        for dependency in dependencies:
            visit(dependency)
        visiting.remove(guid)
        seen.add(guid)
        ordered.append(folder)

    visit("1CAE49450A83DEAB")
    paths, databases = {}, {}
    for folder in ordered:
        for package in folder.glob("*.pak"):
            for relative in archive_paths(package):
                paths.setdefault(relative, []).append(folder)
        database = folder / "resourceDatabase.rdb"
        if database.exists():
            data = database.read_bytes()
            databases[folder] = data, data.lower()

    files = list((addon / "Configs/FactionPresets").rglob("*.conf"))
    files += [addon / "Prefabs/GameModes/AROR_GameMode_Base.et",
              addon / "Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer"]
    references = set()
    for path in files:
        references.update((guid.upper(), relative) for guid, relative in re.findall(
            r'\{([0-9A-Fa-f]{16})\}([^"\r\n]+\.(?:et|conf))', path.read_text(encoding="utf-8")))
    if not references:
        raise ValueError("Audit found no asset references")
    missing, mismatches = [], []
    for guid, relative in sorted(references):
        local = addon / relative
        if local.exists():
            meta = Path(str(local) + ".meta")
            if not meta.exists() or "{" + guid + "}" not in meta.read_text(encoding="utf-8"):
                mismatches.append({"resource": relative, "guid": guid, "reason": "local meta missing or mismatched"})
            continue
        owners = paths.get(relative.lower(), [])
        if not owners:
            missing.append({"resource": relative, "guid": guid})
            continue
        matched = False
        for owner in owners:
            data, lower = databases.get(owner, (b"", b""))
            needle = relative.lower().encode("utf-8")
            at = lower.find(needle)
            # Installed RDBC records put the little-endian resource GUID just after the resource name.
            # Check the local record rather than finding an unrelated matching GUID elsewhere in the DB.
            tail = data[at + len(needle):at + len(needle) + 32] if at >= 0 else b""
            if int(guid, 16).to_bytes(8, "little") in tail:
                matched = True
                break
        if not matched:
            mismatches.append({"resource": relative, "guid": guid, "reason": "GUID not found in installed path record"})
    return {"project_closure": len(ordered), "unique_asset_references": len(references),
            "verified_path_and_guid": len(references) - len(missing) - len(mismatches),
            "missing": missing, "guid_mismatches": mismatches,
            "scope": "Faction preset configs, base game-mode prefab and Chicken manager layer; native loading is unverified."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-addons", type=Path, required=True)
    parser.add_argument("--base-addons", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = audit(Path(__file__).resolve().parent.parent, args.game_addons, args.base_addons)
    except (OSError, ValueError, struct.error, IndexError) as error:
        parser.exit(1, f"Asset audit failed: {error}\n")
    print(json.dumps(result, indent=2))
    raise SystemExit(1 if result["missing"] or result["guid_mismatches"] else 0)
