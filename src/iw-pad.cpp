// IW-Pad (iw-pad.dll): XInput controller + console-style aim assist for Steam MW2 SP x64 (iw4sp.exe) and
// MW3 SP x64 (iw5sp.exe); the x86 build of this DLL covers MW2 and MW3 SP x86. Each supported build is identified
// by its PE timestamp and image size; on any other build the DLL logs and does nothing, so the game runs vanilla.
#include <windows.h>
#include <xinput.h>
#include <mmsystem.h>
#include <d3d9.h>
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
extern uintptr_t iwpad_draw_text_resume;
uintptr_t iwpad_draw_text_resume;
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

// Aim assist dvars. MW2 still registers them (we read the engine's dvar_t* slots); MW3 stripped
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
  int floatExpCount;  // itemDef floatExpressionCount (rect expressions); IW5 drops imageTrack before it
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
  int wdAutoAimRange;                              // WeaponDef autoAimRange; aimAssistRange(+4) and Ads(+8) follow
  uintptr_t keyCatchers, viewPitch, viewYaw, frameMsec, snapFlags, noLookFlags;
  int cmdForward;                                  // usercmd forwardmove offset, rightmove follows
  uintptr_t keyBindIdx, keyBindStr; int keyStride; // keys[k] bind command index (0: none) / bind string
  uintptr_t bindCommands, localizedKeyNames;
  uintptr_t hintCalls[3], hintFunc;                // MW2: Key_GetCommandAssignment call; MW3: CL_GetKeyBinding calls
  bool hintIsGetKeyBinding;
  uintptr_t aaGlob, aaGraphs;
  int aaShift, aaAutoMeleeState, aaLockOnTarget;   // MW3 dropped the 2 autoaim region floats: later fields move -8
  const uintptr_t* aimDvarSlots;
  bool mw3Regs;                                    // x86 usercall registers: false = MW2's, true = MW3's
  uintptr_t drawText;                              // R_AddCmdDrawText, hooked to drop iw5-mod's watermark (0: none)
  int dvarValue = 0x10;                            // dvar_t current value offset (MW3 x86: 0xC)
  uintptr_t useHoldTime = 0;                       // g_useholdtime dvar_t* slot (Player_UpdateActivate hold time)
  uintptr_t zoneLoad = 0;                          // DB_LoadXAssets(XZoneInfo*, count, sync); 0: no iwpad zone
  uintptr_t uiContext = 0;                         // UiContext (Menus[], menuCount); 0: stock menus left alone
  uintptr_t findAsset = 0;                         // DB_FindXAssetHeader(type, name); 0: stock strings left alone
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
  // Ported from the entry above by masked byte signatures (ghidra/mw2-aw.gpr); same code, other addresses.
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
    // same call); R_TextWidth 0x4e9350 (4 args, readChar 0x4db3e0) has no icon case, so it's replaced
    true, 0, {0x4e9350, 0}, 0x4db3e0, 0x690594, 0x52f000, {0xb4dc8c, 0xb4dc10}, true, 0x50090b },
#endif
};
const Game* g;

constexpr int K_ENTER = 13, K_ESCAPE = 27, K_UP = 154, K_DOWN = 155, K_LEFT = 156, K_RIGHT = 157, K_MOUSE1 = 200,
  K_MOUSE2 = 201, K_AUX1 = 207;
constexpr int KEYCATCH_CONSOLE = 0x1, KEYCATCH_UI = 0x10;
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
  // MW3 has no autoaim region; derive it like MW2 does (160x120 vs 90x90 slowdown, scaled by fovScaleInv)
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
bool dvb(AimDvar d, bool def) { auto p = aim_dvar(d); return p ? *(p + g->dvarValue) != 0 : def; }
int dvi(AimDvar d, int def) { auto p = aim_dvar(d); return p ? *reinterpret_cast<int*>(p + g->dvarValue) : def; }
float ownf(uint8_t* p, float def) { return p ? *reinterpret_cast<float*>(p + g->dvarValue) : def; }
bool ownb(uint8_t* p, bool def) { return p ? *(p + g->dvarValue) != 0 : def; }

uint8_t *gpad_console_hud, *gpad_hud_safe_area, *gpad_ps_glyphs, *gpad_in_use, *gpad_reset, *gpad_enabled, *gpad_sensitivity, *gpad_invert, *gpad_deadzone, *gpad_aim_assist, *gpad_aim_strength, *gpad_aim_range, *gpad_autoaim_range;

void register_dvars() {
  auto rb = at<Dvar_RegisterBool_t>(g->dvarBool);
  auto rf = at<Dvar_RegisterFloat_t>(g->dvarFloat);
  constexpr unsigned ARCHIVE = 1;
  gpad_enabled = rb("gpad_enabled", true, ARCHIVE, "Enable controller input");
  gpad_in_use = rb("gpad_in_use", false, 0, "The controller was used last (keyboard or mouse buttons clear it)");
  gpad_reset = rb("gpad_reset", false, 0, "Set to reset the controller settings to their defaults (Look menu row)");
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
  // anymore (offset unknown); map BG_GetWeaponDef + the range fields if per-weapon ranges matter.
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
  if (b < START) return kPadKey[b];
  return 0;  // left stick directions move the player instead
}

struct BtnState { int sentKey; int nextRepeat; } btn[BTN_COUNT];

int now_ms() { return at<Sys_Milliseconds_t>(g->sysMilliseconds)(); }
void send_key(int key, bool down) { at<CL_KeyEvent_t>(g->keyEvent)(0, key, down, now_ms()); }

// keys[k] = { int down; int repeats; binding }: down sits 8 bytes before the binding field (all 5 builds' Key_IsDown)
void set_key_down(int k, bool d) { *at<int*>((g->keyBindIdx ? g->keyBindIdx : g->keyBindStr) - 8 + k * g->keyStride) = d; }

void update_buttons() {
  int catchers = *at<int*>(g->keyCatchers);
  bool console = catchers & KEYCATCH_CONSOLE, menu = catchers & KEYCATCH_UI;
  int now = now_ms();
  for (int b = 0; b < BTN_COUNT; b++) {
    auto& s = btn[b];
    bool down = !console && ownb(gpad_enabled, true) && btn_down(b);
    int want = down ? (menu ? menu_key(b) : game_key(b)) : 0;
    if (s.sentKey && s.sentKey != want) {
      send_key(s.sentKey, false);
      if (b == LT || b == RT) set_key_down(b == LT ? K_MOUSE1 : K_MOUSE2, false);
      s.sentKey = 0;
    }
    if (!want) continue;
    if (!s.sentKey) {
      send_key(want, true); s.sentKey = want; s.nextRepeat = now + 400;
      // PC scripts read the mouse buttons raw (MW2 _climb.gsc ice picks, af_chase knife crawl: mouse1 = left
      // arm, mouse2 = right arm), so mirror LT/RT into mouse1/mouse2 without firing the mouse binds.
      if (b == LT || b == RT) set_key_down(b == LT ? K_MOUSE1 : K_MOUSE2, true);
    } else if (menu && (want == K_UP || want == K_DOWN || want == K_LEFT || want == K_RIGHT) && now >= s.nextRepeat) {
      send_key(want, false); send_key(want, true); s.nextRepeat = now + 120;
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
  if (gpad_in_use) *(gpad_in_use + g->dvarValue) = inUse;
  logmsg(inUse ? "input: controller" : "input: keyboard/mouse");
}

// keys[k] holds an index into the bind command table. MW2 also allows free-form bind strings when
// the index is 0; MW3 only has the fixed table.
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
  for (int k : kPadKey)
    if (binding_of(k)) return;
  char cmd[64];
  for (int i = 0; i < int(std::size(kDefaultBinds)); i++) {
    snprintf(cmd, sizeof cmd, "bind %s \"%s\"\n", kPadKeyName[i], kDefaultBinds[i]);
    at<Cbuf_AddText_t>(g->cbuf)(0, cmd);
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

// WeaponDef range fields, as read by AimAssist_DrawDebugOverlay
float assist_range(const Aim& aa) {
  float scale = dvf(RANGE_SCALE, 1.0f);
  if (!g->getWeaponDef) return ownf(gpad_aim_range, 3200.0f) * scale;
  auto wd = at<BG_GetWeaponDef_t>(g->getWeaponDef)(aa.weapon());
  if (!wd) return 0;
  auto r = reinterpret_cast<float*>(wd + g->wdAutoAimRange);
  return lerp(r[1], r[2], aa.adsLerp()) * scale;
}
float auto_aim_range(const Aim& aa) {
  float scale = dvf(AUTO_RANGE_SCALE, 1.0f);
  if (!g->getWeaponDef) return ownf(gpad_autoaim_range, 1200.0f) * scale;
  auto wd = at<BG_GetWeaponDef_t>(g->getWeaponDef)(aa.weapon());
  return wd ? *reinterpret_cast<float*>(wd + g->wdAutoAimRange) * scale : 0;
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
  if (assist && dvb(SLOW_ENABLED, true) && best_target(aa, assist_range(aa), aa.slowW(), aa.slowH())) {
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
  if (dvb(AUTO_ENABLED, true) && (buttons & CMD_BUTTON_ADS) && aa.adsLerp() > 0 && !reloading) {
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
// Key_KeynumToString(key, translate=1) returns these from the localized key name table; the hint code
// falls back to the raw text when it's not a localization key. They start as coloured button names; where the
// build supports text icons, make_glyphs() rewrites them in place into icons.
char buttonText[START + 1][16] = {
  "^2A^7", "^1B^7", "^5X^7", "^3Y^7", "LB", "RB", "LT", "RT", "L3", "R3",
  "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Back", "Start",
};

// IW4 text draws "^" 0x01 w h <Material*> as an inline icon; w/h are (size-16)/32 font heights, so 0x30 = one line.
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
// structs; IW5 (OpenAssetTools IW5_Assets.h) has stateBitsEntry[54] instead of 48 and a subMaterials pointer at the end.
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

// x64: `mov rcx,[r8+3]; cvtdq2ps xmm6,xmm6` (7 bytes) -> jmp to a stub that does the same, but zero-extends the
// low half when the high half is our marker. Flags it clobbers are rewritten by the next `add` before any use.
bool patch_icon_read() {
  if (P == 4) return true;
  if (!g->iconRead) return false;
  auto p = at<uint8_t*>(g->iconRead);
  static const uint8_t kOrig[] = { 0x49, 0x8B, 0x48, 0x03, 0x0F, 0x5B, 0xF6 };
  if (memcmp(p, kOrig, sizeof kOrig)) { logmsg("glyphs: icon read does not match"); return false; }
  auto stub = static_cast<uint8_t*>(near_jump(uintptr_t(p), nullptr));  // a page near the exe; its jmp is overwritten
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
  memcpy(p, jmp, sizeof jmp);  // ponytail: not atomic vs the render thread; a torn read needs a draw in these ns
  VirtualProtect(p, sizeof jmp, old, &old);
  FlushInstructionCache(GetCurrentProcess(), p, sizeof jmp);
  return true;
}

// MW2 x64 R_TextWidth(text, maxChars, font) measures "^\x01..." as 14 letters, so text after an icon (the weapon
// name after "Press X to pick up") moves right. This is the same function (0x140009b30) plus the icon case the
// draw and word-wrap code have. Font: pixelHeight +8, glyphCount +0xC, glyphs +0x20 ({u16 letter; .. u8 dx at +4}, 0x18).
// MW3 x64 has two such functions (0x1401b80f0 and 0x1401b8020); IW5 colour codes go up to ^; and its icons are
// named: "^" 1|2 w h len name[len].
using ReadChar_t = unsigned (*)(const char** text, int, int);  // MW2's takes 2 arguments; the 3rd is ignored there
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

// IW5 text draw: Material_RegisterHandle(name) for an icon; our names "iwpad00".."iwpad31" are glyphs[0..31].
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

    // one 0x100 slot per glyph at +0x1010: material, texture def, image; address bytes 10, 10+i, base, base
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

// Points the localized key names at one glyph set; called again whenever gpad_ps_glyphs changes.
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
    if (k < K_AUX1 || k > K_AUX1 + START) continue;
    int b = k - K_AUX1;
    DWORD old;
    VirtualProtect(&table[i * 2], 2 * sizeof(void*), PAGE_READWRITE, &old);
    table[i * 2] = localized ? buttonText[b] : kPadKeyName[b];
    k = kPadKey[b];
    VirtualProtect(&table[i * 2], 2 * sizeof(void*), old, &old);
  }
}

void patch_key_names() {
  auto localized = at<const char**>(g->localizedKeyNames);
  auto names = localized - 96 * 2;
  if (strcmp(names[0], "TAB") || strcmp(localized[0], "KEY_TAB")) { logmsg("key name table not found, pad keys unnamed"); return; }
  rename_aux(names, false);
  rename_aux(localized, true);
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
  {"+throw", "+frag"},
};

int pad_keys_for(const char* cmd, int* keys) {
  int n = 0;
  for (int k : kPadKey)
    if (auto b = binding_of(k); n < 2 && b && !_stricmp(b, cmd)) keys[n++] = k;
  return n;
}

// Returns the controller keys for a hint command (0 when the pad isn't in use or nothing is bound).
int hint_pad_keys(const char* cmd, int* keys) {
  static char seen[64][32];  // log each distinct command a hint asks for, to spot missing equivalents
  if (!cmd) return 0;
  int i = 0;
  while (i < 64 && seen[i][0] && strcmp(seen[i], cmd)) i++;
  if (i < 64 && !seen[i][0]) { snprintf(seen[i], sizeof seen[i], "%s", cmd); logmsg("hint asks for %s", cmd); }
  if (!pad_in_use) return 0;
  if (int n = pad_keys_for(cmd, keys)) return n;
  for (auto& e : kConsoleEquivalent)
    if (!_stricmp(cmd, e[0]))
      if (int n = pad_keys_for(e[1], keys)) return n;
  return 0;
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

// --- hooks
void hk_CL_MouseMove(uint8_t* cmd, float frametime) {
  // mouse/gyro look + engine aim assist bookkeeping
#ifdef _WIN64
  at<CL_MouseMove_t>(g->mouseMove)(cmd, frametime);
#else
  (g->mw3Regs ? call_mouse_move_mw3 : call_mouse_move)(g->mouseMove + slide, cmd, frametime);
#endif
  if (!pad.connected || !ownb(gpad_enabled, true) || (*at<int*>(g->keyCatchers) & (KEYCATCH_UI | KEYCATCH_CONSOLE)))
    return;

  auto clamp_move = [](int v) { return v > 127 ? 127 : v < -127 ? -127 : v; };
  auto fwd = reinterpret_cast<int8_t*>(cmd + g->cmdForward), right = fwd + 1;
  *fwd = int8_t(clamp_move(*fwd + int(std::lround(pad.ly * 127))));
  *right = int8_t(clamp_move(*right + int(std::lround(pad.lx * 127))));

  // MW3 vehicles steered by script (Hunter Killer DPV) read getnormalizedcameramovement: usercmd bytes cmdForward + 4
  // (pitch, + = down) and + 5 (yaw, + = right). CL_MouseMove fills them from the mouse while snapFlags+4 has 0x40000
  // or 0x10000; mirror that with the right stick.
  if (g->cameraMove && (at<unsigned*>(g->snapFlags)[1] & 0x50000)) {
    float down = ownb(gpad_invert, false) ? pad.ry : -pad.ry;
    fwd[4] = int8_t(clamp_move(fwd[4] + int(std::lround(down * 127))));
    fwd[5] = int8_t(clamp_move(fwd[5] + int(std::lround(pad.rx * 127))));
  }

  // CL_MouseMove skips look on these conditions too
  if (*at<int*>(g->frameMsec) == 0 || (*at<unsigned*>(g->snapFlags) & 0x800)) return;
  if (g->noLookFlags && (*at<unsigned*>(g->noLookFlags) & 1)) return;
  float& pitch = *at<float*>(g->viewPitch);
  float& yaw = *at<float*>(g->viewYaw);
  apply_look(Aim{at<uint8_t*>(g->aaGlob)}, *reinterpret_cast<int*>(cmd + 4), frametime, pitch, yaw);
}

// While a Predator is steered, CL_CreateCmd calls the remote control move instead of CL_MouseMove. It reads only the
// mouse into usercmd.remoteControlAngles (pitch, yaw: -127..127, mouse right/up is negative), at cmdForward + 0x22 in
// every build. Either stick steers; the player can't walk meanwhile.
void remote_steer(uint8_t* cmd) {
  if (!pad.connected || !ownb(gpad_enabled, true) || (*at<int*>(g->keyCatchers) & (KEYCATCH_UI | KEYCATCH_CONSOLE)))
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
// original; asset types with a pool size > 1), so its menus override the stock ones even after the UI loaded them.
struct XZoneInfo { const char* name; int allocFlags, freeFlags; };
using DB_LoadXAssets_t = void (*)(XZoneInfo* zones, unsigned count, int sync);
void load_our_zone() {
  if (!g->zoneLoad || GetFileAttributesA("zone\\english\\iwpad.ff") == INVALID_FILE_ATTRIBUTES) return;
  XZoneInfo zone = { "iwpad", 1, 0 };  // allocFlags 1 like common: stays loaded across levels; frees nothing
  at<DB_LoadXAssets_t>(g->zoneLoad)(&zone, 1, 1);
  logmsg("zone iwpad loaded");
}

// --- stock menu patches. The UI keeps the loaded menuDef_t pointers in UiContext.Menus; they are checked every
// frame, so menus reloaded with ui.ff get patched again. Offsets shared by both engines (x64); the rest is in
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
// rates like gpad_sensitivity. MW3's own table (x64 0x1403f75b0, profile_menuDvarsSetup; profileMenuOption_sensitivity
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

// A row is every item level with its label (MW2: value + label; MW3 adds a selection bar shadow), minus the
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
// Look are separate menus that both stay loaded, so each gets its own items array; the copies only point into that
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
      // a press (MW2: range / 40, MW3: r.step) is less than the closest steps' gap: the next step that way; a bigger
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
      // one whole step that way instead; a bigger jump (mouse drag) rounds to the nearest step.
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
    // text; the Look sliders are left aligned) inside a row-wide rect; MW3's rect is the bar, set by its rect
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
  // MW3 names each Yes/No row and shows its selection bar shadow while the item with that name has focus; its sliders
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
// slots on a d-pad next to the ammo counter; PC uses the whole screen and a row of slots with key labels
// (common.ff ui/dpad.menu: slot items "slot1".."slot4", ownerdraw 171-174, each followed by its keybinding label,
// placed by rect expressions). While on:
// - each slot's rect expressions (ItemFloatExpression targets 0-3) are switched off and its rect set on a d-pad arm;
// - the first label holds our whole d-pad glyph (dpadText): text expression off, visible always, moved to the front of
//   the menu's item list so the slots draw on top of it. The other labels move off screen;
// - slots and the d-pad fade with the ammo counter: they borrow ui/weaponinfo.menu "weaponbar_line"'s expressions,
//   `forecolor a hudfade("weapon")` and `visible when(1)`. (Stock slots never fade; hudfade("dpad") never runs out on PC.)
// Off: everything saved is put back and the game places the items again.
constexpr int ASSET_TYPE_MENU = 0x1A;  // IW4 and IW5
// The d-pad sits in the compass, like the console (there the ring is the d-pad). The items take the compass items'
// alignment (ui/compassv2.menu "Pass1": rect -110 -79 130 130, align 10 10 = bottom right of the safe area, in a menu
// at 2 -16), so they follow it at any resolution. Ring centre: (2 - 110 + 65, -16 - 79 + 65) = (-43, -30); item rects
// are relative to the dpad menu (exp rect X 56, Y -14), so the centre is (-99, -16) in item units.
// kDpadX/Y: top left of a slot's 28x28 box at that centre; kArm: how far out each slot sits, on the glyph's arms.
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
// Slot counts: stock textalignx 6 / textaligny -10 put them right of the icon; the console has them under it
// (values checked on the Deck).
constexpr float kStockCountAlign[2] = {6, -10}, kCountAlign[2] = {-10, 0};
struct HudItem { uint8_t* item; int expCount; float x, y; uint8_t *exps, *text, *textExp, *visibleExp; bool label; float scale; uint8_t align[2]; };
HudItem hudItems[8];
int nHudItems;
uint8_t *dpadHolder, *weaponFade, *alwaysVisible;
// ItemFloatExpression {int target; Statement_s*} array; IW4 itemDef has visibleExp, disabledExp, textExp in a row.
int exps_off(int countOff) { return (countOff + 4 + P - 1) & ~(P - 1); }
int text_exp_off() { return g->menus->visibleExp + 2 * P; }

// Whole HUD menus moved while on, like the console: the stance figure leaves the compass centre (where the d-pad goes)
// for a gap between the ammo number and the ring, and the ammo block moves left to make that gap. menuDef_t starts
// with windowDef_t like itemDef, so its rect x/y sit at rectY - 4 / rectY. Original values checked before moving.
// ponytail: offsets measured on the Deck against a console screenshot; tune with screenshots.
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
  // x64: mov dword [rsp+0x7c], 1.0; x86 (all three builds): fld1; fstp dword [esp+0x2c]
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
  poll_pad();
  update_in_use();
  if (pad.connected) ensure_binds();
  // Console-style use: with the pad in use, +usereload has to be held to pick up/use (a tap reloads).
  // PC default g_useholdtime is 0, so a press uses at once; keyboard/mouse keep that (like IW4x).
  // Written directly (it's a server dvar, and MW3 has no messages to confirm a "set" went through).
  static int useHold = -1;
  if (int want = pad_in_use ? 250 : 0; want != useHold)
    if (auto d = *at<uint8_t**>(g->useHoldTime)) {
      *reinterpret_cast<int*>(d + g->dvarValue) = want;
      useHold = want;
      logmsg("g_useholdtime %d", want);
    }
  update_buttons();
  // The Look menu's Reset Controller Settings row sets gpad_reset: defaults as in register_dvars, through the console
  // so they are saved like a menu change; gpad_reset is cleared at once so the row flips back to No.
  if (ownb(gpad_reset, false)) {
    *(gpad_reset + g->dvarValue) = 0;
    at<Cbuf_AddText_t>(g->cbuf)(0, "set gpad_enabled 1; set gpad_invert 0; set gpad_aim_assist 1; "
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
  constexpr size_t kMaxScan = 0x10000000;  // ponytail: hosts are ~180 MB images; raise if a host gets bigger
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
      if (nt->FileHeader.TimeDateStamp == game.timestamp && nt->OptionalHeader.SizeOfImage == game.imageSize) g = &game;
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
  void* hint = reinterpret_cast<void*>(g->mw3Regs ? stub_key_get_command_assignment_mw3 : stub_key_get_command_assignment);
  void* mouse = reinterpret_cast<void*>(g->mw3Regs ? stub_mouse_move_mw3 : stub_mouse_move);
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

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, void*) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(self);
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (n > 4) { wcscpy(path + n - 4, L".log"); g_log = _wfopen(path, L"w"); }
    logmsg("IW-Pad loaded");
    if (HANDLE t = CreateThread(nullptr, 0, init, nullptr, 0, nullptr)) CloseHandle(t);
  }
  return TRUE;
}
