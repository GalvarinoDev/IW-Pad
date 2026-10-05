// IW-Pad launcher (iw-pad.exe): starts the game exe in the same folder (iw4sp.exe for MW2, iw5sp.exe for MW3),
// passing our arguments through, and loads iw-pad.dll into it. Steam launch option:
//   MW2: bash -c 'exec "${@/iw4sp.exe/iw-pad.exe}"' -- %command%
//   MW3: bash -c 'exec "${@/iw5sp.exe/iw-pad.exe}"' -- %command%
#include <windows.h>
#include <string>

static void fail(const wchar_t* what) {
  wchar_t msg[256];
  wsprintfW(msg, L"%s failed (error %lu)", what, GetLastError());
  MessageBoxW(nullptr, msg, L"IW-Pad", MB_ICONERROR);
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
  if (exists(L"iw4x-sp.exe")) {
    exe = dir + L"iw4x-sp.exe";
    mode = L" -singleplayer";
  } else if (exists(L"iw5-mod.exe")) {
    exe = dir + L"iw5-mod.exe";
    mode = L" -singleplayer";
  } else if (!exists(L"iw4sp.exe")) {
    exe = dir + L"iw5sp.exe";
    // iw5sp.exe deletes steam_appid.txt at startup and only rewrites it (42680) when this file exists.
    // Without steam_appid.txt, a non-Steam install gets restarted through Steam and exits at once.
    std::wstring flag = dir + L"noship_create_steam_appid.txt";
    CloseHandle(CreateFileW(flag.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr));
  }

  // keep everything after our own argv[0]
  const wchar_t* args = GetCommandLineW();
  bool quoted = *args == L'"';
  for (args += quoted; *args && (quoted ? *args != L'"' : *args != L' '); args++) {}
  if (*args == L'"') args++;
  std::wstring cmd = L"\"" + exe + L"\"" + args + mode;

  STARTUPINFOW si{sizeof si};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
    fail(L"Starting the game");
    return 1;
  }

  // Inject after the game is up: the DLL only patches code that runs every frame,
  // and this avoids loader edge cases with remote threads in suspended processes under Wine.
  WaitForInputIdle(pi.hProcess, 15000);
  size_t bytes = (dll.size() + 1) * sizeof(wchar_t);
  void* mem = VirtualAllocEx(pi.hProcess, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  HANDLE th = mem && WriteProcessMemory(pi.hProcess, mem, dll.c_str(), bytes, nullptr)
    ? CreateRemoteThread(pi.hProcess, nullptr, 0, load, mem, 0, nullptr) : nullptr;
  if (!th && WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
    // e.g. steam_api restarting the game through Steam (no steam_appid.txt)
    MessageBoxW(nullptr, L"The game exited before iw-pad.dll could load. For MW3, is noship_create_steam_appid.txt next to iw5sp.exe?",
                L"IW-Pad", MB_ICONERROR);
  } else if (!th) {
    fail(L"Loading iw-pad.dll");
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
