# IW-Pad details

## Files

| File | Function |
|---|---|
| `iw-pad.exe` | Launcher (64-bit). It starts the game and loads the DLL that matches the game. |
| `iw-pad.dll` | Mod for 64-bit MW2 and MW3. |
| `iw-pad32.dll` | Mod for 32-bit games (all other supported games). |
| `classic/` | Menus and button icons for Call of Duty 1, Call of Duty 1: United Offensive and Call of Duty 2. |

The DLL changes some function calls in game memory. It does not change files on disk.
It checks the game build first. If it does not know the build, it does nothing.

The launcher looks for the game exe in this order: `iw4x-sp.exe`, `iw5-mod.exe`, `CoDUOSP.exe`, `CoDSP.exe`,
`CoD2SP_s.exe`, `iw4sp.exe`, `iw5sp.exe`. It never starts a multiplayer exe.
It adds `-singleplayer` for iw4x-sp and iw5-mod.
Steam installs Call of Duty 1 and Call of Duty 1: United Offensive in one folder. There, Steam's app id picks the game.

`MESA_EXTENSION_MAX_YEAR=2003` stops Call of Duty 1 and Call of Duty 1: United Offensive from crashing at startup on Linux.

## Settings

Set these in **Options > Controls > Look**, in the game console (`~`) or in `config.cfg`.

| Setting | Default | Function |
|---|---|---|
| `gpad_enabled` | `1` | Controller input on or off. |
| `gpad_aim_assist` | `1` | Aim assist on or off. |
| `gpad_aim_strength` | `0.75` | Aim assist strength. `1` is the console strength. |
| `gpad_sensitivity` | `1.0` | Look sensitivity. See the steps below. |
| `gpad_invert` | `0` | Inverts vertical look. |
| `gpad_deadzone` | `0.2` | Stick deadzone. |
| `gpad_ps_glyphs` | `0` | `1` shows PlayStation 3 icons. `0` shows Xbox 360 icons. |
| `gpad_console_hud` | `1` | Console style HUD. MW2 and MW3 only. |
| `gpad_hud_safe_area` | `0.9` | Part of the screen the console style HUD uses. `1` is the screen edges. |
| `gpad_aim_range` | `3200` | Aim assist range. MW3 and the classic games only. |
| `gpad_autoaim_range` | `1200` | Auto aim range. MW3 and the classic games only. |
| `gpad_quick_grenade` | `1` | One press grenade throw. Call of Duty 1 and Call of Duty 1: United Offensive only. |
| `gpad_reset` | `0` | Set to `1` to reset the settings above. |
| `gpad_in_use` | | Read only. `1` when the controller was the last input. |

Controller Sensitivity uses the console steps:

| Step | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| `gpad_sensitivity` | 0.6 | 1 | 1.4 | 1.8 | 2 | 2.2 | 2.6 | 3 | 3.5 | 4 |

Pad buttons use the console names, for example `bind BUTTON_A "+gostand"` or `bind DPAD_UP "+actionslot 1"`.

## Log

A correct start writes these lines to `iw-pad.log`: `game: <name>`, `hooks installed`, `dvars registered`.

If the log stops at `hooks installed` and the controller does nothing, Steam restarted the game without IW-Pad.
The launcher prevents this with an empty `noship_create_steam_appid.txt` next to the game exe.
If the folder is read-only, create that file yourself.

## Build

You need [llvm-mingw](https://github.com/mstorsjo/llvm-mingw). `build.sh` looks for it in `~/tools/llvm-mingw-*`.

```sh
./build.sh
```

The script writes `iw-pad.exe`, `iw-pad.dll` and `iw-pad32.dll` to `build/`.

The classic menu files need Python 3 (standard library only):

```sh
python3 glyphs/q3_pk3.py build/classic/z_iwpad.pk3 cod1    # Call of Duty 1
python3 glyphs/q3_pk3.py build/classic/zz_iwpad.pk3 uo     # Call of Duty 1: United Offensive
python3 glyphs/cod2_iwd.py build/classic/zz_iwpad.iwd      # Call of Duty 2
```

The README controller diagrams come from `docs/controls.py` (Python 3, standard library only). Run it after changing a default bind:

```sh
python3 docs/controls.py
```
