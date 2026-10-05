#!/bin/sh
# Builds iw-pad.dll + iw-pad.exe with llvm-mingw (https://github.com/mstorsjo/llvm-mingw):
# build/ for the x64 games, build/x86/ for the 32-bit ones (the launcher must match the game's bitness).
set -e
cd "$(dirname "$0")"
BIN=$(ls -d "$HOME"/tools/llvm-mingw-*/bin | head -1)
FLAGS="-std=c++20 -O2 -Wall -static"
mkdir -p build/x86
for arch in x86_64 i686; do
  out=build; [ $arch = i686 ] && out=build/x86
  CXX=$BIN/$arch-w64-mingw32-clang++
  "$CXX" $FLAGS -shared -o $out/iw-pad.dll src/iw-pad.cpp -lwinmm
  "$CXX" $FLAGS -mwindows -municode -o $out/iw-pad.exe src/iw-pad-launcher.cpp
done
ls -la build build/x86
