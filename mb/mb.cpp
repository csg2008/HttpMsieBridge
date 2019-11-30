#include <Windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <Commctrl.h>
#include <windowsx.h>
#include <cassert>
#include <xstring>
#include <vector>
#include "bind.h"
#include "CMiniblink.h"
#include "wke.h"
#include "../resource.h"
#include "mb.hpp"



HINSTANCE hInst;
int HttpBridge::CMiniblink::counter = 0;
const LPCWSTR wkeWebViewClassName = L"wkeWebWindow";

void setAppIcon(HWND hwnd) {
    wchar_t szFilePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szFilePath, MAX_PATH)) {
        HICON iconLarge, iconSmall;
        UINT nIcons = ExtractIconExW((LPCWSTR)&szFilePath, -1, NULL, NULL, 0);

        ExtractIconExW((LPCWSTR)&szFilePath, 0, &iconLarge, &iconSmall, nIcons);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)iconLarge);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)iconSmall);
    }
}



int mb_open(const wchar_t *url) {


    return 0;
}

