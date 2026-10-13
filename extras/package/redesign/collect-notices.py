"""Copy license/copyright documents from verified dependency source archives.
Copyright 2026 webmaster94. GPL-2.0-or-later. Archive code is never executed.
"""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import tarfile
import zipfile


def is_notice(name):
    name = Path(name).name.upper().lstrip(".")
    return name.startswith(("LICENSE", "LICENCE", "COPYING", "COPYRIGHT", "NOTICE", "AUTHORS"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archives", type=Path, required=True)
    parser.add_argument("--existing", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    shutil.copytree(args.existing, args.output, dirs_exist_ok=True)
    index = []

    def save(archive, name, content):
        if len(content) > 2_000_000:
            raise ValueError("Unexpectedly large notice: " + name)
        tag = re.sub(r"[^a-zA-Z0-9._-]", "_", archive.name)
        basename = re.sub(r"[^a-zA-Z0-9._-]", "_", Path(name).name)
        digest = hashlib.sha256(name.encode()).hexdigest()[:10]
        folder = args.output / tag
        folder.mkdir(exist_ok=True)
        target = folder / (digest + "-" + basename)
        target.write_bytes(content)
        index.append(f"{target.relative_to(args.output).as_posix()} | {archive.name} | {name}")

    for archive in sorted(args.archives.iterdir()):
        if not archive.is_file():
            continue
        if zipfile.is_zipfile(archive):
            with zipfile.ZipFile(archive) as source:
                for item in source.infolist():
                    if not item.is_dir() and is_notice(item.filename):
                        save(archive, item.filename, source.read(item))
        elif tarfile.is_tarfile(archive):
            with tarfile.open(archive) as source:
                for item in source:
                    if item.isfile() and is_notice(item.name):
                        if item.size > 2_000_000:
                            raise ValueError("Unexpectedly large notice: " + item.name)
                        save(archive, item.name, source.extractfile(item).read())
    (args.output / "SOURCE-INDEX.txt").write_text("\n".join(index) + "\n", encoding="utf-8")
    if not index:
        raise ValueError("No dependency source notices found")
    print(f"Collected {len(index)} dependency notices")


if __name__ == "__main__":
    main()
