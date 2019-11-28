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
#include "entry.hpp"



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

bool isOneInstance() {
    HANDLE mutex = CreateMutexW(NULL, TRUE, wkeWebViewClassName);
    if ((mutex != NULL) && (GetLastError() == ERROR_ALREADY_EXISTS)) {
        ReleaseMutex(mutex);
        return false;
    }

    return true;
}

int APIENTRY wWinMainMB(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    hInst = hInstance;
    //myRegisterClass(hInstance);
    using namespace HttpBridge;

    if (!isOneInstance()) {
        ::MessageBoxW(NULL, L"该进程已经启动", L"错误", MB_OK);
        return 0;
    }

    if (!wkeInitialize()) {
        return 1;
    }

    CMiniblink window(WKE_WINDOW_TYPE_POPUP, NULL, 800, 600);
    bind("add", [&window](int a, int b) {
        window.call("setValue", a + b);
    });
    window.load(L"app:///index.html");
    window.set_quit_on_close();
    window.show();


    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));

    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_MAINAPP));
    ::SendMessage(window.GetHWND(), WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    ::SendMessage(window.GetHWND(), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    MSG msg;
    int ret;
    while ((ret = GetMessage(&msg, 0, 0, 0)) != 0 && CMiniblink::counter > 0) {
        if (ret == -1) {
            break;
        } else {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    return 0;
}

int mb_open(const wchar_t *url) {
    if (!wkeInitialize()) {
        return 1;
    }

   CMiniblink window(WKE_WINDOW_TYPE_POPUP, NULL, 800, 600);
    bind("add", [&window](int a, int b) {
        window.call("setValue", a + b);
    });
    window.load(L"app:///index.html");
    //window.load(url);
    window.set_quit_on_close();
    window.show();

    return 0;
}

