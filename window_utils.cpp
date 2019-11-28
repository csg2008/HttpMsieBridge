// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "defines.h"
#include <windows.h>
#include <crtdbg.h> // _ASSERT() macro
#include "misc/logger.h"
#include "settings.h"
#include "window_utils.h"
#include "resource.h"

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow, std::string title, LPCWSTR windowClassName, WNDPROC winProc) {
    nlohmann::json* appSettings = GetApplicationSettings();
    int default_width = (*appSettings)["window"]["default_size"][0];
    int default_height = (*appSettings)["window"]["default_size"][1];
    bool disable_maximize_button = (*appSettings)["window"]["disable_maximize_button"];
    bool center_on_screen = (*appSettings)["window"]["center_on_screen"];
    bool dpi_aware = (*appSettings)["window"]["dpi_aware"];
    bool start_maximized = (*appSettings)["window"]["start_maximized"];
    bool always_on_top = (*appSettings)["window"]["always_on_top"];

    if (default_width && default_height) {
        if (dpi_aware) {
            GetDpiAwareWindowSize(&default_width, &default_height);
        }
        GetCorrectWindowSize(&default_width, &default_height);
    } else {
        default_width = CW_USEDEFAULT;
        default_height = CW_USEDEFAULT;
    }

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.hbrBackground = (HBRUSH)GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_MAINTITLE));
    wc.hInstance = hInstance;
    wc.lpfnWndProc = winProc;
    wc.lpszClassName = windowClassName;

    ATOM atom = RegisterClassEx(&wc);
    _ASSERT(atom);

    HWND hwnd = CreateWindowEx(0, windowClassName,
            Utf8ToWide(title).c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, default_width, default_height,
            HWND_DESKTOP, 0, hInstance, 0);
    _ASSERT(hwnd);
    if (disable_maximize_button) {
        int style = GetWindowLong(hwnd, GWL_STYLE);
        _ASSERT(style);
        int ret = SetWindowLong(hwnd, GWL_STYLE, style &~WS_MAXIMIZEBOX);
        _ASSERT(ret);
    }
    if (center_on_screen) {
        CenterWindow(hwnd);
    }
    if (always_on_top) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    if (start_maximized) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    } else {
        ShowWindow(hwnd, nCmdShow);
    }
    UpdateWindow(hwnd);
    return hwnd;
}

void CenterWindow(HWND wnd) {
   RECT r,r1;
   GetWindowRect(wnd,&r);
   GetWindowRect(GetDesktopWindow(),&r1);
   MoveWindow(wnd,((r1.right-r1.left)-(r.right-r.left))/2,
      ((r1.bottom-r1.top)-(r.bottom-r.top))/2,
      (r.right-r.left), (r.bottom-r.top),0);
}

HWND GetParentWindow(HWND hwnd) {
    return GetWindow(hwnd, GW_OWNER);
}

bool CenterWindowRelativeToParent(HWND window, HWND parent) {
    RECT rect, parentRect;
    int width, height;
    int screenWidth, screenHeight;
    int x, y;
    GetWindowRect(window, &rect);
    GetWindowRect(parent, &parentRect);
    width  = rect.right  - rect.left;
    height = rect.bottom - rect.top;
    x = ((parentRect.right - parentRect.left) - width) / 2 + parentRect.left;
    y = ((parentRect.bottom - parentRect.top) - height) / 2 + parentRect.top;
    screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }
    if (x + width > screenWidth)  {
        x = screenWidth  - width;
    }
    if (y + height > screenHeight) {
        y = screenHeight - height;
    }
    // Move it a bit so that popup window does not overlay the parent
    // window completely, when it is of the same size or a bit smaller.
    if (x >= parentRect.left && x < parentRect.left + 48) {
        x = parentRect.left + 48;
    }
    if (y >= parentRect.top && y < parentRect.top + 32) {
        y = parentRect.top + 32;
    }
    MoveWindow(window, x, y, width, height, TRUE);
    return true;
}

void GetCorrectWindowSize(int* width, int* height) {
    int max_width = GetSystemMetrics(SM_CXMAXIMIZED) - 96;
    int max_height = GetSystemMetrics(SM_CYMAXIMIZED) - 64;
    LOG_DEBUG << "Window max width/height = " << max_width << "/" << max_height;
    bool max_size_exceeded = (*width > max_width \
            || *height > max_height);
    if (*width > max_width) {
        *width = max_width;
    }
    if (*height > max_height) {
        *height = max_height;
    }
    if (max_size_exceeded) {
        LOG_DEBUG << "Window max size exceeded, new width/height = "
                    << *width << "/" << *height;
    }
}

void GetDpiAwareWindowSize(int* width, int* height) {
    // Win7 DPI:
    // text size Larger 150% => ppix/ppiy 144
    // text size Medium 125% => ppix/ppiy 120
    // text size Smaller 100% => ppix/ppiy 96
    HDC hdc = GetDC(HWND_DESKTOP);
    int ppix = GetDeviceCaps(hdc, LOGPIXELSX);
    int ppiy = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(HWND_DESKTOP, hdc);
    double newZoomLevel = 0.0;
    if (ppix > 96) {
        newZoomLevel = (ppix - 96) / 24;
    }
    if (newZoomLevel > 0.0) {
        *width = *width + (int)ceil(newZoomLevel * 0.25 * (*width));
        *height = *height + (int)ceil(newZoomLevel * 0.25 * (*height));
        LOG_INFO << "DPI, window enlarged by "
                  << ceil(newZoomLevel * 0.25 * 100) << "%"
                  << " new width/height = " << *width << "/" << *height;
    }
}


HWND CreatePopupWindow(HWND parentHandle) {
    nlohmann::json* settings = GetApplicationSettings();
    bool center_relative_to_parent = (*settings)["window"]["center_relative_to_parent"];

    // Title will be set in BrowserWindow::BrowserWindow().
    // CW_USEDEFAULT cannot be used with WS_POPUP.
    HWND hwnd = CreateWindowEx(0, CLASS_NAME_EX, 0, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            0, 0, 0, 0);
    _ASSERT(hwnd);
    if (center_relative_to_parent) {
        // This won't work properly as real width/height is set later
        // when BrowserEvents2::WindowSetWidth() / WindowSetHeight()
        // are triggered. TODO.
        // CenterWindow(hwnd);
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    return hwnd;
}