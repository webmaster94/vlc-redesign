#!/bin/bash
set -euo pipefail
prefix="$(realpath "$1")"
output="$(realpath -m "$2")"
repo="$(cd "$(dirname "$0")/../../../.." && pwd)"
mkdir -p "$output"
for name in native-seek-data profile-security; do
  extra=()
  libs=(-lQt5Core -lqtpcre -lz -lole32 -luuid -lws2_32 -ladvapi32 -lshell32 -luser32 -lkernel32 -lmpr)
  if [ "$name" = profile-security ]; then
    extra=("$repo/modules/gui/qt/redesign_profile.cpp")
    libs=(-lQt5Gui -lqtharfbuzzng -lgdi32 -lcomdlg32 -loleaut32 -limm32 -lwinmm "${libs[@]}")
  fi
  x86_64-w64-mingw32-g++ -std=c++11 -DQT_STATIC -I"$prefix/include" -I"$prefix/include/QtCore" \
    -I"$prefix/include/QtGui" -I"$repo/modules/gui/qt" "$repo/extras/package/redesign/tests/$name.cpp" \
    "${extra[@]}" "$repo/extras/package/redesign/tests/qt56-crt-probe.cpp" -L"$prefix/lib" -Wl,--start-group "${libs[@]}" -Wl,--end-group -static -s -o "$output/$name.exe"
done
x86_64-w64-mingw32-gcc "$repo/extras/package/redesign/tests/printscreen-receiver.c" -static -s -luser32 -o "$output/printscreen-receiver.exe"
