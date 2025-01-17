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
    FLOG_DEBUG << "Fetching settings from settings.json file";

    std::string contents;
    std::string settingsFile;
    
    settingsFile = GetExecutableDirectory() + "\\config\\settings.json";
    contents = GetFileContents(settingsFile);
    if (contents.empty()) {
        settingsFile = GetExecutableDirectory() + "\\settings.json";
        contents = GetFileContents(settingsFile);
        if (contents.empty()) {
            FLOG_WARNING << "Error while opening the settings.json file";
            return ret;
        }
    }

    std::string err;
    *ret = nlohmann::json::parse(contents);

    if (!err.empty()) {
        FLOG_WARNING << "Error while parsing settings.json file: " << err;
    }

    if ((*ret)["integration"]["engine"].is_null()) {
        (*ret)["integration"]["engine"] = "msie";
    } else if ((*ret)["integration"]["engine"].is_string()) {
        if ("msie" != (*ret)["integration"]["engine"].get<std::string>() && "mb" != (*ret)["integration"]["engine"].get<std::string>()) {
            FLOG_WARNING << "Gui engine must be msie or mb, give: " << (*ret)["integration"]["engine"];
            (*ret)["integration"]["engine"] = "msie";
        }
    } else {
        FLOG_WARNING << "Gui engine must be msie or mb, give: " << (*ret)["integration"]["engine"];
        (*ret)["integration"]["engine"] = "msie";
    }

    std::string log_file = (*ret)["integration"]["log_file"];
    if (log_file.length()) {
        if (std::string::npos == log_file.find(":")) {
            log_file = GetExecutableDirectory() + "\\" + log_file;
        }

        (*ret)["integration"]["log_file"] = GetRealPath(log_file);
    }

    if ((*ret)["integration"]["temp"].empty()) {
        (*ret)["integration"]["temp"] = GetRealPath(GetExecutableDirectory());
    } else {
        std::string temp_path = (*ret)["integration"]["temp"];
        if (std::string::npos == temp_path.find(":")) {
            temp_path = GetExecutableDirectory() + "\\" + temp_path;
        }

        (*ret)["integration"]["temp"] = GetRealPath(temp_path);
    }

    if ((*ret)["window"]["title"].empty()) {
        (*ret)["window"]["title"] = GetExecutableName();
    }

    return ret;
}
