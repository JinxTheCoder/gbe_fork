#pragma once

#ifdef _WIN32

#include <windows.h>
#include <gdiplus.h>
#include <filesystem>
#include <string>

#ifdef _MSC_VER
#pragma comment(lib, "Gdiplus.lib")
#endif

namespace MecchaWorkshopUI {

struct DownloadConsentState {
    unsigned long long workshop_id{};
    int result{IDNO};
    bool done{};
    Gdiplus::Image *gif{};
    UINT frame{};
    UINT frame_count{};
};

constexpr UINT_PTR GIF_TIMER_ID = 0x4D43;

inline LRESULT CALLBACK download_consent_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    auto *state = reinterpret_cast<DownloadConsentState *>(
        GetWindowLongPtrA(hwnd, GWLP_USERDATA)
    );

    switch (msg) {
        case WM_NCCREATE: {
            auto *create = reinterpret_cast<CREATESTRUCTA *>(lparam);
            SetWindowLongPtrA(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(create->lpCreateParams)
            );
            return TRUE;
        }

        case WM_CREATE: {
            state = reinterpret_cast<DownloadConsentState *>(
                GetWindowLongPtrA(hwnd, GWLP_USERDATA)
            );

            HFONT font = reinterpret_cast<HFONT>(
                GetStockObject(DEFAULT_GUI_FONT)
            );

            const bool has_gif =
                state &&
                state->gif &&
                state->gif->GetLastStatus() == Gdiplus::Ok;

            const int text_x = has_gif ? 218 : 24;
            const int text_width = has_gif ? 330 : 520;

            const std::string id_text =
                std::to_string(state->workshop_id);

            const std::string text =
                "MECCHA wants to download the missing Steam Workshop map.\r\n\r\n"
                "Workshop ID: " + id_text +
                "\r\n\r\nDownload and install this map now?";

            HWND label = CreateWindowExA(
                0,
                "STATIC",
                text.c_str(),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                text_x,
                30,
                text_width,
                125,
                hwnd,
                nullptr,
                GetModuleHandleA(nullptr),
                nullptr
            );

            HWND yes_btn = CreateWindowExA(
                0,
                "BUTTON",
                "Download",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                338,
                180,
                100,
                32,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(IDYES)
                ),
                GetModuleHandleA(nullptr),
                nullptr
            );

            HWND no_btn = CreateWindowExA(
                0,
                "BUTTON",
                "No",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                448,
                180,
                100,
                32,
                hwnd,
                reinterpret_cast<HMENU>(
                    static_cast<INT_PTR>(IDNO)
                ),
                GetModuleHandleA(nullptr),
                nullptr
            );

            SendMessageA(
                label,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(font),
                TRUE
            );

            SendMessageA(
                yes_btn,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(font),
                TRUE
            );

            SendMessageA(
                no_btn,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(font),
                TRUE
            );

            if (has_gif) {
                state->frame_count =
                    state->gif->GetFrameCount(
                        &Gdiplus::FrameDimensionTime
                    );

                if (state->frame_count > 1) {
                    SetTimer(
                        hwnd,
                        GIF_TIMER_ID,
                        90,
                        nullptr
                    );
                }
            }

            SetFocus(yes_btn);
            return 0;
        }

        case WM_TIMER: {
            if (state &&
                wparam == GIF_TIMER_ID &&
                state->gif &&
                state->frame_count > 1) {

                state->frame =
                    (state->frame + 1) %
                    state->frame_count;

                state->gif->SelectActiveFrame(
                    &Gdiplus::FrameDimensionTime,
                    state->frame
                );

                RECT gif_rect{18, 18, 202, 164};
                InvalidateRect(
                    hwnd,
                    &gif_rect,
                    FALSE
                );
            }

            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);

            if (state &&
                state->gif &&
                state->gif->GetLastStatus() == Gdiplus::Ok) {

                Gdiplus::Graphics graphics(hdc);

                const UINT source_w =
                    state->gif->GetWidth();

                const UINT source_h =
                    state->gif->GetHeight();

                if (source_w && source_h) {
                    const int box_w = 180;
                    const int box_h = 142;

                    const double scale_x =
                        static_cast<double>(box_w) /
                        static_cast<double>(source_w);

                    const double scale_y =
                        static_cast<double>(box_h) /
                        static_cast<double>(source_h);

                    const double scale =
                        scale_x < scale_y
                            ? scale_x
                            : scale_y;

                    const int draw_w =
                        static_cast<int>(
                            source_w * scale
                        );

                    const int draw_h =
                        static_cast<int>(
                            source_h * scale
                        );

                    const int draw_x =
                        18 + (box_w - draw_w) / 2;

                    const int draw_y =
                        18 + (box_h - draw_h) / 2;

                    graphics.DrawImage(
                        state->gif,
                        draw_x,
                        draw_y,
                        draw_w,
                        draw_h
                    );
                }
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND: {
            if (!state) break;

            const int id = LOWORD(wparam);

            if (id == IDYES ||
                id == IDNO ||
                id == IDCANCEL) {

                state->result =
                    id == IDYES
                        ? IDYES
                        : IDNO;

                DestroyWindow(hwnd);
                return 0;
            }

            break;
        }

        case WM_CLOSE:
            if (state) state->result = IDNO;
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, GIF_TIMER_ID);
            if (state) state->done = true;
            return 0;
    }

    return DefWindowProcA(
        hwnd,
        msg,
        wparam,
        lparam
    );
}

inline int show_download_consent(
    unsigned long long workshop_id,
    const std::filesystem::path &gif_path
)
{
    Gdiplus::GdiplusStartupInput startup_input;
    ULONG_PTR gdiplus_token{};

    const Gdiplus::Status gdiplus_status =
        Gdiplus::GdiplusStartup(
            &gdiplus_token,
            &startup_input,
            nullptr
        );

    DownloadConsentState state{};
    state.workshop_id = workshop_id;

    // MECCHA v11.1 robust dialog image loader
    // Try the supplied path, PNG fallback, the steam_api64.dll folder,
    // and finally the game EXE folder.
    auto try_load_image =
        [&](const std::filesystem::path &candidate) -> bool {

            if (state.gif) return true;

            std::error_code ec;
            if (!std::filesystem::is_regular_file(candidate, ec) || ec) {
                return false;
            }

            auto *image =
                new Gdiplus::Image(
                    candidate.wstring().c_str()
                );

            if (!image ||
                image->GetLastStatus() != Gdiplus::Ok) {

                delete image;
                return false;
            }

            state.gif = image;
            return true;
        };

    if (gdiplus_status == Gdiplus::Ok) {
        try_load_image(gif_path);

        std::filesystem::path supplied_png = gif_path;
        supplied_png.replace_extension(".png");
        try_load_image(supplied_png);

        wchar_t module_file[MAX_PATH]{};

        HMODULE steam_module =
            GetModuleHandleW(L"steam_api64.dll");

        if (steam_module &&
            GetModuleFileNameW(
                steam_module,
                module_file,
                MAX_PATH
            )) {

            const auto module_dir =
                std::filesystem::path(module_file)
                    .parent_path();

            try_load_image(
                module_dir /
                "steam_settings" /
                "meccha_dialog.gif"
            );

            try_load_image(
                module_dir /
                "steam_settings" /
                "meccha_dialog.png"
            );
        }

        module_file[0] = L'\0';

        if (GetModuleFileNameW(
                nullptr,
                module_file,
                MAX_PATH
            )) {

            const auto exe_dir =
                std::filesystem::path(module_file)
                    .parent_path();

            try_load_image(
                exe_dir /
                "steam_settings" /
                "meccha_dialog.gif"
            );

            try_load_image(
                exe_dir /
                "steam_settings" /
                "meccha_dialog.png"
            );
        }
    }
    HINSTANCE instance =
        GetModuleHandleA(nullptr);

    const char class_name[] =
        "GBE_MECCHA_DOWNLOAD_CONSENT_V11";

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc =
        download_consent_proc;
    wc.hInstance = instance;
    wc.hCursor =
        LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_BTNFACE + 1
        );
    wc.lpszClassName = class_name;

    RegisterClassExA(&wc);

    const int width = 585;
    const int height = 265;

    RECT desktop{};
    SystemParametersInfoA(
        SPI_GETWORKAREA,
        0,
        &desktop,
        0
    );

    const int x =
        desktop.left +
        ((desktop.right - desktop.left) -
         width) / 2;

    const int y =
        desktop.top +
        ((desktop.bottom - desktop.top) -
         height) / 2;

    HWND owner = GetForegroundWindow();

    HWND hwnd = CreateWindowExA(
        WS_EX_DLGMODALFRAME |
        WS_EX_TOPMOST,
        class_name,
        "MECCHA Workshop - Download Mod",
        WS_CAPTION | WS_SYSMENU,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        instance,
        &state
    );

    if (!hwnd) {
        delete state.gif;

        if (gdiplus_token) {
            Gdiplus::GdiplusShutdown(
                gdiplus_token
            );
        }

        const std::string fallback =
            "MECCHA wants to download Steam Workshop item " +
            std::to_string(workshop_id) +
            ".\n\nDownload and install it now?";

        return MessageBoxA(
            nullptr,
            fallback.c_str(),
            "MECCHA Workshop - Download Mod",
            MB_YESNO |
            MB_ICONQUESTION |
            MB_SETFOREGROUND |
            MB_TOPMOST
        );
    }

    if (owner && owner != hwnd) {
        EnableWindow(owner, FALSE);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};

    while (!state.done) {
        const BOOL result =
            GetMessageA(
                &msg,
                nullptr,
                0,
                0
            );

        if (result <= 0) break;

        if (!IsDialogMessageA(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    if (owner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    delete state.gif;
    state.gif = nullptr;

    if (gdiplus_token) {
        Gdiplus::GdiplusShutdown(
            gdiplus_token
        );
    }

    return state.result;
}

} // namespace MecchaWorkshopUI

#endif
