"""Package a compiled VLC/Reframe tree with an isolated NSIS installer.

Copyright 2026 webmaster94. GPL-2.0-or-later.
No dependency binaries are downloaded by this script.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
from packaging_common import installer, portable, checksums


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--payload", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--licenses", required=True, type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    payload, output = args.payload.resolve(), args.output.resolve()
    if not (payload / "vlc.exe").is_file() or not (payload / "libvlc.dll").is_file():
        parser.error("Payload must be a compiled package-win-common tree")
    output.mkdir(parents=True, exist_ok=True)
    stage = output / "stage"
    if stage.exists():
        parser.error("Use a fresh output directory to avoid packaging stale files")
    shutil.copytree(payload, stage, ignore=shutil.ignore_patterns("sdk", "NSIS", "helpers", "languages", "*.nsi", "axvlc.dll", "npvlc.dll", "np*.dll", "vlc.ico", "spad*"))
    (stage / "plugins/plugins.dat").unlink(missing_ok=True)
    (stage / "vlc.exe").rename(stage / "reframe.exe")
    shutil.copy2(repo / "extras/package/redesign/reframe.ico", stage / "reframe.ico")
    shutil.copy2(repo / "extras/package/redesign/reframe-branding.bmp", stage / "reframe-branding.bmp")
    shutil.copy2(repo / "COPYING.LIB", stage / "COPYING.LIB.txt")
    shutil.copy2(repo / "COPYING.GPL3", stage / "COPYING.GPL3.txt")
    shutil.copy2(repo / "COPYING", stage / "COPYING.GPL2.txt")
    shutil.copy2(repo / "COPYING.GPL3", stage / "COPYING.txt")
    shutil.copy2(repo / "doc/redesign/credits.txt", stage / "CREDITS.txt")
    shutil.copy2(repo / "README.md", stage / "FORK-README.txt")
    shutil.copytree(repo / "extras/package/redesign/fonts", stage / "fonts")
    if os.environ.get("OPENSUBTITLES_APP_KEY"):
        (stage / "opensubtitles-app-key.txt").write_text(os.environ["OPENSUBTITLES_APP_KEY"].strip(), encoding="utf-8")
    if not args.licenses.is_dir():
        parser.error("Matching dependency license directory is required")
    shutil.copytree(args.licenses, stage / "licenses")
    strip = shutil.which("x86_64-w64-mingw32-strip")
    if strip:
        for binary in sorted(stage.rglob("*")):
            if binary.suffix.lower() in (".exe", ".dll"):
                subprocess.run([strip, "--strip-unneeded", str(binary)], check=True)
                stamp = int(binary.stat().st_mtime) & ~1
                os.utime(binary, (stamp, stamp))
    installer(stage, output, repo)
    portable(stage, output)
    checksums(output)


if __name__ == "__main__":
    main()
