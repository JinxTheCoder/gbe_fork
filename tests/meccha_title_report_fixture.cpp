#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>

bool opened = false;
bool reported = false;
unsigned ticks = 0;
HANDLE ready_event = nullptr;

BOOL CALLBACK find_dialog(HWND hwnd, LPARAM)
{
    wchar_t name[256]{};
    GetClassNameW(hwnd, name, 256);
    if (std::wstring(name) != L"GBE_MECCHA_WORKSHOP_MESSAGE_V15") return TRUE;
    opened = true;
    if (ready_event) SetEvent(ready_event);
    reported = std::filesystem::exists("meccha_title_runtime_report.txt");
    if (reported || ticks > 200) PostMessageW(hwnd, WM_CLOSE, 0, 0);
    return FALSE;
}

void CALLBACK tick(HWND, UINT, UINT_PTR, DWORD)
{
    ++ticks;
    EnumThreadWindows(GetCurrentThreadId(), find_dialog, 0);
}

int wmain(int argc, wchar_t **argv)
{
    if (argc != 2) return 2;
    ready_event = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\MECCHA_Title_Test_Ready");
    std::ofstream("steam_appid.txt") << "4704690";
    SetEnvironmentVariableW(L"SteamAppId", L"4704690");
    SetEnvironmentVariableW(L"SteamGameId", L"4704690");
    const auto path = std::filesystem::absolute(argv[1]);
    HMODULE dll = LoadLibraryW(path.c_str());
    if (!dll) return 3;
    auto init = reinterpret_cast<bool (__cdecl *)()>(GetProcAddress(dll, "SteamAPI_Init"));
    auto ugc = reinterpret_cast<void *(__cdecl *)()>(GetProcAddress(dll, "SteamAPI_SteamUGC_v021"));
    auto download = reinterpret_cast<bool (__cdecl *)(void *, unsigned long long, bool)>(GetProcAddress(dll, "SteamAPI_ISteamUGC_DownloadItem"));
    auto shutdown = reinterpret_cast<void (__cdecl *)()>(GetProcAddress(dll, "SteamAPI_Shutdown"));
    if (!init || !ugc || !download || !shutdown || !init()) return 4;
    void *api = ugc();
    if (!api) return 5;
    UINT_PTR timer = SetTimer(nullptr, 0, 200, tick);
    if (!timer) return 6;
    download(api, 7, false);
    KillTimer(nullptr, timer);
    shutdown();
    if (ready_event) CloseHandle(ready_event);
    return opened && reported ? 0 : 1;
}
