// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "../defines.h"
#include <Windows.h>
#include <Winreg.h>
#include <string>

#define WEBVIEW_APPLICATION L"Software\\Http Msie Bridge\\MSIE"
#define WEBVIEW_KEY_PROPERTY L"SOFTWARE\\Microsoft\\Internet Explorer"
#define WEBVIEW_KEY_FEATURE_BROWSER_EMULATION L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION"

bool CreateRegistryKey(HKEY hKeyRoot, const wchar_t* path);
bool CreateRegistryString(HKEY hKeyRoot, const wchar_t* path, const wchar_t* stringName, const wchar_t* stringValue);
LONG GetStringRegKey(HKEY hKey, const std::string &strValueName, std::string &strValue, const std::string &strDefaultValue);