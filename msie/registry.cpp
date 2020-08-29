// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "../defines.h"
#include <Windows.h>
#include <Winreg.h>
#include <crtdbg.h> // _ASSERT() macro
#include "misc/logger.h"
#include "misc/string_utils.h"


// GetStringRegKey() taken from here:
// http://stackoverflow.com/questions/34065/how-to-read-a-value-from-the-windows-registry

LONG GetStringRegKey(HKEY hKey, const std::string &strValueName, std::string &strValue, const std::string &strDefaultValue) {
        strValue = strDefaultValue;
        char szBuffer[512];
        DWORD dwBufferSize = sizeof(szBuffer);
        ULONG nError;
        nError = RegQueryValueExA(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
        if (ERROR_SUCCESS == nError) {
            strValue = szBuffer;
        }
        return nError;
}

bool CreateRegistryKey(HKEY hKeyRoot, const wchar_t* path) {
    HKEY hKey;
    DWORD dwDisposition;
    LONG result = RegCreateKeyEx(hKeyRoot, path,
            0, 0, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
            0, &hKey, &dwDisposition);
    bool ret = false;
    if (result == ERROR_SUCCESS) {
        ret = true;
    } else {
        FLOG_WARNING << "CreateRegistryKey() failed: RegCreateKeyEx() failed: path = "  << WideToUtf8(path);
        _ASSERT(false);
    }
    if (hKey)
        RegCloseKey(hKey);
    return ret;
}

bool CreateRegistryString(HKEY hKeyRoot, const wchar_t* path, const wchar_t* stringName, const wchar_t* stringValue) {
    HKEY hKey;
    DWORD dwDisposition;
    LONG result = RegCreateKeyEx(hKeyRoot, path,
            0, 0, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
            0, &hKey, &dwDisposition);
    bool ret = false;
    if (result == ERROR_SUCCESS) {
        DWORD bufferSize = (DWORD) ((wcslen(stringValue) + 1) * sizeof(wchar_t));
        result = RegSetValueEx(hKey, stringName, 0, REG_SZ, (LPBYTE)stringValue, bufferSize);
        if (result == ERROR_SUCCESS) {
            ret = true;
        } else {
            FLOG_WARNING << "CreateRegistryString() failed: "
                    "RegSetValueEx() failed: "
                    "stringName = " << WideToUtf8(stringName) <<
                    "stringValue = " << WideToUtf8(stringValue);
            _ASSERT(false);
        }
    } else {
        FLOG_WARNING << "CreateRegistryString() failed: "
                "RegCreateKeyEx() failed: path = "  << WideToUtf8(path);
        _ASSERT(false);
    }
    if (hKey)
        RegCloseKey(hKey);
    return true;
}
