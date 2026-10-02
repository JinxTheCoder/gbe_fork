#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

bool found = false;
bool passed = false;

bool capture(HWND hwnd)
{
    RECT rect{};
    GetWindowRect(hwnd, &rect);
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    HDC screen = GetWindowDC(hwnd);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ old = SelectObject(dc, bitmap);
    const BOOL rendered = PrintWindow(hwnd, dc, 0);
    SelectObject(dc, old);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    const bool read = GetDIBits(screen, bitmap, 0, height, pixels.data(), &info, DIB_RGB_COLORS) != 0;
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(hwnd, screen);
    if (!rendered || !read) return false;
    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
    std::ofstream out("meccha_compiled_dll_caption.bmp", std::ios::binary);
    out.write(reinterpret_cast<const char *>(&header), sizeof(header));
    out.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof(BITMAPINFOHEADER));
    out.write(reinterpret_cast<const char *>(pixels.data()), pixels.size());
    return static_cast<bool>(out);
}

BOOL CALLBACK inspect(HWND hwnd, LPARAM)
{
    wchar_t name[160]{};
    GetClassNameW(hwnd, name, 160);
    if (std::wstring(name).find(L"GBE_MECCHA_WORKSHOP_MESSAGE") != 0) return TRUE;
    found = true;
    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, 512);
    const std::wstring expected = L"MECCHA Workshop - Missing Map -J\u0268n\u03C7";
    passed = std::wstring(title) == expected &&
             std::wstring(name) == L"GBE_MECCHA_WORKSHOP_MESSAGE_V15" &&
             (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CAPTION) != WS_CAPTION &&
             GetDlgItem(hwnd, 6206) != nullptr;
    std::wcerr << L"COMPILED DLL CAPTION: [" << title << L"]\n";
    if (!capture(hwnd)) {
        std::cerr << "Window capture failed\n";
        passed = false;
    }
    PostMessageW(GetDlgItem(hwnd, 6206), BM_CLICK, 0, 0);
    return FALSE;
}

void CALLBACK tick(HWND, UINT, UINT_PTR, DWORD)
{
    EnumThreadWindows(GetCurrentThreadId(), inspect, 0);
}

int wmain(int argc, wchar_t **argv)
{
    if (argc != 2) return 2;
    const auto dll_path = std::filesystem::absolute(argv[1]);
    std::ofstream("steam_appid.txt") << "4704690";
    SetEnvironmentVariableW(L"SteamAppId", L"4704690");
    SetEnvironmentVariableW(L"SteamGameId", L"4704690");
    HMODULE dll = LoadLibraryW(dll_path.c_str());
    if (!dll) {
        std::cerr << "DLL load failed: " << GetLastError() << "\n";
        return 3;
    }
    auto init = reinterpret_cast<bool (__cdecl *)()>(GetProcAddress(dll, "SteamAPI_Init"));
    auto ugc = reinterpret_cast<void *(__cdecl *)()>(GetProcAddress(dll, "SteamAPI_SteamUGC_v021"));
    auto download = reinterpret_cast<bool (__cdecl *)(void *, unsigned long long, bool)>(
        GetProcAddress(dll, "SteamAPI_ISteamUGC_DownloadItem"));
    auto shutdown = reinterpret_cast<void (__cdecl *)()>(GetProcAddress(dll, "SteamAPI_Shutdown"));
    if (!init || !ugc || !download || !shutdown || !init()) return 4;
    void *api = ugc();
    if (!api) return 5;
    UINT_PTR timer = SetTimer(nullptr, 0, 200, tick);
    if (!timer) return 6;
    download(api, 7, false);
    KillTimer(nullptr, timer);
    shutdown();
    if (!found || !passed) {
        std::cerr << "FAIL: actual compiled DLL caption was missing or incorrect\n";
        return 1;
    }
    std::cout << "PASS: actual release DLL opened through exported Steam API, uses the custom title bar, and retains the exact Unicode caption\n";
    return 0;
}
