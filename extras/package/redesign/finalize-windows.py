"""Generate the native plugin cache before building release packages.

Runs only on the build machine, never from an elevated installer.
Copyright 2026 webmaster94. GPL-2.0-or-later.
"""
import argparse
import datetime
import os
from pathlib import Path, PurePosixPath
import struct
import subprocess
import zipfile
from packaging_common import VERSION, installer, portable, checksums


def extract_stage(archive_path, stage):
    if stage.exists():
        raise ValueError("Use a fresh staging directory")
    stage.mkdir(parents=True)
    with zipfile.ZipFile(archive_path) as archive:
        for info in archive.infolist():
            path = PurePosixPath(info.filename)
            if (not path.parts or path.parts[0] != "Reframe" or path.is_absolute() or
                    any(p in (".", "..") or ":" in p or "\\" in p for p in path.parts)):
                raise ValueError("Invalid package path")
            if info.is_dir():
                continue
            target = stage.joinpath(*path.parts[1:])
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.read(info))
            # Our ZIP extended timestamp records UTC seconds, independent of
            # the build worker's local time zone or DOS timestamp rounding.
            stamp = int(datetime.datetime(*info.date_time, tzinfo=datetime.timezone.utc).timestamp())
            extra = info.extra
            while len(extra) >= 4:
                kind, size = struct.unpack_from("<HH", extra)
                data, extra = extra[4:4 + size], extra[4 + size:]
                if kind == 0x5455 and len(data) >= 5 and data[0] & 1:
                    stamp = struct.unpack_from("<I", data, 1)[0]
            os.utime(target, (stamp, stamp))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--stage", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("Finalize on Windows using the compiled Windows cache generator")
    output, stage = args.artifacts.resolve(), args.stage.resolve()
    extract_stage(output / f"Reframe-{VERSION}-win64-portable.zip", stage)
    generator, plugins = stage / "vlc-cache-gen.exe", stage / "plugins"
    subprocess.run([str(generator), str(plugins)], check=True, timeout=120)
    cache = plugins / "plugins.dat"
    if not cache.is_file() or cache.stat().st_size < 1000:
        raise RuntimeError("Native plugin cache generation failed")
    repo = Path(__file__).resolve().parents[3]
    installer(stage, output, repo)
    portable(stage, output)
    checksums(output)
    print("PASS: installer and portable archive contain the native plugin cache")


if __name__ == "__main__":
    main()
