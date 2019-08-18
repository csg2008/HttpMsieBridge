// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "defines.h"
#include <windows.h>
#include <DbgHelp.h>
#include <stdio.h>
#include <string>

#include "settings.h"
#include "file_utils.h"
#include "string_utils.h"

int GenerateMiniDump(PEXCEPTION_POINTERS pExceptionPointers) {
    typedef BOOL(WINAPI * MiniDumpWriteDumpT)(
        HANDLE,
        DWORD,
        HANDLE,
        MINIDUMP_TYPE,
        PMINIDUMP_EXCEPTION_INFORMATION,
        PMINIDUMP_USER_STREAM_INFORMATION,
        PMINIDUMP_CALLBACK_INFORMATION
    );

    MiniDumpWriteDumpT pfnMiniDumpWriteDump = NULL;
    HMODULE hDbgHelp = LoadLibrary(_T("DbgHelp.dll"));
    if (NULL == hDbgHelp) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    pfnMiniDumpWriteDump = (MiniDumpWriteDumpT)GetProcAddress(hDbgHelp, "MiniDumpWriteDump");

    if (NULL == pfnMiniDumpWriteDump) {
        FreeLibrary(hDbgHelp);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // create dump file
    TCHAR szFileName[MAX_PATH] = {0};
    TCHAR* szVersion = _T("DumpDemo_v1.0");
    SYSTEMTIME stLocalTime;
    GetLocalTime(&stLocalTime);
    wsprintf(szFileName, L"%s-%04d%02d%02d-%02d%02d%02d.dmp",
        szVersion, stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
        stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond);
    HANDLE hDumpFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    if (INVALID_HANDLE_VALUE == hDumpFile) {
        FreeLibrary(hDbgHelp);
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    // save to dump file
    MINIDUMP_EXCEPTION_INFORMATION expParam;
    expParam.ThreadId = GetCurrentThreadId();
    expParam.ExceptionPointers = pExceptionPointers;
    expParam.ClientPointers = FALSE;
    pfnMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
        hDumpFile, MiniDumpWithDataSegs, (pExceptionPointers ? &expParam : NULL), NULL, NULL);

    // release file
    CloseHandle(hDumpFile);
    FreeLibrary(hDbgHelp);
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI ExceptionFilter(LPEXCEPTION_POINTERS lpExceptionInfo) {
    if (IsDebuggerPresent()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    return GenerateMiniDump(lpExceptionInfo);
}

void FatalError(HWND hwnd, std::string message) {
    nlohmann::json* settings = GetApplicationSettings();
    std::string title = (*settings)["bridge"]["window"]["title"];
    if (title.empty()) {
        title = GetExecutableName();
    }

    MessageBox(hwnd, Utf8ToWide(message).c_str(), Utf8ToWide(title).c_str(), MB_ICONERROR);
    exit(-1);
}

char* WCHAR_TO_CHAR(wchar_t* wide) {
	int asciisize = WideCharToMultiByte(CP_UTF8, 0, wide, -1, 0, 0, 0, 0);
	char* ascii = (char*)malloc(asciisize * sizeof(char));
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, ascii, asciisize, 0, 0);
	return ascii;
}

void GUID_TO_CHAR(const GUID* guid, char* outString, size_t outStringSize) {
    sprintf_s(outString, outStringSize,
              "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
              guid->Data1, guid->Data2, guid->Data3,
              guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
              guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}
