# IW-Pad

Controller support and console aim assist for the single-player campaigns of
Call of Duty: Modern Warfare 2 (2009) and Modern Warfare 3 (2011) on PC.
Works on Windows and on Linux with Proton. Made for [DeckOps](https://github.com/GalvarinoDev/DeckOps), but anyone can use it.

- Sticks move and look. Mouse, gyro and trackpad continue to work.
- Console aim assist: slowdown, lock-on and auto aim.
- Controller buttons operate the menus and show in on-screen hints.
- Game files on disk do not change. The mod operates only when you start the game through `iw-pad.exe`.

## Supported builds

| Version | MW2 | MW3 |
|---|---|---|
| x64 (Steam) | ✅ Works | ⬜ Not tested |
| x86 (32-bit, [AlterWare](https://alterware.dev)) | ✅ Works | ✅ Works |

> [!WARNING]
> These builds have remote code execution (RCE) vulnerabilities in SP and Special Ops. IW-Pad does not repair them. Play offline only.

## Install

1. Download `iw-pad-x64.zip` (current Steam games) or `iw-pad-x86.zip` (32-bit / AlterWare) from the latest release.
2. Extract it into the game folder, next to `iw4sp.exe` or `iw5sp.exe`.
3. **Linux:** set the Steam launch option:
   - MW2: `bash -c 'exec "${@/iw4sp.exe/iw-pad.exe}"' -- %command%`
   - MW3: `bash -c 'exec "${@/iw5sp.exe/iw-pad.exe}"' -- %command%`

   **Windows:** start `iw-pad.exe`.
4. If you use Steam Input, select a gamepad layout, not a keyboard and mouse layout.

If `iw4x-sp.exe` or `iw5-mod.exe` is in the folder, the launcher starts it.

## Controls

A jump · B crouch/prone · X use/reload · Y next weapon · LB special grenade · RB frag ·
LT aim · RT fire · L3 sprint · R3 melee · D-pad action slots · Back objectives · Start pause

These binds are set one time, only if no controller button has a bind. Change them in `config.cfg` like normal binds.

## Settings

Set in the console (`~`) or `config.cfg`.

| Setting | Default | Function |
|---|---|---|
| `gpad_enabled` | `1` | Controller input on/off |
| `gpad_aim_assist` | `1` | Aim assist on/off |
| `gpad_aim_strength` | `0.75` | Aim assist strength (`1` = console) |
| `gpad_sensitivity` | `1.0` | Look sensitivity |
| `gpad_invert` | `0` | Invert vertical look |
| `gpad_deadzone` | `0.2` | Stick deadzone |
| `gpad_aim_range` | `3200` | MW3 only. Aim assist range |
| `gpad_autoaim_range` | `1200` | MW3 only. Auto aim range |

## Troubleshooting

Look at `iw-pad.log` in the game folder. If it shows `unsupported game build`, open an issue and attach the log.

## Build

Requires [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) in `~/tools/llvm-mingw-*`. Run `./build.sh`.
Output: x64 in `build/`, x86 in `build/x86/`.

## Credits

- [IW4x](https://github.com/iw4x/iw4x-client) and [iw3sp_mod](https://github.com/ahrimd0n/iw3sp-mod): aim assist and controller logic.
- [AlterWare](https://alterware.dev): iw4x-sp symbols used to find game addresses.

## License

GPL-3.0. Not affiliated with Activision or Infinity Ward. You must own the game.
