#pragma once

#ifdef _WIN32

#include <windows.h>
#include <commdlg.h>
#include <objidl.h>
#include <gdiplus.h>
#include <filesystem>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <fstream>
#include <system_error>
#include <cstring>

#include "meccha_dialog_gif_data.h"

#ifdef _MSC_VER
#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Comdlg32.lib")
#endif

namespace MecchaWorkshopUI {

// MECCHA v15
// All MECCHA Workshop dialogs use the animated GIF embedded in steam_api64.dll.
// Workshop downloads are imported from a ZIP supplied by the user. SteamCMD
// authentication is intentionally not part of this UI.

constexpr UINT_PTR GIF_TIMER_ID = 0x4D43;
constexpr int BTN_IMPORT = 6201;
constexpr int BTN_HELP = 6202;
constexpr int BTN_COPY = 6203;
constexpr int BTN_BACK = 6204;
constexpr int BTN_CLEAR = 6205;
constexpr int BTN_CLOSE = 6206;
constexpr int DIALOG_TITLE_HEIGHT = 32;
constexpr int DIALOG_CLOSE_WIDTH = 40;

struct GifState {
    Gdiplus::Image *image{};
    IStream *stream{};
    HWND control{};
    HBITMAP bitmap{};
    UINT frame{};
    UINT frame_count{};
};

struct MessageDialogState {
    std::wstring title{};
    std::string body{};
    std::vector<std::pair<int, std::string>> buttons{};
    int result{IDCANCEL};
    bool done{};
    int width{760};
    int height{320};
    int text_x{20};
    int text_y{22};
    int text_width{500};
    int text_height{220};
    int gif_x{550};
    int gif_y{24};
    int gif_width{180};
    int gif_height{142};
    GifState gif{};
};

struct ImportDialogState {
    std::wstring title{};
    std::string initial{};
    std::string value{};
    std::filesystem::path settings_root{};
    HWND edit{};
    bool accepted{};
    bool done{};
    GifState gif{};
};

inline std::wstring branded_dialog_title(const char *title)
{
    std::wstring result = MecchaDialogBrand::narrow_to_wide(title);
    MecchaDialogBrand::append_patch_credit(result);
    return result;
}

inline void draw_dialog_title(HWND hwnd, HDC dc, const std::wstring &title)
{
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    bounds.bottom = DIALOG_TITLE_HEIGHT;
    FillRect(dc, &bounds, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

    RECT text_rect = bounds;
    text_rect.left = 12;
    text_rect.right -= DIALOG_CLOSE_WIDTH + 8;
    HFONT font = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old_font = SelectObject(dc, font ? reinterpret_cast<HGDIOBJ>(font) : GetStockObject(DEFAULT_GUI_FONT));
    const int old_mode = SetBkMode(dc, TRANSPARENT);
    const COLORREF old_color = SetTextColor(dc, RGB(32, 32, 32));
    DrawTextW(dc, title.c_str(), -1, &text_rect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SetTextColor(dc, old_color);
    SetBkMode(dc, old_mode);
    SelectObject(dc, old_font);
    if (font) DeleteObject(font);
}

inline void create_dialog_close_button(HWND hwnd)
{
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    HWND button = CreateWindowExW(
        0, L"BUTTON", L"\u00D7", WS_CHILD | WS_VISIBLE | BS_FLAT,
        bounds.right - DIALOG_CLOSE_WIDTH, 0,
        DIALOG_CLOSE_WIDTH, DIALOG_TITLE_HEIGHT, hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(BTN_CLOSE)),
        GetModuleHandleW(nullptr), nullptr);
    if (button) {
        SendMessageW(button, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }
}

inline bool is_dialog_title_point(HWND hwnd, LPARAM position)
{
    POINT point{static_cast<short>(LOWORD(position)), static_cast<short>(HIWORD(position))};
    ScreenToClient(hwnd, &point);
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    return point.x >= 0 && point.x < bounds.right - DIALOG_CLOSE_WIDTH &&
           point.y >= 0 && point.y < DIALOG_TITLE_HEIGHT;
}

inline bool directory_has_files(const std::filesystem::path &path)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec) || ec) return false;

    try {
        for (std::filesystem::recursive_directory_iterator it(
                 path,
                 std::filesystem::directory_options::skip_permission_denied,
                 ec), end;
             !ec && it != end;
             it.increment(ec)) {
            std::error_code entry_ec;
            if (it->is_regular_file(entry_ec) && !entry_ec) return true;
        }
    } catch (...) {
    }

    return false;
}

inline bool parse_workshop_id_text(
    const std::string &text,
    unsigned long long &out
)
{
    if (text.empty()) return false;

    for (char c : text) {
        if (c < '0' || c > '9') return false;
    }

    try {
        const auto value = std::stoull(text);
        if (!value) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

inline HBITMAP make_scaled_bitmap(
    Gdiplus::Image *image,
    int box_w,
    int box_h
)
{
    if (!image ||
        image->GetLastStatus() != Gdiplus::Ok ||
        box_w <= 0 ||
        box_h <= 0) {
        return nullptr;
    }

    const UINT source_w = image->GetWidth();
    const UINT source_h = image->GetHeight();

    if (!source_w || !source_h) return nullptr;

    const double scale_x =
        static_cast<double>(box_w) /
        static_cast<double>(source_w);

    const double scale_y =
        static_cast<double>(box_h) /
        static_cast<double>(source_h);

    // Fill the entire GIF area instead of letterboxing it.
    // A small amount of centered cropping is preferable to visible padding.
    const double scale =
        scale_x > scale_y ? scale_x : scale_y;

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

inline bool load_embedded_gif(GifState &gif)
{
    if (gif.image) return true;
    if (!MecchaEmbeddedDialogGif::size) return false;

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            MecchaEmbeddedDialogGif::size
        );

    if (!memory) return false;

    void *locked = GlobalLock(memory);

    if (!locked) {
        GlobalFree(memory);
        return false;
    }

    std::memcpy(
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
        image->GetLastStatus() != Gdiplus::Ok) {
        delete image;
        stream->Release();
        return false;
    }

    gif.image = image;
    gif.stream = stream;
    gif.frame_count =
        image->GetFrameCount(
            &Gdiplus::FrameDimensionTime
        );

    if (!gif.frame_count) gif.frame_count = 1;
    return true;
}

inline void cleanup_gif(GifState &gif)
{
    if (gif.bitmap) {
        DeleteObject(gif.bitmap);
        gif.bitmap = nullptr;
    }

    delete gif.image;
    gif.image = nullptr;

    if (gif.stream) {
        gif.stream->Release();
        gif.stream = nullptr;
    }

    gif.control = nullptr;
    gif.frame = 0;
    gif.frame_count = 0;
}

inline void refresh_gif_bitmap(
    GifState &gif,
    int width,
    int height
)
{
    if (!gif.control || !gif.image) return;

    HBITMAP next =
        make_scaled_bitmap(
            gif.image,
            width,
            height
        );

    if (!next) return;

    HBITMAP old = gif.bitmap;
    gif.bitmap = next;

    if (old && old != next) {
        DeleteObject(old);
    }

    InvalidateRect(
        gif.control,
        nullptr,
        FALSE
    );

    UpdateWindow(gif.control);
}

inline void advance_gif(
    GifState &gif,
    int width,
    int height
)
{
    if (!gif.image || gif.frame_count <= 1) return;

    gif.frame =
        (gif.frame + 1) %
        gif.frame_count;

    gif.image->SelectActiveFrame(
        &Gdiplus::FrameDimensionTime,
        gif.frame
    );

    refresh_gif_bitmap(
        gif,
        width,
        height
    );
}

inline LRESULT CALLBACK gif_control_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    if (msg == WM_NCCREATE) {
        auto *create = reinterpret_cast<CREATESTRUCTA *>(lparam);
        SetWindowLongPtrA(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(create->lpCreateParams)
        );
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }

    // WM_PAINT covers the entire client area, so no separate erase is needed.
    if (msg == WM_ERASEBKGND) return 1;

    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, GetSysColorBrush(COLOR_BTNFACE));

        auto *gif = reinterpret_cast<GifState *>(
            GetWindowLongPtrA(hwnd, GWLP_USERDATA)
        );

        if (gif && gif->bitmap) {
            HDC memory_dc = CreateCompatibleDC(dc);
            if (memory_dc) {
                HGDIOBJ previous = SelectObject(memory_dc, gif->bitmap);
                if (previous && previous != HGDI_ERROR) {
                    BitBlt(
                        dc, 0, 0,
                        client.right - client.left,
                        client.bottom - client.top,
                        memory_dc, 0, 0, SRCCOPY
                    );
                    SelectObject(memory_dc, previous);
                }
                DeleteDC(memory_dc);
            }
        }

        EndPaint(hwnd, &paint);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

inline bool create_gif_control(
    HWND parent,
    GifState &gif,
    int x,
    int y,
    int width,
    int height
)
{
    if (!gif.image) return false;

    // Paint the GIF ourselves. A native STATIC bitmap control can add a
    // themed frame or inset; this class only paints the image pixels.
    HINSTANCE instance = GetModuleHandleA(nullptr);
    const char class_name[] = "GBE_MECCHA_WORKSHOP_GIF_V15";
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = gif_control_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = class_name;

    if (!RegisterClassExA(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    gif.control =
        CreateWindowExA(
            0,
            class_name,
            "",
            WS_CHILD |
            WS_VISIBLE,
            x,
            y,
            width,
            height,
            parent,
            nullptr,
            instance,
            &gif
        );

    if (!gif.control) return false;

    refresh_gif_bitmap(
        gif,
        width,
        height
    );

    if (gif.frame_count > 1) {
        SetTimer(
            parent,
            GIF_TIMER_ID,
            90,
            nullptr
        );
    }

    return true;
}

inline void center_window_rect(
    int width,
    int height,
    int &x,
    int &y
)
{
    RECT desktop{};

    SystemParametersInfoA(
        SPI_GETWORKAREA,
        0,
        &desktop,
        0
    );

    x =
        desktop.left +
        ((desktop.right - desktop.left) - width) / 2;

    y =
        desktop.top +
        ((desktop.bottom - desktop.top) - height) / 2;
}

inline LRESULT CALLBACK message_dialog_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    auto *state =
        reinterpret_cast<MessageDialogState *>(
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

            return DefWindowProcA(hwnd, msg, wparam, lparam);
        }

        case WM_CREATE: {
            state =
                reinterpret_cast<MessageDialogState *>(
                    GetWindowLongPtrA(
                        hwnd,
                        GWLP_USERDATA
                    )
                );

            if (!state) return -1;

            create_dialog_close_button(hwnd);

            HFONT font =
                reinterpret_cast<HFONT>(
                    GetStockObject(
                        DEFAULT_GUI_FONT
                    )
                );

            HWND label =
                CreateWindowExA(
                    0,
                    "STATIC",
                    state->body.c_str(),
                    WS_CHILD |
                    WS_VISIBLE |
                    SS_LEFT,
                    state->text_x,
                    state->text_y,
                    state->text_width,
                    state->text_height,
                    hwnd,
                    nullptr,
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            if (label) {
                SendMessageA(
                    label,
                    WM_SETFONT,
                    reinterpret_cast<WPARAM>(font),
                    TRUE
                );
            }

            create_gif_control(
                hwnd,
                state->gif,
                state->gif_x,
                state->gif_y,
                state->gif_width,
                state->gif_height
            );

            const int button_width = 150;
            const int button_height = 32;
            const int gap = 10;
            const int count =
                static_cast<int>(
                    state->buttons.size()
                );

            const int total_width =
                count > 0
                    ? count * button_width +
                      (count - 1) * gap
                    : 0;

            int button_x =
                (state->width - total_width) / 2;

            const int button_y =
                state->height - 78 + DIALOG_TITLE_HEIGHT;

            for (int i = 0; i < count; ++i) {
                const auto &entry =
                    state->buttons[
                        static_cast<size_t>(i)
                    ];

                DWORD style =
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP;

                if (i == 0) {
                    style |= BS_DEFPUSHBUTTON;
                }

                HWND button =
                    CreateWindowExA(
                        0,
                        "BUTTON",
                        entry.second.c_str(),
                        style,
                        button_x,
                        button_y,
                        button_width,
                        button_height,
                        hwnd,
                        reinterpret_cast<HMENU>(
                            static_cast<INT_PTR>(
                                entry.first
                            )
                        ),
                        GetModuleHandleA(nullptr),
                        nullptr
                    );

                if (button) {
                    SendMessageA(
                        button,
                        WM_SETFONT,
                        reinterpret_cast<WPARAM>(font),
                        TRUE
                    );

                    if (i == 0) {
                        SetFocus(button);
                    }
                }

                button_x +=
                    button_width + gap;
            }

            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            if (state) draw_dialog_title(hwnd, dc, state->title);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_PRINTCLIENT:
            if (state) draw_dialog_title(hwnd, reinterpret_cast<HDC>(wparam), state->title);
            return 0;

        case WM_NCHITTEST:
            if (is_dialog_title_point(hwnd, lparam)) return HTCAPTION;
            break;

        case WM_TIMER:
            if (state &&
                wparam == GIF_TIMER_ID) {
                advance_gif(
                    state->gif,
                    state->gif_width,
                    state->gif_height
                );
            }
            return 0;

        case WM_COMMAND:
            if (state) {
                const int id = LOWORD(wparam);

                if (id == BTN_CLOSE) {
                    state->result = IDCANCEL;
                    DestroyWindow(hwnd);
                    return 0;
                }

                for (const auto &entry :
                     state->buttons) {
                    if (entry.first == id) {
                        state->result = id;
                        DestroyWindow(hwnd);
                        return 0;
                    }
                }
            }
            break;

        case WM_CLOSE:
            if (state) {
                state->result = IDCANCEL;
            }

            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(
                hwnd,
                GIF_TIMER_ID
            );

            if (state) {
                if (state->gif.bitmap) {
                    DeleteObject(
                        state->gif.bitmap
                    );

                    state->gif.bitmap = nullptr;
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

inline int show_message_dialog(
    const std::string &title,
    const std::string &body,
    const std::vector<std::pair<int, std::string>> &buttons,
    int width,
    int height,
    int text_width,
    int text_height,
    int gif_x,
    int gif_y = 24
)
{
    Gdiplus::GdiplusStartupInput startup_input;
    ULONG_PTR gdiplus_token{};

    const Gdiplus::Status status =
        Gdiplus::GdiplusStartup(
            &gdiplus_token,
            &startup_input,
            nullptr
        );

    MessageDialogState state{};
    state.title = branded_dialog_title(title.c_str());
    state.body = body;
    state.buttons = buttons;
    state.width = width;
    state.height = height;
    state.text_width = text_width;
    state.text_height = text_height;
    state.text_y += DIALOG_TITLE_HEIGHT;
    state.gif_x = gif_x;
    state.gif_y = gif_y + DIALOG_TITLE_HEIGHT;

    if (status == Gdiplus::Ok) {
        load_embedded_gif(state.gif);
    }

    HINSTANCE instance =
        GetModuleHandleA(nullptr);

    const char class_name[] =
        "GBE_MECCHA_WORKSHOP_MESSAGE_V15";

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc =
        message_dialog_proc;
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

    int x{};
    int y{};

    center_window_rect(
        width,
        height,
        x,
        y
    );

    HWND owner = GetForegroundWindow();

    HWND hwnd =
        CreateWindowExA(
            WS_EX_TOPMOST,
            class_name,
            title.c_str(),
            WS_POPUP |
            WS_BORDER |
            WS_CLIPCHILDREN |
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
        cleanup_gif(state.gif);

        if (gdiplus_token) {
            Gdiplus::GdiplusShutdown(
                gdiplus_token
            );
        }

        return IDCANCEL;
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

        if (!IsDialogMessageA(
                hwnd,
                &msg
            )) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    if (owner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    cleanup_gif(state.gif);

    if (gdiplus_token) {
        Gdiplus::GdiplusShutdown(
            gdiplus_token
        );
    }

    return state.result;
}

inline void show_notice(
    const std::string &title,
    const std::string &body
)
{
    show_message_dialog(
        title,
        body,
        {{IDOK, "OK"}},
        760,
        300,
        500,
        180,
        550,
        30
    );
}

inline bool copy_text_to_clipboard(
    const std::string &text
)
{
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    EmptyClipboard();

    HGLOBAL memory =
        GlobalAlloc(
            GMEM_MOVEABLE,
            text.size() + 1
        );

    if (!memory) {
        CloseClipboard();
        return false;
    }

    void *buffer =
        GlobalLock(memory);

    if (!buffer) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    std::memcpy(
        buffer,
        text.c_str(),
        text.size() + 1
    );

    GlobalUnlock(memory);

    if (!SetClipboardData(
            CF_TEXT,
            memory
        )) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

inline bool choose_zip_file(
    std::filesystem::path &out_path
)
{
    wchar_t file_buffer[32768]{};

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFile = file_buffer;
    ofn.nMaxFile = static_cast<DWORD>(
        sizeof(file_buffer) / sizeof(file_buffer[0])
    );

    static const wchar_t filter[] =
        L"ZIP Archives (*.zip)\0*.zip\0"
        L"All Files (*.*)\0*.*\0\0";

    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    const std::wstring title = branded_dialog_title("Select Workshop Mod ZIP");
    ofn.lpstrTitle = title.c_str();
    ofn.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }

    out_path = std::filesystem::path(file_buffer);

    return true;
}

inline std::string powershell_single_quote(
    const std::string &value
)
{
    std::string result;
    result.reserve(value.size() + 8);

    for (char c : value) {
        if (c == '\'') {
            result += "''";
        } else {
            result.push_back(c);
        }
    }

    return result;
}

inline bool run_hidden_process(
    const std::string &command_line,
    const std::filesystem::path &working_dir,
    DWORD &exit_code
)
{
    std::string mutable_command =
        command_line;

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process{};
    const std::string working =
        working_dir.string();

    const BOOL created =
        CreateProcessA(
            nullptr,
            mutable_command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            working.empty()
                ? nullptr
                : working.c_str(),
            &startup,
            &process
        );

    if (!created) {
        exit_code = GetLastError();
        return false;
    }

    WaitForSingleObject(
        process.hProcess,
        INFINITE
    );

    if (!GetExitCodeProcess(
            process.hProcess,
            &exit_code
        )) {
        exit_code = GetLastError();

        CloseHandle(
            process.hThread
        );

        CloseHandle(
            process.hProcess
        );

        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    return true;
}

inline bool extract_zip(
    const std::filesystem::path &zip_path,
    const std::filesystem::path &destination,
    std::string &error
)
{
    std::error_code ec;

    std::filesystem::remove_all(
        destination,
        ec
    );

    ec.clear();

    std::filesystem::create_directories(
        destination,
        ec
    );

    if (ec) {
        error =
            "Could not create the temporary extraction directory.";
        return false;
    }

    DWORD exit_code{};

    const std::string tar_command =
        "tar.exe -xf \"" +
        zip_path.string() +
        "\" -C \"" +
        destination.string() +
        "\"";

    if (run_hidden_process(
            tar_command,
            destination,
            exit_code
        ) &&
        exit_code == 0 &&
        directory_has_files(destination)) {
        return true;
    }

    const std::string ps_command =
        "powershell.exe -NoProfile -NonInteractive "
        "-ExecutionPolicy Bypass -Command "
        "\"Expand-Archive -LiteralPath '" +
        powershell_single_quote(
            zip_path.string()
        ) +
        "' -DestinationPath '" +
        powershell_single_quote(
            destination.string()
        ) +
        "' -Force\"";

    if (run_hidden_process(
            ps_command,
            destination,
            exit_code
        ) &&
        exit_code == 0 &&
        directory_has_files(destination)) {
        return true;
    }

    error =
        "Windows could not extract the selected ZIP file.";
    return false;
}

inline bool copy_directory_contents(
    const std::filesystem::path &source,
    const std::filesystem::path &destination,
    std::string &error
)
{
    std::error_code ec;

    std::filesystem::remove_all(
        destination,
        ec
    );

    ec.clear();

    std::filesystem::create_directories(
        destination,
        ec
    );

    if (ec) {
        error =
            "Could not create the Workshop staging directory.";
        return false;
    }

    try {
        for (const auto &entry :
             std::filesystem::directory_iterator(
                 source,
                 std::filesystem::directory_options::skip_permission_denied
             )) {

            const auto target =
                destination /
                entry.path().filename();

            std::filesystem::copy(
                entry.path(),
                target,
                std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing,
                ec
            );

            if (ec) {
                error =
                    "Could not copy the Workshop files into Goldberg's cache.";
                return false;
            }
        }
    } catch (...) {
        error =
            "Could not read the extracted Workshop files.";
        return false;
    }

    if (!directory_has_files(destination)) {
        error =
            "The selected ZIP did not contain any Workshop files.";
        return false;
    }

    return true;
}

inline bool import_workshop_zip(
    unsigned long long workshop_id,
    const std::filesystem::path &settings_root,
    const std::filesystem::path &zip_path,
    std::string &error
)
{
    if (!workshop_id) {
        error = "Invalid Workshop ID.";
        return false;
    }

    std::error_code ec;

    if (!std::filesystem::is_regular_file(
            zip_path,
            ec
        ) || ec) {
        error =
            "The selected ZIP file could not be opened.";
        return false;
    }

    const std::string id_text =
        std::to_string(workshop_id);

    const auto cache_root =
        settings_root /
        "workshop_cache";

    const auto extraction =
        cache_root /
        (id_text + ".import_extract");

    const auto staging =
        cache_root /
        (id_text + ".partial");

    const auto target =
        cache_root /
        id_text;

    std::filesystem::create_directories(
        cache_root,
        ec
    );

    if (ec) {
        error =
            "Could not create Goldberg's Workshop cache directory.";
        return false;
    }

    if (!extract_zip(
            zip_path,
            extraction,
            error
        )) {
        std::filesystem::remove_all(
            extraction,
            ec
        );

        return false;
    }

    std::filesystem::path source =
        extraction;

    const auto expected_folder =
        extraction /
        id_text;

    ec.clear();

    if (std::filesystem::is_directory(
            expected_folder,
            ec
        ) &&
        !ec &&
        directory_has_files(
            expected_folder
        )) {
        source = expected_folder;
    } else {
        ec.clear();

        std::filesystem::path single_folder;
        size_t folder_count = 0;
        bool root_has_files = false;

        try {
            for (const auto &entry :
                 std::filesystem::directory_iterator(
                     extraction,
                     std::filesystem::directory_options::skip_permission_denied
                 )) {

                std::error_code entry_ec;

                if (entry.is_regular_file(
                        entry_ec
                    ) &&
                    !entry_ec) {
                    root_has_files = true;
                }

                entry_ec.clear();

                if (entry.is_directory(
                        entry_ec
                    ) &&
                    !entry_ec) {
                    ++folder_count;
                    single_folder =
                        entry.path();
                }
            }
        } catch (...) {
        }

        if (!root_has_files &&
            folder_count == 1 &&
            directory_has_files(
                single_folder
            )) {
            source = single_folder;
        }
    }

    if (!copy_directory_contents(
            source,
            staging,
            error
        )) {
        std::filesystem::remove_all(
            extraction,
            ec
        );

        std::filesystem::remove_all(
            staging,
            ec
        );

        return false;
    }

    std::filesystem::remove_all(
        target,
        ec
    );

    ec.clear();

    std::filesystem::rename(
        staging,
        target,
        ec
    );

    if (ec) {
        error =
            "The Workshop files were extracted but could not be activated.";

        std::filesystem::remove_all(
            staging,
            ec
        );

        std::filesystem::remove_all(
            extraction,
            ec
        );

        return false;
    }

    std::filesystem::remove_all(
        extraction,
        ec
    );

    return directory_has_files(target);
}

inline bool import_workshop_with_picker(
    unsigned long long workshop_id,
    const std::filesystem::path &settings_root
)
{
    std::filesystem::path zip_path;

    if (!choose_zip_file(zip_path)) {
        return false;
    }

    std::string error;

    if (!import_workshop_zip(
            workshop_id,
            settings_root,
            zip_path,
            error
        )) {
        show_notice(
            "MECCHA Workshop - Import Failed",
            "Workshop ID: " +
            std::to_string(workshop_id) +
            "\r\n\r\n" +
            error
        );

        return false;
    }

    show_notice(
        "MECCHA Workshop - Import Complete",
        "Workshop ID: " +
        std::to_string(workshop_id) +
        "\r\n\r\nThe Workshop map was imported successfully.\r\n\r\n"
        "MECCHA can now register it from Goldberg's live Workshop cache."
    );

    return true;
}

inline std::string friend_export_instructions(
    unsigned long long workshop_id
)
{
    const std::string id =
        std::to_string(workshop_id);

    return
        "How Your Friend Can Export This Mod\r\n\r\n"
        "Workshop ID: " + id + "\r\n"
        "MECCHA App ID: 4704690\r\n\r\n"
        "Ask a friend who already has this Workshop map installed.\r\n\r\n"
        "They can normally find it here:\r\n\r\n"
        "Steam\\steamapps\\workshop\\content\\4704690\\" + id + "\r\n\r\n"
        "They should:\r\n\r\n"
        "1. Copy the entire " + id + " folder.\r\n\r\n"
        "2. Compress the folder into a ZIP file.\r\n\r\n"
        "3. Upload the ZIP somewhere such as:\r\n\r\n"
        "   WeTransfer\r\n"
        "   https://wetransfer.com\r\n\r\n"
        "   Google Drive\r\n"
        "   https://drive.google.com\r\n\r\n"
        "   Dropbox\r\n"
        "   https://dropbox.com\r\n\r\n"
        "   GoFile\r\n"
        "   https://gofile.io\r\n\r\n"
        "   Pixeldrain\r\n"
        "   https://pixeldrain.com\r\n\r\n"
        "4. Send you the download link.\r\n\r\n"
        "5. Download the ZIP to your computer.\r\n\r\n"
        "6. Return to MECCHA and press Import Mod.\r\n\r\n"
        "7. Select the ZIP file.\r\n\r\n"
        "MECCHA will extract, install, and register the Workshop map automatically.\r\n\r\n"
        "Only redistribute Workshop content when the mod creator permits it.";
}

inline int show_friend_export(
    unsigned long long workshop_id,
    const std::filesystem::path &settings_root
)
{
    const std::string instructions =
        friend_export_instructions(
            workshop_id
        );

    for (;;) {
        const int action =
            show_message_dialog(
                "MECCHA Workshop - How Friend Can Export",
                instructions,
                {
                    {BTN_COPY, "Copy Instructions"},
                    {BTN_IMPORT, "Import Mod"},
                    {BTN_BACK, "Back"}
                },
                940,
                790,
                650,
                660,
                720,
                28
            );

        if (action == BTN_COPY) {
            const bool copied =
                copy_text_to_clipboard(
                    instructions
                );

            show_notice(
                "MECCHA Workshop",
                copied
                    ? "The export instructions were copied to your clipboard."
                    : "Windows could not copy the instructions to your clipboard."
            );

            continue;
        }

        if (action == BTN_IMPORT) {
            if (import_workshop_with_picker(
                    workshop_id,
                    settings_root
                )) {
                return IDYES;
            }

            continue;
        }

        return IDNO;
    }
}

inline int show_download_consent(
    unsigned long long workshop_id,
    const std::filesystem::path &requested_path
)
{
    const std::filesystem::path settings_root =
        requested_path.has_parent_path()
            ? requested_path.parent_path()
            : requested_path;

    const std::string id =
        std::to_string(workshop_id);

    const std::string body =
        "Missing Workshop Map\r\n\r\n"
        "Workshop ID: " + id + "\r\n\r\n"
        "This map is not installed.\r\n"
        "Automatic Steam download is unavailable.\r\n\r\n"
        "If someone you know already has this Workshop item installed, "
        "they can send you the installed mod files if the mod creator allows redistribution.";

    for (;;) {
        const int action =
            show_message_dialog(
                "MECCHA Workshop - Missing Map",
                body,
                {
                    {BTN_IMPORT, "Import Mod"},
                    {BTN_HELP, "How Friend Can Export"},
                    {IDCANCEL, "Cancel"}
                },
                820,
                350,
                560,
                220,
                610,
                32
            );

        if (action == BTN_IMPORT) {
            if (import_workshop_with_picker(
                    workshop_id,
                    settings_root
                )) {
                return IDYES;
            }

            continue;
        }

        if (action == BTN_HELP) {
            if (show_friend_export(
                    workshop_id,
                    settings_root
                ) == IDYES) {
                return IDYES;
            }

            continue;
        }

        return IDNO;
    }
}

inline LRESULT CALLBACK import_dialog_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wparam,
    LPARAM lparam
)
{
    auto *state =
        reinterpret_cast<ImportDialogState *>(
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

            return DefWindowProcA(hwnd, msg, wparam, lparam);
        }

        case WM_CREATE: {
            state =
                reinterpret_cast<ImportDialogState *>(
                    GetWindowLongPtrA(
                        hwnd,
                        GWLP_USERDATA
                    )
                );

            if (!state) return -1;

            create_dialog_close_button(hwnd);

            HFONT font =
                reinterpret_cast<HFONT>(
                    GetStockObject(
                        DEFAULT_GUI_FONT
                    )
                );

            HWND title =
                CreateWindowExA(
                    0,
                    "STATIC",
                    "Install a local MECCHA Workshop map",
                    WS_CHILD |
                    WS_VISIBLE,
                    20,
                    22 + DIALOG_TITLE_HEIGHT,
                    410,
                    22,
                    hwnd,
                    nullptr,
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            HWND label =
                CreateWindowExA(
                    0,
                    "STATIC",
                    "Steam Workshop ID:",
                    WS_CHILD |
                    WS_VISIBLE,
                    20,
                    58 + DIALOG_TITLE_HEIGHT,
                    360,
                    20,
                    hwnd,
                    nullptr,
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            state->edit =
                CreateWindowExA(
                    WS_EX_CLIENTEDGE,
                    "EDIT",
                    state->initial.c_str(),
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    ES_AUTOHSCROLL,
                    20,
                    82 + DIALOG_TITLE_HEIGHT,
                    390,
                    27,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(6301)
                    ),
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            HWND hint =
                CreateWindowExA(
                    0,
                    "STATIC",
                    "Choose Import & Use, then select the ZIP you received.",
                    WS_CHILD |
                    WS_VISIBLE,
                    20,
                    120 + DIALOG_TITLE_HEIGHT,
                    410,
                    38,
                    hwnd,
                    nullptr,
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            HWND import_button =
                CreateWindowExA(
                    0,
                    "BUTTON",
                    "Import && Use",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    BS_DEFPUSHBUTTON,
                    140,
                    182 + DIALOG_TITLE_HEIGHT,
                    120,
                    32,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(BTN_IMPORT)
                    ),
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            HWND clear_button =
                CreateWindowExA(
                    0,
                    "BUTTON",
                    "Clear Host Map",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP,
                    270,
                    182 + DIALOG_TITLE_HEIGHT,
                    120,
                    32,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(BTN_CLEAR)
                    ),
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            HWND cancel_button =
                CreateWindowExA(
                    0,
                    "BUTTON",
                    "Cancel",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP,
                    400,
                    182 + DIALOG_TITLE_HEIGHT,
                    90,
                    32,
                    hwnd,
                    reinterpret_cast<HMENU>(
                        static_cast<INT_PTR>(IDCANCEL)
                    ),
                    GetModuleHandleA(nullptr),
                    nullptr
                );

            for (HWND control :
                 {title, label, state->edit, hint,
                  import_button, clear_button, cancel_button}) {
                if (control) {
                    SendMessageA(
                        control,
                        WM_SETFONT,
                        reinterpret_cast<WPARAM>(font),
                        TRUE
                    );
                }
            }

            SendMessageA(
                state->edit,
                EM_SETLIMITTEXT,
                20,
                0
            );

            create_gif_control(
                hwnd,
                state->gif,
                500,
                24 + DIALOG_TITLE_HEIGHT,
                180,
                142
            );

            SetFocus(state->edit);

            SendMessageA(
                state->edit,
                EM_SETSEL,
                0,
                -1
            );

            return 0;
        }

        case WM_TIMER:
            if (state &&
                wparam == GIF_TIMER_ID) {
                advance_gif(
                    state->gif,
                    180,
                    142
                );
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            if (state) draw_dialog_title(hwnd, dc, state->title);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_PRINTCLIENT:
            if (state) draw_dialog_title(hwnd, reinterpret_cast<HDC>(wparam), state->title);
            return 0;

        case WM_NCHITTEST:
            if (is_dialog_title_point(hwnd, lparam)) return HTCAPTION;
            break;

        case WM_COMMAND:
            if (!state) break;

            switch (LOWORD(wparam)) {
                case BTN_IMPORT: {
                    const int len =
                        GetWindowTextLengthA(
                            state->edit
                        );

                    std::string value(
                        static_cast<size_t>(len) + 1,
                        '\0'
                    );

                    if (len > 0) {
                        GetWindowTextA(
                            state->edit,
                            value.data(),
                            len + 1
                        );
                    }

                    value.resize(
                        static_cast<size_t>(len)
                    );

                    unsigned long long workshop_id{};

                    if (!parse_workshop_id_text(
                            value,
                            workshop_id
                        )) {
                        show_notice(
                            "MECCHA Workshop",
                            "Enter a valid numeric Steam Workshop ID."
                        );

                        SetFocus(state->edit);
                        return 0;
                    }

                    if (!import_workshop_with_picker(
                            workshop_id,
                            state->settings_root
                        )) {
                        SetFocus(state->edit);
                        return 0;
                    }

                    state->value = value;
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }

                case BTN_CLEAR:
                    state->value.clear();
                    state->accepted = true;
                    DestroyWindow(hwnd);
                    return 0;

                case BTN_CLOSE:
                case IDCANCEL:
                    DestroyWindow(hwnd);
                    return 0;
            }

            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(
                hwnd,
                GIF_TIMER_ID
            );

            if (state) {
                if (state->gif.bitmap) {
                    DeleteObject(
                        state->gif.bitmap
                    );

                    state->gif.bitmap = nullptr;
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

inline bool show_f8_import_dialog(
    const std::string &initial,
    const std::filesystem::path &settings_root,
    std::string &out_value
)
{
    Gdiplus::GdiplusStartupInput startup_input;
    ULONG_PTR gdiplus_token{};

    const Gdiplus::Status status =
        Gdiplus::GdiplusStartup(
            &gdiplus_token,
            &startup_input,
            nullptr
        );

    ImportDialogState state{};
    state.title = branded_dialog_title("MECCHA Workshop - Import Mod");
    state.initial = initial;
    state.settings_root = settings_root;

    if (status == Gdiplus::Ok) {
        load_embedded_gif(state.gif);
    }

    HINSTANCE instance =
        GetModuleHandleA(nullptr);

    const char class_name[] =
        "GBE_MECCHA_WORKSHOP_IMPORT_V15";

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc =
        import_dialog_proc;
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

    const int width = 710;
    const int height = 270;

    int x{};
    int y{};

    center_window_rect(
        width,
        height,
        x,
        y
    );

    HWND owner = GetForegroundWindow();

    HWND hwnd =
        CreateWindowExA(
            WS_EX_TOPMOST,
            class_name,
            "MECCHA Workshop - Import Mod",
            WS_POPUP |
            WS_BORDER |
            WS_CLIPCHILDREN |
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
        cleanup_gif(state.gif);

        if (gdiplus_token) {
            Gdiplus::GdiplusShutdown(
                gdiplus_token
            );
        }

        return false;
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

        if (!IsDialogMessageA(
                hwnd,
                &msg
            )) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    if (owner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    cleanup_gif(state.gif);

    if (gdiplus_token) {
        Gdiplus::GdiplusShutdown(
            gdiplus_token
        );
    }

    if (!state.accepted) return false;

    out_value = state.value;
    return true;
}

} // namespace MecchaWorkshopUI

#endif
