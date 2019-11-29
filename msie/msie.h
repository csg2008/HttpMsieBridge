// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include "../defines.h"
#include <windows.h>
#include <string>

void CenterWindow(HWND hwnd);
HWND GetParentWindow(HWND hwnd);
HWND CreatePopupWindow(HWND parentHandle);
bool CenterWindowRelativeToParent(HWND window, HWND parent);
void GetCorrectWindowSize(int* width, int* height);
void GetDpiAwareWindowSize(int* width, int* height);
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow, std::string title, LPCWSTR windowClassName, WNDPROC winProc);