// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include "defines.h"
#include "misc/file_utils.h"
#include "misc/logger.h"
#include "settings.h"

nlohmann::json* GetApplicationSettings() {
    static nlohmann::json* ret = new nlohmann::json();
    static bool fetched = false;
    if (fetched) {
        return ret;
    }

    fetched = true;
    LOG_DEBUG << "Fetching settings from settings.json file";

    std::string contents;
    std::string settingsFile;
    
    settingsFile = GetExecutableDirectory() + "\\config\\settings.json";
    contents = GetFileContents(settingsFile);
    if (contents.empty()) {
        settingsFile = GetExecutableDirectory() + "\\settings.json";
        contents = GetFileContents(settingsFile);
        if (contents.empty()) {
            LOG_WARNING << "Error while opening the settings.json file";
            return ret;
        }
    }

    std::string err;
    *ret = nlohmann::json::parse(contents);

    if (!err.empty()) {
        LOG_WARNING << "Error while parsing settings.json file: " << err;
    }

    return ret;
}
