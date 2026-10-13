"""Package a compiled VLC/Reframe tree with an isolated NSIS installer.

Copyright 2026 webmaster94. GPL-2.0-or-later.
No dependency binaries are downloaded by this script.
"""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import zipfile

EXTENSIONS = "mp4 mkv avi mov webm m4v wmv ts mpg mpeg flv mp3 flac wav ogg opus m4a aac wma aiff m3u m3u8 xspf".split()


def nsis_quote(value):
    return str(value).replace("$", "$$").replace('"', '$\\"')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--payload", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
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
    (stage / "vlc.exe").rename(stage / "reframe.exe")
    shutil.copy2(repo / "extras/package/redesign/reframe.ico", stage / "reframe.ico")
    shutil.copy2(repo / "COPYING.LIB", stage / "COPYING.LIB.txt")
    shutil.copy2(repo / "doc/redesign/credits.txt", stage / "CREDITS.txt")
    shutil.copy2(repo / "README.md", stage / "FORK-README.txt")
    files = sorted(p for p in stage.rglob("*") if p.is_file())
    install, uninstall = [], []
    directories = set()
    for path in files:
        rel = path.relative_to(stage)
        directory = rel.parent.as_posix().replace("/", "\\")
        suffix = "" if directory == "." else "\\" + directory
        install += [f'SetOutPath "$INSTDIR{nsis_quote(suffix)}"', f'File "{nsis_quote(path)}"']
        uninstall.append(f'Delete "$INSTDIR\\{nsis_quote(rel.as_posix().replace(chr(47), chr(92)))}"')
        for parent in rel.parents:
            if parent.as_posix() != ".":
                directories.add(parent)
    for directory in sorted(directories, key=lambda d: len(d.parts), reverse=True):
        uninstall.append(f'RMDir "$INSTDIR\\{nsis_quote(directory.as_posix().replace(chr(47), chr(92)))}"')
    manifest_files = {}
    for name, lines in {
        "INSTALL_MANIFEST": install,
        "UNINSTALL_MANIFEST": uninstall,
        "ASSOCIATIONS_INSTALL": [f"!insertmacro RegisterType {ext}" for ext in EXTENSIONS],
        "ASSOCIATIONS_UNINSTALL": [f"!insertmacro UnregisterType {ext}" for ext in EXTENSIONS],
    }.items():
        path = output / (name.lower() + ".nsh")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        manifest_files[name] = path
    installer = output / "Reframe-3.0.24-r1-win64.exe"
    cmd = ["makensis", "-V2", f"-DSTAGE={stage}", f"-DOUTPUT_FILE={installer}"]
    cmd.extend(f"-D{name}={path}" for name, path in manifest_files.items())
    cmd.append(str(repo / "extras/package/redesign/installer.nsi"))
    subprocess.run(cmd, check=True)
    portable = output / "Reframe-3.0.24-r1-win64-portable.zip"
    with zipfile.ZipFile(portable, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, "Reframe/" + path.relative_to(stage).as_posix())
        archive.writestr("Reframe/portable/.keep", "")
    hashes = []
    for artifact in (installer, portable):
        hashes.append(hashlib.sha256(artifact.read_bytes()).hexdigest() + "  " + artifact.name)
    (output / "SHA256SUMS").write_text("\n".join(hashes) + "\n")
    print("\n".join(hashes))


if __name__ == "__main__":
    main()
