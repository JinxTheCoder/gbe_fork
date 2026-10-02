#pragma once

#ifdef _WIN32

#include <windows.h>
#include <string>
#include <cstring>
#include <cwchar>

namespace MecchaDialogBrand {

// Change this one line if you ever want a different credit on the dialog title bars.
inline constexpr wchar_t MECCHA_PATCH_CREDIT[] = L"Patched by J\u0268n\u03C7";

inline bool starts_with(const char *text, const char *prefix)
{
    if (!text || !prefix) return false;
    const size_t prefix_len = std::strlen(prefix);
    return std::strncmp(text, prefix, prefix_len) == 0;
}

inline std::wstring narrow_to_wide(const char *text)
{
    if (!text || !text[0]) return {};

    const int input_len = static_cast<int>(std::strlen(text));
    int count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text,
        input_len,
        nullptr,
        0
    );

    UINT code_page = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;

    if (count <= 0) {
        code_page = CP_ACP;
        flags = 0;
        count = MultiByteToWideChar(
            code_page,
            flags,
            text,
            input_len,
            nullptr,
            0
        );
    }

    if (count <= 0) return {};

    std::wstring result(static_cast<size_t>(count), L'\0');

    MultiByteToWideChar(
        code_page,
        flags,
        text,
        input_len,
        result.data(),
        count
    );

    return result;
}

inline void append_patch_credit(std::wstring &title)
{
    if (title.find(MECCHA_PATCH_CREDIT) != std::wstring::npos) return;

    if (!title.empty()) {
        title += L" - ";
    }

    title += MECCHA_PATCH_CREDIT;
}

inline bool is_meccha_class_name(LPCSTR class_name)
{
    if (!class_name || IS_INTRESOURCE(class_name)) return false;
    return starts_with(class_name, "GBE_MECCHA_");
}

inline bool is_meccha_window(HWND hwnd)
{
    if (!hwnd) return false;

    wchar_t class_name[160]{};
    const int copied = GetClassNameW(
        hwnd,
        class_name,
        static_cast<int>(sizeof(class_name) / sizeof(class_name[0]))
    );

    if (copied <= 0) return false;

    constexpr wchar_t prefix[] = L"GBE_MECCHA_";
    return std::wcsncmp(
        class_name,
        prefix,
        (sizeof(prefix) / sizeof(prefix[0])) - 1
    ) == 0;
}

inline ATOM WINAPI register_class_ex_a(const WNDCLASSEXA *source)
{
    if (!source || !is_meccha_class_name(source->lpszClassName)) {
        return ::RegisterClassExA(source);
    }

    const std::wstring class_name = narrow_to_wide(source->lpszClassName);
    if (class_name.empty()) return ::RegisterClassExA(source);

    std::wstring menu_name;

    WNDCLASSEXW target{};
    target.cbSize = sizeof(target);
    target.style = source->style;
    target.lpfnWndProc = source->lpfnWndProc;
    target.cbClsExtra = source->cbClsExtra;
    target.cbWndExtra = source->cbWndExtra;
    target.hInstance = source->hInstance;
    target.hIcon = source->hIcon;
    target.hCursor = source->hCursor;
    target.hbrBackground = source->hbrBackground;

    if (source->lpszMenuName) {
        if (IS_INTRESOURCE(source->lpszMenuName)) {
            target.lpszMenuName = reinterpret_cast<LPCWSTR>(source->lpszMenuName);
        } else {
            menu_name = narrow_to_wide(source->lpszMenuName);
            target.lpszMenuName = menu_name.c_str();
        }
    }

    target.lpszClassName = class_name.c_str();
    target.hIconSm = source->hIconSm;

    return ::RegisterClassExW(&target);
}

inline HWND WINAPI create_window_ex_a(
    DWORD ex_style,
    LPCSTR class_name,
    LPCSTR window_name,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    HWND parent,
    HMENU menu,
    HINSTANCE instance,
    LPVOID parameter
)
{
    if (!is_meccha_class_name(class_name)) {
        return ::CreateWindowExA(
            ex_style,
            class_name,
            window_name,
            style,
            x,
            y,
            width,
            height,
            parent,
            menu,
            instance,
            parameter
        );
    }

    const std::wstring class_name_w = narrow_to_wide(class_name);
    std::wstring title_w = narrow_to_wide(window_name);

    if (window_name && window_name[0]) {
        append_patch_credit(title_w);
    }

    return ::CreateWindowExW(
        ex_style,
        class_name_w.c_str(),
        title_w.c_str(),
        style,
        x,
        y,
        width,
        height,
        parent,
        menu,
        instance,
        parameter
    );
}

inline LRESULT WINAPI def_window_proc_a(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam
)
{
    if (is_meccha_window(hwnd)) {
        return ::DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam
        );
    }

    return ::DefWindowProcA(
        hwnd,
        message,
        wparam,
        lparam
    );
}

inline int WINAPI message_box_a(
    HWND owner,
    LPCSTR text,
    LPCSTR caption,
    UINT type
)
{
    if (!caption || !starts_with(caption, "MECCHA")) {
        return ::MessageBoxA(owner, text, caption, type);
    }

    std::wstring text_w = narrow_to_wide(text);
    std::wstring caption_w = narrow_to_wide(caption);
    append_patch_credit(caption_w);

    return ::MessageBoxW(
        owner,
        text_w.c_str(),
        caption_w.c_str(),
        type
    );
}

} // namespace MecchaDialogBrand

// These wrappers keep the existing MECCHA dialog code unchanged while making
// the custom dialog classes Unicode so the stylized credit renders correctly.
#define RegisterClassExA MecchaDialogBrand::register_class_ex_a
#define CreateWindowExA MecchaDialogBrand::create_window_ex_a
#define DefWindowProcA MecchaDialogBrand::def_window_proc_a
#define MessageBoxA MecchaDialogBrand::message_box_a

#endif

#include "meccha_workshop_ui_impl.h"
