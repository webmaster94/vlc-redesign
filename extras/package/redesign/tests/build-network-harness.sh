#!/bin/bash
# Compile the real Qt subtitle request helper with a non-network fixture client.
# The resulting Windows executable tests the production helper, not a Python mirror.
set -euo pipefail
prefix="$(realpath "$1")"
output="$(realpath -m "$2")"
script_dir="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$(dirname "$output")"
python3 "$script_dir/build-network-harness.py" "${output%.exe}.cpp"
x86_64-w64-mingw32-g++ -std=c++11 -DQT_STATIC \
  -I"$prefix/include" -I"$prefix/include/QtCore" "${output%.exe}.cpp" "$script_dir/qt56-crt-probe.cpp" \
  -L"$prefix/lib" -lQt5Core -lqtpcre -lz -lole32 -luuid -lws2_32 \
  -ladvapi32 -lshell32 -luser32 -lkernel32 -lmpr -static -s -o "$output"
