"""Exercise the actual Windows additional-options reader in an isolated directory.

The extracted function never initializes VLC or executes supplied options. Only a
small parser probe and the Windows system curl are run. No installed profile,
registry, DLL, media engine, or remote service is touched.
"""
import ctypes
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    if os.name != "nt":
        raise SystemExit("Run this test on Windows with the Ubuntu MinGW toolchain available")
    repo = Path(__file__).resolve().parents[4]
    source = (repo / "bin/winvlc.c").read_text(encoding="utf-8")
    reader = source.split("static wchar_t **AdditionalOptions", 1)[1].split(
        "/*\n * Export WinMain", 1
    )[0]
    probe_source = """#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static wchar_t **AdditionalOptions""" + reader + """
int main(void) {
    int argc, count;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    wchar_t **extra = AdditionalOptions(argc, argv, &count);
    printf("%d\\n", count);
    for (int i = 1; i < count; ++i) {
        int size = WideCharToMultiByte(CP_UTF8, 0, extra[i], -1, NULL, 0, NULL, NULL);
        char *text = malloc(size);
        WideCharToMultiByte(CP_UTF8, 0, extra[i], -1, text, size, NULL, NULL);
        printf("%s\\n", text); free(text);
    }
    LocalFree(extra); LocalFree(argv); return 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="reframe-security-") as tmp:
        directory = Path(tmp)
        (directory / "portable").mkdir()
        probe_c, probe_exe = directory / "probe.c", directory / "probe.exe"
        probe_c.write_text(probe_source, encoding="utf-8")
        def linux_path(path):
            return "/mnt/" + path.drive[0].lower() + str(path)[2:].replace("\\", "/")
        subprocess.run([
            "wsl", "-d", "Ubuntu", "-u", "root", "--", "x86_64-w64-mingw32-gcc",
            "-std=c99", linux_path(probe_c), "-lshell32", "-o", linux_path(probe_exe)
        ], check=True, capture_output=True)
        options = directory / "portable/extra-options.txt"
        cases = [
            ("empty", b"", [], ["0"]),
            ("Windows quoting and literal shell syntax", b'--name="a b" "$()&;"', [],
             ["3", "--name=a b", "$()&;"]),
            ("valid UTF-8", '--title="\u00f1\u6e2c"'.encode(), [], ["2", "--title=\u00f1\u6e2c"]),
            ("invalid UTF-8", b"--name=\xff", [], ["0"]),
            ("embedded NUL", b"--name=a\x00--hidden=1", [], ["0"]),
            ("size ceiling accepted", b"x" * 16384, [], ["2", "x" * 16384]),
            ("oversized rejected", b"x" * 16385, [], ["0"]),
            ("argument ceiling accepted", b" ".join([b"x"] * 511), [], ["512"] + ["x"] * 511),
            ("too many arguments rejected", b" ".join([b"x"] * 512), [], ["0"]),
            ("ignore-config bypass", b"--name=a", ["--ignore-config"], ["0"]),
        ]
        passed = []
        for name, content, args, expected in cases:
            options.write_bytes(content)
            result = subprocess.run([str(probe_exe)] + args, capture_output=True, timeout=10, check=True)
            assert result.stdout.decode("utf-8").splitlines() == expected, name
            passed.append(name)
        system_directory = ctypes.create_unicode_buffer(260)
        size = ctypes.windll.kernel32.GetSystemDirectoryW(system_directory, 260)
        assert 0 < size < 260
        curl = str(Path(system_directory.value) / "curl.exe")
        (directory / "_curlrc").write_text('write-out = "DEFAULT_CONFIG_LOADED"\n', encoding="utf-8")
        env = os.environ.copy()
        env["CURL_HOME"] = str(directory)
        for variable in list(env):
            if variable.lower().endswith("_proxy"):
                del env[variable]
        args = ["--silent", "--max-time", "1", "--noproxy", "*", "https://127.0.0.1:1"]
        default = subprocess.run([curl] + args, env=env, capture_output=True, timeout=5)
        disabled = subprocess.run([curl, "-q"] + args, env=env, capture_output=True, timeout=5)
        assert b"DEFAULT_CONFIG_LOADED" in default.stdout, "curl default config test did not load fixture"
        assert b"DEFAULT_CONFIG_LOADED" not in disabled.stdout, "curl -q did not block fixture"
        passed.append("system curl -q disables user config")
        print(json.dumps({"passed": passed}, indent=2))


if __name__ == "__main__":
    main()
