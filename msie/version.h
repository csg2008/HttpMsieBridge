// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#pragma once

#include "misc\logger.h"
#include <stdlib.h>
#include <string>
#include <sstream>
#include "registry.h"

#pragma comment(lib, "Advapi32")

// require min ie version
#define MIN_IE_VERSION "8.0.0.0"

// Parse() and IsLessThanVersion() taken from:
// http://stackoverflow.com/questions/2941491/compare-versions-as-strings

void Parse(int result[4], const std::string& input) {
    std::istringstream parser(input);
    parser >> result[0];
    for(int idx = 1; idx < 4; idx++) {
        parser.get(); //Skip period
        parser >> result[idx];
    }
}

bool IsLessThanVersion(const std::string& a,const std::string& b) {
    int parsedA[4], parsedB[4];
    Parse(parsedA, a);
    Parse(parsedB, b);
    return std::lexicographical_compare(parsedA, parsedA + 4, parsedB, parsedB + 4);
}

std::string GetIeProperty(std::string key) {
    HKEY hKey;
    std::string value = "0.0.0.0";

    LONG lRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WEBVIEW_KEY_PROPERTY, 0, KEY_READ, &hKey);
    if (lRes == ERROR_SUCCESS) {
        GetStringRegKey(hKey, key, value, "0.0.0.0");
        
    }
    RegCloseKey(hKey);

    return value;
}

std::string GetIeVersion() {
    std::string Version = GetIeProperty("Version");
    std::string svcVersion = GetIeProperty("svcVersion");
 
    if (IsLessThanVersion(Version, svcVersion)) {
        Version = svcVersion;
    }

    return Version;
}

static int GET_MSIE_VERSION() {
	return std::stoi(GetIeVersion());
}

int Fix_ie_compat_mode() {
  HKEY hKey;
  DWORD ie_version = 11000;
  TCHAR appname[MAX_PATH + 1];
  TCHAR *p;
  if (GetModuleFileName(NULL, appname, MAX_PATH + 1) == 0) {
    return -1;
  }
  for (p = &appname[lstrlen(appname) - 1]; p != appname && *p != '\\'; p--) {
  }
  p++;
  if (RegCreateKey(HKEY_CURRENT_USER, WEBVIEW_KEY_FEATURE_BROWSER_EMULATION, &hKey) != ERROR_SUCCESS) {
    return -1;
  }
  if (RegSetValueEx(hKey, p, 0, REG_DWORD, (BYTE *)&ie_version, sizeof(ie_version)) != ERROR_SUCCESS) {
    RegCloseKey(hKey);
    return -1;
  }
  RegCloseKey(hKey);
  return 0;
}

bool CheckIeRequire() {
    // We must check IE version, it needs to be at least 6.0 SP2,
    // as CoInternetSetFeatureEnabled() is not available in earlier
    // versions:
    // http://support.microsoft.com/kb/969393
    std::string Version = GetIeVersion();
    LOG_INFO << "IE version from registry: " << Version;

    // Version from registry must be valid, ohterwise for example comparing "abc" will give true.
    if (IsLessThanVersion(Version, MIN_IE_VERSION)) {
        LOG_ERROR << "program require min ie version: " << MIN_IE_VERSION;
        return false;
    }

    if (0 != Fix_ie_compat_mode()) {
        LOG_ERROR << "program require min ie version: ";
        return false;
    }

    return true;
}