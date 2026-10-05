# IW-Pad

IW-Pad adds controller support and console aim assist to the single-player campaigns of
Call of Duty: Modern Warfare 2 (2009) and Call of Duty: Modern Warfare 3 (2011) on PC.

IW-Pad was originally created for [DeckOps](https://github.com/GalvarinoDev/DeckOps), but anyone can use it. It works on Windows and on Linux with Proton.

## Features

- Left stick moves the player. Right stick turns the view.
- Aim assist uses the console logic: slowdown, lock-on and auto aim.
- Mouse, gyro and trackpad input continue to work. Stick input adds to them.
- Controller buttons go through the normal game binds. You can change them in the console or in `config.cfg`.
- Controller buttons operate the menus.
- On-screen hints show controller buttons (for example, `A`) instead of keyboard keys.

## How it works

IW-Pad has two files:

| File | Function |
|---|---|
| `iw-pad.exe` | The launcher. It starts the game and loads `iw-pad.dll` into it. |
| `iw-pad.dll` | The mod. It changes some function calls in the game memory. |

IW-Pad does not change the game files on disk.
IW-Pad operates only when you start the game through `iw-pad.exe`.
If you start the game normally, the game does not change.

The DLL identifies the game build before it changes anything.
If the build is not supported, the DLL writes a message to `iw-pad.log` and does nothing.

## Supported builds

| Version | MW2 | MW3 |
|---|---|---|
| x64 (Steam) | ✅ Works | ⬜ Not tested |
| x86 (32-bit) | ✅ Works | ✅ Works |

> [!WARNING]
> The x64 Steam games do not repair the remote code execution (RCE) vulnerabilities in SP mode. This includes Special Ops.
> The AlterWare clients (iw4x-sp and iw5-mod) also do not repair these vulnerabilities.
> IW-Pad does not repair them.
> Do not play online with these builds. Use them offline only.
> At this time, no client makes MW2 SP or MW3 SP safe to play online in Special Ops.

The 32-bit version can require [AlterWare](https://alterware.dev) (iw4x-sp for MW2, iw5-mod for MW3).

## Install

1. From the latest release, download the zip that agrees with your game:
   - `iw-pad-x64.zip`: for the current Steam games. Most players must use this zip.
   - `iw-pad-x86.zip`: for 32-bit games and AlterWare.
2. Extract `iw-pad.exe` and `iw-pad.dll` into the game folder, next to `iw4sp.exe` or `iw5sp.exe`.
3. In Steam, open the game properties. Set the launch option:
   - MW2: `bash -c 'exec "${@/iw4sp.exe/iw-pad.exe}"' -- %command%`
   - MW3: `bash -c 'exec "${@/iw5sp.exe/iw-pad.exe}"' -- %command%`
4. If you use Steam Input, select a gamepad controller layout. Do not use a keyboard and mouse layout.
5. Start the game.

The launch options above are for Linux. On Windows, start `iw-pad.exe` directly.

If the folder has `iw4x-sp.exe` or `iw5-mod.exe`, the launcher starts that file instead of the game file.

## Default controls

| Button | Action |
|---|---|
| A | Jump |
| B | Crouch / prone |
| X | Use / reload |
| Y | Next weapon |
| LB | Special grenade |
| RB | Frag grenade |
| LT | Aim down sights |
| RT | Fire |
| L3 | Sprint / hold breath |
| R3 | Melee |
| D-pad | Action slots 1 to 4 |
| Back | Objectives / scores |
| Start | Pause menu |

IW-Pad applies these binds one time, only if no controller button has a bind.
The game then saves the binds to `config.cfg`. If you change a bind, the game keeps your change.

## Settings

Set these values in the game console (`~`) or in `config.cfg`.

| Setting | Default | Function |
|---|---|---|
| `gpad_enabled` | `1` | Turns controller input on or off. |
| `gpad_aim_assist` | `1` | Turns aim assist on or off. |
| `gpad_aim_strength` | `0.75` | Aim assist strength. `1` is the same as the console game. |
| `gpad_sensitivity` | `1.0` | Look sensitivity. |
| `gpad_invert` | `0` | Inverts the vertical look. |
| `gpad_deadzone` | `0.2` | Stick deadzone. |
| `gpad_aim_range` | `3200` | MW3 only. Aim assist range. |
| `gpad_autoaim_range` | `1200` | MW3 only. Auto aim range. |

## Troubleshooting

Look at `iw-pad.log` in the game folder. A correct start shows these lines:

- `game: ...`
- `hooks installed`
- `dvars registered`

If the log shows `unsupported game build`, IW-Pad does not know your game build.
Open an issue and attach the log.

## Build

You must have [llvm-mingw](https://github.com/mstorsjo/llvm-mingw).
`build.sh` looks for it in `~/tools/llvm-mingw-*`.

```sh
./build.sh
```

The script writes the x64 files to `build/` and the x86 files to `build/x86/`.
The launcher must have the same bitness as the game.

## Credits

- [IW4x](https://github.com/iw4x/iw4x-client): the aim assist logic is ported from `Controller/Engine/View.cpp`.
- [iw3sp_mod](https://github.com/ahrimd0n/iw3sp-mod): the controller and aim assist logic is ported from `Gamepad.cpp`.
- [AlterWare](https://alterware.dev): iw4x-sp (MW2) and iw5-mod (MW3). IW-Pad operates with these clients on 32-bit installs. We used the iw4x-sp symbols to find the game addresses.
- [llvm-mingw](https://github.com/mstorsjo/llvm-mingw), [Ghidra](https://ghidra-sre.org) and [Capstone](https://www.capstone-engine.org): build and analysis tools.

## License

GPL-3.0. Refer to `LICENSE`.

IW-Pad is not related to Activision or Infinity Ward. You must have a legal copy of the game.
