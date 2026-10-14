#!/usr/bin/env bash
set -euo pipefail
test_root="${1:?Pass an isolated absolute output directory}"
prefix="${2:-/root/vlc-redesign-build/prebuilt/x86_64-w64-mingw32}"
repository="$(cd "$(dirname "$0")/../../../.." && pwd)"
mkdir -p "$test_root"
x86_64-w64-mingw32-g++ -std=gnu++11 -O1 -DQT_STATIC -D_WIN32_WINNT=0x0601 \
  -I"$repository/modules/gui/qt" -I"$prefix/include" \
  -I"$prefix/include/QtCore" -I"$prefix/include/QtGui" \
  "$repository/extras/package/redesign/tests/artwork-security.cpp" \
  "$repository/modules/gui/qt/redesign_image.cpp" \
  -L"$prefix/lib" -static -s -Wl,--start-group \
  -lQt5Gui -lQt5Core -lqwindows -lQt5PlatformSupport -lqtharfbuzzng -lqtpcre -lz \
  -lgdi32 -lcomdlg32 -loleaut32 -limm32 -lwinmm -lole32 -luuid -lws2_32 \
  -ladvapi32 -lshell32 -luser32 -lkernel32 -lmpr -Wl,--end-group \
  -o "$test_root/artwork-security.exe"
