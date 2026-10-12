# Plan: console-style input (after v0.0.3)

Status 2026-10-12: phases 1-3 implemented and built (not yet run in game); phase 4 open.
The release zips in build/release are still v0.0.3. Check iw-pad.log for `pad key names: BUTTON_LTRIG`,
`default controller binds applied` (first run after the AUX binds) and `input: controller` / `input: keyboard/mouse`.

Goal: make the pad look like a console pad to the engine and scripts, the way IW4x, iw3sp-mod and
Plutonium do, so console-only checks work and keyboard/mouse keep PC behaviour.

All facts below were checked in all 5 builds (MW2 x64 / x86 / AlterWare, MW3 x64 / x86) on 2026-10-12.

## 1. Console key numbers instead of AUX keys
- Buttons send the console keynums (IW4x `Mapping/Key.hpp`): A 0x01, B 0x02, X 0x03, Y 0x04, LB 0x05,
  RB 0x06, Start 0x0E, Back 0x0F, L3 0x10, R3 0x11, LT 0x12, RT 0x13, D-pad 0x14-0x17.
- Names: the key name table (`{name, keynum}` x 95) sits right before `localizedKeyNames` in every build
  (x86 -0x300, x64 -0x600; first entry "TAB"). Its 16 AUX1-AUX16 entries are renamed in place to
  BUTTON_A..DPAD_RIGHT with the console keynums, and the matching localized entries get the coloured
  hint names. No code patches: Key_StringToKeynum / Key_KeynumToString / Key_IsDown all read the table.
  - `bind BUTTON_A "+gostand"` parses and is saved by name; `buttonpressed("BUTTON_LTRIG")` works
    (fixes the NY Manhattan ADS hint).
- CL_KeyEvent checked (MW2 x86 0x4d2350): keys < 0x80 turn into ESC during cinematics/loading, so A skips a
  cinematic like on console. The `< 0xcf` branch only prints "unbound". Menus still get ENTER/ESC/arrows.
- Default binds are applied once when no BUTTON_* key is bound (old AUX binds stop parsing and are ignored).
- Start stays ESC (togglemenu).

## 2. Triggers: console keys plus raw mouse state
- LT/RT send BUTTON_LTRIG/BUTTON_RTRIG (own binds: +speed_throw / +attack).
- While held, the DLL also sets `keys[MOUSE1/MOUSE2].down` directly (LT = mouse1 = left arm in the scripts), so PC scripts that read raw mouse
  buttons (MW2 Cliffhanger ice picks, Ending crawl) still work, without firing the mouse binds.
  `keys[k].down` is 8 bytes before the binding field already in `kGames` (all 5 builds, from Key_IsDown).

## 3. "Pad in use" (gpad_in_use)
- New dvar `gpad_in_use`: set by any pad button/stick/trigger, cleared by a keyboard key or mouse button
  while the game window is in front. Mouse movement does not clear it (Deck gyro/trackpads are mouse).
- Hints prefer pad keys only while in use (today: while connected).
- `g_useholdtime` = 250 while in use, 0 otherwise, so keyboard F stays instant (same result as IW4x's
  +usereload-only hold, without a per-build hook).

## 4. Later (needs script overrides)
- MW3 Survival ready-up listens for `skip` on PC; no pad button sends it.
- Per-level `level.console || gpad_in_use` overrides like iw3sp-mod (minigun spin-up on LT, etc.).

## Test list (after this lands)
Cliffhanger climb, Ending crawl, weapon pickup next to a reload, keyboard F use (instant), cinematic skip
with A, binds survive a restart (config shows BUTTON_*), hints switch between pad and keyboard,
NY Manhattan ADS hint clears, MW3 Prague Escape crawl.
