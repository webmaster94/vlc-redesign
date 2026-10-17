"""Measure real GUI playback startup, including single-instance file handoff.

Use an isolated player/profile; --ipc is only for an isolated Windows runner.
Copyright 2026 webmaster94. GPL-2.0-or-later.
"""
import argparse
import json
import math
from pathlib import Path
import struct
import subprocess
import time
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ipc", action="store_true")
    args = parser.parse_args()
    app, root = args.app.resolve(), args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if not (app.parent / "portable").is_dir():
        parser.error("Use an isolated portable profile")
    if not (app.parent / "plugins/plugins.dat").is_file():
        raise AssertionError("Missing packaged plugin cache causes expensive DLL discovery")
    fixtures = [root / f"startup-{name}.wav" for name in ("a", "b")]
    for fixture in fixtures:
        with wave.open(str(fixture), "wb") as output:
            output.setparams((1, 2, 8000, 0, "NONE", "not compressed"))
            output.writeframes(b"".join(struct.pack("<h", int(math.sin(i * math.tau * 440 / 8000) * 8000))
                                       for i in range(8000 * 5)))
    video = root / "startup-video.y4m"
    with video.open("wb") as output:
        output.write(b"YUV4MPEG2 W160 H90 F10:1 Ip A1:1 C420jpeg\n")
        for frame in range(50):
            output.write(b"FRAME\n" + bytes([80 + frame]) * 14400 + bytes([128]) * 7200)
    base = [str(app), "--ignore-config", "--no-qt-privacy-ask", "--aout=dummy", "--verbose=2"]
    rows = []

    def ready(process, report, filename, started):
        while time.perf_counter() - started < 15:
            if report.exists():
                for line in report.read_text(encoding="utf-8").splitlines():
                    try:
                        event = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if event.get("uri", "").endswith(filename) and event.get("time_us", 0) > 0:
                        return round(time.perf_counter() - started, 3)
            if process.poll() is not None:
                raise AssertionError("Player exited before playback progressed")
            time.sleep(0.01)
        raise AssertionError("File did not reach visible GUI playback within 15 seconds")

    def record(name, seconds):
        rows.append(dict(case=name, seconds=seconds))
        print(f"PASS: {name}: {seconds:.3f} s to playback progress", flush=True)
        (root / "startup-results.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")

    for mode in ("first-launch", "video-launch", "repeat-launch", "existing-instance"):
        if mode == "existing-instance" and not args.ipc:
            continue
        report = root / (mode + ".jsonl")
        report.unlink(missing_ok=True)
        media = video if mode == "video-launch" else fixtures[0]
        flags = ["--no-one-instance"] if mode != "existing-instance" else [
            "--one-instance", "--one-instance-when-started-from-file", "--started-from-file"]
        log = root / (mode + ".log")
        with log.open("wb") as output:
            start = time.perf_counter()
            process = subprocess.Popen(base + flags + ["--play-and-exit",
                "--qt-redesign-startup-probe=" + str(report), str(media)],
                stdout=output, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                elapsed = ready(process, report, media.name, start)
                if mode != "existing-instance":
                    record(mode, elapsed)
                else:
                    start = time.perf_counter()
                    secondary = subprocess.Popen(base + flags + [str(fixtures[1])],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                        creationflags=subprocess.CREATE_NO_WINDOW)
                    try:
                        elapsed = ready(process, report, fixtures[1].name, start)
                        secondary.wait(timeout=5)
                        assert secondary.returncode == 0, "Single-instance sender failed"
                    finally:
                        if secondary.poll() is None:
                            secondary.kill()
                            secondary.wait()
                    record(mode, elapsed)
                process.wait(timeout=10)
                assert process.returncode == 0, "Player failed on exit"
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
        text = log.read_text(encoding="utf-8", errors="replace")
        assert "stale plugins cache" not in text, "Packaged plugin timestamps do not match cache"
        assert "cannot read" not in text, "Packaged plugin cache was not readable"
        assert "plug-ins loaded: 493 modules" in text, "Plugin inventory changed or failed to load"


if __name__ == "__main__":
    main()
