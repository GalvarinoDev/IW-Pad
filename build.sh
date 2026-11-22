#!/bin/sh
# Builds with llvm-mingw (https://github.com/mstorsjo/llvm-mingw) into build/: iw-pad.exe (x64, starts every game),
# iw-pad.dll (x64 games) and iw-pad32.dll (32-bit games). A DLL must match the game's bitness; the exe does not.
set -e
cd "$(dirname "$0")"
BIN=$(ls -d "$HOME"/tools/llvm-mingw-*/bin | head -1)
FLAGS="-std=c++20 -O2 -Wall -Wno-c23-extensions -static -s"
mkdir -p build
"$BIN/x86_64-w64-mingw32-clang++" $FLAGS -shared -o build/iw-pad.dll src/iw-pad.cpp -lwinmm
"$BIN/i686-w64-mingw32-clang++" $FLAGS -shared -o build/iw-pad32.dll src/iw-pad.cpp -lwinmm
"$BIN/x86_64-w64-mingw32-clang++" $FLAGS -mwindows -municode -o build/iw-pad.exe src/iw-pad-launcher.cpp -lpsapi
rm -f build/iw-pad32.exe
ls -la build/iw-pad*
