// IW-Pad (iw-pad.dll): XInput controller + console-style aim assist for Steam MW2 SP x64 (iw4sp.exe) and
// MW3 SP x64 (iw5sp.exe). The x86 build of this DLL covers MW2 and MW3 SP x86. Each supported build is identified
// by its PE timestamp and image size. On any other build the DLL logs and does nothing, so the game runs vanilla.
#include <bit>
#include <windows.h>
#include <xinput.h>
#include <mmsystem.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstdarg>
#include <cstring>
#include <iterator>

#ifndef _WIN64
// x86 is MSVC usercall in two places, with different registers per game:
//   MW2: CL_MouseMove(edi = usercmd*, float frametime), Key_GetCommandAssignment(eax = client, cmd, keys)
//   MW3: CL_MouseMove(esi = usercmd*, float frametime), Key_GetCommandAssignment(ecx = client, eax = cmd, ebx = keys)
// These stubs adapt them to cdecl both ways.
extern "C" {
void iwpad_mouse_move(uint8_t* cmd, float frametime);
int iwpad_key_get_command_assignment(int client, const char* cmd, int* keys);
void stub_mouse_move();
void stub_key_get_command_assignment();
void call_mouse_move(uintptr_t fn, uint8_t* cmd, float frametime);
// Remote control move: eax = client, cmd in edi (MW2) / esi (MW3). Run it, then add the sticks.
void iwpad_remote_steer(uint8_t* cmd);
void stub_remote_move();
void stub_remote_move_mw3();
extern uintptr_t iwpad_remote_move;
uintptr_t iwpad_remote_move;
int call_key_get_command_assignment(uintptr_t fn, int client, const char* cmd, int* keys);
void stub_mouse_move_mw3();
void stub_key_get_command_assignment_mw3();
void call_mouse_move_mw3(uintptr_t fn, uint8_t* cmd, float frametime);
int call_key_get_command_assignment_mw3(uintptr_t fn, int client, const char* cmd, int* keys);
// R_AddCmdDrawText entry hook (iw5-mod's watermark): skip the draw when iwpad_skip_text says so, else run the
// 7 overwritten bytes (mov eax,[esp+4]; cmp byte [eax],0) and continue at iwpad_draw_text_resume.
bool iwpad_skip_text(const char* text);
void stub_draw_text();
// Quake 3 family (CoD2): CL_MouseMove(edi = cmd) has no frametime argument. CL_KeyEvent(eax = key, down, time);
// Cbuf_AddText(eax = text). SV_SightTrace(int* hit, start, pass0, pass1, mask) with ebx = end, esi = mins, edi = maxs.
void stub_mouse_move_q3();
void call_key_event_q3(uintptr_t fn, int key, int down, unsigned time);
void call_key_event_uo(uintptr_t fn, int key, int down, unsigned time);
int call_item_set_focus_uo(uintptr_t fn, void* item, float x, float y);
void call_cbuf_q3(uintptr_t fn, const char* text);
void call_sight_trace_q3(uintptr_t fn, int* hit, const float* start, const float* end, const float* mins,
                         const float* maxs, int pass0, int pass1, int mask);
void call_sight_trace_uo(uintptr_t fn, int* hit, const float* start, const float* end, const float* mins,
                         const float* maxs, int pass0, int pass1, int mask);
extern uintptr_t iwpad_draw_text_resume;
uintptr_t iwpad_draw_text_resume;
void stub_uo_keys_refilled();
void iwpad_hint_keys_refilled();
}
asm(R"(
.text
_stub_remote_move:                   # eax = client, edi = cmd
  call *_iwpad_remote_move
  pushl %edi
  call _iwpad_remote_steer
  addl $4, %esp
  ret
_stub_remote_move_mw3:               # eax = client, esi = cmd
  call *_iwpad_remote_move
  pushl %esi
  call _iwpad_remote_steer
  addl $4, %esp
  ret
_stub_mouse_move:                    # edi = cmd, 4(esp) = frametime
  pushl 4(%esp)
  pushl %edi
  call _iwpad_mouse_move
  addl $8, %esp
  ret
_stub_key_get_command_assignment:    # eax = client, 4(esp) = cmd, 8(esp) = keys
  pushl 8(%esp)
  pushl 8(%esp)
  pushl %eax
  call _iwpad_key_get_command_assignment
  addl $12, %esp
  ret
_call_mouse_move:                    # (fn, cmd, frametime)
  pushl %edi
  movl 12(%esp), %edi
  pushl 16(%esp)
  call *12(%esp)
  addl $4, %esp
  popl %edi
  ret
_call_key_get_command_assignment:    # (fn, client, cmd, keys)
  movl 8(%esp), %eax
  pushl 16(%esp)
  pushl 16(%esp)
  call *12(%esp)
  addl $8, %esp
  ret
_stub_mouse_move_mw3:                # esi = cmd, 4(esp) = frametime
  pushl 4(%esp)
  pushl %esi
  call _iwpad_mouse_move
  addl $8, %esp
  ret
_stub_key_get_command_assignment_mw3: # ecx = client, eax = cmd, ebx = keys
  pushl %ebx
  pushl %eax
  pushl %ecx
  call _iwpad_key_get_command_assignment
  addl $12, %esp
  ret
_call_mouse_move_mw3:                # (fn, cmd, frametime)
  pushl %esi
  movl 12(%esp), %esi
  pushl 16(%esp)
  call *12(%esp)
  addl $4, %esp
  popl %esi
  ret
_call_key_get_command_assignment_mw3: # (fn, client, cmd, keys)
  pushl %ebx
  movl 12(%esp), %ecx
  movl 16(%esp), %eax
  movl 20(%esp), %ebx
  call *8(%esp)
  popl %ebx
  ret
.globl _stub_remote_move, _stub_remote_move_mw3, _stub_mouse_move, _stub_key_get_command_assignment, _call_mouse_move, _call_key_get_command_assignment
_stub_draw_text:                     # cdecl R_AddCmdDrawText(text, ...)
  pushl 4(%esp)
  call _iwpad_skip_text
  addl $4, %esp
  testb %al, %al
  jnz 1f
  movl 4(%esp), %eax
  cmpb $0, (%eax)
  jmp *_iwpad_draw_text_resume
1:
  ret
.globl _stub_draw_text
_stub_mouse_move_q3:                 # edi = cmd; frametime comes from Game::frameMsec
  pushl $0
  pushl %edi
  call _iwpad_mouse_move
  addl $8, %esp
  ret
_call_key_event_q3:                  # (fn, key, down, time)
  movl 8(%esp), %eax
  pushl 16(%esp)
  pushl 16(%esp)
  call *12(%esp)
  addl $8, %esp
  ret
_call_key_event_uo:                  # (fn, key, down, time): CoD1/UO CL_KeyEvent(key), ebx = down, ecx = time
  pushl %ebx
  movl 12(%esp), %eax
  movl 16(%esp), %ebx
  movl 20(%esp), %ecx
  pushl %eax
  call *12(%esp)
  addl $4, %esp
  popl %ebx
  ret
_call_item_set_focus_uo:             # (fn, item, x, y): UO Item_SetFocus, esi = item, (float x, float y)
  pushl %esi
  movl 12(%esp), %esi
  pushl 20(%esp)
  pushl 20(%esp)
  call *16(%esp)
  addl $8, %esp
  popl %esi
  ret
_call_sight_trace_uo:                # (fn, hit, start, end, mins, maxs, pass0, pass1, mask): UO SV_SightTrace,
  pushl $0                           # edx = start, ecx = mins, eax = end; stack (hit, maxs, pass0, pass1, mask, 0)
  pushl 40(%esp)
  pushl 40(%esp)
  pushl 40(%esp)
  pushl 40(%esp)
  pushl 28(%esp)
  movl 36(%esp), %edx
  movl 40(%esp), %eax
  movl 44(%esp), %ecx
  call *28(%esp)
  addl $24, %esp
  ret
_stub_uo_keys_refilled:              # jumped to from the end of UO's hint key table refill (cgame 0x30042538)
  popl %esi                          # its epilogue: pop esi; pop ebx; add esp, 8; ret
  popl %ebx
  addl $8, %esp
  pushal
  call _iwpad_hint_keys_refilled
  popal
  ret
_call_cbuf_q3:                       # (fn, text)
  movl 8(%esp), %eax
  call *4(%esp)
  ret
_call_sight_trace_q3:                # (fn, hit, start, end, mins, maxs, pass0, pass1, mask)
  pushl %ebx
  pushl %esi
  pushl %edi
  movl 28(%esp), %ebx
  movl 32(%esp), %esi
  movl 36(%esp), %edi
  pushl 48(%esp)
  pushl 48(%esp)
  pushl 48(%esp)
  pushl 36(%esp)
  pushl 36(%esp)
  call *36(%esp)
  addl $20, %esp
  popl %edi
  popl %esi
  popl %ebx
  ret
.globl _stub_uo_keys_refilled
.globl _stub_mouse_move_q3, _call_key_event_q3, _call_key_event_uo, _call_item_set_focus_uo, _call_cbuf_q3, _call_sight_trace_q3, _call_sight_trace_uo
.globl _stub_mouse_move_mw3, _stub_key_get_command_assignment_mw3, _call_mouse_move_mw3, _call_key_get_command_assignment_mw3
)");
#endif

namespace {

#ifdef _WIN64
constexpr uintptr_t kBase = 0x140000000;
#else
constexpr uintptr_t kBase = 0x400000;
#endif
uintptr_t slide;
template <class T> T at(uintptr_t a) { return reinterpret_cast<T>(a + slide); }
template <class T> T& field(uint8_t* p, int off) { return *reinterpret_cast<T*>(p + off); }

// Aim assist dvars. MW2 still registers them (we read the engine's dvar_t* slots). MW3 stripped
// them, so there we register our own with the same names and the MW2 console defaults.
enum AimDvar { RANGE_SCALE, AUTO_RANGE_SCALE, GRAPH_ENABLED, GRAPH_INDEX, RATE_PITCH, RATE_PITCH_ADS, RATE_YAW,
  RATE_YAW_ADS, ACCEL_ENABLED, ACCEL_LERP, SLOW_ENABLED, SLOW_PITCH, SLOW_PITCH_ADS, SLOW_YAW, SLOW_YAW_ADS,
  AUTO_ENABLED, AUTO_LERP, LOCK_ENABLED, LOCK_DEFLECTION, LOCK_STRENGTH, SCALE_VIEW_AXIS, AIM_DVAR_COUNT };

#ifdef _WIN64
constexpr uintptr_t kMw2AimDvarSlots[AIM_DVAR_COUNT] = {  // written by AimAssist_RegisterDvars, value at +0x10
  0x1403cf050, 0x1403cf058, 0x1403cf060, 0x1403cf070, 0x1403cf078, 0x1403cf080, 0x1403cf088, 0x1403cf090,
  0x1403cf098, 0x1403cf0a8, 0x1403cf0b0, 0x1403cf0d0, 0x1403cf0d8, 0x1403cf0e0, 0x1403cf0e8, 0x1403cf0f0,
  0x1403cf100, 0x1403cf148, 0x1403cf158, 0x1403cf160, 0x1403cf178,
};
#else
constexpr uintptr_t kMw2AimDvarSlots[AIM_DVAR_COUNT] = {
  0x73ba84, 0x73ce70, 0x73c934, 0x73c920, 0x73ce74, 0x73c930, 0x73ba8c, 0x73ce88,
  0x73ba88, 0x73ceac, 0x73c93c, 0x73ce94, 0x73ceb0, 0x73ce8c, 0x73ce90, 0x73ce7c,
  0x73c914, 0x73ce84, 0x73ce64, 0x73c924, 0x73cea4,
};
constexpr uintptr_t kMw2AwAimDvarSlots[AIM_DVAR_COUNT] = {  // AlterWare build: the same slots, +0x3000
  0x73ea84, 0x73fe70, 0x73f934, 0x73f920, 0x73fe74, 0x73f930, 0x73ea8c, 0x73fe88,
  0x73ea88, 0x73feac, 0x73f93c, 0x73fe94, 0x73feb0, 0x73fe8c, 0x73fe90, 0x73fe7c,
  0x73f914, 0x73fe84, 0x73fe64, 0x73f924, 0x73fea4,
};
#endif

// Everything that differs between the supported builds. Mapped in Ghidra (projects mw2.gpr / mw3.gpr /
// mw2-x86.gpr / mw3-x86.gpr). The x64 and x86 DLLs are separate builds, each with its own table.
// menuDef_t / itemDef_s offsets that differ between the engines (x64). IW5 starts menuDef_t with a data pointer and
// adds a handler to itemDef_s after onFocus, so the later item fields move 8 bytes.
// UiContext / menuDef_t / itemDef_s offsets per engine and word size. IW5 starts menuDef_t with a data pointer and adds
// a handler to itemDef_s after onFocus. handlers: mouseEnterText .. dvar, cleared to turn a copy into a decoration.
// editStep: IW5's editFieldDef holds the left/right step where IW4 has the default value.
struct MenuLayout {
  int menuCount, menuName, itemCount, items;
  int itemSize, rectY, rectH, rectClientY, staticFlags, backColor, type, text, dvar, typeData, visibleExp;
  int handlers, handlersEnd;
  int floatExpCount;  // itemDef floatExpressionCount (rect expressions). IW5 drops imageTrack before it
  bool editStep;
};
[[maybe_unused]] constexpr MenuLayout
  kIw4Menus64{0x1438, 0, 0xBC, 0x168, 0x1E8, 0xC, 0x14, 0x20, 0x4C, 0x68, 0xC4, 0xF0, 0x148, 0x188, 0x1A0, 0x108, 0x150, 0x194, false},
  kIw5Menus64{0x1438, 8, 0xB8, 0xC0, 0x1F8, 0xC, 0x14, 0x20, 0x4C, 0x68, 0xC4, 0xF0, 0x150, 0x190, 0x1A8, 0x108, 0x158, 0x198, true},
  kIw4Menus86{0xA38, 0, 0xAC, 0x118, 0x17C, 0x8, 0x10, 0x1C, 0x44, 0x60, 0xB8, 0xE4, 0x110, 0x134, 0x144, 0xF0, 0x114, 0x13C, false},
  kIw5Menus86{0xA38, 4, 0xA8, 0xAC, 0x180, 0x8, 0x10, 0x1C, 0x44, 0x60, 0xB8, 0xE4, 0x114, 0x138, 0x144, 0xF0, 0x118, 0x13C, true};

struct Game {
  const char* name;
  DWORD timestamp, imageSize;
  uintptr_t callMouseMove, mouseMove;              // CL_CreateCmd -> CL_MouseMove(usercmd*, float frametime)
  uintptr_t callEventLoop[2], eventLoop;           // Com_Frame -> Com_EventLoop
  uintptr_t keyEvent, cbuf, sysMilliseconds, keynumToString, dvarBool, dvarFloat;
  uintptr_t getWeaponDef;                          // 0: weapon aim ranges unknown, use gpad_aim_range dvars
  int wdAutoAimRange;                              // WeaponDef autoAimRange. aimAssistRange(+4) and Ads(+8) follow
  uintptr_t keyCatchers, viewPitch, viewYaw, frameMsec, snapFlags, noLookFlags;
  int cmdForward;                                  // usercmd forwardmove offset, rightmove follows
  uintptr_t keyBindIdx, keyBindStr; int keyStride; // keys[k] bind command index (0: none) / bind string
  uintptr_t bindCommands, localizedKeyNames;
  uintptr_t hintCalls[3], hintFunc;                // MW2: Key_GetCommandAssignment call. MW3: CL_GetKeyBinding calls
  bool hintIsGetKeyBinding;
  uintptr_t aaGlob, aaGraphs;
  int aaShift, aaAutoMeleeState, aaLockOnTarget;   // MW3 dropped the 2 autoaim region floats: later fields move -8
  const uintptr_t* aimDvarSlots;
  bool mw3Regs;                                    // x86 usercall registers: false = MW2's, true = MW3's
  uintptr_t drawText;                              // R_AddCmdDrawText, hooked to drop iw5-mod's watermark (0: none)
  int dvarValue = 0x10;                            // dvar_t current value offset (MW3 x86: 0xC)
  uintptr_t useHoldTime = 0;                       // g_useholdtime dvar_t* slot (Player_UpdateActivate hold time)
  uintptr_t zoneLoad = 0;                          // DB_LoadXAssets(XZoneInfo*, count, sync). 0: no iwpad zone
  uintptr_t uiContext = 0;                         // UiContext (Menus[], menuCount). 0: stock menus left alone
  uintptr_t findAsset = 0;                         // DB_FindXAssetHeader(type, name). 0: stock strings left alone
  const MenuLayout* menus = nullptr;
  uintptr_t callRemoteMove = 0, remoteMove = 0;    // CL_CreateCmd -> remote control move (Predator), replaces CL_MouseMove
  bool cameraMove = false;                         // MW3: usercmd camera bytes at cmdForward + 4/5 (getnormalizedcameramovement)
  bool textIcons = false;                          // button glyphs in hints via text icons (make_glyphs)
  uintptr_t iconRead = 0;                          // x64: the text draw's `mov rcx,[r8+3]` (icon Material*)
  // x64 text width functions without an icon case, replaced by text_width(): [0] passes its 4th argument on to
  // SEH_ReadCharFromString (readChar), [1] takes 3 arguments
  uintptr_t textWidth[2] = {}, readChar = 0;
  uintptr_t iconLookupCall = 0, iconLookup = 0;    // IW5: the text draw's icon material lookup by name
  uintptr_t safeArea[2] = {};                      // safeArea_adjusted_horizontal / _vertical dvar_t* slots (0: no console HUD)
  bool iw5 = false;                                // IW5 Material layout, icons by name ("^\x01" w h len name)
  uintptr_t slotAlpha = 0;                         // action slot draw: `mov [rsp+0x7c], 1.0` (active slot alpha), nopped by console_hud
  // Quake 3 family (CoD2): usercall CL_KeyEvent / Cbuf_AddText, CL_MouseMove without frametime, AUX keys as the pad
  // keys, and no engine aim assist: q3_aim() builds the aim globals and screen targets from the game's own data.
  const struct Q3* q3 = nullptr;
  int keyCatchUi = 0x10;
  unsigned noLookMask = 0x800;                     // snapFlags bit that stops look (CL_MouseMove checks it)
  DWORD altIds[2][2] = {};
  uintptr_t cvarGet = 0, cvarSet = 0;              // Q3 string cvars (CoD1/UO): Cvar_Get(name, str, flags), Cvar_Set(name, str, force);
                                                   // value float at dvarValue (+0x1c), integer at +0x20
  int keyNameCount = 96;                           // key name table entries incl. the NULL end (localized table follows)                         // the same code under other PE headers: {timestamp, image size}
};

// Quake 3 family aim data (all x86, all static addresses)
struct Q3 {
  uintptr_t ps;                                    // cl.snap.ps: origin +0x14, velocity +0x20, weapon +0xb0,
                                                   // fWeaponPosFrac +0xb8, viewangles +0xc4, viewHeightCurrent +0xd4
  uintptr_t gentities, numEntities, sightTrace, cgFov, weaponDefs;
  int entSize, entType, entOrigin, entHealth, entActor, entSentient;  // gentity_t
  int actorEye, sentientTeam, etActor, wdAdsZoomFov;
  int psDeltaAngles;  // int[3] short angles: the view is cl.viewangles + these
  uintptr_t cinematic, clsState;
  uintptr_t uiCursorMat;  // the UI cursor's material handle ("ui/cursor"), drawn each UI frame  // CL_KeyEvent skips a video on a key < 128 when cinematic != 0 or state is 1 / 2
  const char* const* binds;                        // default pad binds, in button order
  uintptr_t refDrawText, refTextWidth, refTextHeight, refStretchPic, refRegisterMaterial;  // refexport_t slots
  uintptr_t uiDc;                                  // UI displayContext: cursor x/y ints +0xc/+0x10, menuStack +0x130,
                                                   // openMenuCount +0x170. menuDef itemCount +0x218, items +0x27c;
                                                   // itemDef rect floats +0, window flags +0xe8 (4 visible, 2 focus)
  int aux1 = 207;                                  // AUX1 keynum (UO: 239, JOY1-32 come first)
  int videoStates[2] = {1, 2};                     // cls.state values while a video plays (A skips it)
  bool keyEventUo = false;                         // CL_KeyEvent(key) with ebx = down, ecx = time (CoD1/UO)
  int psWeapon = 0xb0, psAdsFrac = 0xb8, psViewHeight = 0xd4;  // playerState weapon / fWeaponPosFrac / viewHeightCurrent
  const struct Q3Menus* menus = nullptr;           // UI in a DLL with d-pad navigation done by us (UO)
  const char* gameDll = nullptr; uintptr_t gameBase = 0;    // gentities / numEntities live in this DLL (preferred base)
  const char* cgameDll = nullptr; uintptr_t cgameBase = 0;
  uintptr_t fovX = 0;                              // cgame refdef fov_x (ADS zoom included). 0: cgFov + weapon zoom
  bool sightUo = false; int sightMask = 0x801803;  // SV_SightTrace registers (UO) / contents mask (CoD2: sighttracepassed's)
  uintptr_t hintTable = 0; int hintCount = 0;      // cgame command -> keys table the hints read (q3_hint_keys)
  uintptr_t refSetColor = 0, adjustFrom640 = 0;    // UO text hook (hk_uo_draw_text): re SetColor, ri adjust pointer
  uintptr_t videoFillJle = 0;                      // SCR_DrawScreenField: `jle` (0x7e) past the widescreen black fill
  uintptr_t hintRefillEnd = 0;                     // cgame: epilogue of the hint table refill (q3_hint_keys hooks it)
  uintptr_t gameWeaponDefs = 0;                    // game DLL: WeaponDef** pointer (quick grenade)
  int wdSlot = 0x84, psWeaponState = 0xcc;         // WeaponDef weaponSlot, playerState weaponstate (UO values)
};

constexpr const char* kCod2Binds[] = {
  "+gostand", "+stance", "+usereload", "weapnext", "+smoke", "+frag", "+speed", "+attack",
  "+holdbreath", "+melee", "+binoculars", "+prone", "+leanleft", "+leanright", "+scores",
};
// CoD2 SP 1.3 (ghidra/cod2-x86.gpr). Entity layout from the script field tables, geteye and issentient.
[[maybe_unused]] constexpr Q3 kCod2{
  0x617f20, 0x1b15f00, 0x1c6050c, 0x44c3c0, 0x1040c6c, 0x1c64548,
  0x1fc, 0, 0xdc, 0x13c, 0x100, 0x104,
  0x174, 4, 9, 0x268,
  0x54, 0x617dbc, 0x5d5680, 0x1107444,
  kCod2Binds, 0x617be4, 0x617bdc, 0x617be0, 0x617b5c, 0x617ac0, 0x10fec30,
};

// CoD United Offensive SP (ghidra/coduo.gpr, exe part only so far): entities, weapons, FOV, UI, text and the hint
// lookup live in the uo_*.dll modules and aren't mapped yet, so this is input + look only (no aim targets, no icons).
// UO has no +usereload / grenade buttons (grenades are a weapon slot) and ADS is +speed.
constexpr const char* kUoBinds[] = {
  "+gostand", "togglecrouch", "+reload", "weapnext", "weapprev", "weaponslot grenade", "+speed", "+attack",
  "+sprint", "+melee", "weaponslot pistol", "weapalt", "+leanleft", "+leanright", "+scores",
};
static_assert(std::size(kUoBinds) == std::size(kCod2Binds));
// UI module data for menus we navigate ourselves: addresses are the DLL's (preferred base), rebased at runtime
struct Q3Menus {
  const char* module; uintptr_t modBase;
  uintptr_t dcPtr; int cursorOff;                  // displayContext* global. Cursor x/y ints at dc + cursorOff
  uintptr_t stack, count;                          // menuStack[] and openMenuCount
  int flags, itemCount, items, cursorItem;         // menuDef (window flags: 2 focus, 4 visible. itemDefs share them)
  int textRect, type;                              // itemDef textRect (x, baseline y, w, h), type (0 = text)
  uintptr_t setFocus;                              // Item_SetFocus: esi = item, (float x, float y)
  uintptr_t activeMenu; int clickOnlyMenu;        // UI's active menu id (vmMain 8): CL_KeyEvent passes only MOUSE1 to this one
  uintptr_t cursorJle;                             // UI refresh: the `jle` (0x7e) that skips the cursor draw. 0xeb hides it
  uintptr_t currentVm, uiVm;                       // exe: currentVM, uivm. DLL code may only syscall while currentVM is
                                                   // its vm (VM_Call sets it. The syscall entry calls through it unchecked)
  int itemCvar = 0x11c, itemEditField = 0x258;     // itemDef cvar name, editFieldDef* (keyword handlers "cvar" / "cvarFloat")
};
// uo_uix86.dll: Menu_HandleKey 0x400123f0, Menu_GetFocused 0x400159b0, next/prev cursor item 0x40011f00 / 0x40011e20
constexpr Q3Menus kUoMenus{ "uo_uix86.dll", 0x40000000, 0x401be428, 0xf0, 0x401e5140, 0x401be440,
                            0x4c, 0xc4, 0x520, 0xcc, 0xbc, 0xcc, 0x4000ff10, 0x401be418, 7, 0x400082f6, 0x37c07ac, 0x482df44 };
[[maybe_unused]] constexpr Q3 kCodUo{
  .ps = 0x482e094, .gentities = 0x201549c0, .numEntities = 0x20307f0c, .sightTrace = 0x450d70,
  .entSize = 0x2d0, .entOrigin = 0xe4, .entHealth = 0x1e0, .entActor = 0x10c, .entSentient = 0x110,
  .actorEye = 0x1a8, .sentientTeam = 4, .etActor = -1,
  .psDeltaAngles = 0x4c, .clsState = 0x4842100, .binds = kUoBinds,
  // refexport copy at 0x482df80 (GetRefAPI 0x4ae190): DrawText +0xe4, TextWidth +0xdc, TextHeight +0xe0, StretchPic
  // +0x78, RegisterShader +0x14 (SetColor +0x74 below)
  .refDrawText = 0x482e064, .refTextWidth = 0x482e05c, .refTextHeight = 0x482e060, .refStretchPic = 0x482dff8,
  .refRegisterMaterial = 0x482df94,
  .aux1 = 239, .videoStates = {3, 4}, .keyEventUo = true, .psWeapon = 0xc8, .psAdsFrac = 0xd0, .psViewHeight = 0xe8,
  .menus = &kUoMenus,
  // uo_gamex86: g_entities (stride 0x2d0, getEye 0x20035830), level.num_entities. gentity origin +0xe4, health +0x1e0,
  // client +0x108, actor +0x10c (eye +0x1a8), sentient +0x110 (team +4, field table with threatbias). No eType check
  // (etActor -1): living AI = actor + sentient + health. SV_SightTrace 0x450d70 = game syscall 0x34 (AI mask 0x801003).
  .gameDll = "uo_gamex86.dll", .gameBase = 0x20000000, .cgameDll = "uo_cgamex86.dll", .cgameBase = 0x30000000,
  .fovX = 0x30275ec0, .sightUo = true, .sightMask = 0x801003, .hintTable = 0x30073870, .hintCount = 43,
  .refSetColor = 0x482dff4, .adjustFrom640 = 0x47bccf0,
  .videoFillJle = 0x411624,  // SCR_DrawScreenField 0x4115f0
  .hintRefillEnd = 0x30042538, .gameWeaponDefs = 0x20151f34,
};

// CoD1 SP (ghidra/cod1.gpr): UO is built on the same code, so this is kCodUo with CoD1's addresses (each checked by
// hand, ghidra/sigmap/cod1-map.txt). Same pad layout as UO (kUoBinds) so both feel alike. CoD1 has no +sprint and
// no smoke grenades, so L3 and LB do nothing there.
// uix86.dll: Menu_HandleKey 0x40011e20, next/prev cursor item 0x40011920 / 0x40011840
constexpr Q3Menus kCod1Menus{ "uix86.dll", 0x40000000, 0x401bd354, 0xf0, 0x4021a460, 0x401bd36c,
                              0x48, 0xc0, 0x510, 0xc8, 0xb8, 0xc8, 0x4000f9a0, 0x401bd394, 7, 0x40007fcf, 0x13f8d94, 0x14bc5e8,
                              0x118, 0x254 };
[[maybe_unused]] constexpr Q3 kCod1{
  .ps = 0x1425e14, .gentities = 0x2012fa80, .numEntities = 0x202db22c, .sightTrace = 0x44e4a0,
  .entSize = 0x2c8, .entOrigin = 0xe0, .entHealth = 0x1d8, .entActor = 0x108, .entSentient = 0x10c,
  .actorEye = 0x1a8, .sentientTeam = 4, .etActor = -1,
  .psDeltaAngles = 0x48, .clsState = 0x1438200, .binds = kUoBinds,
  .refDrawText = 0x1425ddc, .refTextWidth = 0x1425dd4, .refTextHeight = 0x1425dd8, .refStretchPic = 0x1425d70,
  .refRegisterMaterial = 0x1425d14,  // +0x1c here: CoD1's +0x14 is RegisterModel (live table: cl copy = GetRefAPI table + 8)
  .aux1 = 239, .videoStates = {3, 4}, .keyEventUo = true, .psWeapon = 0xa0, .psAdsFrac = 0xa8, .psViewHeight = 0xc0,
  .menus = &kCod1Menus,
  .gameDll = "gamex86.dll", .gameBase = 0x20000000, .cgameDll = "cgamex86.dll", .cgameBase = 0x30000000,
  .fovX = 0x30125734, .sightUo = true, .sightMask = 0x801003, .hintTable = 0x30060138, .hintCount = 42,
  .refSetColor = 0x1425d6c, .adjustFrom640 = 0x1548630,
  .videoFillJle = 0x41042d,  // SCR_DrawScreenField 0x410400
  .hintRefillEnd = 0x30039578, .gameWeaponDefs = 0x20125650, .wdSlot = 0x78, .psWeaponState = 0xa4,
};
// CoD1 SP 1.5 (Steam, and the patched CD install): gamex86 / cgamex86 / uix86 code is identical to 1.1 (only build
// strings differ), so only the exe addresses move (ported from 1.1 with ghidra/port.py, checked against the decrypted
// Steam image x86/steam/cod1/CoDSP-unpacked.exe).
constexpr Q3Menus kCod1Menus15{ "uix86.dll", 0x40000000, 0x401bd354, 0xf0, 0x4021a460, 0x401bd36c,
                              0x48, 0xc0, 0x510, 0xc8, 0xb8, 0xc8, 0x4000f9a0, 0x401bd394, 7, 0x40007fcf, 0x13f9db4, 0x14bd648,
                              0x118, 0x254 };
[[maybe_unused]] constexpr Q3 kCod1_15{
  .ps = 0x1426e74, .gentities = 0x2012fa80, .numEntities = 0x202db22c, .sightTrace = 0x44eab0,
  .entSize = 0x2c8, .entOrigin = 0xe0, .entHealth = 0x1d8, .entActor = 0x108, .entSentient = 0x10c,
  .actorEye = 0x1a8, .sentientTeam = 4, .etActor = -1,
  .psDeltaAngles = 0x48, .clsState = 0x1439260, .binds = kUoBinds,
  .refDrawText = 0x1426e3c, .refTextWidth = 0x1426e34, .refTextHeight = 0x1426e38, .refStretchPic = 0x1426dd0,
  .refRegisterMaterial = 0x1426d74,  // +0x1c here: CoD1's +0x14 is RegisterModel (live table: cl copy = GetRefAPI table + 8)
  .aux1 = 239, .videoStates = {3, 4}, .keyEventUo = true, .psWeapon = 0xa0, .psAdsFrac = 0xa8, .psViewHeight = 0xc0,
  .menus = &kCod1Menus15,
  .gameDll = "gamex86.dll", .gameBase = 0x20000000, .cgameDll = "cgamex86.dll", .cgameBase = 0x30000000,
  .fovX = 0x30125734, .sightUo = true, .sightMask = 0x801003, .hintTable = 0x30060138, .hintCount = 42,
  .refSetColor = 0x1426dcc, .adjustFrom640 = 0x1549690,
  .videoFillJle = 0x41043d,  // SCR_DrawScreenField 0x410410
  .hintRefillEnd = 0x30039578, .gameWeaponDefs = 0x20125650, .wdSlot = 0x78, .psWeaponState = 0xa4,
};

constexpr Game kGames[] = {
#ifdef _WIN64
  { "MW2 SP x64 (iw4sp.exe)", 0x6AAAA257, 0x4942000,
    0x1400d0583, 0x1400cfcf0, {0x1401fbfa7, 0x1401fc309}, 0x1401fbb80,
    0x1400d0f80, 0x1401ee890, 0x140293dc0, 0x1400d1a10, 0x14026c930, 0x14026cb90,
    0x140085850, 0x428,
    0x140533cb0, 0x140533d14, 0x140533d18, 0x14052fb70, 0x140535d44, 0,
    0x1a,
    0x1405300f0, 0x1405300f8, 0x18,
    0x1403be770, 0x1403be120,
    {0x1400d0e62}, 0x1400d1880, false,
    0x1403cf180, 0x1403d0000,
    0, 0xE58, 0xE70,
    kMw2AimDvarSlots, false, 0, 0x10, 0x140d9c050, 0x1400f0ae0, 0x141a3ab00, 0x1400ef5d0, &kIw4Menus64, 0x1400d053c, 0x1400cff70, false, true, 0x140043a57, {0, 0x140009b30}, 0x140241f80,
    0, 0, {0x1405e0b70, 0x1405e0b78}, false, 0x14008b709 },
  { "MW3 SP x64 (iw5sp.exe)", 0x6A743A58, 0x44BE000,
    0x14007e3c3, 0x14007d9f0, {0x14023d0e7, 0x14023d453}, 0x14023ccb0,
    0x14007eaf0, 0x14022f680, 0x1402f2210, 0x14007f1e0, 0x1402c43f0, 0x1402c4650,
    0, 0,
    0x1406e2550, 0x1406e2738, 0x1406e273c, 0x1406446c0, 0x1406e4774, 0x1406e477c,
    0x1c,
    0x140644a6c, 0, 0xC,
    0x1404c1870, 0x1404c1220,
    {0x1402a36b3, 0x1402a37ca, 0x1401585aa}, 0x14007e960, true,
    0x1404d9a00, 0x1404da860,
    8, 0xE44, 0xE5C,
    nullptr, false, 0, 0x10, 0x14114a9d0, 0, 0x142605050, 0x1400a5a20, &kIw5Menus64, 0x14007e37c, 0x14007de20, true,
    true, 0, {0x1401b80f0, 0x1401b8020}, 0x14028a010, 0x140185b21, 0x1401c4b80, {0x1407172b0, 0x1407172b8}, true, 0x140030ae2 },
#else
  // Steam x86 build (PE 2010-02-10), the one IW4x-SP's zip ships. CL_MouseMove and
  // Key_GetCommandAssignment are usercall here, see the stubs below.
  { "MW2 SP x86 (iw4sp.exe)", 0x4B7215E1, 0x2150000,
    0x579b3d, 0x578fe0, {0x496b68, 0x496dae}, 0x43a300,
    0x4d2350, 0x409b40, 0x44e1b0, 0x49c610, 0x4866b0, 0x49f700,
    0x427850, 0x2e0,
    0x925bb8, 0x925c34, 0x925c38, 0x8889a4, 0x926c64, 0,
    0x1a,
    0, 0x8893c4, 0xC,
    0, 0x72f988,
    {0x579e98}, 0x579c70, false,
    0x73ba90, 0x73c940,
    0, 0xE58, 0xE70,
    kMw2AimDvarSlots, false, 0, 0x10, 0x1063afc, 0, 0, 0, nullptr, 0x579ae2, 0x579960 },
  // AlterWare build (PE 2009-11-07, iw4x-sp's data/iw4sp.exe), mapped by iw4x-sp.exe into its own image.
  // Ported from the entry above by masked byte signatures (ghidra/mw2-aw.gpr). Same code, other addresses.
  { "MW2 SP x86 AlterWare (iw4x-sp)", 0x4AF4BDCC, 0x2153000,
    0x57bb4d, 0x57aff0, {0x4d4098, 0x4d42de}, 0x4987c0,
    0x442d60, 0x4a1090, 0x44e130, 0x434c80, 0x429390, 0x4051d0,
    0x4abe10, 0x2e0,
    0x929140, 0x9291bc, 0x9291c0, 0x88ba04, 0x92a1ec, 0,
    0x1a,
    0, 0x88c94c, 0xC,
    0, 0x732988,
    {0x57c648}, 0x57c520, false,
    0x73ea90, 0x73f940,
    0, 0xE58, 0xE70,
    kMw2AwAimDvarSlots, false, 0, 0x10, 0x106707c, 0, 0x1920470, 0x40b200, &kIw4Menus86, 0x57baf2, 0x57b970, false, true,
    0, {0, 0x508960}, 0x416100, 0, 0, {0x93aaf8, 0x93aa80}, false, 0x4269d8 },
  // Steam x86 build (PE 0x50B8E96C). Same usercall functions as MW2 x86, other registers. The hint
  // lookup is a separate Key_GetCommandAssignment again (MW3 x64 inlines it into CL_GetKeyBinding).
  { "MW3 SP x86 (iw5sp.exe)", 0x50B8E96C, 0x2444000,
    0x57e53f, 0x57d7e0, {0x4586d6, 0x45891d}, 0x54b9f0,
    0x541020, 0x457c90, 0x4a1610, 0x4bb000, 0x4914d0, 0x4f9cc0,
    0, 0,
    0xb36210, 0xb36408, 0xb3640c, 0xa98acc, 0xb37444, 0xb3744c,
    0x1c,
    0xa98e4c, 0, 0xC,
    0x929fa0, 0x929c78,
    {0x57e784, 0x4e533d, 0x4106a0}, 0x57e640, false,
    0x94d290, 0x94e0f8,
    8, 0xE44, 0xE5C,
    nullptr, true, 0x51b100, 0xC, 0x1357a70, 0, 0x1c00458, 0x4ff000, &kIw5Menus86, 0x57e4df, 0x57e360, true,
    // icons by name like x64: 0x690594 calls 0x52f000 = DB_FindXAssetHeader(MATERIAL, name, 1) (iw5-mod hooks the
    // same call). R_TextWidth 0x4e9350 (4 args, readChar 0x4db3e0) has no icon case, so it's replaced
    true, 0, {0x4e9350, 0}, 0x4db3e0, 0x690594, 0x52f000, {0xb4dc8c, 0xb4dc10}, true, 0x50090b },
  // CoD2 SP 1.3 (PE 2006-03-31). The Steam exe is PECompact-packed (image 0x18ca000). Unpacked copies are 0x18cc000.
  // DeckOps' install has a plain 4-section exe with a broken timestamp (0x564544). Same code in all three.
  // Mapped statically 2026-11-19, runs on the Deck and Steam. aaAutoMeleeState / aaLockOnTarget index q3_aim's own buffer.
  { .name = "CoD2 SP 1.3 (CoD2SP_s.exe)", .timestamp = 0x442CA3F7, .imageSize = 0x18cc000,
    .callMouseMove = 0x409dd8, .mouseMove = 0x409720, .callEventLoop = {0x42ed87, 0x42edef}, .eventLoop = 0x42cfb0,
    .keyEvent = 0x40c0e0, .cbuf = 0x41afc0, .sysMilliseconds = 0x450f90, .keynumToString = 0,
    .dvarBool = 0x431f60, .dvarFloat = 0x432020, .getWeaponDef = 0, .wdAutoAimRange = 0x24c,
    .keyCatchers = 0x617f04, .viewPitch = 0x621584, .viewYaw = 0x621588, .frameMsec = 0x5d1fa4, .snapFlags = 0x617f2c,
    .noLookFlags = 0, .cmdForward = 0x18, .keyBindIdx = 0, .keyBindStr = 0x5d2730, .keyStride = 0xC,
    .bindCommands = 0, .localizedKeyNames = 0x59a3e0, .hintCalls = {0x4cdbfe, 0x4ecdaa}, .hintFunc = 0x4cdb30,
    .hintIsGetKeyBinding = false,
    .aaGlob = 0, .aaGraphs = 0, .aaShift = 0, .aaAutoMeleeState = 0xE58, .aaLockOnTarget = 0xE70,
    .aimDvarSlots = nullptr, .mw3Regs = false, .drawText = 0, .dvarValue = 8, .useHoldTime = 0x1c1488c,
    .q3 = &kCod2, .keyCatchUi = 8, .noLookMask = 0x8000,
    .altIds = {{0x442CA3F7, 0x18ca000}, {0x564544, 0x18c4a46}} },
  // CoD United Offensive SP 1.41 (PE 2004-08-20, DeckOps' copy. SafeDisc stub sections unused, code is plain).
  // Quake 3 string cvars, CL_KeyEvent with key on the stack + ebx/ecx, AUX keys at 239. Also accepts the packed Steam exe.
  { .name = "CoD UO SP 1.41 (CoDUOSP.exe)", .timestamp = 0x412653f0, .imageSize = 0x452a000,
    .callMouseMove = 0x40be71, .mouseMove = 0x40b800, .callEventLoop = {0x432612, 0x432691}, .eventLoop = 0x430f00,
    .keyEvent = 0x40e030, .cbuf = 0x421ed0, .sysMilliseconds = 0x455200, .keynumToString = 0,
    .dvarBool = 0, .dvarFloat = 0, .getWeaponDef = 0, .wdAutoAimRange = 0,
    .keyCatchers = 0x4842104, .viewPitch = 0x483736c, .viewYaw = 0x4837370, .frameMsec = 0x48cd5c0, .snapFlags = 0x482e0a0,
    .noLookFlags = 0, .cmdForward = 0x14, .keyBindIdx = 0, .keyBindStr = 0x48ca628, .keyStride = 0xC,
    .bindCommands = 0, .localizedKeyNames = 0x571580, .hintCalls = {}, .hintFunc = 0, .hintIsGetKeyBinding = false,
    .aaGlob = 0, .aaGraphs = 0, .aaShift = 0, .aaAutoMeleeState = 0xE58, .aaLockOnTarget = 0xE70,
    .aimDvarSlots = nullptr, .mw3Regs = false, .drawText = 0, .dvarValue = 0x1c, .useHoldTime = 0,
    .q3 = &kCodUo, .keyCatchUi = 2, .noLookMask = 0x4000,
    .altIds = {{0x412653f0, 0x4524000}},  // Steam exe: packed, same 1.41 code once unpacked (only padding differs)
    .cvarGet = 0x4337f0, .cvarSet = 0x433a60, .keyNameCount = 128 },
  // CoD1 SP 1.1 (PE 2003-10-06, DeckOps' Games/cod1, sha1 887e03c2).
  { .name = "CoD SP (CoDSP.exe)", .timestamp = 0x3f80eabf, .imageSize = 0x146289e,
    .callMouseMove = 0x40af90, .mouseMove = 0x40aa10, .callEventLoop = {0x430790, 0x430844}, .eventLoop = 0x42f170,
    .keyEvent = 0x40d000, .cbuf = 0x4209c0, .sysMilliseconds = 0x452900, .keynumToString = 0,
    .dvarBool = 0, .dvarFloat = 0, .getWeaponDef = 0, .wdAutoAimRange = 0,
    .keyCatchers = 0x1438204, .viewPitch = 0x142e2a4, .viewYaw = 0x142e2a8, .frameMsec = 0x1422c04, .snapFlags = 0x1425e20,
    .noLookFlags = 0, .cmdForward = 0x14, .keyBindIdx = 0, .keyBindStr = 0x1422d68, .keyStride = 0xC,
    .bindCommands = 0, .localizedKeyNames = 0x56d9f0, .hintCalls = {}, .hintFunc = 0, .hintIsGetKeyBinding = false,
    .aaGlob = 0, .aaGraphs = 0, .aaShift = 0, .aaAutoMeleeState = 0xE58, .aaLockOnTarget = 0xE70,
    .aimDvarSlots = nullptr, .mw3Regs = false, .drawText = 0, .dvarValue = 0x1c, .useHoldTime = 0,
    .q3 = &kCod1, .keyCatchUi = 2, .noLookMask = 0x4000, .cvarGet = 0x4319e0, .cvarSet = 0x431ce0, .keyNameCount = 128 },
  // CoD1 SP 1.5 (PE 2004-03-02). Steam exe (packed) and the SafeDisc CD exe. Both decrypt to the same code. Deluxe
  // Edition's unprotected exe (image size 0x146389e) has the same code and constant data as the unpacked Steam image.
  { .name = "CoD SP 1.5 (CoDSP.exe)", .timestamp = 0x40443fda, .imageSize = 0x1467000,
    .callMouseMove = 0x40af90, .mouseMove = 0x40aa10, .callEventLoop = {0x4307a0, 0x430854}, .eventLoop = 0x42f180,
    .keyEvent = 0x40d000, .cbuf = 0x4209d0, .sysMilliseconds = 0x452f10, .keynumToString = 0,
    .dvarBool = 0, .dvarFloat = 0, .getWeaponDef = 0, .wdAutoAimRange = 0,
    .keyCatchers = 0x1439264, .viewPitch = 0x142f304, .viewYaw = 0x142f308, .frameMsec = 0x1423c64, .snapFlags = 0x1426e80,
    .noLookFlags = 0, .cmdForward = 0x14, .keyBindIdx = 0, .keyBindStr = 0x1423dc8, .keyStride = 0xC,
    .bindCommands = 0, .localizedKeyNames = 0x56ea00, .hintCalls = {}, .hintFunc = 0, .hintIsGetKeyBinding = false,
    .aaGlob = 0, .aaGraphs = 0, .aaShift = 0, .aaAutoMeleeState = 0xE58, .aaLockOnTarget = 0xE70,
    .aimDvarSlots = nullptr, .mw3Regs = false, .drawText = 0, .dvarValue = 0x1c, .useHoldTime = 0,
    .q3 = &kCod1_15, .keyCatchUi = 2, .noLookMask = 0x4000, .altIds = {{0x40443fda, 0x146b000}, {0x40443fda, 0x146389e}}, .cvarGet = 0x4319f0, .cvarSet = 0x431cf0, .keyNameCount = 128 },
#endif
};
const Game* g;

constexpr int K_ENTER = 13, K_ESCAPE = 27, K_UP = 154, K_DOWN = 155, K_LEFT = 156, K_RIGHT = 157, K_MOUSE1 = 200,
  K_MOUSE2 = 201;
constexpr int KEYCATCH_CONSOLE = 0x1;
constexpr int CMD_BUTTON_ADS = 0x800, AIM_TARGET_INVALID = 0x7ff;

using CL_MouseMove_t = void (*)(uint8_t* cmd, float frametime);
using Com_EventLoop_t = void (*)();
using CL_KeyEvent_t = void (*)(int client, int key, int down, unsigned time);  // MW3 ignores time
using Cbuf_AddText_t = void (*)(int client, const char* text);
using Sys_Milliseconds_t = int (*)();
using Key_KeynumToString_t = const char* (*)(int key, int translate);
using BG_GetWeaponDef_t = uint8_t* (*)(intptr_t weapon);
using Dvar_RegisterBool_t = uint8_t* (*)(const char*, bool, unsigned, const char*);
using Dvar_RegisterFloat_t = uint8_t* (*)(const char*, float, float, float, unsigned, const char*);

struct AimScreenTarget {
  int entIndex;
  float clipMins[2], clipMaxs[2], aimPos[3], velocity[3], distSqr, crosshairDistSqr;
};
static_assert(sizeof(AimScreenTarget) == 0x34);

// AimAssistGlobals (IW4x's x86 layout matches MW2 x64: no pointers). MW3 is the same up to the aim
// regions, keeps 6 of MW2's 8 region floats (no autoaim), so later fields sit aaShift bytes earlier.
struct Aim {
  uint8_t* p;
  template <class T> T& f(int off) const { return *reinterpret_cast<T*>(p + off); }
  template <class T> T& s(int mw2off) const { return f<T>(mw2off - g->aaShift); }
  const float* velocity() const { return &f<float>(0x0); }
  int weapFlags() const { return f<int>(0x18); }
  int weaponState() const { return f<int>(0x1c); }
  int weapon() const { return f<int>(0x24); }
  bool hasAmmo() const { return f<bool>(0x28); }
  bool initialized() const { return f<bool>(0xb0); }
  float slowW() const { return f<float>(0xb8); }
  float slowH() const { return f<float>(0xbc); }
  float lockW() const { return s<float>(0xd0); }
  float lockH() const { return s<float>(0xd4); }
  // MW3 has no autoaim region. Derive it like MW2 does (160x120 vs 90x90 slowdown, scaled by fovScaleInv)
  float autoW() const { return g->aaShift ? slowW() * (160.0f / 90.0f) * fovScaleInv() : f<float>(0xc0); }
  float autoH() const { return g->aaShift ? slowH() * (120.0f / 90.0f) * fovScaleInv() : f<float>(0xc4); }
  const float* viewOrigin() const { return &s<float>(0xe4); }
  const float* viewAngles() const { return &s<float>(0xf0); }
  const float* viewAxis(int i) const { return &s<float>(0xfc + i * 12); }
  float fovTurnRateScale() const { return s<float>(0x120); }
  float fovScaleInv() const { return s<float>(0x124); }
  float adsLerp() const { return s<float>(0x128); }
  float& pitchDelta() const { return s<float>(0x12c); }
  float& yawDelta() const { return s<float>(0x130); }
  int targetCount() const { return s<int>(0xe3c); }
  const AimScreenTarget& target(int i) const { return (&s<AimScreenTarget>(0x13c))[i]; }
  int autoMeleeState() const { return f<int>(g->aaAutoMeleeState); }
  int& lockOnTarget() const { return f<int>(g->aaLockOnTarget); }
};

// --- logging
FILE* g_log;
void logmsg(const char* fmt, ...) {
  if (!g_log) return;
  va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
  fputc('\n', g_log); fflush(g_log);
}

// --- dvars (dvar_t current value at g->dvarValue)
uint8_t* own_aim_dvars[AIM_DVAR_COUNT];
uint8_t* aim_dvar(AimDvar d) { return g->aimDvarSlots ? *at<uint8_t**>(g->aimDvarSlots[d]) : own_aim_dvars[d]; }
float dvf(AimDvar d, float def) { auto p = aim_dvar(d); return p ? *reinterpret_cast<float*>(p + g->dvarValue) : def; }
// Q3 cvars (g->cvarGet) keep the value as a float plus an int at +0x20. dvars keep bools as a byte
int dv_int(uint8_t* p) { return g->cvarGet ? *reinterpret_cast<int*>(p + 0x20) : *reinterpret_cast<int*>(p + g->dvarValue); }
bool dv_bool(uint8_t* p) { return g->cvarGet ? *reinterpret_cast<int*>(p + 0x20) != 0 : *(p + g->dvarValue) != 0; }
bool dvb(AimDvar d, bool def) { auto p = aim_dvar(d); return p ? dv_bool(p) : def; }
int dvi(AimDvar d, int def) { auto p = aim_dvar(d); return p ? dv_int(p) : def; }
float ownf(uint8_t* p, float def) { return p ? *reinterpret_cast<float*>(p + g->dvarValue) : def; }
bool ownb(uint8_t* p, bool def) { return p ? dv_bool(p) : def; }
using Cvar_Get_t = uint8_t* (*)(const char* name, const char* value, unsigned flags);
using Cvar_Set_t = void (*)(const char* name, const char* value, int force);
// writes a bool: Q3 cvars through Cvar_Set (menus test the string), dvars in place
void set_bool(uint8_t* p, bool v) {
  if (!p) return;
  if (g->cvarSet) at<Cvar_Set_t>(g->cvarSet)(field<const char*>(p, 0), v ? "1" : "0", 1);
  else *(p + g->dvarValue) = v;
}
uint8_t* reg_bool(const char* name, bool v, unsigned flags, const char* desc) {
  if (g->cvarGet) return at<Cvar_Get_t>(g->cvarGet)(name, v ? "1" : "0", flags);
  return at<Dvar_RegisterBool_t>(g->dvarBool)(name, v, flags, desc);
}
uint8_t* reg_float(const char* name, float v, float min, float max, unsigned flags, const char* desc) {
  if (g->cvarGet) { char s[32]; snprintf(s, sizeof s, "%g", v); return at<Cvar_Get_t>(g->cvarGet)(name, s, flags); }
  return at<Dvar_RegisterFloat_t>(g->dvarFloat)(name, v, min, max, flags, desc);
}

uint8_t *gpad_console_hud, *gpad_hud_safe_area, *gpad_ps_glyphs, *gpad_in_use, *gpad_reset, *gpad_enabled, *gpad_sensitivity, *gpad_invert, *gpad_deadzone, *gpad_aim_assist, *gpad_aim_strength, *gpad_aim_range, *gpad_autoaim_range;
[[maybe_unused]] uint8_t* gpad_quick_grenade;  // CoD1 / UO only (x86)

void register_dvars() {
  auto rb = reg_bool;
  auto rf = reg_float;
  constexpr unsigned ARCHIVE = 1;
  gpad_enabled = rb("gpad_enabled", true, ARCHIVE, "Enable controller input");
  gpad_in_use = rb("gpad_in_use", false, 0, "The controller was used last (keyboard or mouse buttons clear it)");
  gpad_reset = rb("gpad_reset", false, 0, "Set to reset the controller settings to their defaults (Look menu row)");
  gpad_quick_grenade = rb("gpad_quick_grenade", true, ARCHIVE,
      "CoD1 / UO: a grenade button throws in one press like the console (0: switch to the grenade, fire to throw)");
  gpad_aim_assist = rb("gpad_aim_assist", true, ARCHIVE, "Enable controller aim assist (slowdown, lock on, auto aim)");
  gpad_invert = rb("gpad_invert", false, ARCHIVE, "Invert controller look pitch");
  gpad_ps_glyphs = rb("gpad_ps_glyphs", false, ARCHIVE, "Button icons: PlayStation 3 instead of Xbox 360");
  gpad_console_hud = rb("gpad_console_hud", true, ARCHIVE, "Console style HUD: safe area margins, d-pad items in a cross");
  gpad_hud_safe_area = rf("gpad_hud_safe_area", 0.9f, 0.5f, 1.0f, ARCHIVE, "Console style HUD: safe area (1 = screen edges)");
  gpad_sensitivity = rf("gpad_sensitivity", 1.0f, 0.1f, 5.0f, ARCHIVE, "Controller look sensitivity");
  gpad_deadzone = rf("gpad_deadzone", 0.2f, 0.0f, 0.9f, ARCHIVE, "Controller stick deadzone");
  gpad_aim_strength = rf("gpad_aim_strength", 0.75f, 0.0f, 1.0f, ARCHIVE,
      "Aim assist strength: scales auto aim snap, lock on and slowdown (1 = console)");
  if (g->aimDvarSlots) return;

  // MW3: the engine no longer registers these. Defaults are MW2's console values.
  // ponytail: weapon aim ranges are fixed here because no MW3 code reads them from the WeaponDef
  // anymore (offset unknown). Map BG_GetWeaponDef + the range fields if per-weapon ranges matter.
  gpad_aim_range = rf("gpad_aim_range", 3200.0f, 0.0f, 20000.0f, 0, "Aim assist range when the weapon range is unknown");
  gpad_autoaim_range = rf("gpad_autoaim_range", 1200.0f, 0.0f, 20000.0f, 0, "Auto aim range when the weapon range is unknown");
  struct { AimDvar d; const char* name; float def, max; bool isBool; } defs[] = {
    {RANGE_SCALE, "aim_aimAssistRangeScale", 1, 2, false}, {AUTO_RANGE_SCALE, "aim_autoAimRangeScale", 1, 2, false},
    {RATE_PITCH, "aim_turnrate_pitch", 90, 1080, false}, {RATE_PITCH_ADS, "aim_turnrate_pitch_ads", 55, 1080, false},
    {RATE_YAW, "aim_turnrate_yaw", 260, 1080, false}, {RATE_YAW_ADS, "aim_turnrate_yaw_ads", 90, 1080, false},
    {ACCEL_ENABLED, "aim_accel_turnrate_enabled", 1, 1, true}, {ACCEL_LERP, "aim_accel_turnrate_lerp", 1200, 4000, false},
    {SLOW_ENABLED, "aim_slowdown_enabled", 1, 1, true},
    {SLOW_PITCH, "aim_slowdown_pitch_scale", 0.4f, 1, false}, {SLOW_PITCH_ADS, "aim_slowdown_pitch_scale_ads", 0.5f, 1, false},
    {SLOW_YAW, "aim_slowdown_yaw_scale", 0.4f, 1, false}, {SLOW_YAW_ADS, "aim_slowdown_yaw_scale_ads", 0.5f, 1, false},
    {AUTO_ENABLED, "aim_autoaim_enabled", 1, 1, true}, {AUTO_LERP, "aim_autoaim_lerp", 40, 100, false},
    {LOCK_ENABLED, "aim_lockon_enabled", 1, 1, true}, {LOCK_DEFLECTION, "aim_lockon_deflection", 0.05f, 1, false},
    {LOCK_STRENGTH, "aim_lockon_strength", 0.6f, 1, false}, {SCALE_VIEW_AXIS, "aim_scale_view_axis", 1, 1, true},
  };
  for (auto& d : defs)
    own_aim_dvars[d.d] = d.isBool ? rb(d.name, d.def != 0, ARCHIVE, "Controller aim assist")
                                  : rf(d.name, d.def, 0, d.max, ARCHIVE, "Controller aim assist");
}

// --- XInput
using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetState_t xinput_get_state;
struct Pad { bool connected; WORD buttons; float lx, ly, rx, ry, lt, rt; } pad;

void stick(SHORT sx, SHORT sy, float dz, float& ox, float& oy) {
  float x = sx / 32767.0f, y = sy / 32767.0f, len = std::sqrt(x * x + y * y);
  if (len <= dz) { ox = oy = 0; return; }
  float scaled = std::fmin(1.0f, (len - dz) / (1.0f - dz)) / len;
  ox = x * scaled; oy = y * scaled;
}

// PlayStation pads under Proton without Steam Input show up only as generic joysticks, not XInput. Read
// those through WinMM with the DualSense / DualShock 4 HID layout: X Y = left stick, Z Rz = right stick,
// WinMM U V = R2 L2 (tested on a DualSense, 2026-10-05), buttons Square Cross Circle Triangle L1 R1 L2 R2 Create
// Options L3 R3, POV = d-pad.
bool winmm_state(XINPUT_STATE& st, bool scan) {
  static UINT joy = 0;
  JOYINFOEX ji{sizeof ji, JOY_RETURNALL | JOY_RETURNPOVCTS};
  if (joyGetPosEx(joy, &ji) != JOYERR_NOERROR) {
    if (!scan) return false;
    UINT i = 0, n = joyGetNumDevs();
    JOYCAPSW caps{};
    for (; i < n && i < 16; i++)
      if (joyGetDevCapsW(i, &caps, sizeof caps) == JOYERR_NOERROR && caps.wNumAxes >= 4 && joyGetPosEx(i, &ji) == JOYERR_NOERROR) break;
    if (i >= n || i >= 16) return false;
    joy = i;
    logmsg("joystick %u via WinMM: %ls, %u axes, %u buttons", i, caps.szPname, caps.wNumAxes, caps.wNumButtons);
  }
  auto axis = [](DWORD v) { int s = int(v) - 32768; return SHORT(s > 32767 ? 32767 : s < -32767 ? -32767 : s); };
  auto& gp = st.Gamepad;
  gp.sThumbLX = axis(ji.dwXpos); gp.sThumbLY = axis(65535 - ji.dwYpos);
  gp.sThumbRX = axis(ji.dwZpos); gp.sThumbRY = axis(65535 - ji.dwRpos);
  gp.bLeftTrigger = BYTE(ji.dwVpos >> 8); gp.bRightTrigger = BYTE(ji.dwUpos >> 8);
  constexpr WORD kButtons[] = { XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB };
  gp.wButtons = 0;
  for (int b = 0; b < int(std::size(kButtons)); b++)
    if (ji.dwButtons & (1u << b)) gp.wButtons |= kButtons[b];
  if (DWORD d = ji.dwPOV; d != JOY_POVCENTERED) {
    if (d <= 4500 || d >= 31500) gp.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
    if (d >= 4500 && d <= 13500) gp.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    if (d >= 13500 && d <= 22500) gp.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
    if (d >= 22500 && d <= 31500) gp.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
  }
  return true;
}

void poll_pad() {
  // Use the first slot with a pad: under Proton, Steam Input's virtual pad isn't always slot 0. Empty slots
  // are slow to query on Windows, so while nothing is connected rescan only once a second.
  static DWORD slot = 0, nextScan = 0;
  XINPUT_STATE st{};
  pad.connected = xinput_get_state && xinput_get_state(slot, &st) == ERROR_SUCCESS;
  bool scan = !pad.connected && GetTickCount() >= nextScan;
  if (scan) {
    nextScan = GetTickCount() + 1000;
    for (DWORD i = 0; xinput_get_state && i < XUSER_MAX_COUNT && !pad.connected; i++)
      if (i != slot && xinput_get_state(i, &st) == ERROR_SUCCESS) { slot = i; pad.connected = true; logmsg("pad in slot %lu", i); }
  }
  if (!pad.connected) pad.connected = winmm_state(st, scan);
  if (scan && !pad.connected) {
    static bool told;
    if (!told) { told = true; logmsg("no XInput pad in any slot or WinMM joystick"); }
  }
  if (!pad.connected) { pad = {}; return; }
  float dz = ownf(gpad_deadzone, 0.2f);
  auto& gp = st.Gamepad;
  pad.buttons = gp.wButtons;
  stick(gp.sThumbLX, gp.sThumbLY, dz, pad.lx, pad.ly);
  stick(gp.sThumbRX, gp.sThumbRY, dz, pad.rx, pad.ry);
  pad.lt = gp.bLeftTrigger / 255.0f; pad.rt = gp.bRightTrigger / 255.0f;
  // first full press of each trigger, to tell which physical trigger arrives as which
  static bool seenL, seenR;
  if (!seenL && gp.bLeftTrigger > 200) { seenL = true; logmsg("left trigger pressed (raw L %u R %u)", gp.bLeftTrigger, gp.bRightTrigger); }
  if (!seenR && gp.bRightTrigger > 200) { seenR = true; logmsg("right trigger pressed (raw L %u R %u)", gp.bLeftTrigger, gp.bRightTrigger); }
}

// --- buttons -> key events
// In gameplay buttons send AUX keys so they go through the normal bind system;
// in menus they send the keys Menu_HandleKey understands.
enum Btn { A, B, X, Y, LB, RB, LT, RT, L3, R3, DUP, DDOWN, DLEFT, DRIGHT, BACK, START, LUP, LDOWN, LLEFT, LRIGHT, BTN_COUNT };
// Console keynums (IW4x Mapping/Key.hpp) and their key names, per Btn up to START. In gameplay buttons send
// these, so binds read "bind BUTTON_A ..." and scripts' buttonpressed("BUTTON_LTRIG") works.
constexpr int kPadKey[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x12, 0x13, 0x10, 0x11, 0x14, 0x15, 0x16, 0x17, 0x0F, 0x0E };
constexpr const char* kPadKeyName[] = {
  "BUTTON_A", "BUTTON_B", "BUTTON_X", "BUTTON_Y", "BUTTON_LSHLDR", "BUTTON_RSHLDR", "BUTTON_LTRIG", "BUTTON_RTRIG",
  "BUTTON_LSTICK", "BUTTON_RSTICK", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "BUTTON_BACK", "BUTTON_START",
};
static_assert(std::size(kPadKey) == START + 1 && std::size(kPadKeyName) == START + 1);
constexpr const char* kDefaultBinds[] = {
  "+gostand", "+stance", "+usereload", "weapnext", "+smoke", "+frag", "+speed_throw", "+attack",
  "+breath_sprint", "+melee", "+actionslot 1", "+actionslot 2", "+actionslot 3", "+actionslot 4", "+scores",
};
static_assert(std::size(kCod2Binds) == std::size(kDefaultBinds));

// IW keeps real gamepad keys (kPadKey). Quake 3 engines only have AUX1-16, renamed to the button names
int k_aux1() { return g->q3 ? g->q3->aux1 : 207; }
int pad_key(int b) { return g->q3 ? k_aux1() + b : kPadKey[b]; }

bool btn_down(int b) {
  constexpr WORD xb[] = { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0, XINPUT_GAMEPAD_LEFT_THUMB,
    XINPUT_GAMEPAD_RIGHT_THUMB, XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
    XINPUT_GAMEPAD_DPAD_RIGHT, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START };
  switch (b) {
    case LT: return pad.lt > 0.25f;
    case RT: return pad.rt > 0.25f;
    case LUP: return pad.ly > 0.5f;
    case LDOWN: return pad.ly < -0.5f;
    case LLEFT: return pad.lx < -0.5f;
    case LRIGHT: return pad.lx > 0.5f;
    default: return (pad.buttons & xb[b]) != 0;
  }
}

int menu_key(int b) {
  switch (b) {
    case A: return K_ENTER;
    case B: case START: return K_ESCAPE;
    case DUP: case LUP: return K_UP;
    case DDOWN: case LDOWN: return K_DOWN;
    case DLEFT: case LLEFT: return K_LEFT;
    case DRIGHT: case LRIGHT: return K_RIGHT;
    default: return 0;
  }
}

int game_key(int b) {
  if (b == START) return K_ESCAPE;
  if (b < START) return pad_key(b);
  return 0;  // left stick directions move the player instead
}

struct BtnState { int sentKey; int nextRepeat; } btn[BTN_COUNT];

int now_ms() { return at<Sys_Milliseconds_t>(g->sysMilliseconds)(); }

#ifndef _WIN64
// UO's Item_SetFocus only focuses a text item while the mouse cursor is inside it, and the menus' own up/down
// (Menu_SetNext/PrevCursorItem) pass the cursor position, so the arrow keys can't leave a parked cursor's item.
// dir 0: focus the first item if none has focus (UO pregame: one invisible full-screen button, normally clicked).
// Step through the focused menu ourselves: park the cursor on each candidate and let Item_SetFocus decide (it runs
// the item's focus sound and onFocus scripts). Returns false when no UI menu is up (the key is sent as usual).
// a game DLL address (as mapped at its preferred base) in the loaded module, nullptr while it isn't loaded
uint8_t* dll_at(const char* module, uintptr_t prefBase, uintptr_t va) {
  auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(module));
  return base ? reinterpret_cast<uint8_t*>(base + va - prefBase) : nullptr;
}
uint8_t* q3_ui(uintptr_t va) { return dll_at(g->q3->menus->module, g->q3->menus->modBase, va); }
uint8_t* q3_ui_dc() { auto p = q3_ui(g->q3->menus->dcPtr); return p ? *reinterpret_cast<uint8_t**>(p) : nullptr; }

bool q3_menu_step(int dir) {
  auto& m = *g->q3->menus;
  auto ui = q3_ui;
  auto dc = q3_ui_dc();
  if (!dc) return false;
  auto stack = reinterpret_cast<uint8_t**>(ui(m.stack));
  uint8_t* menu = nullptr;
  for (int i = *reinterpret_cast<int*>(ui(m.count)) - 1; i >= 0 && !menu; i--)
    if (stack[i] && (field<unsigned>(stack[i], m.flags) & 6) == 6) menu = stack[i];
  if (!menu) { if (dir == 0) logmsg("menu focus: no focused menu"); return false; }
  auto items = reinterpret_cast<uint8_t**>(menu + m.items);
  int n = field<int>(menu, m.itemCount), cur = field<int>(menu, m.cursorItem), focused = -1;
  for (int i = 0; i < n; i++)
    if (items[i] && (field<unsigned>(items[i], m.flags) & 2)) cur = focused = i;
  bool first = dir == 0;  // just make sure something has focus (ENTER only acts on the focused item)
  if (first) {
    if (focused >= 0) return true;
    dir = 1; cur = -1;
  }
  // park the cursor on item i and let Item_SetFocus decide
  auto try_item = [&](uint8_t* mn, int i) {
    auto it = reinterpret_cast<uint8_t**>(mn + m.items)[i];
    if (!it || (field<unsigned>(it, m.flags) & 0x12) || !(field<unsigned>(it, m.flags) & 4)) return false;
    auto r = reinterpret_cast<float*>(field<int>(it, m.type) == 0 ? it + m.textRect : it);
    float x = r[0] + r[2] * 0.5f, y = field<int>(it, m.type) == 0 ? r[1] - r[3] * 0.5f : r[1] + r[3] * 0.5f;
    if (r[2] <= 0 || r[3] <= 0) return false;  // not laid out (never drawn)
    auto cursor = reinterpret_cast<int*>(dc + m.cursorOff);
    cursor[0] = int(x); cursor[1] = int(y);
    auto& vm = *at<void**>(m.currentVm);
    void* saved = vm;
    vm = *at<void**>(m.uiVm);  // Item_SetFocus plays the focus sound through a syscall
    int ok = vm ? call_item_set_focus_uo(reinterpret_cast<uintptr_t>(ui(m.setFocus)), it, x, y) : 0;
    vm = saved;
    if (ok) field<int>(mn, m.cursorItem) = i;
    return ok != 0;
  };
  // Options: the right-hand list (options_menu) stays open, visible but unfocused, under the panel. The mouse moves
  // between them by pointing. Stepping past either end of one menu goes to the other visible one.
  uint8_t* other = nullptr;
  for (int i = *reinterpret_cast<int*>(ui(m.count)) - 1; i >= 0 && !other && !first; i--)
    if (stack[i] && stack[i] != menu && (field<unsigned>(stack[i], m.flags) & 4)) other = stack[i];
  for (int k = 1; k <= n; k++) {
    int raw = (cur < 0 ? (dir > 0 ? -1 : n) : cur) + dir * k;
    if (other && (raw < 0 || raw >= n)) {
      field<unsigned>(menu, m.flags) &= ~2u, field<unsigned>(other, m.flags) |= 2;
      if (focused >= 0) field<unsigned>(items[focused], m.flags) &= ~2u;
      int on = field<int>(other, m.itemCount);
      for (int j = 0; j < on; j++)
        if (try_item(other, dir > 0 ? j : on - 1 - j)) return true;
      field<unsigned>(menu, m.flags) |= 2, field<unsigned>(other, m.flags) &= ~2u;  // nothing focusable there
      if (focused >= 0) field<unsigned>(items[focused], m.flags) |= 2;
      other = nullptr;
    }
    if (try_item(menu, (raw % n + n) % n)) return true;
  }
  if (dir == 0 || first) logmsg("menu focus: nothing focusable (%d items)", n);
  return !first;  // up/down with nothing focusable: swallowed like the stock menus do
}

// UO's sliders only follow the mouse and left / right move the focus like up / down, so with a slider focused,
// left / right change its cvar instead (20 steps). itemDef type +0xcc (10 slider, 11 yes/no, 12 multi), cvar name
// +0x11c, typeData +0x258: editFieldDef {min, max, default} (uo_uix86 keyword handlers 0x40016b20 / 0x40016c10) or
// multiDef {labels[32], strings[32] +0x80, values[32] +0x100, count +0x180, strDef +0x184}.
// Yes/no and multi only step forward, on ENTER with the cursor on them (CoD1 uix86 0x400109a0 / 0x40010c80), and
// ENTER then runs the item's action (e.g. "show graphicsapply"), so those return 2 = send ENTER. Left on a multi
// first sets the value two back, so ENTER lands one back.
int q3_menu_adjust(int dir) {
  auto& m = *g->q3->menus;
  if (!q3_ui_dc() || !g->cvarGet || !g->cvarSet) return false;
  auto stack = reinterpret_cast<uint8_t**>(q3_ui(m.stack));
  for (int i = *reinterpret_cast<int*>(q3_ui(m.count)) - 1; i >= 0; i--) {
    auto menu = stack[i];
    if (!menu || (field<unsigned>(menu, m.flags) & 6) != 6) continue;
    auto items = reinterpret_cast<uint8_t**>(menu + m.items);
    for (int k = 0; k < field<int>(menu, m.itemCount); k++) {
      auto it = items[k];
      if (!it || !(field<unsigned>(it, m.flags) & 2)) continue;
      int type = field<int>(it, m.type);
      auto cvar = field<const char*>(it, m.itemCvar);
      auto ed = field<float*>(it, m.itemEditField);
      if (type == 11) return 2;
      if ((type != 10 && type != 12) || !cvar || !ed) return 0;
      float v = ownf(at<Cvar_Get_t>(g->cvarGet)(cvar, "0", 0), 0);
      char val[32];
      if (type == 12) {
        auto md = reinterpret_cast<uint8_t*>(ed);
        int n = field<int>(md, 0x180), cur = 0;
        // ponytail: string lists (strDef) only step forward. None of our menus' d-pad rows use one
        if (dir > 0 || n < 2 || field<int>(md, 0x184)) return 2;
        auto vals = reinterpret_cast<float*>(md + 0x100);
        for (int j = 1; j < n; j++)
          if (std::fabs(vals[j] - v) < std::fabs(vals[cur] - v)) cur = j;
        snprintf(val, sizeof val, "%g", vals[(cur - 2 + 2 * n) % n]);
        at<Cvar_Set_t>(g->cvarSet)(cvar, val, 1);
        return 2;
      }
      v = std::clamp(v + dir * (ed[1] - ed[0]) / 20, std::min(ed[0], ed[1]), std::max(ed[0], ed[1]));
      snprintf(val, sizeof val, "%g", v);
      at<Cvar_Set_t>(g->cvarSet)(cvar, val, 1);
      return true;
    }
    return false;
  }
  return false;
}
#endif

void send_key(int key, bool down) {
#ifndef _WIN64
  if (g->q3 && g->q3->menus && (key == K_UP || key == K_DOWN) && (*at<int*>(g->keyCatchers) & g->keyCatchUi)) {
    if (!down) return;
    if (q3_menu_step(key == K_DOWN ? 1 : -1)) return;
  }
  if (g->q3 && g->q3->menus && (key == K_LEFT || key == K_RIGHT) && (*at<int*>(g->keyCatchers) & g->keyCatchUi)) {
    if (!down) return;
    int r = q3_menu_adjust(key == K_RIGHT ? 1 : -1);
    if (r == 2) send_key(K_ENTER, true), send_key(K_ENTER, false);
    if (r) return;
  }

#endif
#ifndef _WIN64
  if (g->q3) return (g->q3->keyEventUo ? call_key_event_uo : call_key_event_q3)(g->keyEvent + slide, key, down, now_ms());
#endif
  at<CL_KeyEvent_t>(g->keyEvent)(0, key, down, now_ms());
}
void cbuf(const char* text) {
#ifndef _WIN64
  if (g->q3) return call_cbuf_q3(g->cbuf + slide, text);
#endif
  at<Cbuf_AddText_t>(g->cbuf)(0, text);
}

// keys[k] = { int down; int repeats; binding }: down sits 8 bytes before the binding field (all 5 builds' Key_IsDown)
void set_key_down(int k, bool d) { *at<int*>((g->keyBindIdx ? g->keyBindIdx : g->keyBindStr) - 8 + k * g->keyStride) = d; }

// Q3 menus drop focus on any mouse move away from the focused item (Menu_HandleMouseMove), and Steam Input's gyro /
// trackpad send mouse moves all the time, so a d-pad step would be undone at once. Park the cursor on the item instead.
void q3_cursor_to_focus() {
  if (!g->q3 || !g->q3->uiDc) return;
  auto dc = at<uint8_t*>(g->q3->uiDc);
  for (int m = field<int>(dc, 0x170) - 1; m >= 0; m--) {
    auto menu = field<uint8_t*>(dc, 0x130 + m * 4);
    if (!menu || (field<unsigned>(menu, 0xe8) & 6) != 6) continue;
    auto items = field<uint8_t**>(menu, 0x27c);
    int n = field<int>(menu, 0x218), focus = -1, cursor = field<int>(menu, 0x220);
    for (int i = 0; i < n; i++)
      if (items[i] && (field<unsigned>(items[i], 0xe8) & 6) == 6) focus = i;
    // Popups whose buttons use dvartest/focusdvar (skill level) move cursorItem but never set the focus flag. The
    // item takes it fine when set directly, and ENTER then runs its action.
    if (focus < 0 && cursor >= 0 && cursor < n && items[cursor] && (field<unsigned>(items[cursor], 0xe8) & 4)) {
      field<unsigned>(items[cursor], 0xe8) |= 2;
      focus = cursor;
    }
    if (focus < 0) return;
    auto r = reinterpret_cast<float*>(items[focus]);
    field<int>(dc, 0xc) = int(r[0] + r[2] * 0.5f);
    field<int>(dc, 0x10) = int(r[1] + r[3] * 0.5f);
    return;
  }
}

const char* binding_of(int k);
// CoD1 / UO tap-or-hold buttons (console style), while still on their default binds: B tap = togglecrouch, hold =
// goprone (no +stance in these games). X tap = reload, hold = use (+activate held, so "hold to swap" works).
bool q3_hold_button(int b, bool down, int now) {
  static int pressMs[BTN_COUNT] = {};
  static bool held[BTN_COUNT];
  static const char* const kHold[][4] = {  // bind, tap, hold start, hold end
    {"togglecrouch", "togglecrouch\n", "goprone\n", ""}, {"+reload", "+reload\n-reload\n", "+activate\n", "-activate\n"}};
  if (!g->q3 || (b != B && b != X)) return false;
  auto& h = kHold[b == X];
  auto bind = binding_of(pad_key(b));
  if (!bind || strcmp(bind, h[0])) return false;
  if (down && !pressMs[b]) pressMs[b] = now ? now : 1, held[b] = false;
  else if (down && !held[b] && now - pressMs[b] >= 300) cbuf(h[2]), held[b] = true;
  else if (!down && pressMs[b]) {
    cbuf(held[b] ? h[3] : h[1]);
    pressMs[b] = 0;
  }
  return true;
}

// UO quick grenade (gpad_quick_grenade): a button bound to "weaponslot grenade" / "weaponslot smokegrenade" throws
// like CoD2's offhand. Switch to the grenade, hold fire while the button is down (released early: throw once it's
// up), then switch back to the weapon slot from before. UO weapon defs: game DLL pointer 0x20151f34, weaponSlot +0x84
// (1 primary, 2 primaryb, 3 pistol, 4 grenade, 5 smokegrenade). playerState weapon +0xc8, weaponstate +0xcc (0 ready).
#ifdef _WIN64
bool q3_quick_grenade(int, bool, int) { return false; }
#else
bool q3_quick_grenade(int b, bool down, int now) {
  static struct { int step, btn, slot, back, since; bool released; } q;
  if (!g->q3 || !g->q3->gameDll || !ownb(gpad_quick_grenade, true)) return q.step = 0, false;
  auto bind = b < START ? binding_of(pad_key(b)) : nullptr;  // stick directions have no key
  int slot = !bind ? 0 : !strcmp(bind, "weaponslot grenade") ? 4 : !strcmp(bind, "weaponslot smokegrenade") ? 5 : 0;
  if (q.step && b != q.btn) return slot != 0;  // one throw at a time
  if (!q.step && !(slot && down)) return slot != 0;
  auto defs = g->q3->gameWeaponDefs ? dll_at(g->q3->gameDll, g->q3->gameBase, g->q3->gameWeaponDefs) : nullptr;
  auto ps = at<uint8_t*>(g->q3->ps);
  int weapon = field<int>(ps, g->q3->psWeapon), wstate = field<int>(ps, g->q3->psWeaponState);
  auto def = defs && weapon > 0 ? (*reinterpret_cast<uint8_t***>(defs))[weapon] : nullptr;
  int cur = def ? field<int>(def, g->q3->wdSlot) : 0;
  static const char* kSlotCmd[] = { nullptr, "weaponslot primary\n", "weaponslot primaryb\n", "weaponslot pistol\n" };
  if (!down) q.released = true;
  switch (q.step) {
    case 0:
      q = { 1, b, slot, cur >= 1 && cur <= 3 ? cur : 0, now, false };
      if (cur != slot) cbuf(slot == 4 ? "weaponslot grenade\n" : "weaponslot smokegrenade\n");
      break;
    case 1:  // switching: wait for the grenade to be up
      if (cur == q.slot && wstate == 0) cbuf("+attack\n"), q.step = 2, q.since = now;
      else if (now - q.since > 1500) q.step = 0;  // none left
      break;
    case 2:  // holding: throw on release
      if (q.released) cbuf("-attack\n"), q.step = 3, q.since = now;
      break;
    case 3:  // thrown: back to the old weapon once the throw is done (or the game switched away, out of grenades)
      if ((cur != q.slot || (wstate == 0 && now - q.since > 250)) || now - q.since > 2500) {
        if (q.back && cur != q.back) cbuf(kSlotCmd[q.back]);
        q.step = 0;
      }
      break;
  }
  return true;
}
#endif

int clickPromptMs = -100000;  // when a "Click to ..." prompt was last drawn (pad_prompt)
void update_buttons() {
  int catchers = *at<int*>(g->keyCatchers);
  bool console = catchers & KEYCATCH_CONSOLE, menu = catchers & g->keyCatchUi;
  int now = now_ms();
  for (int b = 0; b < BTN_COUNT; b++) {
    auto& s = btn[b];
    bool down = !console && ownb(gpad_enabled, true) && btn_down(b);
    if (!menu && !s.sentKey && (q3_quick_grenade(b, down, now) || q3_hold_button(b, down, now))) continue;
    int want = down ? (menu ? menu_key(b) : game_key(b)) : 0;
    // a "Click to ..." screen (CoD2 pregame: one full-screen button) takes a click anywhere
    // a video playing: A skips it like a key press (the pad keys are >= 128, which the skip ignores)
    if (want && g->q3 && b == A && !menu && !s.sentKey && ((g->q3->cinematic && *at<uint8_t*>(g->q3->cinematic)) ||
        *at<int*>(g->q3->clsState) == g->q3->videoStates[0] || *at<int*>(g->q3->clsState) == g->q3->videoStates[1])) want = K_ESCAPE;
    if (want && b == A && s.sentKey == K_ESCAPE) want = K_ESCAPE;
    // and the press stays that click until released (else a held button turns into jump once the mission starts)
    if (want && g->q3 && b == A && (s.sentKey == K_MOUSE1 || (menu && now - clickPromptMs < 250))) want = K_MOUSE1;
#ifndef _WIN64
    // UO: ENTER / MOUSE1 act on the focused item only, so focus one first. The pregame menu ("Click to Start the
    // Mission", active menu 7) only gets MOUSE1 from CL_KeyEvent, and with nothing focusable A clicks too.
    if (want == K_ENTER && menu && g->q3 && g->q3->menus && !s.sentKey) {
      auto active = q3_ui(g->q3->menus->activeMenu);
      if (!q3_menu_step(0) || (active && *reinterpret_cast<int*>(active) == g->q3->menus->clickOnlyMenu)) want = K_MOUSE1;
    }
#endif
    if (s.sentKey && s.sentKey != want) {
      send_key(s.sentKey, false);
      if (b == LT || b == RT) set_key_down(b == LT ? K_MOUSE1 : K_MOUSE2, false);
      s.sentKey = 0;
    }
    if (!want) continue;
    if (!s.sentKey) {
      // CoD1 / UO console feel: sprint from crouch / prone stands up first (pm_flags 1 prone, 2 crouched)
      if (!menu && g->q3 && b < START && (field<int>(at<uint8_t*>(g->q3->ps), 0xc) & 3))
        if (auto bind = binding_of(want); bind && !strcmp(bind, "+sprint")) cbuf("+gostand\n-gostand\n");
      send_key(want, true); s.sentKey = want; s.nextRepeat = now + 400;
      if (menu && g->q3) q3_cursor_to_focus();
      // PC scripts read the mouse buttons raw (MW2 _climb.gsc ice picks, af_chase knife crawl: mouse1 = left
      // arm, mouse2 = right arm), so mirror LT/RT into mouse1/mouse2 without firing the mouse binds.
      if (b == LT || b == RT) set_key_down(b == LT ? K_MOUSE1 : K_MOUSE2, true);
    } else if (menu && (want == K_UP || want == K_DOWN || want == K_LEFT || want == K_RIGHT) && now >= s.nextRepeat) {
      send_key(want, false); send_key(want, true); s.nextRepeat = now + 120;
      q3_cursor_to_focus();
    }
  }
}

// "Pad in use": any pad input sets it, a keyboard key or mouse button clears it. Mouse movement doesn't,
// since Deck gyro/trackpads arrive as mouse movement. GetKeyboardState only sees input sent to this
// thread's window (we run on the game's main thread), so typing in other windows doesn't count.
bool pad_in_use;
void update_in_use() {
  bool enabled = pad.connected && ownb(gpad_enabled, true);
  bool padActive = enabled && (pad.buttons || pad.lt > 0.25f || pad.rt > 0.25f || pad.lx || pad.ly || pad.rx || pad.ry);
  // only new presses count, so a key left stuck down (alt-tab, overlay) doesn't keep clearing it
  static BYTE prev[256];
  BYTE ks[256];
  bool kbm = false;
  if (GetKeyboardState(ks)) {
    for (int vk = 1; vk < 256; vk++) kbm |= (ks[vk] & 0x80) && !(prev[vk] & 0x80) && vk != VK_PACKET;
    memcpy(prev, ks, sizeof prev);
  }
  bool inUse = padActive || (pad_in_use && !kbm && enabled);
  if (inUse == pad_in_use) return;
  pad_in_use = inUse;
  set_bool(gpad_in_use, inUse);
  logmsg(inUse ? "input: controller" : "input: keyboard/mouse");
}

// keys[k] holds an index into the bind command table. MW2 also allows free-form bind strings when
// the index is 0. MW3 only has the fixed table.
const char* binding_of(int k) {
  int idx = g->keyBindIdx ? *at<int*>(g->keyBindIdx + k * g->keyStride) : 0;
  if (idx) return at<const char**>(g->bindCommands)[idx];
  return g->keyBindStr ? *at<const char**>(g->keyBindStr + k * g->keyStride) : nullptr;
}

// Bind the pad keys once if the player has none bound (config.cfg starts with unbindall).
// The game then saves them to config.cfg, so player rebinds stick. Binds from older versions (AUX keys)
// no longer parse and are dropped.
bool binds_checked;
void ensure_binds() {
  if (binds_checked) return;
  binds_checked = true;
  for (int b = 0; b <= START; b++)
    if (binding_of(pad_key(b))) return;
  char cmd[64];
  for (int i = 0; i < int(std::size(kDefaultBinds)); i++) {
    snprintf(cmd, sizeof cmd, "bind %s \"%s\"\n", kPadKeyName[i], g->q3 ? g->q3->binds[i] : kDefaultBinds[i]);
    cbuf(cmd);
  }
  logmsg("default controller binds applied");
}

// --- aim assist (ported from iw3sp_mod Gamepad.cpp / IW4x Controller/Engine/View.cpp)
float lerp(float a, float b, float t) { return a + (b - a) * t; }
float angle_norm360(float a) { a = std::fmod(a, 360.0f); return a < 0 ? a + 360.0f : a; }
float angle_sub(float a, float b) { float d = std::fmod(a - b + 180.0f, 360.0f); return (d < 0 ? d + 360.0f : d) - 180.0f; }
float diff_track(float tgt, float cur, float rate, float dt) {
  float d = tgt - cur, step = rate * d * dt;
  if (std::fabs(d) <= 0.001f) return tgt;
  return cur + (std::fabs(step) > std::fabs(d) ? d : step);
}
float diff_track_angle(float tgt, float cur, float rate, float dt) {
  while (tgt - cur > 180.0f) tgt -= 360.0f;
  while (tgt - cur < -180.0f) tgt += 360.0f;
  return angle_norm360(diff_track(tgt, cur, rate, dt));
}
float linear_track(float tgt, float cur, float rate, float dt) {
  float err = tgt - cur, step = (err <= 0 ? -rate : rate) * dt;
  if (std::fabs(err) <= 0.001f || std::fabs(step) > std::fabs(err)) return tgt;
  return cur + step;
}
void vectoangles(const float* v, float& pitch, float& yaw) {
  if (v[0] == 0 && v[1] == 0) { yaw = 0; pitch = v[2] > 0 ? 270.0f : 90.0f; return; }
  yaw = angle_norm360(std::atan2(v[1], v[0]) * 57.29578f);
  pitch = angle_norm360(-std::atan2(v[2], std::sqrt(v[0] * v[0] + v[1] * v[1])) * 57.29578f);
}

// GraphFloat { char name[64]; float knots[32][2]; uint16 knotCount; float scale; }, stride 0x148
float graph_eval(int index, float x) {
  // ponytail: no graph files outside IW (Quake 3 engines): squared response. Load a console graph if it feels off
  if (!g->aaGraphs) return x;
  auto gr = at<uint8_t*>(g->aaGraphs) + index * 0x148;
  auto knots = reinterpret_cast<float(*)[2]>(gr + 0x40);
  int n = *reinterpret_cast<uint16_t*>(gr + 0x140);
  float scale = *reinterpret_cast<float*>(gr + 0x144);
  if (n < 2) return 1.0f;  // graph file not loaded: linear response
  for (int i = 1; i < n; i++)
    if (x <= knots[i][0]) {
      float span = knots[i][0] - knots[i - 1][0];
      float t = span > 0 ? (x - knots[i - 1][0]) / span : 1.0f;
      return lerp(knots[i - 1][1], knots[i][1], t) * scale;
    }
  return knots[n - 1][1] * scale;
}

bool in_center_box(const AimScreenTarget& t, float w, float h) {
  return w >= t.clipMins[0] && t.clipMaxs[0] >= -w && h >= t.clipMins[1] && t.clipMaxs[1] >= -h;
}
const AimScreenTarget* best_target(const Aim& aa, float range, float w, float h) {
  for (int i = 0; i < aa.targetCount(); i++)
    if (aa.target(i).distSqr <= range * range && in_center_box(aa.target(i), w, h)) return &aa.target(i);
  return nullptr;
}
const AimScreenTarget* target_by_ent(const Aim& aa, int ent) {
  if (ent == AIM_TARGET_INVALID) return nullptr;
  for (int i = 0; i < aa.targetCount(); i++)
    if (aa.target(i).entIndex == ent) return &aa.target(i);
  return nullptr;
}

uint8_t* weapon_def(int weapon) {
  if (g->q3) return weapon > 0 && g->q3->weaponDefs ? at<uint8_t**>(g->q3->weaponDefs)[weapon] : nullptr;
  return at<BG_GetWeaponDef_t>(g->getWeaponDef)(weapon);
}

// WeaponDef range fields, as read by AimAssist_DrawDebugOverlay. CoD2 has the same three floats (autoAimRange,
// slowdownAimRange, slowdownAimRangeAds), console leftovers no PC code reads.
float assist_range(const Aim& aa) {
  float scale = dvf(RANGE_SCALE, 1.0f);
  if (!g->getWeaponDef && (!g->q3 || !g->q3->weaponDefs)) return ownf(gpad_aim_range, 3200.0f) * scale;  // UO: defs unmapped
  auto wd = weapon_def(aa.weapon());
  if (!wd) return 0;
  auto r = reinterpret_cast<float*>(wd + g->wdAutoAimRange);
  return lerp(r[1], r[2], aa.adsLerp()) * scale;
}
float auto_aim_range(const Aim& aa) {
  float scale = dvf(AUTO_RANGE_SCALE, 1.0f);
  if (!g->getWeaponDef && !g->q3) return ownf(gpad_autoaim_range, 1200.0f) * scale;
  auto wd = weapon_def(aa.weapon());
  float r = wd ? *reinterpret_cast<float*>(wd + g->wdAutoAimRange) : 0;
  if (g->q3 && !g->q3->weaponDefs) return ownf(gpad_aim_range, 3200.0f) * scale;  // UO: defs unmapped, fights at 1300+
  if (g->q3 && r <= 0) r = ownf(gpad_autoaim_range, 1200.0f);  // CoD2 weapons leave autoAimRange at 0
  return r * scale;
}

bool assist_allowed(const Aim& aa) {
  return aa.initialized() && ownb(gpad_aim_assist, true) && aa.weapon() != 0 && aa.hasAmmo() &&
         !(aa.weaponState() >= 0x1a && aa.weaponState() <= 0x1c);  // stunned
}
bool using_offhand(const Aim& aa) { return (aa.weapFlags() & 2) != 0; }

struct { int ent = AIM_TARGET_INVALID; bool pressed, active; float pitch, yaw; } autoaim;

void apply_look(const Aim& aa, int buttons, float dt, float& pitch, float& yaw) {
  float px = pad.ry, yx = pad.rx;
  if (dvb(GRAPH_ENABLED, true)) {
    int gi = dvi(GRAPH_INDEX, 3);
    float defl = std::fmin(1.0f, std::sqrt(px * px + yx * yx));
    if (gi >= 0 && gi < 4) { float s = graph_eval(gi, defl); px *= s; yx *= s; }
  }
  if (dvb(SCALE_VIEW_AXIS, true)) {
    float ap = std::fabs(px), ay = std::fabs(yx);
    if (ap <= ay) px *= 1.0f - (ay - ap); else yx *= 1.0f - (ap - ay);
  }

  bool assist = assist_allowed(aa);
  float slowP = 1, slowY = 1;
  bool slowed = assist && dvb(SLOW_ENABLED, true) && best_target(aa, assist_range(aa), aa.slowW(), aa.slowH());
  if (slowed) {
    float k = ownf(gpad_aim_strength, 0.75f);  // weaker slowdown = scale closer to 1
    slowY = 1.0f - k * (1.0f - lerp(dvf(SLOW_YAW, 0.4f), dvf(SLOW_YAW_ADS, 0.5f), aa.adsLerp()));
    slowP = using_offhand(aa) ? 1.0f : 1.0f - k * (1.0f - lerp(dvf(SLOW_PITCH, 0.4f), dvf(SLOW_PITCH_ADS, 0.5f), aa.adsLerp()));
  }
  if (aa.autoMeleeState() == 2) px = yx = 0;  // auto melee is steering

  float fov = aa.initialized() ? aa.fovTurnRateScale() : 1.0f, sens = ownf(gpad_sensitivity, 1.0f);
  float rateP = lerp(dvf(RATE_PITCH, 90), dvf(RATE_PITCH_ADS, 55), aa.adsLerp()) * fov * sens * slowP;
  float rateY = lerp(dvf(RATE_YAW, 260), dvf(RATE_YAW_ADS, 90), aa.adsLerp()) * fov * sens * slowY;
  float dP = std::fabs(px) * rateP, dY = std::fabs(yx) * rateY;
  float& pd = aa.pitchDelta();
  float& yd = aa.yawDelta();
  if (dvb(ACCEL_ENABLED, true)) {
    float accel = dvf(ACCEL_LERP, 1200) * sens;
    pd = dP <= pd ? dP : linear_track(dP, pd, accel, dt);
    yd = dY <= yd ? dY : linear_track(dY, yd, accel, dt);
  } else {
    pd = dP; yd = dY;
  }
  float invert = ownb(gpad_invert, false) ? -1.0f : 1.0f;
  pitch -= pd * dt * (px >= 0 ? 1.0f : -1.0f) * invert;  // stick up looks up (pitch decreases)
  yaw -= yd * dt * (yx >= 0 ? 1.0f : -1.0f);              // stick right turns right (yaw decreases)

  if (!assist) return;

  // auto aim: snap toward a target when ADS starts (single player console behaviour)
  bool reloading = aa.weaponState() >= 8 && aa.weaponState() <= 0xc;
  if (dvb(AUTO_ENABLED, true) && (g->q3 || (buttons & CMD_BUTTON_ADS)) && aa.adsLerp() > 0 && !reloading) {
    if (!autoaim.pressed) {
      if (auto t = best_target(aa, auto_aim_range(aa), aa.autoW(), aa.autoH())) {
        autoaim.ent = t->entIndex; autoaim.active = true;
        autoaim.pitch = aa.viewAngles()[0]; autoaim.yaw = aa.viewAngles()[1];
      }
      autoaim.pressed = true;
    }
    if (autoaim.active) {
      if (auto t = target_by_ent(aa, autoaim.ent)) {
        auto o = aa.viewOrigin();
        float dir[3] = { t->aimPos[0] - o[0], t->aimPos[1] - o[1], t->aimPos[2] - o[2] }, tp, ty;
        vectoangles(dir, tp, ty);
        float rate = dvf(AUTO_LERP, 40) * ownf(gpad_aim_strength, 0.75f);
        float np = diff_track_angle(tp, autoaim.pitch, rate, dt);
        float ny = diff_track_angle(ty, autoaim.yaw, rate, dt);
        pitch += angle_sub(np, autoaim.pitch); yaw += angle_sub(ny, autoaim.yaw);
        autoaim.pitch = np; autoaim.yaw = ny;
      } else {
        autoaim.active = false; autoaim.ent = AIM_TARGET_INVALID;
      }
    }
  } else {
    autoaim.active = autoaim.pressed = false; autoaim.ent = AIM_TARGET_INVALID;
  }

  // lock on: follow a target's movement while the player is steering
  int prev = aa.lockOnTarget();
  aa.lockOnTarget() = AIM_TARGET_INVALID;
  if (!dvb(LOCK_ENABLED, true) || using_offhand(aa) || autoaim.active || aa.autoMeleeState() == 2) return;
  float defl = dvf(LOCK_DEFLECTION, 0.05f);
  if (defl > std::fabs(pad.ry) && defl > std::fabs(pad.rx) && defl > std::fabs(pad.lx)) return;
  float range = assist_range(aa);
  auto t = target_by_ent(aa, prev);
  if (!(t && range * range > t->distSqr && in_center_box(*t, aa.lockW(), aa.lockH())))
    t = best_target(aa, range, aa.lockW(), aa.lockH());
  if (!t || t->distSqr <= 0) return;
  aa.lockOnTarget() = t->entIndex;
  auto dot = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
  float arc = std::sqrt(t->distSqr) * 3.14159265f, strength = dvf(LOCK_STRENGTH, 0.6f) * ownf(gpad_aim_strength, 0.75f);
  float rp = (dot(t->velocity, aa.viewAxis(2)) - dot(aa.velocity(), aa.viewAxis(2))) / arc * 180.0f * strength;
  float ry = (dot(t->velocity, aa.viewAxis(1)) - dot(aa.velocity(), aa.viewAxis(1))) / arc * 180.0f * strength;
  pitch -= rp * dt;
  yaw += ry * dt;
}

// --- glyphs
// Key_KeynumToString(key, translate=1) returns these from the localized key name table. The hint code
// falls back to the raw text when it's not a localization key. They start as coloured button names. Where the
// build supports text icons, make_glyphs() rewrites them in place into icons.
char buttonText[START + 1][16] = {
  "^2A^7", "^1B^7", "^5X^7", "^3Y^7", "LB", "RB", "LT", "RT", "L3", "R3",
  "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Back", "Start",
};

// IW4 text draws "^" 0x01 w h <Material*> as an inline icon. w/h are (size-16)/32 font heights, so 0x30 = one line.
//   x86 (depot exe): R_TextWidth 0x5080f0 skips 7 bytes, the draw 0x5377d0 reads the 4-byte pointer at +3.
//   MW2 x64: the width code (0x140009c90) skips 11 bytes, the draw (inlined in 0x1400433b0) reads 8 bytes at +3.
// The SP zones don't ship the console button materials, so each glyph is a copy of the stock "white" material whose
// image holds our own texture (glyphs/glyphs.bin: Xbox 360 and PS3 sets, gpad_ps_glyphs picks one). Strings can't hold a 0 byte, so the
// copies live below 4 GB at an address whose bytes are all nonzero. An x64 pointer always has zero high bytes, so
// there the string holds the low 4 bytes plus a 01 01 01 01 marker (never a real pointer's high half), and a stub at
// the draw's pointer read (Game::iconRead) keeps only the low half when it sees the marker.
const unsigned char kGlyphPixels[] = {
#embed "../glyphs/glyphs.bin"
};
constexpr int kGlyphSets = 2;  // 0: Xbox 360, 1: PS3 (glyphs/gen.py)
constexpr int kGlyphCount = START + 2, GLYPH_DPAD = START + 1;  // per set: the buttons, then the whole d-pad
struct Glyph { char w, h; uint8_t* material; };  // w/h: text units, the draw centers the icon on the line
Glyph glyphs[kGlyphSets][kGlyphCount];
char dpadText[16];  // the whole d-pad glyph (console_hud), written with buttonText by show_glyph_set
constexpr int ASSET_TYPE_MATERIAL = 5;
// Material / MaterialTextureDef / GfxImage (checked at runtime on "white" before anything is copied). IW4: IW4x's
// structs. IW5 (OpenAssetTools IW5_Assets.h) has stateBitsEntry[54] instead of 48 and a subMaterials pointer at the end.
// x64 is the same with 8-byte pointers. TextureDef and GfxImage are the same in both.
constexpr int P = sizeof(void*);
struct MatLayout { int size, texCount, texTable; };
constexpr MatLayout kIw4Mat = P == 8 ? MatLayout{0x78, 0x50, 0x60} : MatLayout{0x60, 0x48, 0x54};
constexpr MatLayout kIw5Mat = P == 8 ? MatLayout{0x88, 0x56, 0x68} : MatLayout{0x68, 0x4E, 0x58};
constexpr int kTexDefSize = P == 8 ? 0x10 : 0xC, kTexDefImage = 8;
constexpr int kImgSize = P == 8 ? 0x28 : 0x20, kImgWidth = P == 8 ? 0x18 : 0x14, kImgName = P == 8 ? 0x20 : 0x1C;

bool readable(const void* p, size_t n) {
  MEMORY_BASIC_INFORMATION mbi;
  return p && VirtualQuery(p, &mbi, sizeof mbi) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
         static_cast<const uint8_t*>(p) + n <= static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
}

void log_hex(const char* what, const void* p, int n) {  // for layout checks that fail on a build
  if (!readable(p, n)) { logmsg("%s %p: unreadable", what, p); return; }
  for (int o = 0; o < n; o += 16) {
    char line[64], *q = line;
    for (int i = 0; i < 16 && o + i < n; i++) q += sprintf(q, "%02x ", static_cast<const uint8_t*>(p)[o + i]);
    logmsg("%s +%02x: %s", what, o, line);
  }
}

bool in_d3d9(void* obj) {  // obj's vtable lies in d3d9.dll (DXVK's included), i.e. it's a live D3D object
  HMODULE m; wchar_t path[MAX_PATH];
  if (!obj || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  *reinterpret_cast<LPCWSTR*>(obj), &m)) return false;
  GetModuleFileNameW(m, path, MAX_PATH);
  return wcsstr(_wcslwr(path), L"d3d9");
}

void* near_jump(uintptr_t addr, void* target);

// x64: `mov rcx,[r8+3]. cvtdq2ps xmm6,xmm6` (7 bytes) -> jmp to a stub that does the same, but zero-extends the
// low half when the high half is our marker. Flags it clobbers are rewritten by the next `add` before any use.
bool patch_icon_read() {
  if (P == 4) return true;
  if (!g->iconRead) return false;
  auto p = at<uint8_t*>(g->iconRead);
  static const uint8_t kOrig[] = { 0x49, 0x8B, 0x48, 0x03, 0x0F, 0x5B, 0xF6 };
  if (memcmp(p, kOrig, sizeof kOrig)) { logmsg("glyphs: icon read does not match"); return false; }
  auto stub = static_cast<uint8_t*>(near_jump(uintptr_t(p), nullptr));  // a page near the exe. Its jmp is overwritten
  if (!stub) return false;
  static const uint8_t kStub[] = {
    0x49, 0x8B, 0x48, 0x03,                          // mov rcx, [r8+3]
    0x41, 0x81, 0x78, 0x07, 0x01, 0x01, 0x01, 0x01,  // cmp dword [r8+7], 0x01010101
    0x75, 0x02,                                      // jne +2
    0x89, 0xC9,                                      // mov ecx, ecx
    0x0F, 0x5B, 0xF6,                                // cvtdq2ps xmm6, xmm6
    0xFF, 0x25, 0, 0, 0, 0,                          // jmp [rip+0] -> back
  };
  memcpy(stub, kStub, sizeof kStub);
  *reinterpret_cast<uint8_t**>(stub + sizeof kStub) = p + sizeof kOrig;
  uint8_t jmp[sizeof kOrig] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90 };
  *reinterpret_cast<int32_t*>(jmp + 1) = int32_t(stub - (p + 5));
  DWORD old;
  VirtualProtect(p, sizeof jmp, PAGE_EXECUTE_READWRITE, &old);
  memcpy(p, jmp, sizeof jmp);  // ponytail: not atomic vs the render thread. A torn read needs a draw in these ns
  VirtualProtect(p, sizeof jmp, old, &old);
  FlushInstructionCache(GetCurrentProcess(), p, sizeof jmp);
  return true;
}

// MW2 x64 R_TextWidth(text, maxChars, font) measures "^\x01..." as 14 letters, so text after an icon (the weapon
// name after "Press X to pick up") moves right. This is the same function (0x140009b30) plus the icon case the
// draw and word-wrap code have. Font: pixelHeight +8, glyphCount +0xC, glyphs +0x20 ({u16 letter; .. u8 dx at +4}, 0x18).
// MW3 x64 has two such functions (0x1401b80f0 and 0x1401b8020). IW5 colour codes go up to ^; and its icons are
// named: "^" 1|2 w h len name[len].
using ReadChar_t = unsigned (*)(const char** text, int, int);  // MW2's takes 2 arguments. The 3rd is ignored there
int text_width(const char* text, int maxChars, uint8_t* font, int readArg) {
  auto readChar = at<ReadChar_t>(g->readChar);
  const char lastColor = g->iw5 ? ';' : '9';
  // Font_s {name, pixelHeight, glyphCount, material, glowMaterial, glyphs}
  constexpr int kHeight = P == 8 ? 8 : 4, kCount = P == 8 ? 0xC : 8, kGlyphs = P == 8 ? 0x20 : 0x14;
  auto glyphs = field<uint8_t*>(font, kGlyphs);
  int count = field<int>(font, kCount), lineW = 0, maxW = 0;
  if (maxChars <= 0) maxChars = 0x7fffffff;
  for (int n = 0; *text && n < maxChars;) {
    unsigned c = readChar(&text, 0, readArg);
    if (c == '\r' || c == '\n') { lineW = 0; continue; }
    if (c == '^' && *text >= '0' && *text <= lastColor) { text++; continue; }
    if (c == '^' && (*text == 1 || *text == 2)) {  // icon: same size as the draw, then skip marker, w, h, pointer/name
      int w = (text[1] - 16) * field<int>(font, kHeight) + 16;
      lineW += (w + (w >> 31 & 31)) >> 5;
      text += g->iw5 ? 4 + text[3] : 3 + P;
    } else {
      uint8_t* glyph = glyphs + 0x150;
      if (c - 0x20 < 0x60) glyph = glyphs + (c - 0x20) * 0x18;
      else for (int lo = 0x60, hi = count - 1; lo <= hi;) {
        int mid = (lo + hi) / 2;
        unsigned letter = field<uint16_t>(glyphs + mid * 0x18, 0);
        if (letter == c) { glyph = glyphs + mid * 0x18; break; }
        if (letter < c) lo = mid + 1; else hi = mid - 1;
      }
      n++;
      lineW += glyph[4];
    }
    if (lineW > maxW) maxW = lineW;
  }
  return maxW;
}

int text_width4(const char* text, int maxChars, uint8_t* font, int readArg) { return text_width(text, maxChars, font, readArg); }
int text_width3(const char* text, int maxChars, uint8_t* font) { return text_width(text, maxChars, font, 0); }

bool replace_text_width() {
  // entries as in the exes: MW2 x64 0x140009b30, MW3 x64 0x1401b80f0, 0x1401b8020
  static const uint8_t kEntries[][9] = {
    { 0x48, 0x89, 0x4C, 0x24, 0x08, 0x55, 0x56, 0x41, 0x54 },
    { 0x48, 0x89, 0x6C, 0x24, 0x20, 0x48, 0x89, 0x4C, 0x24 },
    { 0x48, 0x89, 0x6C, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24 },
    { 0x51, 0x53, 0x57, 0x33, 0xDB, 0x33, 0xFF, 0x39, 0x5C },  // MW3 x86 0x4e9350
    { 0x51, 0x53, 0x55, 0x33, 0xED, 0x33, 0xDB, 0x39, 0x6C },  // MW2 x86 AlterWare 0x508960
  };
  for (int k = 0; k < 2; k++) {
    if (!g->textWidth[k]) continue;
    auto p = at<uint8_t*>(g->textWidth[k]);
    bool known = false;
    for (auto& e : kEntries) known |= !memcmp(p, e, sizeof e);
    if (!known) { logmsg("glyphs: text width %llx does not match", (unsigned long long)g->textWidth[k]); return false; }
    void* fn = k == 0 ? reinterpret_cast<void*>(text_width4) : reinterpret_cast<void*>(text_width3);
    auto stub = static_cast<uint8_t*>(near_jump(uintptr_t(p), fn));
    if (!stub) return false;
    uint8_t jmp[5] = { 0xE9 };
    *reinterpret_cast<int32_t*>(jmp + 1) = int32_t(stub - (p + 5));
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(p, jmp, 5);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
  }
  return true;
}

// IW5 text draw: Material_RegisterHandle(name) for an icon. Our names "iwpad00".."iwpad31" are glyphs[0..31].
char glyphNames[kGlyphSets * kGlyphCount][8];
using MaterialLookup_t = uint8_t* (*)(const char* name, int);
uintptr_t iconLookupNext;  // what the hooked call called before (the game's lookup, or a host's hook of it)
uint8_t* icon_lookup(const char* name, int arg) {
  if (!strncmp(name, "iwpad", 5) && name[5] >= '0' && name[5] <= '9' && name[6] >= '0' && name[6] <= '9' && !name[7]) {
    int i = (name[5] - '0') * 10 + name[6] - '0';
    if (i < kGlyphSets * kGlyphCount && glyphs[0][i].material) return glyphs[0][i].material;
  }
  return reinterpret_cast<MaterialLookup_t>(iconLookupNext)(name, arg);
}
bool patch_call(uintptr_t site, uintptr_t expected, void* hook);
// Hooks the icon lookup call whatever it calls now: iw5-mod hooks the same call (MW3 x86), so our lookup passes the
// names it doesn't know on to iw5-mod's function instead of the game's.
bool patch_icon_lookup() {
  auto p = at<uint8_t*>(g->iconLookupCall);
  if (p[0] != 0xE8) { logmsg("glyphs: icon lookup call %llx not a call", (unsigned long long)g->iconLookupCall); return false; }
  iconLookupNext = uintptr_t(p + 5 + *reinterpret_cast<int32_t*>(p + 1));
  if (iconLookupNext != g->iconLookup + slide) logmsg("glyphs: icon lookup already hooked (%p), chaining", reinterpret_cast<void*>(iconLookupNext));
  return patch_call(g->iconLookupCall, iconLookupNext - slide, reinterpret_cast<void*>(icon_lookup));
}

void make_glyphs() {
  if (!g->textIcons) return;
  using Find_t = uint8_t* (*)(int type, const char* name, int createDefault);
  uint8_t* white = at<Find_t>(g->findAsset)(ASSET_TYPE_MATERIAL, "white", 0);
  const MatLayout& ml = g->iw5 ? kIw5Mat : kIw4Mat;
  if (!white || strcmp(field<const char*>(white, 0), "white") || field<char>(white, ml.texCount) != 1) {
    logmsg("glyphs: white material not as expected, keeping button names");
    log_hex("white material", white, ml.size + 8);
    return;
  }
  uint8_t* texDef = field<uint8_t*>(white, ml.texTable);
  uint8_t* image = readable(texDef, kTexDefSize) ? field<uint8_t*>(texDef, kTexDefImage) : nullptr;
  auto imgName = readable(image, kImgSize) ? field<const char*>(image, kImgName) : nullptr;
  if (!readable(imgName, 7) || (strcmp(imgName, "$white") && strcmp(imgName, "white"))) {  // code images: "$white"
    logmsg("glyphs: white image not as expected, keeping button names");
    log_hex("white material", white, ml.size + 8);
    log_hex("texture def", texDef, 0x20);
    log_hex("image", image, 0x40);
    return;
  }
  auto base = field<IDirect3DBaseTexture9*>(image, 0);
  if (!in_d3d9(base)) { logmsg("glyphs: white image has no D3D texture, keeping button names"); return; }
  IDirect3DDevice9* dev;
  if (FAILED(base->GetDevice(&dev))) { logmsg("glyphs: no device"); return; }
  D3DDEVICE_CREATION_PARAMETERS cp{};
  dev->GetCreationParameters(&cp);
  logmsg("glyphs: device flags 0x%lx", cp.BehaviorFlags);

  uint8_t* mem = nullptr;
  for (uintptr_t b = 0x3F3F0000; !mem && b >= 0x30300000; b -= 0x01010000)
    mem = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(b), 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  if (!mem) { logmsg("glyphs: no zero-free address, keeping button names"); dev->Release(); return; }
  if (P == 8 && !g->iw5 && uintptr_t(mem) >> 32) { logmsg("glyphs: memory above 4 GB, keeping button names"); dev->Release(); return; }
  bool drawOk = g->iw5 ? patch_icon_lookup() : patch_icon_read();
  if (!drawOk) { logmsg("glyphs: draw not patched, keeping button names"); dev->Release(); return; }
  if (!replace_text_width()) logmsg("glyphs: text width not replaced, text after an icon may sit too far right");

  int made = 0;
  const unsigned char* px = kGlyphPixels;
  for (int i = 0; i < kGlyphSets * kGlyphCount; i++) {
    Glyph& gl = glyphs[0][i];
    gl.w = char(px[0]); gl.h = char(px[1]);
    int tw = px[2], th = px[3];
    const unsigned char* bgra = px + 4;
    px = bgra + tw * th * 4;
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(dev->CreateTexture(tw, th, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr)) &&
        FAILED(dev->CreateTexture(tw, th, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr)))
      { logmsg("glyphs: CreateTexture failed"); break; }
    D3DLOCKED_RECT lr;
    if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) { tex->Release(); logmsg("glyphs: LockRect failed"); break; }
    for (int y = 0; y < th; y++) memcpy(static_cast<uint8_t*>(lr.pBits) + y * lr.Pitch, bgra + y * tw * 4, tw * 4);
    tex->UnlockRect(0);

    // one 0x100 slot per glyph at +0x1010: material, texture def, image. Address bytes 10, 10+i, base, base
    uint8_t* m = mem + 0x1010 + i * 0x100;
    uint8_t *def = m + 0xA0, *img = m + 0xB0;
    memcpy(m, white, ml.size);
    memcpy(def, texDef, kTexDefSize);
    memcpy(img, image, kImgSize);
    field<void*>(img, 0) = tex;
    field<uint16_t>(img, kImgWidth) = uint16_t(tw);
    field<uint16_t>(img, kImgWidth + 2) = uint16_t(th);
    field<uint16_t>(img, kImgWidth + 4) = 1;
    field<const char*>(img, kImgName) = i % kGlyphCount == GLYPH_DPAD ? "DPAD" : kPadKeyName[i % kGlyphCount];
    field<uint8_t*>(def, kTexDefImage) = img;
    field<uint8_t*>(m, ml.texTable) = def;
    snprintf(glyphNames[i], sizeof glyphNames[i], "iwpad%02d", i);
    field<const char*>(m, 0) = glyphNames[i];
    gl.material = m;
    made++;
  }
  dev->Release();
  logmsg("glyphs: %d button icons at %p", made, static_cast<void*>(mem));
}

// Points the localized key names at one glyph set. Called again whenever gpad_ps_glyphs changes.
void show_glyph_set(int set) {
  for (int i = 0; i < kGlyphCount; i++) {
    auto& gl = glyphs[set][i];
    if (!gl.material) return;  // make_glyphs didn't run or failed: the button names stay
    // "^7" first: icons are tinted by the current text colour, and hints colour the key (a blue X came out green)
    char* s = i == GLYPH_DPAD ? dpadText : buttonText[i];
    memcpy(s, "^7^\x01", 4);
    s[4] = gl.w; s[5] = gl.h;
    if (g->iw5) {  // by name: "iwpadNN", found by icon_lookup
      s[6] = 7;
      memcpy(s + 7, glyphNames[set * kGlyphCount + i], 8);
      continue;
    }
    uint32_t lo = uint32_t(uintptr_t(gl.material));
    memcpy(s + 6, &lo, 4);
    if (P == 8) memset(s + 10, 1, 4);
    s[6 + P] = 0;
  }
}

// The engine's key name table ({ const char* name; int keynum; } x 95) sits right before the localized one
// in all 5 builds. Nothing uses its 16 AUX1-AUX16 entries, so they become the console keys: names for
// binds/config/scripts, coloured names for hints. Key_StringToKeynum, Key_KeynumToString and Key_IsDown all
// walk these tables, so no code needs patching.
void rename_aux(const char** table, bool localized) {
  for (int i = 0; table[i * 2]; i++) {
    int& k = *reinterpret_cast<int*>(&table[i * 2 + 1]);
    if (k < k_aux1() || k > k_aux1() + START) continue;
    int b = k - k_aux1();
    DWORD old;
    VirtualProtect(&table[i * 2], 2 * sizeof(void*), PAGE_READWRITE, &old);
    table[i * 2] = localized ? buttonText[b] : kPadKeyName[b];
    k = pad_key(b);
    VirtualProtect(&table[i * 2], 2 * sizeof(void*), old, &old);
  }
}

void patch_key_names() {
  auto localized = at<const char**>(g->localizedKeyNames);
  auto names = localized - g->keyNameCount * 2;
  if (strcmp(names[0], "TAB") || strcmp(localized[0], "KEY_TAB")) { logmsg("key name table not found, pad keys unnamed"); return; }
  rename_aux(names, false);
  rename_aux(localized, true);
  if (g->keynumToString)
    logmsg("pad key names: %s", at<Key_KeynumToString_t>(g->keynumToString)(kPadKey[LT], 0));  // expect BUTTON_LTRIG
}

// Hints ("Press [{+usereload}]") show the first two keys bound to a command, in key order, so
// keyboard keys always win. While the pad is in use, prefer the controller keys.
// PC-only commands that the console layout covers with a combined command.
constexpr const char* kConsoleEquivalent[][2] = {
  {"+activate", "+usereload"}, {"+reload", "+usereload"}, {"+melee_breath", "+melee"}, {"+melee_zoom", "+melee"},
  {"+holdbreath", "+breath_sprint"}, {"+sprint", "+breath_sprint"}, {"+toggleads_throw", "+speed_throw"},
  {"toggleads", "+speed_throw"}, {"+prone", "+stance"}, {"togglecrouch", "+stance"}, {"toggleprone", "+stance"},
  {"lowerstance", "+stance"}, {"gocrouch", "+stance"}, {"goprone", "+stance"}, {"+moveup", "+gostand"},
  {"+throw", "+frag"}, {"toggleads", "+speed"}, {"raisestance", "+gostand"}, {"+breath_binoculars", "+holdbreath"},
  // CoD1 / UO: B is togglecrouch (hold: prone), Y weapnext
  {"+prone", "togglecrouch"}, {"toggleprone", "togglecrouch"}, {"goprone", "togglecrouch"}, {"gocrouch", "togglecrouch"},
  {"lowerstance", "togglecrouch"}, {"weaponslot primary", "weapnext"}, {"weaponslot primaryb", "weapnext"},
  {"weapprev", "weapnext"}, {"+activate", "+reload"},  // UO X: tap reload, hold use
};

int pad_keys_for(const char* cmd, int* keys) {
  int n = 0;
  for (int i = 0; i <= START; i++)
    if (int k = pad_key(i); n < 2 && binding_of(k) && !_stricmp(binding_of(k), cmd)) keys[n++] = k;
  return n;
}

int pad_keys_equiv(const char* cmd, int* keys) {
  if (int n = pad_keys_for(cmd, keys)) return n;
  for (auto& e : kConsoleEquivalent)
    if (!_stricmp(cmd, e[0]))
      if (int n = pad_keys_for(e[1], keys)) return n;
  return 0;
}

// Returns the controller keys for a hint command (0 when the pad isn't in use or nothing is bound).
int hint_pad_keys(const char* cmd, int* keys) {
  if (!cmd) return 0;
  return pad_in_use ? pad_keys_equiv(cmd, keys) : 0;
}

// MW2: CL_GetKeyBinding -> Key_GetCommandAssignment(client, cmd, int keys[2])
using Key_GetCommandAssignment_t = int (*)(int client, const char* cmd, int* keys);
int hk_Key_GetCommandAssignment(int client, const char* cmd, int* keys) {
  int pk[2] = {-1, -1};
  if (int n = hint_pad_keys(cmd, pk)) { keys[0] = pk[0]; keys[1] = pk[1]; return n; }
#ifdef _WIN64
  return at<Key_GetCommandAssignment_t>(g->hintFunc)(client, cmd, keys);
#else
  return (g->mw3Regs ? call_key_get_command_assignment_mw3 : call_key_get_command_assignment)(g->hintFunc + slide, client, cmd, keys);
#endif
}

// MW3: the key search is inlined into CL_GetKeyBinding(client, cmd, char out[2][0x80]), so replace it whole.
using CL_GetKeyBinding_t = int (*)(int client, const char* cmd, char* out);
[[maybe_unused]] int hk_CL_GetKeyBinding(int client, const char* cmd, char* out) {
  int pk[2] = {-1, -1};
  if (int n = hint_pad_keys(cmd, pk)) {
    out[0x80] = 0;
    for (int i = 0; i < n; i++)
      snprintf(out + i * 0x80, 0x80, "%s", at<Key_KeynumToString_t>(g->keynumToString)(pk[i], 1));
    return n;
  }
  return at<CL_GetKeyBinding_t>(g->hintFunc)(client, cmd, out);
}

#ifndef _WIN64
// Quake 3 engines have no aim assist, so build what apply_look reads: an AimAssistGlobals copy in MW2's layout
// (aaShift 0) and its screen targets. Targets are living enemy actors in line of sight, in clip units = virtual
// 640x480 pixels from the screen centre, sorted by distance from the crosshair like MW2's list.
// ponytail: regions are MW2's 90x90 slowdown/lock-on and 160x120 auto aim boxes as half extents in these units,
// unverified against console. Tune here (or add dvars) after playing.
constexpr float kSlowRegion = 45, kAutoRegionW = 80, kAutoRegionH = 60;
constexpr float kAimBelowEye = 8, kTargetHalfWidth = 16, kHeadAboveEye = 6;
constexpr int MAX_TARGETS = (0xe3c - 0x13c) / int(sizeof(AimScreenTarget)), MAX_ENTS = 2048;  // CoD2 Toujane has 1100+

Aim q3_aim(float dt) {
  alignas(16) static uint8_t buf[0xE80];
  static float lastPos[MAX_ENTS][3];
  static bool seen[MAX_ENTS];
  const Q3& q = *g->q3;
  Aim aa{buf};
  auto ps = at<uint8_t*>(q.ps);
  auto vec = [](uint8_t* p, int off) { return reinterpret_cast<float*>(p + off); };
  memcpy(vec(buf, 0x0), vec(ps, 0x20), 12);                      // velocity
  aa.f<int>(0x24) = field<int>(ps, q.psWeapon);                   // weapon
  aa.f<bool>(0x28) = true;                                        // ponytail: ammo not checked (assist on empty guns)
  aa.f<bool>(0xb0) = true;
  float ads = field<float>(ps, q.psAdsFrac);
  aa.f<float>(0x128) = ads;

  // view: player origin + view height, axis from the angles (forward, left, up)
  float* org = aa.f<float[3]>(0xe4);
  memcpy(org, vec(ps, 0x14), 12);
  org[2] += field<float>(ps, q.psViewHeight);
  float* ang = aa.f<float[3]>(0xf0);
  auto delta = reinterpret_cast<int*>(ps + q.psDeltaAngles);
  ang[0] = *at<float*>(g->viewPitch) + delta[0] * (360.0f / 65536);
  ang[1] = *at<float*>(g->viewYaw) + delta[1] * (360.0f / 65536);
  ang[2] = 0;
  float sp = std::sin(ang[0] * 0.01745329f), cp = std::cos(ang[0] * 0.01745329f);
  float sy = std::sin(ang[1] * 0.01745329f), cy = std::cos(ang[1] * 0.01745329f);
  float axis[3][3] = { {cp * cy, cp * sy, -sp}, {-sy, cy, 0}, {sp * cy, sp * sy, cp} };
  memcpy(aa.f<float[9]>(0xfc), axis, sizeof axis);

  // FOV: cg_fov, lerped to the weapon's ADS zoom
  float baseFov = q.cgFov ? *reinterpret_cast<float*>(*at<uint8_t**>(q.cgFov) + g->dvarValue) : 80.0f, fov = baseFov;
  if (q.fovX) {  // UO: the cgame's own zoomed fov_x. cg_fov through Cvar_Get (it returns the existing cvar)
    static uint8_t* cgFov;
    if (!cgFov && g->cvarGet) cgFov = at<Cvar_Get_t>(g->cvarGet)("cg_fov", "80", 0);
    if (cgFov) baseFov = fov = ownf(cgFov, 80.0f);
    if (auto f = dll_at(q.cgameDll, q.cgameBase, q.fovX); f && *reinterpret_cast<float*>(f) > 1) fov = *reinterpret_cast<float*>(f);
  } else if (auto wd = weapon_def(aa.weapon()); wd && field<float>(wd, q.wdAdsZoomFov) > 0) {
    fov = lerp(baseFov, field<float>(wd, q.wdAdsZoomFov), ads);
  }
  float tanHalf = std::tan(fov * 0.5f * 0.01745329f), scale = 320.0f / tanHalf;
  float fovScaleInv = std::tan(baseFov * 0.5f * 0.01745329f) / tanHalf;
  aa.f<float>(0x120) = fov / baseFov;
  aa.f<float>(0x124) = fovScaleInv;
  aa.f<float>(0xb8) = aa.f<float>(0xbc) = aa.f<float>(0xd0) = aa.f<float>(0xd4) = kSlowRegion * fovScaleInv;
  aa.f<float>(0xc0) = kAutoRegionW * fovScaleInv;
  aa.f<float>(0xc4) = kAutoRegionH * fovScaleInv;

  // targets
  auto ents = q.gameDll ? dll_at(q.gameDll, q.gameBase, q.gentities) : at<uint8_t*>(q.gentities);
  auto numEnts = q.gameDll ? dll_at(q.gameDll, q.gameBase, q.numEntities) : at<uint8_t*>(q.numEntities);
  if (!ents || !numEnts) { aa.f<int>(0xe3c) = 0; return aa; }  // game DLL not loaded (main menu)
  auto teamOf = [&](uint8_t* e) { auto s = field<uint8_t*>(e, q.entSentient); return s ? field<int>(s, q.sentientTeam) : 0; };
  int myTeam = teamOf(ents), n = 0, count = std::min(*reinterpret_cast<int*>(numEnts), MAX_ENTS);
  auto* targets = &aa.f<AimScreenTarget>(0x13c);
  static const float zero[3] = {};
  for (int i = 1; i < count && n < MAX_TARGETS; i++) {
    uint8_t* e = ents + i * q.entSize;
    uint8_t* actor = field<uint8_t*>(e, q.entActor);
    int team = teamOf(e);
    bool alive = (q.etActor < 0 ? field<uint8_t*>(e, q.entSentient) != nullptr : field<uint8_t>(e, q.entType) == q.etActor) &&
                 field<int>(e, q.entHealth) > 0 && actor;
    float* pos = vec(e, q.entOrigin);
    // entities move on server frames (20 Hz), not every client frame: velocity over the time since the last position
    // change, held in between (per-frame deltas jerk between 0 and 3x the real speed)
    static float lastVel[MAX_ENTS][3], since[MAX_ENTS];
    since[i] += dt;
    if (!seen[i]) {
      memset(lastVel[i], 0, 12); since[i] = 0; memcpy(lastPos[i], pos, 12);
    } else if (memcmp(pos, lastPos[i], 12) && since[i] > 0) {
      for (int k = 0; k < 3; k++) lastVel[i][k] = (pos[k] - lastPos[i][k]) / since[i];
      since[i] = 0; memcpy(lastPos[i], pos, 12);
    } else if (since[i] > 0.2f) {
      memset(lastVel[i], 0, 12);  // stopped
    }
    float* vel = lastVel[i];
    seen[i] = alive;
    if (!alive || !team || team == myTeam || team == 3) continue;  // 3: neutral

    float* eye = vec(actor, q.actorEye);
    float aim[3] = { eye[0], eye[1], eye[2] - kAimBelowEye };
    float d[3] = { aim[0] - org[0], aim[1] - org[1], aim[2] - org[2] };
    float fwd = d[0] * axis[0][0] + d[1] * axis[0][1] + d[2] * axis[0][2];
    if (fwd < 1) continue;  // behind the view
    auto project = [&](const float* p, float& sx, float& sy) {
      float v[3] = { p[0] - org[0], p[1] - org[1], p[2] - org[2] };
      float f = v[0] * axis[0][0] + v[1] * axis[0][1] + v[2] * axis[0][2];
      sx = -(v[0] * axis[1][0] + v[1] * axis[1][1]) / f * scale;  // right is +x
      sy = -(v[0] * axis[2][0] + v[1] * axis[2][1] + v[2] * axis[2][2]) / f * scale;  // down is +y
    };
    float head[3] = { eye[0], eye[1], eye[2] + kHeadAboveEye }, cx, top, x2, bottom;
    project(head, cx, top);
    project(pos, x2, bottom);
    float hw = kTargetHalfWidth / fwd * scale;

    int hit = 0;
    (q.sightUo ? call_sight_trace_uo : call_sight_trace_q3)(q.sightTrace + slide, &hit, org, aim, zero, zero, 0, i, q.sightMask);
    if (hit) continue;

    AimScreenTarget t{};
    t.entIndex = i;
    t.clipMins[0] = std::min(cx, x2) - hw; t.clipMaxs[0] = std::max(cx, x2) + hw;
    t.clipMins[1] = std::min(top, bottom); t.clipMaxs[1] = std::max(top, bottom);
    memcpy(t.aimPos, aim, 12);
    memcpy(t.velocity, vel, 12);
    t.distSqr = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    float mx = (t.clipMins[0] + t.clipMaxs[0]) * 0.5f, my = (t.clipMins[1] + t.clipMaxs[1]) * 0.5f;
    t.crosshairDistSqr = mx * mx + my * my;
    int j = n++;
    for (; j > 0 && targets[j - 1].crosshairDistSqr > t.crosshairDistSqr; j--) targets[j] = targets[j - 1];
    targets[j] = t;
  }
  aa.f<int>(0xe3c) = n;
  static int maxSeen;
  if (n > maxSeen) { maxSeen = n; logmsg("aim: %d targets in view (entities %d, fov %.0f)", n, count, fov); }
  return aa;
}
#endif

#ifndef _WIN64
// --- Quake 3 family hints and button icons
// The hint lookup (CoD2 0x4cdb30: cmd, char out[2][0x80]) returns the UI's cached first two keys, keyboard first.
using Q3GetKeyBinding_t = int (*)(const char* cmd, char* out);
int hk_q3_get_key_binding(const char* cmd, char* out) {
  int pk[2] = {-1, -1};
  if (int n = hint_pad_keys(cmd, pk)) {
    out[0] = out[0x80] = 0;
    for (int i = 0; i < n; i++) snprintf(out + i * 0x80, 0x80, "%s", buttonText[pk[i] - k_aux1()]);
    return n;
  }
  return at<Q3GetKeyBinding_t>(g->hintFunc)(cmd, out);
}

// UO: hints ("Press [{+activate}]", "Press USE [%s]") name keys from the cgame's command table: 0x18-byte entries,
// command string at +0, first two bound keys at +0x10 / +0x14 (-1 none), keyboard keys first, refilled by the game
// when binds change. While the pad is in use our buttons go there. The game's own keys come back when it isn't.
// The stance hints (and others) refill the whole table from the binds (0x30042500) right before reading it, so the end
// of that refill jumps to stub_uo_keys_refilled, which runs this again. Re-patched whenever the cgame DLL is reloaded.
// ponytail: the cgame's "write binds from the table" (0x30042540, after a bind change there) would then bind the pad
// buttons to the equivalent commands too. The binds menu lives in the UI DLL, so not seen so far.
void q3_hint_keys() {
  auto t = g->q3->hintTable ? dll_at(g->q3->cgameDll, g->q3->cgameBase, g->q3->hintTable) : nullptr;
  if (!t) return;
  if (auto ep = dll_at(g->q3->cgameDll, g->q3->cgameBase, g->q3->hintRefillEnd); ep && !memcmp(ep, "\x5e\x5b\x83\xc4\x08\xc3", 6)) {
    DWORD old;
    VirtualProtect(ep, 6, PAGE_EXECUTE_READWRITE, &old);
    ep[0] = 0xE9;
    *reinterpret_cast<int32_t*>(ep + 1) = int32_t(reinterpret_cast<uintptr_t>(stub_uo_keys_refilled) - uintptr_t(ep + 5));
    ep[5] = 0x90;
    VirtualProtect(ep, 6, old, &old);
    FlushInstructionCache(GetCurrentProcess(), ep, 6);
    logmsg("hint key refill hooked");
  }
  static int orig[64][2], wrote[64][2] = {{INT_MIN}};
  if (wrote[0][0] == INT_MIN) for (auto& w : wrote) w[0] = w[1] = INT_MIN;
  for (int i = 0; i < g->q3->hintCount && i < 64; i++) {
    auto e = t + i * 0x18;
    auto k = reinterpret_cast<int*>(e + 0x10);
    if (k[0] != wrote[i][0] || k[1] != wrote[i][1]) orig[i][0] = k[0], orig[i][1] = k[1];  // (re)filled by the game
    int want[2] = { orig[i][0], orig[i][1] }, pk[2] = { -1, -1 };
    if (auto cmd = field<const char*>(e, 0); cmd && pad_in_use)
      if (int n = pad_keys_equiv(cmd, pk)) want[0] = pk[0], want[1] = n > 1 ? pk[1] : -1;
    k[0] = wrote[i][0] = want[0];
    k[1] = wrote[i][1] = want[1];
  }
}

extern "C" void iwpad_hint_keys_refilled() { q3_hint_keys(); }

// Pad key names are "\x01" + 'A' + button. The renderer's DrawText / TextWidth (refexport_t slots, swapped for ours)
// draw that as the button's icon (materials iwpad_xbox_N / iwpad_ps3_N from zz_iwpad.iwd, glyphs/cod2_iwd.py) and
// pass the text around it through. Without the iwd the materials come out as the default image.
constexpr char ICON = '\x01';
constexpr float kIconLines = 1.1f, kIconGap = 0.1f, kIconAbove = 0.82f;  // ponytail: placement by eye. Tune on Deck
using ReDrawText_t = void (*)(const char* text, int maxChars, void* font, float x, float y, float xs, float ys,
                              const float* color, int style);
using ReTextWidth_t = int (*)(const char* text, int maxChars, void* font);
using ReTextHeight_t = int (*)(void* font);
using ReStretchPic_t = void (*)(float x, float y, float w, float h, float s0, float t0, float s1, float t1,
                                const float* color, void* material);
using ReRegisterMaterial_t = void* (*)(const char* name, int, int);
ReDrawText_t q3DrawText;
ReTextWidth_t q3TextWidth;

int pow2(int v) { int p = 1; while (p < v) p *= 2; return p; }
// glyphs.bin entry b of the current set: pixel size (tw, th)
void glyph_size(int set, int b, int& tw, int& th) {
  const unsigned char* p = kGlyphPixels;
  for (int i = 0; i < set * kGlyphCount + b; i++) p += 4 + p[2] * p[3] * 4;
  tw = p[2]; th = p[3];
}
int icon_set() { return ownb(gpad_ps_glyphs, false) ? 1 : 0; }
float icon_width(int b, float lineH) {  // in the units of lineH
  int tw, th; glyph_size(icon_set(), b, tw, th);
  return lineH * kIconLines * tw / th + lineH * kIconGap;
}
bool icon_at(const char* p) { return p[0] == ICON && p[1] >= 'A' && p[1] <= 'A' + START; }

// Walks text, calling seg(start, len) for plain runs and icon(b) for markers. Hints write keys as "[key]". An icon
// drops those brackets.
template <class Seg, class Icon, class Nl> void walk_icons(const char* text, int maxChars, Seg seg, Icon icon, Nl nl) {
  const char* run = text;
  int n = 0;
  for (const char* p = text; *p && n < maxChars; p++, n++) {
    if (*p == '\n') {  // the renderer starts each line at x again, so the icons' x has to as well
      if (p > run) seg(run, int(p - run));
      nl();
      run = p + 1;
      continue;
    }
    if (!icon_at(p)) continue;
    bool br = p > run && p[-1] == '[' && p[2] == ']';
    if (p - br > run) seg(run, int(p - br - run));
    icon(p[1] - 'A');
    run = (p += 1 + br) + 1;
  }
  if (*run) seg(run, -1);
}

// "Click to ..." prompts (ui/pregame.menu, cinematics): with the pad in use they name the button instead, and A / Start
// click (update_buttons) while one was drawn recently
const char* pad_prompt(const char* text, bool drawn) {
  static const char* const kPrompts[][2] = {
    {"Click to Start the Mission", "Press \x01" "A to Start the Mission"},
    {"Click to Continue", "Press \x01" "A to Continue"},
    {"Click FIRE to Skip.", "Press \x01" "A to Skip."},
    {"Press FIRE to Skip.", "Press \x01" "A to Skip."},  // CoD1 / UO
  };
  if (!text || (*text != 'C' && *text != 'P')) return text;
  for (auto& pr : kPrompts)
    if (!strcmp(text, pr[0])) {
      if (drawn) clickPromptMs = now_ms();
      return pad_in_use ? pr[1] : text;
    }
  return text;
}

// "Press USE [&&1] to ..." (CoD2's hints name the action before the key): the icon already says which button, so
// "USE " goes when an icon (or "[" + icon) follows. ponytail: English text only.
const char* drop_use(const char* text, char (&buf)[1024]) {
  if (!text || !strstr(text, "USE ")) return text;
  char* o = buf;
  for (const char* p = text; *p && o < buf + sizeof buf - 1; p++) {
    if (!strncmp(p, "USE ", 4) && (icon_at(p + 4) || (p[4] == '[' && icon_at(p + 5)))) { p += 3; continue; }
    *o++ = *p;
  }
  *o = 0;
  return buf;
}

int hk_re_text_width(const char* text, int maxChars, void* font) {
  char noUse[1024];
  text = drop_use(pad_prompt(text, false), noUse);
  if (!text || !strchr(text, ICON)) return q3TextWidth(text, maxChars, font);
  float w = 0, wmax = 0, lineH = float(at<ReTextHeight_t*>(g->q3->refTextHeight)[0](font));
  char buf[1024];
  walk_icons(text, maxChars, [&](const char* s, int len) {
    snprintf(buf, sizeof buf, "%.*s", len < 0 ? int(sizeof buf) : len, s);
    w += float(q3TextWidth(buf, 0x7fffffff, font));
  }, [&](int b) { w += icon_width(b, lineH); }, [&] { wmax = std::max(wmax, w); w = 0; });
  return int(std::max(wmax, w) + 0.5f);
}

void hk_re_draw_text(const char* text, int maxChars, void* font, float x, float y, float xs, float ys,
                     const float* color, int style) {
  char noUse[1024];
  text = drop_use(pad_prompt(text, true), noUse);
  if (!text || !strchr(text, ICON)) return q3DrawText(text, maxChars, font, x, y, xs, ys, color, style);
  const Q3& q = *g->q3;
  const float x0 = x;
  float lineH = float(at<ReTextHeight_t*>(q.refTextHeight)[0](font)) * ys;
  float white[4] = {1, 1, 1, color ? color[3] : 1};
  char buf[1024];
  walk_icons(text, maxChars, [&](const char* s, int len) {
    snprintf(buf, sizeof buf, "%.*s", len < 0 ? int(sizeof buf) : len, s);
    q3DrawText(buf, 0x7fffffff, font, x, y, xs, ys, color, style);
    x += float(q3TextWidth(buf, 0x7fffffff, font)) * xs;
  }, [&](int b) {
    int set = icon_set(), tw, th;
    glyph_size(set, b, tw, th);
    char name[32];
    snprintf(name, sizeof name, "iwpad_%s_%d", set ? "ps3" : "xbox", b);
    void* mat = at<ReRegisterMaterial_t*>(q.refRegisterMaterial)[0](name, 3, 7);
    float h = lineH * kIconLines, w = h * tw / th;
    at<ReStretchPic_t*>(q.refStretchPic)[0](x, y - h * kIconAbove, w, h, 0, 0, float(tw) / pow2(tw),
                                            float(th) / pow2(th), white, mat);
    x += w + lineH * kIconGap;
  }, [&] { x = x0; y += lineH; });  // ponytail: line advance assumed = font height. Check on two-line hints
}

// The menus highlight the row, so pad input hides the cursor: its material handle points at a transparent one
// (iwpad_blank in zz_iwpad.iwd). Moving the mouse / trackpad (the cursor moving with no pad input) shows it again.
// UO videos are black on widescreen: SCR_DrawScreenField queues a full-screen black StretchPic whenever
// width * 480 > height * 640, but the video frame is drawn at once (RE_StretchRaw), so the queued fill lands on top.
// While a video plays (cls.state 3) the `jle` that skips the fill becomes a `jmp`; the video covers the whole screen.
void q3_video_fill() {
  auto jle = at<uint8_t*>(g->q3->videoFillJle);
  uint8_t want = *at<int*>(g->q3->clsState) == 3 ? 0xEB : 0x7E;
  if ((*jle == 0x7E || *jle == 0xEB) && *jle != want) {
    DWORD old;
    VirtualProtect(jle, 1, PAGE_EXECUTE_READWRITE, &old);
    *jle = want;
    VirtualProtect(jle, 1, old, &old);
  }
}

// Runs after update_buttons, so the cursor parking on a d-pad step doesn't count as a move.
// UO: no material to swap in (no iwd yet), so the jump around the cursor draw is flipped instead.
void q3_hide_cursor() {
  auto m = g->q3->menus;
  if (!g->q3->uiCursorMat && !(m && m->cursorJle)) return;
  static bool hidden;
  static int lastX, lastY;
  int x, y;
  if (m) {
    auto dc = q3_ui_dc();
    if (!dc) return;
    x = field<int>(dc, m->cursorOff); y = field<int>(dc, m->cursorOff + 4);
  } else {
    auto dc = at<uint8_t*>(g->q3->uiDc);
    x = field<int>(dc, 0xc); y = field<int>(dc, 0x10);
  }
  bool padInput = pad.connected && (pad.buttons || pad.lt > 0.25f || pad.rt > 0.25f || pad.lx || pad.ly || pad.rx || pad.ry);
  if (padInput && ownb(gpad_enabled, true)) hidden = true;
  else if (std::abs(x - lastX) + std::abs(y - lastY) > 3) hidden = false;  // a few px: gyro / trackpad noise stays hidden
  lastX = x; lastY = y;
  // looked up each time: a level load frees materials registered before it (the main menu's), so a kept handle dangles
  bool menuOpen = *at<int*>(g->keyCatchers) & g->keyCatchUi;  // never during loads / gameplay
  if (m) {
    auto jle = q3_ui(m->cursorJle);  // the DLL may be reloaded (vid_restart): checked every frame
    uint8_t want = hidden && menuOpen ? 0xEB : 0x7E;
    if ((*jle == 0x7E || *jle == 0xEB) && *jle != want) {
      DWORD old;
      VirtualProtect(jle, 1, PAGE_EXECUTE_READWRITE, &old);
      *jle = want;
      VirtualProtect(jle, 1, old, &old);
    }
    return;
  }
  auto& cur = *at<void**>(g->q3->uiCursorMat);
  static void *stock, *blank;
  if (!cur) return;
  if (cur != blank) stock = cur;
  blank = hidden && menuOpen ? at<ReRegisterMaterial_t*>(g->q3->refRegisterMaterial)[0]("iwpad_blank", 0x30, 0) : nullptr;
  cur = blank ? blank : stock;
}

// CoD1 / UO console wording (glyphs/q3_strings.txt -> q3_text.inc), swapped in while the pad is in use. Stock lines
// match with up to two "%s" (the key / item the game filled in), carried into the new line in order. "[{cmd}]" in
// the new line becomes the pad button bound to cmd (no button: the stock line stays).
const char* const kQ3Text[][2] = {
#include "q3_text.inc"
};
bool wild(const char* p, const char* s, const char** cap, int* len, int n) {
  for (; *p; p++, s++) {
    if (p[0] == '%' && p[1] == 's') {
      for (const char* e = s;; e++) {
        if (n < 2) cap[n] = s, len[n] = int(e - s);
        if (wild(p + 2, e, cap, len, n + 1)) return true;
        if (!*e) return false;
      }
    }
    if (*p != *s) return false;
  }
  return !*s;
}
const char* q3_console_text(const char* text, char (&buf)[1024]) {
  if (!text || !pad_in_use || !g->q3->refSetColor) return text;
  // the client's message lines (CL 0x409130) come space-padded to 60 characters: match without the padding
  const char* orig = text;
  char trim[1024];
  if (size_t n = strlen(text); n && n < sizeof trim && text[n - 1] == ' ') {
    while (n && text[n - 1] == ' ') n--;
    memcpy(trim, text, n), trim[n] = 0, text = trim;
  }
  for (auto& t : kQ3Text) {
    const char* cap[2] = {};
    int len[2] = {};
    if (t[0][0] != '%' && t[0][0] != text[0]) continue;
    if (!wild(t[0], text, cap, len, 0)) continue;
    char* o = buf;
    char* end = buf + sizeof buf - 1;
    int c = 0;
    for (const char* r = t[1]; *r && o < end; r++) {
      if (r[0] == '%' && r[1] == 's') {
        if (c < 2) o += snprintf(o, end - o, "%.*s", len[c], cap[c]), c++;
        r++;
      } else if (r[0] == '[' && r[1] == '{') {
        const char* close = strchr(r, '}');
        if (!close) return orig;
        char cmd[64];
        snprintf(cmd, sizeof cmd, "%.*s", int(close - r - 2), r + 2);
        int pk[2];
        if (!pad_keys_equiv(cmd, pk)) return orig;
        o += snprintf(o, end - o, "[%s]", buttonText[pk[0] - k_aux1()]);
        r = close + (close[1] == ']');
      } else {
        *o++ = *r;
      }
    }
    *o = 0;
    return buf;
  }
  return orig;
}

// UO renderer: DrawText (x, y, font, scale, color, text, xscale, maxChars, a9) in 640x480 units, queued for the
// render thread (0x4e12d0). TextWidth (text, font, scale, xscale, maxChars) in the same units (the xscale is the
// draw's 7th argument, e.g. 640 / vid width at 0x40c9e0. 0 = fixed width). Raw images need the extension in the name. Icons sit like the
// renderer's own inline images (rich text 0x4e1120): scale * 48 high, top at y - (scale * 38.4 + h) / 2, drawn with
// SetColor + StretchPic in screen pixels after ri's adjust-from-640.
using UoDrawText_t = void (*)(float x, float y, int font, float scale, const float* color, const char* text, int a7,
                              int maxChars, int a9);
using UoTextWidth_t = int (*)(const char* text, int font, float scale, float prop, int maxChars);
using UoStretchPic_t = void (*)(float x, float y, float w, float h, float s0, float t0, float s1, float t1, void* shader);
using UoRegisterShader_t = void* (*)(const char* name, int flags);
using UoAdjust_t = void (*)(float* x, float* y, float* w, float* h);
// The renderer's own draw (refexport +0xf0, UO 0x4e12d0): the slot above calls it with maxChars -1, cursor 0 and its
// a7..a9 as the last three. The cgame's other text syscall (CoD1 0x4027af: typed-out centre prints) calls it directly.
using UoDrawTextExt_t = void (*)(float x, float y, int font, float scale, const float* color, const char* text,
                                 int maxChars, int cursor, int a9, int a10, int a11);
UoDrawText_t uoDrawText;
UoTextWidth_t uoTextWidth;
UoDrawTextExt_t uoDrawTextExt;
// refexport +0xe8 / +0xec (CoD1 0x4d5750 / 0x4d5890, UO 0x4e0fe0 / 0x4e1120): the same calls for the client's own
// message lines (CL draw 0x409130: "Objectives updated", iprintlnbold such as training's objectives), centred by width.
UoTextWidth_t uoRichWidth;
UoDrawText_t uoRichDraw;
static_assert(sizeof(float) == sizeof(int));
constexpr float kUoIconSize = 48, kUoIconMid = 38.4f;

const char* uo_text(const char* text, bool drawn, char (&a)[1024], char (&b)[1024]) {
  return drop_use(q3_console_text(pad_prompt(text, drawn), a), b);
}

// The client's message lines are stored as {char, colour byte} pairs ("U\x07s\x07e\x07..."), space-padded. Swapped
// lines are packed again with the line's colour byte so the game still colours / fades them. Lines with icons go
// through the plain hooks instead (unpacked). Returns the text to draw, or null when nothing is swapped.
// The rows sit back to back with no terminator between them: `cells` (the draw's character count) ends this one.
const char* msg_line_swap(const char* text, int cells, char (&out)[2048], bool& icons) {
  icons = false;
  if (!text || !text[0] || !text[1] || uint8_t(text[1]) >= 0x20) return nullptr;  // not a packed line
  char plain[1024], a[1024], b[1024];
  int n = 0;
  int lim = cells > 0 && cells < 1023 ? cells : 1023;
  for (const char* p = text; p[0] && p[1] && n < lim; p += 2) plain[n++] = p[0];
  plain[n] = 0;
  auto t = uo_text(plain, false, a, b);
  if (t == plain) return nullptr;
  if ((icons = strchr(t, ICON) != nullptr)) return snprintf(out, sizeof out, "%s", t), out;
  int o = 0;
  for (; *t && o + 2 < int(sizeof out); t++) out[o++] = *t, out[o++] = text[1];
  while (o < 2 * n && o + 2 < int(sizeof out)) out[o++] = ' ', out[o++] = text[1];  // same padding (centring)
  out[o] = 0;
  return out;
}
int hk_uo_text_width(const char* text, int font, float scale, float prop, int maxChars) {
  char a[1024], b[1024], mout[2048];
  bool icons;
  if (auto t = msg_line_swap(text, maxChars, mout, icons)) {
    if (!icons) return uoTextWidth(t, font, scale, prop, maxChars);
    text = t;
  }
  text = uo_text(text, false, a, b);
  if (!text || !strchr(text, ICON)) return uoTextWidth(text, font, scale, prop, maxChars);
  float w = 0, wmax = 0, lineH = scale * kUoIconSize;
  char buf[1024];
  walk_icons(text, maxChars > 0 ? maxChars : 0x7fffffff, [&](const char* s, int len) {
    snprintf(buf, sizeof buf, "%.*s", len < 0 ? int(sizeof buf) : len, s);
    w += float(uoTextWidth(buf, font, scale, prop, 0));
  }, [&](int b) {
    int tw, th;
    glyph_size(icon_set(), b, tw, th);
    w += lineH * tw / th + lineH * kIconGap;  // as hk_uo_draw_text advances
  }, [&] { wmax = std::max(wmax, w); w = 0; });
  return int(std::max(wmax, w) + 0.5f);
}

void hk_uo_draw_text(float x, float y, int font, float scale, const float* color, const char* text, int a7,
                     int maxChars, int a9) {
  char a[1024], b[1024], mout[2048];
  bool icons;
  if (auto t = msg_line_swap(text, maxChars, mout, icons)) {
    if (!icons) return uoDrawText(x, y, font, scale, color, t, a7, maxChars, a9);
    text = t;
  }
  text = uo_text(text, true, a, b);
  if (!text || !strchr(text, ICON)) return uoDrawText(x, y, font, scale, color, text, a7, maxChars, a9);
  const Q3& q = *g->q3;
  const float x0 = x, lineH = scale * kUoIconSize;
  float white[4] = {1, 1, 1, color ? color[3] : 1};
  char buf[1024];
  walk_icons(text, maxChars > 0 ? maxChars : 0x7fffffff, [&](const char* s, int len) {
    snprintf(buf, sizeof buf, "%.*s", len < 0 ? int(sizeof buf) : len, s);
    uoDrawText(x, y, font, scale, color, buf, a7, 0, a9);
    x += float(uoTextWidth(buf, font, scale, std::bit_cast<float>(a7), 0));
  }, [&](int b) {
    int set = icon_set(), tw, th;
    glyph_size(set, b, tw, th);
    char name[32];
    snprintf(name, sizeof name, "iwpad_%s_%d.tga", set ? "ps3" : "xbox", b);
    void* shader = at<UoRegisterShader_t*>(q.refRegisterMaterial)[0](name, 5);
    float h = lineH, w = h * tw / th, px = x, py = y - (scale * kUoIconMid + h) * 0.5f, pw = w, ph = h;
    at<UoAdjust_t*>(q.adjustFrom640)[0](&px, &py, &pw, &ph);
    at<void (**)(const float*)>(q.refSetColor)[0](white);
    at<UoStretchPic_t*>(q.refStretchPic)[0](px, py, pw, ph, 0, 0, float(tw) / pow2(tw), float(th) / pow2(th), shader);
    x += w + lineH * kIconGap;
  }, [&] { x = x0; y += lineH; });  // ponytail: line advance assumed = icon height. Check on two-line hints
}

void hk_uo_draw_text_ext(float x, float y, int font, float scale, const float* color, const char* text, int maxChars,
                         int cursor, int a9, int a10, int a11) {
  char a[1024], b[1024];
  char mout[2048];
  bool icons;
  if (auto m = msg_line_swap(text, maxChars, mout, icons)) {
    if (!icons) return uoDrawTextExt(x, y, font, scale, color, m, maxChars, cursor, a9, a10, a11);
    text = m;
  }
  auto t = uo_text(text, true, a, b);
  // whole lines with icons go through the plain hook (its wrapper calls the renderer directly, not this slot)
  if (t && strchr(t, ICON) && maxChars < 0 && !cursor) return hk_uo_draw_text(x, y, font, scale, color, text, a9, a10, a11);
  // ponytail: a typed-out line with a key in it shows the raw icon code. Strip it if such a line turns up
  uoDrawTextExt(x, y, font, scale, color, t, maxChars, cursor, a9, a10, a11);
}

int hk_uo_rich_width(const char* text, int font, float scale, float prop, int maxChars) {
  char out[2048];
  bool icons;
  if (auto t = msg_line_swap(text, maxChars, out, icons))
    return icons ? hk_uo_text_width(t, font, scale, prop, maxChars) : uoRichWidth(t, font, scale, prop, maxChars);
  return uoRichWidth(text, font, scale, prop, maxChars);
}
void hk_uo_rich_draw(float x, float y, int font, float scale, const float* color, const char* text, int a7, int a8, int a9) {
  char out[2048];
  bool icons;
  if (auto t = msg_line_swap(text, a8, out, icons)) {
    if (icons) return hk_uo_draw_text(x, y, font, scale, color, t, a7, a8, a9);
    return uoRichDraw(x, y, font, scale, color, t, a7, a8, a9);
  }
  uoRichDraw(x, y, font, scale, color, text, a7, a8, a9);
}

// The refexport table is refilled when the renderer restarts (vid_restart), so check the slots every frame.
void q3_hook_text() {
  if (!g->q3->refDrawText) return;
  if (g->q3->refSetColor) {  // UO
    auto dt = at<UoDrawText_t*>(g->q3->refDrawText);
    auto tw = at<UoTextWidth_t*>(g->q3->refTextWidth);
    if (!*dt || !*tw) return;  // renderer not up yet
    if (*dt != hk_uo_draw_text) { uoDrawText = *dt; *dt = hk_uo_draw_text; logmsg("text draw hooked (UO)"); }
    if (*tw != hk_uo_text_width) { uoTextWidth = *tw; *tw = hk_uo_text_width; }
    if (auto ext = at<UoDrawTextExt_t*>(g->q3->refDrawText + 0xc); *ext && *ext != hk_uo_draw_text_ext)
      uoDrawTextExt = *ext, *ext = hk_uo_draw_text_ext;
    if (auto w = at<UoTextWidth_t*>(g->q3->refDrawText + 4); *w && *w != hk_uo_rich_width) uoRichWidth = *w, *w = hk_uo_rich_width;
    if (auto d = at<UoDrawText_t*>(g->q3->refDrawText + 8); *d && *d != hk_uo_rich_draw) uoRichDraw = *d, *d = hk_uo_rich_draw;
    return;
  }
  auto dt = at<ReDrawText_t*>(g->q3->refDrawText);
  auto tw = at<ReTextWidth_t*>(g->q3->refTextWidth);
  if (*dt != hk_re_draw_text) { q3DrawText = *dt; *dt = hk_re_draw_text; logmsg("text draw hooked"); }
  if (*tw != hk_re_text_width) { q3TextWidth = *tw; *tw = hk_re_text_width; }
}
#endif

// --- hooks
void hk_CL_MouseMove(uint8_t* cmd, float frametime) {
  // mouse/gyro look + engine aim assist bookkeeping
#ifdef _WIN64
  at<CL_MouseMove_t>(g->mouseMove)(cmd, frametime);
#else
  (g->mw3Regs ? call_mouse_move_mw3 : call_mouse_move)(g->mouseMove + slide, cmd, frametime);
#endif
  if (!pad.connected || !ownb(gpad_enabled, true) || (*at<int*>(g->keyCatchers) & (g->keyCatchUi | KEYCATCH_CONSOLE)))
    return;

  auto clamp_move = [](int v) { return v > 127 ? 127 : v < -127 ? -127 : v; };
  auto fwd = reinterpret_cast<int8_t*>(cmd + g->cmdForward), right = fwd + 1;
  *fwd = int8_t(clamp_move(*fwd + int(std::lround(pad.ly * 127))));
  *right = int8_t(clamp_move(*right + int(std::lround(pad.lx * 127))));

  // MW3 vehicles steered by script (Hunter Killer DPV) read getnormalizedcameramovement: usercmd bytes cmdForward + 4
  // (pitch, + = down) and + 5 (yaw, + = right). CL_MouseMove fills them from the mouse while snapFlags+4 has 0x40000
  // or 0x10000. Mirror that with the right stick.
  if (g->cameraMove && (at<unsigned*>(g->snapFlags)[1] & 0x50000)) {
    float down = ownb(gpad_invert, false) ? pad.ry : -pad.ry;
    fwd[4] = int8_t(clamp_move(fwd[4] + int(std::lround(down * 127))));
    fwd[5] = int8_t(clamp_move(fwd[5] + int(std::lround(pad.rx * 127))));
  }

  // CL_MouseMove skips look on these conditions too
  if (*at<int*>(g->frameMsec) == 0 || (*at<unsigned*>(g->snapFlags) & g->noLookMask)) return;
  if (g->noLookFlags && (*at<unsigned*>(g->noLookFlags) & 1)) return;
  float& pitch = *at<float*>(g->viewPitch);
  float& yaw = *at<float*>(g->viewYaw);
#ifndef _WIN64
  if (g->q3) {
    frametime = *at<int*>(g->frameMsec) * 0.001f;
    return apply_look(q3_aim(frametime), *reinterpret_cast<int*>(cmd + 4), frametime, pitch, yaw);
  }
#endif
  apply_look(Aim{at<uint8_t*>(g->aaGlob)}, *reinterpret_cast<int*>(cmd + 4), frametime, pitch, yaw);
}

// While a Predator is steered, CL_CreateCmd calls the remote control move instead of CL_MouseMove. It reads only the
// mouse into usercmd.remoteControlAngles (pitch, yaw: -127..127, mouse right/up is negative), at cmdForward + 0x22 in
// every build. Either stick steers. The player can't walk meanwhile.
void remote_steer(uint8_t* cmd) {
  if (!pad.connected || !ownb(gpad_enabled, true) || (*at<int*>(g->keyCatchers) & (g->keyCatchUi | KEYCATCH_CONSOLE)))
    return;
  auto add = [](int8_t& v, float stick) {
    int n = v - int(std::lround(stick * 127));
    v = int8_t(n > 127 ? 127 : n < -127 ? -127 : n);
  };
  auto angles = reinterpret_cast<int8_t*>(cmd + g->cmdForward + 0x22);
  float up = pad.ry + pad.ly;
  add(angles[0], ownb(gpad_invert, false) ? -up : up);
  add(angles[1], pad.rx + pad.lx);
}

#ifdef _WIN64
void hk_remote_move(int client, uint8_t* cmd) {
  at<void (*)(int, uint8_t*)>(g->remoteMove)(client, cmd);
  remote_steer(cmd);
}
#endif

#ifndef _WIN64
extern "C" bool iwpad_skip_text(const char* text) { return strncmp(text, "AlterWare IW5-Mod", 17) == 0; }

// iw5-mod builds its watermark text at runtime (no literal to blank), so drop it at the draw call instead.
// Installed from the main thread, the one that calls R_AddCmdDrawText, so no draw runs mid-patch.
void hook_draw_text() {
  static const uint8_t kEntry[] = { 0x8B, 0x44, 0x24, 0x04, 0x80, 0x38, 0x00 };
  auto* p = at<uint8_t*>(g->drawText);
  if (memcmp(p, kEntry, sizeof kEntry) != 0) { logmsg("R_AddCmdDrawText does not match, watermark stays"); return; }
  iwpad_draw_text_resume = uintptr_t(p) + sizeof kEntry;
  DWORD old;
  VirtualProtect(p, sizeof kEntry, PAGE_EXECUTE_READWRITE, &old);
  p[0] = 0xE9;
  *reinterpret_cast<int32_t*>(p + 1) = int32_t(uintptr_t(stub_draw_text) - uintptr_t(p + 5));
  p[5] = p[6] = 0x90;
  VirtualProtect(p, sizeof kEntry, old, &old);
  FlushInstructionCache(GetCurrentProcess(), p, sizeof kEntry);
  logmsg("watermark draw hooked");
}
#endif

// --- our zone (menus). Loaded once the game is up, from zone/english/iwpad.ff when it exists. Assets of a
// later zone replace earlier ones in place (the engine copies the new asset over the old one and keeps the
// original. Asset types with a pool size > 1), so its menus override the stock ones even after the UI loaded them.
struct XZoneInfo { const char* name; int allocFlags, freeFlags; };
using DB_LoadXAssets_t = void (*)(XZoneInfo* zones, unsigned count, int sync);
void load_our_zone() {
  if (!g->zoneLoad || GetFileAttributesA("zone\\english\\iwpad.ff") == INVALID_FILE_ATTRIBUTES) return;
  XZoneInfo zone = { "iwpad", 1, 0 };  // allocFlags 1 like common: stays loaded across levels. Frees nothing
  at<DB_LoadXAssets_t>(g->zoneLoad)(&zone, 1, 1);
  logmsg("zone iwpad loaded");
}

// --- stock menu patches. The UI keeps the loaded menuDef_t pointers in UiContext.Menus. They are checked every
// frame, so menus reloaded with ui.ff get patched again. Offsets shared by both engines (x64). The rest is in
// Game::menus.
constexpr int UI_MENUS = 0x38, MAX_ITEM_SIZE = 0x1F8, ITEM_TYPE_SLIDER = 10, ITEM_TYPE_YESNO = 11;

// Controller rows added to the Look screen: label, the console's localized label if the game still has it (MW3's
// code_post_gfx does), dvar, slider range and d-pad step (max 0: Yes/No), and for a slider the dvar_t whose value
// shows next to it, times `shown`, with `unit`; or, with `steps`, the slider moves between those values and shows
// the step number
struct MenuRow {
  const char* label; const char* key; const char* dvar; float min, max, def, step; uint8_t** var; float shown; const char* unit;
  const float* steps; int nSteps;
};
// The console Look Sensitivity list, 1-10 (Medium = 2 is the default): view sensitivity values, which scale the turn
// rates like gpad_sensitivity. MW3's own table (x64 0x1403f75b0, profile_menuDvarsSetup. profileMenuOption_sensitivity
// is 1-10, default 2). MW2's PC data only keeps the Low/Medium/High/Insane presets (code_post_gfx.ff
// viewsensitivity_*.cfg: 0.6 / 1 / 1.8 / 4), which match.
constexpr float kConsoleSensitivity[] = {0.6f, 1.0f, 1.4f, 1.8f, 2.0f, 2.2f, 2.6f, 3.0f, 3.5f, 4.0f};
constexpr MenuRow kControllerRows[] = {
  // a tiny step so that in MW3 too a press only nudges it and show_slider_values moves it a whole step
  {"Controller Sensitivity", nullptr, "gpad_sensitivity", 0.6f, 4.0f, 1.0f, 0.01f, &gpad_sensitivity, 0, "",
   kConsoleSensitivity, int(std::size(kConsoleSensitivity))},
  {"Invert Look", nullptr, "gpad_invert"},
  {"Aim Assist", "@MENU_AIM_ASSIST", "gpad_aim_assist"},
  // percent of console aim assist (100% = console) and of stick travel (console: 20%)
  {"Aim Assist Strength", nullptr, "gpad_aim_strength", 0.0f, 1.0f, 0.75f, 0.05f, &gpad_aim_strength, 100, "%"},
  {"Stick Deadzone", nullptr, "gpad_deadzone", 0.0f, 0.9f, 0.2f, 0.05f, &gpad_deadzone, 100, "%"},
  {"PlayStation Icons", nullptr, "gpad_ps_glyphs"},
  {"Console Style HUD", nullptr, "gpad_console_hud"},
  {"Enable Controller", nullptr, "gpad_enabled"},
  {"Reset Controller Settings", nullptr, "gpad_reset"},
};

using DB_FindXAssetHeader_t = const char** (*)(int type, const char* name, int createDefault);  // MW2 has no 3rd arg
constexpr int ASSET_TYPE_LOCALIZE = 0x1B;
// the LocalizeEntry {value, name} for name, or null (never creates one)
const char** find_localized(const char* name) {
  return g->findAsset ? at<DB_FindXAssetHeader_t>(g->findAsset)(ASSET_TYPE_LOCALIZE, name, 0) : nullptr;
}

bool text_is(uint8_t* item, int off, const char* s) { auto t = field<const char*>(item, off); return t && !strcmp(t, s); }

// A row is every item level with its label (MW2: value + label. MW3 adds a selection bar shadow), minus the
// thin divider lines that sit on or just above it.
int row_items(uint8_t** src, int count, uint8_t* label, uint8_t** out) {
  auto& L = *g->menus;
  int n = 0;
  float y = field<float>(label, L.rectY);
  for (int j = 0; j < count && n < 4; j++)
    if (std::fabs(field<float>(src[j], L.rectY) - y) < 5 && field<float>(src[j], L.rectH) >= 6) out[n++] = src[j];
  return n;
}

// pc_options_look gets a divider and the controller rows below its own rows: copies of its Mouse Sensitivity slider
// row, Smooth Mouse Yes/No row and divider, pointed at the gpad_ dvars. The frontend (ui.ff) and in-game (common.ff)
// Look are separate menus that both stay loaded, so each gets its own items array. The copies only point into that
// menu's zone.
struct LookPatch {
  uint8_t* menu; uint8_t* items[112];
  alignas(8) uint8_t copies[64][MAX_ITEM_SIZE];
  float sliders[4][8];
  uint8_t *bars[5], *barSliders[5]; int nBars;  // MW3 slider highlight bars and their sliders
  uint8_t *values[4], *valueSliders[4]; const MenuRow* valueRows[4]; char valueText[4][32]; float last[4]; int nValues;  // slider values
};
LookPatch lookPatches[4];  // frontend and in-game Look (MW3: pc_options_look_ingame), plus reloads

constexpr uint32_t WINDOW_DECORATION = 0x100000, WINDOW_HASFOCUS = 2, WINDOW_VISIBLE = 4;

// A slider row's bar shows while its slider has focus. Set from the dynamicFlags (right after staticFlags) every
// frame instead of a cloned visibility expression: IW5 x86 expressions are bigger than they look and get overwritten.
void show_slider_bars(LookPatch& p) {
  int dyn = g->menus->staticFlags + 4;
  for (int k = 0; k < p.nBars; k++) {
    auto& flags = field<uint32_t>(p.bars[k], dyn);
    flags = field<uint32_t>(p.barSliders[k], dyn) & WINDOW_HASFOCUS ? flags | WINDOW_VISIBLE : flags & ~WINDOW_VISIBLE;
  }
}

// Each controller slider's current value ("0.20") sits just right of its bar. Set every frame: the text follows the
// dvar and the position the slider's rect, which MW3 recomputes from its rect expressions. rect and rectClient are
// {x, y, w, h}.
// index of the step closest to v
int nearest_step(const MenuRow& r, float v) {
  int n = 0;
  for (int j = 1; j < r.nSteps; j++) if (std::fabs(r.steps[j] - v) < std::fabs(r.steps[n] - v)) n = j;
  return n;
}

void show_slider_values(LookPatch& p) {
  auto& L = *g->menus;
  for (int k = 0; k < p.nValues; k++) {
    auto& r = *p.valueRows[k];
    float v = ownf(*r.var, r.def);
    if (r.steps && *r.var && v != p.last[k]) {
      // a press (MW2: range / 40, MW3: r.step) is less than the closest steps' gap: the next step that way. A bigger
      // jump (mouse drag) goes to the nearest step
      int n = nearest_step(r, v);
      if (float last = p.last[k]; std::fabs(v - last) < 0.1f) {
        if (v > last) for (n = r.nSteps - 1; n > 0 && r.steps[n - 1] > last + 0.001f; n--) {}  // first step above
        else for (n = 0; n < r.nSteps - 1 && r.steps[n + 1] < last - 0.001f; n++) {}       // last step below
      }
      v = r.steps[n];
      *reinterpret_cast<float*>(*r.var + g->dvarValue) = v;
      p.last[k] = v;
    } else if (!L.editStep && *r.var && v != p.last[k]) {
      // MW2 (Item_HandleKey slider case) moves a slider by (max - min) / ui_sliderSteps per press, default 40, and
      // has no step field (IW5 has, so MW3 already steps by r.step). That's a fraction of r.step, so a press moves
      // one whole step that way instead. A bigger jump (mouse drag) rounds to the nearest step.
      float d = v - p.last[k];
      v = std::fabs(d) < r.step ? p.last[k] + (d > 0 ? r.step : -r.step) : std::round(v / r.step) * r.step;
      v = std::fmin(std::fmax(v, r.min), r.max);
      *reinterpret_cast<float*>(*r.var + g->dvarValue) = v;
      p.last[k] = v;
    }
    if (r.steps) snprintf(p.valueText[k], sizeof p.valueText[k], "%d", nearest_step(r, v) + 1);  // a value set in
    else snprintf(p.valueText[k], sizeof p.valueText[k], "%g%s", std::round(v * r.shown * 10) / 10, r.unit);  // between: nearest
    field<const char*>(p.values[k], L.text) = p.valueText[k];
    // right of the bar: MW2 (Item_Paint slider case) draws it SLIDER_WIDTH 96 wide at rect x + textalignx (0x18 before
    // text. The Look sliders are left aligned) inside a row-wide rect. MW3's rect is the bar, set by its rect
    // expressions (its rectClient keeps the menu file's row-wide rect, and text is drawn from rectClient, so both get x)
    auto s = p.valueSliders[k];
    float x = field<float>(s, L.rectY - 4) + 8 +
              (L.editStep ? field<float>(s, L.rectY + 4) : field<float>(s, L.text - 0x18) + 96);
    for (int y : {L.rectY, L.rectClientY}) {
      field<float>(p.values[k], y - 4) = x;
      field<float>(p.values[k], y + 4) = 40;
    }
  }
}

void patch_look(uint8_t* look) {
  auto& L = *g->menus;
  LookPatch* p = nullptr;
  for (auto& lp : lookPatches) if (lp.menu == look) p = &lp;
  if (p && field<uint8_t**>(look, L.items) == p->items) {
    show_slider_bars(*p);
    show_slider_values(*p);
    return;
  }
  static uint8_t* failed;
  if (look == failed) return;
  // a free slot, or one whose menu was unloaded or reloaded (no longer uses its items)
  if (!p) for (auto& lp : lookPatches) if (!p && (!lp.menu || field<uint8_t**>(lp.menu, L.items) != lp.items)) p = &lp;
  if (!p) {
    failed = look;
    logmsg("menu patch: no free Look slot");
    return;
  }
  auto src = field<uint8_t**>(look, L.items);
  int count = field<int>(look, L.itemCount);
  uint8_t *slider = nullptr, *sliderLabel = nullptr, *yesNo = nullptr, *yesNoLabel = nullptr, *divider = nullptr;
  float lastRow = 0;
  for (int j = 0; j < count; j++) {
    auto it = src[j];
    auto text = field<const char*>(it, L.text);
    if (field<int>(it, L.type) == ITEM_TYPE_SLIDER) slider = it;
    else if (field<int>(it, L.type) == ITEM_TYPE_YESNO && text_is(it, L.dvar, "m_filter")) yesNo = it;
    else if (text_is(it, L.text, "@MENU_MOUSE_SENSITIVITY")) sliderLabel = it;
    else if (text_is(it, L.text, "@MENU_SMOOTH_MOUSE")) yesNoLabel = it;
    if (text && !strncmp(text, "@MENU_", 6)) lastRow = std::fmax(lastRow, field<float>(it, L.rectY));
  }
  for (int j = 0; yesNoLabel && j < count; j++) {  // Look's divider, just above the Smooth Mouse row
    float d = field<float>(yesNoLabel, L.rectY) - field<float>(src[j], L.rectY);
    if (field<float>(src[j], L.rectH) < 6 && d >= 0 && d < 10) divider = src[j];
  }
  uint8_t *sliderRow[4], *yesNoRow[4];
  int sliderRowN = slider && sliderLabel ? row_items(src, count, sliderLabel, sliderRow) : 0;
  int yesNoRowN = yesNo && yesNoLabel ? row_items(src, count, yesNoLabel, yesNoRow) : 0;
  // MW3 names each Yes/No row and shows its selection bar shadow while the item with that name has focus. Its sliders
  // get no highlight at all, so there every slider row gets the Yes/No row's bar. Copies of the shadow itself draw
  // at the Smooth Mouse row whatever their rect, so the copied rows go without one.
  auto rowName = yesNo ? field<const char*>(yesNo, 0) : nullptr;
  uint8_t* shadow = nullptr;
  for (int k = 0; rowName && k < yesNoRowN; k++)
    if (yesNoRow[k] != yesNo && text_is(yesNoRow[k], 0, rowName) && field<uint8_t*>(yesNoRow[k], L.visibleExp))
      shadow = yesNoRow[k];
  int need = 1 + int(std::size(kControllerRows)) * (sliderRowN > yesNoRowN ? sliderRowN : yesNoRowN) + (shadow ? 5 : 0) + 4;
  if (!sliderRowN || !yesNoRowN || !divider || need > 64 || count + need > 112) {
    failed = look;
    logmsg("menu patch: pc_options_look items not found");
    return;
  }
  // the rows go right after Look's last row in the item list, which is also the d-pad order
  int at = 0;
  for (int j = 0; j < count; j++) if (std::fabs(field<float>(src[j], L.rectY) - lastRow) < 5) at = j + 1;
  uint8_t *added[40], *beforeSlider[1];
  int c = 0, nAdded = 0, nBefore = 0, s = 0;
  p->nBars = p->nValues = 0;
  auto copy = [&](uint8_t* from, float dy) {  // copy of from, moved down by dy
    auto it = p->copies[c++];
    memcpy(it, from, L.itemSize);
    field<float>(it, L.rectY) += dy;
    field<float>(it, L.rectClientY) += dy;
    return it;
  };
  // the Yes/No row's selection bar as a decoration at the row `y`, shown by show_slider_bars
  auto highlight = [&](uint8_t** out, int& n, const char* name, float y) {
    float dy = y - field<float>(yesNoLabel, L.rectY);
    auto bar = out[n++] = copy(yesNo, dy);
    field<int>(bar, L.type) = 0;
    field<const char*>(bar, L.text) = nullptr;
    field<uint32_t>(bar, L.staticFlags) |= WINDOW_DECORATION;
    memset(bar + L.handlers, 0, L.handlersEnd - L.handlers);
    for (int k = 0; k < 4; k++) field<float>(bar, L.backColor + 4 * k) = 1;
    field<uint8_t*>(bar, L.visibleExp) = nullptr;
    field<uint32_t>(bar, L.staticFlags + 4) &= ~WINDOW_VISIBLE;
    field<const char*>(bar, 0) = name;  // not smooth_mouse, or that row's shadow shows too
    return bar;
  };
  if (shadow) {
    p->bars[p->nBars] = highlight(beforeSlider, nBefore, field<const char*>(slider, L.dvar), field<float>(sliderLabel, L.rectY));
    p->barSliders[p->nBars++] = slider;
  }
  float y = lastRow + 40;  // the gap Look leaves before its own divider
  added[nAdded++] = copy(divider, y - field<float>(yesNoLabel, L.rectY));
  for (auto& r : kControllerRows) {
    bool isSlider = r.max > 0;
    auto label = isSlider ? sliderLabel : yesNoLabel, value = isSlider ? slider : yesNo;
    float dy = y - field<float>(label, L.rectY);
    auto bar = isSlider && shadow ? highlight(added, nAdded, r.dvar, y) : nullptr;
    for (int k = 0, rn = isSlider ? sliderRowN : yesNoRowN; k < rn; k++) {
      auto from = (isSlider ? sliderRow : yesNoRow)[k];
      if (from == shadow) continue;
      auto it = added[nAdded++] = copy(from, dy);
      if (from == label) field<const char*>(it, L.text) = r.key && find_localized(r.key + 1) ? r.key : r.label;
      if (from != value) continue;
      if (rowName) field<const char*>(it, 0) = r.dvar;  // its own row name, so no other row's bar or shadow shows
      if (bar) {
        p->bars[p->nBars] = bar;
        p->barSliders[p->nBars++] = it;
      }
      field<const char*>(it, L.dvar) = r.dvar;
      // the reset row is an action, not a setting: the engine still paints Yes/No from the dvar each frame, so its
      // textscale (0x10 before text) is 0
      if (!strcmp(r.dvar, "gpad_reset")) field<float>(it, L.text - 0x10) = 0;
      if (isSlider) {  // editFieldDef {min, max, def (IW5: step), range, ...}
        auto tpl = field<float*>(slider, L.typeData), e = p->sliders[s++];
        memcpy(e, tpl, sizeof p->sliders[0]);
        e[0] = r.min; e[1] = r.max; e[2] = L.editStep ? r.step : r.def;
        if (tpl[3] == tpl[1] - tpl[0]) e[3] = r.max - r.min;
        field<float*>(it, L.typeData) = e;
        // the value: a copy of the label without its rect expressions, placed by show_slider_values
        auto v = added[nAdded++] = copy(label, dy);
        field<int>(v, L.floatExpCount) = 0;
        // left aligned, unshifted (labels are right aligned, MW2's with textalignx -180): textAlignMode and
        // textalignx are 0x1C and 0x18 before text
        field<int>(v, L.text - 0x1C) &= ~3;
        field<float>(v, L.text - 0x18) = 0;
        p->values[p->nValues] = v;
        p->valueSliders[p->nValues] = it;
        p->last[p->nValues] = ownf(*r.var, r.def);
        p->valueRows[p->nValues++] = &r;
      }
    }
    y += 20;
  }
  int n = 0;  // decorations are drawn in list order, so the slider highlights go before their slider
  for (int j = 0; j < count; j++) {
    if (src[j] == slider) for (int k = 0; k < nBefore; k++) p->items[n++] = beforeSlider[k];
    p->items[n++] = src[j];
    if (j + 1 == at) for (int k = 0; k < nAdded; k++) p->items[n++] = added[k];
  }
  p->menu = look;
  field<uint8_t**>(look, L.items) = p->items;
  field<int>(look, L.itemCount) = n;
  logmsg("menu patched: %s +%d controller items", field<const char*>(look, L.menuName), n - count);
}

// --- console style HUD (gpad_console_hud). The console keeps the HUD inside a TV safe area and draws the four action
// slots on a d-pad next to the ammo counter. PC uses the whole screen and a row of slots with key labels
// (common.ff ui/dpad.menu: slot items "slot1".."slot4", ownerdraw 171-174, each followed by its keybinding label,
// placed by rect expressions). While on:
// - each slot's rect expressions (ItemFloatExpression targets 0-3) are switched off and its rect set on a d-pad arm;
// - the first label holds our whole d-pad glyph (dpadText): text expression off, visible always, moved to the front of
//   the menu's item list so the slots draw on top of it. The other labels move off screen;
// - slots and the d-pad fade with the ammo counter: they borrow ui/weaponinfo.menu "weaponbar_line"'s expressions,
//   `forecolor a hudfade("weapon")` and `visible when(1)`. (Stock slots never fade. Hudfade("dpad") never runs out on PC.)
// Off: everything saved is put back and the game places the items again.
constexpr int ASSET_TYPE_MENU = 0x1A;  // IW4 and IW5
// The d-pad sits in the compass, like the console (there the ring is the d-pad). The items take the compass items'
// alignment (ui/compassv2.menu "Pass1": rect -110 -79 130 130, align 10 10 = bottom right of the safe area, in a menu
// at 2 -16), so they follow it at any resolution. Ring centre: (2 - 110 + 65, -16 - 79 + 65) = (-43, -30). Item rects
// are relative to the dpad menu (exp rect X 56, Y -14), so the centre is (-99, -16) in item units.
// kDpadX/Y: top left of a slot's 28x28 box at that centre. kArm: how far out each slot sits, on the glyph's arms.
// +3/-2: measured on the Deck against the drawn ring (in these units, so it holds at any resolution).
constexpr float kSlotSize = 28, kDpadX = -99 + 3 - kSlotSize / 2, kDpadY = -16 - 2 - kSlotSize / 2, kArm = 35;  // console: items just outside the d-pad
// MW3's up slot (night vision) sits closer in: at kArm it lands on the compass's top edge. Set on the Deck.
constexpr float kArmUpMw3 = 30;
uint8_t compassAlign[2];  // horzAlign, vertAlign of the compass item (rectDef_s: x, y, w, h, then the two bytes)
// The d-pad glyph is text: at its biggest icon size it's ~3.5 lines, so its size comes from the label's textscale
// (stock 0.25: ~50 px on the Deck). Its baseline sits kDpadBaseline under the d-pad centre (set on the Deck, scales with it).
constexpr float kDpadScale = 0.3f, kDpadBaseline = 6;  // 0.3: about the art's own 64 px on the Deck
// itemDef textscale: type, dataType, alignment, fontEnum, textAlignMode, textalignx, textaligny, textscale (IW4)
int text_scale_off() { return g->menus->type + 7 * 4; }
int text_align_off() { return g->menus->type + 5 * 4; }  // textalignx, textaligny
// Slot counts: stock textalignx 6 / textaligny -10 put them right of the icon. The console has them under it
// (values checked on the Deck).
constexpr float kStockCountAlign[2] = {6, -10}, kCountAlign[2] = {-10, 0};
struct HudItem { uint8_t* item; int expCount; float x, y; uint8_t *exps, *text, *textExp, *visibleExp; bool label; float scale; uint8_t align[2]; };
HudItem hudItems[8];
int nHudItems;
uint8_t *dpadHolder, *weaponFade, *alwaysVisible;
// ItemFloatExpression {int target; Statement_s*} array. IW4 itemDef has visibleExp, disabledExp, textExp in a row.
int exps_off(int countOff) { return (countOff + 4 + P - 1) & ~(P - 1); }
int text_exp_off() { return g->menus->visibleExp + 2 * P; }

// Whole HUD menus moved while on, like the console: the stance figure leaves the compass centre (where the d-pad goes)
// for a gap between the ammo number and the ring, and the ammo block moves left to make that gap. menuDef_t starts
// with windowDef_t like itemDef, so its rect x/y sit at rectY - 4 / rectY. Original values checked before moving.
// ponytail: offsets measured on the Deck against a console screenshot. Tune with screenshots.
struct MenuMove { const char* name; float x, y, dx, dy; uint8_t* menu; bool client; };  // client: rectClient moves too
// weaponinfo listed twice: MW2 puts it at y -20, MW3 at -16 (the other entry logs "not where expected" once).
MenuMove menuMoves[] = { {"stance", -84, -76, -62, -7}, {"weaponinfo", 2, -20, -30, 0}, {"weaponinfo", 2, -16, -30, 0} };
// The stance figure (ui/stance.menu "chRect", one expression: forecolor a hudfade("stance"), which never runs out on PC)
// also takes the weapon fade while on.
uint8_t *stanceItem, *stanceExps;

// Logs a menu's rect and each item's name, rect, alignment, expressions and text placement (once per menu).
void log_menu(uint8_t* menu) {
  auto& L = *g->menus;
  auto r = [&](uint8_t* w, int y) { return std::array<float, 4>{field<float>(w, y - 4), field<float>(w, y), field<float>(w, y + 4), field<float>(w, y + 8)}; };
  auto mr = r(menu, L.menuName + L.rectY);
  logmsg("menu %s rect %g %g %g %g align %d %d, %d items", field<const char*>(menu, L.menuName), mr[0], mr[1], mr[2], mr[3],
         menu[L.menuName + L.rectY + 12], menu[L.menuName + L.rectY + 13], field<int>(menu, L.itemCount));
  for (int k = 0; k < field<int>(menu, L.itemCount); k++) {
    auto it = field<uint8_t**>(menu, L.items)[k];
    auto ir = r(it, L.rectY);
    auto exps = field<uint8_t*>(it, exps_off(L.floatExpCount));
    int n = field<int>(it, L.floatExpCount);
    char t[64] = "";
    for (int e = 0, o = 0; e < n && e < 8 && exps; e++) o += snprintf(t + o, sizeof t - o, "%d ", field<int>(exps, e * 2 * P));
    auto name = field<const char*>(it, 0);
    logmsg("  item %d %s type %d rect %g %g %g %g align %d %d exps [%s] vis %p textExp %p scale %g textalign %g %g", k,
           name ? name : "-", field<int>(it, L.type), ir[0], ir[1], ir[2], ir[3], it[L.rectY + 12], it[L.rectY + 13], t,
           field<void*>(it, L.visibleExp), field<void*>(it, text_exp_off()), field<float>(it, text_scale_off()),
           field<float>(it, text_align_off()), field<float>(it, text_align_off() + 4));
  }
}

bool find_dpad_items() {
  nHudItems = 0, dpadHolder = nullptr;
  auto& L = *g->menus;
  auto find = at<uint8_t* (*)(int, const char*, int)>(g->findAsset);
  static bool logged = false;
  if (!logged)
    for (auto n : {"dpad", "weaponinfo", "compassv2", "stance"})
      if (auto m = find(ASSET_TYPE_MENU, n, 0); m && text_is(m, L.menuName, n)) logged = true, log_menu(m);
  auto dpad = find(ASSET_TYPE_MENU, "dpad", 0), wi = find(ASSET_TYPE_MENU, "weaponinfo", 0);
  if (!dpad || !text_is(dpad, L.menuName, "dpad") || !wi || !text_is(wi, L.menuName, "weaponinfo") || field<int>(wi, L.itemCount) < 1)
    { logmsg("console hud: dpad/weaponinfo menu not found"); return false; }
  auto compass = find(ASSET_TYPE_MENU, "compassv2", 0);
  uint8_t* pass1 = nullptr;
  if (compass && text_is(compass, L.menuName, "compassv2"))
    for (int k = 0; k < field<int>(compass, L.itemCount); k++)
      if (auto it = field<uint8_t**>(compass, L.items)[k]; text_is(it, 0, "Pass1")) pass1 = it;
  if (!pass1 || !pass1[L.rectY + 12]) { logmsg("console hud: compass Pass1 not found"); return false; }
  memcpy(compassAlign, pass1 + L.rectY + 12, 2);
  auto line = field<uint8_t**>(wi, L.items)[0];
  weaponFade = field<uint8_t*>(line, exps_off(L.floatExpCount));
  alwaysVisible = field<uint8_t*>(line, L.visibleExp);
  if (field<int>(line, L.floatExpCount) != 1 || !weaponFade || field<int>(weaponFade, 0) != 8 || !alwaysVisible)  // 8: forecolor a
    { logmsg("console hud: weaponbar_line not as expected"); return false; }

  auto items = field<uint8_t**>(dpad, L.items);
  int count = field<int>(dpad, L.itemCount);
  // slot N is the item named "slotN", its label the item after it. Arms: 1 up, 2 down, 3 left, 4 right.
  static const float kDir[4][2] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
  for (int j = 0; j + 1 < count && nHudItems < 8; j++) {
    auto name = field<const char*>(items[j], 0);
    if (!name || strncmp(name, "slot", 4) || name[4] < '1' || name[4] > '4' || name[5]) continue;
    int n = name[4] - '1';
    for (int k : {j, j + 1}) {
      bool label = k != j, holder = label && !dpadHolder;
      if (holder) dpadHolder = items[k];
      float x = label ? (holder ? kDpadX + kSlotSize / 2 : -10000) : kDpadX + kDir[n][0] * kArm;
      float armY = n == 0 && g->iw5 ? kArmUpMw3 : kArm;
      float y = label ? (holder ? kDpadY + kSlotSize / 2 + kDpadBaseline : -10000) : kDpadY + kDir[n][1] * armY;
      auto it = items[k];
      hudItems[nHudItems++] = {it, field<int>(it, L.floatExpCount), x, y, field<uint8_t*>(it, exps_off(L.floatExpCount)),
                               field<uint8_t*>(it, L.text), field<uint8_t*>(it, text_exp_off()), field<uint8_t*>(it, L.visibleExp), label,
                               field<float>(it, text_scale_off()), {it[L.rectY + 12], it[L.rectY + 13]}};
    }
  }
  if (!dpadHolder) { logmsg("console hud: no dpad slots"); return false; }
  for (auto& m : menuMoves) {
    auto menu = find(ASSET_TYPE_MENU, m.name, 0);
    int y = L.menuName + L.rectY, cy = L.menuName + L.rectClientY;  // menuDef_t: windowDef_t starts at menuName
    if (menu && text_is(menu, L.menuName, m.name) && field<float>(menu, y - 4) == m.x && field<float>(menu, y) == m.y) {
      m.menu = menu;
      m.client = field<float>(menu, cy - 4) == m.x && field<float>(menu, cy) == m.y;
    }
    else logmsg("console hud: %s menu not where expected, left in place", m.name);
  }
  if (auto st = menuMoves[0].menu; st && field<int>(st, L.itemCount) > 0) {
    auto it = field<uint8_t**>(st, L.items)[0];
    if (text_is(it, 0, "chRect") && field<int>(it, L.floatExpCount) == 1) stanceItem = it, stanceExps = field<uint8_t*>(it, exps_off(L.floatExpCount));
  }
  if (!stanceItem) logmsg("console hud: stance item not as expected, it won't fade");
  if (field<float>(dpadHolder, text_scale_off()) != 0.25f) { logmsg("console hud: label textscale not where expected"); return false; }
  std::swap(items[0], *std::find(items, items + count, dpadHolder));  // draws first, under the slots
  logmsg("console hud: %d dpad items", nHudItems);
  return true;
}

// An "active" action slot (night vision on, the slot's weapon in hand) draws brightened at alpha 1, whatever the item's
// forecolor alpha, so it stayed up after the weapon fade hid everything else. While on, the store is nopped and the
// slot keeps the fade's alpha (still brightened).
void slot_alpha(bool on) {
  // x64: mov dword [rsp+0x7c], 1.0. x86 (all three builds): fld1. Fstp dword [esp+0x2c]
  static const uint8_t kStore64[8] = {0xC7, 0x44, 0x24, 0x7C, 0x00, 0x00, 0x80, 0x3F}, kStore86[6] = {0xD9, 0xE8, 0xD9, 0x5C, 0x24, 0x2C};
  const uint8_t* kStore = P == 8 ? kStore64 : kStore86;
  constexpr int n = P == 8 ? 8 : 6;
  static int state = -1;  // -1 unchecked, 0 stock, 1 nopped, 2 bytes not as expected
  if (!g->slotAlpha || state == 2 || state == on) return;
  auto p = at<uint8_t*>(g->slotAlpha);
  if (state == -1 && memcmp(p, kStore, n)) { state = 2; logmsg("console hud: slot alpha store not found"); return; }
  DWORD old;
  VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old);
  if (on) memset(p, 0x90, n); else memcpy(p, kStore, n);
  VirtualProtect(p, n, old, &old);
  FlushInstructionCache(GetCurrentProcess(), p, n);
  state = on;
}

void console_hud() {
  if (!g->safeArea[0]) return;
  bool on = ownb(gpad_console_hud, true);
  slot_alpha(on);
  float area = on ? ownf(gpad_hud_safe_area, 0.9f) : 1.0f;
  for (auto slot : g->safeArea)
    if (auto d = *at<uint8_t**>(slot)) *reinterpret_cast<float*>(d + g->dvarValue) = area;

  // retried about once a second until the HUD menus are there (they load with the level)
  static bool found = false;
  static int tries = 0;
  if (!found && (tries++ % 60 || !(found = find_dpad_items()))) return;
  auto& L = *g->menus;
  if (stanceItem) field<uint8_t*>(stanceItem, exps_off(L.floatExpCount)) = on ? weaponFade : stanceExps;
  for (auto& m : menuMoves)
    if (m.menu)
      for (int y : {L.menuName + L.rectY, L.menuName + L.rectClientY})
        if (y == L.menuName + L.rectY || m.client) field<float>(m.menu, y - 4) = m.x + on * m.dx, field<float>(m.menu, y) = m.y + on * m.dy;
  for (int k = 0; k < nHudItems; k++) {
    auto& h = hudItems[k];
    field<int>(h.item, L.floatExpCount) = on ? 1 : h.expCount;
    field<uint8_t*>(h.item, exps_off(L.floatExpCount)) = on ? weaponFade : h.exps;
    if (h.item == dpadHolder) {
      field<uint8_t*>(h.item, L.text) = on ? reinterpret_cast<uint8_t*>(dpadText) : h.text;
      field<uint8_t*>(h.item, text_exp_off()) = on ? nullptr : h.textExp;
      field<uint8_t*>(h.item, L.visibleExp) = on ? alwaysVisible : h.visibleExp;
      field<float>(h.item, text_scale_off()) = on ? kDpadScale : h.scale;
    }
    for (int y : {L.rectY, L.rectClientY}) memcpy(h.item + y + 12, on ? compassAlign : h.align, 2);
    if (!on) continue;
    for (int y : {L.rectY, L.rectClientY}) {
      field<float>(h.item, y - 4) = h.x;
      field<float>(h.item, y) = h.y;
      if (!h.label) field<float>(h.item, y + 4) = field<float>(h.item, y + 8) = kSlotSize;
    }
  }
  for (int k = 0; k < nHudItems; k++) {  // counts under the icons (only where the stock values are as expected)
    auto& h = hudItems[k];
    auto a = &field<float>(h.item, text_align_off());
    if (!h.label && ((a[0] == kStockCountAlign[0] && a[1] == kStockCountAlign[1]) || (a[0] == kCountAlign[0] && a[1] == kCountAlign[1])))
      memcpy(a, on ? kCountAlign : kStockCountAlign, sizeof kCountAlign);
  }
}

void patch_menus() {
  if (!g->uiContext) return;
  auto dc = at<uint8_t*>(g->uiContext);
  for (int i = 0, n = field<int>(dc, g->menus->menuCount); i < n; i++) {
    auto menu = field<uint8_t*>(dc, UI_MENUS + sizeof(void*) * i);
    auto name = menu ? field<const char*>(menu, g->menus->menuName) : nullptr;
    if (name && (!strcmp(name, "pc_options_look") || !strcmp(name, "pc_options_look_ingame"))) patch_look(menu);
  }
}

// --- localized strings with a keyboard key written into the text. While the pad is in use their LocalizeEntry
// {value, name} points at a copy of the stock text with the key (and its colour code) replaced, and back at the
// stock text for keyboard/mouse. Looked up every frame because the entries are reloaded with their zones (a reloaded
// entry holds the stock text again). Names missing from a game are skipped.
// The pad text is the button's key name (buttonText: an icon from the current set, or the coloured name).
struct PadString { const char* name; const char* key; int button; const char* stock; char text[96]; };
PadString padStrings[] = {
  {"PLATFORM_BACK_CAPS", "ESC", B},      // MW2 options footer "BACK - ESC"
  {"PLATFORM_BACK_SHORTCUT", "ESC", B},  // MW3 options footer "Back ESC"
};

void swap_pad_strings() {
  if (!g->findAsset) return;
  for (auto& s : padStrings) {
    auto entry = find_localized(s.name);
    if (!entry) continue;
    if (*entry != s.text) s.stock = *entry;
    // rebuilt every frame: buttonText changes with the glyph set
    auto key = strstr(s.stock, s.key);
    if (!key) continue;
    auto cut = key - s.stock >= 2 && key[-2] == '^' ? key - 2 : key;
    snprintf(s.text, sizeof s.text, "%.*s%s^7%s", int(cut - s.stock), s.stock, buttonText[s.button], key + strlen(s.key));
    *entry = pad_in_use ? s.text : s.stock;
  }
}

void hk_Com_EventLoop() {
  static bool first = true;
  if (first) {
    first = false; register_dvars(); logmsg("dvars registered");
    load_our_zone();
    make_glyphs();
#ifndef _WIN64
    if (g->drawText) hook_draw_text();
#endif
  }
#ifndef _WIN64
  if (g->q3) q3_hook_text();
#endif
  poll_pad();
  update_in_use();
  if (pad.connected) ensure_binds();
  // Console-style use: with the pad in use, +usereload has to be held to pick up/use (a tap reloads).
  // PC default g_useholdtime is 0, so a press uses at once. Keyboard/mouse keep that (like IW4x).
  // Written directly (it's a server dvar, and MW3 has no messages to confirm a "set" went through).
  static int useHold = -1;
  if (int want = pad_in_use ? 250 : 0; want != useHold && g->useHoldTime)
    if (auto d = *at<uint8_t**>(g->useHoldTime)) {
      *reinterpret_cast<int*>(d + g->dvarValue) = want;
      useHold = want;
      logmsg("g_useholdtime %d", want);
    }
  update_buttons();
#ifndef _WIN64
  if (g->q3) q3_hide_cursor(), q3_hint_keys();
  if (g->q3 && g->q3->videoFillJle) q3_video_fill();
#endif
  // The Look menu's Reset Controller Settings row sets gpad_reset: defaults as in register_dvars, through the console
  // so they are saved like a menu change. gpad_reset is cleared at once so the row flips back to No.
  if (ownb(gpad_reset, false)) {
    set_bool(gpad_reset, false);
    cbuf("set gpad_enabled 1; set gpad_invert 0; set gpad_aim_assist 1; "
        "set gpad_aim_strength 0.75; set gpad_sensitivity 1; set gpad_deadzone 0.2; set gpad_ps_glyphs 0; set gpad_console_hud 1\n");
    logmsg("controller settings reset");
  }
  patch_menus();
  console_hud();
  swap_pad_strings();
  static int glyphSet = -1;
  if (int set = ownb(gpad_ps_glyphs, false); set != glyphSet) { show_glyph_set(set); glyphSet = set; }
  at<Com_EventLoop_t>(g->eventLoop)();
}

// rel32 calls can only reach +-2GB, so route them through an absolute jump placed near the exe
// (x86: everything is in reach, call the hook directly)
void* near_jump(uintptr_t addr, void* target) {
  if (sizeof(void*) == 4) return target;
  for (uintptr_t a = (addr & ~uintptr_t(0xFFFF)) - 0x10000; a > addr - 0x7FF00000; a -= 0x10000) {
    if (auto p = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(a), 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE))) {
      p[0] = 0xFF; p[1] = 0x25; *reinterpret_cast<uint32_t*>(p + 2) = 0;  // jmp [rip+0]
      *reinterpret_cast<void**>(p + 6) = target;
      return p;
    }
  }
  return nullptr;
}

bool patch_call(uintptr_t site, uintptr_t expected, void* hook) {
  if (!site) return true;
  auto p = at<uint8_t*>(site);
  if (p[0] != 0xE8 || uintptr_t(p + 5 + *reinterpret_cast<int32_t*>(p + 1)) != expected + slide) {
    logmsg("call site %llx does not match, skipping", (unsigned long long)site);
    return false;
  }
  auto stub = near_jump(uintptr_t(p), hook);
  if (!stub) { logmsg("no memory near %llx", (unsigned long long)site); return false; }
  DWORD old;
  VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
  *reinterpret_cast<int32_t*>(p + 1) = int32_t(static_cast<uint8_t*>(stub) - (p + 5));
  VirtualProtect(p, 5, old, &old);
  FlushInstructionCache(GetCurrentProcess(), p, 5);
  return true;
}

// AlterWare hosts draw a watermark every frame from a string literal in their own part of the image (after the
// game's): iw4x-sp "AlterWare IW4x-SP", iw5-mod "AlterWare IW5-Mod MP" (cut to 17 chars). An empty string draws
// nothing. The host's section headers were overwritten by the game's, so scan memory up to the next free region.
[[maybe_unused]] void hide_watermark(uint8_t* exe) {
  static const char* const kMarks[] = { "AlterWare IW4x-SP", "AlterWare IW5-Mod MP" };
  constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                              PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  constexpr size_t kMaxScan = 0x10000000;  // ponytail: hosts are ~180 MB images. Raise if a host gets bigger
  MEMORY_BASIC_INFORMATION mbi;
  for (uint8_t* p = exe + g->imageSize; p < exe + kMaxScan && VirtualQuery(p, &mbi, sizeof mbi) && mbi.State != MEM_FREE;
       p = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize) {
    if (mbi.State != MEM_COMMIT || !(mbi.Protect & kReadable) || (mbi.Protect & PAGE_GUARD)) continue;
    auto* base = static_cast<uint8_t*>(mbi.BaseAddress);
    auto* end = base + mbi.RegionSize;
    for (auto* q = base; (q = static_cast<uint8_t*>(memchr(q, 'A', end - q))); q++)
      for (auto mark : kMarks) {
        size_t n = strlen(mark) + 1;
        if (size_t(end - q) < n || memcmp(q, mark, n) != 0) continue;
        DWORD old;
        VirtualProtect(q, 1, PAGE_READWRITE, &old);
        *q = 0;
        VirtualProtect(q, 1, old, &old);
        logmsg("watermark hidden");
        return;
      }
  }
}

DWORD WINAPI init(void*) {
  // The game is the main exe, or (under an AlterWare host like iw5-mod) an image mapped at the default base.
  uint8_t* exe = nullptr;
  IMAGE_NT_HEADERS* nt = nullptr;
  for (auto cand : { reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr)), reinterpret_cast<uint8_t*>(kBase) }) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(cand, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || *reinterpret_cast<WORD*>(cand) != IMAGE_DOS_SIGNATURE)
      continue;
    exe = cand;
    nt = reinterpret_cast<IMAGE_NT_HEADERS*>(exe + reinterpret_cast<IMAGE_DOS_HEADER*>(exe)->e_lfanew);
    for (auto& game : kGames)
      for (auto id : { std::array<DWORD, 2>{game.timestamp, game.imageSize}, {game.altIds[0][0], game.altIds[0][1]},
                       {game.altIds[1][0], game.altIds[1][1]} })
        if (id[0] && nt->FileHeader.TimeDateStamp == id[0] && nt->OptionalHeader.SizeOfImage == id[1]) g = &game;
    if (g) break;
  }
  if (!g) {
    if (!nt) { logmsg("no game image found; controller support disabled"); return 0; }
    logmsg("unsupported game build (timestamp %08lx, size %lx); controller support disabled",
        nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage);
    return 0;
  }
  slide = uintptr_t(exe) - kBase;
  logmsg("game: %s", g->name);
#ifndef _WIN64
  hide_watermark(exe);
#endif

  for (auto name : { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" })
    if (auto m = LoadLibraryW(name)) {
      xinput_get_state = reinterpret_cast<XInputGetState_t>(GetProcAddress(m, "XInputGetState"));
      if (xinput_get_state) { logmsg("using %ls", name); break; }
    }

#ifdef _WIN64
  void* hint = g->hintIsGetKeyBinding ? reinterpret_cast<void*>(hk_CL_GetKeyBinding)
                                      : reinterpret_cast<void*>(hk_Key_GetCommandAssignment);
  void* mouse = reinterpret_cast<void*>(hk_CL_MouseMove);
  void* remote = reinterpret_cast<void*>(hk_remote_move);
#else
  iwpad_remote_move = g->remoteMove + slide;
  void* remote = reinterpret_cast<void*>(g->mw3Regs ? stub_remote_move_mw3 : stub_remote_move);
  void* hint = reinterpret_cast<void*>(g->q3 ? reinterpret_cast<void*>(hk_q3_get_key_binding)
                                     : g->mw3Regs ? reinterpret_cast<void*>(stub_key_get_command_assignment_mw3)
                                                  : reinterpret_cast<void*>(stub_key_get_command_assignment));
  if (g->q3 && g->q3->refDrawText)  // pad key names become icon markers (see hk_re_draw_text)
    for (int b = 0; b <= START; b++) { buttonText[b][0] = ICON; buttonText[b][1] = char('A' + b); buttonText[b][2] = 0; }
  void* mouse = reinterpret_cast<void*>(g->q3 ? stub_mouse_move_q3 : g->mw3Regs ? stub_mouse_move_mw3 : stub_mouse_move);
#endif
  bool ok = true;
  ok &= patch_call(g->callMouseMove, g->mouseMove, mouse);
  ok &= patch_call(g->callRemoteMove, g->remoteMove, remote);
  for (auto site : g->callEventLoop) ok &= patch_call(site, g->eventLoop, reinterpret_cast<void*>(hk_Com_EventLoop));
  for (auto site : g->hintCalls) ok &= patch_call(site, g->hintFunc, hint);
  patch_key_names();
  logmsg(ok ? "hooks installed" : "some hooks failed");
  return 0;
}

}  // namespace

#ifndef _WIN64
extern "C" void iwpad_mouse_move(uint8_t* cmd, float frametime) { hk_CL_MouseMove(cmd, frametime); }
extern "C" void iwpad_remote_steer(uint8_t* cmd) { remote_steer(cmd); }
extern "C" int iwpad_key_get_command_assignment(int client, const char* cmd, int* keys) {
  return hk_Key_GetCommandAssignment(client, cmd, keys);
}
#endif

// logs crashes (code, address, module + offset). The game's own handling continues
LONG WINAPI log_crash(EXCEPTION_POINTERS* e) {
  static int n;
  auto r = e->ExceptionRecord;
  if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && n++ < 3) {
    HMODULE m = nullptr;
    char name[MAX_PATH] = "?";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(r->ExceptionAddress), &m);
    if (m) GetModuleFileNameA(m, name, sizeof name);
    logmsg("CRASH: access violation at %p (%s +0x%llx), %s %p", r->ExceptionAddress, strrchr(name, '\\') ? strrchr(name, '\\') + 1 : name,
           (unsigned long long)(uintptr_t(r->ExceptionAddress) - uintptr_t(m)), r->ExceptionInformation[0] ? "writing" : "reading",
           reinterpret_cast<void*>(r->ExceptionInformation[1]));
#ifndef _WIN64
    // stack words that point into a module (return addresses): who called the faulting code
    auto sp = reinterpret_cast<uintptr_t*>(e->ContextRecord->Esp);
    for (int i = 0, shown = 0; i < 64 && shown < 8; i++) {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(sp + i, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) break;
      HMODULE mm = nullptr;
      if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(sp[i]), &mm) || !mm)
        continue;
      char mn[MAX_PATH] = "?";
      GetModuleFileNameA(mm, mn, sizeof mn);
      logmsg("  stack+%02x: %s +0x%x", i * 4, strrchr(mn, '\\') ? strrchr(mn, '\\') + 1 : mn, unsigned(sp[i] - uintptr_t(mm)));
      shown++;
    }
#endif
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, void*) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(self);
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    // iw-pad.log next to the DLL, also for iw-pad32.dll. The previous session's log stays as iw-pad.old.log.
    while (n && path[n - 1] != L'\\' && path[n - 1] != L'/') n--;
    if (n && n + 16 < MAX_PATH) {
      wchar_t old[MAX_PATH];
      wcscpy(path + n, L"iw-pad.old.log"); wcscpy(old, path);
      wcscpy(path + n, L"iw-pad.log");
      MoveFileExW(path, old, MOVEFILE_REPLACE_EXISTING);
      g_log = _wfopen(path, L"w");
    }
    AddVectoredExceptionHandler(0, log_crash);
    logmsg("IW-Pad loaded");
    if (HANDLE t = CreateThread(nullptr, 0, init, nullptr, 0, nullptr)) CloseHandle(t);
  }
  return TRUE;
}
