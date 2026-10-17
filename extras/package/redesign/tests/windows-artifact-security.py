"""Inspect release files without installing or running any payload."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import struct
import zipfile


def inspect(name, data):
    assert data[:2] == b"MZ", name
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0", name
    machine, _, _, _, _, _, characteristics = struct.unpack_from("<HHIIIHH", data, pe + 4)
    optional = pe + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    flags = struct.unpack_from("<H", data, optional + 70)[0]
    directories = optional + (112 if magic == 0x20B else 96)
    relocations = struct.unpack_from("<II", data, directories + 5 * 8)[1]
    return dict(file=name, machine=hex(machine), DEP=bool(flags & 0x100),
                ASLR=bool(flags & 0x40), high_entropy=bool(flags & 0x20),
                CFG=bool(flags & 0x4000), relocation_size=relocations,
                relocations_stripped=bool(characteristics & 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifacts", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    root = args.artifacts.resolve()
    # Verify every release asset against the locally supplied published manifest.
    for line in (root / "SHA256SUMS").read_text().splitlines():
        expected, name = line.split(maxsplit=1)
        name = name.lstrip("*")
        assert Path(name).name == name and "\\" not in name, "unsafe hash-manifest path"
        digest = hashlib.sha256()
        with (root / name).open("rb") as file:
            for chunk in iter(lambda: file.read(1024 * 1024), b""):
                digest.update(chunk)
        assert digest.hexdigest() == expected, name
    portable, = root.glob("*win64-portable.zip")
    installer, = root.glob("*win64.exe")
    rows = [inspect("installer", installer.read_bytes())]
    assert rows[0]["DEP"], "installer missing DEP"
    with zipfile.ZipFile(portable) as archive:
        assert archive.getinfo("Reframe/plugins/plugins.dat").file_size > 1000, "missing native plugin cache"
        seen = set()
        for info in archive.infolist():
            name = info.filename
            path = PurePosixPath(name)
            assert not path.is_absolute() and ".." not in path.parts and "\\" not in name
            assert path.parts and path.parts[0] == "Reframe" and ":" not in name
            assert name not in seen, "duplicate ZIP name"
            seen.add(name)
            assert (info.external_attr >> 16) & 0o170000 != 0o120000, "ZIP symbolic link"
            assert not info.flag_bits & 1, "encrypted entry"
            if name.lower().endswith((".exe", ".dll")):
                row = inspect(name, archive.read(info))
                assert row["machine"] == "0x8664", name
                assert row["DEP"] and row["ASLR"] and row["high_entropy"], name
                # The upstream cache-generation CLI tool has no relocation table.
                # Require actual relocation data for the player and every DLL.
                if name.lower().endswith(".dll") or path.name == "reframe.exe":
                    assert row["relocation_size"] and not row["relocations_stripped"], name
                rows.append(row)
    if args.json:
        args.json.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    print(json.dumps(dict(PE_files=len(rows),
                         DEP=dict(Counter(row["DEP"] for row in rows)),
                         ASLR_flag=dict(Counter(row["ASLR"] for row in rows)),
                         high_entropy=dict(Counter(row["high_entropy"] for row in rows)),
                         CFG=dict(Counter(row["CFG"] for row in rows)),
                         missing_relocations=[row["file"] for row in rows if not row["relocation_size"]],
                         installer=rows[0]), indent=2))


if __name__ == "__main__":
    main()
