#define NOMINMAX
#include <iostream>
#include "meccha_workshop_ui.h"

namespace {
std::wstring expected_title;
const wchar_t *expected_class = nullptr;
int close_command = IDCANCEL;
bool found_dialog = false;
bool passed = true;

BOOL CALLBACK inspect_dialog(HWND hwnd, LPARAM)
{
    wchar_t class_name[160]{};
    GetClassNameW(hwnd, class_name, 160);
    if (std::wstring(class_name) != expected_class) return TRUE;

    wchar_t title[512]{};
    GetWindowTextW(hwnd, title, 512);
    found_dialog = true;
    if (!IsWindowUnicode(hwnd) || std::wstring(title) != expected_title) {
        std::wcerr << L"Caption mismatch. Expected [" << expected_title
                   << L"], got [" << title << L"]\n";
        passed = false;
    }

    HWND gif = FindWindowExW(hwnd, nullptr, L"GBE_MECCHA_WORKSHOP_GIF_V14", nullptr);
    if (!gif) {
        std::cerr << "Embedded GIF control was not created\n";
        passed = false;
    } else {
        const LONG_PTR style = GetWindowLongPtrW(gif, GWL_STYLE);
        const LONG_PTR ex_style = GetWindowLongPtrW(gif, GWL_EXSTYLE);
        if ((style & WS_BORDER) ||
            (ex_style & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE))) {
            std::cerr << "GIF has a native border\n";
            passed = false;
        }
        auto *state = reinterpret_cast<MecchaWorkshopUI::GifState *>(
            GetWindowLongPtrW(gif, GWLP_USERDATA));
        if (!state || !state->bitmap) {
            std::cerr << "GIF frame was not rendered\n";
            passed = false;
        }
    }

    PostMessageW(hwnd, WM_COMMAND, close_command, 0);
    return FALSE;
}

void CALLBACK inspect_timer(HWND, UINT, UINT_PTR, DWORD)
{
    EnumThreadWindows(GetCurrentThreadId(), inspect_dialog, 0);
}

UINT_PTR begin_check(const wchar_t *class_name, const wchar_t *title, int command)
{
    expected_class = class_name;
    expected_title = title;
    close_command = command;
    found_dialog = false;
    return SetTimer(nullptr, 0, 50, inspect_timer);
}

void end_check(UINT_PTR timer)
{
    KillTimer(nullptr, timer);
    if (!found_dialog) {
        std::cerr << "Expected dialog did not open\n";
        passed = false;
    }
}
}

int main()
{
    UINT_PTR timer = begin_check(
        L"GBE_MECCHA_WORKSHOP_MESSAGE_V14",
        L"MECCHA Workshop - Missing Map -J\u0268n\u03C7", IDCANCEL);
    if (!timer) return 2;
    MecchaWorkshopUI::show_download_consent(3772646297ULL, std::filesystem::path{L"test"});
    end_check(timer);

    timer = begin_check(
        L"GBE_MECCHA_WORKSHOP_MESSAGE_V14",
        L"MECCHA Workshop - How Friend Can Export -J\u0268n\u03C7",
        MecchaWorkshopUI::BTN_BACK);
    if (!timer) return 2;
    MecchaWorkshopUI::show_friend_export(3772646297ULL, std::filesystem::path{L"test"});
    end_check(timer);

    timer = begin_check(
        L"GBE_MECCHA_WORKSHOP_MESSAGE_V14",
        L"MECCHA Workshop - Import Complete -J\u0268n\u03C7", IDOK);
    if (!timer) return 2;
    MecchaWorkshopUI::show_notice("MECCHA Workshop - Import Complete", "Test notice");
    end_check(timer);

    timer = begin_check(
        L"GBE_MECCHA_WORKSHOP_IMPORT_V14",
        L"MECCHA Workshop - Import Mod -J\u0268n\u03C7", IDCANCEL);
    if (!timer) return 2;
    std::string value;
    if (MecchaWorkshopUI::show_f8_import_dialog("", std::filesystem::path{L"test"}, value)) {
        std::cerr << "Cancelled import was accepted\n";
        passed = false;
    }
    end_check(timer);

    if (!passed) return 1;
    std::cout << "PASS: four actual dialogs retain the exact Unicode caption; GIF frames render without native borders\n";
    return 0;
}
