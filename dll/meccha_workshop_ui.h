#pragma once

#ifdef _WIN32

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <filesystem>
#include <string>

#include "meccha_dialog_gif_data.h"

#ifdef _MSC_VER
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Ole32.lib")
#endif

namespace MecchaWorkshopUI {

// MECCHA v11.3
// The dialog GIF is compiled directly into steam_api64.dll.
// No external meccha_dialog.gif or meccha_dialog.png is required.

struct DownloadConsentState {
    unsigned long long workshop_id{};
    int result{IDNO};
    bool done{};
    Gdiplus::Image *image{};
    IStream *image_stream{};
    HWND image_control{};
    HBITMAP bitmap{};
    UINT frame{};
    UINT frame_count{};
};

constexpr UINT_PTR GIF_TIMER_ID = 0x4D43;

inline HBITMAP make_scaled_bitmap(
    Gdiplus::Image *image,
    int box_w,
    int box_h
)
{
    if (!image ||
        image->GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }

    const UINT source_w = image->GetWidth();
    const UINT source_h = image->GetHeight();

    if (!source_w || !source_h) {
        return nullptr;
    }

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

    int draw_w =
        static_cast<int>(source_w * scale);

    int draw_h =
        static_cast<int>(source_h * scale);

    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    Gdiplus::Bitmap scaled(
        box_w,
        box_h,
        PixelFormat32bppARGB
    );

    Gdiplus::Graphics graphics(&scaled);

    graphics.SetInterpolationMode(
        Gdiplus::InterpolationModeHighQualityBicubic
    );

    graphics.SetPixelOffsetMode(
        Gdiplus::PixelOffsetModeHighQuality
    );

    graphics.Clear(
        Gdiplus::Color(240, 240, 240)
    );

    const int draw_x =
        (box_w - draw_w) / 2;

    const int draw_y =
        (box_h - draw_h) / 2;

    graphics.DrawImage(
        image,
        draw_x,
        draw_y,
        draw_w,
        draw_h
    );

    HBITMAP bitmap{};

    if (scaled.GetHBITMAP(
            Gdiplus::Color(240, 240, 240),
            &bitmap
        ) != Gdiplus::Ok) {
        return nullptr;
    }

    return bitmap;
}

inline void refresh_image_bitmap(
    DownloadConsentState *state
)
{
    if (!state ||
        !state->image_control ||
        !state->image) {
        return;
    }

    HBITMAP new_bitmap =
        make_scaled_bitmap(
            state->image,
            180,
            142
        );

    if (!new_bitmap) {
        return;
    }

    HBITMAP old_bitmap =
        reinterpret_cast<HBITMAP>(
            SendMessageA(
                state->image_control,
                STM_SETIMAGE,
                IMAGE_BITMAP,
                reinterpret_cast<LPARAM>(
                    new_bitmap
                )
            )
        );

    if (old_bitmap &&
        old_bitmap != new_bitmap) {
        DeleteObject(old_bitmap);
    }

    state->bitmap = new_bitmap;

    InvalidateRect(
        state->image_control,
        nullptr,
        TRUE
    );

    UpdateWindow(
        state->image_control
    );
}

inline bool load_embedded_gif(
    DownloadConsentState &state
)
{
    if (state.image) {
        return true;
    }

    if (!MecchaEmbeddedDialogGif::size) {
        return false;
    }

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            MecchaEmbeddedDialogGif::size
        );

    if (!memory) {
        return false;
    }

    void *locked =
        GlobalLock(memory);

    if (!locked) {
        GlobalFree(memory);
        return false;
    }

    memcpy(
        locked,
        MecchaEmbeddedDialogGif::data,
        MecchaEmbeddedDialogGif::size
    );

    GlobalUnlock(memory);

    IStream *stream{};

    const HRESULT hr =
        CreateStreamOnHGlobal(
            memory,
            TRUE,
            &stream
        );

    if (FAILED(hr) || !stream) {
        GlobalFree(memory);
        return false;
    }

    auto *image =
        new Gdiplus::Image(
            stream,
            FALSE
        );

    if (!image ||
        image->GetLastStatus() !=
            Gdiplus::Ok) {

        delete image;
        stream->Release();
        return false;
    }

    state.image = image;
    state.image_stream = stream;
    return true;
}

inline LRESULT CALLBACK download_consent_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    auto *state =
        reinterpret_cast<DownloadConsentState *>(
            GetWindowLongPtrA(
                hwnd,
                GWLP_USERDATA
            )
        );

    switch (msg) {
        case WM_NCCREATE: {
            auto *create =
                reinterpret_cast<CREATESTRUCTA *>(
                    lparam
                );

            SetWindowLongPtrA(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(
                    create->lpCreateParams
                )
            );

            return TRUE;
        }

        case WM_CREATE: {
            state =
                reinterpret_cast<DownloadConsentState *>(
                    GetWindowLongPtrA(
                        hwnd,
                        GWLP_USERDATA
                    )
                );

            HFONT font =
                reinterpret_cast<HFONT>(
                    GetStockObject(
                        DEFAULT_GUI_FONT
                    )
                );

            const bool has_image =
                state &&
                state->image &&
                state->image->GetLastStatus() ==
                    Gdiplus::Ok;

            const int text_x =
                has_image ? 218 : 24;

            const int text_width =
                has_image ? 330 : 520;

            if (has_image) {
                state->image_control =
                    CreateWindowExA(
                        WS_EX_CLIENTEDGE,
                        "STATIC",
                        "",
                        WS_CHILD |
                        WS_VISIBLE |
                        SS_BITMAP |
                        SS_CENTERIMAGE,
                        18,
                        18,
                        180,
                        142,
                        hwnd,
                        nullptr,
                        GetModuleHandleA(nullptr),
                        nullptr
                    );

                refresh_image_bitmap(state);

                state->frame_count =
                    state->image->GetFrameCount(
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

            const std::string id_text =
                std::to_string(
                    state->workshop_id
                );

            const std::string text =
                "MECCHA wants to download the missing Steam Workshop map.\r\n\r\n"
                "Workshop ID: " + id_text +
                "\r\n\r\nDownload and install this map now?";

            HWND label =
                CreateWindowExA(
                    0,
                    "STATIC",
                    text.c_str(),
                    WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                    text_x,
                    30,
                    text_width,
                    125,
                    hwnd,
                    nullptr,
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            HWND yes_btn =
                CreateWindowExA(
                    0,
                    "BUTTON",
                    "Download",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_DEFPUSHBUTTON,
                    338,
                    180,
                    100,
                    32,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(
                            IDYES
                        )
                    ),
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            HWND no_btn =
                CreateWindowExA(
                    0,
                    "BUTTON",
                    "No",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP,
                    448,
                    180,
                    100,
                    32,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(
                            IDNO
                        )
                    ),
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            SendMessageA(
                label,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    font
                ),
                TRUE
            );

            SendMessageA(
                yes_btn,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    font
                ),
                TRUE
            );

            SendMessageA(
                no_btn,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    font
                ),
                TRUE
            );

            SetFocus(yes_btn);
            return 0;
        }

        case WM_TIMER: {
            if (state &&
                wparam == GIF_TIMER_ID &&
                state->image &&
                state->frame_count > 1) {

                state->frame =
                    (state->frame + 1) %
                    state->frame_count;

                state->image->SelectActiveFrame(
                    &Gdiplus::FrameDimensionTime,
                    state->frame
                );

                refresh_image_bitmap(state);
            }

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
            if (state) {
                state->result = IDNO;
            }

            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(
                hwnd,
                GIF_TIMER_ID
            );

            if (state) {
                if (state->bitmap) {
                    DeleteObject(
                        state->bitmap
                    );

                    state->bitmap = nullptr;
                }

                state->done = true;
            }

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
    const std::filesystem::path &requested_path
)
{
    (void)requested_path;

    Gdiplus::GdiplusStartupInput
        startup_input;

    ULONG_PTR gdiplus_token{};

    const Gdiplus::Status
        gdiplus_status =
            Gdiplus::GdiplusStartup(
                &gdiplus_token,
                &startup_input,
                nullptr
            );

    DownloadConsentState state{};
    state.workshop_id = workshop_id;

    if (gdiplus_status ==
        Gdiplus::Ok) {
        load_embedded_gif(state);
    }

    HINSTANCE instance =
        GetModuleHandleA(nullptr);

    const char class_name[] =
        "GBE_MECCHA_DOWNLOAD_CONSENT_V113";

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc =
        download_consent_proc;
    wc.hInstance = instance;
    wc.hCursor =
        LoadCursor(
            nullptr,
            IDC_ARROW
        );
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_BTNFACE + 1
        );
    wc.lpszClassName =
        class_name;

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
        ((desktop.right -
          desktop.left) -
         width) / 2;

    const int y =
        desktop.top +
        ((desktop.bottom -
          desktop.top) -
         height) / 2;

    HWND owner =
        GetForegroundWindow();

    HWND hwnd =
        CreateWindowExA(
            WS_EX_DLGMODALFRAME |
            WS_EX_TOPMOST,
            class_name,
            "MECCHA Workshop - Download Mod",
            WS_CAPTION |
            WS_SYSMENU,
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
        delete state.image;

        if (state.image_stream) {
            state.image_stream->Release();
            state.image_stream = nullptr;
        }

        if (gdiplus_token) {
            Gdiplus::GdiplusShutdown(
                gdiplus_token
            );
        }

        const std::string fallback =
            "MECCHA wants to download Steam Workshop item " +
            std::to_string(
                workshop_id
            ) +
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

    if (owner &&
        owner != hwnd) {
        EnableWindow(
            owner,
            FALSE
        );
    }

    ShowWindow(
        hwnd,
        SW_SHOW
    );

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

        if (result <= 0) {
            break;
        }

        if (!IsDialogMessageA(
                hwnd,
                &msg
            )) {

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    if (owner &&
        IsWindow(owner)) {

        EnableWindow(
            owner,
            TRUE
        );

        SetForegroundWindow(owner);
    }

    delete state.image;
    state.image = nullptr;

    if (state.image_stream) {
        state.image_stream->Release();
        state.image_stream = nullptr;
    }

    if (gdiplus_token) {
        Gdiplus::GdiplusShutdown(
            gdiplus_token
        );
    }

    return state.result;
}

} // namespace MecchaWorkshopUI

#endif
