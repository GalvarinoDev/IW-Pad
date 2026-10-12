#!/bin/bash
# Builds zone/iwpad -> build/zone/iwpad.ff with the IW4x OpenAssetTools fork (github.com/iw4x-x64/oat, GPL-3.0),
# which can write MW2 x64 ("IW4MS") zones. Build OAT once: ./generate.sh --cc=gcc && make -C build config=release_x64 LinkerCli
# The game loads zone/english/iwpad.ff when the DLL finds it (iw-pad.cpp: load_our_zone).
set -e
OAT="${OAT:-$HOME/tools/oat/build/bin/Release_x64}"
cd "$(dirname "$0")/iwpad"
"$OAT/Linker" --no-color --output-folder ../../build/zone iwpad
