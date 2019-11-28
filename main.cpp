// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "defines.h"
#include <Windows.h>
#include <crtdbg.h> // _ASSERT() macro

#include "mmsystem.h"
#include "resource.h"
#include "misc/logger.h"
#include "misc/file_utils.h"
#include "misc/debug.h"
#include "msie/internet_features.h"
#include "msie/browser_window.h"
#include "settings.h"
#include "misc/instance.h"
#include "misc/version.h"
#include "misc/string_utils.h"
#include "web_server.h"
#include "window_utils.h"

#pragma comment(lib,"winmm.lib")

#define BROWSER_GENERIC_TIMER 1

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

Instance g_Instance;
int g_windowCount = 0;
HWND g_hwnd = 0;
HMENU hTrayMenu;
LPSHELLEXECUTEINFO shellInfo;
HINSTANCE g_hInstance = 0;
std::string g_cgiEnvironmentFromArgv = "";

NOTIFYICONDATA GetTrayData(HWND hwnd) {
    static NOTIFYICONDATA tray = {0};
    tray.hWnd = hwnd;
    if (tray.cbSize) {
        return tray;
    }
    nlohmann::json* appSettings = GetApplicationSettings();
    std::string main_window_title = (*appSettings)["window"]["title"];
    std::string startup_to_tray_message = (*appSettings)["window"]["startup_to_tray_message"];
    tray.cbSize = sizeof(tray);
    tray.uID = 1;
    tray.uTimeout = 1000;
    tray.uCallbackMessage = WM_TRAY_MESSAGE;
    tray.hIcon = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(IDR_MAINTRAY), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    wcscpy_s(tray.szTip, Utf8ToWide(main_window_title).c_str());
    wcscpy_s(tray.szInfo, 256, Utf8ToWide(startup_to_tray_message).c_str());
    wcscpy_s(tray.szInfoTitle, 64, Utf8ToWide(main_window_title).c_str());
    tray.uFlags = NIF_ICON | NIF_INFO | NIF_MESSAGE | NIF_TIP;
    tray.dwInfoFlags = NIIF_NONE;

    return tray;
}

bool ShowTrayTip(HWND hwnd, std::string msg, int snd) {
    NOTIFYICONDATA tray = GetTrayData(hwnd);

    if (snd + 10000 > IDS_SNDMAX) {
        snd = IDS_SNDMAX;
    } else if (snd > 0) {
        snd = snd + 10000;
    }
    if (snd > 0) {
        PlaySound(MAKEINTRESOURCE(snd), g_hInstance, SND_ASYNC|SND_RESOURCE|SND_NODEFAULT);
    }

    wcscpy_s(tray.szInfo, 256, Utf8ToWide(msg).c_str());
 
    return 0 != Shell_NotifyIcon(NIM_MODIFY, &tray);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    BrowserWindow* browser = 0;
    UINT_PTR timer = 0;
    BOOL b = 0;
    WORD childEvent = 0;
    HWND childHandle = 0;
    HWND shellBrowserHandle = 0;
    HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);

    // Settings
    nlohmann::json* appSettings = GetApplicationSettings();
    std::string title = (*appSettings)["window"]["title"];
    std::string homepage = (*appSettings)["integration"]["homepage"];

    bool minimize_to_tray = (*appSettings)["window"]["minimize_to_tray"];
    if (g_windowCount > 1) {
        minimize_to_tray = false;
    }

    switch (uMsg) {
        case WM_SIZE:
            browser = GetBrowserWindow(std::to_string((long long) hwnd));
            if (browser) {
                browser->OnResize(uMsg, wParam, lParam);
                return 0;
            } else {
                LOG_WARNING << "WindowProc(): event WM_SIZE: could not fetch BrowserWindow";
            }
            break;
        case WM_CREATE:
            AppendMenu(hSysMenu, MF_STRING, IDM_SETTING, TEXT("Setting"));
            AppendMenu(hSysMenu, MF_STRING, IDM_ABOUT,   TEXT("About"));

            g_windowCount++;
            browser = new BrowserWindow(hwnd, homepage);
            StoreBrowserWindow(std::to_string((long long) hwnd), browser);
            timer = SetTimer(hwnd, BROWSER_GENERIC_TIMER, 50, 0);
            if (!timer) {
                LOG_WARNING << "WindowProc(): event WM_CREATE: SetTimer() failed";
            }
            return 0;
        case WM_SYSCOMMAND:
            if (wParam == IDM_ABOUT) {
                MessageBox(NULL, L"welcome to use http mise bridge\nwebsite: http://github.com/csg800", ConvertW(title.c_str()), MB_OK | MB_ICONINFORMATION);
            } else if (wParam == SC_MINIMIZE && minimize_to_tray) {
                LOG_DEBUG << "Minimize to tray";
                ShowWindow(hwnd, SW_MINIMIZE);
                Sleep(200);
                ShowWindow(hwnd, SW_HIDE);
                ShowTrayTip(hwnd, (*appSettings)["window"]["minimize_to_tray_message"].get<std::string>(), 0);
                break;
            } else {
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
            
            return 0;
        break;
        case WM_TRAY_MESSAGE:
            if (lParam == WM_LBUTTONDOWN) {
                LOG_DEBUG << "Restore from tray";
                ShowWindow(hwnd, SW_SHOW);
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                break;
            } else if (lParam == WM_RBUTTONDOWN) {
				POINT pt;
                GetCursorPos(&pt);
				SetForegroundWindow(hwnd);

				int cmd = ::TrackPopupMenu(::GetSubMenu(hTrayMenu, 0), TPM_RETURNCMD, pt.x, pt.y, NULL, hwnd, NULL);
                if (cmd == IDM_ABOUT) {
                    MessageBox(NULL, L"welcome to use http mise bridge\nwebsite: http://github.com/csg800", ConvertW(title.c_str()), MB_OK | MB_ICONINFORMATION);
                } else if(cmd == IDM_SHOW) {
                    LOG_DEBUG << "Restore from tray";
                    ShowWindow(hwnd, SW_SHOW);
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                }else if(cmd == IDM_EXIT) {
                    PostMessage(hwnd, WM_DESTROY, NULL, NULL);
                }
            }
            break;
        case WM_DESTROY:
            g_windowCount--;
            b = KillTimer(hwnd, BROWSER_GENERIC_TIMER);
            _ASSERT(b);
            RemoveBrowserWindow(std::to_string((long long) hwnd));
            if (g_windowCount <= 0) {
                StopWebServer();

                if (NULL != shellInfo) {
                    closeProcess(shellInfo);
                }

                Shell_NotifyIcon(NIM_DELETE, &GetTrayData(g_hwnd));

                // Cannot call PostQuitMessage as cookies won't be flushed to disk
                // if application is closed immediately. See comment #2:
                // https://code.google.com/p/phpdesktop/issues/detail?id=146
                // I suppose that this PostQuitMessage was added here so that
                // application is forced to exit cleanly in case Mongoose
                // web server hanged while stopping it, as I recall such case.
                // -------------------
                // PostQuitMessage(0);
                // -------------------
                PostQuitMessage(0);
            }
            break;
        case WM_TIMER:
            if (wParam == BROWSER_GENERIC_TIMER) {
                browser = GetBrowserWindow(std::to_string((long long) hwnd));
                if (browser) {
                    browser->OnTimer(uMsg, wParam, lParam);
                    return 0;
                } else {
                    LOG_WARNING << "WindowProc(): event WM_TIMER failed: could not fetch BrowserWindow";
                }
            }
            break;
        case WM_GETMINMAXINFO:
            browser = GetBrowserWindow(std::to_string((long long) hwnd));
            if (browser) {
                browser->OnGetMinMaxInfo(uMsg, wParam, lParam);
                return 0;
            } else {
                // GetMinMaxInfo may fail during window creation, so
                // log severity is only DEBUG.
                LOG_DEBUG << "WindowProc(): event WM_GETMINMAXINFO: could not fetch BrowserWindow";
            }
            break;
        case WM_SETFOCUS:
            browser = GetBrowserWindow(std::to_string((long long) hwnd));
            if (browser) {
                browser->SetFocus();
                return 0;
            } else {
                LOG_DEBUG << "WindowProc(): event WM_SETFOCUS: could not fetch BrowserWindow";
            }
            break;
        /*
        case WM_PARENTNOTIFY:
            LOG_DEBUG << "WM_PARENTNOTIFY";
            browser = GetBrowserWindow(hwnd);
            if (browser) {
                childEvent = LOWORD(wParam);
                // For example WM_LBUTTONDOWN.
                LOG_DEBUG << "childEvent = " << childEvent;
                if (childEvent == WM_DESTROY) {
                    LOG_DEBUG << "childEvent == WM_DESTROY";
                    childHandle = (HWND)HIWORD(wParam);
                    shellBrowserHandle = browser->GetShellBrowserHandle();
                    LOG_DEBUG << "childHandle = " << childHandle;
                    LOG_DEBUG << "shellBrowserHandle = " << shellBrowserHandle;
                    if (childHandle && shellBrowserHandle
                            && childHandle == shellBrowserHandle) {
                        LOG_DEBUG << "!!!!!!!!!!!!!!!!";
                    }
                }
            } else {
                LOG_DEBUG << "WindowProc(): event WM_PARENTNOTIFY: "
                        "could not fetch BrowserWindow";
            }
            break;
        */
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool ProcessKeyboardMessage(MSG* msg) {
    if (msg->message == WM_KEYDOWN
            || msg->message == WM_KEYUP
            || msg->message == WM_SYSKEYDOWN
            || msg->message == WM_SYSKEYUP) {
        HWND root = GetAncestor(msg->hwnd, GA_ROOT);
        BrowserWindow* browser = GetBrowserWindow(std::to_string((long long) root));
        if (browser) {
            if (browser->TranslateAccelerator(msg))
                return true;
        } else {
            LOG_DEBUG << "ProcessKeyboardMessage(): could not fetch BrowserWindow";
        }
    }
    return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpstrCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(ExceptionFilter);

    g_hInstance = hInstance;
    nlohmann::json* settings = GetApplicationSettings();

    // Debugging options.
    bool show_console = (*settings)["integration"]["show_console"];
    std::string log_level = (*settings)["integration"]["log_level"];
    std::string log_file = (*settings)["integration"]["log_file"];
    if (log_file.length()) {
        if (std::string::npos == log_file.find(":")) {
            log_file = GetExecutableDirectory() + "\\" + log_file;
        }

        log_file = GetRealPath(log_file);
    }

    InitializeLogging(show_console, log_level, log_file);
    
    LOG_INFO << "--------------------------------------------------------";
    LOG_INFO << "Started application http msie bridge";

    GetAllHDSerialNumber();

    if (log_file.length()) {
        LOG_INFO << "Logging to: " << log_file << ", Log level = " << FILELog::ToString(FILELog::ReportingLevel());
    } else {
        LOG_INFO << "No logging file set, Log level = " << FILELog::ToString(FILELog::ReportingLevel());
    }

    // Command line arguments
    LPWSTR *argv;
    int argc;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 0; i < argc; i++) {
            std::string argument = WideToUtf8(std::wstring(argv[i]));
            size_t pos = argument.find("=");
            if (pos != std::string::npos) {
                std::string name = argument.substr(0, pos);
                std::string value = argument.substr(pos+1, std::string::npos);
                if (name == "--cgi-environment" && value.length()) {
                    g_cgiEnvironmentFromArgv.assign(value);
                }
            }
        }
    } else {
        LOG_WARNING << "CommandLineToArgvW() failed";
    }

    // check ie min require
    if (!CheckIeRequire()) {
        LOG_ERROR << "check ie min require failed, quit...";
        return -1;
    }

    // startup spec application
    std::string startup = (*settings)["integration"]["startup"];
    if ("" != startup) {
        shellInfo = &exec(startup, 3, false);

        if (NULL == shellInfo) {
            LOG_ERROR << "Startup spec application " << startup << " failed";
            return -1;
        }
    }
 
    // Main window title option.
    std::string main_window_title = (*settings)["window"]["title"];
    if (main_window_title.empty()) {
        main_window_title = GetExecutableName();
    }

    // Single instance guid option.
    const bool instance = (*settings)["window"]["instance"];
    if (instance) {
        g_Instance.Initialize(CLASS_NAME_EX);
	    if (g_Instance.IsRunning()) {
            HWND hwnd = FindWindow(CLASS_NAME_EX, NULL);
            if (hwnd) {
                if (IsIconic(hwnd)){
                    ShowWindow(hwnd, SW_RESTORE);
                }

                SetForegroundWindow(hwnd);
                return 0;
            }
        }
    }

    if (!StartWebServer()) {
        FatalError(NULL, "Could not start internal web server, Exiting application...");
    }

    // From the MSDN "WebBrowser Customization" docs:
    //   Your application should use OleInitialize rather than CoInitialize
    //   to start COM. OleInitialize enables support for the Clipboard,
    //   drag-and-drop operations, OLE, and in-place activation.
    // See: http://msdn.microsoft.com/en-us/library/aa770041(v=vs.85).aspx
    HRESULT hr = OleInitialize(NULL);
    _ASSERT(SUCCEEDED(hr));

    SetInternetFeatures();
    
    hTrayMenu = LoadMenu(GetModuleHandle(0), MAKEINTRESOURCE(IDR_POPUP_MENU));
    g_hwnd = CreateMainWindow(hInstance, nCmdShow, main_window_title, CLASS_NAME_EX, WindowProc);
    Shell_NotifyIcon(NIM_ADD, &GetTrayData(g_hwnd));

    HACCEL  hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));

    MSG msg;
    int ret;
    while ((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
        if (ret == -1) {
            LOG_ERROR << "WinMain.GetMessage() returned -1";
            _ASSERT(false);
            break;
        } else {
            if (!ProcessKeyboardMessage(&msg) && !TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    OleUninitialize();

    LOG_INFO << "Ended application";
    LOG_INFO << "--------------------------------------------------------";

    ShutdownLogging();

    return ret;
}
