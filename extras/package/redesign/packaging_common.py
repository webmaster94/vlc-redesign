"""Shared release packaging. Copyright 2026 webmaster94. GPL-2.0-or-later."""
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile

VERSION = "3.0.24-r6"
EXTENSIONS = "mp4 mkv avi mov webm m4v wmv ts mpg mpeg flv mp3 flac wav ogg opus m4a aac wma aiff m3u m3u8 xspf".split()


def nsis_quote(value):
    return str(value).replace("$", "$$").replace('"', '$\\"')


def installer(stage, output, repo):
    files = sorted(p for p in stage.rglob("*") if p.is_file() and "portable" not in p.relative_to(stage).parts)
    install, uninstall, directories = [], [], set()
    for path in files:
        rel = path.relative_to(stage)
        directory = rel.parent.as_posix().replace("/", "\\")
        suffix = "" if directory == "." else "\\" + directory
        install += [f'SetOutPath "$INSTDIR{nsis_quote(suffix)}"', f'File "{nsis_quote(path)}"']
        uninstall.append(f'Delete "$INSTDIR\\{nsis_quote(rel.as_posix().replace(chr(47), chr(92)))}"')
        directories.update(parent for parent in rel.parents if parent.as_posix() != ".")
    for directory in sorted(directories, key=lambda d: len(d.parts), reverse=True):
        uninstall.append(f'RMDir "$INSTDIR\\{nsis_quote(directory.as_posix().replace(chr(47), chr(92)))}"')
    manifests = {}
    for name, lines in {
        "INSTALL_MANIFEST": install,
        "UNINSTALL_MANIFEST": uninstall,
        "ASSOCIATIONS_INSTALL": [f"!insertmacro RegisterType {ext}" for ext in EXTENSIONS],
        "ASSOCIATIONS_UNINSTALL": [f"!insertmacro UnregisterType {ext}" for ext in EXTENSIONS],
    }.items():
        path = output / (name.lower() + ".nsh")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        manifests[name] = path
    compiler = shutil.which("makensis")
    if not compiler and os.name == "nt":
        compiler = str(Path(os.environ["ProgramFiles(x86)"]) / "NSIS/makensis.exe")
    if not compiler or not Path(compiler).is_file():
        raise RuntimeError("NSIS compiler is required")
    prefix = "/" if os.name == "nt" else "-"
    command = [compiler, prefix + "V2", f"{prefix}DSTAGE={stage}",
               f"{prefix}DOUTPUT_FILE={output / ('Reframe-' + VERSION + '-win64.exe')}"]
    command += [f"{prefix}D{name}={path}" for name, path in manifests.items()]
    command.append(str(repo / "extras/package/redesign/installer.nsi"))
    subprocess.run(command, check=True, timeout=300)


def portable(stage, output):
    archive_path = output / f"Reframe-{VERSION}-win64-portable.zip"
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(p for p in stage.rglob("*") if p.is_file()):
            if "portable" in path.relative_to(stage).parts:
                continue
            info = zipfile.ZipInfo.from_file(path, "Reframe/" + path.relative_to(stage).as_posix())
            info.compress_type = zipfile.ZIP_DEFLATED
            stamp = int(path.stat().st_mtime)
            filetime = (stamp + 11644473600) * 10000000
            # UTC timestamps keep the plugin cache valid across time zones with
            # extractors supporting the ZIP NTFS or extended timestamp fields.
            info.extra = struct.pack("<HHBI", 0x5455, 5, 1, stamp)
            info.extra += struct.pack("<HHIHHQQQ", 0x000a, 32, 0, 1, 24,
                                      filetime, filetime, filetime)
            with path.open("rb") as source, archive.open(info, "w") as target:
                shutil.copyfileobj(source, target)
        archive.writestr("Reframe/portable/.keep", "")


def checksums(output):
    names = sorted(p for p in output.iterdir() if p.is_file() and
                   (p.suffix in (".exe", ".zip", ".xz") or p.name == "CREDITS.txt"))
    lines = []
    for path in names:
        digest = hashlib.sha256()
        with path.open("rb") as file:
            for chunk in iter(lambda: file.read(1024 * 1024), b""):
                digest.update(chunk)
        lines.append(digest.hexdigest() + "  " + path.name)
    (output / "SHA256SUMS").write_text("\n".join(lines) + "\n", encoding="utf-8")
