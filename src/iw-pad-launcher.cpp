// IW-Pad launcher (iw-pad.exe): starts the game exe in the same folder (MW2, MW3, CoD1, UO or CoD2 SP), passing our
// arguments through, and loads the DLL into it. Built x64 only: a DLL must match the game's bitness, so 64-bit MW2/MW3
// get iw-pad.dll and the 32-bit games (CoD1/UO/CoD2, 32-bit MW2/MW3, AlterWare) get iw-pad32.dll.
// Steam launch option (Linux):
//   MW2: bash -c 'exec "${@/iw4sp.exe/iw-pad.exe}"' -- %command%
//   MW3: bash -c 'exec "${@/iw5sp.exe/iw-pad.exe}"' -- %command%
//   CoD2: bash -c 'exec "${@/CoD2SP_s.exe/iw-pad.exe}"' -- %command%
//   CoD UO: MESA_EXTENSION_MAX_YEAR=2003 bash -c 'exec "${@/CoDUOSP.exe/iw-pad.exe}"' -- %command%
//   CoD: MESA_EXTENSION_MAX_YEAR=2003 IWPAD_EXE=CoDSP.exe bash -c 'exec "${@/CoDSP.exe/iw-pad.exe}"' -- %command%
// IWPAD_EXE is optional and names the game exe to start from this folder, e.g. CoDSP.exe when CoDUOSP.exe is there too.
#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <string>

static void fail(const wchar_t* what) {
  wchar_t msg[256];
  wsprintfW(msg, L"%s failed (error %lu)", what, GetLastError());
  MessageBoxW(nullptr, msg, L"IW-Pad", MB_ICONERROR);
}

// LoadLibraryW inside a 32-bit (WoW64) game. Ours is in the 64-bit kernel32, so read the export table of the game's
// own 32-bit kernelbase / kernel32 (whichever exports it as code, not as a forwarder).
static uintptr_t loadlibrary32(HANDLE proc) {
  HMODULE mods[1024];
  DWORD need = 0;
  if (!EnumProcessModulesEx(proc, mods, sizeof mods, &need, LIST_MODULES_32BIT)) return 0;
  for (const wchar_t* want : {L"kernelbase.dll", L"kernel32.dll"}) {
    for (DWORD i = 0; i < need / sizeof(HMODULE) && i < 1024; i++) {
      wchar_t name[MAX_PATH];
      if (!GetModuleBaseNameW(proc, mods[i], name, MAX_PATH) || lstrcmpiW(name, want)) continue;
      uintptr_t base = reinterpret_cast<uintptr_t>(mods[i]);
      auto rd = [&](uintptr_t rva, void* out, size_t n) {
        return ReadProcessMemory(proc, reinterpret_cast<void*>(base + rva), out, n, nullptr);
      };
      IMAGE_DOS_HEADER dos;
      IMAGE_NT_HEADERS32 nt;
      IMAGE_EXPORT_DIRECTORY ex;
      if (!rd(0, &dos, sizeof dos) || !rd(dos.e_lfanew, &nt, sizeof nt)) break;
      auto dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
      if (!rd(dir.VirtualAddress, &ex, sizeof ex)) break;
      for (DWORD j = 0; j < ex.NumberOfNames; j++) {
        DWORD nameRva, fnRva;
        WORD ord;
        char fn[16] = {};
        if (!rd(ex.AddressOfNames + j * 4, &nameRva, 4) || !rd(nameRva, fn, 13) || strcmp(fn, "LoadLibraryW")) continue;
        if (!rd(ex.AddressOfNameOrdinals + j * 2, &ord, 2) || !rd(ex.AddressOfFunctions + ord * 4, &fnRva, 4)) break;
        if (fnRva - dir.VirtualAddress < dir.Size) break;  // forwarder string: try the next module
        return base + fnRva;
      }
      break;
    }
  }
  return 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  wchar_t self[MAX_PATH];
  GetModuleFileNameW(nullptr, self, MAX_PATH);
  std::wstring dir(self);
  dir.resize(dir.find_last_of(L"\\/") + 1);
  auto exists = [&](const wchar_t* name) { return GetFileAttributesW((dir + name).c_str()) != INVALID_FILE_ATTRIBUTES; };
  std::wstring exe = dir + L"iw4sp.exe", dll = dir + L"iw-pad.dll", mode;
  // 32-bit installs: prefer AlterWare's client (iw4x-sp / iw5-mod), which hosts the game exe in its own process.
  // They pick the game mode from the command line (iw5-mod otherwise shows its launcher or starts MP).
  // Both are 32-bit (iw-pad32.dll).
  if (exists(L"iw4x-sp.exe")) {
    exe = dir + L"iw4x-sp.exe";
    mode = L" -singleplayer";
  } else if (exists(L"iw5-mod.exe")) {
    exe = dir + L"iw5-mod.exe";
    mode = L" -singleplayer";
  } else if (wchar_t pick[64] = {}; GetEnvironmentVariableW(L"IWPAD_EXE", pick, 64) - 1u < 63u && !wcspbrk(pick, L"\\/:") &&
             !wcsstr(pick, L"..") && exists(pick)) {
    exe = dir + pick;  // a plain file name in this folder; anything else falls through to the checks below
  } else if (wchar_t app[16] = {}; exists(L"CoDSP.exe") && GetEnvironmentVariableW(L"SteamAppId", app, 16) && !wcscmp(app, L"2620")) {
    exe = dir + L"CoDSP.exe";  // Steam installs CoD1 (2620) and UO (2640) in one folder: Steam's app id picks the game
  } else if (exists(L"CoDUOSP.exe")) {
    // CoD United Offensive SP (never CoDUOMP.exe). Its folder also has a CoDSP.exe (CoD1 1.5): CoD1 only via app id 2620.
    exe = dir + L"CoDUOSP.exe";
  } else if (exists(L"CoDSP.exe")) {
    exe = dir + L"CoDSP.exe";  // CoD1 SP (never CoDMP.exe)
  } else if (exists(L"CoD2SP_s.exe")) {
    exe = dir + L"CoD2SP_s.exe";  // singleplayer only: never CoD2MP_s.exe (CoD2x / PunkBuster live there)
  } else {
    if (!exists(L"iw4sp.exe")) exe = dir + L"iw5sp.exe";
    // iw4sp.exe and iw5sp.exe delete steam_appid.txt at startup and only rewrite it (10180 / 42680) when this file
    // exists. Without steam_appid.txt, a game started outside Steam (Windows: iw-pad.exe run directly) is restarted
    // through Steam: the process we loaded iw-pad.dll into exits and the restarted game runs without it.
    std::wstring flag = dir + L"noship_create_steam_appid.txt";
    CloseHandle(CreateFileW(flag.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr));
  }

  // keep everything after our own argv[0]
  const wchar_t* args = GetCommandLineW();
  bool quoted = *args == L'"';
  for (args += quoted; *args && (quoted ? *args != L'"' : *args != L' '); args++) {}
  if (*args == L'"') args++;
  std::wstring cmd = L"\"" + exe + L"\"" + args + mode;

  DWORD type = SCS_64BIT_BINARY;
  bool x86 = GetBinaryTypeW(exe.c_str(), &type) && type == SCS_32BIT_BINARY;
  if (x86) dll = dir + L"iw-pad32.dll";


  STARTUPINFOW si{sizeof si};
  PROCESS_INFORMATION pi{};
  // r_vidModes: UO 0x57b4a8, CoD1 1.1 0x563a28, CoD1 1.5 0x564a28 (same 13 modes). Each candidate is checked against
  // the stock entries before writing, so only the one matching this build gets patched.
  std::wstring lower = exe;
  CharLowerW(lower.data());  // IWPAD_EXE may be spelled in any case
  const bool uo = lower.ends_with(L"coduosp.exe"), cod1 = !uo && lower.ends_with(L"codsp.exe");
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, uo || cod1 ? CREATE_SUSPENDED : 0, nullptr, dir.c_str(), &si, &pi)) {
    fail(L"Starting the game");
    return 1;
  }
  if (uo || cod1) {
    // CoD1 / UO list only 4:3 video modes. Unused ones (r_mode 0, 1, 2, 5) become 16:9 / 16:10 for ui/options_graphics.menu.
    // Done before R_Init reads the table at startup, long before iw-pad.dll is loaded (a vid_restart afterwards cuts off
    // the intro videos). r_vidModes: {char* name; int w, h; float aspect}[13].
    struct Mode { uint32_t name; int w, h; float aspect; };
    const Mode stock[] = {{0, 320, 240, 1}, {0, 400, 300, 1}, {0, 512, 384, 1}, {0, 960, 720, 1}};
    const Mode wide[] = {{0, 1280, 720, 1}, {0, 1280, 800, 1}, {0, 1920, 1080, 1}, {0, 1600, 900, 1}};
    const int slot[] = {0, 1, 2, 5};
    static const char names[] = "1280x720 (16:9)\0" "1280x800 (16:10)\0" "1920x1080 (16:9)\0" "1600x900 (16:9)";
    const int nameAt[] = {0, 16, 33, 50};
    const uintptr_t cands[] = {uo ? 0x57b4a8u : 0x563a28u, uo ? 0u : 0x564a28u};
    auto patch = [&] {
      for (auto modes : cands) {
        auto table = reinterpret_cast<Mode*>(modes);
        Mode cur[4];
        bool ok = modes != 0;
        for (int i = 0; ok && i < 4; i++)
          ok = ReadProcessMemory(pi.hProcess, &table[slot[i]], &cur[i], sizeof(Mode), nullptr) &&
               cur[i].w == stock[i].w && cur[i].h == stock[i].h;
        auto str = static_cast<char*>(ok ? VirtualAllocEx(pi.hProcess, nullptr, sizeof names, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) : nullptr);
        if (!str || !WriteProcessMemory(pi.hProcess, str, names, sizeof names, nullptr)) continue;
        DWORD old;
        VirtualProtectEx(pi.hProcess, table, 13 * sizeof(Mode), PAGE_READWRITE, &old);
        for (int i = 0; i < 4; i++) {
          Mode m = wide[i];
          m.name = uint32_t(reinterpret_cast<uintptr_t>(str + nameAt[i]));
          WriteProcessMemory(pi.hProcess, &table[slot[i]], &m, sizeof m, nullptr);
        }
        VirtualProtectEx(pi.hProcess, table, 13 * sizeof(Mode), old, &old);
        return true;
      }
      return false;
    };
    bool patched = patch();
    ResumeThread(pi.hThread);
    // Steam exes are packed: the table only exists once the unpacker has run, which is well before R_Init reads it.
    // ponytail: 1 ms polling race against R_Init. Fine in practice (unpack -> R_Init is hundreds of ms)
    for (int ms = 0; !patched && ms < 5000 && WaitForSingleObject(pi.hProcess, 1) == WAIT_TIMEOUT; ms++) patched = patch();
  }

  // Inject after the game is up: the DLL only patches code that runs every frame,
  // and this avoids loader edge cases with remote threads in suspended processes under Wine.
  WaitForInputIdle(pi.hProcess, 15000);
  size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
  void* mem = VirtualAllocEx(pi.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      x86 ? loadlibrary32(pi.hProcess) : reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")));
  HANDLE th = load && mem && WriteProcessMemory(pi.hProcess, mem, dll.c_str(), bytes, nullptr)
    ? CreateRemoteThread(pi.hProcess, nullptr, 0, load, mem, 0, nullptr) : nullptr;
  if (!th && WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
    // e.g. steam_api restarting the game through Steam (no steam_appid.txt)
    MessageBoxW(nullptr, L"The game exited before iw-pad.dll could load. Is noship_create_steam_appid.txt next to the game exe?",
                L"IW-Pad", MB_ICONERROR);
  } else if (!th) {
    fail(x86 ? L"Loading iw-pad32.dll" : L"Loading iw-pad.dll");
  } else {
    WaitForSingleObject(th, 15000);
    CloseHandle(th);
  }

  // stay alive so Steam keeps tracking the game session
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(pi.hProcess, &code);
  return int(code);
}
